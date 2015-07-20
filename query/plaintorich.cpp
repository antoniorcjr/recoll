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


#include <string>
#include <utility>
#include <list>
#include <set>
#include <vector>
#include <map>
#include <algorithm>

using std::vector;
using std::list;
using std::pair;
using std::set;

#include "rcldb.h"
#include "rclconfig.h"
#include "debuglog.h"
#include "textsplit.h"
#include "utf8iter.h"
#include "smallut.h"
#include "plaintorich.h"
#include "cancelcheck.h"
#include "unacpp.h"

// For debug printing
static string vecStringToString(const vector<string>& t)
{
    string sterms;
    for (vector<string>::const_iterator it = t.begin(); it != t.end(); it++) {
	sterms += "[" + *it + "] ";
    }
    return sterms;
}

struct MatchEntry {
    // Start/End byte offsets in the document text
    pair<int, int> offs;
    // Index of the search group this comes from: this is to relate a 
    // match to the original user input.
    unsigned int grpidx;
    MatchEntry(int sta, int sto, unsigned int idx) 
	: offs(sta, sto), grpidx(idx)
    {
    }
};

// Text splitter used to take note of the position of query terms
// inside the result text. This is then used to insert highlight tags.
class TextSplitPTR : public TextSplit {
 public:

    // Out: begin and end byte positions of query terms/groups in text
    vector<MatchEntry> tboffs;  

    TextSplitPTR(const HighlightData& hdata)
    :  m_wcount(0), m_hdata(hdata)
    {
	// We separate single terms and groups and extract the group
	// terms for computing positions list before looking for group
	// matches
	for (vector<vector<string> >::const_iterator vit = hdata.groups.begin();
	     vit != hdata.groups.end(); vit++) {
	    if (vit->size() == 1) {
		m_terms[vit->front()] = vit - hdata.groups.begin();
	    } else if (vit->size() > 1) {
		for (vector<string>::const_iterator it = vit->begin(); 
		     it != vit->end(); it++) {
		    m_gterms.insert(*it);
		}
	    }
	}
    }

    // Accept word and its position. If word is search term, add
    // highlight zone definition. If word is part of search group
    // (phrase or near), update positions list.
    virtual bool takeword(const std::string& term, int pos, int bts, int bte) {
	string dumb = term;
	if (o_index_stripchars) {
	    if (!unacmaybefold(term, dumb, "UTF-8", UNACOP_UNACFOLD)) {
		LOGINFO(("PlainToRich::takeword: unac failed for [%s]\n",
			 term.c_str()));
		return true;
	    }
	}

	//LOGDEB2(("Input dumbbed term: '%s' %d %d %d\n", dumb.c_str(), 
	// pos, bts, bte));

	// If this word is a search term, remember its byte-offset span. 
	map<string, unsigned int>::const_iterator it = m_terms.find(dumb);
	if (it != m_terms.end()) {
	    tboffs.push_back(MatchEntry(bts, bte, (*it).second));
	}
	
	// If word is part of a search group, update its positions list
	if (m_gterms.find(dumb) != m_gterms.end()) {
	    // Term group (phrase/near) handling
	    m_plists[dumb].push_back(pos);
	    m_gpostobytes[pos] = pair<int,int>(bts, bte);
	    //LOGDEB2(("Recorded bpos for %d: %d %d\n", pos, bts, bte));
	}

	// Check for cancellation request
	if ((m_wcount++ & 0xfff) == 0)
	    CancelCheck::instance().checkCancel();

	return true;
    }

    // Must be called after the split to find the phrase/near match positions
    virtual bool matchGroups();

private:
    virtual bool matchGroup(unsigned int idx);

    // Word count. Used to call checkCancel from time to time.
    int m_wcount;

    // In: user query terms
    map<string, unsigned int>    m_terms; 

    // m_gterms holds all the terms in m_groups, as a set for quick lookup
    set<string>    m_gterms;

    const HighlightData& m_hdata;

    // group/near terms word positions.
    map<string, vector<int> > m_plists;
    map<int, pair<int, int> > m_gpostobytes;
};


/** Sort by shorter comparison class */
class VecIntCmpShorter {
    public:
	/** Return true if and only if a is strictly shorter than b.
	 */
        bool operator()(const vector<int> *a, const vector<int> *b) {
            return a->size() < b->size();
        }
};

