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
#ifndef _beaglequeue_h_included_
#define _beaglequeue_h_included_

#include <list>
using std::list;

/**
 * Process the Beagle indexing queue. 
 *
 * Beagle MUST NOT be running, else mayhem will ensue. 
 *
 * This is mainly written to reuse the Beagle Firefox plug-in (which
 * copies visited pages and bookmarks to the queue).
 */

#include "fstreewalk.h"
#include "rcldoc.h"

class DbIxStatusUpdater;
class CirCache;
class RclConfig;
class BeagleQueueCache;
namespace Rcl {
    class Db;
}

class BeagleQueueIndexer : public FsTreeWalkerCB {
public:
    BeagleQueueIndexer(RclConfig *cnf, Rcl::Db *db,
                       DbIxStatusUpdater *updfunc = 0);
    ~BeagleQueueIndexer();

    /** This is called by the top indexer in recollindex. 
     *  Does the walking and the talking */
    bool index();

    /** Called when we fstreewalk the queue dir */
    FsTreeWalker::Status 
    processone(const string &, const struct stat *, FsTreeWalker::CbFlag);

    /** Index a list of files. No db cleaning or stemdb updating. 
     *  Used by the real time monitor */
    bool indexFiles(list<string>& files);
    /** Purge a list of files. No way to do this currently and dont want
     *  to do anything as this is mostly called by the monitor when *I* delete
     *  files inside the queue dir */
    bool purgeFiles(list<string>& files) {return true;}

    /** Called when indexing data from the cache, and from internfile for
     * search result preview */
    bool getFromCache(const string& udi, Rcl::Doc &doc, string& data,
                      string *hittype = 0);
private:
    RclConfig *m_config;
    Rcl::Db   *m_db;
    BeagleQueueCache  *m_cache;
    string     m_queuedir;
    DbIxStatusUpdater *m_updater;
    bool       m_nocacheindex;

    bool indexFromCache(const string& udi);
    void updstatus(const string& udi);
};

#endif /* _beaglequeue_h_included_ */
