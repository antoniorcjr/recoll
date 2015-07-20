#include "autoconfig.h"

#ifdef RCL_MONITOR
/* Copyright (C) 2006 J.F.Dockes 
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

/**
 * Recoll real time monitor processing. This file has the code to retrieve
 * event from the event queue and do the database-side processing. Also the 
 * initialization function.
 */

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fnmatch.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <vector>
using std::list;
using std::vector;

#include "debuglog.h"
#include "rclmon.h"
#include "debuglog.h"
#include "execmd.h"
#include "recollindex.h"
#include "pathut.h"
#include "x11mon.h"
#include "subtreelist.h"

typedef unsigned long mttcast;

static pthread_t rcv_thrid;

// Seconds between auxiliary db (stem, spell) updates:
static const int dfltauxinterval = 60 *60;
static int auxinterval = dfltauxinterval;

// Seconds between indexing queue processing: for merging events to 
// fast changing files and saving some of the indexing overhead.
static const int dfltixinterval = 30;
static int ixinterval = dfltixinterval;

static RclMonEventQueue rclEQ;

// 
// Delayed events: this is a special feature for fast changing files.
// A list of pattern/delays can be specified in the configuration so
// that they don't get re-indexed before some timeout is elapsed. Such
// events are kept on a separate queue (m_dqueue) with an auxiliary
// list in time-to-reindex order, while the normal events are on
// m_iqueue.

// Queue management performance: on a typical recoll system there will
// be only a few entries on the event queues and no significant time
// will be needed to manage them. Even on a busy system, the time used
// would most probably be negligible compared to the actual processing
// of the indexing events. So this is just for reference. Let I be the
// number of immediate events and D the number of delayed ones, N
// stands for either.
//
// Periodic timeout polling: the recollindex process periodically (2S)
// wakes up to check for exit requests. At this time it also checks
// the queues for new entries (should not happen because the producer
// would normally wake up the consumer threads), or ready entries
// among the delayed ones. At this time it calls the "empty()"
// routine. This has constant time behaviour (checks for stl container
// emptiness and the top entry of the delays list).
//
// Adding a new event (pushEvent()): this performs a search for an
// existing event with the same path (O(log(N)), then an insert on the
// appropriate queue (O(log(N))) and an insert on the times list (O(D)). 
//  
// Popping an event: this is constant time as it just looks at the
// tops of the normal and delayed queues.


// Indexing event container: a map indexed by file path for fast
// insertion of duplicate events to the same file
typedef map<string, RclMonEvent> queue_type;

// Entries for delayed events are duplicated (as iterators) on an
// auxiliary, sorted by time-to-reindex list. We could get rid of
// this, the price would be that the RclEQ.empty() call would have to
// walk the whole queue instead of only looking at the first delays
// entry.
typedef list<queue_type::iterator> delays_type;

// DelayPat stores a path wildcard pattern and a minimum time between
// reindexes, it is read from the recoll configuration
struct DelayPat {
    string pattern;
    int    seconds;
    DelayPat() : seconds(0) {}
};

/** Private part of RclEQ: things that we don't wish to exist in the interface
 *  include file.
 */
class RclEQData {
public:
    int        m_opts;
    // Queue for normal files (unlimited reindex)
    queue_type m_iqueue;
    // Queue for delayed reindex files
    queue_type m_dqueue;
    // The delays list stores pointers (iterators) to elements on
    // m_dqueue.  The list is kept in time-to-index order. Elements of
    // m_dqueue which are also in m_delays can only be deleted while
    // walking m_delays, so we are certain that the m_dqueue iterators
    // stored in m_delays remain valid.
    delays_type m_delays;
    // Configured intervals for path patterns, read from the configuration.
    vector<DelayPat> m_delaypats;
    RclConfig *m_config;
    bool       m_ok;
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
    RclEQData() 
	: m_config(0), m_ok(false)
    {
	if (!pthread_mutex_init(&m_mutex, 0) && !pthread_cond_init(&m_cond, 0))
	    m_ok = true;
    }
    void readDelayPats(int dfltsecs);
    DelayPat searchDelayPats(const string& path)
    {
	for (vector<DelayPat>::iterator it = m_delaypats.begin();
	     it != m_delaypats.end(); it++) {
	    if (fnmatch(it->pattern.c_str(), path.c_str(), 0) == 0) {
		return *it;
	    }
	}
	return DelayPat();
    }
    void delayInsert(const queue_type::iterator &qit);
};

