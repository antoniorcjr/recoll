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

#ifndef TEST_SMALLUT
#ifdef HAVE_CONFIG_H
#include "autoconfig.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <math.h>

#include <string>
#include <iostream>
#include <list>
#include "unordered_defs.h"
using namespace std;

#include "smallut.h"
#include "utf8iter.h"
#include "hldata.h"
#include "cstr.h"

void map_ss_cp_noshr(const map<string,string> s, map<string,string> *d)
{
    for (map<string,string>::const_iterator it= s.begin();
         it != s.end(); it++) {
        d->insert(
            pair<string,string>(string(it->first.begin(), it->first.end()),
                                string(it->second.begin(), it->second.end())));
    }
}

int stringicmp(const string & s1, const string& s2) 
{
    string::const_iterator it1 = s1.begin();
    string::const_iterator it2 = s2.begin();
    int size1 = s1.length(), size2 = s2.length();
    char c1, c2;

    if (size1 > size2) {
	while (it1 != s1.end()) { 
	    c1 = ::toupper(*it1);
	    c2 = ::toupper(*it2);
	    if (c1 != c2) {
		return c1 > c2 ? 1 : -1;
	    }
	    ++it1; ++it2;
	}
	return size1 == size2 ? 0 : 1;
    } else {
	while (it2 != s2.end()) { 
	    c1 = ::toupper(*it1);
	    c2 = ::toupper(*it2);
	    if (c1 != c2) {
		return c1 > c2 ? 1 : -1;
	    }
	    ++it1; ++it2;
	}
	return size1 == size2 ? 0 : -1;
    }
}
void stringtolower(string& io)
{
    string::iterator it = io.begin();
    string::iterator ite = io.end();
    while (it != ite) {
	*it = ::tolower(*it);
	it++;
    }
}
string stringtolower(const string& i)
{
    string o = i;
    stringtolower(o);
    return o;
}
extern int stringisuffcmp(const string& s1, const string& s2)
{
    string::const_reverse_iterator r1 = s1.rbegin(), re1 = s1.rend(),
	r2 = s2.rbegin(), re2 = s2.rend();
    while (r1 != re1 && r2 != re2) {
	char c1 = ::toupper(*r1);
	char c2 = ::toupper(*r2);
	if (c1 != c2) {
	    return c1 > c2 ? 1 : -1;
	}
	++r1; ++r2;
    }
    return 0;
}

//  s1 is already lowercase
int stringlowercmp(const string & s1, const string& s2) 
{
    string::const_iterator it1 = s1.begin();
    string::const_iterator it2 = s2.begin();
    int size1 = s1.length(), size2 = s2.length();
    char c2;

    if (size1 > size2) {
	while (it1 != s1.end()) { 
	    c2 = ::tolower(*it2);
	    if (*it1 != c2) {
		return *it1 > c2 ? 1 : -1;
	    }
	    ++it1; ++it2;
	}
	return size1 == size2 ? 0 : 1;
    } else {
	while (it2 != s2.end()) { 
	    c2 = ::tolower(*it2);
	    if (*it1 != c2) {
		return *it1 > c2 ? 1 : -1;
	    }
	    ++it1; ++it2;
	}
	return size1 == size2 ? 0 : -1;
    }
}

//  s1 is already uppercase
int stringuppercmp(const string & s1, const string& s2) 
{
    string::const_iterator it1 = s1.begin();
    string::const_iterator it2 = s2.begin();
    int size1 = s1.length(), size2 = s2.length();
    char c2;

    if (size1 > size2) {
	while (it1 != s1.end()) { 
	    c2 = ::toupper(*it2);
	    if (*it1 != c2) {
		return *it1 > c2 ? 1 : -1;
	    }
	    ++it1; ++it2;
	}
	return size1 == size2 ? 0 : 1;
    } else {
	while (it2 != s2.end()) { 
	    c2 = ::toupper(*it2);
	    if (*it1 != c2) {
		return *it1 > c2 ? 1 : -1;
	    }
	    ++it1; ++it2;
	}
	return size1 == size2 ? 0 : -1;
    }
}

// Compare charset names, removing the more common spelling variations
bool samecharset(const string &cs1, const string &cs2)
{
    string mcs1, mcs2;
    // Remove all - and _, turn to lowecase
    for (unsigned int i = 0; i < cs1.length();i++) {
	if (cs1[i] != '_' && cs1[i] != '-') {
	    mcs1 += ::tolower(cs1[i]);
	}
    }
    for (unsigned int i = 0; i < cs2.length();i++) {
	if (cs2[i] != '_' && cs2[i] != '-') {
	    mcs2 += ::tolower(cs2[i]);
	}
    }
    return mcs1 == mcs2;
}

