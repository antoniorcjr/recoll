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
#ifndef TEST_COPYFILE
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <utime.h>

#include <cstring>

#include "copyfile.h"
#include "debuglog.h"

using namespace std;

#define CPBSIZ 8192

bool copyfile(const char *src, const char *dst, string &reason, int flags)
{
  int sfd = -1;
  int dfd = -1;
  bool ret = false;
  char buf[CPBSIZ];
  int oflags = O_WRONLY|O_CREAT|O_TRUNC;

  LOGDEB(("copyfile: %s to %s\n", src, dst));

  if ((sfd = open(src, O_RDONLY)) < 0) {
      reason += string("open ") + src + ": " + strerror(errno);
      goto out;
  }

  if (flags & COPYFILE_EXCL) {
      oflags |= O_EXCL;
  }

  if ((dfd = open(dst, oflags, 0644)) < 0) {
      reason += string("open/creat ") + dst + ": " + strerror(errno);
      // If we fail because of an open/truncate error, we do not want to unlink
      // the file, we might succeed...
      flags |= COPYFILE_NOERRUNLINK;
      goto out;
  }

  for (;;) {
      int didread;
      didread = read(sfd, buf, CPBSIZ);
      if (didread < 0) {
	  reason += string("read src ") + src + ": " + strerror(errno);
	  goto out;
      }
      if (didread == 0)
	  break;
      if (write(dfd, buf, didread) != didread) {
	  reason += string("write dst ") + src + ": " + strerror(errno);
	  goto out;
      }
  }

  ret = true;
 out:
  if (ret == false && !(flags&COPYFILE_NOERRUNLINK))
    unlink(dst);
  if (sfd >= 0)
    close(sfd);
  if (dfd >= 0)
    close(dfd);
  return ret;
}

bool renameormove(const char *src, const char *dst, string &reason)
{
    // First try rename(2). If this succeeds we're done. If this fails
    // with EXDEV, try to copy. Unix really should have a library function
    // for this.
    if (rename(src, dst) == 0) {
        return true;
    }
    if (errno != EXDEV) {
        reason += string("rename(2) failed: ") + strerror(errno);
        return false;
    } 

    struct stat st;
    if (stat(src, &st) < 0) {
        reason += string("Can't stat ") + src + " : " + strerror(errno);
        return false;
    }
    if (!copyfile(src, dst, reason))
        return false;

    struct stat st1;
    if (stat(dst, &st1) < 0) {
        reason += string("Can't stat ") + dst + " : " + strerror(errno);
        return false;
    }

    // Try to preserve modes, owner, times. This may fail for a number
    // of reasons
    if ((st1.st_mode & 0777) != (st.st_mode & 0777)) {
        if (chmod(dst, st.st_mode&0777) != 0) {
	    reason += string("Chmod ") + dst + "Error : " + strerror(errno);
	}
    }
    if (st.st_uid != st1.st_uid || st.st_gid != st1.st_gid) {
        if (chown(dst, st.st_uid, st.st_gid) != 0) {
	    reason += string("Chown ") + dst + "Error : " + strerror(errno);
	}
    }
    struct timeval times[2];
    times[0].tv_sec = st.st_atime;
    times[0].tv_usec = 0;
    times[1].tv_sec = st.st_mtime;
    times[1].tv_usec = 0;
    utimes(dst, times);

    // All ok, get rid of origin
    if (unlink(src) < 0) {
        reason += string("Can't unlink ") + src + "Error : " + strerror(errno);
    }

    return true;
}


#else 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <iostream>

#include "copyfile.h"

using namespace std;

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_m     0x2
#define OPT_e     0x4

static const char *thisprog;
static char usage [] =
"trcopyfile [-m] src dst\n"
" -m : move instead of copying\n"
" -e : fail if dest exists (only for copy)\n"
"\n"
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
            case 'm':	op_flags |= OPT_m; break;
            case 'e':	op_flags |= OPT_e; break;
            default: Usage();	break;
            }
        argc--; argv++;
    }

    if (argc != 2)
        Usage();
    string src = *argv++;argc--;
    string dst = *argv++;argc--;
    bool ret;
    string reason;
    if (op_flags & OPT_m) {
        ret = renameormove(src.c_str(), dst.c_str(), reason);
    } else {
        int flags = 0;
        if (op_flags & OPT_e) {
            flags |= COPYFILE_EXCL;
        }
        ret = copyfile(src.c_str(), dst.c_str(), reason, flags);
    }
    if (!ret) {
        cerr << reason << endl;
        exit(1);
    }  else {
	cout << "Succeeded" << endl;
	if (!reason.empty()) {
	    cout << "Warnings: " << reason << endl;
	}
	exit(0);
    }
}

#endif
