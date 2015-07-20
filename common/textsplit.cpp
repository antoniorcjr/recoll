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
#ifndef TEST_TEXTSPLIT
#include "autoconfig.h"

#include <assert.h>
#include <stdlib.h>

#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>
#include "unordered_defs.h"
using namespace std;

#include "textsplit.h"
#include "debuglog.h"
//#define UTF8ITER_CHECK
#include "utf8iter.h"
#include "uproplist.h"


/**
 * Splitting a text into words. The code in this file works with utf-8
 * in a semi-clean way (see uproplist.h). Ascii still gets special
 * treatment in the sense that many special characters can only be
 * ascii (e.g. @, _,...). However, this compromise works quite well
 * while being much more light-weight than a full-blown Unicode
 * approach (ICU...)
 */

// Ascii character classes: we have three main groups, and then some chars
// are their own class because they want special handling.
// 
// We have an array with 256 slots where we keep the character types. 
// The array could be fully static, but we use a small function to fill it 
// once.
// The array is actually a remnant of the original version which did no utf8.
// Only the lower 127 slots are  now used, but keep it at 256
// because it makes some tests in the code simpler.
const unsigned int charclasses_size = 256;
enum CharClass {LETTER=256, SPACE=257, DIGIT=258, WILD=259, 
                A_ULETTER=260, A_LLETTER=261, SKIP=262};
static int charclasses[charclasses_size];

// Non-ascii UTF-8 characters are handled with sets holding all
// characters with interesting properties. This is far from full-blown
// management of Unicode properties, but seems to do the job well
// enough in most common cases
static vector<unsigned int> vpuncblocks;
static STD_UNORDERED_SET<unsigned int> spunc;
static STD_UNORDERED_SET<unsigned int> visiblewhite;
static STD_UNORDERED_SET<unsigned int> sskip;

class CharClassInit {
public:
    CharClassInit() 
    {
	unsigned int i;

	// Set default value for all: SPACE
	for (i = 0 ; i < 256 ; i ++)
	    charclasses[i] = SPACE;

	char digits[] = "0123456789";
	for (i = 0; i  < strlen(digits); i++)
	    charclasses[int(digits[i])] = DIGIT;

	char upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	for (i = 0; i  < strlen(upper); i++)
	    charclasses[int(upper[i])] = A_ULETTER;

	char lower[] = "abcdefghijklmnopqrstuvwxyz";
	for (i = 0; i  < strlen(lower); i++)
	    charclasses[int(lower[i])] = A_LLETTER;

	char wild[] = "*?[]";
	for (i = 0; i  < strlen(wild); i++)
	    charclasses[int(wild[i])] = WILD;

        // Characters with special treatment:
        //
        // The first ones are mostly span-constructing "glue"
        // characters, for example those typically allowing us to
        // search for an email address as a whole (bob@isp.org instead
        // of as a phrase "bob isp org"
        //
        // The case of the minus sign is a complicated one. It went
        // from glue to non-glue to glue along Recoll versions. 
        // See minus-hyphen-dash.txt in doc/notes
	char special[] = ".@+-#'_\n\r\f";
	for (i = 0; i  < strlen(special); i++)
	    charclasses[int(special[i])] = special[i];

	for (i = 0; i < sizeof(unipunc) / sizeof(int); i++) {
	    spunc.insert(unipunc[i]);
	}
	spunc.insert((unsigned int)-1);

	for (i = 0; i < sizeof(unipuncblocks) / sizeof(int); i++) {
	    vpuncblocks.push_back(unipuncblocks[i]);
	}
	assert((vpuncblocks.size() % 2) == 0);

	for (i = 0; i < sizeof(avsbwht) / sizeof(int); i++) {
	    visiblewhite.insert(avsbwht[i]);
	}
	for (i = 0; i < sizeof(uniskip) / sizeof(int); i++) {
	    sskip.insert(uniskip[i]);
	}
    }
};
static const CharClassInit charClassInitInstance;

static inline int whatcc(unsigned int c)
{
    if (c <= 127) {
	return charclasses[c]; 
    } else {
        if (c == 0x2010) {
            // Special treatment for hyphen: handle as ascii minus. See
            // doc/notes/minus-hyphen-dash.txt
            return 0x2010;
        } else if (sskip.find(c) != sskip.end()) {
	    return SKIP;
	} else if (spunc.find(c) != spunc.end()) {
	    return SPACE;
	} else {
	    vector<unsigned int>::iterator it = 
		lower_bound(vpuncblocks.begin(), vpuncblocks.end(), c);
	    if (c == *it)
		return SPACE;
	    if ((it - vpuncblocks.begin()) % 2 == 1) {
		return SPACE;
	    } else {
		return LETTER;
	    }
	} 
    }
}

