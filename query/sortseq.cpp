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
#include <algorithm>

#include "debuglog.h"
#include "sortseq.h"

using std::string;

class CompareDocs { 
    DocSeqSortSpec ss;
public: 
    CompareDocs(const DocSeqSortSpec &sortspec) : ss(sortspec) {}

    // It's not too clear in the std::sort doc what this should do. This 
    // behaves as operator< 
    int operator()(const Rcl::Doc *x, const Rcl::Doc *y) 
    { 
	LOGDEB1(("Comparing .. \n"));

	map<string,string>::const_iterator xit, yit;
	xit = x->meta.find(ss.field);
	yit = y->meta.find(ss.field);
	if (xit == x->meta.end() || yit == y->meta.end())
	    return 0;
	return ss.desc ? yit->second < xit->second : xit->second < yit->second;
    } 
};

bool DocSeqSorted::setSortSpec(const DocSeqSortSpec &sortspec)
{
    LOGDEB(("DocSeqSorted::setSortSpec\n"));
    m_spec = sortspec;
    int count = m_seq->getResCnt();
    LOGDEB(("DocSeqSorted:: count %d\n", count));
    m_docs.resize(count);
    int i;
    for (i = 0; i < count; i++) {
	if (!m_seq->getDoc(i, m_docs[i])) {
	    LOGERR(("DocSeqSorted: getDoc failed for doc %d\n", i));
	    count = i;
	    break;
	}
    }
    m_docs.resize(count);
    m_docsp.resize(count);
    for (i = 0; i < count; i++)
	m_docsp[i] = &m_docs[i];

    CompareDocs cmp(sortspec);
    sort(m_docsp.begin(), m_docsp.end(), cmp);
    return true;
}

bool DocSeqSorted::getDoc(int num, Rcl::Doc &doc, string *)
{
    LOGDEB(("DocSeqSorted::getDoc(%d)\n", num));
    if (num < 0 || num >= int(m_docsp.size()))
	return false;
    doc = *m_docsp[num];
    return true;
}