template <class T> bool stringToStrings(const string &s, T &tokens, 
                                        const string& addseps)
{
    string current;
    tokens.clear();
    enum states {SPACE, TOKEN, INQUOTE, ESCAPE};
    states state = SPACE;
    for (unsigned int i = 0; i < s.length(); i++) {
	switch (s[i]) {
        case '"': 
	    switch(state) {
            case SPACE: 
		state=INQUOTE; continue;
            case TOKEN: 
	        current += '"';
		continue;
            case INQUOTE: 
                tokens.insert(tokens.end(), current);
		current.clear();
		state = SPACE;
		continue;
            case ESCAPE:
	        current += '"';
	        state = INQUOTE;
                continue;
	    }
	    break;
        case '\\': 
	    switch(state) {
            case SPACE: 
            case TOKEN: 
                current += '\\';
                state=TOKEN; 
                continue;
            case INQUOTE: 
                state = ESCAPE;
                continue;
            case ESCAPE:
                current += '\\';
                state = INQUOTE;
                continue;
	    }
	    break;

        case ' ': 
        case '\t': 
        case '\n': 
        case '\r': 
	    switch(state) {
            case SPACE: 
                continue;
            case TOKEN: 
		tokens.insert(tokens.end(), current);
		current.clear();
		state = SPACE;
		continue;
            case INQUOTE: 
            case ESCAPE:
                current += s[i];
                continue;
	    }
	    break;

        default:
            if (!addseps.empty() && addseps.find(s[i]) != string::npos) {
                switch(state) {
                case ESCAPE:
                    state = INQUOTE;
                    break;
                case INQUOTE: 
                    break;
                case SPACE: 
                    tokens.insert(tokens.end(), string(1, s[i]));
                    continue;
                case TOKEN: 
                    tokens.insert(tokens.end(), current);
                    current.erase();
                    tokens.insert(tokens.end(), string(1, s[i]));
                    state = SPACE;
                    continue;
                }
            } else switch(state) {
                case ESCAPE:
                    state = INQUOTE;
                    break;
                case SPACE: 
                    state = TOKEN;
                    break;
                case TOKEN: 
                case INQUOTE: 
                    break;
                }
	    current += s[i];
	}
    }
    switch(state) {
    case SPACE: 
	break;
    case TOKEN: 
	tokens.insert(tokens.end(), current);
	break;
    case INQUOTE: 
    case ESCAPE:
	return false;
    }
    return true;
}

template bool stringToStrings<list<string> >(const string &, 
					     list<string> &, const string&);
template bool stringToStrings<vector<string> >(const string &, 
					       vector<string> &,const string&);
template bool stringToStrings<set<string> >(const string &,
					    set<string> &, const string&);
template bool stringToStrings<STD_UNORDERED_SET<string> >
(const string &, STD_UNORDERED_SET<string> &, const string&);

template <class T> void stringsToString(const T &tokens, string &s) 
{
    for (typename T::const_iterator it = tokens.begin();
	 it != tokens.end(); it++) {
	bool hasblanks = false;
	if (it->find_first_of(" \t\n") != string::npos)
	    hasblanks = true;
	if (it != tokens.begin())
	    s.append(1, ' ');
	if (hasblanks)
	    s.append(1, '"');
	for (unsigned int i = 0; i < it->length(); i++) {
	    char car = it->at(i);
	    if (car == '"') {
		s.append(1, '\\');
		s.append(1, car);
	    } else {
		s.append(1, car);
	    }
	}
	if (hasblanks)
	    s.append(1, '"');
    }
}
template void stringsToString<list<string> >(const list<string> &, string &);
template void stringsToString<vector<string> >(const vector<string> &,string &);
template void stringsToString<set<string> >(const set<string> &, string &);
template <class T> string stringsToString(const T &tokens)
{
    string out;
    stringsToString<T>(tokens, out);
    return out;
}
template string stringsToString<list<string> >(const list<string> &);
template string stringsToString<vector<string> >(const vector<string> &);
template string stringsToString<set<string> >(const set<string> &);

template <class T> void stringsToCSV(const T &tokens, string &s, 
				     char sep)
{
    s.erase();
    for (typename T::const_iterator it = tokens.begin();
	 it != tokens.end(); it++) {
	bool needquotes = false;
	if (it->empty() || 
	    it->find_first_of(string(1, sep) + "\"\n") != string::npos)
	    needquotes = true;
	if (it != tokens.begin())
	    s.append(1, sep);
	if (needquotes)
	    s.append(1, '"');
	for (unsigned int i = 0; i < it->length(); i++) {
	    char car = it->at(i);
	    if (car == '"') {
		s.append(2, '"');
	    } else {
		s.append(1, car);
	    }
	}
	if (needquotes)
	    s.append(1, '"');
    }
}
template void stringsToCSV<list<string> >(const list<string> &, string &, char);
template void stringsToCSV<vector<string> >(const vector<string> &,string &, 
					    char);

