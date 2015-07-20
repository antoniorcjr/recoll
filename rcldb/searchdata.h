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
#ifndef _SEARCHDATA_H_INCLUDED_
#define _SEARCHDATA_H_INCLUDED_

/** 
 * Structures to hold data coming almost directly from the gui
 * and handle its translation to Xapian queries.
 * This is not generic code, it reflects the choices made for the user 
 * interface, and it also knows some specific of recoll's usage of Xapian 
 * (ie: term prefixes)
 */
#include <string>
#include <vector>

#include "rcldb.h"
#include "refcntr.h"
#include "smallut.h"
#include "cstr.h"
#include "hldata.h"

class RclConfig;
class AdvSearch;

namespace Rcl {

/** Search clause types */
enum SClType {
    SCLT_AND, 
    SCLT_OR, SCLT_FILENAME, SCLT_PHRASE, SCLT_NEAR, SCLT_PATH,
    SCLT_SUB
};

class SearchDataClause;
class SearchDataClauseDist;

/** 
    A SearchData object represents a Recoll user query, for translation
    into a Xapian query tree. This could probably better called a 'question'.

    This is a list of SearchDataClause objects combined through either
    OR or AND.

    Clauses either reflect user entry in a query field: some text, a
    clause type (AND/OR/NEAR etc.), possibly a distance, or are the
    result of parsing query language input. A clause can also point to
    another SearchData representing a subquery.

    The content of each clause when added may not be fully parsed yet
    (may come directly from a gui field). It will be parsed and may be
    translated to several queries in the Xapian sense, for exemple
    several terms and phrases as would result from 
    ["this is a phrase"  term1 term2] . 

    This is why the clauses also have an AND/OR/... type. They are an 
    intermediate form between the primary user input and 
    the final Xapian::Query tree.

    For example, a phrase clause could be added either explicitly or
    using double quotes: {SCLT_PHRASE, [this is a phrase]} or as
    {SCLT_XXX, ["this is a phrase"]}

*/
class SearchData {
public:
    SearchData(SClType tp, const string& stemlang) 
	: m_tp(tp), m_stemlang(stemlang)
    {
	if (m_tp != SCLT_OR && m_tp != SCLT_AND) 
	    m_tp = SCLT_OR;
	commoninit();
    }
    SearchData() 
	: m_tp(SCLT_AND)
    {
	commoninit();
    }
    
    ~SearchData();

    /** Is there anything but a file name search in here ? */
    bool fileNameOnly();

    /** Do we have wildcards anywhere apart from filename searches ? */
    bool haveWildCards() {return m_haveWildCards;}

    /** Translate to Xapian query. rcldb knows about the void*  */
    bool toNativeQuery(Rcl::Db &db, void *);

    /** We become the owner of cl and will delete it */
    bool addClause(SearchDataClause *cl);

    /** If this is a simple query (one field only, no distance clauses),
     * add phrase made of query terms to query, so that docs containing the
     * user terms in order will have higher relevance. This must be called 
     * before toNativeQuery().
     * @param threshold: don't use terms more frequent than the value 
     *     (proportion of docs where they occur) 	
     */
    bool maybeAddAutoPhrase(Rcl::Db &db, double threshold);

    const std::string& getStemLang() {return m_stemlang;}

    void setMinSize(size_t size) {m_minSize = size;}
    void setMaxSize(size_t size) {m_maxSize = size;}

    /** Set date span for filtering results */
    void setDateSpan(DateInterval *dip) {m_dates = *dip; m_haveDates = true;}

    /** Add file type for filtering results */
    void addFiletype(const std::string& ft) {m_filetypes.push_back(ft);}
    /** Add file type to not wanted list */
    void remFiletype(const std::string& ft) {m_nfiletypes.push_back(ft);}

    /** Retrieve error description */
    std::string getReason() {return m_reason;}

    /** Return term expansion data. Mostly used by caller for highlighting
     */
    void getTerms(HighlightData& hldata) const;

    /** 
     * Get/set the description field which is retrieved from xapian after
     * initializing the query. It is stored here for usage in the GUI.
     */
    std::string getDescription() {return m_description;}
    void setDescription(const std::string& d) {m_description = d;}

    /** Return an XML version of the contents, for storage in search history
	by the GUI */
    string asXML();

    void setTp(SClType tp) 
    {
	m_tp = tp;
    }

    void setMaxExpand(int max)
    {
	m_softmaxexpand = max;
    }
    bool getAutoDiac() {return m_autodiacsens;}
    bool getAutoCase() {return m_autocasesens;}
    int getMaxExp() {return m_maxexp;}
    int getMaxCl() {return m_maxcl;}
    int getSoftMaxExp() {return m_softmaxexpand;}

