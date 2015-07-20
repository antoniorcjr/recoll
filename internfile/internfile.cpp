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

#ifndef TEST_INTERNFILE
#include "autoconfig.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <string>
#include <iostream>
#include <map>
#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

#include "cstr.h"
#include "internfile.h"
#include "rcldoc.h"
#include "mimetype.h"
#include "debuglog.h"
#include "mimehandler.h"
#include "execmd.h"
#include "pathut.h"
#include "rclconfig.h"
#include "mh_html.h"
#include "fileudi.h"
#include "cancelcheck.h"
#include "copyfile.h"
#include "fetcher.h"
#include "extrameta.h"

// The internal path element separator. This can't be the same as the rcldb 
// file to ipath separator : "|"
// We replace it with a control char if it comes out of a filter (ie:
// rclzip or rclchm can do this). If you want the SOH control char
// inside an ipath, you're out of luck (and a bit weird).
static const string cstr_isep(":");

static const char cchar_colon_repl = '\x01';
static string colon_hide(const string& in)
{
    string out;
    for (string::const_iterator it = in.begin(); it != in.end(); it++) {
	out += *it == ':' ? cchar_colon_repl : *it;
    }
    return out;
}
static string colon_restore(const string& in)
{
    string out;
    for (string::const_iterator it = in.begin(); it != in.end(); it++) {
	out += *it == cchar_colon_repl ? ':' : *it;
    }
    return out;
}

// This is used when the user wants to retrieve a search result doc's parent
// (ie message having a given attachment)
bool FileInterner::getEnclosingUDI(const Rcl::Doc &doc, string& udi)
{
    LOGDEB(("FileInterner::getEnclosingUDI(): url [%s] ipath [%s]\n", 
	    doc.url.c_str(), doc.ipath.c_str()));
    string eipath = doc.ipath;
    string::size_type colon;
    if (eipath.empty())
	return false;
    if ((colon =  eipath.find_last_of(cstr_isep)) != string::npos) {
	eipath.erase(colon);
    } else {
	eipath.erase();
    }
    
    make_udi(url_gpath(doc.idxurl.empty() ? doc.url : doc.idxurl), eipath, udi);
    return true;
}

string FileInterner::getLastIpathElt(const string& ipath)
{
    string::size_type sep;
    if ((sep =  ipath.find_last_of(cstr_isep)) != string::npos) {
	return ipath.substr(sep + 1);
    } else {
	return ipath;
    }
}

// Constructor: identify the input file, possibly create an
// uncompressed temporary copy, and create the top filter for the
// uncompressed file type.
//
// Empty handler on return says that we're in error, this will be
// processed by the first call to internfile().
// Split into "constructor calls init()" to allow use from other constructor
FileInterner::FileInterner(const string &fn, const struct stat *stp,
			   RclConfig *cnf, int flags, const string *imime)
    : m_ok(false), m_missingdatap(0), m_uncomp((flags & FIF_forPreview) != 0)
{
    LOGDEB0(("FileInterner::FileInterner(fn=%s)\n", fn.c_str()));
    if (fn.empty()) {
	LOGERR(("FileInterner::FileInterner: empty file name!\n"));
	return;
    }
    initcommon(cnf, flags);
    init(fn, stp, cnf, flags, imime);
}

