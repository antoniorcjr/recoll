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
#ifndef _RCLCONFIG_H_INCLUDED_
#define _RCLCONFIG_H_INCLUDED_

#include <string>
#include <vector>
#include <set>
#include <utility>
#include <map>
#include "unordered_defs.h"

using std::string;
using std::vector;
using std::pair;
using std::set;
using std::map;


#include "conftree.h"
#include "smallut.h"

class RclConfig;

// A small class used for parameters that need to be computed from the
// config string, and which can change with the keydir. Minimize work
// by using the keydirgen and a saved string to avoid unneeded
// recomputations
class ParamStale {
public:
    RclConfig *parent;
    ConfNull  *conffile;
    string    paramname;
    bool      active; // Check at init if config defines name at all
    int       savedkeydirgen;
    string    savedvalue;

    ParamStale(RclConfig *rconf, const string& nm);
    void init(ConfNull *cnf);
    bool needrecompute();
};

// Hold the description for an external metadata-gathering command
struct MDReaper {
  string fieldname;
  vector<string> cmdv;
};

// Data associated to a indexed field name: 
struct FieldTraits {
    string pfx; // indexing prefix, 
    int    wdfinc; // Index time term frequency increment (default 1)
    double boost; // Query time boost (default 1.0)
    bool   pfxonly; // Suppress prefix-less indexing

    FieldTraits() 
        : wdfinc(1), boost(1.0), pfxonly(false)
        {}
};

class RclConfig {
 public:

    // Constructor: we normally look for a configuration file, except
    // if this was specified on the command line and passed through
    // argcnf
    RclConfig(const string *argcnf = 0);

    RclConfig(const RclConfig &r) 
    : m_stpsuffstate(this, "recoll_noindex"),
      m_skpnstate(this, "skippedNames"),
      m_rmtstate(this, "indexedmimetypes"),
      m_xmtstate(this, "excludedmimetypes"),
      m_mdrstate(this, "metadatacmds") {
        initFrom(r);
    }

    ~RclConfig() {
	freeAll();
    }

    // Return a writable clone of the main config. This belongs to the
    // caller (must delete it when done)
    ConfNull *cloneMainConfig();

    /** (re)Read recoll.conf */
    bool updateMainConfig();

    bool ok() const {return m_ok;}
    const string &getReason() const {return m_reason;}

    /** Return the directory where this configuration is stored. 
     *  This was possibly silently created by the rclconfig
     *  constructor it it is the default one (~/.recoll) and it did 
     *  not exist yet. */
    string getConfDir() const {return m_confdir;}

    /** Check if the config files were modified since we read them */
    bool sourceChanged() const;

    /** Returns true if this is ~/.recoll */
    bool isDefaultConfig() const;
    /** Get the local value for /usr/local/share/recoll/ */
    const string& getDatadir() const {return m_datadir;}

    /** Set current directory reference, and fetch automatic parameters. */
    void setKeyDir(const string &dir);
    string getKeyDir() const {return m_keydir;}

    /** Get generic configuration parameter according to current keydir */
    bool getConfParam(const string &name, string &value) const
    {
	if (m_conf == 0)
	    return false;
	return m_conf->get(name, value, m_keydir);
    }
    /** Variant with autoconversion to int */
    bool getConfParam(const string &name, int *value) const;
    /** Variant with autoconversion to bool */
    bool getConfParam(const string &name, bool *value) const;
    /** Variant with conversion to vector<string>
     *  (stringToStrings). Can fail if the string is malformed. */
    bool getConfParam(const string &name, vector<string> *value) const;
    /** Variant with conversion to vector<int> */
    bool getConfParam(const string &name, vector<int> *value) const;

    enum ThrStage {ThrIntern=0, ThrSplit=1, ThrDbWrite=2};
    pair<int, int> getThrConf(ThrStage who) const;

    /** 
     * Get list of config names under current sk, with possible 
     * wildcard filtering 
     */
    vector<string> getConfNames(const char *pattern = 0) const
    {
	return m_conf->getNames(m_keydir, pattern);
    }

    /** Check if name exists anywhere in config */
    bool hasNameAnywhere(const string& nm) const
    {
        return m_conf? m_conf->hasNameAnywhere(nm) : false;
    }