    friend class ::AdvSearch;

private:
    // Combine type. Only SCLT_AND or SCLT_OR here
    SClType                   m_tp; 
    // Complex query descriptor
    std::vector<SearchDataClause*> m_query;
    // Restricted set of filetypes if not empty.
    std::vector<std::string>            m_filetypes; 
    // Excluded set of file types if not empty
    std::vector<std::string>            m_nfiletypes;
    // Autophrase if set. Can't be part of the normal chain because
    // it uses OP_AND_MAYBE
    RefCntr<SearchDataClauseDist>   m_autophrase;
    // 
    bool                      m_haveDates;
    DateInterval              m_dates; // Restrict to date interval
    size_t                    m_maxSize;
    size_t                    m_minSize;
    // Printable expanded version of the complete query, retrieved/set
    // from rcldb after the Xapian::setQuery() call
    std::string m_description; 
    std::string m_reason;
    bool   m_haveWildCards;
    std::string m_stemlang;

    // Parameters set at the start of ToNativeQuery because they need
    // an rclconfig. Actually this does not make sense and it would be
    // simpler to just pass an rclconfig to the constructor;
    bool m_autodiacsens;
    bool m_autocasesens;
    int m_maxexp;
    int m_maxcl;

    // Parameters which are not part of the main query data but may influence
    // translation in special cases.
    // Maximum TermMatch (e.g. wildcard) expansion. This is normally set
    // from the configuration with a high default, but may be set to a lower
    // value during "find-as-you-type" operations from the GUI
    int m_softmaxexpand;

    bool expandFileTypes(Rcl::Db &db, std::vector<std::string>& exptps);
    bool clausesToQuery(Rcl::Db &db, SClType tp,     
			std::vector<SearchDataClause*>& query,
			string& reason, void *d);
    void commoninit();

    /* Copyconst and assignment private and forbidden */
    SearchData(const SearchData &) {}
    SearchData& operator=(const SearchData&) {return *this;};
};

class SearchDataClause {
public:
    enum Modifier {SDCM_NONE=0, SDCM_NOSTEMMING=1, SDCM_ANCHORSTART=2,
		   SDCM_ANCHOREND=4, SDCM_CASESENS=8, SDCM_DIACSENS=16};

    SearchDataClause(SClType tp) 
    : m_tp(tp), m_parentSearch(0), m_haveWildCards(0), 
      m_modifiers(SDCM_NONE), m_weight(1.0), m_exclude(false)
    {}
    virtual ~SearchDataClause() {}
    virtual bool toNativeQuery(Rcl::Db &db, void *) = 0;
    bool isFileName() const {return m_tp == SCLT_FILENAME ? true: false;}
    virtual std::string getReason() const {return m_reason;}
    virtual void getTerms(HighlightData&) const {}

    SClType getTp() const
    {
	return m_tp;
    }
    void setParent(SearchData *p) 
    {
	m_parentSearch = p;
    }
    string getStemLang() 
    {
	return (m_modifiers & SDCM_NOSTEMMING) || m_parentSearch == 0 ? 
	    cstr_null : m_parentSearch->getStemLang();
    }
    bool getAutoDiac()
    {
	return m_parentSearch ? m_parentSearch->getAutoDiac() : false;
    }
    bool getAutoCase()
    {
	return m_parentSearch ? m_parentSearch->getAutoCase() : true;
    }
    int getMaxExp() 
    {
	return m_parentSearch ? m_parentSearch->getMaxExp() : 10000;
    }
    int getMaxCl() 
    {
	return m_parentSearch ? m_parentSearch->getMaxCl() : 100000;
    }
    int getSoftMaxExp() 
    {
	return m_parentSearch ? m_parentSearch->getSoftMaxExp() : -1;
    }
    virtual void setModifiers(Modifier mod) 
    {
	m_modifiers = mod;
    }
    virtual void addModifier(Modifier mod) 
    {
	m_modifiers = Modifier(m_modifiers | mod);
    }
    virtual void setWeight(float w) 
    {
	m_weight = w;
    }
    virtual bool getexclude() const
    {
	return m_exclude;
    }
    virtual void setexclude(bool onoff)
    {
	m_exclude = onoff;
    }

    friend class SearchData;
protected:
    std::string      m_reason;
    SClType     m_tp;
    SearchData *m_parentSearch;
    bool        m_haveWildCards;
    Modifier    m_modifiers;
    float       m_weight;
    bool        m_exclude;
private:
    SearchDataClause(const SearchDataClause&) 
    {
    }
    SearchDataClause& operator=(const SearchDataClause&) 
    {
	return *this;
    }
};
    
/**
 * "Simple" data clause with user-entered query text. This can include 
 * multiple phrases and words, but no specified distance.
 */
class TextSplitQ;
class SearchDataClauseSimple : public SearchDataClause {
public:
    SearchDataClauseSimple(SClType tp, const std::string& txt, 
			   const std::string& fld = std::string())
	: SearchDataClause(tp), m_text(txt), m_field(fld), m_curcl(0)
    {
	m_haveWildCards = 
	    (txt.find_first_of(cstr_minwilds) != std::string::npos);
    }
    SearchDataClauseSimple(const std::string& txt, SClType tp)
	: SearchDataClause(tp), m_text(txt), m_curcl(0)
    {
	m_haveWildCards = 
	    (txt.find_first_of(cstr_minwilds) != std::string::npos);
    }

