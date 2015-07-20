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
#ifndef TEST_WASASTRINGTOQUERY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "smallut.h"
#include "wasastringtoquery.h"

#undef DEB_WASASTRINGTOQ
#ifdef DEB_WASASTRINGTOQ
#define DPRINT(X) fprintf X
#define DUMPQ(Q) {string D;Q->describe(D);fprintf(stderr, "%s\n", D.c_str());}
#else
#define DPRINT(X)
#define DUMPQ(Q)
#endif

WasaQuery::~WasaQuery()
{
    for (vector<WasaQuery*>::iterator it = m_subs.begin();
	 it != m_subs.end(); it++) {
	delete *it;
    }
    m_subs.clear();
}

static const char* reltosrel(WasaQuery::Rel rel)
{
    switch (rel) {
    case WasaQuery::REL_EQUALS: return "=";
    case WasaQuery::REL_CONTAINS: return ":";
    case WasaQuery::REL_LT: return "<";
    case WasaQuery::REL_LTE: return "<=";
    case WasaQuery::REL_GT: return ">";
    case WasaQuery::REL_GTE: return ">=";
    default: return "?";
    }
}

void WasaQuery::describe(string &desc) const
{
    desc += "(";
    string fieldspec = m_fieldspec.empty() ? string() : m_fieldspec + 
	reltosrel(m_rel);
    switch (m_op) {
    case OP_NULL: 
	desc += "NULL"; 
	break;
    case OP_LEAF: 
	if (m_exclude)
	    desc += "NOT (";
	desc += fieldspec + m_value;
	if (m_exclude)
	    desc += ")";
	break;
    case OP_OR: 
    case OP_AND:
	for (vector<WasaQuery *>::const_iterator it = m_subs.begin();
	     it != m_subs.end(); it++) {
	    (*it)->describe(desc);
	    vector<WasaQuery *>::const_iterator it1 = it;
	    it1++;
	    if (it1 != m_subs.end())
		desc += m_op == OP_OR ? "OR ": "AND ";
	}
	break;
    }
    if (desc[desc.length() - 1] == ' ')
	desc.erase(desc.length() - 1);
    desc += ")"; 
    if (m_modifiers != 0) {
	if (m_modifiers & WQM_BOOST)     desc += "BOOST|";
	if (m_modifiers & WQM_CASESENS)  desc += "CASESENS|";
	if (m_modifiers & WQM_DIACSENS)  desc += "DIACSENS|";
	if (m_modifiers & WQM_FUZZY)     desc += "FUZZY|";
	if (m_modifiers & WQM_NOSTEM)    desc += "NOSTEM|";
	if (m_modifiers & WQM_PHRASESLACK) {
	    char buf[100];
	    sprintf(buf, "%d", m_slack);
	    desc += "PHRASESLACK(" + string(buf) + string(")|");
	}
	if (m_modifiers & WQM_PROX)      desc += "PROX|";
	if (m_modifiers & WQM_REGEX)     desc += "REGEX|";
	if (m_modifiers & WQM_SLOPPY)    desc += "SLOPPY|";
	if (m_modifiers & WQM_WORDS)     desc += "WORDS|";

	if (desc.length() > 0 && desc[desc.length()-1] == '|')
	    desc.erase(desc.length()-1);
    }
    desc += " ";
}

// The string query parser code:

/* Shamelessly lifted from Beagle:			
 * This is our regular Expression Pattern:
 * we expect something like this:
 * -key:"Value String"modifiers
 * key:Value
 * or
 * Value
*/

/* The master regular expression used to parse a query string
 * Sub-expressions in parenthesis are numbered from 1. Each opening
 * parenthesis increases the index, but we're not interested in all
 * Deviations from standard:
 *  Relation: the standard-conformant line read as (release<1.16):
        "(:|=|<|>|<=|>=)"            //7 Relation
    but we are not actually making use of the relation type
    (interpreting all as ":"), and this can product unexpected results
    as a (ie pasted) search for nonexfield=value will silently drop
    the nonexfield part, while the user probably was not aware of
    triggering a field search (expecting just ':' to do this).
 */