// testing whatcc...
#if 0
  unsigned int testvalues[] = {'a', '0', 0x80, 0xbf, 0xc0, 0x05c3, 0x1000, 
			       0x2000, 0x2001, 0x206e, 0x206f, 0x20d0, 0x2399, 
			       0x2400, 0x2401, 0x243f, 0x2440, 0xff65};
  int ntest = sizeof(testvalues) / sizeof(int);
  for (int i = 0; i < ntest; i++) {
      int ret = whatcc(testvalues[i]);
      printf("Tested value 0x%x, returned value %d %s\n",
	     testvalues[i], ret, ret == LETTER ? "LETTER" : 
	     ret == SPACE ? "SPACE" : "OTHER");
  }
#endif

// CJK Unicode character detection:
//
// 2E80..2EFF; CJK Radicals Supplement
// 3000..303F; CJK Symbols and Punctuation
// 3040..309F; Hiragana
// 30A0..30FF; Katakana
// 3100..312F; Bopomofo
// 3130..318F; Hangul Compatibility Jamo
// 3190..319F; Kanbun
// 31A0..31BF; Bopomofo Extended
// 31C0..31EF; CJK Strokes
// 31F0..31FF; Katakana Phonetic Extensions
// 3200..32FF; Enclosed CJK Letters and Months
// 3300..33FF; CJK Compatibility
// 3400..4DBF; CJK Unified Ideographs Extension A
// 4DC0..4DFF; Yijing Hexagram Symbols
// 4E00..9FFF; CJK Unified Ideographs
// A700..A71F; Modifier Tone Letters
// AC00..D7AF; Hangul Syllables
// F900..FAFF; CJK Compatibility Ideographs
// FE30..FE4F; CJK Compatibility Forms
// FF00..FFEF; Halfwidth and Fullwidth Forms
// 20000..2A6DF; CJK Unified Ideographs Extension B
// 2F800..2FA1F; CJK Compatibility Ideographs Supplement
// Note: the p > 127 test is not necessary, but optimizes away the ascii case
#define UNICODE_IS_CJK(p)						\
    ((p) > 127 &&							\
     (((p) >= 0x2E80 && (p) <= 0x2EFF) ||				\
      ((p) >= 0x3000 && (p) <= 0x9FFF) ||				\
      ((p) >= 0xA700 && (p) <= 0xA71F) ||				\
      ((p) >= 0xAC00 && (p) <= 0xD7AF) ||				\
      ((p) >= 0xF900 && (p) <= 0xFAFF) ||				\
      ((p) >= 0xFE30 && (p) <= 0xFE4F) ||				\
      ((p) >= 0xFF00 && (p) <= 0xFFEF) ||				\
      ((p) >= 0x20000 && (p) <= 0x2A6DF) ||				\
      ((p) >= 0x2F800 && (p) <= 0x2FA1F)))

bool TextSplit::isCJK(int c)
{
    return UNICODE_IS_CJK(c);
}

bool          TextSplit::o_processCJK = true;
unsigned int  TextSplit::o_CJKNgramLen = 2;
bool          TextSplit::o_noNumbers = false;

// Final term checkpoint: do some checking (the kind which is simpler
// to do here than in the main loop), then send term to our client.
inline bool TextSplit::emitterm(bool isspan, string &w, int pos, 
				int btstart, int btend)
{
    LOGDEB2(("TextSplit::emitterm: [%s] pos %d\n", w.c_str(), pos));

    unsigned int l = w.length();

#ifdef TEXTSPLIT_STATS
    // Update word length statistics. Do this before we filter out
    // long words because stats are used to detect bad text
    if (!isspan || m_wordLen == m_span.length())
	m_stats.newsamp(m_wordChars);
#endif

    if (l > 0 && l < (unsigned)m_maxWordLength) {
	// 1 byte word: we index single ascii letters and digits, but
	// nothing else. We might want to turn this into a test for a
	// single utf8 character instead ?
	if (l == 1) {
	    unsigned int c = ((unsigned int)w[0]) & 0xff;
	    if (charclasses[c] != A_ULETTER && charclasses[c] != A_LLETTER && 
                charclasses[c] != DIGIT &&
		(!(m_flags & TXTS_KEEPWILD) || charclasses[c] != WILD)
		) {
		//cerr << "ERASING single letter term " << c << endl;
		return true;
	    }
	}
	if (pos != m_prevpos || l != m_prevlen) {
	    bool ret = takeword(w, pos, btstart, btend);
	    m_prevpos = pos;
	    m_prevlen = w.length();
	    return ret;
	}
	LOGDEB2(("TextSplit::emitterm:dup: [%s] pos %d\n", w.c_str(), pos));
    }
    return true;
}

