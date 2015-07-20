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

#include "rcldoc.h"
#include "debuglog.h"

namespace Rcl {
    const string Doc::keyabs("abstract");
    const string Doc::keyapptg("rclaptg");
    const string Doc::keyau("author");
    const string Doc::keybcknd("rclbes");
    const string Doc::keybght("beagleHitType");
    const string Doc::keycc("collapsecount");
    const string Doc::keychildurl("childurl");
    const string Doc::keydmt("dmtime");
    const string Doc::keyds("dbytes");
    const string Doc::keyfmt("fmtime");
    const string Doc::keyfn("filename");
    const string Doc::keytcfn("containerfilename");
    const string Doc::keyfs("fbytes");
    const string Doc::keyipt("ipath");
    const string Doc::keykw("keywords");
    const string Doc::keymd5("md5");
    const string Doc::keymt("mtime");
    const string Doc::keyoc("origcharset");
    const string Doc::keypcs("pcbytes");
    const string Doc::keyrr("relevancyrating");
    const string Doc::keysig("sig");
    const string Doc::keysz("size");
    const string Doc::keytp("mtype");
    const string Doc::keytt("title");
    const string Doc::keyudi("rcludi");
    const string Doc::keyurl("url");

    void Doc::dump(bool dotext) const
    {
        LOGDEB(("Rcl::Doc::dump: url: [%s]\n", url.c_str()));
        LOGDEB(("Rcl::Doc::dump: idxurl: [%s]\n", idxurl.c_str()));
        LOGDEB(("Rcl::Doc::dump: ipath: [%s]\n", ipath.c_str()));
        LOGDEB(("Rcl::Doc::dump: mimetype: [%s]\n", mimetype.c_str()));
        LOGDEB(("Rcl::Doc::dump: fmtime: [%s]\n", fmtime.c_str()));
        LOGDEB(("Rcl::Doc::dump: dmtime: [%s]\n", dmtime.c_str()));
        LOGDEB(("Rcl::Doc::dump: origcharset: [%s]\n", origcharset.c_str()));
        LOGDEB(("Rcl::Doc::dump: syntabs: [%d]\n", syntabs));
        LOGDEB(("Rcl::Doc::dump: pcbytes: [%s]\n", pcbytes.c_str()));
        LOGDEB(("Rcl::Doc::dump: fbytes: [%s]\n", fbytes.c_str()));
        LOGDEB(("Rcl::Doc::dump: dbytes: [%s]\n", dbytes.c_str()));
        LOGDEB(("Rcl::Doc::dump: sig: [%s]\n", sig.c_str()));
        LOGDEB(("Rcl::Doc::dump: pc: [%d]\n", pc));
        LOGDEB(("Rcl::Doc::dump: xdocid: [%lu]\n", (unsigned long)xdocid));
        for (map<string, string>::const_iterator it = meta.begin();
             it != meta.end(); it++) {
            LOGDEB(("Rcl::Doc::dump: meta[%s]: [%s]\n", 
                    (*it).first.c_str(), (*it).second.c_str()));
        }
        if (dotext)
            LOGDEB(("Rcl::Doc::dump: text: \n[%s]\n", text.c_str()));
    }
}

