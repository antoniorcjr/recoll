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
#ifndef _READFILE_H_INCLUDED_
#define _READFILE_H_INCLUDED_

#include <sys/types.h>

#include <string>
using std::string;

/** 
 * Read file in chunks, calling an accumulator for each chunk. Can be used 
 * for reading in a file, computing an md5...
 */
class FileScanDo {
public:
    virtual ~FileScanDo() {}
    virtual bool init(size_t size, string *reason) = 0;
    virtual bool data(const char *buf, int cnt, string* reason) = 0;
};
bool file_scan(const string &filename, FileScanDo* doer, string *reason = 0);
/* Same but only process count cnt from offset offs. Set cnt to size_t(-1) 
 * for no limit */
bool file_scan(const string &fn, FileScanDo* doer, off_t offs, size_t cnt,
               string *reason = 0);

/**
 * Read file into string.
 * @return true for ok, false else
 */
bool file_to_string(const string &filename, string &data, string *reason = 0);

/** Read file chunk into string. Set cnt to size_t(-1) for whole file */
bool file_to_string(const string &filename, string &data, 
                    off_t offs, size_t cnt, string *reason = 0);

#endif /* _READFILE_H_INCLUDED_ */
