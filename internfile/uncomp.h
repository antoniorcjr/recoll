/* Copyright (C) 2013 J.F.Dockes
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
#ifndef _UNCOMP_H_INCLUDED_
#define _UNCOMP_H_INCLUDED_

#include <vector>
#include <string>

#include "pathut.h"
#include "ptmutex.h"

/// Uncompression script interface.
class Uncomp {
public:
    Uncomp(bool docache = false)
	: m_dir(0), m_docache(docache)
    {
    }
    ~Uncomp();

    /** Uncompress the input file into a temporary one, by executing the
     * script given as input. 
     * Return the path to the uncompressed file (which is inside a 
     * temporary directory).
     */
    bool uncompressfile(const std::string& ifn, 
			const std::vector<std::string>& cmdv,
			std::string& tfile);

private:
    TempDir *m_dir;
    string   m_tfile;
    string   m_srcpath;
    bool m_docache;

    class UncompCache {
    public:
	UncompCache()
	    : m_dir(0)
	{
	}
	~UncompCache()
	{
	    delete m_dir;
	}
	PTMutexInit m_lock;
	TempDir *m_dir;
	string   m_tfile;
	string   m_srcpath;
    };
    static UncompCache o_cache;
};

#endif /* _UNCOMP_H_INCLUDED_ */