void RclEQData::readDelayPats(int dfltsecs)
{
    if (m_config == 0)
	return;
    string patstring;
    if (!m_config->getConfParam("mondelaypatterns", patstring) || 
	patstring.empty())
	return;

    vector<string> dplist;
    if (!stringToStrings(patstring, dplist)) {
	LOGERR(("rclEQData: bad pattern list: [%s]\n", patstring.c_str()));
	return;
    }

    for (vector<string>::iterator it = dplist.begin(); 
	 it != dplist.end(); it++) {
	string::size_type pos = it->find_last_of(":");
	DelayPat dp;
	dp.pattern = it->substr(0, pos);
	if (pos != string::npos && pos != it->size()-1) {
	    dp.seconds = atoi(it->substr(pos+1).c_str());
	} else {
	    dp.seconds = dfltsecs;
	}
	m_delaypats.push_back(dp);
	LOGDEB2(("rclmon::readDelayPats: add [%s] %d\n", 
		 dp.pattern.c_str(), dp.seconds));
    }
}

// Insert event (as queue iterator) into delays list, in time order,
// We DO NOT take care of duplicate qits. erase should be called first
// when necessary.
void RclEQData::delayInsert(const queue_type::iterator &qit)
{
    MONDEB(("RclEQData::delayInsert: minclock %lu\n", 
            (mttcast)qit->second.m_minclock));
    for (delays_type::iterator dit = m_delays.begin(); 
	 dit != m_delays.end(); dit++) {
	queue_type::iterator qit1 = *dit;
	if ((*qit1).second.m_minclock > qit->second.m_minclock) {
	    m_delays.insert(dit, qit);
	    return;
	}
    }
    m_delays.push_back(qit);
}

RclMonEventQueue::RclMonEventQueue()
{
    m_data = new RclEQData;
}

RclMonEventQueue::~RclMonEventQueue()
{
    delete m_data;
}

void RclMonEventQueue::setopts(int opts)
{
    if (m_data)
	m_data->m_opts = opts;
}

/** Wait until there is something to process on the queue, or timeout.
 *  Must be called with the queue locked 
 */
bool RclMonEventQueue::wait(int seconds, bool *top)
{
    MONDEB(("RclMonEventQueue::wait\n"));
    if (!empty()) {
	MONDEB(("RclMonEventQueue:: imm return\n"));
	return true;
    }

    int err;
    if (seconds > 0) {
	struct timespec to;
	to.tv_sec = time(0L) + seconds;
	to.tv_nsec = 0;
	if (top)
	    *top = false;
	if ((err = 
	     pthread_cond_timedwait(&m_data->m_cond, &m_data->m_mutex, &to))) {
	    if (err == ETIMEDOUT) {
		*top = true;
		MONDEB(("RclMonEventQueue:: timeout\n"));
		return true;
	    }
	    LOGERR(("RclMonEventQueue::wait:pthread_cond_timedwait failed"
		    "with err %d\n", err));
	    return false;
	}
    } else {
	if ((err = pthread_cond_wait(&m_data->m_cond, &m_data->m_mutex))) {
	    LOGERR(("RclMonEventQueue::wait: pthread_cond_wait failed"
		    "with err %d\n", err));
	    return false;
	}
    }
    MONDEB(("RclMonEventQueue:: normal return\n"));
    return true;
}

bool RclMonEventQueue::lock()
{
    MONDEB(("RclMonEventQueue:: lock\n"));
    if (pthread_mutex_lock(&m_data->m_mutex)) {
	LOGERR(("RclMonEventQueue::lock: pthread_mutex_lock failed\n"));
	return false;
    }
    MONDEB(("RclMonEventQueue:: lock return\n"));
    return true;
}

