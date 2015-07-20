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
#ifndef _SORTSEQ_H_INCLUDED_
#define _SORTSEQ_H_INCLUDED_

#include <vector>
#include <string>

#include "refcntr.h"
#include "docseq.h"

/** 
 * A sorted sequence is created from the first N documents of another one, 
 * and sorts them according to the given criteria.
 */
class DocSeqSorted : public DocSeqModifier {
 public:
    DocSeqSorted(RefCntr<DocSequence> iseq, DocSeqSortSpec &sortspec)
	:  DocSeqModifier(iseq)
    {
	setSortSpec(sortspec);
    }
    virtual ~DocSeqSorted() {}
    virtual bool canSort() {return true;}
    virtual bool setSortSpec(const DocSeqSortSpec &sortspec);
    virtual bool getDoc(int num, Rcl::Doc &doc, string *sh = 0);
    virtual int getResCnt() {return m_docsp.size();}
 private:
    DocSeqSortSpec          m_spec;
    std::vector<Rcl::Doc>   m_docs;
    std::vector<Rcl::Doc *> m_docsp;
};

#endif /* _SORTSEQ_H_INCLUDED_ */
