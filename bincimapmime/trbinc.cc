/* Copyright (C) 2006 J.F.Dockes */
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include <sstream>

#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

#include "mime.h"

static char *thisprog;

static char usage [] =
    "trbinc <mboxfile> \n\n"
    ;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_s	  0x2 
#define OPT_b	  0x4 

#define DEFCOUNT 10

const char *hnames[] = {"Subject", "Content-type"};
int nh = sizeof(hnames) / sizeof(char *);

int main(int argc, char **argv)
{
    int count = DEFCOUNT;
    
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 's':	op_flags |= OPT_s; break;
	    case 'b':	op_flags |= OPT_b; if (argc < 2)  Usage();
		if ((sscanf(*(++argv), "%d", &count)) != 1) 
		    Usage(); 
		argc--; 
		goto b1;
	    default: Usage();	break;
	    }
    b1: argc--; argv++;
    }

    if (argc != 1)
	Usage();

    char *mfile = *argv++;argc--;
    int fd;
    if ((fd = open(mfile, 0)) < 0) {
	perror("Opening");
	exit(1);
    }
    Binc::MimeDocument doc;

#if 0
    doc.parseFull(fd);
#else
    char *cp;
    int size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, 0);
    fprintf(stderr, "Size: %d\n", size);
    cp = (char *)malloc(size);
    if (cp==0) {
	fprintf(stderr, "Malloc %d failed\n", size);
	exit(1);
    }
    int n;
    if ((n=read(fd, cp, size)) != size) {
	fprintf(stderr, "Read failed: requested %d, got %d\n", size, n);
	exit(1);
    }
    std::stringstream s(string(cp, size), ios::in);
    doc.parseFull(s);
#endif

    if (!doc.isHeaderParsed() && !doc.isAllParsed()) {
	fprintf(stderr, "Parse error\n");
	exit(1);
    }
    close(fd);
    Binc::HeaderItem hi;
    for (int i = 0; i < nh ; i++) {
	if (!doc.h.getFirstHeader(hnames[i], hi)) {
	    fprintf(stderr, "No %s\n", hnames[i]);
	    exit(1);
	}
	printf("%s: %s\n", hnames[i], hi.getValue().c_str());
    }
    exit(0);
}
