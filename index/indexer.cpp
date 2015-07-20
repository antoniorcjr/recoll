/* Copyright (C) 2004 J.F.Dockes
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
#ifdef HAVE_CONFIG_H
#include "autoconfig.h"
#endif

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <algorithm>

#include "cstr.h"
#include "debuglog.h"
#include "indexer.h"
#include "fsindexer.h"
#include "beaglequeue.h"
#include "mimehandler.h"

#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif

ConfIndexer::ConfIndexer(RclConfig *cnf, DbIxStatusUpdater *updfunc)
    : m_config(cnf), m_db(cnf), m_fsindexer(0), 
      m_dobeagle(false), m_beagler(0),
      m_updater(updfunc)
{
    m_config->getConfParam("processwebqueue", &m_dobeagle);
}

ConfIndexer::~ConfIndexer()
{
     deleteZ(m_fsindexer);
     deleteZ(m_beagler);
}

// Determine if this is likely the first time that the user runs
// indexing.  We don't look at the xapiandb as this may have been
// explicitely removed for valid reasons, but at the indexing status
// file, which should be unexistant-or-empty only before any indexing
// has ever run
bool ConfIndexer::runFirstIndexing()
{
    // Indexing status file existing and not empty ?
    struct stat st;
    if (stat(m_config->getIdxStatusFile().c_str(), &st) == 0 && 
	st.st_size > 0) {
	LOGDEB0(("ConfIndexer::runFirstIndexing: no: status file not empty\n"));
	exit(1);
	return false;
    }
    // And only do this if the user has kept the default topdirs (~). 
    vector<string>tdl = m_config->getTopdirs();
    if (tdl.size() != 1 || tdl[0].compare(path_canon(path_tildexpand("~")))) {
	LOGDEB0(("ConfIndexer::runFirstIndexing: no: not home only\n"));
	return false;
    }
    return true;
}

bool ConfIndexer::firstFsIndexingSequence()
{
    LOGDEB(("ConfIndexer::firstFsIndexingSequence\n"));
    deleteZ(m_fsindexer);
    m_fsindexer = new FsIndexer(m_config, &m_db, m_updater);
    if (!m_fsindexer) {
	return false;
    }
    int flushmb = m_db.getFlushMb();
    m_db.setFlushMb(2);
    m_fsindexer->index(true);
    m_db.doFlush();
    m_db.setFlushMb(flushmb);
    return true;
}

bool ConfIndexer::index(bool resetbefore, ixType typestorun)
{
    Rcl::Db::OpenMode mode = resetbefore ? Rcl::Db::DbTrunc : Rcl::Db::DbUpd;
    if (!m_db.open(mode)) {
	LOGERR(("ConfIndexer: error opening database %s : %s\n", 
                m_config->getDbDir().c_str(), m_db.getReason().c_str()));
	return false;
    }

    m_config->setKeyDir(cstr_null);
    if (typestorun & IxTFs) {
	if (runFirstIndexing()) {
	    firstFsIndexingSequence();
	}
        deleteZ(m_fsindexer);
        m_fsindexer = new FsIndexer(m_config, &m_db, m_updater);
        if (!m_fsindexer || !m_fsindexer->index()) {
	    m_db.close();
            return false;
        }
    }

    if (m_dobeagle && (typestorun & IxTBeagleQueue)) {
        deleteZ(m_beagler);
        m_beagler = new BeagleQueueIndexer(m_config, &m_db, m_updater);
        if (!m_beagler || !m_beagler->index()) {
	    m_db.close();
            return false;
        }
    }

    if (typestorun == IxTAll) {
        // Get rid of all database entries that don't exist in the
        // filesystem anymore. Only if all *configured* indexers ran.
        if (m_updater && !m_updater->update(DbIxStatus::DBIXS_PURGE, string())) {
	    m_db.close();
	    return false;
	}
        m_db.purge();
    }

    // The close would be done in our destructor, but we want status
    // here. Makes no sense to check for cancel, we'll have to close
    // anyway
    if (m_updater)
	m_updater->update(DbIxStatus::DBIXS_CLOSING, string());
    if (!m_db.close()) {
	LOGERR(("ConfIndexer::index: error closing database in %s\n", 
		m_config->getDbDir().c_str()));
	return false;
    }

    if (m_updater && !m_updater->update(DbIxStatus::DBIXS_CLOSING, string()))
	return false;
    createStemmingDatabases();
    if (m_updater && !m_updater->update(DbIxStatus::DBIXS_CLOSING, string()))
	return false;
    createAspellDict();
    clearMimeHandlerCache();
    if (m_updater)
	m_updater->update(DbIxStatus::DBIXS_DONE, string());
    return true;
}

bool ConfIndexer::indexFiles(list<string>& ifiles, IxFlag flag)
{
    list<string> myfiles;
    string origcwd = m_config->getOrigCwd();
    for (list<string>::const_iterator it = ifiles.begin(); 
	 it != ifiles.end(); it++) {
	myfiles.push_back(path_canon(*it, &origcwd));
    }
    myfiles.sort();

    if (!m_db.open(Rcl::Db::DbUpd)) {
	LOGERR(("ConfIndexer: indexFiles error opening database %s\n", 
                m_config->getDbDir().c_str()));
	return false;
    }
    m_config->setKeyDir(cstr_null);
    bool ret = false;
    if (!m_fsindexer)
        m_fsindexer = new FsIndexer(m_config, &m_db, m_updater);
    if (m_fsindexer)
        ret = m_fsindexer->indexFiles(myfiles, flag);
    LOGDEB2(("ConfIndexer::indexFiles: fsindexer returned %d, "
            "%d files remainining\n", ret, myfiles.size()));

    if (m_dobeagle && !myfiles.empty() && !(flag & IxFNoWeb)) {
        if (!m_beagler)
            m_beagler = new BeagleQueueIndexer(m_config, &m_db, m_updater);
        if (m_beagler) {
            ret = ret && m_beagler->indexFiles(myfiles);
        } else {
            ret = false;
        }
    }

    // The close would be done in our destructor, but we want status here
    if (!m_db.close()) {
	LOGERR(("ConfIndexer::index: error closing database in %s\n", 
		m_config->getDbDir().c_str()));
	return false;
    }
    ifiles = myfiles;
    clearMimeHandlerCache();
    return ret;
}

bool ConfIndexer::docsToPaths(vector<Rcl::Doc> &docs, vector<string> &paths)
{
    for (vector<Rcl::Doc>::iterator it = docs.begin(); it != docs.end(); it++) {
	Rcl::Doc &idoc = *it;
	string backend;
	idoc.getmeta(Rcl::Doc::keybcknd, &backend);

	// This only makes sense for file system files: beagle docs are
	// always up to date because they can't be updated in the cache,
	// only added/removed. Same remark as made inside internfile, we
	// need a generic way to handle backends.
	if (!backend.empty() && backend.compare("FS"))
	    continue;

	// Filesystem document. The url has to be like file://
	if (idoc.url.find(cstr_fileu) != 0) {
	    LOGERR(("idx::docsToPaths: FS backend and non fs url: [%s]\n",
		    idoc.url.c_str()));
	    continue;
	}
	paths.push_back(idoc.url.substr(7, string::npos));
    }
    return true;
}

// Update index for specific documents. The docs come from an index
// query, so the udi, backend etc. fields are filled.
bool ConfIndexer::updateDocs(std::vector<Rcl::Doc> &docs, IxFlag flag)
{
    vector<string> paths;
    docsToPaths(docs, paths);
    list<string> files(paths.begin(), paths.end());
    if (!files.empty()) {
	return indexFiles(files, flag);
    }
    return true;
}

bool ConfIndexer::purgeFiles(std::list<string> &files, IxFlag flag)
{
    list<string> myfiles;
    string origcwd = m_config->getOrigCwd();
    for (list<string>::const_iterator it = files.begin(); 
	 it != files.end(); it++) {
	myfiles.push_back(path_canon(*it, &origcwd));
    }
    myfiles.sort();

    if (!m_db.open(Rcl::Db::DbUpd)) {
	LOGERR(("ConfIndexer: purgeFiles error opening database %s\n", 
                m_config->getDbDir().c_str()));
	return false;
    }
    bool ret = false;
    m_config->setKeyDir(cstr_null);
    if (!m_fsindexer)
        m_fsindexer = new FsIndexer(m_config, &m_db, m_updater);
    if (m_fsindexer)
        ret = m_fsindexer->purgeFiles(myfiles);

    if (m_dobeagle && !myfiles.empty() && !(flag & IxFNoWeb)) {
        if (!m_beagler)
            m_beagler = new BeagleQueueIndexer(m_config, &m_db, m_updater);
        if (m_beagler) {
            ret = ret && m_beagler->purgeFiles(myfiles);
        } else {
            ret = false;
        }
    }

    // The close would be done in our destructor, but we want status here
    if (!m_db.close()) {
	LOGERR(("ConfIndexer::purgefiles: error closing database in %s\n", 
		m_config->getDbDir().c_str()));
	return false;
    }
    return ret;
}

// Create stemming databases. We also remove those which are not
// configured. 
bool ConfIndexer::createStemmingDatabases()
{
    string slangs;
    if (m_config->getConfParam("indexstemminglanguages", slangs)) {
        if (!m_db.open(Rcl::Db::DbUpd)) {
            LOGERR(("ConfIndexer::createStemmingDb: could not open db\n"))
            return false;
        }
	vector<string> langs;
	stringToStrings(slangs, langs);

	// Get the list of existing stem dbs from the database (some may have 
	// been manually created, we just keep those from the config
	vector<string> dblangs = m_db.getStemLangs();
	vector<string>::const_iterator it;
	for (it = dblangs.begin(); it != dblangs.end(); it++) {
	    if (find(langs.begin(), langs.end(), *it) == langs.end())
		m_db.deleteStemDb(*it);
	}
	m_db.createStemDbs(langs);
    }
    m_db.close();
    return true;
}

bool ConfIndexer::createStemDb(const string &lang)
{
    if (!m_db.open(Rcl::Db::DbUpd))
	return false;
    vector<string> langs;
    stringToStrings(lang, langs);
    return m_db.createStemDbs(langs);
}

// The language for the aspell dictionary is handled internally by the aspell
// module, either from a configuration variable or the NLS environment.
bool ConfIndexer::createAspellDict()
{
    LOGDEB2(("ConfIndexer::createAspellDict()\n"));
#ifdef RCL_USE_ASPELL
    // For the benefit of the real-time indexer, we only initialize
    // noaspell from the configuration once. It can then be set to
    // true if dictionary generation fails, which avoids retrying
    // it forever.
    static int noaspell = -12345;
    if (noaspell == -12345) {
	noaspell = false;
	m_config->getConfParam("noaspell", &noaspell);
    }
    if (noaspell)
	return true;

    if (!m_db.open(Rcl::Db::DbRO)) {
        LOGERR(("ConfIndexer::createAspellDict: could not open db\n"));
	return false;
    }

    Aspell aspell(m_config);
    string reason;
    if (!aspell.init(reason)) {
	LOGERR(("ConfIndexer::createAspellDict: aspell init failed: %s\n", 
		reason.c_str()));
	noaspell = true;
	return false;
    }
    LOGDEB(("ConfIndexer::createAspellDict: creating dictionary\n"));
    if (!aspell.buildDict(m_db, reason)) {
	LOGERR(("ConfIndexer::createAspellDict: aspell buildDict failed: %s\n", 
		reason.c_str()));
	noaspell = true;
	return false;
    }
#endif
    return true;
}

vector<string> ConfIndexer::getStemmerNames()
{
    return Rcl::Db::getStemmerNames();
}
