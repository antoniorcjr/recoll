/* Copyright (C) 2011 J.F.Dockes
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


#ifndef _TERMPROC_H_INCLUDED_
#define _TERMPROC_H_INCLUDED_

#include "textsplit.h"
#include "stoplist.h"

namespace Rcl {

/** 
 * Termproc objects take a stream of term tokens as input and do something
 * with them: transform to lowercase, filter out stop words, generate n-grams,
 * finally index or generate search clauses, etc. They are chained and can 
 * be arranged to form different pipelines depending on the desired processing
 * steps: for example, optional stoplist or commongram processing.
 *
 * Shared processing steps are defined in this file. The first and last steps
 * (ie: adding index term) are usually defined in the specific module.
 */

/** 
 * The base class takes care of chaining: all derived classes call its 
 * takeword() and flush() methods to ensure that terms go through the pipe.
 */
class TermProc {
public:
    TermProc(TermProc* next) : m_next(next) {}
    virtual ~TermProc() {}
    virtual bool takeword(const string &term, int pos, int bs, int be)
    {
	if (m_next)
	    return m_next->takeword(term, pos, bs, be);
	else
	    return true;
    }
    virtual void newpage(int pos)
    {
	if (m_next)
	    m_next->newpage(pos);
    }
    virtual bool flush()
    {
	if (m_next)
	    return m_next->flush();
	else
	    return true;
    }
private:
    TermProc *m_next;
    /* Copyconst and assignment private and forbidden */
    TermProc(const TermProc &) {}
    TermProc& operator=(const TermProc &) {return *this;};
};

/** 
 * Specialized TextSplit class: this will probably replace the base
 * TextSplit when we've converted all the code. The takeword() routine in this
 * calls a TermProc's instead of being overriden in a user derived class.
 * The text_to_words() method also takes care of flushing.
 */
class TextSplitP : public TextSplit {
public:
    TextSplitP(TermProc *prc, Flags flags = Flags(TXTS_NONE))
	: TextSplit(flags), m_prc(prc)  {}

    virtual bool text_to_words(const string &in)
    {
	bool ret = TextSplit::text_to_words(in);
	if (m_prc && !m_prc->flush())
	    return false;
	return ret;
    }

    virtual bool takeword(const string& term, int pos, int bs, int be)
    {
	if (m_prc)
	    return m_prc->takeword(term, pos, bs, be);
	else
	    return true;
    }
    virtual void newpage(int pos)
    {
	if (m_prc)
	    return m_prc->newpage(pos);
    }

private:
    TermProc *m_prc;
};

/** Unaccent and lowercase term. This is usually the first in the pipeline */
class TermProcPrep : public TermProc {
public:
    TermProcPrep(TermProc *nxt)	
	: TermProc(nxt), m_totalterms(0), m_unacerrors(0) 
    {
    }

    virtual bool takeword(const string& itrm, int pos, int bs, int be)
    {
	m_totalterms++;
	string otrm;
	if (!unacmaybefold(itrm, otrm, "UTF-8", UNACOP_UNACFOLD)) {
	    LOGDEB(("splitter::takeword: unac [%s] failed\n", itrm.c_str()));
	    m_unacerrors++;
	    // We don't generate a fatal error because of a bad term,
	    // but one has to put the limit somewhere
	    if (m_unacerrors > 500 && 
		(double(m_totalterms) / double(m_unacerrors)) < 2.0) {
		// More than 1 error for every other term
		LOGERR(("splitter::takeword: too many unac errors %d/%d\n",
			m_unacerrors, m_totalterms));
		return false;
	    }
	    return true;
	}
	// It may happen in some weird cases that the output from unac is 
	// empty (if the word actually consisted entirely of diacritics ...)
	// The consequence is that a phrase search won't work without addional
	// slack. 
	if (otrm.empty())
	    return true;
	else
	    return TermProc::takeword(otrm, pos, bs, be);
    }

    virtual bool flush()
    {
	m_totalterms = m_unacerrors = 0;
	return TermProc::flush();
    }

private:
    int m_totalterms;
    int m_unacerrors;
};

/** Compare to stop words list and discard if match found */
class TermProcStop : public TermProc {
public:
    TermProcStop(TermProc *nxt, const Rcl::StopList& stops)
	: TermProc(nxt), m_stops(stops) 
    {
    }

    virtual bool takeword(const string& term, int pos, int bs, int be)
    {
	if (m_stops.isStop(term)) {
	    return true;
	}
	return TermProc::takeword(term, pos, bs, be);
    }

private:
    const Rcl::StopList& m_stops;
};

/** Handle common-gram generation: combine frequent terms with neighbours to
 *  shorten the positions lists for phrase searches.
 *  NOTE: This does not currently work because of bad interaction with the 
 *  spans (ie john@domain.com) generation in textsplit. Not used, kept for
 *  testing only
 */
class TermProcCommongrams : public TermProc {
public:
    TermProcCommongrams(TermProc *nxt, const Rcl::StopList& stops)
	: TermProc(nxt), m_stops(stops), m_onlygrams(false) 
    {
    }

    virtual bool takeword(const string& term, int pos, int bs, int be)
    {
	LOGDEB1(("TermProcCom::takeword: pos %d %d %d [%s]\n", 
		 pos, bs, be, term.c_str()));
	bool isstop = m_stops.isStop(term);
	bool twogramemit = false;

	if (!m_prevterm.empty() && (m_prevstop || isstop)) {
	    // create 2-gram. space unnecessary but improves
	    // the readability of queries
	    string twogram;
	    twogram.swap(m_prevterm);
	    twogram.append(1, ' ');
	    twogram += term;
	    // When emitting a complex term we set the bps to 0. This may
	    // be used by our clients
	    if (!TermProc::takeword(twogram, m_prevpos, 0, 0))
		return false;
	    twogramemit = true;
#if 0
	    if (m_stops.isStop(twogram)) {
		firstword = twogram;
		isstop = false;
	    }
#endif
	}
	
	m_prevterm = term;
	m_prevstop = isstop;
	m_prevpos = pos;
	m_prevsent = false;
	m_prevbs = bs;
	m_prevbe = be;
	// If flags allow, emit the bare term at the current pos.
	if (!m_onlygrams || (!isstop && !twogramemit)) {
	    if (!TermProc::takeword(term, pos, bs, be))
		return false;
	    m_prevsent = true;
	} 

	return true;
    }

    virtual bool flush()
    {
	if (!m_prevsent && !m_prevterm.empty())
	    if (!TermProc::takeword(m_prevterm, m_prevpos, m_prevbs, m_prevbe))
		return false;
	    
	m_prevterm.clear();
	m_prevsent = true;
	return TermProc::flush();
    }
    void onlygrams(bool on)
    {
	m_onlygrams = on;
    }
private:
    // The stoplist we're using
    const Rcl::StopList& m_stops;
    // Remembered data for the last processed term
    string m_prevterm;
    bool   m_prevstop;
    int    m_prevpos;
    int    m_prevbs;
    int    m_prevbe;
    bool   m_prevsent;
    // If this is set, we only emit longest grams
    bool   m_onlygrams;
};


} // End namespace Rcl

#endif /* _TERMPROC_H_INCLUDED_ */