void FileInterner::init(const string &f, const struct stat *stp, RclConfig *cnf,
                        int flags, const string *imime)
{
    if (f.empty()) {
	LOGERR(("FileInterner::init: empty file name!\n"));
	return;
    }
    m_fn = f;

    // Compute udi for the input file. This is used by filters which
    // manage some kind of cache.  Indexing by udi makes things easier
    // because they sometimes get a temp as actual input.
    string udi;
    make_udi(f, cstr_null, udi);

    cnf->setKeyDir(path_getfather(m_fn));

    string l_mime;
    bool usfci = false;
    cnf->getConfParam("usesystemfilecommand", &usfci);

    // In general, even when the input mime type is set (when
    // previewing), we can't use it: it's the type for the actual
    // document, but this can be part of a compound document, and
    // we're dealing with the top level file here, or this could be a
    // compressed file. The flag tells us we really can use it
    // (e.g. the beagle indexer sets it).
    if (flags & FIF_doUseInputMimetype) {
        if (!imime) {
            LOGERR(("FileInterner:: told to use null imime\n"));
            return;
        }
        l_mime = *imime;
    } else {
        LOGDEB(("FileInterner::init fn [%s] mime [%s] preview %d\n", 
                f.c_str(), imime?imime->c_str() : "(null)", m_forPreview));

        // Run mime type identification in any case (see comment above).
        l_mime = mimetype(m_fn, stp, m_cfg, usfci);

        // If identification fails, try to use the input parameter. This
        // is then normally not a compressed type (it's the mime type from
        // the db), and is only set when previewing, not for indexing
        if (l_mime.empty() && imime)
            l_mime = *imime;
    }

    size_t docsize = stp->st_size;

    if (!l_mime.empty()) {
	// Has mime: check for a compressed file. If so, create a
	// temporary uncompressed file, and rerun the mime type
	// identification, then do the rest with the temp file.
	vector<string>ucmd;
	if (m_cfg->getUncompressor(l_mime, ucmd)) {
	    // Check for compressed size limit
	    int maxkbs = -1;
	    if (!m_cfg->getConfParam("compressedfilemaxkbs", &maxkbs) ||
		maxkbs < 0 || !stp || int(stp->st_size / 1024) < maxkbs) {
		if (!m_uncomp.uncompressfile(m_fn, ucmd, m_tfile)) {
		    return;
		}
		LOGDEB1(("FileInterner:: after ucomp: tfile %s\n", 
			 m_tfile.c_str()));
		m_fn = m_tfile;
		// Stat the uncompressed file, mainly to get the size
		struct stat ucstat;
		if (stat(m_fn.c_str(), &ucstat) != 0) {
		    LOGERR(("FileInterner: can't stat the uncompressed file"
			    "[%s] errno %d\n", m_fn.c_str(), errno));
		    return;
		} else {
		    docsize = ucstat.st_size;
		}
		l_mime = mimetype(m_fn, &ucstat, m_cfg, usfci);
		if (l_mime.empty() && imime)
		    l_mime = *imime;
	    } else {
		LOGINFO(("FileInterner:: %s over size limit %d kbs\n",
			 m_fn.c_str(), maxkbs));
	    }
	}
    }

    if (l_mime.empty()) {
	// No mime type. We let it through as config may warrant that
	// we index all file names
	LOGDEB0(("FileInterner:: no mime: [%s]\n", m_fn.c_str()));
    }

    // Look for appropriate handler (might still return empty)
    m_mimetype = l_mime;
    RecollFilter *df = getMimeHandler(l_mime, m_cfg, !m_forPreview);

    if (!df or df->is_unknown()) {
	// No real handler for this type, for now :( 
	LOGDEB(("FileInterner:: unprocessed mime: [%s] [%s]\n", 
		l_mime.c_str(), f.c_str()));
	if (!df)
	    return;
    }
    df->set_property(Dijon::Filter::OPERATING_MODE, 
		     m_forPreview ? "view" : "index");
    df->set_property(Dijon::Filter::DJF_UDI, udi);

    // Get fields computed from extended attributes. We use the
    // original file, not the m_fn which may be the uncompressed temp
    // file
    if (!m_noxattrs)
	reapXAttrs(m_cfg, f, m_XAttrsFields);

    // Gather metadata from external commands as configured.
    reapMetaCmds(m_cfg, f, m_cmdFields);

    df->set_docsize(docsize);
    if (!df->set_document_file(l_mime, m_fn)) {
	delete df;
	LOGERR(("FileInterner:: error converting %s\n", m_fn.c_str()));
	return;
    }

    m_handlers.push_back(df);
    LOGDEB(("FileInterner:: init ok %s [%s]\n", l_mime.c_str(), m_fn.c_str()));
    m_ok = true;
}

// Setup from memory data (ie: out of the web cache). imime needs to be set.
FileInterner::FileInterner(const string &data, RclConfig *cnf, 
                           int flags, const string& imime)
    : m_ok(false), m_missingdatap(0), m_uncomp((flags & FIF_forPreview) != 0)
{
    LOGDEB0(("FileInterner::FileInterner(data)\n"));
    initcommon(cnf, flags);
    init(data, cnf, flags, imime);
}

void FileInterner::init(const string &data, RclConfig *cnf, 
                        int flags, const string& imime)
{
    if (imime.empty()) {
	LOGERR(("FileInterner: inmemory constructor needs input mime type\n"));
        return;
    }
    m_mimetype = imime;

    // Look for appropriate handler (might still return empty)
    RecollFilter *df = getMimeHandler(m_mimetype, m_cfg, !m_forPreview);

    if (!df) {
	// No handler for this type, for now :( if indexallfilenames
	// is set in the config, this normally wont happen (we get mh_unknown)
	LOGDEB(("FileInterner:: unprocessed mime [%s]\n", m_mimetype.c_str()));
	return;
    }
    df->set_property(Dijon::Filter::OPERATING_MODE, 
			    m_forPreview ? "view" : "index");

    bool result = false;
    df->set_docsize(data.length());
    if (df->is_data_input_ok(Dijon::Filter::DOCUMENT_STRING)) {
	result = df->set_document_string(m_mimetype, data);
    } else if (df->is_data_input_ok(Dijon::Filter::DOCUMENT_DATA)) {
	result = df->set_document_data(m_mimetype, data.c_str(), data.length());
    } else if (df->is_data_input_ok(Dijon::Filter::DOCUMENT_FILE_NAME)) {
	TempFile temp = dataToTempFile(data, m_mimetype);
	if (temp.isNotNull() && 
	    (result = df->set_document_file(m_mimetype, temp->filename()))) {
	    m_tmpflgs[m_handlers.size()] = true;
	    m_tempfiles.push_back(temp);
	}
    }
    if (!result) {
	LOGINFO(("FileInterner:: set_doc failed inside for mtype %s\n", 
                 m_mimetype.c_str()));
	delete df;
	return;
    }
    m_handlers.push_back(df);
    m_ok = true;
}

