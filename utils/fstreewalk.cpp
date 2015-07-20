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
#ifdef HAVE_CONFIG_H
#include "autoconfig.h"
#endif

#ifndef TEST_FSTREEWALK

#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fnmatch.h>
#include <cstring>
#include <algorithm>

#include <sstream>
#include <vector>
#include <deque>
#include <set>

#include "cstr.h"
#include "debuglog.h"
#include "pathut.h"
#include "fstreewalk.h"

#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

bool FsTreeWalker::o_useFnmPathname = true;

const int FsTreeWalker::FtwTravMask = FtwTravNatural|
    FtwTravBreadth|FtwTravFilesThenDirs|FtwTravBreadthThenDepth;

class DirId {
public:
    dev_t dev;
    ino_t ino;
    DirId(dev_t d, ino_t i) : dev(d), ino(i) {}
    bool operator<(const DirId& r) const 
    {
	return dev < r.dev || (dev == r.dev && ino < r.ino);
    }
};

class FsTreeWalker::Internal {
public:
    Internal(int opts)
    : options(opts), depthswitch(4), maxdepth(-1), errors(0)
    {
    }
    int options;
    int depthswitch;
    int maxdepth;
    int basedepth;
    stringstream reason;
    vector<string> skippedNames;
    vector<string> skippedPaths;
    // When doing Breadth or FilesThenDirs traversal, we keep a list
    // of directory paths to be processed, and we do not recurse.
    deque<string> dirs;
    int errors;
    set<DirId> donedirs;
    void logsyserr(const char *call, const string &param) 
    {
	errors++;
	reason << call << "(" << param << ") : " << errno << " : " << 
	    strerror(errno) << endl;
    }
};

FsTreeWalker::FsTreeWalker(int opts)
{
    data = new Internal(opts);
}

FsTreeWalker::~FsTreeWalker()
{
    delete data;
}

void FsTreeWalker::setOpts(int opts)
{
    if (data) {
	data->options = opts;
    }
}
int FsTreeWalker::getOpts()
{
    if (data) {
	return data->options;
    } else {
	return 0;
    }
}
void FsTreeWalker::setDepthSwitch(int ds)
{
    if (data) {
        data->depthswitch = ds;
    }
}
void FsTreeWalker::setMaxDepth(int md)
{
    if (data) {
        data->maxdepth = md;
    }
}

string FsTreeWalker::getReason()
{
    string reason = data->reason.str();
    data->reason.str(string());
    data->errors = 0;
    return reason;
}

int FsTreeWalker::getErrCnt()
{
    return data->errors;
}

bool FsTreeWalker::addSkippedName(const string& pattern)
{
    if (find(data->skippedNames.begin(), 
	     data->skippedNames.end(), pattern) == data->skippedNames.end())
	data->skippedNames.push_back(pattern);
    return true;
}
bool FsTreeWalker::setSkippedNames(const vector<string> &patterns)
{
    data->skippedNames = patterns;
    return true;
}
bool FsTreeWalker::inSkippedNames(const string& name)
{
    for (vector<string>::const_iterator it = data->skippedNames.begin(); 
	 it != data->skippedNames.end(); it++) {
	if (fnmatch(it->c_str(), name.c_str(), 0) == 0) {
	    return true;
	}
    }
    return false;
}

bool FsTreeWalker::addSkippedPath(const string& ipath)
{
    string path = (data->options & FtwNoCanon) ? ipath : path_canon(ipath);
    if (find(data->skippedPaths.begin(), 
	     data->skippedPaths.end(), path) == data->skippedPaths.end())
	data->skippedPaths.push_back(path);
    return true;
}
bool FsTreeWalker::setSkippedPaths(const vector<string> &paths)
{
    data->skippedPaths = paths;
    for (vector<string>::iterator it = data->skippedPaths.begin();
	 it != data->skippedPaths.end(); it++)
        if (!(data->options & FtwNoCanon))
            *it = path_canon(*it);
    return true;
}
bool FsTreeWalker::inSkippedPaths(const string& path, bool ckparents)
{
    int fnmflags = o_useFnmPathname ? FNM_PATHNAME : 0;
#ifdef FNM_LEADING_DIR
    if (ckparents)
        fnmflags |= FNM_LEADING_DIR;
#endif

    for (vector<string>::const_iterator it = data->skippedPaths.begin(); 
	 it != data->skippedPaths.end(); it++) {
#ifndef FNM_LEADING_DIR
        if (ckparents) {
            string mpath = path;
            while (mpath.length() > 2) {
                if (fnmatch(it->c_str(), mpath.c_str(), fnmflags) == 0) 
                    return true;
                mpath = path_getfather(mpath);
            }
        } else 
#endif /* FNM_LEADING_DIR */
        if (fnmatch(it->c_str(), path.c_str(), fnmflags) == 0) {
            return true;
        }
    }
    return false;
}

