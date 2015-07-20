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
#ifndef _BGLFETCHER_H_INCLUDED_
#define _BGLFETCHER_H_INCLUDED_
#include "fetcher.h"

/** 
 * The Beagle cache fetcher: 
 */
class BGLDocFetcher : public DocFetcher{
    virtual bool fetch(RclConfig* cnf, const Rcl::Doc& idoc, RawDoc& out);
    virtual bool makesig(RclConfig* cnf, const Rcl::Doc& idoc, string& sig);
    virtual ~BGLDocFetcher() {}
};

#endif /* _BGLFETCHER_H_INCLUDED_ */