#define SETMINMAX(POS, STA, STO)  {if ((POS) < (STA)) (STA) = (POS); \
	if ((POS) > (STO)) (STO) = (POS);}

// Check that at least an entry from the first position list is inside
// the window and recurse on next list. The window is readjusted as
// the successive terms are found.
//
// @param window the search window width
// @param plists the position list vector
// @param i the position list to process (we then recurse with the next list)
// @param min the current minimum pos for a found term
// @param max the current maximum pos for a found term
// @param sp, ep output: the found area
// @param minpos bottom of search: this is the highest point of
//    any previous match. We don't look below this as overlapping matches 
//    make no sense for highlighting.
static bool do_proximity_test(int window, vector<vector<int>* >& plists, 
			      unsigned int i, int min, int max, 
			      int *sp, int *ep, int minpos)
{
    LOGDEB1(("do_prox_test: win %d i %d min %d max %d minpos %d\n", 
	     window, i, min, max, minpos));
    int tmp = max + 1 - window;
    if (tmp < minpos)
	tmp = minpos;

    // Find 1st position bigger than window start
    vector<int>::iterator it = plists[i]->begin();
    while (it != plists[i]->end() && *it < tmp)
	it++;

    // Look for position inside window. If not found, no match. If
    // found: if this is the last list we're done, else recurse on
    // next list after adjusting the window
    while (it != plists[i]->end()) {
	int pos = *it;
	if (pos > min + window - 1) 
	    return false;
	if (i + 1 == plists.size()) {
	    SETMINMAX(pos, *sp, *ep);
	    return true;
	}
	SETMINMAX(pos, min, max);
	if (do_proximity_test(window,plists, i + 1, min, max, sp, ep, minpos)) {
	    SETMINMAX(pos, *sp, *ep);
	    return true;
	}
	it++;
    }
    return false;
}

// Find NEAR matches for one group of terms, update highlight map
bool TextSplitPTR::matchGroup(unsigned int grpidx)
{
    const vector<string>& terms = m_hdata.groups[grpidx];
    int window = m_hdata.groups[grpidx].size() + m_hdata.slacks[grpidx];

    LOGDEB1(("TextSplitPTR::matchGroup:d %d: %s\n", window,
	    vecStringToString(terms).c_str()));

    // The position lists we are going to work with. We extract them from the 
    // (string->plist) map
    vector<vector<int>* > plists;
    // A revert plist->term map. This is so that we can find who is who after
    // sorting the plists by length.
    map<vector<int>*, string> plistToTerm;

    // Find the position list for each term in the group. It is
    // possible that this particular group was not actually matched by
    // the search, so that some terms are not found.
    for (vector<string>::const_iterator it = terms.begin(); 
	 it != terms.end(); it++) {
	map<string, vector<int> >::iterator pl = m_plists.find(*it);
	if (pl == m_plists.end()) {
	    LOGDEB1(("TextSplitPTR::matchGroup: [%s] not found in m_plists\n",
		    (*it).c_str()));
	    return false;
	}
	plists.push_back(&(pl->second));
	plistToTerm[&(pl->second)] = *it;
    }
    // I think this can't actually happen, was useful when we used to
    // prune the groups, but doesn't hurt.
    if (plists.size() < 2) {
	LOGDEB1(("TextSplitPTR::matchGroup: no actual groups found\n"));
	return false;
    }
    // Sort the positions lists so that the shorter is first
    std::sort(plists.begin(), plists.end(), VecIntCmpShorter());

    { // Debug
	map<vector<int>*, string>::iterator it;
	it =  plistToTerm.find(plists[0]);
	if (it == plistToTerm.end()) {
	    // SuperWeird
	    LOGERR(("matchGroup: term for first list not found !?!\n"));
	    return false;
	}
	LOGDEB1(("matchGroup: walking the shortest plist. Term [%s], len %d\n",
		it->second.c_str(), plists[0]->size()));
    }

    // Minpos is the highest end of a found match. While looking for
    // further matches, we don't want the search to extend before
    // this, because it does not make sense for highlight regions to
    // overlap
    int minpos = 0;
    // Walk the shortest plist and look for matches
    for (vector<int>::iterator it = plists[0]->begin(); 
	 it != plists[0]->end(); it++) {
	int pos = *it;
	int sta = int(10E9), sto = 0;
	LOGDEB2(("MatchGroup: Testing at pos %d\n", pos));
	if (do_proximity_test(window,plists, 1, pos, pos, &sta, &sto, minpos)) {
	    LOGDEB1(("TextSplitPTR::matchGroup: MATCH termpos [%d,%d]\n", 
		     sta, sto)); 
	    // Maybe extend the window by 1st term position, this was not
	    // done by do_prox..
	    SETMINMAX(pos, sta, sto);
	    minpos = sto+1;
	    // Translate the position window into a byte offset window
	    map<int, pair<int, int> >::iterator i1 =  m_gpostobytes.find(sta);
	    map<int, pair<int, int> >::iterator i2 =  m_gpostobytes.find(sto);
	    if (i1 != m_gpostobytes.end() && i2 != m_gpostobytes.end()) {
		LOGDEB2(("TextSplitPTR::matchGroup: pushing bpos %d %d\n",
			i1->second.first, i2->second.second));
		tboffs.push_back(MatchEntry(i1->second.first, 
					    i2->second.second, grpidx));
	    } else {
		LOGDEB0(("matchGroup: no bpos found for %d or %d\n", sta, sto));
	    }
	} else {
	    LOGDEB1(("matchGroup: no group match found at this position\n"));
	}
    }

    return true;
}