// Check for an acronym/abbreviation ie I.B.M. This only works with
// ascii (no non-ascii utf-8 acronym are possible)
bool TextSplit::span_is_acronym(string *acronym)
{
    bool acron = false;

    if (m_wordLen != m_span.length() && 
        m_span.length() > 2 && m_span.length() <= 20) {
        acron = true;
        // Check odd chars are '.'
        for (unsigned int i = 1 ; i < m_span.length(); i += 2) {
            if (m_span[i] != '.') {
                acron = false;
                break;
            }
        }
        if (acron) {
            // Check that even chars are letters
            for (unsigned int i = 0 ; i < m_span.length(); i += 2) {
                int c = m_span[i];
                if (!((c >= 'a' && c <= 'z')||(c >= 'A' && c <= 'Z'))) {
                    acron = false;
                    break;
                }
            }
        }
    }
    if (acron) {
        for (unsigned int i = 0; i < m_span.length(); i += 2) {
            *acronym += m_span[i];
        }
    }
    return acron;
}


        // Generate terms from span. Have to take into account the
        // flags: ONLYSPANS, NOSPANS, noNumbers
bool TextSplit::words_from_span(int bp)
{
#if 0
    cerr << "Span: [" << m_span << "] " << " w_i_s size: " << 
        m_words_in_span.size() <<  " : ";
    for (unsigned int i = 0; i < m_words_in_span.size(); i++) {
        cerr << " [" << m_words_in_span[i].first << " " <<
            m_words_in_span[i].second << "] ";
                
    }
    cerr << endl;
#endif
    unsigned int spanwords = m_words_in_span.size();
    int pos = m_spanpos;
    // Byte position of the span start
    int spboffs = bp - m_span.size();

    for (unsigned int i = 0; 
         i < ((m_flags&TXTS_ONLYSPANS) ? 1 : spanwords); 
         i++, pos++) {

        int deb = m_words_in_span[i].first;

        for (unsigned int j = ((m_flags&TXTS_ONLYSPANS) ? spanwords-1 : i);
             j < ((m_flags&TXTS_NOSPANS) ? i+1 : spanwords);
             j++) {

            int fin = m_words_in_span[j].second;
            //cerr << "i " << i << " j " << j << " deb " << deb << 
            // " fin " << fin << endl;
            if (fin - deb > int(m_span.size()))
                break;
            string word(m_span.substr(deb, fin-deb));
            if (!emitterm(j != i+1, word, pos, spboffs+deb, spboffs+fin))
                return false;
        }
    }
    return true;
}

/**
 * A method called at word boundaries (different places in
 * text_to_words()), to adjust the current state of the parser, and
 * possibly generate term(s). While inside a span (words linked by
 * glue characters), we just keep track of the word boundaries. Once
 * actual white-space is reached, we get called with spanerase set to
 * true, and we process the span, calling the emitterm() routine for
 * each generated term.
 * 
 * The object flags can modify our behaviour, deciding if we only emit
 * single words (bill, recoll, org), only spans (bill@recoll.org), or
 * words and spans (bill@recoll.org, recoll.org, jf, recoll...)
 * 
 * @return true if ok, false for error. Splitting should stop in this case.
 * @param spanerase Set if the current span is at its end. Process it.
 * @param bp        The current BYTE position in the stream
 */
inline bool TextSplit::doemit(bool spanerase, int bp)
{
    LOGDEB2(("TextSplit::doemit: sper %d bp %d spp %d spanwords %u wS %d wL %d "
            "inn %d span [%s]\n",
            spanerase, bp, m_spanpos, m_words_in_span.size(), 
            m_wordStart, m_wordLen, m_inNumber, m_span.c_str()));

    if (m_wordLen) {
        // We have a current word. Remember it

        // Limit max span word count
        if (m_words_in_span.size() >= 6) {
            spanerase = true;
        } 

        m_words_in_span.push_back(pair<int,int>(m_wordStart, 
                                                m_wordStart + m_wordLen));
	m_wordpos++;
	m_wordLen = m_wordChars = 0;
    }

    if (spanerase) {
        // We encountered a span-terminating character. Produce terms.

        string acronym;
        if (span_is_acronym(&acronym)) {
            if (!emitterm(false, acronym, m_spanpos, bp - m_span.length(), bp))
                return false;
        }

	// Maybe trim at end. These are chars that we might keep
	// inside a span, but not at the end.
	while (m_span.length() > 0) {
	    switch (*(m_span.rbegin())) {
	    case '.':
	    case '-':
	    case ',':
	    case '@':
	    case '_':
	    case '\'':
		m_span.resize(m_span.length()-1);
                if (m_words_in_span.size() &&
                    m_words_in_span.back().second > m_span.size())
                    m_words_in_span.back().second = m_span.size();
		if (--bp < 0) 
		    bp = 0;
		break;
	    default:
		goto breaktrimloop;
	    }
	}
    breaktrimloop:

        if (!words_from_span(bp)) {
            return false;
        }
	discardspan();

    } else {
    
	m_wordStart = m_span.length();

    }

    return true;
}

void TextSplit::discardspan()
{
    m_words_in_span.clear();
    m_span.erase();
    m_spanpos = m_wordpos;
    m_wordStart = 0;
    m_wordLen = m_wordChars = 0;
}

