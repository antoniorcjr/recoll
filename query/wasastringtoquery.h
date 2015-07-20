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
#ifndef _WASASTRINGTOQUERY_H_INCLUDED_
#define _WASASTRINGTOQUERY_H_INCLUDED_

#include <string>
#include <vector>

using std::string;
using std::vector;
/* Note: Xesam used to be named wasabi. We changed the references to wasabi in
   the comments, but not the code */

/** 
 * A simple class to represent a parsed Xesam user language element. 
 * Can hold one leaf element or an array of subqueries to be joined by AND/OR
 *
 * The complete query is represented by a top WasaQuery holding a
 * chain of ANDed subclauses. Some of the subclauses may be themselves
 * OR'ed lists (it doesn't go deeper). Entries in the AND list may be
 * negated (AND NOT).
 *
 * For LEAF elements, the value can hold one or several words. In the
 * latter case, it should be interpreted as a phrase (comes from a
 * user-entered "quoted string"), except if the modifier flags say otherwise.
 * 
 * Some fields only make sense either for compound or LEAF queries. This 
 * is commented for each. We should subclass really.
 *
 * Note that wasaStringToQuery supposedly parses the whole Xesam 
 * User Search Language v 0.95, but that some elements are dropped or
 * ignored during the translation to a native Recoll query in wasaToRcl
 */
class WasaQuery {
public:
    /** Type of this element: leaf or AND/OR chain */
    enum Op {OP_NULL, OP_LEAF, OP_OR, OP_AND};
    /** Relation to be searched between field and value. Recoll actually only
	supports "contain" except for a size field */
    enum Rel {REL_NULL, REL_EQUALS, REL_CONTAINS, REL_LT, REL_LTE, 
	      REL_GT, REL_GTE};
    /** Modifiers for terms: case/diacritics handling,
	stemming control... */
    enum Modifier {WQM_CASESENS = 1, WQM_DIACSENS = 2, WQM_NOSTEM = 4, 
		   WQM_BOOST = 8, WQM_PROX = 0x10, WQM_SLOPPY = 0x20, 
		   WQM_WORDS = 0x40, WQM_PHRASESLACK = 0x80, WQM_REGEX = 0x100,
		   WQM_FUZZY = 0x200, WQM_QUOTED = 0x400};

    typedef vector<WasaQuery*> subqlist_t;

    WasaQuery() 
	: m_op(OP_NULL), m_rel(REL_NULL), m_exclude(false), 
	  m_modifiers(0), m_slack(0), m_weight(1.0)
    {}

    ~WasaQuery();

    /** Get string describing the query tree from this point */
    void describe(string &desc) const;

    /** Op to be performed on either value (may be LEAF or EXCL, or subqs */
    WasaQuery::Op      m_op;

    /** Field specification if any (ie: title, author ...) Only OPT_LEAF */
    string             m_fieldspec;
    /** Relation between field and value: =, :, <,>,<=, >= */
    WasaQuery::Rel     m_rel;

    /* Negating flag */
    bool             m_exclude;

    /* String value. Valid for op == OP_LEAF or EXCL */
    string             m_value;

    /** Subqueries. Valid for conjunctions */
    vector<WasaQuery*> m_subs;
    
    unsigned int   m_modifiers;
    int            m_slack;
    float          m_weight;
};

/**
 * Wasabi query string parser class. Could be a simple function
 * really, but there might be some parser initialization work done in
 * the constructor.
 */
class StringToWasaQuery {
public:
    StringToWasaQuery();
    ~StringToWasaQuery();
    WasaQuery *stringToQuery(const string& str, string& reason);
    class Internal;
private:
    Internal *internal;
};

#endif /* _WASASTRINGTOQUERY_H_INCLUDED_ */
