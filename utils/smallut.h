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
#ifndef _SMALLUT_H_INCLUDED_
#define _SMALLUT_H_INCLUDED_

#include <stdlib.h>

#include <string>
#include <vector>
#include <map>
#include <set>

using std::string;
using std::vector;
using std::map;
using std::set;

// Note these are all ascii routines
extern int stringicmp(const string& s1, const string& s2);
// For find_if etc.
struct StringIcmpPred {
    StringIcmpPred(const string& s1) 
        : m_s1(s1) 
    {}
    bool operator()(const string& s2) {
        return stringicmp(m_s1, s2) == 0;
    }
    const string& m_s1;
};

extern int stringlowercmp(const string& alreadylower, const string& s2);
extern int stringuppercmp(const string& alreadyupper, const string& s2); 

extern void stringtolower(string& io);
extern string stringtolower(const string& io);

// Is one string the end part of the other ?
extern int stringisuffcmp(const string& s1, const string& s2);

// Divine language from locale
extern std::string localelang();
// Divine 8bit charset from language
extern std::string langtocode(const string& lang);

// Compare charset names, removing the more common spelling variations
extern bool samecharset(const string &cs1, const string &cs2);

// Parse date interval specifier into pair of y,m,d dates.  The format
// for the time interval is based on a subset of iso 8601 with 
// the addition of open intervals, and removal of all time indications.
// 'P' is the Period indicator, it's followed by a length in
// years/months/days (or any subset thereof)
// Dates: YYYY-MM-DD YYYY-MM YYYY
// Periods: P[nY][nM][nD] where n is an integer value. 
// At least one of YMD must be specified
// The separator for the interval is /. Interval examples
// YYYY/ (from YYYY) YYYY-MM-DD/P3Y (3 years after date) etc.
// This returns a pair of y,m,d dates.
struct DateInterval {
    int y1;int m1;int d1; int y2;int m2;int d2;
};
extern bool parsedateinterval(const string&s, DateInterval *di);
extern int monthdays(int mon, int year);

/**
 * Parse input string into list of strings. 
 *
 * Token delimiter is " \t\n" except inside dquotes. dquote inside
 * dquotes can be escaped with \ etc...
 * Input is handled a byte at a time, things will work as long as space tab etc.
 * have the ascii values and can't appear as part of a multibyte char. utf-8 ok
 * but so are the iso-8859-x and surely others. addseps do have to be 
 * single-bytes
 */
template <class T> bool stringToStrings(const string& s, T &tokens, 
					const string& addseps = "");

/**
 * Inverse operation:
 */
template <class T> void stringsToString(const T &tokens, string &s);
template <class T> std::string stringsToString(const T &tokens);

/**
 * Strings to CSV string. tokens containing the separator are quoted (")
 * " inside tokens is escaped as "" ([word "quote"] =>["word ""quote"""]
 */
template <class T> void stringsToCSV(const T &tokens, string &s, 
					char sep = ',');

/**
 * Split input string. No handling of quoting
 */
extern void stringToTokens(const string &s, vector<string> &tokens, 
			   const string &delims = " \t", bool skipinit=true);

/** Convert string to boolean */
extern bool stringToBool(const string &s);

/** Remove instances of characters belonging to set (default {space,
    tab}) at beginning and end of input string */
extern void trimstring(string &s, const char *ws = " \t");

/** Escape things like < or & by turning them into entities */
extern string escapeHtml(const string &in);

/** Replace some chars with spaces (ie: newline chars). This is not utf8-aware
 *  so chars should only contain ascii */
extern string neutchars(const string &str, const string &chars);
extern void neutchars(const string &str, string& out, const string &chars);

/** Turn string into something that won't be expanded by a shell. In practise
 *  quote with double-quotes and escape $`\ */
extern string escapeShell(const string &str);

/** Truncate a string to a given maxlength, avoiding cutting off midword
 *  if reasonably possible. */
extern string truncate_to_word(const string &input, string::size_type maxlen);

/** Truncate in place in an utf8-legal way */
extern void utf8truncate(string &s, int maxlen);

/** Convert byte count into unit (KB/MB...) appropriate for display */
string displayableBytes(off_t size);

/** Break big string into lines */
string breakIntoLines(const string& in, unsigned int ll = 100, 
		      unsigned int maxlines= 50);
/** Small utility to substitute printf-like percents cmds in a string */
bool pcSubst(const string& in, string& out, const map<char, string>& subs);
/** Substitute printf-like percents and also %(key) */
bool pcSubst(const string& in, string& out, const map<string, string>& subs);

/** Append system error message */
void catstrerror(string *reason, const char *what, int _errno);

/** Compute times to help with perf issues */
class Chrono {
 public:
  Chrono();
  /** Reset origin */
  long restart();
  /** Snapshot current time */
  static void refnow();
  /** Get current elapsed since creation or restart
   *
   *  @param frozen give time since the last refnow call (this is to
   * allow for using one actual system call to get values from many
   * chrono objects, like when examining timeouts in a queue)
   */
  long millis(int frozen = 0);
  long ms() {return millis();}
  long micros(int frozen = 0);
  long long nanos(int frozen = 0);
  float secs(int frozen = 0);
 private:
  long	m_secs;
  long 	m_nsecs; 
};

/** Temp buffer with automatic deallocation */
struct TempBuf {
    TempBuf() 
        : m_buf(0)
    {}
    TempBuf(int n)
    {
        m_buf = (char *)malloc(n);
    }
    ~TempBuf()
    { 
        if (m_buf)
            free(m_buf);
    }
    char *setsize(int n) { return (m_buf = (char *)realloc(m_buf, n)); }
    char *buf() {return m_buf;}
    char *m_buf;
};

inline void leftzeropad(string& s, unsigned len)
{
    if (s.length() && s.length() < len)
	s = s.insert(0, len - s.length(), '0');
}

// Duplicate map<string,string> while ensuring no shared string data (to pass
// to other thread):
void map_ss_cp_noshr(const std::map<std::string,std::string> s,
                      std::map<std::string,std::string> *d);

// Code for static initialization of an stl map. Somewhat like Boost.assign. 
// Ref: http://stackoverflow.com/questions/138600/initializing-a-static-stdmapint-int-in-c
// Example use: map<int, int> m = create_map<int, int> (1,2) (3,4) (5,6) (7,8);

template <typename T, typename U>
class create_map
{
private:
    std::map<T, U> m_map;
public:
    create_map(const T& key, const U& val)
    {
        m_map[key] = val;
    }

    create_map<T, U>& operator()(const T& key, const U& val)
    {
        m_map[key] = val;
        return *this;
    }

    operator std::map<T, U>()
    {
        return m_map;
    }
};
template <typename T>
class create_vector
{
private:
    std::vector<T> m_vector;
public:
    create_vector(const T& val)
    {
        m_vector.push_back(val);
    }

    create_vector<T>& operator()(const T& val)
    {
        m_vector.push_back(val);
        return *this;
    }

    operator std::vector<T>()
    {
        return m_vector;
    }
};

#ifndef MIN
#define MIN(A,B) (((A)<(B)) ? (A) : (B))
#endif
#ifndef MAX
#define MAX(A,B) (((A)>(B)) ? (A) : (B))
#endif
#ifndef deleteZ
#define deleteZ(X) {delete X;X = 0;}
#endif

void smallut_init_mt();

#endif /* _SMALLUT_H_INCLUDED_ */
