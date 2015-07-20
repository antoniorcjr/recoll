/* Copyright (C) 2004 J.F.Dockes
 *
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

#ifndef TEST_WIPEDIR
#include "autoconfig.h"

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#include <cstring>
#include <string>
#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

#include "debuglog.h"
#include "pathut.h"
#include "wipedir.h"

int wipedir(const string& dir, bool selfalso, bool recurse)
{
    struct stat st;
    int statret;
    int ret = -1;

    statret = lstat(dir.c_str(), &st);
    if (statret == -1) {
	LOGERR(("wipedir: cant stat %s, errno %d\n", dir.c_str(), errno));
	return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
	LOGERR(("wipedir: %s not a directory\n", dir.c_str()));
	return -1;
    }

    if (access(dir.c_str(), R_OK|W_OK|X_OK) < 0) {
	LOGERR(("wipedir: no write access to %s\n", dir.c_str()));
	return -1;
    }

    DIR *d = opendir(dir.c_str());
    if (d == 0) {
	LOGERR(("wipedir: cant opendir %s, errno %d\n", dir.c_str(), errno));
	return -1;
    }
    int remaining = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != 0) {
	if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) 
	    continue;

	string fn = path_cat(dir, ent->d_name);

	struct stat st;
	int statret = lstat(fn.c_str(), &st);
	if (statret == -1) {
	    LOGERR(("wipedir: cant stat %s, errno %d\n", fn.c_str(), errno));
	    goto out;
	}
	if (S_ISDIR(st.st_mode)) {
	    if (recurse) {
		int rr = wipedir(fn, true, true);
		if (rr == -1) 
		    goto out;
		else 
		    remaining += rr;
	    } else {
		remaining++;
	    }
	} else {
	    if (unlink(fn.c_str()) < 0) {
		LOGERR(("wipedir: cant unlink %s, errno %d\n", 
			fn.c_str(), errno));
		goto out;
	    }
	}
    }

    ret = remaining;
    if (selfalso && ret == 0) {
	if (rmdir(dir.c_str()) < 0) {
	    LOGERR(("wipedir: rmdir(%s) failed, errno %d\n",
		    dir.c_str(), errno));
	    ret = -1;
	}
    }

 out:
    if (d)
	closedir(d);
    return ret;
}


#else // FILEUT_TEST

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "wipedir.h"

using namespace std;
static const char *thisprog;

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_r	  0x2 
#define OPT_s	  0x4 
static char usage [] =
"wipedir [-r -s] topdir\n"
" -r : recurse\n"
" -s : also delete topdir\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}
int main(int argc, const char **argv)
{
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'r':	op_flags |= OPT_r; break;
	    case 's':	op_flags |= OPT_s; break;
	    default: Usage();	break;
	    }
    b1: argc--; argv++;
    }

    if (argc != 1)
	Usage();

    string dir = *argv++;argc--;

    bool topalso = ((op_flags&OPT_s) != 0);
    bool recurse = ((op_flags&OPT_r) != 0);
    int cnt = wipedir(dir, topalso, recurse);
    printf("wipedir returned %d\n", cnt);
    exit(0);
}

#endif
