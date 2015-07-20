/* Copyright (C) 2012 J.F.Dockes
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

#include <sys/stat.h>
#include <errno.h>

#include "debuglog.h"
#include "rcldoc.h"

#include "fetcher.h"
#include "bglfetcher.h"
#include "debuglog.h"
#include "ptmutex.h"
#include "beaglequeuecache.h"

// We use a single beagle cache object to access beagle data. We protect it 
// against multiple thread access.
static PTMutexInit o_beagler_mutex;

bool BGLDocFetcher::fetch(RclConfig* cnf, const Rcl::Doc& idoc, RawDoc& out)
{
    string udi;
    if (!idoc.getmeta(Rcl::Doc::keyudi, &udi) || udi.empty()) {
	LOGERR(("BGLDocFetcher:: no udi in idoc\n"));
	return false;
    }
    Rcl::Doc dotdoc;
    {
	PTMutexLocker locker(o_beagler_mutex);
	// Retrieve from our webcache (beagle data). The beagler
	// object is created at the first call of this routine and
	// deleted when the program exits.
	static BeagleQueueCache o_beagler(cnf);
	if (!o_beagler.getFromCache(udi, dotdoc, out.data)) {
	    LOGINFO(("BGLDocFetcher::fetch: failed for [%s]\n", udi.c_str()));
	    return false;
	}
    }
    if (dotdoc.mimetype.compare(idoc.mimetype)) {
	LOGINFO(("BGLDocFetcher:: udi [%s], mimetp mismatch: in: [%s], bgl "
		 "[%s]\n", idoc.mimetype.c_str(), dotdoc.mimetype.c_str()));
    }
    out.kind = RawDoc::RDK_DATA;
    return true;
}
    
bool BGLDocFetcher::makesig(RclConfig* cnf, const Rcl::Doc& idoc, string& sig)
{
    // Bgl sigs are empty
    sig.clear();
    return true;
}

