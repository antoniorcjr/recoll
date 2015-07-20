/*
 *   Copyright 2004 J.F.Dockes
 *
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
#include "autoconfig.h"

#include <errno.h>
#include <iostream>
#include <string>
#include <vector>
#include <list>
using namespace std;

#include "cstr.h"
#include "mimehandler.h"
#include "debuglog.h"
#include "rclconfig.h"
#include "smallut.h"
#include "md5.h"

#include "mh_exec.h"
#include "mh_execm.h"
#include "mh_html.h"
#include "mh_mail.h"
#include "mh_mbox.h"
#include "mh_text.h"
#include "mh_symlink.h"
#include "mh_unknown.h"
#include "ptmutex.h"

// Performance help: we use a pool of already known and created
// handlers. There can be several instances for a given mime type
// (think email attachment in email message: 2 rfc822 handlers are
// needed simulteanously)
static multimap<string, RecollFilter*>  o_handlers;
static list<multimap<string, RecollFilter*>::iterator> o_hlru;
typedef list<multimap<string, RecollFilter*>::iterator>::iterator hlruit_tp;

static PTMutexInit o_handlers_mutex;

static const unsigned int max_handlers_cache_size = 100;

/* Look for mime handler in pool */
static RecollFilter *getMimeHandlerFromCache(const string& key)
{
    PTMutexLocker locker(o_handlers_mutex);
    string xdigest;
    MD5HexPrint(key, xdigest);
    LOGDEB(("getMimeHandlerFromCache: %s cache size %u\n", 
	    xdigest.c_str(), o_handlers.size()));

    multimap<string, RecollFilter *>::iterator it = o_handlers.find(key);
    if (it != o_handlers.end()) {
	RecollFilter *h = it->second;
	hlruit_tp it1 = find(o_hlru.begin(), o_hlru.end(), it);
	if (it1 != o_hlru.end()) {
	    o_hlru.erase(it1);
	} else {
	    LOGERR(("getMimeHandlerFromCache: lru position not found\n"));
	}
	o_handlers.erase(it);
	LOGDEB(("getMimeHandlerFromCache: %s found size %u\n", 
		xdigest.c_str(), o_handlers.size()));
	return h;
    }
    LOGDEB(("getMimeHandlerFromCache: %s not found\n", xdigest.c_str()));
    return 0;
}

/* Return mime handler to pool */
void returnMimeHandler(RecollFilter *handler)
{
    typedef multimap<string, RecollFilter*>::value_type value_type;

    if (handler == 0) {
	LOGERR(("returnMimeHandler: bad parameter\n"));
	return;
    }
    handler->clear();

    PTMutexLocker locker(o_handlers_mutex);

    LOGDEB(("returnMimeHandler: returning filter for %s cache size %d\n", 
	    handler->get_mime_type().c_str(), o_handlers.size()));

    // Limit pool size. The pool can grow quite big because there are
    // many filter types, each of which can be used in several copies
    // at the same time either because it occurs several times in a
    // stack (ie mail attachment to mail), or because several threads
    // are processing the same mime type at the same time.
    multimap<string, RecollFilter *>::iterator it;
    if (o_handlers.size() >= max_handlers_cache_size) {
	static int once = 1;
	if (once) {
	    once = 0;
	    for (it = o_handlers.begin(); it != o_handlers.end(); it++) {
		LOGDEB1(("Cache full. key: %s\n", it->first.c_str()));
	    }
	    LOGDEB1(("Cache LRU size: %u\n", o_hlru.size()));
	}
	if (o_hlru.size() > 0) {
	    it = o_hlru.back();
	    o_hlru.pop_back();
	    delete it->second;
	    o_handlers.erase(it);
	}
    }
    it = o_handlers.insert(value_type(handler->get_id(), handler));
    o_hlru.push_front(it);
}

void clearMimeHandlerCache()
{
    LOGDEB(("clearMimeHandlerCache()\n"));
    typedef multimap<string, RecollFilter*>::value_type value_type;
    multimap<string, RecollFilter *>::iterator it;
    PTMutexLocker locker(o_handlers_mutex);
    for (it = o_handlers.begin(); it != o_handlers.end(); it++) {
	delete it->second;
    }
    o_handlers.clear();
}

/** For mime types set as "internal" in mimeconf: 
  * create appropriate handler object. */
