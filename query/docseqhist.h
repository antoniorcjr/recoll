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
#ifndef _DOCSEQHIST_H_INCLUDED_
#define _DOCSEQHIST_H_INCLUDED_

#include "docseq.h"
#include "dynconf.h"

namespace Rcl {
    class Db;
}

/** DynConf Document history entry */
class RclDHistoryEntry : public DynConfEntry {
 public:
    RclDHistoryEntry() : unixtime(0) {}
    RclDHistoryEntry(long t, const string& u) 
	: unixtime(t), udi(u) {}
    virtual ~RclDHistoryEntry() {}
    virtual bool decode(const string &value);
    virtual bool encode(string& value);
    virtual bool equal(const DynConfEntry& other);
    long unixtime;
    string udi;
};

/** A DocSequence coming from the history file. 
 *  History is kept as a list of urls. This queries the db to fetch
 *  metadata for an url key */
class DocSequenceHistory : public DocSequence {
 public:
    DocSequenceHistory(Rcl::Db *d, RclDynConf *h, const string &t) 
	: DocSequence(t), m_db(d), m_hist(h), m_prevnum(-1), m_prevtime(-1) {}
    virtual ~DocSequenceHistory() {}

    virtual bool getDoc(int num, Rcl::Doc &doc, string *sh = 0);
    virtual int getResCnt();
    virtual string getDescription() {return m_description;}
    void setDescription(const string& desc) {m_description = desc;}
protected:
    virtual Rcl::Db *getDb();
private:
    Rcl::Db    *m_db;
    RclDynConf *m_hist;
    int         m_prevnum;
    long        m_prevtime;
    string      m_description; // This is just an nls translated 'doc history'
    list<RclDHistoryEntry> m_hlist;
    list<RclDHistoryEntry>::const_iterator m_it;
};

extern bool historyEnterDoc(RclDynConf *dncf, const string& udi);

#endif /* _DOCSEQ_H_INCLUDED_ */
