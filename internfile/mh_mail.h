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
#ifndef _MAIL_H_INCLUDED_
#define _MAIL_H_INCLUDED_

#include <sstream>
#include <vector>
#include <map>
using std::vector;
using std::map;

#include "mimehandler.h"

namespace Binc {
    class MimeDocument;
    class MimePart;
}

class MHMailAttach;

/** 
 * Translate a mail folder file into internal documents (also works
 * for maildir files). This has to keep state while parsing a mail folder
 * file. 
 */
class MimeHandlerMail : public RecollFilter {
public:
    MimeHandlerMail(RclConfig *cnf, const string &id);
    virtual ~MimeHandlerMail();
    virtual bool set_document_file(const string& mt, const string& file_path);
    virtual bool set_document_string(const string& mt, const string& data);
    virtual bool is_data_input_ok(DataInput input) const {
	if (input == DOCUMENT_FILE_NAME || input == DOCUMENT_STRING)
	    return true;
	return false;
    }
    virtual bool next_document();
    virtual bool skip_to_document(const string& ipath);
    virtual void clear();

private:
    bool processMsg(Binc::MimePart *doc, int depth);
    void walkmime(Binc::MimePart* doc, int depth);
    bool processAttach();
    Binc::MimeDocument     *m_bincdoc;
    int                     m_fd;
    std::stringstream      *m_stream;

    // Current index in parts. starts at -1 for self, then index into
    // attachments
    int                     m_idx; 
    // Start of actual text (after the reprinted headers. This is for 
    // generating a semi-meaningful "abstract")
    string::size_type       m_startoftext; 
    string                  m_subject; 
    vector<MHMailAttach *>  m_attachments;
    // Additional headers to be process as per config + field name translation
    map<string,string>      m_addProcdHdrs; 
};

class MHMailAttach {
public:
    string          m_contentType;
    string          m_filename;
    string          m_charset;
    string          m_contentTransferEncoding;
    Binc::MimePart *m_part;
};

#endif /* _MAIL_H_INCLUDED_ */
