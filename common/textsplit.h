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
#ifndef _TEXTSPLIT_H_INCLUDED_
#define _TEXTSPLIT_H_INCLUDED_

#include <math.h>

#include <string>
#include <vector>

using std::string;
using std::vector;
using std::pair;

class Utf8Iter;

/** 
 * Split text into words. 
 * See comments at top of .cpp for more explanations.
 * This uses a callback function. It could be done with an iterator instead,
 * but 'ts much simpler this way...
 */
class TextSplit {
public:
    // Should we activate special processing of Chinese characters ? This
    // needs a little more cpu, so it can be turned off globally. This is set
    // by rclconfig, changing it means reindexing
    static bool o_processCJK;
    static unsigned int  o_CJKNgramLen;
    static const unsigned int o_CJKMaxNgramLen =  5;
    static void cjkProcessing(bool onoff, unsigned int ngramlen = 2) 
    {
	o_processCJK = onoff;
	o_CJKNgramLen = ngramlen <= o_CJKMaxNgramLen ? 
	    ngramlen : o_CJKMaxNgramLen;
    }

    // Are we indexing numbers ? Set by rclconfig. Change needs reindex
    static bool o_noNumbers;
    static void noNumbers()
    {
	o_noNumbers = true;
    }

    enum Flags {
        // Default: will return spans and words (a_b, a, b)
        TXTS_NONE = 0, 
        // Only return maximum spans (a@b.com, not a, b, or com) 
        TXTS_ONLYSPANS = 1,  
        // Special: Only return atomic words (a, b, com).  This is not
        // used for indexing, but for position computation during
        // abstract generation,
        TXTS_NOSPANS = 2,  
        // Handle wildcards as letters. This is used with ONLYSPANS
        // for parsing a user query (never alone).
        TXTS_KEEPWILD = 4 
    };
    
    TextSplit(Flags flags = Flags(TXTS_NONE))
	: m_flags(flags), m_maxWordLength(40), m_prevpos(-1)
    {
    }
    virtual ~TextSplit() {}

    virtual void setMaxWordLength(int l)
    {
	m_maxWordLength = l;
    }
    /** Split text, emit words and positions. */
    virtual bool text_to_words(const string &in);

    /** Process one output word: to be implemented by the actual user class */
    virtual bool takeword(const string& term, 
			  int pos,  // term pos
			  int bts,  // byte offset of first char in term
			  int bte   // byte offset of first char after term
			  ) = 0; 

    /** Called when we encounter formfeed \f 0x0c. Override to use the event.
     * Mostly or exclusively used with pdftoxx output. Other filters mostly 
     * just don't know about pages. */
    virtual void newpage(int /*pos*/)
    {
    }

    // Static utility functions:

    /** Count words in string, as the splitter would generate them */
    static int countWords(const string &in, Flags flgs = TXTS_ONLYSPANS);

    /** Check if this is visibly not a single block of text */
    static bool hasVisibleWhite(const string &in);

    /** Split text span into strings, at white space, allowing for substrings
     * quoted with " . Escaping with \ works as usual inside the quoted areas.
     * This has to be kept separate from smallut.cpp's stringsToStrings, which
     * basically works only if whitespace is ascii, and which processes 
     * non-utf-8 input (iso-8859 config files work ok). This hopefully
     * handles all Unicode whitespace, but needs correct utf-8 input
     */
    static bool stringToStrings(const string &s, vector<string> &tokens);

    /** Is char CJK ? */
    static bool isCJK(int c);

    /** Statistics about word length (average and dispersion) can
     * detect bad data like undecoded base64 or other mis-identified
     * pieces of data taken as text. In practise, this keeps some junk out 
     * of the index, but does not decrease the index size much, and is
     * probably not worth the trouble in general. Code kept because it
     * probably can be useful in special cases. Base64 data does has
     * word separators in it (+/) and is characterised by high average
     * word length (>10, often close to 20) and high word length
     * dispersion (avg/sigma > 0.8). In my tests, most natural
     * language text has average word lengths around 5-8 and avg/sigma
     * < 0.7
     */
#ifdef TEXTSPLIT_STATS
    class Stats {
    public:
	Stats()
	{
	    reset();
	}
	void reset()
	{
	    count = 0;
	    totlen = 0;
	    sigma_acc = 0;
	}
	void newsamp(unsigned int len)
	{
	    ++count;
	    totlen += len;
	    double avglen = double(totlen) / double(count);
	    sigma_acc += (avglen - len) * (avglen - len);
	}
	struct Values {
	    int count;
	    double avglen;
	    double sigma;
	};
	Values get()
	{
	    Values v;
	    v.count = count;
	    v.avglen = double(totlen) / double(count);
	    v.sigma = sqrt(sigma_acc / count);
	    return v;
	}
    private:
	int count;
	int totlen;
	double sigma_acc;
    };

    Stats::Values getStats()
    {
	return m_stats.get();
    }
    void resetStats()
    {
	m_stats.reset();
    }
#endif // TEXTSPLIT_STATS

private:
    Flags         m_flags;
    int           m_maxWordLength;

    // Current span. Might be jf.dockes@wanadoo.f
    string        m_span; 

    vector <pair<unsigned int, unsigned int> > m_words_in_span;

    // Current word: no punctuation at all in there. Byte offset
    // relative to the current span and byte length
    int           m_wordStart;
    unsigned int  m_wordLen;

    // Currently inside number
    bool          m_inNumber;

    // Term position of current word and span
    int           m_wordpos; 
    int           m_spanpos;

    // It may happen that our cleanup would result in emitting the
    // same term twice. We try to avoid this
    int           m_prevpos;
    unsigned int  m_prevlen;

#ifdef TEXTSPLIT_STATS
    // Stats counters. These are processed in TextSplit rather than by a 
    // TermProc so that we can take very long words (not emitted) into
    // account.
    Stats         m_stats;
#endif
    // Word length in characters. Declared but not updated if !TEXTSPLIT_STATS
    unsigned int  m_wordChars;

    // This processes cjk text:
    bool cjk_to_words(Utf8Iter *it, unsigned int *cp);

    bool emitterm(bool isspan, string &term, int pos, int bs, int be);
    bool doemit(bool spanerase, int bp);
    void discardspan();
    bool span_is_acronym(std::string *acronym);
    bool words_from_span(int bp);
};

#endif /* _TEXTSPLIT_H_INCLUDED_ */