void FileInterner::initcommon(RclConfig *cnf, int flags)
{
    m_cfg = cnf;
    m_forPreview = ((flags & FIF_forPreview) != 0);
    // Initialize handler stack.
    m_handlers.reserve(MAXHANDLERS);
    for (unsigned int i = 0; i < MAXHANDLERS; i++)
	m_tmpflgs[i] = false;
    m_targetMType = cstr_textplain;
    m_cfg->getConfParam("noxattrfields", &m_noxattrs);
}

FileInterner::FileInterner(const Rcl::Doc& idoc, RclConfig *cnf, int flags)
    : m_ok(false), m_missingdatap(0), m_uncomp(((flags & FIF_forPreview) != 0))
{
    LOGDEB0(("FileInterner::FileInterner(idoc)\n"));
    initcommon(cnf, flags);

    DocFetcher *fetcher = docFetcherMake(idoc);
    if (fetcher == 0) {
        LOGERR(("FileInterner:: no backend\n"));
        return;
    }
    DocFetcher::RawDoc rawdoc;
    if (!fetcher->fetch(cnf, idoc, rawdoc)) {
	LOGERR(("FileInterner:: fetcher failed\n"));
	return;
    }
    switch (rawdoc.kind) {
    case DocFetcher::RawDoc::RDK_FILENAME:
        init(rawdoc.data, &rawdoc.st, cnf, flags, &idoc.mimetype);
	break;
    case DocFetcher::RawDoc::RDK_DATA:
        init(rawdoc.data, cnf, flags, idoc.mimetype);
	break;
    default:
	LOGERR(("FileInterner::FileInterner(idoc): bad rawdoc kind ??\n"));
    }
    return;
}

bool FileInterner::makesig(RclConfig *cnf, const Rcl::Doc& idoc, string& sig)
{
    DocFetcher *fetcher = docFetcherMake(idoc);
    if (fetcher == 0) {
        LOGERR(("FileInterner::makesig no backend for doc\n"));
        return false;
    }

    bool ret = fetcher->makesig(cnf, idoc, sig);

    delete fetcher;
    return ret;
}

FileInterner::~FileInterner()
{
    for (vector<RecollFilter*>::iterator it = m_handlers.begin();
	 it != m_handlers.end(); it++) {
        returnMimeHandler(*it);
    }
    // m_tempfiles will take care of itself
}

// Create a temporary file for a block of data (ie: attachment) found
// while walking the internal document tree, with a type for which the
// handler needs an actual file (ie : external script).
TempFile FileInterner::dataToTempFile(const string& dt, const string& mt)
{
    // Create temp file with appropriate suffix for mime type
    TempFile temp(new TempFileInternal(m_cfg->getSuffixFromMimeType(mt)));
    if (!temp->ok()) {
	LOGERR(("FileInterner::dataToTempFile: cant create tempfile: %s\n",
		temp->getreason().c_str()));
	return TempFile();
    }

    int fd = open(temp->filename(), O_WRONLY);
    if (fd < 0) {
	LOGERR(("FileInterner::dataToTempFile: open(%s) failed errno %d\n",
		temp->filename(), errno));
	return TempFile();
    }
    if (write(fd, dt.c_str(), dt.length()) != (int)dt.length()) {
	close(fd);
	LOGERR(("FileInterner::dataToTempFile: write to %s failed errno %d\n",
		temp->filename(), errno));
	return TempFile();
    }
    close(fd);
    return temp;
}

// See if the error string is formatted as a missing helper message,
// accumulate helper name if it is. The format of the message is:
// RECFILTERROR HELPERNOTFOUND program1 [program2 ...]
void FileInterner::checkExternalMissing(const string& msg, const string& mt)
{
    LOGDEB2(("checkExternalMissing: [%s]\n", msg.c_str()));
    if (m_missingdatap && msg.find("RECFILTERROR") == 0) {
	vector<string> verr;
	stringToStrings(msg, verr);
	if (verr.size() > 2) {
	    vector<string>::iterator it = verr.begin();
	    it++;
	    if (*it == "HELPERNOTFOUND") {
		it++;
		for (; it != verr.end(); it++) {
		    m_missingdatap->addMissing(*it, mt);
		}
	    }
	}		    
    }
}

void FIMissingStore::getMissingExternal(string& out) 
{
    for (map<string, set<string> >::const_iterator it = 
	     m_typesForMissing.begin(); it != m_typesForMissing.end(); it++) {
	out += string(" ") + it->first;
    }
    trimstring(out);
}

void FIMissingStore::getMissingDescription(string& out)
{
    out.erase();

    for (map<string, set<string> >::const_iterator it = 
	     m_typesForMissing.begin(); it != m_typesForMissing.end(); it++) {
	out += it->first + " (";
	set<string>::const_iterator it3;
	for (it3 = it->second.begin(); 
		 it3 != it->second.end(); it3++) {
	    out += *it3 + " ";
	}
	trimstring(out);
	out += ")";
	out += "\n";
    }
}