static inline int slashcount(const string& p)
{
    int n = 0;
    for (unsigned int i = 0; i < p.size(); i++)
        if (p[i] == '/')
            n++;
    return n;
}

FsTreeWalker::Status FsTreeWalker::walk(const string& _top, 
					FsTreeWalkerCB& cb)
{
    string top = (data->options & FtwNoCanon) ? _top : path_canon(_top);

    if ((data->options & FtwTravMask) == 0) {
        data->options |= FtwTravNatural;
    }

    data->basedepth = slashcount(top); // Only used for breadthxx
    struct stat st;
    // We always follow symlinks at this point. Makes more sense.
    if (stat(top.c_str(), &st) == -1) {
	// Note that we do not return an error if the stat call
	// fails. A temp file may have gone away.
	data->logsyserr("stat", top);
	return errno == ENOENT ? FtwOk : FtwError;
    }

    // Recursive version, using the call stack to store state. iwalk
    // will process files and recursively descend into subdirs in
    // physical order of the current directory.
    if ((data->options & FtwTravMask) == FtwTravNatural) {
        return iwalk(top, &st, cb);
    }

    // Breadth first of filesThenDirs semi-depth first order
    // Managing queues of directories to be visited later, in breadth or
    // depth order. Null marker are inserted in the queue to indicate
    // father directory changes (avoids computing parents all the time).
    data->dirs.push_back(top);
    Status status;
    while (!data->dirs.empty()) {
        string dir, nfather;
        if (data->options & (FtwTravBreadth|FtwTravBreadthThenDepth)) {
            // Breadth first, pop and process an older dir at the
            // front of the queue. This will add any child dirs at the
            // back
            dir = data->dirs.front();
            data->dirs.pop_front();
            if (dir.empty()) {
                // Father change marker. 
                if (data->dirs.empty())
                    break;
                dir = data->dirs.front();
                data->dirs.pop_front();
                nfather = path_getfather(dir);
                if (data->options & FtwTravBreadthThenDepth) {
                    // Check if new depth warrants switch to depth first
                    // traversal (will happen on next loop iteration).
                    int curdepth = slashcount(dir) - data->basedepth;
                    if (curdepth >= data->depthswitch) {
                        //fprintf(stderr, "SWITCHING TO DEPTH FIRST\n");
                        data->options &= ~FtwTravMask;
                        data->options |= FtwTravFilesThenDirs;
                    }
                }
            }
        } else {
            // Depth first, pop and process latest dir
            dir = data->dirs.back();
            data->dirs.pop_back();
            if (dir.empty()) {
                // Father change marker. 
                if (data->dirs.empty())
                    break;
                dir = data->dirs.back();
                data->dirs.pop_back();
                nfather = path_getfather(dir);
            }
        }

        // If changing parent directory, advise our user.
        if (!nfather.empty()) {
            if (stat(nfather.c_str(), &st) == -1) {
                data->logsyserr("stat", nfather);
		return errno == ENOENT ? FtwOk : FtwError;
            }
            if ((status = cb.processone(nfather, &st, FtwDirReturn)) & 
                (FtwStop|FtwError)) {
                return status;
            }
        }

        if (stat(dir.c_str(), &st) == -1) {
            data->logsyserr("stat", dir);
	    return errno == ENOENT ? FtwOk : FtwError;
        }
        // iwalk will not recurse in this case, just process file entries
        // and append subdir entries to the queue.
        status = iwalk(dir, &st, cb);
        if (status != FtwOk)
            return status;
    }
    return FtwOk;
}

