/* Copyright (C) 2009 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "autoconfig.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>

#include <iostream>
#include <list>
#include <map>
#include <algorithm>

#include "cstr.h"
#include "pathut.h"
#include "conftree.h"
#include "rclconfig.h"
#include "fstreewalk.h"
#include "rcldb.h"
#include "readfile.h"
#include "indexer.h"
#include "fsindexer.h"
#include "transcode.h"
#include "debuglog.h"
#include "internfile.h"
#include "smallut.h"
#include "wipedir.h"
#include "fileudi.h"
#include "cancelcheck.h"
#include "rclinit.h"
#include "execmd.h"
#include "extrameta.h"

using namespace std;

#ifdef IDX_THREADS
class DbUpdTask {
public:
    // Take some care to avoid sharing string data (if string impl is cow)
    DbUpdTask(const string& u, const string& p, const Rcl::Doc& d)
	: udi(u.begin(), u.end()), parent_udi(p.begin(), p.end())
    {
        d.copyto(&doc);
    }
    string udi;
    string parent_udi;
    Rcl::Doc doc;
};
extern void *FsIndexerDbUpdWorker(void*);

class InternfileTask {
public:
    // Take some care to avoid sharing string data (if string impl is cow)
    InternfileTask(const std::string &f, const struct stat *i_stp,
		   map<string,string> lfields)
	: fn(f.begin(), f.end()), statbuf(*i_stp)
    {
        map_ss_cp_noshr(lfields, &localfields);
    }
    string fn;
    struct stat statbuf;
    map<string,string> localfields;
};
extern void *FsIndexerInternfileWorker(void*);
#endif // IDX_THREADS

// Thread safe variation of the "missing helpers" storage. Only the
// addMissing method needs protection, the rest are called from the
// main thread either before or after the exciting part
class FSIFIMissingStore : public FIMissingStore {
#ifdef IDX_THREADS
    PTMutexInit m_mutex;
#endif
public:
    virtual void addMissing(const string& prog, const string& mt)
    {
#ifdef IDX_THREADS
	PTMutexLocker locker(m_mutex);
#endif
	FIMissingStore::addMissing(prog, mt);
    }
};

FsIndexer::FsIndexer(RclConfig *cnf, Rcl::Db *db, DbIxStatusUpdater *updfunc) 
    : m_config(cnf), m_db(db), m_updater(updfunc), 
      m_missing(new FSIFIMissingStore), m_detectxattronly(false)
#ifdef IDX_THREADS
    , m_iwqueue("Internfile", cnf->getThrConf(RclConfig::ThrIntern).first), 
      m_dwqueue("Split", cnf->getThrConf(RclConfig::ThrSplit).first)
#endif // IDX_THREADS
{
    LOGDEB1(("FsIndexer::FsIndexer\n"));
    m_havelocalfields = m_config->hasNameAnywhere("localfields");
    m_config->getConfParam("detectxattronly", &m_detectxattronly);
    
#ifdef IDX_THREADS
    m_stableconfig = new RclConfig(*m_config);
    m_loglevel = DebugLog::getdbl()->getlevel();
    m_haveInternQ = m_haveSplitQ = false;
    int internqlen = cnf->getThrConf(RclConfig::ThrIntern).first;
    int internthreads = cnf->getThrConf(RclConfig::ThrIntern).second;
    if (internqlen >= 0) {
	if (!m_iwqueue.start(internthreads, FsIndexerInternfileWorker, this)) {
	    LOGERR(("FsIndexer::FsIndexer: intern worker start failed\n"));
	    return;
	}
	m_haveInternQ = true;
    } 
    int splitqlen = cnf->getThrConf(RclConfig::ThrSplit).first;
    int splitthreads = cnf->getThrConf(RclConfig::ThrSplit).second;
    if (splitqlen >= 0) {
	if (!m_dwqueue.start(splitthreads, FsIndexerDbUpdWorker, this)) {
	    LOGERR(("FsIndexer::FsIndexer: split worker start failed\n"));
	    return;
	}
	m_haveSplitQ = true;
    }
    LOGDEB(("FsIndexer: threads: haveIQ %d iql %d iqts %d "
	    "haveSQ %d sql %d sqts %d\n", m_haveInternQ, internqlen, 
	    internthreads, m_haveSplitQ, splitqlen, splitthreads));
#endif // IDX_THREADS
}

FsIndexer::~FsIndexer() 
{
    LOGDEB1(("FsIndexer::~FsIndexer()\n"));

#ifdef IDX_THREADS
    void *status;
    if (m_haveInternQ) {
	status = m_iwqueue.setTerminateAndWait();
	LOGDEB0(("FsIndexer: internfile wrkr status: %ld (1->ok)\n", 
		 long(status)));
    }
    if (m_haveSplitQ) {
	status = m_dwqueue.setTerminateAndWait();
	LOGDEB0(("FsIndexer: dbupd worker status: %ld (1->ok)\n", 
		 long(status)));
    }
    delete m_stableconfig;
#endif // IDX_THREADS

    delete m_missing;
}

bool FsIndexer::init()
{
    if (m_tdl.empty()) {
        m_tdl = m_config->getTopdirs();
        if (m_tdl.empty()) {
            LOGERR(("FsIndexers: no topdirs list defined\n"));
            return false;
        }
    }
    return true;
}

// Recursively index each directory in the topdirs:
bool FsIndexer::index(bool quickshallow)
{
    Chrono chron;
    if (!init())
	return false;

    if (m_updater) {
#ifdef IDX_THREADS
	PTMutexLocker locker(m_updater->m_mutex);
#endif
	m_updater->status.reset();
	m_updater->status.dbtotdocs = m_db->docCnt();
    }

    m_walker.setSkippedPaths(m_config->getSkippedPaths());
    if (quickshallow) {
	m_walker.setOpts(m_walker.getOpts() | FsTreeWalker::FtwSkipDotFiles);
	m_walker.setMaxDepth(2);
    }

    for (vector<string>::const_iterator it = m_tdl.begin();
	 it != m_tdl.end(); it++) {
	LOGDEB(("FsIndexer::index: Indexing %s into %s\n", it->c_str(), 
		getDbDir().c_str()));

	// Set the current directory in config so that subsequent
	// getConfParams() will get local values
	m_config->setKeyDir(*it);

	// Adjust the "follow symlinks" option
	bool follow;
	int opts = m_walker.getOpts();
	if (m_config->getConfParam("followLinks", &follow) && follow) {
	    opts |= FsTreeWalker::FtwFollow;
	} else {
	    opts &= ~FsTreeWalker::FtwFollow;
	}	    
	m_walker.setOpts(opts);

	int abslen;
	if (m_config->getConfParam("idxabsmlen", &abslen))
	    m_db->setAbstractParams(abslen, -1, -1);

	// Walk the directory tree
	if (m_walker.walk(*it, *this) != FsTreeWalker::FtwOk) {
	    LOGERR(("FsIndexer::index: error while indexing %s: %s\n", 
		    it->c_str(), m_walker.getReason().c_str()));
	    return false;
	}
    }

#ifdef IDX_THREADS
    if (m_haveInternQ) 
	m_iwqueue.waitIdle();
    if (m_haveSplitQ)
	m_dwqueue.waitIdle();
    m_db->waitUpdIdle();
#endif // IDX_THREADS

    if (m_missing) {
	string missing;
	m_missing->getMissingDescription(missing);
	if (!missing.empty()) {
	    LOGINFO(("FsIndexer::index missing helper program(s):\n%s\n", 
		     missing.c_str()));
	}
	m_config->storeMissingHelperDesc(missing);
    }
    LOGINFO(("fsindexer index time:  %d mS\n", chron.ms()));
    return true;
}

static bool matchesSkipped(const vector<string>& tdl,
                           FsTreeWalker& walker,
                           const string& path)
{
    // Check path against topdirs and skippedPaths. We go up the
    // ancestors until we find either a topdirs or a skippedPaths
    // match. If topdirs is found first-> ok to index (it's possible
    // and useful to configure a topdir under a skippedPath in the
    // config). This matches what happens during the normal fs tree
    // walk.
    string canonpath = path_canon(path);
    string mpath = canonpath;
    string topdir;
    while (mpath.length() > 1) {
        for (vector<string>::const_iterator it = tdl.begin();  
             it != tdl.end(); it++) {
            // the topdirs members are already canonized.
            LOGDEB2(("indexfiles:matchesskpp: comparing ancestor [%s] to "
                     "topdir [%s]\n", mpath.c_str(), it->c_str()));
            if (!mpath.compare(*it)) {
                topdir = *it;
                goto goodpath;
            }
        }

        if (walker.inSkippedPaths(mpath, false)) {
            LOGDEB(("FsIndexer::indexFiles: skipping [%s] (skpp)\n", 
                    path.c_str()));
            return true;
        }

        string::size_type len = mpath.length();
        mpath = path_getfather(mpath);
        // getfather normally returns a path ending with /, canonic
        // paths don't (except for '/' itself).
        if (!mpath.empty() && mpath[mpath.size()-1] == '/')
            mpath.erase(mpath.size()-1);
        // should not be necessary, but lets be prudent. If the
        // path did not shorten, something is seriously amiss
        // (could be an assert actually)
        if (mpath.length() >= len) {
            LOGERR(("FsIndexer::indexFile: internal Error: path [%s] did not "
                    "shorten\n", mpath.c_str()));
            return true;
        }
    }
    // We get there if neither topdirs nor skippedPaths tests matched
    LOGDEB(("FsIndexer::indexFiles: skipping [%s] (ntd)\n", path.c_str()));
    return true;

goodpath:

    // Then check all path components up to the topdir against skippedNames
    mpath = canonpath;
    while (mpath.length() >= topdir.length() && mpath.length() > 1) {
        string fn = path_getsimple(mpath);
        if (walker.inSkippedNames(fn)) {
            LOGDEB(("FsIndexer::indexFiles: skipping [%s] (skpn)\n", 
                    path.c_str()));
            return true;
        }

        string::size_type len = mpath.length();
        mpath = path_getfather(mpath);
        // getfather normally returns a path ending with /, getsimple 
        // would then return ''
        if (!mpath.empty() && mpath[mpath.size()-1] == '/')
            mpath.erase(mpath.size()-1);
        // should not be necessary, but lets be prudent. If the
        // path did not shorten, something is seriously amiss
        // (could be an assert actually)
        if (mpath.length() >= len)
            return true;
    }
    return false;
}

/** 
 * Index individual files, out of a full tree run. No database purging
 */