FIMissingStore::FIMissingStore(const string& in)
{
    // The "missing" file is text. Each line defines a missing filter
    // and the list of mime types actually encountered that needed it
    // (see method getMissingDescription())

    vector<string> lines;
    stringToTokens(in, lines, "\n");

    for (vector<string>::const_iterator it = lines.begin();
	 it != lines.end(); it++) {
	// Lines from the file are like: 
	//
	// filter name string (mime1 mime2) 
	//
	// We can't be too sure that there will never be a parenthesis
	// inside the filter string as this comes from the filter
	// itself. The list part is safer, so we start from the end.
	const string& line = *it;
	string::size_type lastopen = line.find_last_of("(");
	if (lastopen == string::npos)
	    continue;
	string::size_type lastclose = line.find_last_of(")");
	if (lastclose == string::npos || lastclose <= lastopen + 1)
	    continue;
	string smtypes = line.substr(lastopen+1, lastclose - lastopen - 1);
	vector<string> mtypes;
	stringToTokens(smtypes, mtypes);
	string filter = line.substr(0, lastopen);
	trimstring(filter);
	if (filter.empty())
	    continue;

	for (vector<string>::const_iterator itt = mtypes.begin(); 
	     itt != mtypes.end(); itt++) {
	    m_typesForMissing[filter].insert(*itt);
	}
    }
}

// Helper for extracting a value from a map.
static inline bool getKeyValue(const map<string, string>& docdata, 
			       const string& key, string& value)
{
    map<string,string>::const_iterator it;
    it = docdata.find(key);
    if (it != docdata.end()) {
	value = it->second;
	LOGDEB2(("getKeyValue: [%s]->[%s]\n", key.c_str(), value.c_str()));
	return true;
    }
    LOGDEB2(("getKeyValue: no value for [%s]\n", key.c_str()));
    return false;
}

bool FileInterner::dijontorcl(Rcl::Doc& doc)
{
    RecollFilter *df = m_handlers.back();
    if (df == 0) {
	//??
	LOGERR(("FileInterner::dijontorcl: null top handler ??\n"));
	return false;
    }
    const map<string, string>& docdata = df->get_meta_data();

    for (map<string,string>::const_iterator it = docdata.begin(); 
	 it != docdata.end(); it++) {
	if (it->first == cstr_dj_keycontent) {
	    doc.text = it->second;
	    if (doc.fbytes.empty()) {
		// It's normally set by walking the filter stack, in
		// collectIpathAndMt, which was called before us.  It
		// can happen that the doc size is still empty at this
		// point if the last container filter is directly
		// returning text/plain content, so that there is no
		// ipath-less filter at the top
		char cbuf[30];
		sprintf(cbuf, "%d", int(doc.text.length()));
		doc.fbytes = cbuf;
	    }
	} else if (it->first == cstr_dj_keymd) {
	    doc.dmtime = it->second;
	} else if (it->first == cstr_dj_keyanc) {
	    doc.haschildren = true;
	} else if (it->first == cstr_dj_keyorigcharset) {
	    doc.origcharset = it->second;
	} else if (it->first == cstr_dj_keyfn) {
	    // Only if not set during the stack walk
	    const string *fnp = 0;
	    if (!doc.peekmeta(Rcl::Doc::keyfn, &fnp) || fnp->empty())
		doc.meta[Rcl::Doc::keyfn] = it->second;
	} else if (it->first == cstr_dj_keymt || 
		   it->first == cstr_dj_keycharset) {
	    // don't need/want these.
	} else {
	    doc.addmeta(m_cfg->fieldCanon(it->first), it->second);
	}
    }
    if (doc.meta[Rcl::Doc::keyabs].empty() && 
	!doc.meta[cstr_dj_keyds].empty()) {
	doc.meta[Rcl::Doc::keyabs] = doc.meta[cstr_dj_keyds];
	doc.meta.erase(cstr_dj_keyds);
    }
    return true;
}

