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
#include "cstr.h"

#include "fetcher.h"
#include "fsfetcher.h"
#include "fsindexer.h"
#include "debuglog.h"

using std::string;

static bool urltopath(RclConfig* cnf,
		      const Rcl::Doc& idoc, string& fn, struct stat& st)
{
    // The url has to be like file://
    if (idoc.url.find(cstr_fileu) != 0) {
	LOGERR(("FSDocFetcher::fetch/sig: non fs url: [%s]\n", 
		idoc.url.c_str()));
	return false;
    }
    fn = idoc.url.substr(7, string::npos);
    cnf->setKeyDir(path_getfather(fn));
    bool follow = false;
    cnf->getConfParam("followLinks", &follow);

    if ((follow ? stat(fn.c_str(), &st) : lstat(fn.c_str(), &st))< 0) {
	LOGERR(("FSDocFetcher::fetch: stat errno %d for [%s]\n", 
		errno, fn.c_str()));
	return false;
    }
    return true;
}

bool FSDocFetcher::fetch(RclConfig* cnf, const Rcl::Doc& idoc, RawDoc& out)
{
    string fn;
    if (!urltopath(cnf, idoc, fn, out.st))
	return false;
    out.kind = RawDoc::RDK_FILENAME;
    out.data = fn;
    return true;
}
    
bool FSDocFetcher::makesig(RclConfig* cnf, const Rcl::Doc& idoc, string& sig)
{
    string fn;
    struct stat st;
    if (!urltopath(cnf, idoc, fn, st))
	return false;
    FsIndexer::makesig(&st, sig);
    return true;
}

