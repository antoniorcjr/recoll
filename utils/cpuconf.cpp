/* Copyright (C) 2013 J.F.Dockes
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

#ifndef TEST_CPUCONF

#include "autoconfig.h"
#include "cpuconf.h"
#include "execmd.h"
#include "smallut.h"

#if defined(__gnu_linux__) 
bool getCpuConf(CpuConf& conf)
{
    vector<string> cmdv = create_vector<string>("sh")("-c")
	("egrep ^processor /proc/cpuinfo | wc -l");

    string result;
    if (!ExecCmd::backtick(cmdv, result))
	return false;
    conf.ncpus = atoi(result.c_str());
    if (conf.ncpus < 1 || conf.ncpus > 100)
	conf.ncpus = 1;
    return true;
}

#elif defined(__FreeBSD__) 
bool getCpuConf(CpuConf& conf)
{
    vector<string> cmdv = create_vector<string>("sysctl")("hw.ncpu");

    string result;
    if (!ExecCmd::backtick(cmdv, result))
	return false;
    conf.ncpus = atoi(result.c_str());
    if (conf.ncpus < 1 || conf.ncpus > 100)
	conf.ncpus = 1;
    return true;
}
//#elif defined(__APPLE__)

#else // Any other system

// Generic, pretend there is one
bool getCpuConf(CpuConf& cpus)
{
    cpus.ncpus = 1;
    return true;
}
#endif


#else // TEST_CPUCONF

#include <stdlib.h>

#include <iostream>
using namespace std;

#include "cpuconf.h"

// Test driver
int main(int argc, const char **argv)
{
    CpuConf cpus;
    if (!getCpuConf(cpus)) {
	cerr << "getCpuConf failed" << endl;
	exit(1);
    }
    cout << "Cpus: " << cpus.ncpus << endl;
    exit(0);
}
#endif // TEST_CPUCONF