static const char * parserExpr = 
    "(OR|\\|\\|)[[:space:]]*"        //1 OR,|| 
    "|"
    "(AND|&&)[[:space:]]*"           // 2 AND,&& (ignored, default)
    "|"
    "("                              //3 
      "([+-])?"                      //4 Force or exclude indicator
      "("                            //5
        "([[:alpha:]][[:alnum:]:]*)" //6 Field spec: ie: "dc:title:letitre"
        "[[:space:]]*"
        "(:|=|>|<)"            //7 Relation
        "[[:space:]]*)?"
      "("                            //8
        "(\""                        //9
          "([^\"]+)"                 //10 "A quoted term"
        "\")"                        
        "([bcCdDeflLoprsw.0-9]*)"             //11 modifiers
        "|"
        "([^[:space:]\"]+)"          //12 ANormalTerm
      ")"
    ")[[:space:]]*"
;

// For debugging the parser. But see also NMATCH
static const char *matchNames[] = {
     /* 0*/   "",
     /* 1*/   "OR",
     /* 2*/   "AND",
     /* 3*/   "",
     /* 4*/   "+-",
     /* 5*/   "",
     /* 6*/   "FIELD",
     /* 7*/   "RELATION",
     /* 8*/   "",
     /* 9*/   "",
     /*10*/   "QUOTEDTERM",
     /*11*/   "MODIFIERS",
     /*12*/   "TERM",
};
#define NMATCH (sizeof(matchNames) / sizeof(char *))

// Symbolic names for the interesting submatch indices
enum SbMatchIdx {SMI_OR=1, SMI_AND=2, SMI_PM=4, SMI_FIELD=6, SMI_REL=7,
		 SMI_QUOTED=10, SMI_MODIF=11, SMI_TERM=12};

static const int maxmatchlen = 1024;
static const int errbuflen = 300;

class StringToWasaQuery::Internal {
public:
    Internal() 
	: m_rxneedsfree(false)
    {}
    ~Internal()
    {
	if (m_rxneedsfree)
	    regfree(&m_rx);
    }
    bool checkSubMatch(int i, char *match, string& reason)
    {
	if (i < 0 || i >= int(NMATCH) || m_pmatch[i].rm_so == -1) {
	    //DPRINT((stderr, "checkSubMatch: no match: i %d rm_so %d\n", 
	    //i, m_pmatch[i].rm_so));
	    return false;
	}
	if (m_pmatch[i].rm_eo - m_pmatch[i].rm_so <= 0) {
	    // weird and fatal
	    reason = "Internal regular expression handling error";
	    return false;
	}
	//DPRINT((stderr, "checkSubMatch: so %d eo %d\n", m_pmatch[i].rm_so, 
	//m_pmatch[i].rm_eo));
	memcpy(match, m_cp + m_pmatch[i].rm_so, 
	       m_pmatch[i].rm_eo - m_pmatch[i].rm_so);
	match[m_pmatch[i].rm_eo - m_pmatch[i].rm_so] = 0;
	return true;
    }

    WasaQuery *stringToQuery(const string& str, string& reason);

    friend class StringToWasaQuery;
private:
    const char *m_cp;
    regex_t     m_rx;
    bool        m_rxneedsfree;
    regmatch_t  m_pmatch[NMATCH];
};

StringToWasaQuery::StringToWasaQuery() 
    : internal(new Internal)
{
}

StringToWasaQuery::~StringToWasaQuery()
{
    delete internal;
}

WasaQuery *
StringToWasaQuery::stringToQuery(const string& str, string& reason)
{
    if (internal == 0)
	return 0;
    WasaQuery *wq = internal->stringToQuery(str, reason);
    DUMPQ(wq);
    return wq;
}

