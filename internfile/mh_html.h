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
#ifndef _HTML_H_INCLUDED_
#define _HTML_H_INCLUDED_

#include <string>

#include "mimehandler.h"

/**
 * Convert html to utf-8 text and extract whatever metadata we can find.
 */
class MimeHandlerHtml : public RecollFilter {
 public:
    MimeHandlerHtml(RclConfig *cnf, const string& id) 
	: RecollFilter(cnf, id) 
    {
    }
    virtual ~MimeHandlerHtml() 
    {
    }
    virtual bool set_document_file(const string& mt, const string &file_path);
    virtual bool set_document_string(const string& mt, const string &data);
    virtual bool is_data_input_ok(DataInput input) const {
	if (input == DOCUMENT_FILE_NAME || input == DOCUMENT_STRING)
	    return true;
	return false;
    }
    virtual bool next_document();
    const string& get_html() 
    {
	return m_html;
    }
    virtual void clear() {
	m_filename.erase();
	m_html.erase();
	RecollFilter::clear(); 
    }
private:
    string m_filename;
    string m_html;
};

#endif /* _HTML_H_INCLUDED_ */
