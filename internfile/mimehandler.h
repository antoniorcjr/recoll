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
#ifndef _MIMEHANDLER_H_INCLUDED_
#define _MIMEHANDLER_H_INCLUDED_
#include "autoconfig.h"

#include <stdio.h>

#include <string>

#include "Filter.h"
#include "cstr.h"

class RclConfig;

class RecollFilter : public Dijon::Filter {
public:
    RecollFilter(RclConfig *config, const std::string& id)
	: m_config(config), m_forPreview(false), m_havedoc(false), m_id(id)
    {}
    virtual ~RecollFilter() {}
    virtual void setConfig(RclConfig *config)
    {
	m_config = config;
    }
    virtual bool set_property(Properties p, const std::string &v) {
	switch (p) {
	case DJF_UDI: 
	    m_udi = v;
	    break;
	case DEFAULT_CHARSET: 
	    m_dfltInputCharset = v;
	    break;
	case OPERATING_MODE: 
	    if (!v.empty() && v[0] == 'v') 
		m_forPreview = true; 
	    else 
		m_forPreview = false;
	    break;
	}
	return true;
    }

    // We don't use this for now
    virtual bool set_document_uri(const std::string& mtype, 
				  const std::string &) 
    {
	m_mimeType = mtype;
	return false;
    }

    // This does nothing right now but should be called from the
    // subclass method in case we need some common processing one day
    // (was used for xattrs at some point).  Yes this is the "call
    // super" anti-pattern, bad, but we have several layers of derived
    // classes, so that implementing the template method approach (by
    // having a pure virtual called from here and implemented in the
    // subclass) would have to be repeated in each derived class. It's
    // just simpler this way.
    virtual bool set_document_file(const std::string& mtype, 
				   const std::string & /*file_path*/) 
    {
	m_mimeType = mtype;
	return true;
    }

    // Default implementations
    virtual bool set_document_string(const std::string& mtype, 
				     const std::string &) 
    {
	m_mimeType = mtype;
	return false;
    }
    virtual bool set_document_data(const std::string& mtype, 
				   const char *cp, unsigned int sz) 
    {
	return set_document_string(mtype, std::string(cp, sz));
    }

    virtual void set_docsize(size_t size)
    {
	char csize[30];
	sprintf(csize, "%lld", (long long)size);
	m_metaData[cstr_dj_keydocsize] = csize;
    }

    virtual bool has_documents() const {return m_havedoc;}

    // Most doc types are single-doc
    virtual bool skip_to_document(const std::string& s) {
	if (s.empty())
	    return true;
	return false;
    }

    virtual bool is_data_input_ok(DataInput input) const {
	if (input == DOCUMENT_FILE_NAME)
	    return true;
	return false;
    }

    virtual std::string get_error() const {
	return m_reason;
    }

    virtual const std::string& get_id() const
    {
	return m_id;
    }

    // "Call super" anti-pattern again. Must be called from derived
    // classes which reimplement clear()
    virtual void clear() {
	Dijon::Filter::clear();
	m_forPreview = m_havedoc = false;
	m_dfltInputCharset.clear();
	m_reason.clear();
    }

    // This only makes sense if the contents are currently txt/plain
    // It converts from keyorigcharset to UTF-8 and sets keycharset.
    bool txtdcode(const std::string& who);

protected:
    bool preview() {return m_forPreview;}

    RclConfig *m_config;
    bool   m_forPreview;
    std::string m_dfltInputCharset;
    std::string m_reason;
    bool   m_havedoc;
    std::string m_udi; // May be set by creator as a hint
    // m_id is and md5 of the filter definition line (from mimeconf) and
    // is used when fetching/returning filters to / from the cache.
    std::string m_id;
};

/**
 * Return indexing handler object for the given mime type. The returned 
 * pointer should be passed to returnMimeHandler() for recycling, after use.
 * @param mtyp input mime type, ie text/plain
 * @param cfg  the recoll config object to be used
 * @param filtertypes decide if we should restrict to types in 
 *     indexedmimetypes (if this is set at all).
 */
extern RecollFilter *getMimeHandler(const std::string &mtyp, RclConfig *cfg,
				     bool filtertypes=false);

/// Free up filter for reuse (you can also delete it)
extern void returnMimeHandler(RecollFilter *);

/// Clean up cache at the end of an indexing pass. For people who use
/// the GUI to index: avoid all those filter processes forever hanging
/// off recoll.
extern void clearMimeHandlerCache();

/// Can this mime type be interned ?
extern bool canIntern(const std::string mimetype, RclConfig *cfg);

#endif /* _MIMEHANDLER_H_INCLUDED_ */
