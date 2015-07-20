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
#ifndef _DYNCONF_H_INCLUDED_
#define _DYNCONF_H_INCLUDED_

/**
 * Dynamic configuration storage
 *
 * This used to be called "history" because of the initial usage.
 * Used to store some parameters which would fit neither in recoll.conf,
 * basically because they change a lot, nor in the QT preferences file, mostly 
 * because they are specific to a configuration directory.
 * Examples:
 *  - History of documents selected for preview
 *  - Active and inactive external databases (depend on the 
 *    configuration directory)
 *  - ...
 *
 * The storage is performed in a ConfSimple file, with subkeys and
 * encodings which depend on the data stored. Under each section, the keys 
 * are sequential numeric, so this basically manages a set of lists.
 *
 * The code ensures that a a given value (as defined by the
 * DynConfEntry::equal() method is only stored once. If undesirable,
 * equal() should always return false.
 */

#include <string>
#include <list>
#include <utility>

#include "conftree.h"
#include "base64.h"

/** Interface for a stored object. */
class DynConfEntry {
 public:
    virtual ~DynConfEntry() {}
    /** Decode object-as-string coming out from storage */
    virtual bool decode(const std::string &value) = 0;
    /** Encode object state into state for storing */
    virtual bool encode(std::string& value) = 0;
    /** Compare objects */
    virtual bool equal(const DynConfEntry &other) = 0;
};

/** Stored object specialization for generic string storage */
class RclSListEntry : public DynConfEntry {
 public:
    RclSListEntry() 
    {
    }
    virtual ~RclSListEntry() {}
    RclSListEntry(const std::string& v) 
    : value(v) 
    {
    }
    virtual bool decode(const std::string &enc)
    {
	base64_decode(enc, value);
	return true;
    }
    virtual bool encode(std::string& enc)
    {
	base64_encode(value, enc);
	return true;
    }
    virtual bool equal(const DynConfEntry& other)
    {
	const RclSListEntry& e = dynamic_cast<const RclSListEntry&>(other);
	return e.value == value;
    }

    std::string value;
};

/** The dynamic configuration class */
class RclDynConf {
 public:
    RclDynConf(const std::string &fn)
        : m_data(fn.c_str()) 
    {
    }
    bool ok() 
    {
	return m_data.getStatus() == ConfSimple::STATUS_RW;
    }
    std::string getFilename() 
    {
	return m_data.getFilename();
    }

    // Generic methods
    bool eraseAll(const std::string& sk);

    /** Insert new entry for section sk
     * @param sk section this is for
     * @param n  new entry
     * @param s a scratch entry used for decoding and comparisons,
     *        avoiding templating the routine for the actual entry type.
     */
    bool insertNew(const std::string& sk, DynConfEntry &n, DynConfEntry &s, 
                   int maxlen = -1);
    template<typename Tp> std::list<Tp> getList(const std::string& sk);

    // Specialized methods for simple string lists, designated by the
    // subkey value
    bool enterString(const std::string sk, const std::string value, int maxlen = -1);
    std::list<std::string> getStringList(const std::string sk);

 private:
    unsigned int m_mlen;
    ConfSimple   m_data;
};

template<typename Tp> std::list<Tp> RclDynConf::getList(const std::string &sk)
{
    std::list<Tp> mlist;
    Tp entry;
    std::vector<std::string> names = m_data.getNames(sk);
    for (std::vector<std::string>::const_iterator it = names.begin(); 
	 it != names.end(); it++) {
	std::string value;
	if (m_data.get(*it, value, sk)) {
	    if (!entry.decode(value))
		continue;
	    mlist.push_front(entry);
	}
    }
    return mlist;
}

// Defined subkeys. Values in dynconf.cpp
// History
extern const std::string docHistSubKey;
// All external indexes
extern const std::string allEdbsSk;
// Active external indexes
extern const std::string actEdbsSk;
// Advanced search history
extern const std::string advSearchHistSk;

#endif /* _DYNCONF_H_INCLUDED_ */