void stringToTokens(const string& str, vector<string>& tokens,
		    const string& delims, bool skipinit)
{
    string::size_type startPos = 0, pos;

    // Skip initial delims, return empty if this eats all.
    if (skipinit && 
	(startPos = str.find_first_not_of(delims, 0)) == string::npos) {
	return;
    }
    while (startPos < str.size()) { 
        // Find next delimiter or end of string (end of token)
        pos = str.find_first_of(delims, startPos);

        // Add token to the vector and adjust start
	if (pos == string::npos) {
	    tokens.push_back(str.substr(startPos));
	    break;
	} else if (pos == startPos) {
	    // Dont' push empty tokens after first
	    if (tokens.empty())
		tokens.push_back(string());
	    startPos = ++pos;
	} else {
	    tokens.push_back(str.substr(startPos, pos - startPos));
	    startPos = ++pos;
	}
    }
}

bool stringToBool(const string &s)
{
    if (s.empty())
	return false;
    if (isdigit(s[0])) {
	int val = atoi(s.c_str());
	return val ? true : false;
    }
    if (s.find_first_of("yYtT") == 0)
	return true;
    return false;
}

void trimstring(string &s, const char *ws)
{
    string::size_type pos = s.find_first_not_of(ws);
    if (pos == string::npos) {
	s.clear();
	return;
    }
    s.replace(0, pos, string());

    pos = s.find_last_not_of(ws);
    if (pos != string::npos && pos != s.length()-1)
	s.replace(pos+1, string::npos, string());
}

// Remove some chars and replace them with spaces
string neutchars(const string &str, const string &chars)
{
    string out;
    neutchars(str, out, chars);
    return out;
}
void neutchars(const string &str, string &out, const string& chars)
{
    string::size_type startPos, pos;

    for (pos = 0;;) { 
        // Skip initial chars, break if this eats all.
        if ((startPos = str.find_first_not_of(chars, pos)) == string::npos)
	    break;
        // Find next delimiter or end of string (end of token)
        pos = str.find_first_of(chars, startPos);
        // Add token to the output. Note: token cant be empty here
	if (pos == string::npos) {
	    out += str.substr(startPos);
	} else {
	    out += str.substr(startPos, pos - startPos) + " ";
	}
    }
}


/* Truncate a string to a given maxlength, avoiding cutting off midword
 * if reasonably possible. Note: we could also use textsplit, stopping when
 * we have enough, this would be cleanly utf8-aware but would remove 
 * punctuation */
static const string cstr_SEPAR = " \t\n\r-:.;,/[]{}";
string truncate_to_word(const string &input, string::size_type maxlen)
{
    string output;
    if (input.length() <= maxlen) {
	output = input;
    } else {
	output = input.substr(0, maxlen);
	string::size_type space = output.find_last_of(cstr_SEPAR);
	// Original version only truncated at space if space was found after
	// maxlen/2. But we HAVE to truncate at space, else we'd need to do
	// utf8 stuff to avoid truncating at multibyte char. In any case,
	// not finding space means that the text probably has no value.
	// Except probably for Asian languages, so we may want to fix this 
	// one day
	if (space == string::npos) {
	    output.erase();
	} else {
	    output.erase(space);
	}
    }
    return output;
}

void utf8truncate(string &s, int maxlen)
{
    if (s.size() <= string::size_type(maxlen))
	return;
    Utf8Iter iter(s);
    int pos = 0;
    while (iter++ != string::npos) 
	if (iter.getBpos() < string::size_type(maxlen))
	    pos = iter.getBpos();

    s.erase(pos);
}

// Escape things that would look like markup
string escapeHtml(const string &in)
{
    string out;
    for (string::size_type pos = 0; pos < in.length(); pos++) {
	switch(in.at(pos)) {
	case '<':
	    out += "&lt;";
	    break;
	case '&':
	    out += "&amp;";
	    break;
	default:
	    out += in.at(pos);
	}
    }
    return out;
}

string escapeShell(const string &in)
{
    string out;
    out += "\"";
    for (string::size_type pos = 0; pos < in.length(); pos++) {
	switch(in.at(pos)) {
	case '$':
	    out += "\\$";
	    break;
	case '`':
	    out += "\\`";
	    break;
	case '"':
	    out += "\\\"";
	    break;
	case '\n':
	    out += "\\\n";
	    break;
	case '\\':
	    out += "\\\\";
	    break;
	default:
	    out += in.at(pos);
	}
    }
    out += "\"";
    return out;
}


// Substitute printf-like percent cmds inside a string
bool pcSubst(const string& in, string& out, const map<char, string>& subs)
{
    string::const_iterator it;
    for (it = in.begin(); it != in.end();it++) {
	if (*it == '%') {
	    if (++it == in.end()) {
		out += '%';
		break;
	    }
	    if (*it == '%') {
		out += '%';
		continue;
	    }
	    map<char,string>::const_iterator tr;
	    if ((tr = subs.find(*it)) != subs.end()) {
		out += tr->second;
	    } else {
		// We used to do "out += *it;" here but this does not make
                // sense
	    }
	} else {
	    out += *it;
	}
    }
    return true;
}

