/* Copyright (C) 2006 J.F.Dockes
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
#include <cstdio>
#include <string>
#include <list>
#include <algorithm>
using std::string;
using std::list;

#include "rclconfig.h"
#include "wasastringtoquery.h"
#include "rcldb.h"
#include "searchdata.h"
#include "wasatorcl.h"
#include "debuglog.h"
#include "smallut.h"
#include "rclconfig.h"
#include "refcntr.h"
#include "textsplit.h"

static Rcl::SearchData *wasaQueryToRcl(const RclConfig *config, 
				       const string& stemlang,
				       WasaQuery *wasa, 
				       const string& autosuffs, string& reason)
{
    if (wasa == 0) {
	reason = "NULL query";
	return 0;
    }
    if (wasa->m_op != WasaQuery::OP_AND && wasa->m_op != WasaQuery::OP_OR) {
	reason = "Top query neither AND nor OR ?";
	LOGERR(("wasaQueryToRcl: top query neither AND nor OR!\n"));
	return 0;
    }

    Rcl::SearchData *sdata = new 
	Rcl::SearchData(wasa->m_op == WasaQuery::OP_AND ? Rcl::SCLT_AND : 
			Rcl::SCLT_OR, stemlang);
    LOGDEB2(("wasaQueryToRcl: %s chain\n", wasa->m_op == WasaQuery::OP_AND ? 
	     "AND" : "OR"));

    WasaQuery::subqlist_t::iterator it;
    Rcl::SearchDataClause *nclause;

    // Walk the list of clauses. Some pseudo-field types need special
    // processing, which results in setting data in the top struct
    // instead of adding a clause. We check for these first
    for (it = wasa->m_subs.begin(); it != wasa->m_subs.end(); it++) {

	if (!stringicmp("mime", (*it)->m_fieldspec) ||
	    !stringicmp("format", (*it)->m_fieldspec)) {
	    if ((*it)->m_op == WasaQuery::OP_LEAF) {
		if ((*it)->m_exclude) {
		    sdata->remFiletype((*it)->m_value);
		} else {
		    sdata->addFiletype((*it)->m_value);
		}
	    } else {
		reason = "internal error: mime clause not leaf??";
		return 0;
	    }
	    continue;
	} 

	// Xesam uses "type", we also support "rclcat", for broad
	// categories like "audio", "presentation", etc.
	if (!stringicmp("rclcat", (*it)->m_fieldspec) ||
	    !stringicmp("type", (*it)->m_fieldspec)) {
	    if ((*it)->m_op != WasaQuery::OP_LEAF) {
		reason = "internal error: rclcat/type clause not leaf??";
		return 0;
	    }
	    vector<string> mtypes;
	    if (config && config->getMimeCatTypes((*it)->m_value, mtypes)
		&& !mtypes.empty()) {
		for (vector<string>::iterator mit = mtypes.begin();
		     mit != mtypes.end(); mit++) {
		    if ((*it)->m_exclude) {
			sdata->remFiletype(*mit);
		    } else {
			sdata->addFiletype(*mit);
		    }
		}
	    } else {
		reason = "Unknown rclcat/type value: no mime types found";
		return 0;
	    }
	    continue;
	}

	// Handle "date" spec
	if (!stringicmp("date", (*it)->m_fieldspec)) {
	    if ((*it)->m_op != WasaQuery::OP_LEAF) {
		reason = "Negative date filtering not supported";
		return 0;
	    }
	    DateInterval di;
	    if (!parsedateinterval((*it)->m_value, &di)) {
		LOGERR(("wasaQueryToRcl: bad date interval format\n"));
		reason = "Bad date interval format";
		return 0;
	    }
	    LOGDEB(("wasaQueryToRcl:: date span:  %d-%d-%d/%d-%d-%d\n",
		    di.y1,di.m1,di.d1, di.y2,di.m2,di.d2));
	    sdata->setDateSpan(&di);
	    continue;
	} 

	// Handle "size" spec
	if (!stringicmp("size", (*it)->m_fieldspec)) {
	    if ((*it)->m_op != WasaQuery::OP_LEAF) {
		reason = "Negative size filtering not supported";
		return 0;
	    }
	    char *cp;
	    size_t size = strtoll((*it)->m_value.c_str(), &cp, 10);
	    if (*cp != 0) {
		switch (*cp) {
		case 'k': case 'K': size *= 1E3;break;
		case 'm': case 'M': size *= 1E6;break;
		case 'g': case 'G': size *= 1E9;break;
		case 't': case 'T': size *= 1E12;break;
		default: 
		    reason = string("Bad multiplier suffix: ") + *cp;
		    return 0;
		}
	    }

	    switch ((*it)->m_rel) {
	    case WasaQuery::REL_EQUALS:
		sdata->setMaxSize(size);
		sdata->setMinSize(size);
		break;
	    case WasaQuery::REL_LT:
	    case WasaQuery::REL_LTE:
		sdata->setMaxSize(size);
		break;
	    case WasaQuery::REL_GT: 
	    case WasaQuery::REL_GTE:
		sdata->setMinSize(size);
		break;
	    default:
		reason = "Bad relation operator with size query. Use > < or =";
		return 0;
	    }
	    continue;
	} 

	// "Regular" processing follows:
	unsigned int mods = (unsigned int)(*it)->m_modifiers;
	LOGDEB0(("wasaQueryToRcl: clause modifiers 0x%x\n", mods));
	nclause = 0;

	switch ((*it)->m_op) {
	case WasaQuery::OP_NULL:
	case WasaQuery::OP_AND:
	default:
	    reason = "Found bad NULL or AND query type in list";
	    LOGERR(("wasaQueryToRcl: found bad NULL or AND q type in list\n"));
	    continue;

	case WasaQuery::OP_LEAF: {
	    LOGDEB0(("wasaQueryToRcl: leaf clause [%s:%s] slack %d excl %d\n", 
		     (*it)->m_fieldspec.c_str(), (*it)->m_value.c_str(),
		     (*it)->m_slack, (*it)->m_exclude));

            // Change terms found in the "autosuffs" list into "ext"
            // field queries
            if ((*it)->m_fieldspec.empty() && !autosuffs.empty()) {
                vector<string> asfv;
                if (stringToStrings(autosuffs, asfv)) {
                    if (find_if(asfv.begin(), asfv.end(), 
                                StringIcmpPred((*it)->m_value)) != asfv.end()) {
                        (*it)->m_fieldspec = "ext";
                        (*it)->m_modifiers |= WasaQuery::WQM_NOSTEM;
                    }
                }
            }

	    if (!stringicmp("dir", (*it)->m_fieldspec)) {
		// dir filtering special case
		nclause = new Rcl::SearchDataClausePath((*it)->m_value, 
							(*it)->m_exclude);
	    } else {
		if ((*it)->m_exclude && wasa->m_op != WasaQuery::OP_AND) {
		    LOGERR(("wasaQueryToRcl: excl clause inside OR list!\n"));
		    continue;
		}

		if (mods & WasaQuery::WQM_QUOTED) {
		    Rcl::SClType tp = (mods & WasaQuery::WQM_PROX)  ?
			Rcl::SCLT_NEAR :
			Rcl::SCLT_PHRASE;
		    nclause = new Rcl::SearchDataClauseDist(tp, (*it)->m_value,
							    (*it)->m_slack,
							    (*it)->m_fieldspec);
		} else {
                    // If term has commas or slashes inside, take it
                    // as a list, turn the slashes/commas to spaces,
                    // leave unquoted. Otherwise, this would end up as
                    // a phrase query. This is a handy way to enter
                    // multiple terms to be searched inside a
                    // field. We interpret ',' as AND, and '/' as
                    // OR. No mixes allowed and ',' wins.
		    Rcl::SClType tp = (*it)->m_exclude ? Rcl::SCLT_OR:
			Rcl::SCLT_AND;
                    string ns = neutchars((*it)->m_value, ",");
                    if (ns.compare((*it)->m_value)) {
                        // had ','
                        tp = Rcl::SCLT_AND;
                    } else {
                        ns = neutchars((*it)->m_value, "/");
                        if (ns.compare((*it)->m_value)) {
                            tp = Rcl::SCLT_OR;
                        }
                    }
		    nclause = new Rcl::SearchDataClauseSimple(tp, ns,
                                                            (*it)->m_fieldspec);
		}
		nclause->setexclude((*it)->m_exclude);
	    }

	    if (nclause == 0) {
		reason = "Out of memory";
		LOGERR(("wasaQueryToRcl: out of memory\n"));
		return 0;
	    }
	}
	    break;
	    
	case WasaQuery::OP_OR:
	    LOGDEB2(("wasaQueryToRcl: OR clause [%s]:[%s]\n", 
		     (*it)->m_fieldspec.c_str(), (*it)->m_value.c_str()));
	    // Create a subquery.
	    Rcl::SearchData *sub = 
		wasaQueryToRcl(config, stemlang, *it, autosuffs, reason);
	    if (sub == 0) {
		continue;
	    }
	    nclause = 
		new Rcl::SearchDataClauseSub(RefCntr<Rcl::SearchData>(sub));
	    if (nclause == 0) {
		LOGERR(("wasaQueryToRcl: out of memory\n"));
		reason = "Out of memory";
		return 0;
	    }
	}

	if (mods & WasaQuery::WQM_NOSTEM)
	    nclause->addModifier(Rcl::SearchDataClause::SDCM_NOSTEMMING);
	if (mods & WasaQuery::WQM_DIACSENS)
	    nclause->addModifier(Rcl::SearchDataClause::SDCM_DIACSENS);
	if (mods & WasaQuery::WQM_CASESENS)
	    nclause->addModifier(Rcl::SearchDataClause::SDCM_CASESENS);
	if ((*it)->m_weight != 1.0)
	    nclause->setWeight((*it)->m_weight);
	sdata->addClause(nclause);
    }

    return sdata;
}

Rcl::SearchData *wasaStringToRcl(const RclConfig *config, const string& stemlang,
				 const string &qs, string &reason, 
                                 const string& autosuffs)
{
    StringToWasaQuery parser;
    WasaQuery *wq = parser.stringToQuery(qs, reason);
    if (wq == 0) 
	return 0;
    return wasaQueryToRcl(config, stemlang, wq, autosuffs, reason);
}
