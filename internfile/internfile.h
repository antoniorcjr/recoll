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
#ifndef _INTERNFILE_H_INCLUDED_
#define _INTERNFILE_H_INCLUDED_
#include "autoconfig.h"

#include <string>
#include <vector>
#include <map>
#include <set>
using std::string;
using std::vector;
using std::map;
using std::set;

#include "mimehandler.h"
#include "uncomp.h"
#include "pathut.h"

class RclConfig;
namespace Rcl {
    class Doc;
}

struct stat;

/** Storage for missing helper program info. We want to keep this out of the 
 * FileInterner class, because the data will typically be accumulated by several
 * FileInterner objects. Can't use static member either (because there
 * may be several separate usages of the class which shouldn't mix
 * their data).
 */
class FIMissingStore {
public:
    FIMissingStore() {}
    FIMissingStore(const string& in);
    virtual ~FIMissingStore() {}
    virtual void addMissing(const string& prog, const string& mt)
    {
	m_typesForMissing[prog].insert(mt);
    }
    // Get simple progs list string
    virtual void getMissingExternal(string& out);
    // Get progs + assoc mtypes description string
    virtual void getMissingDescription(string& out);

    // Missing external programs
    map<string, set<string> > m_typesForMissing;
};

/** 
 * Convert data from file-serialized form (either an actual File
 * System file or a memory image) into one or several documents in
 * internal representation (Rcl::Doc). This can be used for indexing,
 * or viewing at query time (GUI preview), or extracting an internal
 * document out of a compound file into a simple one.
 *
 * Things work a little differently when indexing or previewing:
 *  - When indexing, all data has to come from the datastore, and it is 
 *    normally desired that all found subdocuments be returned (ie:
 *    all messages and attachments out of a single file mail folder)
 *  - When previewing, some data is taken from the index (ie: the mime type 
 *    is already known, and a single document usually needs to be processed,
 *    so that the full doc identifier is passed in: high level url 
 *    (ie: file path) and internal identifier: ipath, ie: message and 
 *    attachment number.
 *
 * Internfile is the part of the code which knows about ipath structure. 
 *
 * The class has a number of static helper method which could just as well not
 * be members and are in there just for namespace reasons.
 * 
 */
class FileInterner {
 public:
    /** Operation modifier flags */
    enum Flags {FIF_none, FIF_forPreview, FIF_doUseInputMimetype};
    /** Return values for internfile() */
    enum Status {FIError, FIDone, FIAgain};

    /** Constructors take the initial step to preprocess the data object and
     *  create the top filter */

    /**
     * Identify and possibly decompress file, and create the top filter.
     * - The mtype parameter is not always set (it is when the object is
     *   created for previewing a file). 
     * - Filter output may be different for previewing and indexing.
     *
     * This constructor is now only used for indexing, the form with
     * an Rcl::Doc parameter to identify the data is always used
     * at query time.
     *
     * @param fn file name.
     * @param stp pointer to updated stat struct.
     * @param cnf Recoll configuration.
     * @param td  temporary directory to use as working space if 
     *   decompression needed. Must be private and will be wiped clean.
     * @param mtype mime type if known. For a compressed file this is the 
     *   mime type for the uncompressed version.
     */
    FileInterner(const string &fn, const struct stat *stp, 
		 RclConfig *cnf, int flags, const string *mtype = 0);
    
    /** 
     * Alternate constructor for the case where the data is in memory.
     * This is mainly for data extracted from the web cache. The mime type
     * must be set, input must be already uncompressed.
     */
    FileInterner(const string &data, RclConfig *cnf, 
                 int flags, const string& mtype);

    /**
     * Alternate constructor used at query time. We don't know where
     * the data was stored, this is determined from the Rcl::Doc data
     * 
     * @param idoc Rcl::Doc object built from index data. The back-end
     *   storage identifier (rclbes field) is used to build the
     *   appropriate fetcher which uses the rest of the Doc fields (url,
     *   ipath...) to retrieve the file or a file reference, which we
     *   then process normally.
     */
    FileInterner(const Rcl::Doc& idoc, RclConfig *cnf, int flags);

    ~FileInterner();

    void setMissingStore(FIMissingStore *st)
    {
	m_missingdatap = st;
    }

    /** 
     * Turn file or file part into Recoll document.
     * 
     * For multidocument files (ie: mail folder), this must be called
     * multiple times to retrieve the subdocuments.
     *
     * @param doc output document
     * @param ipath internal path. If set by caller, the specified subdoc will
     *  be returned. Else the next document according to current state will 
     *  be returned, and doc.ipath will be set on output.
     * @return FIError and FIDone are self-explanatory. If FIAgain is returned,
     *  this is a multi-document file, with more subdocs, and internfile() 
     *  should be called again to get the following one(s).
     */
    Status internfile(Rcl::Doc& doc, const string &ipath = "");

