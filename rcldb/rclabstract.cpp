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
#include "autoconfig.h"

#include <math.h>

#include <map>
#include "unordered_defs.h"

using namespace std;

#include "debuglog.h"
#include "rcldb.h"
#include "rcldb_p.h"
#include "rclquery.h"
#include "rclquery_p.h"
#include "textsplit.h"
#include "searchdata.h"
#include "utf8iter.h"
#include "hldata.h"

namespace Rcl {
// This is used as a marker inside the abstract frag lists, but
// normally doesn't remain in final output (which is built with a
// custom sep. by our caller).
static const string cstr_ellipsis("...");
// This is used to mark positions overlapped by a multi-word match term
static const string occupiedmarker("?");

#undef DEBUGABSTRACT  
#ifdef DEBUGABSTRACT
#define LOGABS LOGDEB
static void listList(const string& what, const vector<string>&l)
{
    string a;
    for (vector<string>::const_iterator it = l.begin(); it != l.end(); it++) {
        a = a + *it + " ";
    }
    LOGDEB(("%s: %s\n", what.c_str(), a.c_str()));
}
#else
#define LOGABS LOGDEB2
static void listList(const string&, const vector<string>&)
{
}
#endif

// Unprefix terms. Actually it's not completely clear if we should
// remove prefixes and keep all terms or prune the prefixed
// ones. There is no good way to be sure what will provide the best
// result in general.
static const bool prune_prefixed_terms = true; 
static void noPrefixList(const vector<string>& in, vector<string>& out) 
{
    for (vector<string>::const_iterator qit = in.begin(); 
	 qit != in.end(); qit++) {
	if (prune_prefixed_terms) {
	    if (has_prefix(*qit))
		continue;
	}
	out.push_back(strip_prefix(*qit));
    }
    sort(out.begin(), out.end());
    vector<string>::iterator it = unique(out.begin(), out.end());
    out.resize(it - out.begin());
}

bool Query::Native::getMatchTerms(unsigned long xdocid, vector<string>& terms)
{
    if (!xenquire) {
	LOGERR(("Query::getMatchTerms: no query opened\n"));
	return -1;
    }

    terms.clear();
    Xapian::TermIterator it;
    Xapian::docid id = Xapian::docid(xdocid);
    vector<string> iterms;
    XAPTRY(iterms.insert(iterms.begin(),
                        xenquire->get_matching_terms_begin(id),
                        xenquire->get_matching_terms_end(id)),
           m_q->m_db->m_ndb->xrdb, m_q->m_reason);
    if (!m_q->m_reason.empty()) {
	LOGERR(("getMatchTerms: xapian error: %s\n", m_q->m_reason.c_str()));
	return false;
    }
    noPrefixList(iterms, terms);
    return true;
}

// Retrieve db-wide frequencies for the query terms and store them in
// the query object. This is done at most once for a query, and the data is used
// while computing abstracts for the different result documents.
void Query::Native::setDbWideQTermsFreqs()
{
    // Do it once only for a given query.
    if (!termfreqs.empty())
	return;

    vector<string> qterms;
    {
	vector<string> iqterms;
	m_q->getQueryTerms(iqterms);
	noPrefixList(iqterms, qterms);
    }
    // listList("Query terms: ", qterms);
    Xapian::Database &xrdb = m_q->m_db->m_ndb->xrdb;

    double doccnt = xrdb.get_doccount();
    if (doccnt == 0) 
	doccnt = 1;

    for (vector<string>::const_iterator qit = qterms.begin(); 
	 qit != qterms.end(); qit++) {
	termfreqs[*qit] = xrdb.get_termfreq(*qit) / doccnt;
	LOGABS(("setDbWideQTermFreqs: [%s] db freq %.1e\n", qit->c_str(), 
		termfreqs[*qit]));
    }
}

// Compute matched terms quality coefficients for a matched document by
// retrieving the Within Document Frequencies and multiplying by
// overal term frequency, then using log-based thresholds.
// 2012: it's not too clear to me why exactly we do the log thresholds thing.
//  Preferring terms wich are rare either or both in the db and the document 
//  seems reasonable though
// To avoid setting a high quality for a low frequency expansion of a
// common stem, which seems wrong, we group the terms by
// root, compute a frequency for the group from the sum of member
// occurrences, and let the frequency for each group member be the
// aggregated frequency.
double Query::Native::qualityTerms(Xapian::docid docid, 
				   const vector<string>& terms,
				   multimap<double, vector<string> >& byQ)
{
    LOGABS(("qualityTerms\n"));
    setDbWideQTermsFreqs();

    map<string, double> termQcoefs;
    double totalweight = 0;

    Xapian::Database &xrdb = m_q->m_db->m_ndb->xrdb;
    double doclen = xrdb.get_doclength(docid);
    if (doclen == 0) 
	doclen = 1;
    HighlightData hld;
    if (!m_q->m_sd.isNull()) {
	m_q->m_sd->getTerms(hld);
    }

#ifdef DEBUGABSTRACT
    {
	string deb;
	hld.toString(deb);
	LOGABS(("qualityTerms: hld: %s\n", deb.c_str()));
    }
#endif

    // Group the input terms by the user term they were possibly expanded from
    map<string, vector<string> > byRoot;
    for (vector<string>::const_iterator qit = terms.begin(); 
	 qit != terms.end(); qit++) {
	bool found = false;
	map<string, string>::const_iterator eit = hld.terms.find(*qit);
	if (eit != hld.terms.end()) {
	    byRoot[eit->second].push_back(*qit);
	} else {
	    LOGDEB0(("qualityTerms: [%s] not found in hld\n", (*qit).c_str()));
	    byRoot[*qit].push_back(*qit);
	}
    }

#ifdef DEBUGABSTRACT
    {
	string byRootstr;
	for (map<string, vector<string> >::const_iterator debit = 
		 byRoot.begin();  debit != byRoot.end(); debit++) {
	    byRootstr.append("[").append(debit->first).append("]->");
	    for (vector<string>::const_iterator it = debit->second.begin();
		 it != debit->second.end(); it++) {
		byRootstr.append("[").append(*it).append("] ");
	    }
	    byRootstr.append("\n");
	}
	LOGABS(("\nqualityTerms: uterms to terms: %s\n", byRootstr.c_str()));
    }
#endif

    // Compute in-document and global frequencies for the groups.
    map<string, double> grpwdfs;
    map<string, double> grptfreqs;
    for (map<string, vector<string> >::const_iterator git = byRoot.begin();
	 git != byRoot.end(); git++) {
	for (vector<string>::const_iterator qit = git->second.begin(); 
	     qit != git->second.end(); qit++) {
	    Xapian::TermIterator term = xrdb.termlist_begin(docid);
	    term.skip_to(*qit);
	    if (term != xrdb.termlist_end(docid) && *term == *qit) {
		if (grpwdfs.find(git->first) != grpwdfs.end()) {
		    grpwdfs[git->first] = term.get_wdf() / doclen;
		    grptfreqs[git->first] = termfreqs[*qit];
		} else {
		    grpwdfs[git->first] += term.get_wdf() / doclen;
		    grptfreqs[git->first] += termfreqs[*qit];
		}
	    }
	}    
    }

    // Build a sorted by quality container for the groups
    for (map<string, vector<string> >::const_iterator git = byRoot.begin();
	 git != byRoot.end(); git++) {
	double q = (grpwdfs[git->first]) * grptfreqs[git->first];
	q = -log10(q);
	if (q < 3) {
	    q = 0.05;
	} else if (q < 4) {
	    q = 0.3;
	} else if (q < 5) {
	    q = 0.7;
	} else if (q < 6) {
	    q = 0.8;
	} else {
	    q = 1;
	}
	totalweight += q;
	byQ.insert(pair<double, vector<string> >(q, git->second));
    }

#ifdef DEBUGABSTRACT
    for (multimap<double, vector<string> >::reverse_iterator mit= byQ.rbegin(); 
	 mit != byQ.rend(); mit++) {
	LOGABS(("qualityTerms: group\n"));
	for (vector<string>::const_iterator qit = mit->second.begin();
	     qit != mit->second.end(); qit++) {
	    LOGABS(("%.1e->[%s]\n", mit->first, qit->c_str()));
	}
    }
#endif
    return totalweight;
}

// Return page number for first match of "significant" term.
int Query::Native::getFirstMatchPage(Xapian::docid docid, string& term)
{
    if (!m_q|| !m_q->m_db || !m_q->m_db->m_ndb || !m_q->m_db->m_ndb->m_isopen) {
	LOGERR(("Query::getFirstMatchPage: no db\n"));
	return -1;
    }
    Rcl::Db::Native *ndb(m_q->m_db->m_ndb);
    Xapian::Database& xrdb(ndb->xrdb);

    vector<string> terms;
    getMatchTerms(docid, terms);

    if (terms.empty()) {
	LOGDEB(("getFirstMatchPage: empty match term list (field match?)\n"));
	return -1;
    }

    vector<int> pagepos;
    ndb->getPagePositions(docid, pagepos);
    if (pagepos.empty())
	return -1;
	
    setDbWideQTermsFreqs();

    // We try to use a page which matches the "best" term. Get a sorted list
    multimap<double, vector<string> > byQ;
    double totalweight = qualityTerms(docid, terms, byQ);

    for (multimap<double, vector<string> >::reverse_iterator mit = byQ.rbegin(); 
	 mit != byQ.rend(); mit++) {
	for (vector<string>::const_iterator qit = mit->second.begin();
	     qit != mit->second.end(); qit++) {
	    string qterm = *qit;
	    Xapian::PositionIterator pos;
	    string emptys;
	    try {
		for (pos = xrdb.positionlist_begin(docid, qterm); 
		     pos != xrdb.positionlist_end(docid, qterm); pos++) {
		    int pagenum = ndb->getPageNumberForPosition(pagepos, *pos);
		    if (pagenum > 0) {
			term = qterm;
			return pagenum;
		    }
		}
	    } catch (...) {
		// Term does not occur. No problem.
	    }
	}
    }
    return -1;
}

// Build a document abstract by extracting text chunks around the query terms
// This uses the db termlists, not the original document.
//
// DatabaseModified and other general exceptions are catched and
// possibly retried by our caller
int Query::Native::makeAbstract(Xapian::docid docid,
				vector<Snippet>& vabs, 
				int imaxoccs, int ictxwords)
{
    Chrono chron;
    LOGABS(("makeAbstract: docid %ld imaxoccs %d ictxwords %d\n", 
	    long(docid), imaxoccs, ictxwords));

    // The (unprefixed) terms matched by this document
    vector<string> matchedTerms;
    getMatchTerms(docid, matchedTerms);
    if (matchedTerms.empty()) {
	LOGDEB(("makeAbstract::Empty term list\n"));
	return ABSRES_ERROR;
    }

    listList("Match terms: ", matchedTerms);

    // Retrieve the term frequencies for the query terms. This is
    // actually computed only once for a query, and for all terms in
    // the query (not only the matches for this doc)
    setDbWideQTermsFreqs();

    // Build a sorted by quality container for the match terms We are
    // going to try and show text around the less common search terms.
    // Terms issued from an original one by stem expansion are
    // aggregated by the qualityTerms() routine.
    multimap<double, vector<string> > byQ;
    double totalweight = qualityTerms(docid, matchedTerms, byQ);
    LOGABS(("makeAbstract:%d: computed Qcoefs.\n", chron.ms()));
    // This can't happen, but would crash us
    if (totalweight == 0.0) {
	LOGERR(("makeAbstract: totalweight == 0.0 !\n"));
	return ABSRES_ERROR;
    }

    Rcl::Db::Native *ndb(m_q->m_db->m_ndb);
    Xapian::Database& xrdb(ndb->xrdb);

    ///////////////////
    // For each of the query terms, ask xapian for its positions list
    // in the document. For each position entry, insert it and its
    // neighbours in the set of 'interesting' positions

    // The terms 'array' that we partially populate with the document
    // terms, at their positions around the search terms positions:
    map<unsigned int, string> sparseDoc;
    // Also remember apart the search term positions so that we can list
    // them with their snippets.
    STD_UNORDERED_SET<unsigned int> searchTermPositions;

    // Remember max position. Used to stop walking positions lists while 
    // populating the adjacent slots.
    unsigned int maxpos = 0;

    // Total number of occurences for all terms. We stop when we have too much
    unsigned int totaloccs = 0;

    // Total number of slots we populate. The 7 is taken as
    // average word size. It was a mistake to have the user max
    // abstract size parameter in characters, we basically only deal
    // with words. We used to limit the character size at the end, but
    // this damaged our careful selection of terms
    const unsigned int maxtotaloccs = imaxoccs > 0 ? imaxoccs :
	m_q->m_db->getAbsLen() /(7 * (m_q->m_db->getAbsCtxLen() + 1));
    int ctxwords = ictxwords == -1 ? m_q->m_db->getAbsCtxLen() : ictxwords;
    LOGABS(("makeAbstract:%d: mxttloccs %d ctxwords %d\n", 
	    chron.ms(), maxtotaloccs, ctxwords));

    int ret = ABSRES_OK;

    // Let's go populate
    for (multimap<double, vector<string> >::reverse_iterator mit = byQ.rbegin(); 
	 mit != byQ.rend(); mit++) {
	unsigned int maxgrpoccs;
	float q;
	if (byQ.size() == 1) {
	    maxgrpoccs = maxtotaloccs;
	    q = 1.0;
	} else {
	    // We give more slots to the better term groups
	    q = mit->first / totalweight;
	    maxgrpoccs = int(ceil(maxtotaloccs * q));
	}
	unsigned int grpoccs = 0;

	for (vector<string>::const_iterator qit = mit->second.begin();
	     qit != mit->second.end(); qit++) {

	    // Group done ?
	    if (grpoccs >= maxgrpoccs) 
		break;

	    string qterm = *qit;

	    LOGABS(("makeAbstract: [%s] %d max grp occs (coef %.2f)\n", 
		    qterm.c_str(), maxgrpoccs, q));

	    // The match term may span several words
	    int qtrmwrdcnt = 
		TextSplit::countWords(qterm, TextSplit::TXTS_NOSPANS);

	    Xapian::PositionIterator pos;
	    // There may be query terms not in this doc. This raises an
	    // exception when requesting the position list, we catch it ??
	    // Not clear how this can happen because we are walking the
	    // match list returned by Xapian. Maybe something with the
	    // fields?
	    string emptys;
	    try {
		for (pos = xrdb.positionlist_begin(docid, qterm); 
		     pos != xrdb.positionlist_end(docid, qterm); pos++) {
		    int ipos = *pos;
		    if (ipos < int(baseTextPosition)) // Not in text body
			continue;
		    LOGABS(("makeAbstract: [%s] at pos %d grpoccs %d maxgrpoccs"
			    " %d\n", qterm.c_str(), ipos, grpoccs, maxgrpoccs));

		    totaloccs++;
		    grpoccs++;

		    // Add adjacent slots to the set to populate at next
		    // step by inserting empty strings. Special provisions
		    // for adding ellipsis and for positions overlapped by
		    // the match term.
		    unsigned int sta = MAX(int(baseTextPosition), 
					   ipos - ctxwords);
		    unsigned int sto = ipos + qtrmwrdcnt-1 + 
			m_q->m_db->getAbsCtxLen();
		    for (unsigned int ii = sta; ii <= sto;  ii++) {
			if (ii == (unsigned int)ipos) {
			    sparseDoc[ii] = qterm;
			    searchTermPositions.insert(ii);
			    if (ii > maxpos)
				maxpos = ii;
			} else if (ii > (unsigned int)ipos && 
				   ii < (unsigned int)ipos + qtrmwrdcnt) {
			    sparseDoc[ii] = occupiedmarker;
			} else if (!sparseDoc[ii].compare(cstr_ellipsis)) {
			    // For an empty slot, the test has a side
			    // effect of inserting an empty string which
			    // is what we want.
			    sparseDoc[ii] = emptys;
			}
		    }
		    // Add ellipsis at the end. This may be replaced later by
		    // an overlapping extract. Take care not to replace an
		    // empty string here, we really want an empty slot,
		    // use find()
		    if (sparseDoc.find(sto+1) == sparseDoc.end()) {
			sparseDoc[sto+1] = cstr_ellipsis;
		    }

		    // Group done ?
		    if (grpoccs >= maxgrpoccs) {
			ret |= ABSRES_TRUNC;
			LOGABS(("Db::makeAbstract: max group occs cutoff\n"));
			break;
		    }
		    // Global done ?
		    if (totaloccs >= maxtotaloccs) {
			ret |= ABSRES_TRUNC;
			LOGABS(("Db::makeAbstract: max occurrences cutoff\n"));
			break;
		    }
		}
	    } catch (...) {
		// Term does not occur. No problem.
	    }

	    if (totaloccs >= maxtotaloccs) {
		ret |= ABSRES_TRUNC;
		LOGABS(("Db::makeAbstract: max1 occurrences cutoff\n"));
		break;
	    }
	}
    }
    maxpos += ctxwords + 1;

    LOGABS(("makeAbstract:%d:chosen number of positions %d\n", 
	    chron.millis(), totaloccs));
    // This can happen if there are term occurences in the keywords
    // etc. but not elsewhere ?
    if (totaloccs == 0) {
	LOGDEB(("makeAbstract: no occurrences\n"));
	return ABSRES_OK;
    }

    // Walk all document's terms position lists and populate slots
    // around the query terms. We arbitrarily truncate the list to
    // avoid taking forever. If we do cutoff, the abstract may be
    // inconsistant (missing words, potentially altering meaning),
    // which is bad. 
    { 
	Xapian::TermIterator term;
	int cutoff = m_q->m_snipMaxPosWalk;
	for (term = xrdb.termlist_begin(docid);
	     term != xrdb.termlist_end(docid); term++) {
	    // Ignore prefixed terms
	    if (has_prefix(*term))
		continue;
	    if (m_q->m_snipMaxPosWalk > 0 && cutoff-- < 0) {
		ret |= ABSRES_TERMMISS;
		LOGDEB0(("makeAbstract: max term count cutoff %d\n", 
			 m_q->m_snipMaxPosWalk));
		break;
	    }

	    map<unsigned int, string>::iterator vit;
	    Xapian::PositionIterator pos;
	    for (pos = xrdb.positionlist_begin(docid, *term); 
		 pos != xrdb.positionlist_end(docid, *term); pos++) {
		if (m_q->m_snipMaxPosWalk > 0 && cutoff-- < 0) {
		    ret |= ABSRES_TERMMISS;
		    LOGDEB0(("makeAbstract: max term count cutoff %d\n", 
			    m_q->m_snipMaxPosWalk));
		    break;
		}
		// If we are beyond the max possible position, stop
		// for this term
		if (*pos > maxpos) {
		    break;
		}
		if ((vit = sparseDoc.find(*pos)) != sparseDoc.end()) {
		    // Don't replace a term: the terms list is in
		    // alphabetic order, and we may have several terms
		    // at the same position, we want to keep only the
		    // first one (ie: dockes and dockes@wanadoo.fr)
		    if (vit->second.empty()) {
			LOGDEB2(("makeAbstract: populating: [%s] at %d\n", 
				 (*term).c_str(), *pos));
			sparseDoc[*pos] = *term;
		    }
		}
	    }
	}
    }

#if 0
    // Debug only: output the full term[position] vector
    bool epty = false;
    int ipos = 0;
    for (map<unsigned int, string>::iterator it = sparseDoc.begin(); 
	 it != sparseDoc.end();
	 it++, ipos++) {
	if (it->empty()) {
	    if (!epty)
		LOGDEB(("makeAbstract:vec[%d]: [%s]\n", ipos, it->c_str()));
	    epty=true;
	} else {
	    epty = false;
	    LOGDEB(("makeAbstract:vec[%d]: [%s]\n", ipos, it->c_str()));
	}
    }
#endif

    vector<int> vpbreaks;
    ndb->getPagePositions(docid, vpbreaks);

    LOGABS(("makeAbstract:%d: extracting. Got %u pages\n", chron.millis(),
	    vpbreaks.size()));
    // Finally build the abstract by walking the map (in order of position)
    vabs.clear();
    string chunk;
    bool incjk = false;
    int page = 0;
    string term;
    for (map<unsigned int, string>::const_iterator it = sparseDoc.begin();
	 it != sparseDoc.end(); it++) {
	LOGDEB2(("Abtract:output %u -> [%s]\n", it->first, it->second.c_str()));
	if (!occupiedmarker.compare(it->second)) {
	    LOGDEB(("Abstract: qtrm position not filled ??\n"));
	    continue;
	}
	if (chunk.empty() && !vpbreaks.empty()) {
	    page =  ndb->getPageNumberForPosition(vpbreaks, it->first);
	    if (page < 0) 
		page = 0;
	    term.clear();
	}
	Utf8Iter uit(it->second);
	bool newcjk = false;
	if (TextSplit::isCJK(*uit))
	    newcjk = true;
	if (!incjk || (incjk && !newcjk))
	    chunk += " ";
	incjk = newcjk;
	if (searchTermPositions.find(it->first) != searchTermPositions.end())
	    term = it->second;
	if (it->second == cstr_ellipsis) {
	    vabs.push_back(Snippet(page, chunk).setTerm(term));
	    chunk.clear();
	} else {
	    if (it->second.compare(end_of_field_term) && 
		it->second.compare(start_of_field_term))
		chunk += it->second;
	}
    }
    if (!chunk.empty())
	vabs.push_back(Snippet(page, chunk).setTerm(term));

    LOGDEB2(("makeAbtract: done in %d mS\n", chron.millis()));
    return ret;
}


}