/** Sort integer pairs by increasing first value and decreasing width */
class PairIntCmpFirst {
public:
    bool operator()(const MatchEntry& a, const MatchEntry& b) {
	if (a.offs.first != b.offs.first)
	    return a.offs.first < b.offs.first;
	return a.offs.second > b.offs.second;
    }
};

// Look for matches to PHRASE and NEAR term groups and finalize the
// matched regions list (sort it by increasing start then decreasing
// length)
// Actually, we handle all groups as NEAR (ignore order).
bool TextSplitPTR::matchGroups()
{
    for (unsigned int i = 0; i < m_hdata.groups.size(); i++) {
	if (m_hdata.groups[i].size() <= 1)
	    continue;
	matchGroup(i);
    }

    // Sort regions by increasing start and decreasing width.  
    // The output process will skip overlapping entries.
    std::sort(tboffs.begin(), tboffs.end(), PairIntCmpFirst());
    return true;
}


// Fix result text for display inside the gui text window.
//
// We call overridden functions to output header data, beginnings and ends of
// matches etc.
//
// If the input is text, we output the result in chunks, arranging not
// to cut in the middle of a tag, which would confuse qtextedit. If
// the input is html, the body is always a single output chunk.
bool PlainToRich::plaintorich(const string& in, 
			      list<string>& out, // Output chunk list
			      const HighlightData& hdata,
			      int chunksize)
{
    Chrono chron;
    bool ret = true;
    LOGDEB1(("plaintorichich: in: [%s]\n", in.c_str()));

    m_hdata = &hdata;
    // Compute the positions for the query terms.  We use the text
    // splitter to break the text into words, and compare the words to
    // the search terms,
    TextSplitPTR splitter(hdata);
    // Note: the splitter returns the term locations in byte, not
    // character, offsets.
    splitter.text_to_words(in);
    LOGDEB2(("plaintorich: split done %d mS\n", chron.millis()));
    // Compute the positions for NEAR and PHRASE groups.
    splitter.matchGroups();
    LOGDEB2(("plaintorich: group match done %d mS\n", chron.millis()));

    out.clear();
    out.push_back("");
    list<string>::iterator olit = out.begin();

    // Rich text output
    *olit = header();

    // No term matches. Happens, for example on a snippet selected for
    // a term match when we are actually looking for a group match
    // (the snippet generator does this...).
    if (splitter.tboffs.empty()) {
	LOGDEB1(("plaintorich: no term matches\n"));
	ret = false;
    }

    // Iterator for the list of input term positions. We use it to
    // output highlight tags and to compute term positions in the
    // output text
    vector<MatchEntry>::iterator tPosIt = splitter.tboffs.begin();
    vector<MatchEntry>::iterator tPosEnd = splitter.tboffs.end();

#if 0
    for (vector<pair<int, int> >::const_iterator it = splitter.tboffs.begin();
	 it != splitter.tboffs.end(); it++) {
	LOGDEB2(("plaintorich: region: %d %d\n", it->first, it->second));
    }
#endif

    // Input character iterator
    Utf8Iter chariter(in);

    // State variables used to limit the number of consecutive empty lines,
    // convert all eol to '\n', and preserve some indentation
    int eol = 0;
    int hadcr = 0;
    int inindent = 1;

    // HTML state
    bool intag = false, inparamvalue = false;
    // My tag state
    int inrcltag = 0;

    string::size_type headend = 0;
    if (m_inputhtml) {
	headend = in.find("</head>");
	if (headend == string::npos)
	    headend = in.find("</HEAD>");
	if (headend != string::npos)
	    headend += 7;
    }

    for (string::size_type pos = 0; pos != string::npos; pos = chariter++) {
	// Check from time to time if we need to stop
	if ((pos & 0xfff) == 0) {
	    CancelCheck::instance().checkCancel();
	}

	// If we still have terms positions, check (byte) position. If
	// we are at or after a term match, mark.
	if (tPosIt != tPosEnd) {
	    int ibyteidx = chariter.getBpos();
	    if (ibyteidx == tPosIt->offs.first) {
		if (!intag && ibyteidx >= (int)headend) {
		    *olit += startMatch(tPosIt->grpidx);
		}
                inrcltag = 1;
	    } else if (ibyteidx == tPosIt->offs.second) {
		// Output end of match region tags
		if (!intag && ibyteidx > (int)headend) {
		    *olit += endMatch();
		}
		// Skip all highlight areas that would overlap this one
		int crend = tPosIt->offs.second;
		while (tPosIt != splitter.tboffs.end() && 
		       tPosIt->offs.first < crend)
		    tPosIt++;
                inrcltag = 0;
	    }
	}
        
        unsigned int car = *chariter;

        if (car == '\n') {
            if (!hadcr)
                eol++;
            hadcr = 0;
            continue;
        } else if (car == '\r') {
            hadcr++;
            eol++;
            continue;
        } else if (eol) {
            // Got non eol char in line break state. Do line break;
	    inindent = 1;
            hadcr = 0;
            if (eol > 2)
                eol = 2;
            while (eol) {
		if (!m_inputhtml && m_eolbr)
		    *olit += "<br>";
                *olit += "\n";
                eol--;
            }
            // Maybe end this chunk, begin next. Don't do it on html
            // there is just no way to do it right (qtextedit cant grok
            // chunks cut in the middle of <a></a> for example).
            if (!m_inputhtml && !inrcltag && 
                olit->size() > (unsigned int)chunksize) {
                out.push_back(string(startChunk()));
                olit++;
            }
        }

        switch (car) {
        case '<':
	    inindent = 0;
            if (m_inputhtml) {
                if (!inparamvalue)
                    intag = true;
                chariter.appendchartostring(*olit);    
            } else {
                *olit += "&lt;";
            }
            break;
        case '>':
	    inindent = 0;
            if (m_inputhtml) {
                if (!inparamvalue)
                    intag = false;
            }
            chariter.appendchartostring(*olit);    
            break;
        case '&':
	    inindent = 0;
            if (m_inputhtml) {
                chariter.appendchartostring(*olit);
            } else {
                *olit += "&amp;";
            }
            break;
        case '"':
	    inindent = 0;
            if (m_inputhtml && intag) {
                inparamvalue = !inparamvalue;
            }
            chariter.appendchartostring(*olit);
            break;

	case ' ': 
	    if (m_eolbr && inindent) {
		*olit += "&nbsp;";
	    } else {
		chariter.appendchartostring(*olit);
	    }
	    break;
	case '\t': 
	    if (m_eolbr && inindent) {
		*olit += "&nbsp;&nbsp;&nbsp;&nbsp;";
	    } else {
		chariter.appendchartostring(*olit);
	    }
	    break;

        default:
	    inindent = 0;
            chariter.appendchartostring(*olit);
        }

    } // End chariter loop

#if 0
    {
	FILE *fp = fopen("/tmp/debugplaintorich", "a");
	fprintf(fp, "BEGINOFPLAINTORICHOUTPUT\n");
	for (list<string>::iterator it = out.begin();
	     it != out.end(); it++) {
	    fprintf(fp, "BEGINOFPLAINTORICHCHUNK\n");
	    fprintf(fp, "%s", it->c_str());
	    fprintf(fp, "ENDOFPLAINTORICHCHUNK\n");
	}
	fprintf(fp, "ENDOFPLAINTORICHOUTPUT\n");
	fclose(fp);
    }
#endif
    LOGDEB2(("plaintorich: done %d mS\n", chron.millis()));
    return ret;
}
