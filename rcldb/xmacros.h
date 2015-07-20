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

#ifndef _xmacros_h_included_
#define _xmacros_h_included_

// Generic Xapian exception catching code. We do this quite often,
// and I have no idea how to do this except for a macro
#define XCATCHERROR(MSG) \
 catch (const Xapian::Error &e) {		   \
    MSG = e.get_msg();				   \
    if (MSG.empty()) MSG = "Empty error message";  \
 } catch (const std::string &s) {		   \
    MSG = s;					   \
    if (MSG.empty()) MSG = "Empty error message";  \
 } catch (const char *s) {			   \
    MSG = s;					   \
    if (MSG.empty()) MSG = "Empty error message";  \
 } catch (...) {				   \
    MSG = "Caught unknown xapian exception";	   \
 } 

#define XAPTRY(STMTTOTRY, XAPDB, ERSTR)       \
    for (int tries = 0; tries < 2; tries++) { \
	try {                                 \
            STMTTOTRY;                        \
            ERSTR.erase();                    \
            break;                            \
	} catch (const Xapian::DatabaseModifiedError &e) { \
            ERSTR = e.get_msg();                           \
	    XAPDB.reopen();                                \
            continue;                                      \
	} XCATCHERROR(ERSTR);                              \
        break;                                             \
    }

#endif