bool FsIndexer::indexFiles(list<string>& files, ConfIndexer::IxFlag flag)
{
    LOGDEB(("FsIndexer::indexFiles\n"));
    int ret = false;

    if (!init())
        return false;

    int abslen;
    if (m_config->getConfParam("idxabsmlen", &abslen))
	m_db->setAbstractParams(abslen, -1, -1);

    m_purgeCandidates.setRecord(true);

    // We use an FsTreeWalker just for handling the skipped path/name lists
    FsTreeWalker walker;
    walker.setSkippedPaths(m_config->getSkippedPaths());

    for (list<string>::iterator it = files.begin(); it != files.end(); ) {
        LOGDEB2(("FsIndexer::indexFiles: [%s]\n", it->c_str()));

        m_config->setKeyDir(path_getfather(*it));
	if (m_havelocalfields)
	    localfieldsfromconf();

	bool follow = false;
	m_config->getConfParam("followLinks", &follow);

        walker.setSkippedNames(m_config->getSkippedNames());
	// Check path against indexed areas and skipped names/paths
        if (!(flag&ConfIndexer::IxFIgnoreSkip) && 
	    matchesSkipped(m_tdl, walker, *it)) {
            it++; 
	    continue;
        }

	struct stat stb;
	int ststat = follow ? stat(it->c_str(), &stb) : 
	    lstat(it->c_str(), &stb);
	if (ststat != 0) {
	    LOGERR(("FsIndexer::indexFiles: lstat(%s): %s", it->c_str(),
		    strerror(errno)));
            it++; 
	    continue;
	}

	if (processone(*it, &stb, FsTreeWalker::FtwRegular) != 
	    FsTreeWalker::FtwOk) {
	    LOGERR(("FsIndexer::indexFiles: processone failed\n"));
	    goto out;
	}
        it = files.erase(it);
    }

    ret = true;
out:
#ifdef IDX_THREADS
    if (m_haveInternQ) 
	m_iwqueue.waitIdle();
    if (m_haveSplitQ)
	m_dwqueue.waitIdle();
    m_db->waitUpdIdle();
#endif // IDX_THREADS

    // Purge possible orphan documents
    if (ret == true) {
	LOGDEB(("Indexfiles: purging orphans\n"));
	const vector<string>& purgecandidates = m_purgeCandidates.getCandidates();
	for (vector<string>::const_iterator it = purgecandidates.begin();
	     it != purgecandidates.end(); it++) {
	    LOGDEB(("Indexfiles: purging orphans for %s\n", it->c_str()));
	    m_db->purgeOrphans(*it);
	}
#ifdef IDX_THREADS
	m_db->waitUpdIdle();
#endif // IDX_THREADS
    }

    LOGDEB(("FsIndexer::indexFiles: done\n"));
    return ret;
}


