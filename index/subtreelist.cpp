/* Copyright (C) 2007 J.F.Dockes
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

#ifndef TEST_SUBTREELIST

#include "cstr.h"
#include "refcntr.h"
#include "rcldb.h"
#include "searchdata.h"
#include "rclquery.h"
#include "subtreelist.h"
#include "debuglog.h"

bool subtreelist(RclConfig *config, const string& top, 
		 vector<string>& paths)
{
    LOGDEB(("subtreelist: top: [%s]\n", top.c_str()));
    Rcl::Db rcldb(config);
    if (!rcldb.open(Rcl::Db::DbRO)) {
	LOGERR(("subtreelist: can't open database in [%s]: %s\n",
		config->getDbDir().c_str(), rcldb.getReason().c_str()));
	return false;
    }

    Rcl::SearchData *sd = new Rcl::SearchData(Rcl::SCLT_OR, cstr_null);
    RefCntr<Rcl::SearchData> rq(sd);

    sd->addClause(new Rcl::SearchDataClausePath(top, false));

    Rcl::Query query(&rcldb);
    query.setQuery(rq);
    int cnt = query.getResCnt();

    for (int i = 0; i < cnt; i++) {
	Rcl::Doc doc;
	if (!query.getDoc(i, doc))
	    break;
	string path = fileurltolocalpath(doc.url);
	if (!path.empty())
	    paths.push_back(path);
    }
    return true;
}

#else // TEST


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <iostream>
#include <vector>
#include <string>
using namespace std;

#include "subtreelist.h"
#include "rclconfig.h"
#include "rclinit.h"

static char *thisprog;
static char usage [] =
" <path> : list document paths in this tree\n"
;
static void
Usage(void)
{
    cerr << thisprog <<  ": usage:" << endl << usage;
    exit(1);
}

static int     op_flags;
#define OPT_o     0x2 

int main(int argc, char **argv)
{
    string top;

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            default: Usage();   break;
            }
	argc--; argv++;
    }
    if (argc < 1)
	Usage();
    top = *argv++;argc--;
    string reason;
    RclConfig *config = recollinit(0, 0, reason, 0);
    if (!config || !config->ok()) {
	fprintf(stderr, "Recoll init failed: %s\n", reason.c_str());
	exit(1);
    }

    vector<string> paths;
    if (!subtreelist(config, top, paths)) {
	cerr << "subtreelist failed" << endl;
	exit(1);
    }
    for (vector<string>::const_iterator it = paths.begin(); 
	 it != paths.end(); it++) {
	cout << *it << endl;
    }
    exit(0);
}
#endif