bool RclMonEventQueue::unlock()
{
    MONDEB(("RclMonEventQueue:: unlock\n"));
    if (pthread_mutex_unlock(&m_data->m_mutex)) {
	LOGERR(("RclMonEventQueue::lock: pthread_mutex_unlock failed\n"));
	return false;
    }
    return true;
}

void RclMonEventQueue::setConfig(RclConfig *cnf)
{
    m_data->m_config = cnf;
    // Don't use ixinterval here, could be 0 ! Base the default
    // delayed reindex delay on the default ixinterval delay
    m_data->readDelayPats(10 * dfltixinterval);
}

RclConfig *RclMonEventQueue::getConfig()
{
    return m_data->m_config;
}

bool RclMonEventQueue::ok()
{
    if (m_data == 0) {
	LOGINFO(("RclMonEventQueue: not ok: bad state\n"));
	return false;
    }
    if (stopindexing) {
	LOGINFO(("RclMonEventQueue: not ok: stop request\n"));
	return false;
    }
    if (!m_data->m_ok) {
	LOGINFO(("RclMonEventQueue: not ok: queue terminated\n"));
	return false;
    }
    return true;
}

void RclMonEventQueue::setTerminate()
{
    MONDEB(("RclMonEventQueue:: setTerminate\n"));
    lock();
    m_data->m_ok = false;
    pthread_cond_broadcast(&m_data->m_cond);
    unlock();
}

// Must be called with the queue locked
bool RclMonEventQueue::empty()
{
    if (m_data == 0) {
	MONDEB(("RclMonEventQueue::empty(): true (m_data==0)\n"));
	return true;
    }
    if (!m_data->m_iqueue.empty()) {
	MONDEB(("RclMonEventQueue::empty(): false (m_iqueue not empty)\n"));
	return true;
    }
    if (m_data->m_dqueue.empty()) {
	MONDEB(("RclMonEventQueue::empty(): true (m_Xqueue both empty)\n"));
	return true;
    }
    // Only dqueue has events. Have to check the delays (only the
    // first, earliest one):
    queue_type::iterator qit = *(m_data->m_delays.begin());
    if (qit->second.m_minclock > time(0)) {
	MONDEB(("RclMonEventQueue::empty(): true (no delay ready %lu)\n",
		(mttcast)qit->second.m_minclock));
	return true;
    }
    MONDEB(("RclMonEventQueue::empty(): returning false (delay expired)\n"));
    return false;
}

// Retrieve indexing event for processing. Returns empty event if
// nothing interesting is found
// Must be called with the queue locked
RclMonEvent RclMonEventQueue::pop()
{
    time_t now = time(0);
    MONDEB(("RclMonEventQueue::pop(), now %lu\n", (mttcast)now));

    // Look at the delayed events, get rid of the expired/unactive
    // ones, possibly return an expired/needidx one.
    while (!m_data->m_delays.empty()) {
	delays_type::iterator dit = m_data->m_delays.begin();
	queue_type::iterator qit = *dit;
	MONDEB(("RclMonEventQueue::pop(): in delays: evt minclock %lu\n", 
		(mttcast)qit->second.m_minclock));
	if (qit->second.m_minclock <= now) {
	    if (qit->second.m_needidx) {
		RclMonEvent ev = qit->second;
		qit->second.m_minclock = time(0) + qit->second.m_itvsecs;
		qit->second.m_needidx = false;
		m_data->m_delays.erase(dit);
		m_data->delayInsert(qit);
		return ev;
	    } else {
		// Delay elapsed without new update, get rid of event.
		m_data->m_dqueue.erase(qit);
		m_data->m_delays.erase(dit);
	    }
	} else {
	    // This and following events are for later processing, we
	    // are done with the delayed event list.
	    break;
	}
    }

    // Look for non-delayed event 
    if (!m_data->m_iqueue.empty()) {
	queue_type::iterator qit = m_data->m_iqueue.begin();
	RclMonEvent ev = qit->second;
	m_data->m_iqueue.erase(qit);
	return ev;
    }

    return RclMonEvent();
}