bool pcSubst(const string& in, string& out, const map<string, string>& subs)
{
    out.erase();
    string::size_type i;
    for (i = 0; i < in.size(); i++) {
	if (in[i] == '%') {
	    if (++i == in.size()) {
		out += '%';
		break;
	    }
	    if (in[i] == '%') {
		out += '%';
		continue;
	    }
            string key = "";
            if (in[i] == '(') {
                if (++i == in.size()) {
                    out += string("%(");
                    break;
                }
                string::size_type j = in.find_first_of(")", i);
                if (j == string::npos) {
                    // ??concatenate remaining part and stop
                    out += in.substr(i-2);
                    break;
                }
                key = in.substr(i, j-i);
                i = j;
            } else {
                key = in[i];
            }
	    map<string,string>::const_iterator tr;
	    if ((tr = subs.find(key)) != subs.end()) {
		out += tr->second;
	    } else {
                // Substitute to nothing, that's the reasonable thing to do
                // instead of keeping the %(key)
                // out += key.size()==1? key : string("(") + key + string(")");
	    }
	} else {
	    out += in[i];
	}
    }
    return true;
}

// Convert byte count into unit (KB/MB...) appropriate for display
string displayableBytes(off_t size)
{
    char sizebuf[50];
    const char *unit;
    
    double roundable = 0;
    if (size < 1000) {
	unit = " B ";
	roundable = double(size);
    } else if (size < 1E6) {
	unit = " KB ";
	roundable = double(size) / 1E3;
    } else if (size < 1E9) {
	unit = " MB ";
	roundable = double(size) / 1E6;
    } else {
	unit = " GB ";
	roundable = double(size) / 1E9;
    }
    size = round(roundable);
    sprintf(sizebuf, "%lld" "%s", (long long)size, unit);
    return string(sizebuf);
}

string breakIntoLines(const string& in, unsigned int ll, 
		      unsigned int maxlines)
{
    string query = in;
    string oq;
    unsigned int nlines = 0;
    while (query.length() > 0) {
	string ss = query.substr(0, ll);
	if (ss.length() == ll) {
	    string::size_type pos = ss.find_last_of(" ");
	    if (pos == string::npos) {
		pos = query.find_first_of(" ");
		if (pos != string::npos)
		    ss = query.substr(0, pos+1);
		else 
		    ss = query;
	    } else {
		ss = ss.substr(0, pos+1);
	    }
	}
	// This cant happen, but anyway. Be very sure to avoid an infinite loop
	if (ss.length() == 0) {
	    oq = query;
	    break;
	}
	oq += ss + "\n";
	if (nlines++ >= maxlines) {
	    oq += " ... \n";
	    break;
	}
	query= query.substr(ss.length());
    }
    return oq;
}

////////////////////

#if 0
// Internal redefinition of system time interface to help with dependancies
struct m_timespec {
  time_t tv_sec;
  long   tv_nsec;
};
#endif

#ifndef CLOCK_REALTIME
typedef int clockid_t;
#define CLOCK_REALTIME 1
#endif

#define MILLIS(TV) ( (long)(((TV).tv_sec - m_secs) * 1000L + \
  ((TV).tv_nsec - m_nsecs) / 1000000))
#define MICROS(TV) ( (long)(((TV).tv_sec - m_secs) * 1000000L + \
  ((TV).tv_nsec - m_nsecs) / 1000))
#define NANOS(TV) ( (long long)(((TV).tv_sec - m_secs) * 1000000000LL + \
  ((TV).tv_nsec - m_nsecs)))

// Using clock_gettime() is nice because it gives us ns res and it helps with
// computing threads work times, but it's also a pita because it forces linking
// with -lrt. So keep it optional. And not on the mac anyway
// #define USE_CLOCK_GETTIME

#ifdef __APPLE__
#undef USE_CLOCK_GETTIME
#endif

#ifndef USE_CLOCK_GETTIME
#include <sys/time.h>
#endif

static void gettime(clockid_t clk_id, struct timespec *ts)
{
#ifndef USE_CLOCK_GETTIME
    struct timeval tv;
    gettimeofday(&tv, 0);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
#else
    clock_gettime(clk_id, ts);
#endif
}
///// End system interface

// Note: this not protected against multithread access and not reentrant, but
// this is mostly debug code, and it won't crash, just show bad results. Also 
// the frozen thing is not used that much
static timespec frozen_tv;
void Chrono::refnow()
{
  gettime(CLOCK_REALTIME, &frozen_tv);
}

Chrono::Chrono()
{
  restart();
}

// Reset and return value before rest in milliseconds
long Chrono::restart()
{
  struct timespec tv;
  gettime(CLOCK_REALTIME, &tv);
  long ret = MILLIS(tv);
  m_secs = tv.tv_sec;
  m_nsecs = tv.tv_nsec;
  return ret;
}

// Get current timer value, milliseconds
long Chrono::millis(int frozen)
{
    return nanos() / 1000000;
}

//
long Chrono::micros(int frozen)
{
    return nanos() / 1000;
}

long long Chrono::nanos(int frozen)
{
  if (frozen) {
    return NANOS(frozen_tv);
  } else {
    struct timespec tv;
    gettime(CLOCK_REALTIME, &tv);
    return NANOS(tv);
  }
}

