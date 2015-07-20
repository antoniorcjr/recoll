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

#ifndef _CONFLINKRCL_H_INCLUDED_
#define _CONFLINKRCL_H_INCLUDED_

/** 
 * A Gui-to-Data link class for ConfTree
 * Has a subkey pointer member which makes it easy to change the
 * current subkey for a number at a time.
 */
#include "confgui.h"
#include "conftree.h"
#include "debuglog.h"

namespace confgui {

class ConfLinkRclRep : public ConfLinkRep {
public:
    ConfLinkRclRep(ConfNull *conf, const string& nm, 
		   string *sk = 0)
	: m_conf(conf), m_nm(nm), m_sk(sk)
    {
    }
    virtual ~ConfLinkRclRep() {}

    virtual bool set(const string& val) 
    {
	if (!m_conf)
	    return false;
	LOGDEB1(("Setting [%s] value to [%s]\n",  m_nm.c_str(), val.c_str()));
	bool ret = m_conf->set(m_nm, val, m_sk?*m_sk:"");
	if (!ret)
	    LOGERR(("Value set failed\n"));
	return ret;
    }
    virtual bool get(string& val) 
    {
	if (!m_conf)
	    return false;
	bool ret = m_conf->get(m_nm, val, m_sk?*m_sk:"");
	LOGDEB1(("ConfLinkRcl::get: [%s] sk [%s] -> [%s]\n", 
		 m_nm.c_str(), m_sk?m_sk->c_str():"",
		 ret ? val.c_str() : "no value"));
	return ret;
    }
private:
    ConfNull     *m_conf;
    const string  m_nm;
    const string *m_sk;
};

} // Namespace confgui

#endif /* _CONFLINKRCL_H_INCLUDED_ */