// Note that the 'norecurse' flag is handled as part of the directory read. 
// This means that we always go into the top 'walk()' parameter if it is a 
// directory, even if norecurse is set. Bug or Feature ?
FsTreeWalker::Status FsTreeWalker::iwalk(const string &top, 
					 struct stat *stp,
					 FsTreeWalkerCB& cb)
{
    Status status = FtwOk;
    bool nullpush = false;

    // Tell user to process the top entry itself
    if (S_ISDIR(stp->st_mode)) {
	if ((status = cb.processone(top, stp, FtwDirEnter)) & 
	    (FtwStop|FtwError)) {
	    return status;
	}
    } else if (S_ISREG(stp->st_mode)) {
	return cb.processone(top, stp, FtwRegular);
    } else {
	return status;
    }


    int curdepth = slashcount(top) - data->basedepth;
    if (data->maxdepth >= 0 && curdepth >= data->maxdepth) {
	LOGDEB1(("FsTreeWalker::iwalk: Maxdepth reached: [%s]\n", top.c_str()));
	return status;
    }

    // This is a directory, read it and process entries:

    // Detect if directory already seen. This could just be several
    // symlinks pointing to the same place (if FtwFollow is set), it
    // could also be some other kind of cycle. In any case, there is
    // no point in entering again.
    // For now, we'll ignore the "other kind of cycle" part and only monitor
    // this is FtwFollow is set
    if (data->options & FtwFollow) {
	DirId dirid(stp->st_dev, stp->st_ino);
	if (data->donedirs.find(dirid) != data->donedirs.end()) {
	    LOGINFO(("Not processing [%s] (already seen as other path)\n", 
		     top.c_str()));
	    return status;
	}
	data->donedirs.insert(dirid);
    }

    DIR *d = opendir(top.c_str());
    if (d == 0) {
	data->logsyserr("opendir", top);
	switch (errno) {
	case EPERM:
	case EACCES:
	case ENOENT:
	    goto out;
	default:
	    status = FtwError;
	    goto out;
	}
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != 0) {
        string fn;
        struct stat st;
	// Maybe skip dotfiles
	if ((data->options & FtwSkipDotFiles) && ent->d_name[0] == '.')
	    continue;
	// Skip . and ..
	if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) 
	    continue;

	// Skipped file names match ?
	if (!data->skippedNames.empty()) {
	    if (inSkippedNames(ent->d_name))
		continue;
	}

        fn = path_cat(top, ent->d_name);
        int statret = (data->options & FtwFollow) ? stat(fn.c_str(), &st) :
            lstat(fn.c_str(), &st);
        if (statret == -1) {
            data->logsyserr("stat", fn);
            continue;
        }
        if (!data->skippedPaths.empty()) {
            // We do not check the ancestors. This means that you can have
            // a topdirs member under a skippedPath, to index a portion of
            // an ignored area. This is the way it had always worked, but
            // this was broken by 1.13.00 and the systematic use of 
            // FNM_LEADING_DIR
            if (inSkippedPaths(fn, false))
                continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (data->options & FtwNoRecurse) {
                status = cb.processone(fn, &st, FtwDirEnter);
            } else {
                if (data->options & FtwTravNatural) {
                    status = iwalk(fn, &st, cb);
                } else {
                    // If first subdir, push marker to separate
                    // from entries for other dir. This is to help
                    // with generating DirReturn callbacks
                    if (!nullpush) {
                        if (!data->dirs.empty() && 
			    !data->dirs.back().empty())
                            data->dirs.push_back(cstr_null);
                        nullpush = true;
                    }
                    data->dirs.push_back(fn);
                    continue;
                }
            }
            // Note: only recursive case gets here.
            if (status & (FtwStop|FtwError))
                goto out;
            if (!(data->options & FtwNoRecurse)) 
                if ((status = cb.processone(top, &st, FtwDirReturn)) 
                    & (FtwStop|FtwError))
                    goto out;
        } else if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
            if ((status = cb.processone(fn, &st, FtwRegular)) & 
                (FtwStop|FtwError)) {
                goto out;
            }
        }
        // We ignore other file types (devices etc...)
    } // readdir loop

 out:
    if (d)
	closedir(d);
    return status;
}
	
#else // TEST_FSTREEWALK

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <iostream>

#include "rclinit.h"
#include "rclconfig.h"
#include "fstreewalk.h"

using namespace std;

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_p	  0x2 
#define OPT_P	  0x4 
#define OPT_r     0x8
#define OPT_c     0x10
#define OPT_b     0x20
#define OPT_d     0x40
#define OPT_m     0x80
#define OPT_L     0x100
#define OPT_w     0x200
#define OPT_M     0x400
#define OPT_D     0x800

class myCB : public FsTreeWalkerCB {
 public:
    FsTreeWalker::Status processone(const string &path, 
                                    const struct stat *st,
                                    FsTreeWalker::CbFlag flg)
    {
	if (flg == FsTreeWalker::FtwDirEnter) {
            if (op_flags & OPT_r) 
                cout << path << endl;
            else
                cout << "[Entering " << path << "]" << endl;
	} else if (flg == FsTreeWalker::FtwDirReturn) {
	    cout << "[Returning to " << path << "]" << endl;
	} else if (flg == FsTreeWalker::FtwRegular) {
	    cout << path << endl;
	}
	return FsTreeWalker::FtwOk;
    }
};