float Chrono::secs(int frozen)
{
  struct timespec tv;
  gettime(CLOCK_REALTIME, &tv);
  float secs = (float)(frozen?frozen_tv.tv_sec:tv.tv_sec - m_secs);
  float nsecs = (float)(frozen?frozen_tv.tv_nsec:tv.tv_nsec - m_nsecs); 
  return secs + nsecs * 1e-9;
}

// Date is Y[-M[-D]]
static bool parsedate(vector<string>::const_iterator& it, 
              vector<string>::const_iterator end, DateInterval *dip)
{
    dip->y1 = dip->m1 = dip->d1 = dip->y2 = dip->m2 = dip->d2 = 0;
    if (it->length() > 4 || !it->length() || 
        it->find_first_not_of("0123456789") != string::npos) {
        return false;
    }
    if (it == end || sscanf(it++->c_str(), "%d", &dip->y1) != 1) {
        return false;
    }
    if (it == end || *it == "/")
        return true;
    if (*it++ != "-") {
        return false;
    }

    if (it->length() > 2 || !it->length() || 
        it->find_first_not_of("0123456789") != string::npos) {
        return false;
    }
    if (it == end || sscanf(it++->c_str(), "%d", &dip->m1) != 1) {
        return false;
    }
    if (it == end || *it == "/")
        return true;
    if (*it++ != "-") {
        return false;
    }

    if (it->length() > 2 || !it->length() || 
        it->find_first_not_of("0123456789") != string::npos) {
        return false;
    }
    if (it == end || sscanf(it++->c_str(), "%d", &dip->d1) != 1) {
        return -1;
    }

    return true;
}

// Called with the 'P' already processed. Period ends at end of string
// or at '/'. We dont' do a lot effort at validation and will happily
// accept 10Y1Y4Y (the last wins)
static bool parseperiod(vector<string>::const_iterator& it, 
                        vector<string>::const_iterator end, DateInterval *dip)
{
    dip->y1 = dip->m1 = dip->d1 = dip->y2 = dip->m2 = dip->d2 = 0;
    while (it != end) {
        int value;
        if (it->find_first_not_of("0123456789") != string::npos) {
            return false;
        }
        if (sscanf(it++->c_str(), "%d", &value) != 1) {
            return false;
        }
        if (it == end || it->empty())
            return false;
        switch (it->at(0)) {
        case 'Y': case 'y': dip->y1 = value;break;
        case 'M': case 'm': dip->m1 = value;break;
        case 'D': case 'd': dip->d1 = value;break;
        default: return false;
        }
        it++;
        if (it == end)
            return true;
        if (*it == "/") {
            return true;
        }
    }
    return true;
}

static void cerrdip(const string& s, DateInterval *dip)
{
    cerr << s << dip->y1 << "-" << dip->m1 << "-" << dip->d1 << "/"
         << dip->y2 << "-" << dip->m2 << "-" << dip->d2 
         << endl;
}

