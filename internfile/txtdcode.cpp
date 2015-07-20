/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "autoconfig.h"

#include "cstr.h"
#include "transcode.h"
#include "mimehandler.h"
#include "debuglog.h"
#include "smallut.h"


// Called after decoding from utf-8 failed. Handle the common case
// where this is a good old 8bit-encoded text document left-over when
// the locale was switched to utf-8. We try to guess a charset
// according to the locale language and use it. This is a very rough
// heuristic, but may be better than discarding the data.
static bool alternate_decode(const string& in, string& out)
{
    string lang = localelang();
    string code = langtocode(lang);
    LOGDEB(("RecollFilter::txtdcode: trying alternate decode from %s\n",
	    code.c_str()));
    return transcode(in, out, code, cstr_utf8);
}

bool RecollFilter::txtdcode(const string& who)
{
    if (m_metaData[cstr_dj_keymt].compare(cstr_textplain)) {
	LOGERR(("%s::txtdcode: called on non txt/plain: %s\n", who.c_str(),
		m_metaData[cstr_dj_keymt].c_str()));
	return false;
    }

    string& ocs = m_metaData[cstr_dj_keyorigcharset];
    string& itext = m_metaData[cstr_dj_keycontent];
    LOGDEB1(("%s::txtdcode: %d bytes from [%s] to UTF-8\n", 
	     who.c_str(), itext.size(), ocs.c_str()));
    int ecnt;
    string otext;
    bool ret = transcode(itext, otext, ocs, cstr_utf8, &ecnt);
    if (!ret || ecnt > int(itext.size() / 100)) {
	LOGERR(("%s::txtdcode: transcode %d bytes to UTF-8 failed "
		"for input charset [%s] ret %d ecnt %d\n", 
		who.c_str(), itext.size(), ocs.c_str(), ret, ecnt));

	if (samecharset(ocs, cstr_utf8)) {
	    ret = alternate_decode(itext, otext);
	}
	if (!ret) {
	    itext.erase();
	    return false;
	}
    }

    itext.swap(otext);
    m_metaData[cstr_dj_keycharset] = cstr_utf8;
    return true;
}