static const char *thisprog;

// Note that breadth first sorting is relatively expensive: less inode
// locality, more disk usage (and also more user memory usage, does
// not appear here). Some typical results on a real tree with 2.6
// million entries (220MB of name data)
// Recoll 1.13
// time trfstreewalk / > /data/tmp/old
// real    13m32.839s user    0m4.443s sys     0m31.128s
// 
// Recoll 1.14
// time trfstreewalk / > /data/tmp/nat;
// real    13m28.685s user    0m4.430s sys     0m31.083s
// time trfstreewalk -d / > /data/tmp/depth;
// real    13m30.051s user    0m4.140s sys     0m33.862s
// time trfstreewalk -m / > /data/tmp/mixed;
// real    14m53.245s user    0m4.244s sys     0m34.494s
// time trfstreewalk -b / > /data/tmp/breadth;
// real    17m10.585s user    0m4.532s sys     0m35.033s

static char usage [] =
"trfstreewalk [-p pattern] [-P ignpath] [-r] [-c] [-L] topdir\n"
" -r : norecurse\n"
" -c : no path canonification\n"
" -L : follow symbolic links\n"
" -b : use breadth first walk\n"
" -d : use almost depth first (dir files, then subdirs)\n"
" -m : use breadth up to 4 deep then switch to -d\n"
" -w : unset default FNM_PATHNAME when using fnmatch() to match skipped paths\n"
" -M <depth>: limit depth (works with -b/m/d)\n"
" -D : skip dotfiles\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

int main(int argc, const char **argv)
{
    vector<string> patterns;
    vector<string> paths;
    int maxdepth = -1;

    thisprog = argv[0];
    argc--; argv++;
    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'b':	op_flags |= OPT_b; break;
	    case 'c':	op_flags |= OPT_c; break;
	    case 'd':	op_flags |= OPT_d; break;
	    case 'D':	op_flags |= OPT_D; break;
	    case 'L':	op_flags |= OPT_L; break;
	    case 'm':	op_flags |= OPT_m; break;
	    case 'M':	op_flags |= OPT_M; if (argc < 2)  Usage();
		maxdepth = atoi(*(++argv));
		argc--; 
		goto b1;
	    case 'p':	op_flags |= OPT_p; if (argc < 2)  Usage();
		patterns.push_back(*(++argv));
		argc--; 
		goto b1;
	    case 'P':	op_flags |= OPT_P; if (argc < 2)  Usage();
		paths.push_back(*(++argv));
		argc--; 
		goto b1;
	    case 'r':	op_flags |= OPT_r; break;
	    case 'w':	op_flags |= OPT_w; break;
	    default: Usage();	break;
	    }
    b1: argc--; argv++;
    }

    if (argc != 1)
	Usage();
    string topdir = *argv++;argc--;

    int opt = 0;
    if (op_flags & OPT_r)
	opt |= FsTreeWalker::FtwNoRecurse;
    if (op_flags & OPT_c)
	opt |= FsTreeWalker::FtwNoCanon;
    if (op_flags & OPT_L)
	opt |= FsTreeWalker::FtwFollow;
    if (op_flags & OPT_D)
	opt |= FsTreeWalker::FtwSkipDotFiles;

    if (op_flags & OPT_b)
	opt |= FsTreeWalker::FtwTravBreadth;
    else if (op_flags & OPT_d)
	opt |= FsTreeWalker::FtwTravFilesThenDirs;
    else if (op_flags & OPT_m)
	opt |= FsTreeWalker::FtwTravBreadthThenDepth;

    string reason;
    if (!recollinit(0, 0, reason)) {
	fprintf(stderr, "Init failed: %s\n", reason.c_str());
	exit(1);
    }
    if (op_flags & OPT_w) {
	FsTreeWalker::setNoFnmPathname();
    }
    FsTreeWalker walker;
    walker.setOpts(opt); 
    walker.setMaxDepth(maxdepth);
    walker.setSkippedNames(patterns);
    walker.setSkippedPaths(paths);
    myCB cb;
    walker.walk(topdir, cb);
    if (walker.getErrCnt() > 0)
	cout << walker.getReason();
}

#endif // TEST_FSTREEWALK