    /** Get default charset for current keydir (was set during setKeydir) 
     * filenames are handled differently */
    const string &getDefCharset(bool filename = false) const;

    /** Get list of top directories. This is needed from a number of places
     * and needs some cleaning-up code. An empty list is always an error, no
     * need for other status */
    vector<string> getTopdirs() const;

    /** Get database directory */
    string getDbDir() const;
    /** Get stoplist file name */
    string getStopfile() const;
    /** Get indexing pid file name */
    string getPidfile() const;
    /** Get indexing status file name */
    string getIdxStatusFile() const;
    /** Do path translation according to the ptrans table */
    void urlrewrite(const string& dbdir, string& url) const;
    ConfSimple *getPTrans()
    {
	return m_ptrans;
    }
    /** Get Web Queue directory name */
    string getWebQueueDir() const;

    /** Get list of skipped file names for current keydir */
    vector<string>& getSkippedNames();

    /** Get list of skipped paths patterns. Doesn't depend on the keydir */
    vector<string> getSkippedPaths() const;
    /** Get list of skipped paths patterns, daemon version (may add some)
	Doesn't depend on the keydir */
    vector<string> getDaemSkippedPaths() const;

    /** 
     * mimemap: Check if file name should be ignored because of suffix
     *
     * The list of ignored suffixes is initialized on first call, and
     * not changed for subsequent setKeydirs.
     */
    bool inStopSuffixes(const string& fn);

    /** 
     * Check in mimeconf if input mime type is a compressed one, and
     * return command to uncompress if it is.
     *
     * The returned command has substitutable places for input file name 
     * and temp dir name, and will return output name
     */
    bool getUncompressor(const string &mtpe, vector<string>& cmd) const;

    /** mimemap: compute mimetype */
    string getMimeTypeFromSuffix(const string &suffix) const;
    /** mimemap: get a list of all indexable mime types defined */
    vector<string> getAllMimeTypes() const;
    /** mimemap: Get appropriate suffix for mime type. This is inefficient */
    string getSuffixFromMimeType(const string &mt) const;

    /** mimeconf: get input filter for mimetype */
    string getMimeHandlerDef(const string &mimetype, bool filtertypes=false);

    /** For lines like: "name = some value; attr1 = value1; attr2 = val2"
     * Separate the value and store the attributes in a ConfSimple 
     * @param whole the raw value. No way to escape a semi-colon in there.
     */
    static bool valueSplitAttributes(const string& whole, string& value, 
				     ConfSimple& attrs) ;

    /** Return icon path for mime type and tag */
    string getMimeIconPath(const string &mt, const string& apptag) const;

    /** mimeconf: get list of file categories */
    bool getMimeCategories(vector<string>&) const;
    /** mimeconf: is parameter one of the categories ? */
    bool isMimeCategory(string&) const;
    /** mimeconf: get list of mime types for category */
    bool getMimeCatTypes(const string& cat, vector<string>&) const;

    /** mimeconf: get list of gui filters (doc cats by default */
    bool getGuiFilterNames(vector<string>&) const;
    /** mimeconf: get query lang frag for named filter */
    bool getGuiFilter(const string& filtername, string& frag) const;

    /** fields: get field prefix from field name. Use additional query
       aliases if isquery is set */
    bool getFieldTraits(const string& fldname, const FieldTraits **,
        bool isquery = false) const;

    const set<string>& getStoredFields() const {return m_storedFields;}

    set<string> getIndexedFields() const;

    /** Get canonic name for possible alias */
    string fieldCanon(const string& fld) const;

    /** Get canonic name for possible alias, including query-only aliases */
    string fieldQCanon(const string& fld) const;

    /** Get xattr name to field names translations */
    const map<string, string>& getXattrToField() const {return m_xattrtofld;}

    /** Get value of a parameter inside the "fields" file. Only some filters 
     * use this (ie: mh_mail). The information specific to a given filter
     * is typically stored in a separate section(ie: [mail]) 
     */
    vector<string> getFieldSectNames(const string &sk, const char* = 0) const;
    bool getFieldConfParam(const string &name, const string &sk, string &value)
    const;