    virtual ~SearchDataClauseSimple() 
    {
    }

    /** Translate to Xapian query */
    virtual bool toNativeQuery(Rcl::Db &, void *);

    virtual void getTerms(HighlightData& hldata) const
    {
	hldata.append(m_hldata);
    }
    virtual const std::string& gettext() 
    {
	return m_text;
    }
    virtual const std::string& getfield() 
    {
	return m_field;
    }
protected:
    std::string  m_text;  // Raw user entry text.
    std::string  m_field; // Field specification if any
    HighlightData m_hldata;
    // Current count of Xapian clauses, to check against expansion limit
    int  m_curcl;

    bool processUserString(Rcl::Db &db, const string &iq,
			   std::string &ermsg,
			   void* pq, int slack = 0, bool useNear = false);
    bool expandTerm(Rcl::Db &db, std::string& ermsg, int mods, 
		    const std::string& term, 
		    std::vector<std::string>& exp, 
                    std::string& sterm, const std::string& prefix);
    // After splitting entry on whitespace: process non-phrase element
    void processSimpleSpan(Rcl::Db &db, string& ermsg, const string& span, 
			   int mods, void *pq);
    // Process phrase/near element
    void processPhraseOrNear(Rcl::Db &db, string& ermsg, TextSplitQ *splitData, 
			     int mods, void *pq, bool useNear, int slack);
};

/** 
 * Filename search clause. This is special because term expansion is only
 * performed against the unsplit file name terms. 
 *
 * There is a big advantage in expanding only against the
 * field, especially for file names, because this makes searches for
 * "*xx" much faster (no need to scan the whole main index).
 */
class SearchDataClauseFilename : public SearchDataClauseSimple {
public:
    SearchDataClauseFilename(const std::string& txt)
	: SearchDataClauseSimple(txt, SCLT_FILENAME)
    {
	// File name searches don't count when looking for wild cards.
	m_haveWildCards = false;
    }

    virtual ~SearchDataClauseFilename() 
    {
    }

    virtual bool toNativeQuery(Rcl::Db &, void *);
};


/** 
 * Pathname filtering clause. This is special because of history:
 *  - Pathname filtering used to be performed as a post-processing step 
 *    done with the url fields of doc data records.
 *  - Then it was done as special phrase searchs on path elements prefixed
 *    with XP.
 *  Up to this point dir filtering data was stored as part of the searchdata
 *  object, not in the SearchDataClause tree. Only one, then a list,
 *  of clauses where stored, and they were always ANDed together.
 *
 *  In order to allow for OR searching, dir clauses are now stored in a
 *  specific SearchDataClause, but this is still special because the field has
 *  non-standard phrase-like processing, reflected in index storage by
 *  an empty element representing / (as "XP").
 * 
 * A future version should use a standard phrase with an anchor to the
 * start if the path starts with /. As this implies an index format
 * change but is no important enough to warrant it, this has to wait for
 * the next format change.
 */
class SearchDataClausePath : public SearchDataClauseSimple {
public:
    SearchDataClausePath(const std::string& txt, bool excl = false)
	: SearchDataClauseSimple(SCLT_PATH, txt, "dir")
    {
	m_exclude = excl;
	m_haveWildCards = false;
    }

    virtual ~SearchDataClausePath() 
    {
    }

    virtual bool toNativeQuery(Rcl::Db &, void *);

};

/** 
 * A clause coming from a NEAR or PHRASE entry field. There is only one 
 * std::string group, and a specified distance, which applies to it.
 */
class SearchDataClauseDist : public SearchDataClauseSimple {
public:
    SearchDataClauseDist(SClType tp, const std::string& txt, int slack, 
			 const std::string& fld = std::string())
	: SearchDataClauseSimple(tp, txt, fld), m_slack(slack)
    {
    }

    virtual ~SearchDataClauseDist() 
    {
    }

    virtual bool toNativeQuery(Rcl::Db &, void *);
    virtual int getslack() const
    {
	return m_slack;
    }
private:
    int m_slack;
};

/** Subquery */
class SearchDataClauseSub : public SearchDataClause {
public:
    SearchDataClauseSub(RefCntr<SearchData> sub) 
	: SearchDataClause(SCLT_SUB), m_sub(sub) 
    {
    }
    virtual bool toNativeQuery(Rcl::Db &db, void *p)
    {
	bool ret = m_sub->toNativeQuery(db, p);
	if (!ret) 
	    m_reason = m_sub->getReason();
	return ret;
    }

    virtual void getTerms(HighlightData& hldata) const
    {
	m_sub.getconstptr()->getTerms(hldata);
    }

protected:
    RefCntr<SearchData> m_sub;
};

} // Namespace Rcl

#endif /* _SEARCHDATA_H_INCLUDED_ */