// Add new event (update or delete) to the processing queue.
// It seems that a newer event is always correct to override any
// older. TBVerified ?
// Some conf-designated files, supposedly updated at a high rate get
// special processing to limit their reindexing rate.
bool RclMonEventQueue::pushEvent(const RclMonEvent &ev)
{
    MONDEB(("RclMonEventQueue::pushEvent for %s\n", ev.m_path.c_str()));
    lock();

    DelayPat pat = m_data->searchDelayPats(ev.m_path);
    if (pat.seconds != 0) {
	// Using delayed reindex queue. Need to take care of minclock and also
	// insert into the in-minclock-order list
	queue_type::iterator qit = m_data->m_dqueue.find(ev.m_path);
	if (qit == m_data->m_dqueue.end()) {
	    // Not there yet, insert new
	    qit = 
		m_data->m_dqueue.insert(queue_type::value_type(ev.m_path, ev)).first;
	    // Set the time to next index to "now" as it has not been
	    // indexed recently (otherwise it would still be in the
	    // queue), and add the iterator to the delay queue.
	    qit->second.m_minclock = time(0);
	    qit->second.m_needidx = true;
	    qit->second.m_itvsecs = pat.seconds;
	    m_data->delayInsert(qit);
	} else {
	    // Already in queue. Possibly update type but save minclock
	    // (so no need to touch m_delays). Flag as needing indexing
	    time_t saved_clock = qit->second.m_minclock;
	    qit->second = ev;
	    qit->second.m_minclock = saved_clock;
	    qit->second.m_needidx = true;
	}
    } else {
	// Immediate event: just insert it, erasing any previously
	// existing entry
	m_data->m_iqueue[ev.m_path] = ev;
    }

    pthread_cond_broadcast(&m_data->m_cond);
    unlock();
    return true;
}

static bool checkfileanddelete(const string& fname)
{
    bool ret;
    ret = access(fname.c_str(), 0) == 0;
    unlink(fname.c_str());
    return ret;
}

// It's possible to override the normal indexing delay by creating a
// file in the config directory (which we then remove). And yes there
// is definitely a race condition (we can suppress the delay and file
// before the target doc is queued), and we can't be sure that the
// delay suppression will be used for the doc the user intended it
// for. But this is used for non-critical function and the race
// condition should happen reasonably seldom.
// We check for the request file in all possible user config dirs
// (usually, there is only the main one)
static bool expeditedIndexingRequested(RclConfig *conf)
{
    static vector<string> rqfiles;
    if (rqfiles.empty()) {
	rqfiles.push_back(path_cat(conf->getConfDir(), "rclmonixnow"));
	const char *cp;
	if ((cp = getenv("RECOLL_CONFTOP"))) {
	    rqfiles.push_back(path_cat(cp, "rclmonixnow"));
	} 
	if ((cp = getenv("RECOLL_CONFMID"))) {
	    rqfiles.push_back(path_cat(cp, "rclmonixnow"));
	} 
    }
    bool found  = false;
    for (vector<string>::const_iterator it = rqfiles.begin(); 
	 it != rqfiles.end(); it++) {
	found = found || checkfileanddelete(*it);
    }
    return found;
}