// Collect the ipath from the current path in the document tree.
// While we're at it, we also set the mimetype and filename,
// which are special properties: we want to get them from the topmost
// doc with an ipath, not the last one which is usually text/plain We
// also set the author and modification time from the last doc which
// has them.
//
// The docsize is fetched from the first element without an ipath
// (first non container). If the last element directly returns
// text/plain so that there is no ipath-less element, the value will
// be set in dijontorcl(). 
// 
// The whole thing is a bit messy but it's not obvious how it should
// be cleaned up as the "inheritance" rules inside the stack are
// actually complicated.
void FileInterner::collectIpathAndMT(Rcl::Doc& doc) const
{
    LOGDEB2(("FileInterner::collectIpathAndMT\n"));
    bool hasipath = false;

    if (!m_noxattrs) {
	docFieldsFromXattrs(m_cfg, m_XAttrsFields, doc);
    }

    docFieldsFromMetaCmds(m_cfg, m_cmdFields, doc);

    // If there is no ipath stack, the mimetype is the one from the file
    doc.mimetype = m_mimetype;

    string ipathel;
    for (vector<RecollFilter*>::const_iterator hit = m_handlers.begin();
	 hit != m_handlers.end(); hit++) {
	const map<string, string>& docdata = (*hit)->get_meta_data();
	if (getKeyValue(docdata, cstr_dj_keyipath, ipathel)) {
	    if (!ipathel.empty()) {
		// We have a non-empty ipath
		hasipath = true;
		getKeyValue(docdata, cstr_dj_keymt, doc.mimetype);
		getKeyValue(docdata, cstr_dj_keyfn, doc.meta[Rcl::Doc::keyfn]);
	    } else {
		if (doc.fbytes.empty())
		    getKeyValue(docdata, cstr_dj_keydocsize, doc.fbytes);
	    }
	    doc.ipath += colon_hide(ipathel) + cstr_isep;
	} else {
	    if (doc.fbytes.empty())
		getKeyValue(docdata, cstr_dj_keydocsize, doc.fbytes);
	    doc.ipath += cstr_isep;
	}
	getKeyValue(docdata, cstr_dj_keyauthor, doc.meta[Rcl::Doc::keyau]);
	getKeyValue(docdata, cstr_dj_keymd, doc.dmtime);
    }

    // Trim empty tail elements in ipath.
    if (hasipath) {
	LOGDEB2(("IPATH [%s]\n", doc.ipath.c_str()));
	string::size_type sit = doc.ipath.find_last_not_of(cstr_isep);
	if (sit == string::npos)
	    doc.ipath.erase();
	else if (sit < doc.ipath.length() -1)
	    doc.ipath.erase(sit+1);
    } else {
	doc.ipath.erase();
    }
}

// Remove handler from stack. Clean up temp file if needed.
void FileInterner::popHandler()
{
    if (m_handlers.empty())
	return;
    int i = m_handlers.size() - 1;
    if (m_tmpflgs[i]) {
	m_tempfiles.pop_back();
	m_tmpflgs[i] = false;
    }
    returnMimeHandler(m_handlers.back());
    m_handlers.pop_back();
}

enum addResols {ADD_OK, ADD_CONTINUE, ADD_BREAK, ADD_ERROR};

// Just got document from current top handler. See what type it is,
// and possibly add a filter/handler to the stack
int FileInterner::addHandler()
{
    const map<string, string>& docdata = m_handlers.back()->get_meta_data();
    string charset, mimetype;
    getKeyValue(docdata, cstr_dj_keycharset, charset);
    getKeyValue(docdata, cstr_dj_keymt, mimetype);

    LOGDEB(("FileInterner::addHandler: next_doc is %s target [%s]\n", 
	    mimetype.c_str(), m_targetMType.c_str()));

    // If we find a document of the target type (text/plain in
    // general), we're done decoding. If we hit text/plain, we're done
    // in any case
    if (!stringicmp(mimetype, m_targetMType) || 
	!stringicmp(mimetype, cstr_textplain)) {
	m_reachedMType = mimetype;
	LOGDEB1(("FileInterner::addHandler: target reached\n"));
	return ADD_BREAK;
    }

    // We need to stack another handler. Check stack size
    if (m_handlers.size() >= MAXHANDLERS) {
	// Stack too big. Skip this and go on to check if there is
	// something else in the current back()
	LOGERR(("FileInterner::addHandler: stack too high\n"));
	return ADD_CONTINUE;
    }

    RecollFilter *newflt = getMimeHandler(mimetype, m_cfg);
    if (!newflt) {
	// If we can't find a handler, this doc can't be handled
	// but there can be other ones so we go on
	LOGINFO(("FileInterner::addHandler: no filter for [%s]\n",
		 mimetype.c_str()));
	return ADD_CONTINUE;
    }
    newflt->set_property(Dijon::Filter::OPERATING_MODE, 
			 m_forPreview ? "view" : "index");
    if (!charset.empty())
	newflt->set_property(Dijon::Filter::DEFAULT_CHARSET, charset);

    // Get current content: we don't use getkeyvalue() here to avoid
    // copying the text, which may be big.
    string ns;
    const string *txt = &ns;
    {
	map<string,string>::const_iterator it;
	it = docdata.find(cstr_dj_keycontent);
	if (it != docdata.end())
	    txt = &it->second;
    }
    bool setres = false;
    newflt->set_docsize(txt->length());
    if (newflt->is_data_input_ok(Dijon::Filter::DOCUMENT_STRING)) {
	setres = newflt->set_document_string(mimetype, *txt);
    } else if (newflt->is_data_input_ok(Dijon::Filter::DOCUMENT_DATA)) {
	setres = newflt->set_document_data(mimetype,txt->c_str(),txt->length());
    } else if (newflt->is_data_input_ok(Dijon::Filter::DOCUMENT_FILE_NAME)) {
	TempFile temp = dataToTempFile(*txt, mimetype);
	if (temp.isNotNull() && 
	    (setres = newflt->set_document_file(mimetype, temp->filename()))) {
	    m_tmpflgs[m_handlers.size()] = true;
	    m_tempfiles.push_back(temp);
	    // Hack here, but really helps perfs: if we happen to
	    // create a temp file for, ie, an image attachment, keep
	    // it around for preview to use it through get_imgtmp()
	    if (!mimetype.compare(0, 6, "image/")) {
		m_imgtmp = m_tempfiles.back();
	    }
	}
    }
    if (!setres) {
	LOGINFO(("FileInterner::addHandler: set_doc failed inside %s "
		 " for mtype %s\n", m_fn.c_str(), mimetype.c_str()));
	delete newflt;
	if (m_forPreview)
	    return ADD_ERROR;
	return ADD_CONTINUE;
    }
    // add handler and go on, maybe this one will give us text...
    m_handlers.push_back(newflt);
    LOGDEB1(("FileInterner::addHandler: added\n"));
    return ADD_OK;
}

