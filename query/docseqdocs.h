/* Copyright (C) 2004-2013 J.F.Dockes
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
#ifndef _DOCSEQDOCS_H_INCLUDED_
#define _DOCSEQDOCS_H_INCLUDED_

#include "docseq.h"
#include "rcldoc.h"

namespace Rcl {
    class Db;
}

/** A DocSequence that's just built from a bunch of docs */
class DocSequenceDocs : public DocSequence {
 public:
    DocSequenceDocs(Rcl::Db *d, const std::vector<Rcl::Doc> docs, 
		    const string &t) 
	: DocSequence(t), m_db(d), m_docs(docs)
    {
    }
    virtual ~DocSequenceDocs() 
    {
    }
    virtual bool getDoc(int num, Rcl::Doc &doc, string *sh = 0)
    {
	if (sh)
	    *sh = string();
	if (num < 0 || num >= int(m_docs.size()))
	    return false;
	doc = m_docs[num];
	return true;
    }
    virtual int getResCnt()
    {
	return m_docs.size();
    }
    virtual string getDescription() 
    {
	return m_description;
    }
    void setDescription(const string& desc) 
    {
	m_description = desc;
    }
protected:
    virtual Rcl::Db *getDb()
    {
	return m_db;
    }
 private:
    Rcl::Db    *m_db;
    string      m_description;
    std::vector<Rcl::Doc> m_docs;
};

#endif /* _DOCSEQ_H_INCLUDED_ */