bool startMonitor(RclConfig *conf, int opts)
{
    if (!conf->getConfParam("monauxinterval", &auxinterval))
	auxinterval = dfltauxinterval;
    if (!conf->getConfParam("monixinterval", &ixinterval))
	ixinterval = dfltixinterval;


    rclEQ.setConfig(conf);
    rclEQ.setopts(opts);

    if (pthread_create(&rcv_thrid, 0, &rclMonRcvRun, &rclEQ) != 0) {
	LOGERR(("startMonitor: cant create event-receiving thread\n"));
	return false;
    }

    if (!rclEQ.lock()) {
	LOGERR(("startMonitor: cant lock queue ???\n"));
	return false;
    }
    LOGDEB(("start_monitoring: entering main loop\n"));

    bool timedout;
    time_t lastauxtime = time(0);
    time_t lastixtime = lastauxtime;
    bool didsomething = false;
    list<string> modified;
    list<string> deleted;

    // Set a relatively short timeout for better monitoring of exit requests
    while (rclEQ.wait(2, &timedout)) {
	// Queue is locked.

	// x11IsAlive() can't be called from ok() because both threads call it
	// and Xlib is not multithreaded.
        bool x11dead = !(opts & RCLMON_NOX11) && !x11IsAlive();
        if (x11dead)
            LOGDEB(("RclMonprc: x11 is dead\n"));
	if (!rclEQ.ok() || x11dead) {
	    rclEQ.unlock();
	    break;
	}
	    
	// Process event queue
	for (;;) {
	    // Retrieve event
	    RclMonEvent ev = rclEQ.pop();
	    if (ev.m_path.empty())
		break;
	    switch (ev.evtype()) {
	    case RclMonEvent::RCLEVT_MODIFY:
	    case RclMonEvent::RCLEVT_DIRCREATE:
		LOGDEB0(("Monitor: Modify/Check on %s\n", ev.m_path.c_str()));
		modified.push_back(ev.m_path);
		break;
	    case RclMonEvent::RCLEVT_DELETE:
		LOGDEB0(("Monitor: Delete on %s\n", ev.m_path.c_str()));
		// If this is for a directory (which the caller should
		// tell us because he knows), we should purge the db
		// of all the subtree, because on a directory rename,
		// inotify will only generate one event for the
		// renamed top, not the subentries. This is relatively
		// complicated to do though, and we currently do not
		// do it, and just wait for a restart to do a full run and
		// purge.
		deleted.push_back(ev.m_path);
		if (ev.evflags() & RclMonEvent::RCLEVT_ISDIR) {
		    vector<string> paths;
		    if (subtreelist(conf, ev.m_path, paths)) {
			deleted.insert(deleted.end(),
				       paths.begin(), paths.end());
		    }
		}
		break;
	    default:
		LOGDEB(("Monitor: got Other on [%s]\n", ev.m_path.c_str()));
	    }
	}
	// Unlock queue before processing lists
	rclEQ.unlock();

	// Process. We don't do this every time but let the lists accumulate
        // a little, this saves processing. Start at once if list is big.
        time_t now = time(0);
        if (expeditedIndexingRequested(conf) ||
	    (now - lastixtime > ixinterval) || 
	    (deleted.size() + modified.size() > 20)) {
            lastixtime = now;
	    // Used to do the modified list first, but it does seem
	    // smarter to make room first...
            if (!deleted.empty()) {
                deleted.sort();
                deleted.unique();
                if (!purgefiles(conf, deleted))
                    break;
                deleted.clear();
                didsomething = true;
            }
            if (!modified.empty()) {
                modified.sort();
                modified.unique();
                if (!indexfiles(conf, modified))
                    break;
                modified.clear();
                didsomething = true;
            }
        }

	// Recreate the auxiliary dbs every hour at most.
	if (didsomething && time(0) - lastauxtime > auxinterval) {
	    lastauxtime = time(0);
	    didsomething = false;
	    if (!createAuxDbs(conf)) {
		// We used to bail out on error here. Not anymore,
		// because this is most of the time due to a failure
		// of aspell dictionary generation, which is not
		// critical.
	    }
	}

	// Check for a config change
	if (!(opts & RCLMON_NOCONFCHECK) && o_reexec && conf->sourceChanged()) {
	    LOGDEB(("Rclmonprc: config changed, reexecuting myself\n"));
	    // We never want to have a -n option after a config
	    // change. -n was added by the reexec after the initial
	    // pass even if it was not given on the command line
	    o_reexec->removeArg("-n");
	    o_reexec->reexec();
	}
	// Lock queue before waiting again
	rclEQ.lock();
    }
    LOGDEB(("Rclmonprc: calling queue setTerminate\n"));
    rclEQ.setTerminate();
    // Wait for receiver thread before returning
    pthread_join(rcv_thrid, 0);
    LOGDEB(("Monitor: returning\n"));
    return true;
}

#endif // RCL_MONITOR
