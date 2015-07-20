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
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>

#include <iostream>
#include <list>
#include <string>
#include <cstdlib>

using namespace std;

#include "debuglog.h"
#include "rclinit.h"
#include "indexer.h"
#include "smallut.h"
#include "pathut.h"
#include "rclmon.h"
#include "x11mon.h"
#include "cancelcheck.h"
#include "rcldb.h"
#include "beaglequeue.h"
#include "recollindex.h"
#include "fsindexer.h"
#include "rclionice.h"
#include "execmd.h"

// Command line options
static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_z     0x2 
#define OPT_h     0x4 
#define OPT_i     0x8
#define OPT_s     0x10
#define OPT_c     0x20
#define OPT_S     0x40
#define OPT_m     0x80
#define OPT_D     0x100
#define OPT_e     0x200
#define OPT_w     0x400
#define OPT_x     0x800
#define OPT_l     0x1000
#define OPT_b     0x2000
#define OPT_f     0x4000
#define OPT_C     0x8000
#define OPT_Z     0x10000
#define OPT_n     0x20000
#define OPT_r     0x40000

ReExec *o_reexec;

// Globals for atexit cleanup
static ConfIndexer *confindexer;

// This is set as an atexit routine, 
static void cleanup()
{
    deleteZ(confindexer);
}

// Global stop request flag. This is checked in a number of place in the
// indexing routines.
int stopindexing;

// Receive status updates from the ongoing indexing operation
// Also check for an interrupt request and return the info to caller which
// should subsequently orderly terminate what it is doing.
class MyUpdater : public DbIxStatusUpdater {
 public:
    MyUpdater(const RclConfig *config) 
	: m_prevphase(DbIxStatus::DBIXS_NONE)
    {
	m_fd = open(config->getIdxStatusFile().c_str(), 
		    O_WRONLY|O_CREAT|O_TRUNC, 0600);
	if (m_fd < 0)
	    LOGERR(("Can't open/create status file: [%s]\n",
		    config->getIdxStatusFile().c_str()));
    }

    virtual bool update() 
    {
	// Update the status file. Avoid doing it too often
	if (status.phase != m_prevphase || m_chron.millis() > 300) {
	    m_prevphase = status.phase;
	    m_chron.restart();
	    lseek(m_fd, 0, 0);
	    int fd1 = dup(m_fd);
	    FILE *fp = fdopen(fd1, "w");
	    fprintf(fp, "phase = %d\n", int(status.phase));
	    fprintf(fp, "docsdone = %d\n", status.docsdone);
	    fprintf(fp, "filesdone = %d\n", status.filesdone);
	    fprintf(fp, "dbtotdocs = %d\n", status.dbtotdocs);
	    fprintf(fp, "fn = %s\n", status.fn.c_str());
	    if (ftruncate(m_fd, off_t(ftell(fp))) < 0) {
		// ? kill compiler warning about ignoring ftruncate return
		LOGDEB(("Status update: ftruncate failed\n"));
	    }
            // Flush data and closes fd1. m_fd still valid
	    fclose(fp); 
	}

	if (stopindexing) {
	    return false;
	}

	// If we are in the monitor, we also need to check X11 status
	// during the initial indexing pass (else the user could log
	// out and the indexing would go on, not good (ie: if the user
	// logs in again, the new recollindex will fail).
	if ((op_flags & OPT_m) && !(op_flags & OPT_x) && !x11IsAlive()) {
	    LOGDEB(("X11 session went away during initial indexing pass\n"));
	    stopindexing = true;
	    return false;
	}

	return true;
    }

private:
    int    m_fd;
    Chrono m_chron;
    DbIxStatus::Phase m_prevphase;
};
static MyUpdater *updater;

static void sigcleanup(int sig)
{
    fprintf(stderr, "Got signal, registering stop request\n");
    LOGDEB(("Got signal, registering stop request\n"));
    CancelCheck::instance().setCancel();
    stopindexing = 1;
}

static void makeIndexerOrExit(RclConfig *config, bool inPlaceReset)
{
    if (!confindexer) {
	confindexer = new ConfIndexer(config, updater);
	if (inPlaceReset)
	    confindexer->setInPlaceReset();
    }
    if (!confindexer) {
        cerr << "Cannot create indexer" << endl;
        exit(1);
    }
}

void rclIxIonice(const RclConfig *config)
{
    string clss, classdata;
    if (!config->getConfParam("monioniceclass", clss) || clss.empty())
	clss = "3";
    config->getConfParam("monioniceclassdata", classdata);
    rclionice(clss, classdata);
}

