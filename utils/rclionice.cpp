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
#include <stdio.h>
#include <unistd.h>

#include <string>

#include "rclionice.h"
#include "execmd.h"
#include "debuglog.h"

using namespace std;

bool rclionice(const string& clss, const string& cdata)
{
    string ionicexe;
    if (!ExecCmd::which("ionice", ionicexe)) {
	// ionice not found, bail out
	LOGDEB0(("rclionice: ionice not found\n"));
	return false;
    }
    vector<string> args;
    args.push_back("-c");
    args.push_back(clss);

    if (!cdata.empty()) {
	args.push_back("-n");
	args.push_back(cdata);
    }
    
    char cpid[100];
    sprintf(cpid, "%d", getpid());
    args.push_back("-p");
    args.push_back(cpid);

    ExecCmd cmd;
    int status = cmd.doexec(ionicexe, args);

    if (status) {
	LOGERR(("rclionice: failed, status 0x%x\n", status));
	return false;
    }
    return true;
}