    /** mimeview: get/set external viewer exec string(s) for mimetype(s) */
    string getMimeViewerDef(const string &mimetype, const string& apptag, 
			    bool useall) const;
    string getMimeViewerAllEx() const;
    bool setMimeViewerAllEx(const string& allex);
    bool getMimeViewerDefs(vector<pair<string, string> >&) const;
    bool setMimeViewerDef(const string& mimetype, const string& cmd);
    /** Check if mime type is designated as needing no uncompress before view
     * (if a file of this type is found compressed). Default is true,
     *  exceptions are found in the nouncompforviewmts mimeview list */
    bool mimeViewerNeedsUncomp(const string &mimetype) const;

    /** Retrieve extra metadata-gathering commands */
    const vector<MDReaper>& getMDReapers();

    /** Store/retrieve missing helpers description string */
    bool getMissingHelperDesc(string&) const;
    void storeMissingHelperDesc(const string &s);

    /** Find exec file for external filter. cmd is the command name from the
     * command string returned by getMimeHandlerDef */
    string findFilter(const string& cmd) const;

    /** Thread config init is not done automatically because not all
	programs need it and it uses debuglog so that it's better to
	call it after primary init */
    void initThrConf();

    const string& getOrigCwd() 
    {
	return o_origcwd;
    }

    RclConfig& operator=(const RclConfig &r) {
	if (this != &r) {
	    freeAll();
	    initFrom(r);
	}
	return *this;
    }

    friend class ParamStale;

 private:
    int m_ok;
    string m_reason;    // Explanation for bad state
    string m_confdir;   // User directory where the customized files are stored
    string m_datadir;   // Example: /usr/local/share/recoll
    string m_keydir;    // Current directory used for parameter fetches.
    int    m_keydirgen; // To help with knowing when to update computed data.

    vector<string> m_cdirs; // directory stack for the confstacks

    ConfStack<ConfTree> *m_conf;   // Parsed configuration files
    ConfStack<ConfTree> *mimemap;  // The files don't change with keydir, 
    ConfStack<ConfSimple> *mimeconf; // but their content may depend on it.
    ConfStack<ConfSimple> *mimeview; // 
    ConfStack<ConfSimple> *m_fields;
    ConfSimple            *m_ptrans; // Paths translations
    map<string, FieldTraits>  m_fldtotraits; // Field to field params
    map<string, string>  m_aliastocanon;
    map<string, string>  m_aliastoqcanon;
    set<string>          m_storedFields;
    map<string, string>  m_xattrtofld;

    void        *m_stopsuffixes;
    unsigned int m_maxsufflen;
    ParamStale   m_stpsuffstate;

    ParamStale   m_skpnstate;
    vector<string> m_skpnlist;

    // Original current working directory. Set once at init before we do any
    // chdir'ing and used for converting user args to absolute paths.
    static string o_origcwd;

    // Parameters auto-fetched on setkeydir
    string m_defcharset;
    static string o_localecharset;
    // Limiting set of mime types to be processed. Normally empty.
    ParamStale    m_rmtstate;
    STD_UNORDERED_SET<string>   m_restrictMTypes; 
    // Exclusion set of mime types. Normally empty
    ParamStale    m_xmtstate;
    STD_UNORDERED_SET<string>   m_excludeMTypes; 

    vector<pair<int, int> > m_thrConf;

    // Same idea with the metadata-gathering external commands,
    // (e.g. used to reap tagging info: "tmsu tags %f")
    ParamStale    m_mdrstate;
    vector<MDReaper> m_mdreapers;

    /** Create initial user configuration */
    bool initUserConfig();
    /** Copy from other */
    void initFrom(const RclConfig& r);
    /** Init pointers to 0 */
    void zeroMe();
    /** Free data then zero pointers */
    void freeAll();
    bool readFieldsConfig(const string& errloc);
};

// This global variable defines if we are running with an index
// stripped of accents and case or a raw one. Ideally, it should be
// constant, but it needs to be initialized from the configuration, so
// there is no way to do this. It never changes after initialization
// of course. Changing the value on a given index imposes a
// reset. When using multiple indexes, all must have the same value
extern bool o_index_stripchars;

// This global variable defines if we use mtime instead of ctime for
// up-to-date tests. This is mostly incompatible with xattr indexing,
// in addition to other issues. See recoll.conf comments. 
extern bool o_uptodate_test_use_mtime;

#endif /* _RCLCONFIG_H_INCLUDED_ */