static inline bool isalphanum(int what, unsigned int flgs)
{
    return what == A_LLETTER || what == A_ULETTER ||
	what == DIGIT || what == LETTER ||
	((flgs & TextSplit::TXTS_KEEPWILD) && what == WILD);
}
static inline bool isdigit(int what, unsigned int flgs)
{
    return what == DIGIT || ((flgs & TextSplit::TXTS_KEEPWILD) && what == WILD);
}

#ifdef TEXTSPLIT_STATS
#define STATS_INC_WORDCHARS ++m_wordChars
#else
#define STATS_INC_WORDCHARS
#endif

/** 
 * Splitting a text into terms to be indexed.
 * We basically emit a word every time we see a separator, but some chars are
 * handled specially so that special cases, ie, c++ and jfd@recoll.com etc, 
 * are handled properly,
 */
bool TextSplit::text_to_words(const string &in)
{
    LOGDEB1(("TextSplit::text_to_words: docjk %d (%d) %s%s%s [%s]\n", 
	     o_processCJK, o_CJKNgramLen,
	     m_flags & TXTS_NOSPANS ? " nospans" : "",
	     m_flags & TXTS_ONLYSPANS ? " onlyspans" : "",
	     m_flags & TXTS_KEEPWILD ? " keepwild" : "",
	     in.substr(0,50).c_str()));

    if (in.empty())
	return true;

    m_span.erase();
    m_inNumber = false;
    m_wordStart = m_wordLen = m_wordChars = m_prevpos = m_prevlen = m_wordpos 
	= m_spanpos = 0;
    bool pagepending = false;
    bool softhyphenpending = false;

    // Running count of non-alphanum chars. Reset when we see one;
    int nonalnumcnt = 0;

    Utf8Iter it(in);

    for (; !it.eof(); it++) {
	unsigned int c = *it;
	nonalnumcnt++;

	if (c == (unsigned int)-1) {
	    LOGERR(("Textsplit: error occured while scanning UTF-8 string\n"));
	    return false;
	}

	if (o_processCJK && UNICODE_IS_CJK(c)) {
	    // CJK character hit. 
	    // Do like at EOF with the current non-cjk data.
	    if (m_wordLen || m_span.length()) {
		if (!doemit(true, it.getBpos()))
		    return false;
	    }

	    // Hand off situation to the cjk routine.
	    if (!cjk_to_words(&it, &c)) {
		LOGERR(("Textsplit: scan error in cjk handler\n"));
		return false;
	    }

	    // Check for eof, else c contains the first non-cjk
	    // character after the cjk sequence, just go on.
	    if (it.eof())
		break;
	}

	int cc = whatcc(c);

	switch (cc) {
	case SKIP:
	    // Special-case soft-hyphen. To work, this depends on the
	    // fact that only SKIP calls "continue" inside the
	    // switch. All the others will do the softhyphenpending
	    // reset after the switch
	    if (c == 0xad) {
		softhyphenpending = true;
	    } else {
		softhyphenpending = false;
	    }
	    // Skips the softhyphenpending reset
	    continue;

	case DIGIT:
	    nonalnumcnt = 0;
	    if (m_wordLen == 0)
		m_inNumber = true;
	    m_wordLen += it.appendchartostring(m_span);
	    STATS_INC_WORDCHARS;
	    break;

	case SPACE:
	    nonalnumcnt = 0;
	SPACE:
	    if (m_wordLen || m_span.length()) {
		if (!doemit(true, it.getBpos()))
		    return false;
		m_inNumber = false;
	    }
	    if (pagepending) {
		pagepending = false;
		newpage(m_wordpos);
	    }
	    break;

	case WILD:
	    if (m_flags & TXTS_KEEPWILD)
		goto NORMALCHAR;
	    else
		goto SPACE;
	    break;

	case '-':
	case '+':
	    if (m_wordLen == 0) {
		// + or - don't start a term except if this looks like
		// it's going to be to be a number
		if (isdigit(whatcc(it[it.getCpos()+1]), m_flags)) {
		    // -10
		    m_inNumber = true;
		    m_wordLen += it.appendchartostring(m_span);
		    STATS_INC_WORDCHARS;
                    break;
		} 
	    } else if (m_inNumber) {
                if ((m_span[m_span.length() - 1] == 'e' ||
				      m_span[m_span.length() - 1] == 'E')) {
                    if (isdigit(whatcc(it[it.getCpos()+1]), m_flags)) {
                        m_wordLen += it.appendchartostring(m_span);
                        STATS_INC_WORDCHARS;
                        break;
                    }
                }
	    } else {
                if (cc == '+') {
                    int nextc = it[it.getCpos()+1];
                    if (nextc == '+' || nextc == -1 || visiblewhite.find(nextc) 
                        != visiblewhite.end()) {
                        // someword++[+...] !
                        m_wordLen += it.appendchartostring(m_span);
                        STATS_INC_WORDCHARS;
                        break;
                    }
                } else {
                    // Treat '-' inside span as glue char
                    if (!doemit(false, it.getBpos()))
                        return false;
                    m_inNumber = false;
                    m_wordStart += it.appendchartostring(m_span);
                    break;
                }
	    }
            goto SPACE;
	    break;

	case 0x2010:
            // Hyphen is replaced with ascii minus
	    if (m_wordLen != 0) {
                // Treat '-' inside span as glue char
                if (!doemit(false, it.getBpos()))
                    return false;
                m_inNumber = false;
                m_span += '-';
                m_wordStart++;
                break;
            }
            goto SPACE;

	case '.':
	{
	    // Need a little lookahead here. At worse this gets the end null
	    int nextc = it[it.getCpos()+1];
	    int nextwhat = whatcc(nextc);
	    if (m_inNumber) {
		if (!isdigit(nextwhat, m_flags))
		    goto SPACE;
                m_wordLen += it.appendchartostring(m_span);
                STATS_INC_WORDCHARS;
		break;
	    } else {
		// Found '.' while not in number

		// Only letters and digits make sense after
		if (!isalphanum(nextwhat, m_flags))
		    goto SPACE;

		// Keep an initial '.' for catching .net, and .34 (aka
		// 0.34) but this adds quite a few spurious terms !
                if (m_span.length() == 0) {
                    // Check for number like .1
                    if (isdigit(nextwhat, m_flags)) {
                        m_inNumber = true;
                    }
                    m_wordLen += it.appendchartostring(m_span);
                    STATS_INC_WORDCHARS;
                    break;
                }

                // '.' between words: span glue
                if (m_wordLen) {
                    if (!doemit(false, it.getBpos()))
                        return false;
                    m_wordStart += it.appendchartostring(m_span);
                }
	    }
	}
        break;

	case '@':
	case '_':
	case '\'':
	    // If in word, potential span: o'brien, jf@dockes.org,
	    // else just ignore
	    if (m_wordLen) {
		if (!doemit(false, it.getBpos()))
		    return false;
		m_inNumber = false;
                m_wordStart += it.appendchartostring(m_span);
	    }
	    break;

	case '#': 
	    // Keep it only at the beginning of a word (hashtag), 
            if (m_wordLen == 0) {
                m_wordLen += it.appendchartostring(m_span);
                STATS_INC_WORDCHARS;
                break;
            }
            // or at the end (special case for c# ...)
	    if (m_wordLen > 0) {
		int w = whatcc(it[it.getCpos()+1]);
		if (w == SPACE || w == '\n' || w == '\r') {
		    m_wordLen += it.appendchartostring(m_span);
		    STATS_INC_WORDCHARS;
		    break;
		}
	    }
	    goto SPACE;
	    break;

	case '\n':
	case '\r':
	    if (m_span.length() && *m_span.rbegin() == '-') {
                // if '-' is the last char before end of line, we
                // strip it.  We have no way to know if this is added
                // because of the line split or if it was part of an
                // actual compound word (would need a dictionary to
                // check).  As soft-hyphen *should* be used if the '-'
                // is not part of the text, it is better to properly
                // process a real compound word, and produce wrong
                // output from wrong text. The word-emitting routine
                // will strip the trailing '-'.
                goto SPACE;
            } else if (softhyphenpending) {
		// Don't reset soft-hyphen
		continue;
	    } else {
		// Normal case: EOL is white space
		goto SPACE;
	    }
	    break;

	case '\f':
	    pagepending = true;
	    goto SPACE;
	    break;

#ifdef RCL_SPLIT_CAMELCASE
            // Camelcase handling. 
            // If we get uppercase ascii after lowercase ascii, emit word.
            // This emits "camel" when hitting the 'C' of camelCase
            // Not enabled by defaults as this makes phrase searches quite 
            // confusing. 
            // ie "MySQL manual" is matched by "MySQL manual" and 
            // "my sql manual" but not "mysql manual"
	case A_ULETTER:
	    if (m_span.length() && 
                charclasses[(unsigned char)m_span[m_span.length() - 1]] == 
                A_LLETTER) {
                if (m_wordLen) {
                    if (!doemit(false, it.getBpos()))
                        return false;
                }
            }
            goto NORMALCHAR;

            // CamelCase handling.
            // If we get lowercase after uppercase and the current
            // word length is bigger than one, it means we had a
            // string of several upper-case letters:  an
            // acronym (readHTML) or a single letter article (ALittleHelp).
            // Emit the uppercase word before proceeding
        case A_LLETTER:
	    if (m_span.length() && 
                charclasses[(unsigned char)m_span[m_span.length() - 1]] == 
                A_ULETTER && m_wordLen > 1) {
                // Multiple upper-case letters. Single letter word
                // or acronym which we want to emit now
                m_wordLen--;
                if (!doemit(false, it.getBpos()))
                    return false;
                // m_wordstart could be 0 here if the span was reset
                // for excessive length
                if (m_wordStart)
                    m_wordStart--;
                m_wordLen++;
            }
            goto NORMALCHAR;
#endif /* CAMELCASE */

	default:
	NORMALCHAR:
	    nonalnumcnt = 0;
            if (m_inNumber && c != 'e' && c != 'E') {
                m_inNumber = false;
            }
	    m_wordLen += it.appendchartostring(m_span);
	    STATS_INC_WORDCHARS;
	    break;
	}
	softhyphenpending = false;
    }
    if (m_wordLen || m_span.length()) {
	if (!doemit(true, it.getBpos()))
	    return false;
    }
    return true;
}