/** Purge docs for given files out of the database */
bool FsIndexer::purgeFiles(list<string>& files)
{
    LOGDEB(("FsIndexer::purgeFiles\n"));
    bool ret = false;
    if (!init())
	return false;

    for (list<string>::iterator it = files.begin(); it != files.end(); ) {
	string udi;
	make_udi(*it, cstr_null, udi);
        // rcldb::purgefile returns true if the udi was either not
        // found or deleted, false only in case of actual error
        bool existed;
	if (!m_db->purgeFile(udi, &existed)) {
	    LOGERR(("FsIndexer::purgeFiles: Database error\n"));
	    goto out;
	}
        // If we actually deleted something, take it off the list
        if (existed) {
            it = files.erase(it);
        } else {
            it++;
        }
    }

    ret = true;
out:
#ifdef IDX_THREADS
    if (m_haveInternQ) 
	m_iwqueue.waitIdle();
    if (m_haveSplitQ)
	m_dwqueue.waitIdle();
    m_db->waitUpdIdle();
#endif // IDX_THREADS
    LOGDEB(("FsIndexer::purgeFiles: done\n"));
    return ret;
}

// Local fields can be set for fs subtrees in the configuration file 
void FsIndexer::localfieldsfromconf()
{
    LOGDEB1(("FsIndexer::localfieldsfromconf\n"));

    string sfields;
    m_config->getConfParam("localfields", sfields);
    if (!sfields.compare(m_slocalfields)) 
	return;

    m_slocalfields = sfields;
    m_localfields.clear();
    if (sfields.empty())
	return;

    string value;
    ConfSimple attrs;
    m_config->valueSplitAttributes(sfields, value, attrs);
    vector<string> nmlst = attrs.getNames(cstr_null);
    for (vector<string>::const_iterator it = nmlst.begin();
         it != nmlst.end(); it++) {
	string nm = m_config->fieldCanon(*it);
	attrs.get(*it, m_localfields[nm]);
	LOGDEB2(("FsIndexer::localfieldsfromconf: [%s]->[%s]\n",
		 nm.c_str(), m_localfields[nm].c_str()));
    }
}

