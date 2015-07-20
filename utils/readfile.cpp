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
#ifndef TEST_READFILE
#include "autoconfig.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef O_STREAMING
#define O_STREAMING 0
#endif
#include <errno.h>
#include <cstdio>
#include <cstring>

#include <string>

#ifndef NO_NAMESPACES
using std::string;
#endif /* NO_NAMESPACES */

#include "readfile.h"
#include "smallut.h"

#ifndef MIN
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#endif

class FileToString : public FileScanDo {
public:
    FileToString(string& data) : m_data(data) {}
    string& m_data;
    bool init(size_t size, string *reason) {
	if (size > 0)
	    m_data.reserve(size); 
	return true;
    }
    bool data(const char *buf, int cnt, string *reason) {
	try {
	    m_data.append(buf, cnt);
	} catch (...) {
	    catstrerror(reason, "append", errno);
	    return false;
	}
	return true;
    }
};

bool file_to_string(const string &fn, string &data, string *reason)
{
    return file_to_string(fn, data, 0, size_t(-1), reason);
}
bool file_to_string(const string &fn, string &data, off_t offs, size_t cnt,
                    string *reason)
{
    FileToString accum(data);
    return file_scan(fn, &accum, offs, cnt, reason);
}

bool file_scan(const string &fn, FileScanDo* doer, string *reason)
{
    return file_scan(fn, doer, 0, size_t(-1), reason);
}

const int RDBUFSZ = 4096;
// Note: the fstat() + reserve() (in init()) calls divide cpu usage almost by 2
// on both linux i586 and macosx (compared to just append())
// Also tried a version with mmap, but it's actually slower on the mac and not
// faster on linux.
bool file_scan(const string &fn, FileScanDo* doer, off_t startoffs, 
               size_t cnttoread, string *reason)
{
    bool ret = false;
    bool noclosing = true;
    int fd = 0;
    struct stat st;
    // Initialize st_size: if fn.empty() , the fstat() call won't happen. 
    st.st_size = 0;

    // If we have a file name, open it, else use stdin.
    if (!fn.empty()) {
	fd = open(fn.c_str(), O_RDONLY|O_STREAMING);
	if (fd < 0 || fstat(fd, &st) < 0) {
	    catstrerror(reason, "open/stat", errno);
	    return false;
	}
	noclosing = false;
    }

    if (cnttoread != (size_t)-1 && cnttoread) {
	doer->init(cnttoread+1, reason);
    } else if (st.st_size > 0) {
	doer->init(st.st_size+1, reason);
    } else {
	doer->init(0, reason);
    }

    off_t curoffs = 0;
    if (startoffs > 0 && !fn.empty()) {
        if (lseek(fd, startoffs, SEEK_SET) != startoffs) {
            catstrerror(reason, "lseek", errno);
            return false;
        }
        curoffs = startoffs;
    }

    char buf[RDBUFSZ];
    size_t totread = 0;
    for (;;) {
        size_t toread = RDBUFSZ;
        if (startoffs > 0 && curoffs < startoffs) {
            toread = MIN(RDBUFSZ, startoffs - curoffs);
        }

        if (cnttoread != size_t(-1)) {
            toread = MIN(toread, cnttoread - totread);
        }
	int n = read(fd, buf, toread);
	if (n < 0) {
	    catstrerror(reason, "read", errno);
	    goto out;
	}
	if (n == 0)
	    break;

        curoffs += n;
        if (curoffs - n < startoffs) 
            continue;
            
	if (!doer->data(buf, n, reason)) {
	    goto out;
	}
        totread += n;
        if (cnttoread > 0 && totread >= cnttoread) 
            break;
    }

    ret = true;
 out:
    if (fd >= 0 && !noclosing)
	close(fd);
    return ret;
}

#else // Test
#include "autoconfig.h"

#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <string>
#include <iostream>
using namespace std;

#include "readfile.h"
#include "fstreewalk.h"

using namespace std;

class myCB : public FsTreeWalkerCB {
 public:
    FsTreeWalker::Status processone(const string &path, 
				    const struct stat *st,
				    FsTreeWalker::CbFlag flg)
    {
	if (flg == FsTreeWalker::FtwDirEnter) {
	    //cout << "[Entering " << path << "]" << endl;
	} else if (flg == FsTreeWalker::FtwDirReturn) {
	    //cout << "[Returning to " << path << "]" << endl;
	} else if (flg == FsTreeWalker::FtwRegular) {
	    //cout << path << endl;
	    string s, reason;
	    if (!file_to_string(path, s, &reason)) {
		cerr << "Failed: " << reason << " : " << path << endl;
	    } else {
		//cout << 
		//"================================================" << endl;
		cout << path << endl;
		//		cout << s;
	    }
	    reason.clear();
	}
	return FsTreeWalker::FtwOk;
    }
};

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_c     0x2
#define OPT_o     0x4

static const char *thisprog;
static char usage [] =
"trreadfile [-o offs] [-c cnt] topdirorfile\n\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

int main(int argc, const char **argv)
{
    off_t offs = 0;
    size_t cnt = size_t(-1);
    thisprog = argv[0];
    argc--; argv++;

  while (argc > 0 && **argv == '-') {
    (*argv)++;
    if (!(**argv))
      /* Cas du "adb - core" */
      Usage();
    while (**argv)
      switch (*(*argv)++) {
      case 'c':	op_flags |= OPT_c; if (argc < 2)  Usage();
	  cnt = atoll(*(++argv)); argc--; 
	goto b1;
      case 'o':	op_flags |= OPT_o; if (argc < 2)  Usage();
	  offs = strtoull(*(++argv), 0, 0); argc--; 
	goto b1;
      default: Usage();	break;
      }
  b1: argc--; argv++;
  }

  if (argc != 1)
    Usage();
  string top = *argv++;argc--;
  cerr << "filename " << top << " offs " << offs << " cnt " << cnt << endl;

  struct stat st;
  if (!top.empty() && stat(top.c_str(), &st) < 0) {
      perror("stat");
      exit(1);
  }
  if (!top.empty() && S_ISDIR(st.st_mode)) {
      FsTreeWalker walker;
      myCB cb;
      walker.walk(top, cb);
      if (walker.getErrCnt() > 0)
	  cout << walker.getReason();
  } else {
      string s, reason;
      if (!file_to_string(top, s, offs, cnt, &reason)) {
	  cerr << reason << endl;
	  exit(1);
      } else {
	  cout << s;
      }
  }
  exit(0);
}
#endif //TEST_READFILE
