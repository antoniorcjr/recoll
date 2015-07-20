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
#ifndef _MH_TEXT_H_INCLUDED_
#define _MH_TEXT_H_INCLUDED_
#include <sys/types.h>

#include <string>
using std::string;

#include "mimehandler.h"

/**
 * Handler for text/plain files. 
 *
 * Maybe try to guess charset, or use default, then transcode to utf8
 */
class MimeHandlerText : public RecollFilter {
 public:
    MimeHandlerText(RclConfig *cnf, const string& id) 
        : RecollFilter(cnf, id), m_paging(false), m_offs(0) 
    {
    }
    virtual ~MimeHandlerText() 
    {
    }
    virtual bool set_document_file(const string& mt, const string &file_path);
    virtual bool set_document_string(const string&, const string&);
    virtual bool is_data_input_ok(DataInput input) const {
	if (input == DOCUMENT_FILE_NAME || input == DOCUMENT_STRING)
	    return true;
	return false;
    }
    virtual bool next_document();
    virtual bool skip_to_document(const string& s);
    virtual void clear() 
    {
        m_paging = false;
	m_text.erase(); 
        m_fn.erase();
        m_offs = 0;
	RecollFilter::clear();
    }
private:
    bool   m_paging;
    string m_text;
    string m_fn;
    off_t  m_offs; // Offset of next read in file if we're paging
    size_t m_pagesz;
    string m_charsetfromxattr; 

    bool readnext();
};

#endif /* _MH_TEXT_H_INCLUDED_ */