void FsIndexer::setlocalfields(const map<string, string>& fields, Rcl::Doc& doc)
{
    for (map<string, string>::const_iterator it = fields.begin();
	 it != fields.end(); it++) {
        // Being chosen by the user, localfields override values from
        // the filter. The key is already canonic (see
        // localfieldsfromconf())
	doc.meta[it->first] = it->second;
    }
}

void FsIndexer::makesig(const struct stat *stp, string& out)
{
    char cbuf[100]; 
    sprintf(cbuf, "%lld" "%ld", (long long)stp->st_size, 
            o_uptodate_test_use_mtime ? 
            (long)stp->st_mtime : (long)stp->st_ctime);
    out = cbuf;
}

#ifdef IDX_THREADS
// Called updworker as seen from here, but the first step (and only in
// most meaningful configurations) is doing the word-splitting, which
// is why the task is referred as "Split" in the grand scheme of
// things. An other stage usually deals with the actual index update.
void *FsIndexerDbUpdWorker(void * fsp)
{
    recoll_threadinit();
    FsIndexer *fip = (FsIndexer*)fsp;
    WorkQueue<DbUpdTask*> *tqp = &fip->m_dwqueue;
    DebugLog::getdbl()->setloglevel(fip->m_loglevel);

    DbUpdTask *tsk;
    for (;;) {
	size_t qsz;
	if (!tqp->take(&tsk, &qsz)) {
	    tqp->workerExit();
	    return (void*)1;
	}
	LOGDEB0(("FsIndexerDbUpdWorker: task ql %d\n", int(qsz)));
	if (!fip->m_db->addOrUpdate(tsk->udi, tsk->parent_udi, tsk->doc)) {
	    LOGERR(("FsIndexerDbUpdWorker: addOrUpdate failed\n"));
	    tqp->workerExit();
	    return (void*)0;
	}
	delete tsk;
    }
}

