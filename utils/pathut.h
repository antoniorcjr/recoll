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
#ifndef _PATHUT_H_INCLUDED_
#define _PATHUT_H_INCLUDED_
#include <unistd.h>

#include <string>
#include <vector>
#include <set>

#include "refcntr.h"

/// Add a / at the end if none there yet.
extern void path_catslash(std::string &s);
/// Concatenate 2 paths
extern std::string path_cat(const std::string &s1, const std::string &s2);
/// Get the simple file name (get rid of any directory path prefix
extern std::string path_getsimple(const std::string &s);
/// Simple file name + optional suffix stripping
extern std::string path_basename(const std::string &s, 
				 const std::string &suff = std::string());
/// Component after last '.'
extern std::string path_suffix(const std::string &s);
/// Get the father directory
extern std::string path_getfather(const std::string &s);
/// Get the current user's home directory
extern std::string path_home();
/// Expand ~ at the beginning of std::string 
extern std::string path_tildexpand(const std::string &s);
/// Use getcwd() to make absolute path if needed. Beware: ***this can fail***
/// we return an empty path in this case.
extern std::string path_absolute(const std::string &s);
/// Clean up path by removing duplicated / and resolving ../ + make it absolute
extern std::string path_canon(const std::string &s, const std::string *cwd=0);
/// Use glob(3) to return the file names matching pattern inside dir
extern std::vector<std::string> path_dirglob(const std::string &dir, 
				   const std::string pattern);
/// Encode according to rfc 1738
extern std::string url_encode(const std::string& url, 
			      std::string::size_type offs = 0);
/// Transcode to utf-8 if possible or url encoding, for display.
extern bool printableUrl(const std::string &fcharset, 
			 const std::string &in, std::string &out);
//// Convert to file path if url is like file://. This modifies the
//// input (and returns a copy for convenience)
extern std::string fileurltolocalpath(std::string url);
/// Test for file:/// url
extern bool urlisfileurl(const std::string& url);
/// 
extern std::string url_parentfolder(const std::string& url);

/// Return the host+path part of an url. This is not a general
/// routine, it does the right thing only in the recoll context
extern std::string url_gpath(const std::string& url);

/// Stat parameter and check if it's a directory
extern bool path_isdir(const std::string& path);

/// Dump directory
extern bool readdir(const std::string& dir, std::string& reason, 
		    std::set<std::string>& entries);

/** A small wrapper around statfs et al, to return percentage of disk
    occupation */
bool fsocc(const std::string &path, int *pc, // Percent occupied
	   long long *avmbs = 0 // Mbs available to non-superuser. Mb=1024*1024
	   );

/// Retrieve the temp dir location: $RECOLL_TMPDIR else $TMPDIR else /tmp
extern const std::string& tmplocation();

/// Create temporary directory (inside the temp location)
extern bool maketmpdir(std::string& tdir, std::string& reason);

/// mkdir -p
extern bool makepath(const std::string& path);

/// Temporary file class
class TempFileInternal {
public:
    TempFileInternal(const std::string& suffix);
    ~TempFileInternal();
    const char *filename() 
    {
	return m_filename.c_str();
    }
    const std::string &getreason() 
    {
	return m_reason;
    }
    void setnoremove(bool onoff)
    {
	m_noremove = onoff;
    }
    bool ok() 
    {
	return !m_filename.empty();
    }
private:
    std::string m_filename;
    std::string m_reason;
    bool m_noremove;
};

typedef RefCntr<TempFileInternal> TempFile;

/// Temporary directory class. Recursively deleted by destructor.
class TempDir {
public:
    TempDir();
    ~TempDir();
    const char *dirname() {return m_dirname.c_str();}
    const std::string &getreason() {return m_reason;}
    bool ok() {return !m_dirname.empty();}
    /// Recursively delete contents but not self.
    bool wipe();
private:
    std::string m_dirname;
    std::string m_reason;
    TempDir(const TempDir &) {}
    TempDir& operator=(const TempDir &) {return *this;};
};

/// Lock/pid file class. This is quite close to the pidfile_xxx
/// utilities in FreeBSD with a bit more encapsulation. I'd have used
/// the freebsd code if it was available elsewhere
class Pidfile {
public:
    Pidfile(const std::string& path)	: m_path(path), m_fd(-1) {}
    ~Pidfile();
    /// Open/create the pid file.
    /// @return 0 if ok, > 0 for pid of existing process, -1 for other error.
    pid_t open();
    /// Write pid into the pid file
    /// @return 0 ok, -1 error
    int write_pid();
    /// Close the pid file (unlocks)
    int close();
    /// Delete the pid file
    int remove();
    const std::string& getreason() {return m_reason;}
private:
    std::string m_path;
    int    m_fd;
    std::string m_reason;
    pid_t read_pid();
    int flopen();
};



// Freedesktop thumbnail standard path routine
// On return, path will have the appropriate value in all cases,
// returns true if the file already exists
extern bool thumbPathForUrl(const std::string& url, int size, std::string& path);

// Must be called in main thread before starting other threads
extern void pathut_init_mt();

#endif /* _PATHUT_H_INCLUDED_ */