// Using an utf8iter pointer just to avoid needing its definition in
// textsplit.h
//
// We output ngrams for exemple for char input a b c and ngramlen== 2, 
// we generate: a ab b bc c as words
//
// This is very different from the normal behaviour, so we don't use
// the doemit() and emitterm() routines
//
// The routine is sort of a mess and goes to show that we'd probably
// be better off converting the whole buffer to utf32 on entry...
bool TextSplit::cjk_to_words(Utf8Iter *itp, unsigned int *cp)
{
    LOGDEB1(("cjk_to_words: m_wordpos %d\n", m_wordpos));
    Utf8Iter &it = *itp;

    // We use an offset buffer to remember the starts of the utf-8
    // characters which we still need to use.
    assert(o_CJKNgramLen < o_CJKMaxNgramLen);
    unsigned int boffs[o_CJKMaxNgramLen+1];

    // Current number of valid offsets;
    unsigned int nchars = 0;
    unsigned int c = 0;
    for (; !it.eof(); it++) {
	c = *it;
	if (!UNICODE_IS_CJK(c)) {
	    // Return to normal handler
	    break;
	}
	if (whatcc(c) == SPACE) {
	    // Flush the ngram buffer and go on
	    nchars = 0;
	    continue;
	}
	if (nchars == o_CJKNgramLen) {
	    // Offset buffer full, shift it. Might be more efficient
	    // to have a circular one, but things are complicated
	    // enough already...
	    for (unsigned int i = 0; i < nchars-1; i++) {
		boffs[i] = boffs[i+1];
	    }
	}  else {
	    nchars++;
	}

	// Take note of byte offset for this character.
	boffs[nchars-1] = it.getBpos();

	// Output all new ngrams: they begin at each existing position
	// and end after the new character. onlyspans->only output
	// maximum words, nospans=> single chars
	if (!(m_flags & TXTS_ONLYSPANS) || nchars == o_CJKNgramLen) {
	    unsigned int btend = it.getBpos() + it.getBlen();
	    unsigned int loopbeg = (m_flags & TXTS_NOSPANS) ? nchars-1 : 0;
	    unsigned int loopend = (m_flags & TXTS_ONLYSPANS) ? 1 : nchars;
	    for (unsigned int i = loopbeg; i < loopend; i++) {
		if (!takeword(it.buffer().substr(boffs[i], 
						       btend-boffs[i]),
				m_wordpos - (nchars-i-1), boffs[i], btend)) {
		    return false;
		}
	    }

	    if ((m_flags & TXTS_ONLYSPANS)) {
		// Only spans: don't overlap: flush buffer
		nchars = 0;
	    }
	}
	// Increase word position by one, other words are at an
	// existing position. This could be subject to discussion...
	m_wordpos++;
    }

    // If onlyspans is set, there may be things to flush in the buffer
    // first
    if ((m_flags & TXTS_ONLYSPANS) && nchars > 0 && nchars != o_CJKNgramLen)  {
	unsigned int btend = it.getBpos(); // Current char is out
	if (!takeword(it.buffer().substr(boffs[0], btend-boffs[0]),
			    m_wordpos - nchars,
			    boffs[0], btend)) {
	    return false;
	}
    }

    m_span.erase();
    m_inNumber = false;
    m_wordStart = m_wordLen = m_wordChars = m_prevpos = m_prevlen = 0;
    m_spanpos = m_wordpos;
    *cp = c;
    return true;
}

