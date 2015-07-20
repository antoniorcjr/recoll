/* Copyright (C) 2007 J.F.Dockes 
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

#ifndef _rclquery_p_h_included_
#define _rclquery_p_h_included_

#include <map>
#include <vector>

using std::map;
using std::vector;

#include <xapian.h>
#include "rclquery.h"

namespace Rcl {

class Query::Native {
public:
    /** The query I belong to */
    Query                *m_q;

    /** query descriptor: terms and subqueries joined by operators
     * (or/and etc...)
     */
    Xapian::Query    xquery; 

    Xapian::Enquire      *xenquire; // Open query descriptor.
    Xapian::MSet          xmset;    // Partial result set
    // Term frequencies for current query. See makeAbstract, setQuery
    map<string, double>  termfreqs; 

    Native(Query *q)
	: m_q(q), xenquire(0)
    { 
    }
    ~Native() 
    {
	clear();
    }
    void clear() 
    {
	delete xenquire; xenquire = 0;
	termfreqs.clear();
    }
    /** Return a list of terms which matched for a specific result document */
    bool getMatchTerms(unsigned long xdocid, std::vector<std::string>& terms);
    int makeAbstract(Xapian::docid id, vector<Snippet>&,
		     int maxoccs = -1, int ctxwords = -1);
    int getFirstMatchPage(Xapian::docid docid, std::string& term);
    void setDbWideQTermsFreqs();
    double qualityTerms(Xapian::docid docid, 
			const std::vector<std::string>& terms,
			std::multimap<double, std::vector<std::string> >& byQ);
};

}
#endif /* _rclquery_p_h_included_ */