class MakeListWalkerCB : public FsTreeWalkerCB {
public:
    MakeListWalkerCB(list<string>& files)
	: m_files(files)
    {
    }
    virtual FsTreeWalker::Status 
    processone(const string & fn, const struct stat *, FsTreeWalker::CbFlag flg) 
    {
	if (flg == FsTreeWalker::FtwDirEnter || flg == FsTreeWalker::FtwRegular)
	    m_files.push_back(fn);
	return FsTreeWalker::FtwOk;
    }
    list<string>& m_files;
};

// Build a list of things to index and call indexfiles.
bool recursive_index(RclConfig *config, const string& top)
{
    list<string> files;
    MakeListWalkerCB cb(files);
    FsTreeWalker walker;
    walker.walk(top, cb);
    return indexfiles(config, files);
}

// Index a list of files. We just call the top indexer method, which
// will sort out what belongs to the indexed trees and call the
// appropriate indexers.
//
// This is called either from the command line or from the monitor. In
// this case we're called repeatedly in the same process, and the
// confindexer is only created once by makeIndexerOrExit (but the db closed and
// flushed every time)
bool indexfiles(RclConfig *config, list<string> &filenames)
{
    if (filenames.empty())
	return true;
    makeIndexerOrExit(config, (op_flags & OPT_Z) != 0);
    return confindexer->indexFiles(filenames, (op_flags&OPT_f) ? 
				   ConfIndexer::IxFIgnoreSkip : 
				   ConfIndexer::IxFNone);
}

// Delete a list of files. Same comments about call contexts as indexfiles.
bool purgefiles(RclConfig *config, list<string> &filenames)
{
    if (filenames.empty())
	return true;
    makeIndexerOrExit(config, (op_flags & OPT_Z) != 0);
    return confindexer->purgeFiles(filenames, ConfIndexer::IxFNone);
}

// Create stemming and spelling databases
bool createAuxDbs(RclConfig *config)
{
    makeIndexerOrExit(config, false);

    if (!confindexer->createStemmingDatabases())
	return false;

    if (!confindexer->createAspellDict())
	return false;

    return true;
}

// Create additional stem database 
static bool createstemdb(RclConfig *config, const string &lang)
{
    makeIndexerOrExit(config, false);
    return confindexer->createStemDb(lang);
}

static const char *thisprog;

static const char usage [] =
"\n"
"recollindex [-h] \n"
"    Print help\n"
"recollindex [-z|-Z] \n"
"    Index everything according to configuration file\n"
"    -z : reset database before starting indexing\n"
"    -Z : in place reset: consider all documents as changed. Can also\n"
"         be combined with -i or -r but not -m\n"
#ifdef RCL_MONITOR
"recollindex -m [-w <secs>] -x [-D] [-C]\n"
"    Perform real time indexing. Don't become a daemon if -D is set.\n"
"    -w sets number of seconds to wait before starting.\n"
"    -C disables monitoring config for changes/reexecuting.\n"
"    -n disables initial incremental indexing (!and purge!).\n"
#ifndef DISABLE_X11MON
"    -x disables exit on end of x11 session\n"
#endif /* DISABLE_X11MON */
#endif /* RCL_MONITOR */
"recollindex -e <filename [filename ...]>\n"
"    Purge data for individual files. No stem database updates\n"
"recollindex -i [-f] [-Z] <filename [filename ...]>\n"
"    Index individual files. No database purge or stem database updates\n"
"    -f : ignore skippedPaths and skippedNames while doing this\n"
"recollindex -r [-f] [-Z] <top> \n"
"   Recursive partial reindex\n"
"recollindex -l\n"
"    List available stemming languages\n"
"recollindex -s <lang>\n"
"    Build stem database for additional language <lang>\n"
#ifdef FUTURE_IMPROVEMENT
"recollindex -b\n"
"    Process the Beagle queue\n"
#endif
#ifdef RCL_USE_ASPELL
"recollindex -S\n"
"    Build aspell spelling dictionary.>\n"
#endif
"Common options:\n"
"    -c <configdir> : specify config directory, overriding $RECOLL_CONFDIR\n"
;

static void
Usage(FILE *where = stderr)
{
    FILE *fp = (op_flags & OPT_h) ? stdout : stderr;
    fprintf(fp, "%s: Usage: %s", thisprog, usage);
    fprintf(fp, "Recoll version: %s\n", Rcl::version_string().c_str());
    exit((op_flags & OPT_h)==0);
}

static RclConfig *config;

void lockorexit(Pidfile *pidfile)
{
    pid_t pid;
    if ((pid = pidfile->open()) != 0) {
	cerr << "Can't become exclusive indexer: " << pidfile->getreason() << 
	    ". Return (other pid?): " << pid << endl;
	exit(1);
    }
    if (pidfile->write_pid() != 0) {
	cerr << "Can't become exclusive indexer: " << pidfile->getreason() <<
	    endl;
	exit(1);
    }
}

