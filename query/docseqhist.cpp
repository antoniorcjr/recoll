/* Copyright (C) 2005 J.F.Dockes
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
#include <stdio.h>
#include <math.h>
#include <time.h>

#include <cmath>
#include <list>
using std::list;

#include "docseqhist.h"
#include "rcldb.h"
#include "fileudi.h"
#include "base64.h"
#include "debuglog.h"
#include "smallut.h"

// Encode document history entry: 
// U + Unix time + base64 of udi
// The U distinguishes udi-based entries from older fn+ipath ones
bool RclDHistoryEntry::encode(string& value)
{
    char chartime[30];
    sprintf(chartime,"%ld", unixtime);
    string budi;
    base64_encode(udi, budi);
    value = string("U ") + string(chartime) + " " + budi;
    return true;
}

// Decode. We support historical entries which were like "time b64fn [b64ipath]"
// Current entry format is "U time b64udi"
bool RclDHistoryEntry::decode(const string &value)
{
    vector<string> vall;
    stringToStrings(value, vall);

    vector<string>::const_iterator it = vall.begin();
    udi.erase();
    string fn, ipath;
    switch (vall.size()) {
    case 2:
        // Old fn+ipath, null ipath case 
        unixtime = atoll((*it++).c_str());
        base64_decode(*it++, fn);
        break;
    case 3:
        if (!it->compare("U")) {
            // New udi-based entry
            it++;
            unixtime = atoll((*it++).c_str());
            base64_decode(*it++, udi);
        } else {
            // Old fn + ipath. We happen to know how to build an udi
            unixtime = atoll((*it++).c_str());
            base64_decode(*it++, fn);
            base64_decode(*it, ipath);
        }
        break;
    default: 
        return false;
    }

    if (!fn.empty()) {
        // Old style entry found, make an udi, using the fs udi maker
        make_udi(fn, ipath, udi);
    }
    LOGDEB1(("RclDHistoryEntry::decode: udi [%s]\n", udi.c_str()));
    return true;
}

bool RclDHistoryEntry::equal(const DynConfEntry& other)
{
    const RclDHistoryEntry& e = dynamic_cast<const RclDHistoryEntry&>(other);
    return e.udi == udi;
}

bool historyEnterDoc(RclDynConf *dncf, const string& udi)
{
    LOGDEB1(("historyEnterDoc: [%s] into %s\n", 
	    udi.c_str(), dncf->getFilename().c_str()));
    RclDHistoryEntry ne(time(0), udi);
    RclDHistoryEntry scratch;
    return dncf->insertNew(docHistSubKey, ne, scratch, 200);
}

list<RclDHistoryEntry> getDocHistory(RclDynConf* dncf)
{
    return dncf->getList<RclDHistoryEntry>(docHistSubKey);
}


bool DocSequenceHistory::getDoc(int num, Rcl::Doc &doc, string *sh) 
{
    // Retrieve history list
    if (!m_hist)
	return false;
    if (m_hlist.empty())
	m_hlist = getDocHistory(m_hist);

    if (num < 0 || num >= (int)m_hlist.size())
	return false;
    int skip;
    if (m_prevnum >= 0 && num >= m_prevnum) {
	skip = num - m_prevnum;
    } else {
	skip = num;
	m_it = m_hlist.begin();
	m_prevtime = -1;
    }
    m_prevnum = num;
    while (skip--) 
	m_it++;
    if (sh) {
	if (m_prevtime < 0 || 
            abs (float(m_prevtime) - float(m_it->unixtime)) > 86400) {
	    m_prevtime = m_it->unixtime;
	    time_t t = (time_t)(m_it->unixtime);
	    *sh = string(ctime(&t));
	    // Get rid of the final \n in ctime
	    sh->erase(sh->length()-1);
	} else
	    sh->erase();
    }

    // For now history does not store an index id. Use empty doc as ref.
    Rcl::Doc idxdoc;
    bool ret = m_db->getDoc(m_it->udi, idxdoc, doc);
    if (!ret || doc.pc == -1) {
	doc.url = "UNKNOWN";
        doc.ipath = "";
    }

    // Ensure the snippets link won't be shown as it does not make
    // sense (no query terms...)
    doc.haspages = 0;

    return ret;
}

Rcl::Db *DocSequenceHistory::getDb()
{
    return m_db;
}

int DocSequenceHistory::getResCnt()
{	
    if (m_hlist.empty())
	m_hlist = getDocHistory(m_hist);
    return m_hlist.size();
}