// Specialization for countWords 
class TextSplitCW : public TextSplit {
 public:
    int wcnt;
    TextSplitCW(Flags flags) : TextSplit(flags), wcnt(0) {}
    bool takeword(const string &, int, int, int) {
	wcnt++;
	return true;
    }
};

int TextSplit::countWords(const string& s, TextSplit::Flags flgs)
{
    TextSplitCW splitter(flgs);
    splitter.text_to_words(s);
    return splitter.wcnt;
}

bool TextSplit::hasVisibleWhite(const string &in)
{
    Utf8Iter it(in);
    for (; !it.eof(); it++) {
	unsigned int c = (unsigned char)*it;
	LOGDEB3(("TextSplit::hasVisibleWhite: testing 0x%04x\n", c));
	if (c == (unsigned int)-1) {
	    LOGERR(("hasVisibleWhite: error while scanning UTF-8 string\n"));
	    return false;
	}
	if (visiblewhite.find(c) != visiblewhite.end())
	    return true;
    }
    return false;
}

template <class T> bool u8stringToStrings(const string &s, T &tokens)
{
    Utf8Iter it(s);

    string current;
    tokens.clear();
    enum states {SPACE, TOKEN, INQUOTE, ESCAPE};
    states state = SPACE;
    for (; !it.eof(); it++) {
	unsigned int c = *it;
	if (visiblewhite.find(c) != visiblewhite.end()) 
	    c = ' ';
	LOGDEB3(("TextSplit::stringToStrings: 0x%04x\n", c));
	if (c == (unsigned int)-1) {
	    LOGERR(("TextSplit::stringToStrings: error while "
		    "scanning UTF-8 string\n"));
	    return false;
	}

	switch (c) {
	    case '"': 
	    switch(state) {
	    case SPACE: state = INQUOTE; continue;
	    case TOKEN: goto push_char;
	    case ESCAPE: state = INQUOTE; goto push_char;
	    case INQUOTE: tokens.push_back(current);current.clear();
		state = SPACE; continue;
	    }
	    break;
	    case '\\': 
	    switch(state) {
	    case SPACE: 
	    case TOKEN: state=TOKEN; goto push_char;
	    case INQUOTE: state = ESCAPE; continue;
	    case ESCAPE: state = INQUOTE; goto push_char;
	    }
	    break;

	    case ' ': 
	    case '\t': 
	    case '\n': 
	    case '\r': 
	    switch(state) {
	      case SPACE: continue;
	      case TOKEN: tokens.push_back(current); current.clear();
		state = SPACE; continue; 
	    case INQUOTE: 
	    case ESCAPE: goto push_char;
	    }
	    break;

	    default:
	    switch(state) {
	      case ESCAPE: state = INQUOTE; break;
	      case SPACE:  state = TOKEN;  break;
	      case TOKEN: 
	      case INQUOTE: break;
	    }
	push_char:
	    it.appendchartostring(current);
	}
    }

    // End of string. Process residue, and possible error (unfinished quote)
    switch(state) {
    case SPACE: break;
    case TOKEN: tokens.push_back(current); break;
    case INQUOTE: 
    case ESCAPE: return false;
    }
    return true;
}