int main(int argc, char **argv)
{
    string a_config;
    int sleepsecs = 60;

    // The reexec struct is used by the daemon to shed memory after
    // the initial indexing pass and to restart when the configuration
    // changes
    o_reexec = new ReExec;
    o_reexec->init(argc, argv);

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'b': op_flags |= OPT_b; break;
	    case 'c':	op_flags |= OPT_c; if (argc < 2)  Usage();
		a_config = *(++argv);
		argc--; goto b1;
#ifdef RCL_MONITOR
	    case 'C': op_flags |= OPT_C; break;
	    case 'D': op_flags |= OPT_D; break;
#endif
	    case 'e': op_flags |= OPT_e; break;
	    case 'f': op_flags |= OPT_f; break;
	    case 'h': op_flags |= OPT_h; break;
	    case 'i': op_flags |= OPT_i; break;
	    case 'l': op_flags |= OPT_l; break;
	    case 'm': op_flags |= OPT_m; break;
	    case 'n': op_flags |= OPT_n; break;
	    case 'r': op_flags |= OPT_r; break;
	    case 's': op_flags |= OPT_s; break;
#ifdef RCL_USE_ASPELL
	    case 'S': op_flags |= OPT_S; break;
#endif
	    case 'w':	op_flags |= OPT_w; if (argc < 2)  Usage();
		if ((sscanf(*(++argv), "%d", &sleepsecs)) != 1) 
		    Usage(); 
		argc--; goto b1;
	    case 'x': op_flags |= OPT_x; break;
	    case 'Z': op_flags |= OPT_Z; break;
	    case 'z': op_flags |= OPT_z; break;
	    default: Usage(); break;
	    }
    b1: argc--; argv++;
    }
    if (op_flags & OPT_h)
	Usage(stdout);
#ifndef RCL_MONITOR
    if (op_flags & (OPT_m | OPT_w|OPT_x)) {
	cerr << "Sorry, -m not available: real-time monitoring was not "
	    "configured in this build\n";
	exit(1);
    }