WasaQuery *
StringToWasaQuery::Internal::stringToQuery(const string& str, string& reason)
{
    if (m_rxneedsfree)
	regfree(&m_rx);

    char errbuf[errbuflen+1];
    int errcode;
    if ((errcode = regcomp(&m_rx, parserExpr, REG_EXTENDED)) != 0) {
	regerror(errcode, &m_rx, errbuf, errbuflen);
	reason = errbuf;
	return 0;
    }
    m_rxneedsfree = true;

    const char *cpe;
    m_cp = str.c_str();
    cpe = str.c_str() + str.length();

    WasaQuery *query = new WasaQuery;
    query->m_op = WasaQuery::OP_AND;
    WasaQuery *orChain = 0;
    bool prev_or = false;

    // Loop on repeated regexp matches on the main string.
    for (int loop = 0;;loop++) {
	if ((errcode = regexec(&m_rx, m_cp, NMATCH, m_pmatch, 0))) {
	    regerror(errcode, &m_rx, errbuf, errbuflen);
	    reason = errbuf;
	    return 0;
	}
	if (m_pmatch[0].rm_eo <= 0) {
	    // weird and fatal
	    reason = "Internal regular expression handling error";
	    return 0;
	}

#ifdef DEB_WASASTRINGTOQ
	DPRINT((stderr, "Next part:\n"));
	for (unsigned int i = 0; i < NMATCH; i++) {
	    if (m_pmatch[i].rm_so == -1) 	continue;
	    char match[maxmatchlen+1];
	    memcpy(match, m_cp + m_pmatch[i].rm_so,
		   m_pmatch[i].rm_eo - m_pmatch[i].rm_so);
	    match[m_pmatch[i].rm_eo - m_pmatch[i].rm_so] = 0;
	    if (matchNames[i][0])
		DPRINT((stderr, "%10s: [%s] (%d->%d)\n", matchNames[i], match, 
			(int)m_pmatch[i].rm_so, (int)m_pmatch[i].rm_eo));
	}
#endif

	char match[maxmatchlen+1];
	if (checkSubMatch(SMI_OR, match, reason)) {
	    if (prev_or) {
		// Bad syntax
		reason = "Bad syntax: consecutive OR";
		return 0;
	    }

	    if (orChain == 0) {
		// Fist OR seen: start OR subclause.
		if ((orChain = new WasaQuery()) == 0) {
		    reason = "Out of memory";
		    return 0;
		}
		orChain->m_op = WasaQuery::OP_OR;
	    }

	    // For the first OR, we need to transfer the previous
	    // query from the main vector to the OR subquery
	    if (orChain->m_subs.empty() && !query->m_subs.empty()) {
		orChain->m_subs.push_back(query->m_subs.back());
		query->m_subs.pop_back();
	    }
	    prev_or = true;

	} else if (checkSubMatch(SMI_AND, match, reason)) {
	    // Do nothing, AND is the default. We might want to check for 
	    // errors like consecutive ANDs, or OR AND

	} else {

	    WasaQuery *nclause = new WasaQuery;
	    if (nclause == 0) {
		reason = "Out of memory";
		return 0;
	    }

	    // Check for quoted or unquoted value
	    unsigned int mods = 0;
	    if (checkSubMatch(SMI_QUOTED, match, reason)) {
		nclause->m_value = match;
                mods |= WasaQuery::WQM_QUOTED;
	    } else if (checkSubMatch(SMI_TERM, match, reason)) {
		nclause->m_value = match;
	    }

	    if (nclause->m_value.empty()) {
		// Isolated +- or fieldname: without a value. Ignore until
		// told otherwise.
		DPRINT((stderr, "Clause with empty value, skipping\n"));
		delete nclause;
		goto nextfield;
	    }
	    
	    if (checkSubMatch(SMI_MODIF, match, reason)) {
		DPRINT((stderr, "Got modifiers: [%s]\n", match));
		for (unsigned int i = 0; i < strlen(match); i++) {
		    switch (match[i]) {
		    case 'b': 
			mods |= WasaQuery::WQM_BOOST; 
			nclause->m_weight = 10.0;
			break;
		    case 'c': break;
		    case 'C': mods |= WasaQuery::WQM_CASESENS; break;
		    case 'd': break;
		    case 'D': mods |= WasaQuery::WQM_DIACSENS; break;
		    case 'e': mods |= WasaQuery::WQM_CASESENS | 
			    WasaQuery::WQM_DIACSENS |  
			    WasaQuery::WQM_NOSTEM; 
			break;
		    case 'f': mods |= WasaQuery::WQM_FUZZY; break;
		    case 'l': mods |= WasaQuery::WQM_NOSTEM; break;
		    case 'L': break;
		    case 'o': 
			mods |= WasaQuery::WQM_PHRASESLACK; 
			// Default slack if specified only by 'o' is 10.
			nclause->m_slack = 10;
			if (i < strlen(match) - 1) {
			    char *endptr;
			    int slack = strtol(match+i+1, &endptr, 10);
			    if (endptr != match+i+1) {
				i += endptr - (match+i+1);
				nclause->m_slack = slack;
			    }
			}
			break;
		    case 'p': 
			mods |= WasaQuery::WQM_PROX; 
			nclause->m_slack = 10;
			break;
		    case 'r': mods |= WasaQuery::WQM_REGEX; break;
		    case 's': mods |= WasaQuery::WQM_SLOPPY; break;
		    case 'w': mods |= WasaQuery::WQM_WORDS; break;
		    case '.':case '0':case '1':case '2':case '3':case '4':
		    case '5':case '6':case '7':case '8':case '9':
		    {
			int n;
			float factor;
			if (sscanf(match+i, "%f %n", &factor, &n)) {
			    nclause->m_weight = factor;
			    DPRINT((stderr, "Got factor %.2f len %d\n",
				    factor, n));
			}
			if (n)
			    i += n-1;
		    }
		    }
		}
	    }
	    nclause->m_modifiers = WasaQuery::Modifier(mods);

	    // Field indicator ?
	    if (checkSubMatch(SMI_FIELD, match, reason)) {
		// We used Check for special fields indicating sorting
		// etc. here but this went away from the spec. See 1.4
		// if it comes back
		nclause->m_fieldspec = match;
		if (checkSubMatch(SMI_REL, match, reason)) {
		    switch (match[0]) {
		    case '=':nclause->m_rel = WasaQuery::REL_EQUALS;break;
		    case ':':nclause->m_rel = WasaQuery::REL_CONTAINS;break;
		    case '<':
			if (match[1] == '=')
			    nclause->m_rel = WasaQuery::REL_LTE;
			else
			    nclause->m_rel = WasaQuery::REL_LT;
			break;
		    case '>':
			if (match[1] == '=')
			    nclause->m_rel = WasaQuery::REL_GTE;
			else
			    nclause->m_rel = WasaQuery::REL_GT;
			break;
		    default:
			nclause->m_rel = WasaQuery::REL_CONTAINS;
		    }
		} else {
		    // ?? If field matched we should have a relation
		    nclause->m_rel = WasaQuery::REL_CONTAINS;
		}
	    }

	    nclause->m_op = WasaQuery::OP_LEAF;
	    // +- indicator ?
	    if (checkSubMatch(SMI_PM, match, reason) && match[0] == '-') {
		nclause->m_exclude = true;
	    } else {
		nclause->m_exclude = false;
	    }

	    if (prev_or) {
		// The precedent token was an OR, add new clause to or chain
		//DPRINT((stderr, "Adding to OR chain\n"));
		orChain->m_subs.push_back(nclause);
	    } else {
		if (orChain) {
		    // Getting out of OR. Add the OR subquery to the main one
		    //DPRINT((stderr, "Adding OR chain to main\n"));
		    query->m_subs.push_back(orChain);
		    orChain = 0;
		} 
		//DPRINT((stderr, "Adding to main chain\n"));
		// Add new clause to main query
		query->m_subs.push_back(nclause);
	    }

	    prev_or = false;
	}

    nextfield:
	// Advance current string position. We checked earlier that
	// the increment is strictly positive, so we won't loop
	// forever
	m_cp += m_pmatch[0].rm_eo;
	if (m_cp >= cpe)
	    break;
    }

    if (orChain) {
	// Getting out of OR. Add the OR subquery to the main one
	DPRINT((stderr, "Adding OR chain to main.Before: \n"));
	DUMPQ(query);
	DUMPQ(orChain);
	query->m_subs.push_back(orChain);
    }

    regfree(&m_rx);
    m_rxneedsfree = false;
    return query;
}

#else // TEST

#include <stdio.h>
#include <stdlib.h>

#include "wasastringtoquery.h"

static char *thisprog;

int main(int argc, char **argv)
{
    thisprog = argv[0];
    argc--; argv++;

    if (argc != 1) {
	fprintf(stderr, "need one arg\n");
	exit(1);
    }
    const string str = *argv++;argc--;
    string reason;
    StringToWasaQuery qparser;
    WasaQuery *q = qparser.stringToQuery(str, reason);
    if (q == 0) {
	fprintf(stderr, "stringToQuery failed: %s\n", reason.c_str());
	exit(1);
    }
    string desc;
    q->describe(desc);
    fprintf(stderr, "Finally: %s\n", desc.c_str());
    exit(0);
}

#endif // TEST_WASASTRINGTOQUERY