bool TextSplit::stringToStrings(const string &s, vector<string> &tokens)
{
    return u8stringToStrings<vector<string> >(s, tokens);
}

#else  // TEST driver ->

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>

#include <iostream>

#include "textsplit.h"
#include "readfile.h"
#include "debuglog.h"
#include "transcode.h"
#include "unacpp.h"
#include "termproc.h"

using namespace std;

class myTermProc : public Rcl::TermProc {
    int first;
    bool nooutput;
public:
    myTermProc() : TermProc(0), first(1), nooutput(false) {}
    void setNoOut(bool val) {nooutput = val;}
    virtual bool takeword(const string &term, int pos, int bs, int be)
    {
	if (nooutput)
	    return true;
	FILE *fp = stdout;
	if (first) {
	    fprintf(fp, "%3s %-20s %4s %4s\n", "pos", "Term", "bs", "be");
	    first = 0;
	}
	fprintf(fp, "%3d %-20s %4d %4d\n", pos, term.c_str(), bs, be);
	return true;
    }
};

#define OPT_s	  0x1 
#define OPT_w	  0x2
#define OPT_q	  0x4
#define OPT_c     0x8
#define OPT_k     0x10
#define OPT_C     0x20
#define OPT_n     0x40
#define OPT_S     0x80
#define OPT_u     0x100
#define OPT_p     0x200

bool dosplit(const string& data, TextSplit::Flags flags, int op_flags)
{
    myTermProc printproc;

    Rcl::TermProc *nxt = &printproc;

//    Rcl::TermProcCommongrams commonproc(nxt, stoplist);
//    if (op_flags & OPT_S)
//        nxt = &commonproc;

    Rcl::TermProcPrep preproc(nxt);
    if (op_flags & OPT_u) 
        nxt = &preproc;

    Rcl::TextSplitP splitter(nxt, flags);

    if (op_flags & OPT_q)
        printproc.setNoOut(true);

    splitter.text_to_words(data);

#ifdef TEXTSPLIT_STATS
	TextSplit::Stats::Values v = splitter.getStats();
	cout << "Average length: " 
	     <<  v.avglen
	     << " Standard deviation: " 
	     << v.sigma
	     << " Coef of variation "
	     << v.sigma / v.avglen
	     << endl;
#endif
    return true;
}