void *FsIndexerInternfileWorker(void * fsp)
{
    recoll_threadinit();
    FsIndexer *fip = (FsIndexer*)fsp;
    WorkQueue<InternfileTask*> *tqp = &fip->m_iwqueue;
    DebugLog::getdbl()->setloglevel(fip->m_loglevel);
    RclConfig myconf(*(fip->m_stableconfig));

    InternfileTask *tsk = 0;
    for (;;) {
	if (!tqp->take(&tsk)) {
	    tqp->workerExit();
	    return (void*)1;
	}
	LOGDEB0(("FsIndexerInternfileWorker: task fn %s\n", tsk->fn.c_str()));
	if (fip->processonefile(&myconf, tsk->fn, &tsk->statbuf,
				tsk->localfields) !=
	    FsTreeWalker::FtwOk) {
	    LOGERR(("FsIndexerInternfileWorker: processone failed\n"));
	    tqp->workerExit();
	    return (void*)0;
	}
	LOGDEB1(("FsIndexerInternfileWorker: done fn %s\n", tsk->fn.c_str()));
	delete tsk;
    }
}
#endif // IDX_THREADS

/// This method gets called for every file and directory found by the
/// tree walker. 
///
/// It checks with the db if the file has changed and needs to be
/// reindexed. If so, it calls internfile() which will identify the
/// file type and call an appropriate handler to convert the document into
/// internal format, which we then add to the database.
///
/// Accent and majuscule handling are performed by the db module when doing
/// the actual indexing work. The Rcl::Doc created by internfile()
/// mostly contains pretty raw utf8 data.
FsTreeWalker::Status 
FsIndexer::processone(const std::string &fn, const struct stat *stp, 
		      FsTreeWalker::CbFlag flg)
{
    if (m_updater) {
#ifdef IDX_THREADS
	PTMutexLocker locker(m_updater->m_mutex);
#endif
	if (!m_updater->update()) {
	    return FsTreeWalker::FtwStop;
	}
    }

    // If we're changing directories, possibly adjust parameters (set
    // the current directory in configuration object)
    if (flg == FsTreeWalker::FtwDirEnter || 
	flg == FsTreeWalker::FtwDirReturn) {
	m_config->setKeyDir(fn);

	// Set up skipped patterns for this subtree. 
	m_walker.setSkippedNames(m_config->getSkippedNames());

        // Adjust local fields from config for this subtree
	if (m_havelocalfields)
	    localfieldsfromconf();

	if (flg == FsTreeWalker::FtwDirReturn)
	    return FsTreeWalker::FtwOk;
    }

#ifdef IDX_THREADS
    if (m_haveInternQ) {
        InternfileTask *tp = new InternfileTask(fn, stp, m_localfields);
        if (m_iwqueue.put(tp)) {
            return FsTreeWalker::FtwOk;
        } else {
	    return FsTreeWalker::FtwError;
        }
    }
#endif

    return processonefile(m_config, fn, stp, m_localfields);
}

// File name transcoded to utf8 for indexing.  If this fails, the file
// name won't be indexed, no big deal Note that we used to do the full
// path here, but I ended up believing that it made more sense to use
// only the file name The charset is used is the one from the locale.
static string compute_utf8fn(RclConfig *config, const string& fn)
{
    string charset = config->getDefCharset(true);
    string utf8fn; 
    int ercnt;
    if (!transcode(path_getsimple(fn), utf8fn, charset, "UTF-8", &ercnt)) {
	LOGERR(("processone: fn transcode failure from [%s] to UTF-8: %s\n",
		charset.c_str(), path_getsimple(fn).c_str()));
    } else if (ercnt) {
	LOGDEB(("processone: fn transcode %d errors from [%s] to UTF-8: %s\n",
		ercnt, charset.c_str(), path_getsimple(fn).c_str()));
    }
    LOGDEB2(("processone: fn transcoded from [%s] to [%s] (%s->%s)\n",
	     path_getsimple(fn).c_str(), utf8fn.c_str(), charset.c_str(), 
	     "UTF-8"));
    return utf8fn;
}

