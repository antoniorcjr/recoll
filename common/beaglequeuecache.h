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
#ifndef _beaglequeuecache_h_included_
#define _beaglequeuecache_h_included_

#include <string>
using std::string;

class RclConfig;
namespace Rcl {
    class Db;
    class Doc;
}
class CirCache;

/**
 * Manage the CirCache for the Beagle Queue indexer. Separated from the main
 * indexer code because it's also used for querying (getting the data for a
 * preview 
 */
class BeagleQueueCache {
public:
    BeagleQueueCache(RclConfig *config);
    ~BeagleQueueCache();

    bool getFromCache(const string& udi, Rcl::Doc &doc, string& data,
                      string *hittype = 0);
    // We could write proxies for all the circache ops, but why bother?
    CirCache *cc() {return m_cache;}

private:
    CirCache *m_cache;
};
extern const string cstr_bgc_mimetype;

#endif /* _beaglequeuecache_h_included_ */
