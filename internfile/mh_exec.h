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
#ifndef _MH_EXEC_H_INCLUDED_
#define _MH_EXEC_H_INCLUDED_

#include <string>
#include <vector>
using std::vector;
using std::string;

#include "mimehandler.h"

/** 
 * Turn external document into internal one by executing an external filter.
 *
 * The command to execute, and its parameters, are stored in the "params" 
 * which is built in mimehandler.cpp out of data from the mimeconf file.
 *
 * As any RecollFilter, a MimeHandlerExec object can be reset
 * by calling clear(), and will stay initialised for the same mtype
 * (cmd, params etc.)
 */
class MimeHandlerExec : public RecollFilter {
 public:
    ///////////////////////
    // Members not reset by clear(). params, cfgFilterOutputMtype and 
    // cfgFilterOutputCharset
    // define what I am.  missingHelper is a permanent error
    // (no use to try and execute over and over something that's not
    // here).

    // Parameters: this has been built by our creator, from config file 
    // data. We always add the file name at the end before actual execution
    vector<string> params;
    // Filter output type. The default for ext. filters is to output html, 
    // but some don't, in which case the type is defined in the config.
    string cfgFilterOutputMtype;
    // Output character set if the above type is not text/html. For
    // those filters, the output charset has to be known: ie set by a command
    // line option.
    string cfgFilterOutputCharset; 
    bool missingHelper;
    ////////////////

    MimeHandlerExec(RclConfig *cnf, const string& id) 
	: RecollFilter(cnf, id), missingHelper(false) 
    {}
    virtual bool set_document_file(const string& mt, const string &file_path) {
	RecollFilter::set_document_file(mt, file_path);
	m_fn = file_path;
	m_havedoc = true;
	return true;
    }
    virtual bool next_document();
    virtual bool skip_to_document(const string& ipath); 
    virtual void clear() {
	m_fn.erase(); 
	m_ipath.erase();
	RecollFilter::clear(); 
    }

protected:
    string m_fn;
    string m_ipath;

    // Set up the character set metadata fields and possibly transcode
    // text/plain output. 
    // @param charset when called from mh_execm, a possible explicit
    //       value from the filter (else the data will come from the config)
    virtual void handle_cs(const string& mt, const string& charset = string());

private:
    virtual void finaldetails();
};

#endif /* _MH_EXEC_H_INCLUDED_ */
