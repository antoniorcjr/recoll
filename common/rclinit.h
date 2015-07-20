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
#ifndef _RCLINIT_H_INCLUDED_
#define _RCLINIT_H_INCLUDED_

#include <string>
#ifndef NO_NAMESPACES
using std::string;
#endif

class RclConfig;
/**
 * Initialize by reading configuration, opening log file, etc.
 * 
 * This must be called from the main thread before starting any others. It sets
 * up the global signal handling. other threads must call recoll_threadinit()
 * when starting.
 *
 * @param flags   misc modifiers
 * @param cleanup function to call before exiting (atexit)
 * @param sigcleanup function to call on terminal signal (INT/HUP...) This 
 *       should typically set a flag which tells the program (recoll, 
 *       recollindex etc.. to exit as soon as possible (after closing the db, 
 *       etc.). cleanup will then be called by exit().
 * @param reason in case of error: output string explaining things
 * @param argcnf Configuration directory name from the command line (overriding
 *               default and environment
 * @return the parsed configuration.
 */
enum RclInitFlags {RCLINIT_NONE=0, RCLINIT_DAEMON=1};
extern RclConfig *recollinit(RclInitFlags flags,
			     void (*cleanup)(void), void (*sigcleanup)(int), 
			     string &reason, const string *argcnf = 0);
inline RclConfig *recollinit(void (*cleanup)(void), void (*sigcleanup)(int), 
			     string &reason, const string *argcnf = 0) {
    return recollinit(RCLINIT_NONE, cleanup, sigcleanup, reason, argcnf);
}

// Threads need to call this to block signals.  
// The main thread handles all signals.
extern void recoll_threadinit();

// Check if main thread
extern bool recoll_ismainthread();

#endif /* _RCLINIT_H_INCLUDED_ */
