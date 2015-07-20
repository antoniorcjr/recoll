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
#ifndef _RECOLL_H_INCLUDED_
#define _RECOLL_H_INCLUDED_

#include <string>

#include "rclconfig.h"
#include "rcldb.h"
#include "ptmutex.h"

#include <QString>

// Misc declarations in need of sharing between the UI files

// Open the database if needed. We now force a close/open by default
extern bool maybeOpenDb(std::string &reason, bool force = true, 
			bool *maindberror = 0);

/** Retrieve configured stemming languages */
bool getStemLangs(vector<string>& langs);

extern RclConfig *theconfig;

extern void rememberTempFile(TempFile);
extern void forgetTempFile(string &fn);

extern Rcl::Db *rcldb;
extern int recollNeedsExit;
extern void startManual(const string& helpindex);

extern void applyStyleSheet(const QString&);

#ifdef RCL_USE_ASPELL
class Aspell;
extern Aspell *aspell;
#endif

inline std::string qs2utf8s(const QString& qs)
{
    return std::string((const char *)qs.toUtf8());
}
#endif /* _RECOLL_H_INCLUDED_ */