    /** Extract subdoc defined by ipath in idoc to file. See params for
	idocToFile() */
    bool interntofile(TempFile& otemp, const string& tofile,
		      const string& ipath, const string& mimetype);

    /** Return the file's (top level object) mimetype (useful for 
     *  creating the pseudo-doc for container files) 
     */ 
    const string&  getMimetype() {return m_mimetype;}

    /** We normally always return text/plain data. A caller can request
     *  that we stop conversion at the native document type (ie: extracting
     *  an email attachment in its native form for an external viewer)
     */
    void setTargetMType(const string& tp) {m_targetMType = tp;}

    /** In case we see an html version while converting, it is set aside 
     *  and can be recovered 
     */
    const string& get_html() {return m_html;}

    /** If we happen to be processing an image file and need a temp file,
	we keep it around to save work for our caller, which can get it here */
    TempFile get_imgtmp() {return m_imgtmp;}

    const string& getReason() const 
    {
	return m_reason;
    }
    bool ok() const
    {
	return m_ok;
    }

    /**
     * Get UDI for immediate parent for document. 
     *
     * This is not in general the same as the "parent" document used 
     * with Rcl::Db::addOrUpdate(). The latter is the enclosing file,
     * this would be for exemple the email containing the attachment.
     * This is in internfile because of the ipath computation.
     */
    static bool getEnclosingUDI(const Rcl::Doc &doc, string& udi);

    /** Return last element in ipath, like basename */
    static std::string getLastIpathElt(const std::string& ipath);

    /** 
     * Build sig for doc coming from rcldb. This is here because we know how
     * to query the right backend. Used to check up-to-dateness at query time */
    static bool makesig(RclConfig *cnf, const Rcl::Doc& idoc, string& sig);

    /** Extract internal document into temporary file. 
     *  This is used mainly for starting an external viewer for a
     *  subdocument (ie: mail attachment). This really would not need to be
     *  a member. It creates a FileInterner object to do the actual work
     * @return true for success.
     * @param temp output reference-counted temp file object (goes
     *   away magically). Only used if tofile.empty()
     * @param tofile output file if not empty.
     * @param cnf The recoll config
     * @param doc Doc data taken from the index. We use it to construct a
     *    FileInterner object.
     */
    static bool idocToFile(TempFile& temp, const string& tofile, 
			   RclConfig *cnf, const Rcl::Doc& doc);

    /** 
     * Does file appear to be the compressed version of a document?
     */
    static bool isCompressed(const string& fn, RclConfig *cnf);

    /** 
     * Check input compressed, allocate temp file and uncompress if it is.  
     * @return true if ok, false for error. Actual decompression is indicated
     *  by the TempFile status (!isNull())
     */
    static bool maybeUncompressToTemp(TempFile& temp, const string& fn, 
                                      RclConfig *cnf, const Rcl::Doc& doc);

 private:
    static const unsigned int MAXHANDLERS = 20;
    RclConfig             *m_cfg;
    string                 m_fn;
    string                 m_mimetype; // Mime type for [uncompressed] file
    bool                   m_forPreview;
    string                 m_html; // Possibly set-aside html text for preview
    TempFile               m_imgtmp; // Possible reference to an image temp file
    string                 m_targetMType;
    string                 m_reachedMType; // target or text/plain
    string                 m_tfile;
    bool                   m_ok; // Set after construction if ok
    // Fields found in file extended attributes. This is kept here,
    // not in the file-level handler because we are only interested in
    // the top-level file, not any temp file necessitated by
    // processing the internal doc hierarchy.
    map<string, string> m_XAttrsFields;
    // Fields gathered by executing configured external commands
    map<string, string> m_cmdFields;

    // Filter stack, path to the current document from which we're
    // fetching subdocs
    vector<RecollFilter*> m_handlers;
    // Temporary files used for decoding the current stack
    bool                   m_tmpflgs[MAXHANDLERS];
    vector<TempFile>       m_tempfiles;
    // Error data if any
    string                 m_reason;
    FIMissingStore        *m_missingdatap;

    Uncomp                 m_uncomp;

    bool                   m_noxattrs; // disable xattrs usage

    // Pseudo-constructors
    void init(const string &fn, const struct stat *stp, 
              RclConfig *cnf, int flags, const string *mtype = 0);
    void init(const string &data, RclConfig *cnf, int flags, 
	      const string& mtype);
    void initcommon(RclConfig *cnf, int flags);

    bool dijontorcl(Rcl::Doc&);
    void collectIpathAndMT(Rcl::Doc&) const;
    TempFile dataToTempFile(const string& data, const string& mt);
    void popHandler();
    int addHandler();
    void checkExternalMissing(const string& msg, const string& mt);
    void processNextDocError(Rcl::Doc &doc);
};

 
#endif /* _INTERNFILE_H_INCLUDED_ */