// Information and debug after a next_document error
void FileInterner::processNextDocError(Rcl::Doc &doc)
{
    collectIpathAndMT(doc);
    m_reason = m_handlers.back()->get_error();
    checkExternalMissing(m_reason, doc.mimetype);
    LOGERR(("FileInterner::internfile: next_document error "
	    "[%s%s%s] %s %s\n", m_fn.c_str(), doc.ipath.empty() ? "" : "|", 
	    doc.ipath.c_str(), doc.mimetype.c_str(), m_reason.c_str()));
}

FileInterner::Status FileInterner::internfile(Rcl::Doc& doc, const string& ipath)
{
    LOGDEB(("FileInterner::internfile. ipath [%s]\n", ipath.c_str()));

    // Get rid of possible image tempfile from older call
    m_imgtmp.release();

    if (m_handlers.size() < 1) {
	// Just means the constructor failed
	LOGDEB(("FileInterner::internfile: no handler: constructor failed\n"));
	return FIError;
    }

    // Input Ipath vector when retrieving a given subdoc for previewing
    // Note that the vector is big enough for the maximum stack. All values
    // over the last significant one are ""
    // We set the ipath for the first handler here, others are set
    // when they're pushed on the stack
    vector<string> vipath;
    int vipathidx = 0;
    if (!ipath.empty()) {
	vector<string> lipath;
	stringToTokens(ipath, lipath, cstr_isep, true);
	for (vector<string>::iterator it = lipath.begin();
	     it != lipath.end(); it++) {
	    *it = colon_restore(*it);
	}
	vipath.insert(vipath.begin(), lipath.begin(), lipath.end());
	if (!m_handlers.back()->skip_to_document(vipath[m_handlers.size()-1])){
	    LOGERR(("FileInterner::internfile: can't skip\n"));
	    return FIError;
	}
    }

    // Try to get doc from the topmost handler
    // Security counter: looping happens when we stack one other 
    // handler or when walking the file document tree without finding
    // something to index (typical exemple: email with multiple image
    // attachments and no image filter installed). So we need to be
    // quite generous here, especially because there is another
    // security in the form of a maximum handler stack size.
    int loop = 0;
    while (!m_handlers.empty()) {
        CancelCheck::instance().checkCancel();
	if (loop++ > 1000) {
	    LOGERR(("FileInterner:: looping!\n"));
	    return FIError;
	}
	// If there are no more docs at the current top level we pop and
	// see if there is something at the previous one
	if (!m_handlers.back()->has_documents()) {
            // If looking for a specific doc, this is an error. Happens if
            // the index is stale, and the ipath points to the wrong message
            // for exemple (one with less attachments)
	    if (m_forPreview) {
                m_reason += "Requested document does not exist. ";
                m_reason += m_handlers.back()->get_error();
                LOGERR(("FileInterner: requested document does not exist\n"));
		return FIError;
            }
	    popHandler();
	    continue;
	}

	// While indexing, don't stop on next_document() error. There
	// might be ie an error while decoding an attachment, but we
	// still want to process the rest of the mbox! For preview: fatal.
	if (!m_handlers.back()->next_document()) {
	    processNextDocError(doc);
	    if (m_forPreview) {
                m_reason += "Requested document does not exist. ";
                m_reason += m_handlers.back()->get_error();
                LOGERR(("FileInterner: requested document does not exist\n"));
		return FIError;
            }
	    popHandler();
	    continue;
	}

	// Look at the type for the next document and possibly add
	// handler to stack.
	switch (addHandler()) {
	case ADD_OK: // Just go through: handler has been stacked, use it
	    LOGDEB2(("addHandler returned OK\n"));
	    break; 
	case ADD_CONTINUE: 
	    // forget this doc and retrieve next from current handler
	    // (ipath stays same)
	    LOGDEB2(("addHandler returned CONTINUE\n"));
	    continue;
	case ADD_BREAK: 
	    // Stop looping: doc type ok, need complete its processing
	    // and return it
	    LOGDEB2(("addHandler returned BREAK\n"));
	    goto breakloop; // when you have to you have to
	case ADD_ERROR: 
	    LOGDEB2(("addHandler returned ERROR\n"));
	    return FIError;
	}

	// If we have an ipath, meaning that we are seeking a specific
	// document (ie: previewing a search result), we may have to
	// seek to the correct entry of a compound doc (ie: archive or
	// mail). When we are out of ipath entries, we stop seeking,
	// the handlers stack may still grow for translation (ie: if
	// the target doc is msword, we'll still stack the
	// word-to-text translator).
	if (!ipath.empty()) {
	    if (m_handlers.size() <= vipath.size() &&
		!m_handlers.back()->skip_to_document(vipath[m_handlers.size()-1])) {
		LOGERR(("FileInterner::internfile: can't skip\n"));
		return FIError;
	    }
	}
    }
 breakloop:
    if (m_handlers.empty()) {
	LOGDEB(("FileInterner::internfile: conversion ended with no doc\n"));
	return FIError;
    }

    // Compute ipath and significant mimetype.  ipath is returned
    // through doc.ipath. We also retrieve some metadata fields from
    // the ancesters (like date or author). This is useful for email
    // attachments. The values will be replaced by those internal to
    // the document (by dijontorcl()) if any, so the order of calls is
    // important. We used to only do this when indexing, but the aux
    // fields like filename and author may be interesting when
    // previewing too
    collectIpathAndMT(doc);
    if (m_forPreview) {
	doc.mimetype = m_reachedMType;
    }
    // Keep this AFTER collectIpathAndMT
    dijontorcl(doc);

    // Possibly destack so that we can test for FIDone. While doing this
    // possibly set aside an ancestor html text (for the GUI preview)
    while (!m_handlers.empty() && !m_handlers.back()->has_documents()) {
	if (m_forPreview) {
	    MimeHandlerHtml *hth = 
		dynamic_cast<MimeHandlerHtml*>(m_handlers.back());
	    if (hth) {
		m_html = hth->get_html();
	    }
	}
	popHandler();
    }
    if (m_handlers.empty())
	return FIDone;
    else 
	return FIAgain;
}