// Compute date + period. Won't work out of the unix era. 
// or pre-1970 dates. Just convert everything to unixtime and
// seconds (with average durations for months/years), add and convert
// back
static bool addperiod(DateInterval *dp, DateInterval *pp)
{
    struct tm tm;
    // Create a struct tm with possibly non normalized fields and let
    // timegm sort it out
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = dp->y1 - 1900 + pp->y1;
    tm.tm_mon = dp->m1 + pp->m1 -1;
    tm.tm_mday = dp->d1 + pp->d1;
#ifdef sun
    time_t tres = mktime(&tm);
    localtime_r(&tres, &tm);
#else
    time_t tres = timegm(&tm);
    gmtime_r(&tres, &tm);
#endif
    dp->y1 = tm.tm_year + 1900;
    dp->m1 = tm.tm_mon + 1;
    dp->d1 = tm.tm_mday;
    //cerrdip("Addperiod return", dp);
    return true;
}
int monthdays(int mon, int year)
{
    switch (mon) {
    // We are returning a few two many 29 days februaries, no problem
    case 2: return (year % 4) == 0 ? 29 : 28;
    case 1:case 3:case 5:case 7: case 8:case 10:case 12: return 31;
    default: return 30;
    }
}
bool parsedateinterval(const string& s, DateInterval *dip)
{
    vector<string> vs;
    dip->y1 = dip->m1 = dip->d1 = dip->y2 = dip->m2 = dip->d2 = 0;
    DateInterval p1, p2, d1, d2;
    p1 = p2 = d1 = d2 = *dip;
    bool hasp1 = false, hasp2 = false, hasd1 = false, hasd2 = false, 
        hasslash = false;

    if (!stringToStrings(s, vs, "PYMDpymd-/")) {
        return false;
    }
    if (vs.empty())
        return false;

    vector<string>::const_iterator it = vs.begin();
    if (*it == "P" || *it == "p") {
        it++;
        if (!parseperiod(it, vs.end(), &p1)) {
            return false;
        }
        hasp1 = true;
        //cerrdip("p1", &p1);
        p1.y1 = -p1.y1;
        p1.m1 = -p1.m1;
        p1.d1 = -p1.d1;
    } else if (*it == "/") {
        hasslash = true;
        goto secondelt;
    } else {
        if (!parsedate(it, vs.end(), &d1)) {
            return false;
        }
        hasd1 = true;
    }

    // Got one element and/or /
secondelt:
    if (it != vs.end()) {
        if (*it != "/") {
            return false;
        }
        hasslash = true;
        it++;
        if (it == vs.end()) {
            // ok
        } else if (*it == "P" || *it == "p") {
            it++;
            if (!parseperiod(it, vs.end(), &p2)) {
                return false;
            }
        hasp2 = true;
        } else {
            if (!parsedate(it, vs.end(), &d2)) {
                return false;
            }
            hasd2 = true;
        }
    }

    // 2 periods dont' make sense
    if (hasp1 && hasp2) {
        return false;
    }
    // Nothing at all doesn't either
    if (!hasp1 && !hasd1 && !hasp2 && !hasd2) {
        return false;
    }

    // Empty part means today IF other part is period, else means
    // forever (stays at 0)
    time_t now = time(0);
    struct tm *tmnow = gmtime(&now);
    if ((!hasp1 && !hasd1) && hasp2) {
        d1.y1 = 1900 + tmnow->tm_year;
        d1.m1 = tmnow->tm_mon + 1;
        d1.d1 = tmnow->tm_mday;
        hasd1 = true;
    } else if ((!hasp2 && !hasd2) && hasp1) {
        d2.y1 = 1900 + tmnow->tm_year;
        d2.m1 = tmnow->tm_mon + 1;
        d2.d1 = tmnow->tm_mday;
        hasd2 = true;
    }

    // Incomplete dates have different meanings depending if there is
    // a period or not (actual or infinite indicated by a / + empty)
    //
    // If there is no explicit period, an incomplete date indicates a
    // period of the size of the uncompleted elements. Ex: 1999
    // actually means 1999/P12M
    // 
    // If there is a period, the incomplete date should be extended
    // to the beginning or end of the unspecified portion. Ex: 1999/
    // means 1999-01-01/ and /1999 means /1999-12-31
    if (hasd1) {
        if (!(hasslash || hasp2)) {
            if (d1.m1 == 0) {
                p2.m1 = 12;
                d1.m1 = 1;
                d1.d1 = 1;
            } else if (d1.d1 == 0) {
                d1.d1 = 1;
                p2.d1 = monthdays(d1.m1, d1.y1);
            }
            hasp2 = true;
        } else {
            if (d1.m1 == 0) {
                d1.m1 = 1;
                d1.d1 = 1;
            } else if (d1.d1 == 0) {
                d1.d1 = 1;
            }
        }
    }
    // if hasd2 is true we had a /
    if (hasd2) {
        if (d2.m1 == 0) {
            d2.m1 = 12;
            d2.d1 = 31;
        } else if (d2.d1 == 0) {
            d2.d1 = monthdays(d2.m1, d2.y1);
        }
    }
    if (hasp1) {
        // Compute d1
        d1 = d2;
        if (!addperiod(&d1, &p1)) {
            return false;
        }
    } else if (hasp2) {
        // Compute d2
        d2 = d1;
        if (!addperiod(&d2, &p2)) {
            return false;
        }
    }

    dip->y1 = d1.y1;
    dip->m1 = d1.m1;
    dip->d1 = d1.d1;
    dip->y2 = d2.y1;
    dip->m2 = d2.m1;
    dip->d2 = d2.d1;
    return true;
}


void catstrerror(string *reason, const char *what, int _errno)
{
    if (!reason)
	return;
    if (what)
	reason->append(what);

    reason->append(": errno: ");

    char nbuf[20];
    sprintf(nbuf, "%d", _errno);
    reason->append(nbuf);

    reason->append(" : ");

#ifdef sun
    // Note: sun strerror is noted mt-safe ??
    reason->append(strerror(_errno));
#else
#define ERRBUFSZ 200    
    char errbuf[ERRBUFSZ];
    // There are 2 versions of strerror_r. 
    // - The GNU one returns a pointer to the message (maybe
    //   static storage or supplied buffer).
    // - The POSIX one always stores in supplied buffer and
    //   returns 0 on success. As the possibility of error and
    //   error code are not specified, we're basically doomed
    //   cause we can't use a test on the 0 value to know if we
    //   were returned a pointer... 
    // Also couldn't find an easy way to disable the gnu version without
    // changing the cxxflags globally, so forget it. Recent gnu lib versions
    // normally default to the posix version.
    // At worse we get no message at all here.
    errbuf[0] = 0;
    strerror_r(_errno, errbuf, ERRBUFSZ);
    reason->append(errbuf);
#endif
}