static const char *teststrings[] = {
    "Un bout de texte \nnormal. 2eme phrase.3eme;quatrieme.\n",
    "\"Jean-Francois Dockes\" <jfd@okyz.com>\n",
    "n@d @net .net net@ t@v@c c# c++ o'brien 'o'brien'",
    "_network_ some_span",
    "data123\n",
    "134 +134 -14 0.1 .1 2. -1.5 +1.5 1,2 1.54e10 1,2e30 .1e10 1.e-8\n",
    "@^#$(#$(*)\n",
    "192.168.4.1 one\n\rtwo\r",
    "[olala][ululu]  (valeur) (23)\n",
    "utf-8 ucs-4© \\nodef\n",
    "A b C 2 . +",
    "','this\n",
    " ,able,test-domain",
    " -wl,--export-dynamic",
    " ~/.xsession-errors",
    "this_very_long_span_this_very_long_span_this_very_long_span",
    "soft\xc2\xadhyphen",
    "soft\xc2\xad\nhyphen",
    "soft\xc2\xad\n\rhyphen",
    "real\xe2\x80\x90hyphen",
    "real\xe2\x80\x90\nhyphen",
    "hyphen-\nminus",
};
const int teststrings_cnt = sizeof(teststrings)/sizeof(char *);

static string teststring1 = " nouvel-an ";

static string thisprog;

static string usage =
    " textsplit [opts] [filename]\n"
    "   -q : no output\n"
    "   -s :  only spans\n"
    "   -w :  only words\n"
    "   -n :  no numbers\n"
    "   -k :  preserve wildcards (?*)\n"
    "   -c : just count words\n"
    "   -u : use unac\n"
    "   -C [charset] : input charset\n"
    "   -S [stopfile] : stopfile to use for commongrams\n"
    " if filename is 'stdin', will read stdin for data (end with ^D)\n\n"
    " textplit -p somephrase : display results from stringToStrings()\n"
    "  \n"
    ;

static void
Usage(void)
{
    cerr << thisprog  << ": usage:\n" << usage;
    exit(1);
}

static int        op_flags;

int main(int argc, char **argv)
{
    string charset, stopfile;

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'c':	op_flags |= OPT_c; break;
            case 'C':	op_flags |= OPT_C; if (argc < 2)  Usage();
                charset = *(++argv); argc--; 
                goto b1;
	    case 'k':	op_flags |= OPT_k; break;
	    case 'n':	op_flags |= OPT_n; break;
	    case 'p':	op_flags |= OPT_p; break;
	    case 'q':	op_flags |= OPT_q; break;
	    case 's':	op_flags |= OPT_s; break;
            case 'S':	op_flags |= OPT_S; if (argc < 2)  Usage();
                stopfile = *(++argv); argc--; 
                goto b1;
	    case 'u':	op_flags |= OPT_u; break;
	    case 'w':	op_flags |= OPT_w; break;
	    default: Usage();	break;
	    }
    b1: argc--; argv++;
    }
    DebugLog::getdbl()->setloglevel(DEBDEB1);
    DebugLog::setfilename("stderr");

    TextSplit::Flags flags = TextSplit::TXTS_NONE;

    if (op_flags&OPT_s)
	flags = TextSplit::TXTS_ONLYSPANS;
    else if (op_flags&OPT_w)
	flags = TextSplit::TXTS_NOSPANS;
    if (op_flags & OPT_k) 
	flags = (TextSplit::Flags)(flags | TextSplit::TXTS_KEEPWILD); 
    if (op_flags & OPT_n)
	TextSplit::noNumbers();

    Rcl::StopList stoplist;
    if (op_flags & OPT_S) {
	if (!stoplist.setFile(stopfile)) {
	    cerr << "Can't read stopfile: " << stopfile << endl;
	    exit(1);
	}
    }
    string odata, reason;
    if (argc == 1) {
	const char *filename = *argv++;	argc--;
        if (op_flags& OPT_p) {
            vector<string> tokens;
            TextSplit::stringToStrings(filename, tokens);
            for (vector<string>::const_iterator it = tokens.begin();
                 it != tokens.end(); it++) {
                cout << "[" << *it << "] ";
            }
            cout << endl;
            exit(0);
        }
	if (!strcmp(filename, "stdin")) {
	    char buf[1024];
	    int nread;
	    while ((nread = read(0, buf, 1024)) > 0) {
		odata.append(buf, nread);
	    }
	} else if (!file_to_string(filename, odata, &reason)) {
            cerr << "Failed: file_to_string(" << filename << ") failed: " 
                 << reason << endl;
	    exit(1);
        }
    } else {
        if (op_flags & OPT_p)
            Usage();
        for (int i = 0; i < teststrings_cnt; i++) {
            cout << endl << teststrings[i] << endl;  
            dosplit(teststrings[i], flags, op_flags);
        }
        exit(0);
    }

    string& data = odata;
    string ndata;
    if ((op_flags & OPT_C)) {
        if (!transcode(odata, ndata, charset, "UTF-8")) {
            cerr << "Failed: transcode error" << endl;
            exit(1);
        } else {
            data = ndata;
        }
    }

    if (op_flags & OPT_c) {
	int n = TextSplit::countWords(data, flags);
	cout << n << " words" << endl;
    } else {
        dosplit(data, flags, op_flags);
    }    
}
#endif // TEST
