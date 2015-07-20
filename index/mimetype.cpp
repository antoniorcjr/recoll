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

#ifndef TEST_MIMETYPE
#include "autoconfig.h"

#include <sys/stat.h>

#include <ctype.h>
#include <string>
#include <list>
using namespace std;

#include "mimetype.h"
#include "debuglog.h"
#include "execmd.h"
#include "rclconfig.h"
#include "smallut.h"
#include "idfile.h"
#include "pxattr.h"

/// Identification of file from contents. This is called for files with
/// unrecognized extensions.
///
/// The system 'file' utility does not always work for us. For exemple
/// it will mistake mail folders for simple text files if there is no
/// 'Received' header, which would be the case, for exemple in a
/// 'Sent' folder. Also "file -i" does not exist on all systems, and
/// is quite costly to execute.
/// So we first call the internal file identifier, which currently
/// only knows about mail, but in which we can add the more
/// current/interesting file types.
/// As a last resort we execute 'file' (except if forbidden by config)

static string mimetypefromdata(RclConfig *cfg, const string &fn, bool usfc)
{
    // First try the internal identifying routine
    string mime = idFile(fn.c_str());

#ifdef USE_SYSTEM_FILE_COMMAND
    if (usfc && mime.empty()) {
	// Last resort: use "file -i", or its configured replacement.
	vector<string> cmd = create_vector<string>(FILE_PROG) ("-i") (fn);
        string scommand;
        if (cfg->getConfParam("systemfilecommand", scommand)) {
            stringToStrings(scommand, cmd);
            cmd.push_back(fn);
        }

	string result;
	if (!ExecCmd::backtick(cmd, result)) {
	    LOGERR(("mimetypefromdata: exec %s failed\n", FILE_PROG));
	    return string();
	}
	LOGDEB2(("mimetype: [%s] \"file\" output [%s]\n", 
		 result.c_str(), fn.c_str()));
	
	// The normal output from "file -i" looks like the following:
	//   thefilename.xxx: text/plain; charset=us-ascii
	// Sometimes the semi-colon is missing like in:
	//     mimetype.cpp: text/x-c charset=us-ascii
	// And sometimes we only get the mime type. This apparently happens
	// when 'file' believes that the file name is binary

	trimstring(result, " \t\n\r");

	// If there is no colon and there is a slash, this is hopefuly
	// the mime type
	if (result.find_first_of(":") == string::npos && 
	    result.find_first_of("/") != string::npos) {
	    return result;
	}

	// Else the result should begin with the file name. Get rid of it:
	if (result.find(fn) != 0) {
	    // Garbage "file" output. Maybe the result of a charset
	    // conversion attempt?
	    LOGERR(("mimetype: can't interpret 'file' output: [%s]\n",
		    result.c_str()));
	    return string();
	}
	result = result.substr(fn.size());

	// Now should look like ": text/plain; charset=us-ascii"
	// Split it, and take second field
	list<string> res;
	stringToStrings(result, res);
	if (res.size() <= 1)
	    return string();
	list<string>::iterator it = res.begin();
	mime = *++it;
	// Remove possible semi-colon at the end
	trimstring(mime, " \t;");

	// File -i will sometimes return strange stuff (ie: "very small file")
	if(mime.find("/") == string::npos) 
	    mime.clear();
    }
#endif //USE_SYSTEM_FILE_COMMAND

    return mime;
}

/// Guess mime type, first from suffix, then from file data. We also
/// have a list of suffixes that we don't touch at all.
string mimetype(const string &fn, const struct stat *stp,
		RclConfig *cfg, bool usfc)
{
    // Use stat data if available to check for non regular files
    if (stp) {
	// Note: the value used for directories is different from what
	// file -i would print on Linux (inode/directory). Probably
	// comes from bsd. Thos may surprise a user trying to use a
	// 'mime:' filter with the query language, but it's not work
	// changing (would force a reindex).
	if (S_ISDIR(stp->st_mode))
	    return "inode/directory";
	if (S_ISLNK(stp->st_mode))
	    return "inode/symlink";
	if (!S_ISREG(stp->st_mode))
	    return "inode/x-fsspecial";
    }

    string mtype;

    // Extended attribute has priority on everything, as per:
    // http://freedesktop.org/wiki/CommonExtendedAttributes
    if (pxattr::get(fn, "mime_type", &mtype)) {
	LOGDEB0(("Mimetype: 'mime_type' xattr : [%s]\n", mtype.c_str()));
	if (mtype.empty()) {
	    LOGDEB0(("Mimetype: getxattr() returned empty mime type !\n"));
	} else {
	    return mtype;
	}
    }

    if (cfg == 0)  {
	LOGERR(("Mimetype: null config ??\n"));
	return mtype;
    }

    if (cfg->inStopSuffixes(fn)) {
	LOGDEB(("mimetype: fn [%s] in stopsuffixes\n", fn.c_str()));
	return mtype;
    }

    // Compute file name suffix and search the mimetype map
    string::size_type dot = fn.find_first_of(".");
    while (dot != string::npos) {
	string suff = stringtolower(fn.substr(dot));
	mtype = cfg->getMimeTypeFromSuffix(suff);
	if (!mtype.empty() || dot >= fn.size() - 1)
	    break;
	dot = fn.find_first_of(".", dot + 1);
    }

    // If type was not determined from suffix, examine file data. Can
    // only do this if we have an actual file (as opposed to a pure
    // name).
    if (mtype.empty() && stp)
	mtype = mimetypefromdata(cfg, fn, usfc);

 out:
    return mtype;
}


#else // TEST->

#include <stdio.h>
#include <sys/stat.h>

#include <cstdlib>
#include <iostream>

#include "debuglog.h"
#include "rclconfig.h"
#include "rclinit.h"
#include "mimetype.h"

using namespace std;
int main(int argc, const char **argv)
{
    string reason;
    RclConfig *config = recollinit(0, 0, reason);

    if (config == 0 || !config->ok()) {
	string str = "Configuration problem: ";
	str += reason;
	fprintf(stderr, "%s\n", str.c_str());
	exit(1);
    }

    while (--argc > 0) {
	string filename = *++argv;
	struct stat st;
	if (lstat(filename.c_str(), &st)) {
	    fprintf(stderr, "Can't stat %s\n", filename.c_str());
	    continue;
	}
	cout << filename << " -> " << 
	    mimetype(filename, &st, config, true) << endl;

    }
    return 0;
}


#endif // TEST