static RecollFilter *mhFactory(RclConfig *config, const string &mime,
				bool nobuild, string& id)
{
    LOGDEB2(("mhFactory(%s)\n", mime.c_str()));
    string lmime(mime);
    stringtolower(lmime);
    if (cstr_textplain == lmime) {
	LOGDEB2(("mhFactory(%s): returning MimeHandlerText\n", mime.c_str()));
	MD5String("MimeHandlerText", id);
	return nobuild ? 0 : new MimeHandlerText(config, id);
    } else if ("text/html" == lmime) {
	LOGDEB2(("mhFactory(%s): returning MimeHandlerHtml\n", mime.c_str()));
	MD5String("MimeHandlerHtml", id);
	return nobuild ? 0 : new MimeHandlerHtml(config, id);
    } else if ("text/x-mail" == lmime) {
	LOGDEB2(("mhFactory(%s): returning MimeHandlerMbox\n", mime.c_str()));
	MD5String("MimeHandlerMbox", id);
	return nobuild ? 0 : new MimeHandlerMbox(config, id);
    } else if ("message/rfc822" == lmime) {
	LOGDEB2(("mhFactory(%s): returning MimeHandlerMail\n", mime.c_str()));
	MD5String("MimeHandlerMail", id);
	return nobuild ? 0 : new MimeHandlerMail(config, id);
    } else if ("inode/symlink" == lmime) {
	LOGDEB2(("mhFactory(%s): ret MimeHandlerSymlink\n", mime.c_str()));
	MD5String("MimeHandlerSymlink", id);
	return nobuild ? 0 : new MimeHandlerSymlink(config, id);
    } else if (lmime.find("text/") == 0) {
        // Try to handle unknown text/xx as text/plain. This
        // only happen if the text/xx was defined as "internal" in
        // mimeconf, not at random. For programs, for example this
        // allows indexing and previewing as text/plain (no filter
        // exec) but still opening with a specific editor.
	LOGDEB2(("mhFactory(%s): returning MimeHandlerText(x)\n",mime.c_str()));
	MD5String("MimeHandlerText", id);
        return nobuild ? 0 : new MimeHandlerText(config, id); 
    } else {
	// We should not get there. It means that "internal" was set
	// as a handler in mimeconf for a mime type we actually can't
	// handle.
	LOGERR(("mhFactory: mime type [%s] set as internal but unknown\n", 
		lmime.c_str()));
	MD5String("MimeHandlerUnknown", id);
	return nobuild ? 0 : new MimeHandlerUnknown(config, id);
    }
}

static const string cstr_mh_charset("charset");
/**
 * Create a filter that executes an external program or script
 * A filter def can look like:
 *      someprog -v -t " h i j";charset= xx; mimetype=yy
 * A semi-colon list of attr=value pairs may come after the exec spec.
 * This list is treated by replacing semi-colons with newlines and building
 * a confsimple. This is done quite brutally and we don't support having
 * a ';' inside a quoted string for now. Can't see a use for it.
 */
MimeHandlerExec *mhExecFactory(RclConfig *cfg, const string& mtype, string& hs,
                               bool multiple, const string& id)
{
    ConfSimple attrs;
    string cmdstr;

    if (!cfg->valueSplitAttributes(hs, cmdstr, attrs)) {
	LOGERR(("mhExecFactory: bad config line for [%s]: [%s]\n", 
		mtype.c_str(), hs.c_str()));
        return 0;
    }

    // Split command name and args, and build exec object
    list<string> cmdtoks;
    stringToStrings(cmdstr, cmdtoks);
    if (cmdtoks.empty()) {
	LOGERR(("mhExecFactory: bad config line for [%s]: [%s]\n", 
		mtype.c_str(), hs.c_str()));
	return 0;
    }
    MimeHandlerExec *h = multiple ? 
	new MimeHandlerExecMultiple(cfg, id) :
        new MimeHandlerExec(cfg, id);
    list<string>::iterator it = cmdtoks.begin();
    h->params.push_back(cfg->findFilter(*it++));
    h->params.insert(h->params.end(), it, cmdtoks.end());

    // Handle additional attributes. We substitute the semi-colons
    // with newlines and use a ConfSimple
    string value;
    if (attrs.get(cstr_mh_charset, value)) 
        h->cfgFilterOutputCharset = stringtolower((const string&)value);
    if (attrs.get(cstr_dj_keymt, value))
        h->cfgFilterOutputMtype = stringtolower((const string&)value);

#if 0
    string scmd;
    for (it = h->params.begin(); it != h->params.end(); it++) {
	scmd += string("[") + *it + "] ";
    }
    LOGDEB(("mhExecFactory:mt [%s] cfgmt [%s] cfgcs [%s] cmd: [%s]\n",
	    mtype.c_str(), h->cfgFilterOutputMtype.c_str(), h->cfgFilterOutputCharset.c_str(),
	    scmd.c_str()));
#endif

    return h;
}