void HighlightData::toString(std::string& out)
{
    out.append("\nUser terms (orthograph): ");
    for (std::set<std::string>::const_iterator it = uterms.begin();
	 it != uterms.end(); it++) {
	out.append(" [").append(*it).append("]");
    }
    out.append("\nUser terms to Query terms:");
    for (map<string, string>::const_iterator it = terms.begin();
	 it != terms.end(); it++) {
	out.append("[").append(it->first).append("]->[");
	out.append(it->second).append("] ");
    }
    out.append("\nGroups: ");
    char cbuf[200];
    sprintf(cbuf, "Groups size %d grpsugidx size %d ugroups size %d",
	    int(groups.size()), int(grpsugidx.size()), int(ugroups.size()));
    out.append(cbuf);

    unsigned int ugidx = (unsigned int)-1;
    for (unsigned int i = 0; i < groups.size(); i++) {
	if (ugidx != grpsugidx[i]) {
	    ugidx = grpsugidx[i];
	    out.append("\n(");
	    for (unsigned int j = 0; j < ugroups[ugidx].size(); j++) {
		out.append("[").append(ugroups[ugidx][j]).append("] ");
	    }
	    out.append(") ->");
	}
	out.append(" {");
	for (unsigned int j = 0; j < groups[i].size(); j++) {
	    out.append("[").append(groups[i][j]).append("]");
	}
	sprintf(cbuf, "%d", slacks[i]);
	out.append("}").append(cbuf);
    }
    out.append("\n");
}

void HighlightData::append(const HighlightData& hl)
{
    uterms.insert(hl.uterms.begin(), hl.uterms.end());
    terms.insert(hl.terms.begin(), hl.terms.end());
    size_t ugsz0 = ugroups.size();
    ugroups.insert(ugroups.end(), hl.ugroups.begin(), hl.ugroups.end());

    groups.insert(groups.end(), hl.groups.begin(), hl.groups.end());
    slacks.insert(slacks.end(), hl.slacks.begin(), hl.slacks.end());
    for (std::vector<unsigned int>::const_iterator it = hl.grpsugidx.begin(); 
	 it != hl.grpsugidx.end(); it++) {
	grpsugidx.push_back(*it + ugsz0);
    }
}

static const char *vlang_to_code[] = {
    "be", "cp1251",
    "bg", "cp1251",
    "cs", "iso-8859-2",
    "el", "iso-8859-7",
    "he", "iso-8859-8",
    "hr", "iso-8859-2",
    "hu", "iso-8859-2",
    "ja", "eucjp",
    "kk", "pt154",
    "ko", "euckr",
    "lt", "iso-8859-13",
    "lv", "iso-8859-13",
    "pl", "iso-8859-2",
    "rs", "iso-8859-2",
    "ro", "iso-8859-2",
    "ru", "koi8-r",
    "sk", "iso-8859-2",
    "sl", "iso-8859-2",
    "sr", "iso-8859-2",
    "th", "iso-8859-11",
    "tr", "iso-8859-9",
    "uk", "koi8-u",
};

string langtocode(const string& lang)
{
    static STD_UNORDERED_MAP<string, string> lang_to_code;
    if (lang_to_code.empty()) {
	for (unsigned int i = 0; 
	     i < sizeof(vlang_to_code) / sizeof(char *); i += 2) {
	    lang_to_code[vlang_to_code[i]] = vlang_to_code[i+1];
	}
    }
    STD_UNORDERED_MAP<string,string>::const_iterator it = 
	lang_to_code.find(lang);

    // Use cp1252 by default...
    if (it == lang_to_code.end())
	return cstr_cp1252;

    return it->second;
}

string localelang()
{
    const char *lang = getenv("LANG");

    if (lang == 0 || *lang == 0 || !strcmp(lang, "C") || !strcmp(lang, "POSIX"))
	return "en";
    string locale(lang);
    string::size_type under = locale.find_first_of("_");
    if (under == string::npos)
	return locale;
    return locale.substr(0, under);
}

// Initialization for static stuff to be called from main thread before going 
// multiple
void smallut_init_mt()
{
    // Init langtocode() static table
    langtocode("");
}

#else // TEST_SMALLUT

#include <string>
using namespace std;
#include <iostream>

#include "smallut.h"

struct spair {
    const char *s1;
    const char *s2;
};
struct spair pairs[] = {
    {"", ""},
    {"", "a"},
    {"a", ""},
    {"a", "a"},
    {"A", "a"},
    {"a", "A"},
    {"A", "A"},
    {"12", "12"},
    {"a", "ab"},
    {"ab", "a"},
    {"A", "Ab"},
    {"a", "Ab"},
};
int npairs = sizeof(pairs) / sizeof(struct spair);
struct spair suffpairs[] = {
    {"", ""},
    {"", "a"},
    {"a", ""},
    {"a", "a"},
    {"toto.txt", ".txt"},
    {"TXT", "toto.txt"},
    {"toto.txt", ".txt1"},
    {"toto.txt1", ".txt"},
};
int nsuffpairs = sizeof(suffpairs) / sizeof(struct spair);