#endif

    if ((op_flags & OPT_z) && (op_flags & (OPT_i|OPT_e|OPT_r)))
	Usage();
    if ((op_flags & OPT_Z) && (op_flags & (OPT_m)))
	Usage();

    string reason;
    RclInitFlags flags = (op_flags & OPT_m) && !(op_flags&OPT_D) ? 
	RCLINIT_DAEMON : RCLINIT_NONE;
    config = recollinit(flags, cleanup, sigcleanup, reason, &a_config);
    if (config == 0 || !config->ok()) {
	cerr << "Configuration problem: " << reason << endl;
	exit(1);
    }
    o_reexec->atexit(cleanup);

    string rundir;
    config->getConfParam("idxrundir", rundir);
    if (!rundir.compare("tmp")) {
	LOGINFO(("recollindex: changing current directory to [%s]\n",
		tmplocation().c_str()));
	if (chdir(tmplocation().c_str()) < 0) {
	    LOGERR(("chdir(%s) failed, errno %d\n", 
		    tmplocation().c_str(), errno));
	}
    } else if (!rundir.empty()) {
	LOGINFO(("recollindex: changing current directory to [%s]\n",
		 rundir.c_str()));
	if (chdir(rundir.c_str()) < 0) {
	    LOGERR(("chdir(%s) failed, errno %d\n", 
		    rundir.c_str(), errno));
	}
    }

    bool rezero((op_flags & OPT_z) != 0);
    bool inPlaceReset((op_flags & OPT_Z) != 0);
    Pidfile pidfile(config->getPidfile());
    updater = new MyUpdater(config);

    // Log something at LOGINFO to reset the trace file. Else at level
    // 3 it's not even truncated if all docs are up to date.
    LOGINFO(("recollindex: starting up\n"));

    if (setpriority(PRIO_PROCESS, 0, 20) != 0) {
        LOGINFO(("recollindex: can't setpriority(), errno %d\n", errno));
    }
    // Try to ionice. This does not work on all platforms
    rclIxIonice(config);

    if (op_flags & (OPT_i|OPT_e)) {
	lockorexit(&pidfile);

	list<string> filenames;

	if (argc == 0) {
	    // Read from stdin
	    char line[1024];
	    while (fgets(line, 1023, stdin)) {
		string sl(line);
		trimstring(sl, "\n\r");
		filenames.push_back(sl);
	    }
	} else {
	    while (argc--) {
		filenames.push_back(*argv++);
	    }
	}

        // Note that -e and -i may be both set. In this case we first erase,
        // then index. This is a slightly different from -Z -i because we 
        // warranty that all subdocs are purged.
        bool status = true;
	if (op_flags & OPT_e) {
	    status = purgefiles(config, filenames);
        }
        if (status && (op_flags & OPT_i)) {
	    status = indexfiles(config, filenames);
        }
        if (confindexer && !confindexer->getReason().empty())
            cerr << confindexer->getReason() << endl;
        exit(status ? 0 : 1);
    } else if (op_flags & OPT_r) {
	if (argc != 1) 
	    Usage();
	string top = *argv++; argc--;
	bool status = recursive_index(config, top);
        if (confindexer && !confindexer->getReason().empty())
            cerr << confindexer->getReason() << endl;
        exit(status ? 0 : 1);
    } else if (op_flags & OPT_l) {
	if (argc != 0) 
	    Usage();
	vector<string> stemmers = ConfIndexer::getStemmerNames();
	for (vector<string>::const_iterator it = stemmers.begin(); 
	     it != stemmers.end(); it++) {
	    cout << *it << endl;
	}
	exit(0);
    } else if (op_flags & OPT_s) {
	if (argc != 1) 
	    Usage();
	string lang = *argv++; argc--;
	exit(!createstemdb(config, lang));
#ifdef RCL_USE_ASPELL
    } else if (op_flags & OPT_S) {
	makeIndexerOrExit(config, false);
        exit(!confindexer->createAspellDict());
#endif // ASPELL

#ifdef RCL_MONITOR
    } else if (op_flags & OPT_m) {
	if (argc != 0) 
	    Usage();
	lockorexit(&pidfile);
	if (!(op_flags&OPT_D)) {
	    LOGDEB(("recollindex: daemonizing\n"));
	    if (daemon(0,0) != 0) {
	      fprintf(stderr, "daemon() failed, errno %d\n", errno);
	      LOGERR(("daemon() failed, errno %d\n", errno));
	      exit(1);
	    }
	}
	// Need to rewrite pid, it changed
	pidfile.write_pid();

        // Not too sure if I have to redo the nice thing after daemon(),
        // can't hurt anyway (easier than testing on all platforms...)
        if (setpriority(PRIO_PROCESS, 0, 20) != 0) {
            LOGINFO(("recollindex: can't setpriority(), errno %d\n", errno));
        }
	// Try to ionice. This does not work on all platforms
	rclIxIonice(config);

	if (sleepsecs > 0) {
	    LOGDEB(("recollindex: sleeping %d\n", sleepsecs));
	    for (int i = 0; i < sleepsecs; i++) {
	      sleep(1);
	      // Check that x11 did not go away while we were sleeping.
	      if (!(op_flags & OPT_x) && !x11IsAlive()) {
		LOGDEB(("X11 session went away during initial sleep period\n"));
		exit(0);
	      }
	    }
	}
	if (!(op_flags & OPT_n)) {
	    makeIndexerOrExit(config, inPlaceReset);
	    LOGDEB(("Recollindex: initial indexing pass before monitoring\n"));
	    if (!confindexer->index(rezero, ConfIndexer::IxTAll) || 
		stopindexing) {
		LOGERR(("recollindex, initial indexing pass failed, "
			"not going into monitor mode\n"));
		exit(1);
	    }
	    deleteZ(confindexer);
	    o_reexec->insertArgs(vector<string>(1, "-n"));
	    LOGINFO(("recollindex: reexecuting with -n after initial full pass\n"));
	    // Note that -n will be inside the reexec when we come
	    // back, but the monitor will explicitely strip it before
	    // starting a config change exec to ensure that we do a
	    // purging pass in this case.
	    o_reexec->reexec();
	}
        if (updater) {
	    updater->status.phase = DbIxStatus::DBIXS_MONITOR;
	    updater->status.fn.clear();
	    updater->update();
	}
	int opts = RCLMON_NONE;
	if (op_flags & OPT_D)
	    opts |= RCLMON_NOFORK;
	if (op_flags & OPT_C)
	    opts |= RCLMON_NOCONFCHECK;
	if (op_flags & OPT_x)
	    opts |= RCLMON_NOX11;
	bool monret = startMonitor(config, opts);
	MONDEB(("Monitor returned %d, exiting\n", monret));
	exit(monret == false);
#endif // MONITOR

    } else if (op_flags & OPT_b) {
        cerr << "Not yet" << endl;
        return 1;
    } else {
	lockorexit(&pidfile);
	makeIndexerOrExit(config, inPlaceReset);
	bool status = confindexer->index(rezero, ConfIndexer::IxTAll);
	if (!status) 
	    cerr << "Indexing failed" << endl;
        if (!confindexer->getReason().empty())
            cerr << confindexer->getReason() << endl;

        if (updater) {
	    updater->status.phase = DbIxStatus::DBIXS_DONE;
	    updater->status.fn.clear();
	    updater->update();
	}
	return !status;
    }
}
