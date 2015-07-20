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

#ifndef TEST_TRANSCODE
#include "autoconfig.h"

#include <string>
#include <iostream>
#ifndef NO_NAMESPACES
using std::string;
#endif /* NO_NAMESPACES */

#include <errno.h>
#include <iconv.h>

#include "transcode.h"
#include "debuglog.h"
#include "ptmutex.h"
#ifdef RCL_ICONV_INBUF_CONST
#define ICV_P2_TYPE const char**
#else
#define ICV_P2_TYPE char**
#endif

// We gain approximately 25% exec time for word at a time conversions by
// caching the iconv_open thing. 
//
// We may also lose some concurrency on multiproc because of the
// necessary locking, but we only have one processing-intensive
// possible thread for now (the indexing one), so this is probably not
// an issue (and could be worked around with a slightly more
// sohisticated approach).
#define ICONV_CACHE_OPEN

bool transcode(const string &in, string &out, const string &icode,
	       const string &ocode, int *ecnt)
{
    LOGDEB2(("Transcode: %s -> %s\n", icode.c_str(), ocode.c_str()));
#ifdef ICONV_CACHE_OPEN
    static iconv_t ic = (iconv_t)-1;
    static string cachedicode;
    static string cachedocode;
    static PTMutexInit o_cachediconv_mutex;
    PTMutexLocker locker(o_cachediconv_mutex);
#else 
    iconv_t ic;
#endif
    bool ret = false;
    const int OBSIZ = 8192;
    char obuf[OBSIZ], *op;
    bool icopen = false;
    int mecnt = 0;
    out.erase();
    size_t isiz = in.length();
    out.reserve(isiz);
    const char *ip = in.c_str();

#ifdef ICONV_CACHE_OPEN
    if (cachedicode.compare(icode) || cachedocode.compare(ocode)) {
	if (ic != (iconv_t)-1) {
	    iconv_close(ic);
	    ic = (iconv_t)-1;
	}
#endif
	if((ic = iconv_open(ocode.c_str(), icode.c_str())) == (iconv_t)-1) {
	    out = string("iconv_open failed for ") + icode
		+ " -> " + ocode;
#ifdef ICONV_CACHE_OPEN
	    cachedicode.erase();
	    cachedocode.erase();
#endif
	    goto error;
	}

#ifdef ICONV_CACHE_OPEN
	cachedicode.assign(icode);
	cachedocode.assign(ocode);
    }
#endif

    icopen = true;

    while (isiz > 0) {
	size_t osiz;
	op = obuf;
	osiz = OBSIZ;

	if(iconv(ic, (ICV_P2_TYPE)&ip, &isiz, &op, &osiz) == (size_t)-1 && 
	   errno != E2BIG) {
#if 0
	    out.erase();
	    out = string("iconv failed for ") + icode + " -> " + ocode +
		" : " + strerror(errno);
#endif
	    if (errno == EILSEQ) {
		LOGDEB1(("transcode:iconv: bad input seq.: shift, retry\n"));
		LOGDEB1((" Input consumed %d output produced %d\n",
			 ip - in.c_str(), out.length() + OBSIZ - osiz));
		out.append(obuf, OBSIZ - osiz);
		out += "?";
		mecnt++;
		ip++;isiz--;
		continue;
	    }
	    // Normally only EINVAL is possible here: incomplete
	    // multibyte sequence at the end. This is not fatal. Any
	    // other is supposedly impossible, we return an error
	    if (errno == EINVAL)
		goto out;
	    else
		goto error;
	}

	out.append(obuf, OBSIZ - osiz);
    }

#ifndef ICONV_CACHE_OPEN
    icopen = false;
    if(iconv_close(ic) == -1) {
	out.erase();
	out = string("iconv_close failed for ") + icode + " -> " + ocode;
	goto error;
    }
#endif

out:
    ret = true;

error:

    if (icopen) {
#ifndef ICONV_CACHE_OPEN
	iconv_close(ic);
#else
	// Just reset conversion
        iconv(ic, 0, 0, 0, 0);
#endif
    }

    if (mecnt)
	LOGDEB(("transcode: [%s]->[%s] %d errors\n", 
		 icode.c_str(), ocode.c_str(), mecnt));
    if (ecnt)
	*ecnt = mecnt;
    return ret;
}


#else

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <string>
#include <iostream>


using namespace std;

#include "readfile.h"
#include "transcode.h"

// Repeatedly transcode a small string for timing measurements
static const string testword("\xc3\xa9\x6c\x69\x6d\x69\x6e\xc3\xa9\xc3\xa0");
// Without cache 10e6 reps on y -> 6.68
// With cache                   -> 4.73
// With cache and lock          -> 4.9
void looptest()
{
    cout << testword << endl;
    string out;
    for (int i = 0; i < 10*1000*1000; i++) {
	if (!transcode(testword, out, "UTF-8", "UTF-16BE")) {
	    cerr << "Transcode failed" << endl;
	    break;
	}
    }
}

int main(int argc, char **argv)
{
#if 0
    looptest();
    exit(0);
#endif
    if (argc != 5) {
	cerr << "Usage: transcode ifilename icode ofilename ocode" << endl;
	exit(1);
    }
    const string ifilename = argv[1];
    const string icode = argv[2];
    const string ofilename = argv[3];
    const string ocode = argv[4];

    string text;
    if (!file_to_string(ifilename, text)) {
	cerr << "Couldnt read file, errno " << errno << endl;
	exit(1);
    }
    string out;
    if (!transcode(text, out, icode, ocode)) {
	cerr << out << endl;
	exit(1);
    }
    int fd = open(ofilename.c_str(), O_CREAT|O_TRUNC|O_WRONLY, 0666);
    if (fd < 0) {
	perror("Open/create output");
	exit(1);
    }
    if (write(fd, out.c_str(), out.length()) != (int)out.length()) {
	perror("write");
	exit(1);
    }
    close(fd);
    exit(0);
}
#endif