FsTreeWalker::Status 
FsIndexer::processonefile(RclConfig *config, 
			  const std::string &fn, const struct stat *stp,
			  const map<string, string>& localfields)
{
    ////////////////////
    // Check db up to date ? Doing this before file type
    // identification means that, if usesystemfilecommand is switched
    // from on to off it may happen that some files which are now
    // without mime type will not be purged from the db, resulting
    // in possible 'cannot intern file' messages at query time...

    // Document signature. This is based on m/ctime and size and used
    // for the uptodate check (the value computed here is checked
    // against the stored one). Changing the computation forces a full
    // reindex of course.
    string sig;
    makesig(stp, sig);
    string udi;
    make_udi(fn, cstr_null, udi);
    bool existingDoc;
    bool needupdate = m_db->needUpdate(udi, sig, &existingDoc);

    // If ctime (which we use for the sig) differs from mtime, then at most
    // the extended attributes were changed, no need to index content.
    // This unfortunately leaves open the case where the data was
    // modified, then the extended attributes, in which case we will
    // miss the data update. We would have to store both the mtime and
    // the ctime to avoid this
    bool xattronly = m_detectxattronly && !m_db->inFullReset() && 
	existingDoc && needupdate && (stp->st_mtime < stp->st_ctime);
    
    if (!needupdate) {
	LOGDEB0(("processone: up to date: %s\n", fn.c_str()));
	if (m_updater) {
#ifdef IDX_THREADS
	    PTMutexLocker locker(m_updater->m_mutex);
#endif
	    // Status bar update, abort request etc.
	    m_updater->status.fn = fn;
	    ++(m_updater->status.filesdone);
	    if (!m_updater->update()) {
		return FsTreeWalker::FtwStop;
	    }
	}
	return FsTreeWalker::FtwOk;
    }

    LOGDEB0(("processone: processing: [%s] %s\n", 
             displayableBytes(stp->st_size).c_str(), fn.c_str()));

    string utf8fn = compute_utf8fn(config, fn);

    // parent_udi is initially the same as udi, it will be used if there 
    // are subdocs.
    string parent_udi = udi;

    Rcl::Doc doc;
    char ascdate[30];
    sprintf(ascdate, "%ld", long(stp->st_mtime));

    bool hadNullIpath = false;
    string mimetype;

    if (!xattronly) {
	FileInterner interner(fn, stp, config, FileInterner::FIF_none);
	if (!interner.ok()) {
	    // no indexing whatsoever in this case. This typically means that
	    // indexallfilenames is not set
	    return FsTreeWalker::FtwOk;
	}
	mimetype = interner.getMimetype();

	interner.setMissingStore(m_missing);
	FileInterner::Status fis = FileInterner::FIAgain;
	bool hadNonNullIpath = false;
	while (fis == FileInterner::FIAgain) {
	    doc.erase();
	    try {
		fis = interner.internfile(doc);
	    } catch (CancelExcept) {
		LOGERR(("fsIndexer::processone: interrupted\n"));
		return FsTreeWalker::FtwStop;
	    }

	    // We index at least the file name even if there was an error.
	    // We'll change the signature to ensure that the indexing will
	    // be retried every time.
            
            // If there is an error and the base doc was already seen,
            // we're done
            if (fis == FileInterner::FIError && hadNullIpath)
		return FsTreeWalker::FtwOk;

	    // Internal access path for multi-document files. If empty, this is
	    // for the main file.
	    if (doc.ipath.empty()) {
		hadNullIpath = true;
		if (hadNonNullIpath) {
		    // Note that only the filters can reliably compute
		    // this. What we do is dependant of the doc order (if
		    // we see the top doc first, we won't set the flag)
		    doc.haschildren = true;
		}
	    } else {
		hadNonNullIpath = true;
		make_udi(fn, doc.ipath, udi);
	    }

	    // Set file name, mod time and url if not done by
            // filter. We used to set the top-level container file
            // name for all subdocs without a proper file name, but
            // this did not make sense (resulted in multiple not
            // useful hits on the subdocs when searching for the
            // file name).
	    if (doc.fmtime.empty())
		doc.fmtime = ascdate;
	    if (doc.url.empty())
		doc.url = cstr_fileu + fn;
	    const string *fnp = 0;
	    if (doc.ipath.empty()) {
                if (!doc.peekmeta(Rcl::Doc::keyfn, &fnp) || fnp->empty())
                    doc.meta[Rcl::Doc::keyfn] = utf8fn;
            } 
            // Set container file name for all docs, top or subdoc
            doc.meta[Rcl::Doc::keytcfn] = utf8fn;

	    char cbuf[100]; 
	    sprintf(cbuf, "%lld", (long long)stp->st_size);
	    doc.pcbytes = cbuf;
	    // Document signature for up to date checks. All subdocs inherit the
	    // file's.
	    doc.sig = sig;

	    // If there was an error, ensure indexing will be
	    // retried. This is for the once missing, later installed
	    // filter case. It can make indexing much slower (if there are
	    // myriads of such files, the ext script is executed for them
	    // and fails every time)
	    if (fis == FileInterner::FIError) {
		doc.sig += cstr_plus;
	    }

	    // Possibly add fields from local config
	    if (m_havelocalfields) 
		setlocalfields(localfields, doc);

	    // Add document to database. If there is an ipath, add it
	    // as a child of the file document.
#ifdef IDX_THREADS
	    if (m_haveSplitQ) {
		DbUpdTask *tp = new DbUpdTask(udi, doc.ipath.empty() ? 
					      cstr_null : parent_udi, doc);
		if (!m_dwqueue.put(tp)) {
		    LOGERR(("processonefile: wqueue.put failed\n"));
		    return FsTreeWalker::FtwError;
		} 
	    } else {
#endif
		if (!m_db->addOrUpdate(udi, doc.ipath.empty() ? 
				       cstr_null : parent_udi, doc)) {
		    return FsTreeWalker::FtwError;
		}
#ifdef IDX_THREADS
	    }
#endif

	    // Tell what we are doing and check for interrupt request
	    if (m_updater) {
#ifdef IDX_THREADS
		PTMutexLocker locker(m_updater->m_mutex);
#endif
		++(m_updater->status.docsdone);
		if (m_updater->status.dbtotdocs < m_updater->status.docsdone)
		    m_updater->status.dbtotdocs = m_updater->status.docsdone;
		m_updater->status.fn = fn;
		if (!doc.ipath.empty())
		    m_updater->status.fn += "|" + doc.ipath;
		if (!m_updater->update()) {
		    return FsTreeWalker::FtwStop;
		}
	    }
	}

	// If this doc existed and it's a container, recording for
	// possible subdoc purge (this will be used only if we don't do a
	// db-wide purge, e.g. if we're called from indexfiles()).
	LOGDEB2(("processOnefile: existingDoc %d hadNonNullIpath %d\n",
		 existingDoc, hadNonNullIpath));
	if (existingDoc && hadNonNullIpath) {
	    m_purgeCandidates.record(parent_udi);
	}
    }

    // If we had no instance with a null ipath, we create an empty
    // document to stand for the file itself, to be used mainly for up
    // to date checks. Typically this happens for an mbox file.
    //
    // If xattronly is set, ONLY the extattr metadata is valid and will be used
    // by the following step.
    if (xattronly || hadNullIpath == false) {
	LOGDEB(("Creating empty doc for file or pure xattr update\n"));
	Rcl::Doc fileDoc;
	if (xattronly) {
	    map<string, string> xfields;
	    reapXAttrs(config, fn, xfields);
	    docFieldsFromXattrs(config, xfields, fileDoc);
	    fileDoc.onlyxattr = true;
	} else {
	    fileDoc.fmtime = ascdate;
	    fileDoc.meta[Rcl::Doc::keyfn] = 
                fileDoc.meta[Rcl::Doc::keytcfn] = utf8fn;
	    fileDoc.haschildren = true;
	    fileDoc.mimetype = mimetype;
	    fileDoc.url = cstr_fileu + fn;
	    if (m_havelocalfields) 
		setlocalfields(localfields, fileDoc);
	    char cbuf[100]; 
	    sprintf(cbuf, "%lld", (long long)stp->st_size);
	    fileDoc.pcbytes = cbuf;
	}

	fileDoc.sig = sig;

#ifdef IDX_THREADS
	if (m_haveSplitQ) {
	    DbUpdTask *tp = new DbUpdTask(parent_udi, cstr_null, fileDoc);
	    if (!m_dwqueue.put(tp))
		return FsTreeWalker::FtwError;
	    else
		return FsTreeWalker::FtwOk;
	}
#endif
	if (!m_db->addOrUpdate(parent_udi, cstr_null, fileDoc)) 
	    return FsTreeWalker::FtwError;
    }

    return FsTreeWalker::FtwOk;
}
