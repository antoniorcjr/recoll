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

//
// Small test program to evaluate the cost of using mutex locks: calls
// to methods doing a small (150 bytes) base64 encoding job + string
// manips, with and without locking.  The performance cost is
// negligible on all machines I tested (around 0.3% to 2% depending on
// the system and machine), but not inexistent, you would not want
// this in a tight loop.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
using namespace std;

#include "ptmutex.h"
#include "base64.h"

static char *thisprog;
static char usage [] =
"ptmutex [-l] count\n"
"\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_l	  0x2 

static const string convertbuffer = 
    "* The recoll GUI program sometimes crashes when running a query while\
       the indexing thread is active. Possible workarounds:";

static PTMutexInit o_lock;
void workerlock(string& out)
{
    PTMutexLocker locker(o_lock);
    base64_encode(convertbuffer, out);
}

void workernolock(string& out)
{
    base64_encode(convertbuffer, out);
}

int main(int argc, char **argv)
{
    int count = 0;
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'l':	op_flags |= OPT_l; break;
	    default: Usage();	break;
	    }
    b1: argc--; argv++;
    }

    if (argc != 1)
	Usage();
    count = atoi(*argv++);argc--;

    if (op_flags & OPT_l) {
	fprintf(stderr, "Looping %d, locking\n", count);
	for (int i = 0; i < count; i++) {
	    string s;
	    workerlock(s);
	}
    } else {
	fprintf(stderr, "Looping %d, no locking\n", count);
	for (int i = 0; i < count; i++) {
	    string s;
	    workernolock(s);
	}
    }
    exit(0);
}