// Temporary while we fix backend things
static string urltolocalpath(string url)
{
    return url.substr(7, string::npos);
}

// Extract subdoc out of multidoc into temporary file. 
// We do the usual internfile stuff: create a temporary directory,
// then create an interner and call internfile. The target mtype is set to
// the input mtype, so that no data conversion is performed.
// We then write the data out of the resulting document into the output file.
// There are two temporary objects:
// - The internfile temporary directory gets destroyed by its destructor
// - The output temporary file which is held in a reference-counted
//   object and will be deleted when done with.
// This DOES NOT work with a non-internal file (because at least one conversion 
// is always performed).
bool FileInterner::idocToFile(TempFile& otemp, const string& tofile,
			      RclConfig *cnf, const Rcl::Doc& idoc)
{
    LOGDEB(("FileInterner::idocToFile\n"));
    // idoc.dump();

    if (idoc.ipath.empty()) {
	LOGDEB(("FileInterner::idocToFile: not a sub-document !\n"));
	// We could do a copy here but it's much more complicated than
	// it seems because the source is not necessarily a simple
	// depending on the backend. Until we fix the Internfile
	// constructor to not do the first conversion, it's much saner
	// to just return an error
	return false;
    }

    // We set FIF_forPreview for consistency with the previous version
    // which determined this by looking at mtype!=null. Probably
    // doesn't change anything in this case.
    FileInterner interner(idoc, cnf, FIF_forPreview);
    interner.setTargetMType(idoc.mimetype);
    return interner.interntofile(otemp, tofile, idoc.ipath, idoc.mimetype);
}

bool FileInterner::interntofile(TempFile& otemp, const string& tofile,
				const string& ipath, const string& mimetype)
{
    if (!ok()) {
	LOGERR(("FileInterner::interntofile: constructor failed\n"));
	return false;
    }
    Rcl::Doc doc;
    Status ret = internfile(doc, ipath);
    if (ret == FileInterner::FIError) {
	LOGERR(("FileInterner::interntofile: internfile() failed\n"));
	return false;
    }

    // Specialcase text/html. This is to work around a bug that will
    // get fixed some day: internfile initialisation does not check
    // targetmtype, so that at least one conversion is always
    // performed. A common case would be an "Open" on an html file
    // (we'd end up with text/plain content). As the html version is
    // saved in this case, use it.  
    if (!stringlowercmp("text/html", mimetype) && !get_html().empty()) {
        doc.text = get_html();
        doc.mimetype = "text/html";
    }

    string filename;
    TempFile temp;
    if (tofile.empty()) {
	TempFile temp1(new TempFileInternal(
			   m_cfg->getSuffixFromMimeType(mimetype)));
	temp = temp1;
	if (!temp->ok()) {
	    LOGERR(("FileInterner::interntofile: can't create temp file\n"));
	    return false;
	}
	filename = temp->filename();
    } else {
	filename = tofile;
    }

    int fd = open(filename.c_str(), O_WRONLY|O_CREAT, 0600);
    if (fd < 0) {
	LOGERR(("FileInterner::interntofile: open(%s) failed errno %d\n",
		filename.c_str(), errno));
	return false;
    }
    const string& dt = doc.text;
    if (write(fd, dt.c_str(), dt.length()) != (int)dt.length()) {
	close(fd);
	LOGERR(("FileInterner::interntofile: write to %s failed errno %d\n",
		filename.c_str(), errno));
	return false;
    }
    close(fd);

    if (tofile.empty())
	otemp = temp;
    return true;
}