// Periods test strings
const char* periods[] = {
    "2001",    // Year 2001
    "2001/",  // 2001 or later 
    "2001/P3Y", // 2001 -> 2004 or 2005, ambiguous
    "2001-01-01/P3Y", // 01-2001 -> 01 2004
    "2001-03-03/2001-05-01", // Explicit one
    "P3M/", // 3 months ago to now
    "P1Y1M/2001-03-01", // 2000-02-01/2001-03-01
    "/2001", // From the epoch to the end of 2001
};
const int nperiods = sizeof(periods) / sizeof(char*);

const char *thisprog;
static void cerrdip(const string& s, DateInterval *dip)
{
    cerr << s << dip->y1 << "-" << dip->m1 << "-" << dip->d1 << "/"
         << dip->y2 << "-" << dip->m2 << "-" << dip->d2 
         << endl;
}

int main(int argc, char **argv)
{
    thisprog = *argv++;argc--;

#if 1
    if (argc <=0 ) {
        cerr << "Usage: smallut <stringtosplit>" << endl;
        exit(1);
    }
    string s = *argv++;argc--;
    vector<string> vs;
    if (!stringToStrings(s, vs, ":-()")) {
        cerr << "Bad entry" << endl;
        exit(1);
    }
    for (vector<string>::const_iterator it = vs.begin(); it != vs.end(); it++)
        cerr << "[" << *it << "] ";
    cerr << endl;
    exit(0);
#elif 0
    if (argc <=0 ) {
        cerr << "Usage: smallut <dateinterval>" << endl;
        exit(1);
    }
    string s = *argv++;argc--;
    DateInterval di;
    if (!parsedateinterval(s, &di)) {
        cerr << "Parse failed" << endl;
        exit(1);
    }
    cerrdip("", &di);
    exit(0);
#elif 0
    DateInterval di;
    for (int i = 0; i < nperiods; i++) {
        if (!parsedateinterval(periods[i], &di)) {
            cerr << "Parsing failed for [" << periods[i] << "]" << endl;
        } else {
            cerrdip(string(periods[i]).append(" : "), &di);
        }
    }
    exit(0);
#elif 0
    for (int i = 0; i < npairs; i++) {
	{
	    int c = stringicmp(pairs[i].s1, pairs[i].s2);
	    printf("'%s' %s '%s' ", pairs[i].s1, 
		   c == 0 ? "==" : c < 0 ? "<" : ">", pairs[i].s2);
	}
	{
	    int cl = stringlowercmp(pairs[i].s1, pairs[i].s2);
	    printf("L '%s' %s '%s' ", pairs[i].s1, 
		   cl == 0 ? "==" : cl < 0 ? "<" : ">", pairs[i].s2);
	}
	{
	    int cu = stringuppercmp(pairs[i].s1, pairs[i].s2);
	    printf("U '%s' %s '%s' ", pairs[i].s1, 
		   cu == 0 ? "==" : cu < 0 ? "<" : ">", pairs[i].s2);
	}
	printf("\n");
    }
#elif 0
    for (int i = 0; i < nsuffpairs; i++) {
	int c = stringisuffcmp(suffpairs[i].s1, suffpairs[i].s2);
	printf("[%s] %s [%s] \n", suffpairs[i].s1, 
	       c == 0 ? "matches" : c < 0 ? "<" : ">", suffpairs[i].s2);
    }
#elif 0
    std::string testit("\303\251l\303\251gant");
    for (int sz = 10; sz >= 0; sz--) {
	utf8truncate(testit, sz);
	cout << testit << endl;
    }
#elif 0
    std::string testit("ligne\ndeuxieme ligne\r3eme ligne\r\n");
    cout << "[" << neutchars(testit, "\r\n") << "]" << endl;
    string i, o;
    cout << "neutchars(null) is [" << neutchars(i, "\r\n") << "]" << endl;
#elif 0
    map<string, string> substs;
    substs["a"] = "A_SUBST";
    substs["title"] = "TITLE_SUBST";
    string in = "a: %a title: %(title) pcpc: %% %";
    string out;
    pcSubst(in, out, substs);
    cout << in << " => " << out << endl;

    in = "unfinished: %(unfinished";
    pcSubst(in, out, substs);
    cout << in << " => " << out << endl;
    in = "unfinished: %(";
    pcSubst(in, out, substs);
    cout << in << " => " << out << endl;
    in = "empty: %()";
    pcSubst(in, out, substs);
    cout << in << " => " << out << endl;
    substs.clear();
    in = "a: %a title: %(title) pcpc: %% %";
    pcSubst(in, out, substs);
    cout << "After map clear: " << in << " => " << out << endl;
#elif 0
    list<string> tokens;
    tokens.push_back("");
    tokens.push_back("a,b");
    tokens.push_back("simple value");
    tokens.push_back("with \"quotes\"");
    string out;
    stringsToCSV(tokens, out);
    cout << "CSV line: [" << out << "]" << endl;
#endif

}

#endif