/* Get handler/filter object for given mime type: */
RecollFilter *getMimeHandler(const string &mtype, RclConfig *cfg, 
			      bool filtertypes)
{
    LOGDEB(("getMimeHandler: mtype [%s] filtertypes %d\n", 
	     mtype.c_str(), filtertypes));
    RecollFilter *h = 0;

    // Get handler definition for mime type. We do this even if an
    // appropriate handler object may be in the cache.
    // This is fast, and necessary to conform to the
    // configuration, (ie: text/html might be filtered out by
    // indexedmimetypes but an html handler could still be in the
    // cache because it was needed by some other interning stack).
    string hs;
    hs = cfg->getMimeHandlerDef(mtype, filtertypes);
    string id;

    if (!hs.empty()) { 
	// Got a handler definition line
	// Break definition into type (internal/exec/execm) 
	// and name/command string 
        string::size_type b1 = hs.find_first_of(" \t");
        string handlertype = hs.substr(0, b1);
	string cmdstr;
	if (b1 != string::npos) {
	    cmdstr = hs.substr(b1);
            trimstring(cmdstr);
	}
	bool internal = !stringlowercmp("internal", handlertype);
	if (internal) {
	    // For internal types let the factory compute the id
	    mhFactory(cfg, cmdstr.empty() ? mtype : cmdstr, true, id);
	} else {
	    // exec/execm: use the md5 of the def line
	    MD5String(hs, id);
	}

#if 0
	{ // string xdigest; LOGDEB2(("getMimeHandler: [%s] hs [%s] id [%s]\n", 
	  //mtype.c_str(), hs.c_str(), MD5HexPrint(id, xdigest).c_str()));
	}
#endif

        // Do we already have a handler object in the cache ?
	h = getMimeHandlerFromCache(id);
	if (h != 0)
	    goto out;

	LOGDEB2(("getMimeHandler: %s not in cache\n", mtype.c_str()));

	// Not in cache. 
	if (internal) {
	    // If there is a parameter after "internal" it's the mime
	    // type to use. This is so that we can have bogus mime
	    // types like text/x-purple-html-log (for ie: specific
	    // icon) and still use the html filter on them. This is
	    // partly redundant with the localfields/rclaptg, but
	    // better and the latter will probably go away at some
	    // point in the future.
	    LOGDEB2(("handlertype internal, cmdstr [%s]\n", cmdstr.c_str()));
	    h = mhFactory(cfg, cmdstr.empty() ? mtype : cmdstr, false, id);
	    goto out;
	} else if (!stringlowercmp("dll", handlertype)) {
	} else {
            if (cmdstr.empty()) {
		LOGERR(("getMimeHandler: bad line for %s: %s\n", 
			mtype.c_str(), hs.c_str()));
		goto out;
	    }
            if (!stringlowercmp("exec", handlertype)) {
                h = mhExecFactory(cfg, mtype, cmdstr, false, id);
		goto out;
            } else if (!stringlowercmp("execm", handlertype)) {
                h = mhExecFactory(cfg, mtype, cmdstr, true, id);
		goto out;
            } else {
		LOGERR(("getMimeHandler: bad line for %s: %s\n", 
			mtype.c_str(), hs.c_str()));
		goto out;
            }
	}
    }

    // We get here if there was no specific error, but there is no
    // identified mime type, or no handler associated.

    // Finally, unhandled files are either ignored or their name and
    // generic metadata is indexed, depending on configuration
    {
	bool indexunknown = false;
	cfg->getConfParam("indexallfilenames", &indexunknown);
	if (indexunknown) {
	    MD5String("MimeHandlerUnknown", id);
	    if ((h = getMimeHandlerFromCache(id)) == 0)
		h = new MimeHandlerUnknown(cfg, id);
	}
	goto out;
    }

out:
    if (h) {
	h->set_property(RecollFilter::DEFAULT_CHARSET, cfg->getDefCharset());
	// In multithread context, and in case this handler is out
	// from the cache, it may have a config pointer belonging to
	// another thread. Fix it.
	h->setConfig(cfg);
    }
    return h;
}

/// Can this mime type be interned (according to config) ?
bool canIntern(const std::string mtype, RclConfig *cfg)
{
    if (mtype.empty())
	return false;
    string hs = cfg->getMimeHandlerDef(mtype);
    if (hs.empty())
	return false;
    return true;
}