bool FileInterner::isCompressed(const string& fn, RclConfig *cnf)
{
    LOGDEB(("FileInterner::isCompressed: [%s]\n", fn.c_str()));
    struct stat st;
    if (stat(fn.c_str(), &st) < 0) {
        LOGERR(("FileInterner::isCompressed: can't stat [%s]\n", fn.c_str()));
        return false;
    }
    string l_mime = mimetype(fn, &st, cnf, true);
    if (l_mime.empty()) {
        LOGERR(("FileInterner::isUncompressed: can't get mime for [%s]\n", 
                fn.c_str()));
        return false;
    }

    vector<string> ucmd;
    if (cnf->getUncompressor(l_mime, ucmd)) {
        return true;
    }
    return false;
}

// Static.
bool FileInterner::maybeUncompressToTemp(TempFile& temp, const string& fn, 
                                         RclConfig *cnf, const Rcl::Doc& doc)
{
    LOGDEB(("FileInterner::maybeUncompressToTemp: [%s]\n", fn.c_str()));
    struct stat st;
    if (stat(fn.c_str(), &st) < 0) {
        LOGERR(("FileInterner::maybeUncompressToTemp: can't stat [%s]\n", 
                fn.c_str()));
        return false;
    }
    string l_mime = mimetype(fn, &st, cnf, true);
    if (l_mime.empty()) {
        LOGERR(("FileInterner::maybeUncompress.: can't id. mime for [%s]\n", 
                fn.c_str()));
        return false;
    }

    vector<string>ucmd;
    if (!cnf->getUncompressor(l_mime, ucmd)) {
        return true;
    }
    // Check for compressed size limit
    int maxkbs = -1;
    if (cnf->getConfParam("compressedfilemaxkbs", &maxkbs) &&
        maxkbs >= 0 && int(st.st_size / 1024) > maxkbs) {
        LOGINFO(("FileInterner:: %s over size limit %d kbs\n",
                 fn.c_str(), maxkbs));
        return false;
    }
    temp = 
      TempFile(new TempFileInternal(cnf->getSuffixFromMimeType(doc.mimetype)));
    if (!temp->ok()) {
        LOGERR(("FileInterner: cant create temporary file"));
        return false;
    }

    Uncomp uncomp;
    string uncomped;
    if (!uncomp.uncompressfile(fn, ucmd, uncomped)) {
        return false;
    }

    // uncompressfile choses the output file name, there is good
    // reason for this, but it's not nice here. Have to move, the
    // uncompressed file, hopefully staying on the same dev.
    string reason;
    if (!renameormove(uncomped.c_str(), temp->filename(), reason)) {
        LOGERR(("FileInterner::maybeUncompress: move [%s] -> [%s] "
                "failed: %s\n", 
                uncomped.c_str(), temp->filename(), reason.c_str()));
        return false;
    }
    return true;
}

#else

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <sys/stat.h>

using namespace std;

#include "debuglog.h"
#include "rclinit.h"
#include "internfile.h"
#include "rclconfig.h"
#include "rcldoc.h"

static string thisprog;

static string usage =
    " internfile <filename> [ipath]\n"
    "  \n\n"
    ;

static void
Usage(void)
{
    cerr << thisprog  << ": usage:\n" << usage;
    exit(1);
}

static int        op_flags;
#define OPT_q	  0x1 

RclConfig *config;
int main(int argc, char **argv)
{
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    default: Usage();	break;
	    }
	argc--; argv++;
    }
    DebugLog::getdbl()->setloglevel(DEBDEB1);
    DebugLog::setfilename("stderr");

    if (argc < 1)
	Usage();
    string fn(*argv++);
    argc--;
    string ipath;
    if (argc >= 1) {
	ipath.append(*argv++);
	argc--;
    }
    string reason;
    config = recollinit(0, 0, reason);

    if (config == 0 || !config->ok()) {
	string str = "Configuration problem: ";
	str += reason;
	fprintf(stderr, "%s\n", str.c_str());
	exit(1);
    }
    struct stat st;
    if (stat(fn.c_str(), &st)) {
	perror("stat");
	exit(1);
    }
    FileInterner interner(fn, &st, config, 0);
    Rcl::Doc doc;
    FileInterner::Status status = interner.internfile(doc, ipath);
    switch (status) {
    case FileInterner::FIDone:
    case FileInterner::FIAgain:
	break;
    case FileInterner::FIError:
    default:
	fprintf(stderr, "internfile failed\n");
	exit(1);
    }
    
    cout << "doc.url [[[[" << doc.url << 
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.ipath [[[[" << doc.ipath <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.mimetype [[[[" << doc.mimetype <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.fmtime [[[[" << doc.fmtime <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.dmtime [[[[" << doc.dmtime <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.origcharset [[[[" << doc.origcharset <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.meta[title] [[[[" << doc.meta["title"] <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.meta[keywords] [[[[" << doc.meta["keywords"] <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.meta[abstract] [[[[" << doc.meta["abstract"] <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.text [[[[" << doc.text << "]]]]\n";
}

#endif // TEST_INTERNFILE
