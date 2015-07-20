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

#include <qglobal.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFrame>
#include <qlayout.h>
#include <qwidget.h>
#include <qlabel.h>
#include <qtimer.h>
#include <qmessagebox.h>
#include <qcheckbox.h>
#include <QListWidget>

#include <list>
using std::list;

#include "confgui.h"
#include "recoll.h"
#include "confguiindex.h"
#include "smallut.h"
#include "debuglog.h"
#include "rcldb.h"
#include "conflinkrcl.h"
#include "execmd.h"
#include "rclconfig.h"

namespace confgui {
static const int spacing = 3;
static const int margin = 3;

ConfIndexW::ConfIndexW(QWidget *parent, RclConfig *config)
    : QDialog(parent), m_rclconf(config)
{
    setWindowTitle(QString::fromLocal8Bit(config->getConfDir().c_str()));
    tabWidget = new QTabWidget;
    reloadPanels();

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok
				     | QDialogButtonBox::Cancel);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);

    resize(QSize(600, 450).expandedTo(minimumSizeHint()));

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(acceptChanges()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(rejectChanges()));
}

void ConfIndexW::acceptChanges()
{
    LOGDEB(("ConfIndexW::acceptChanges()\n"));
    if (!m_conf) {
	LOGERR(("ConfIndexW::acceptChanges: no config\n"));
	return;
    }
    // Let the changes to disk
    if (!m_conf->holdWrites(false)) {
	QMessageBox::critical(0, "Recoll",  
			      tr("Can't write configuration file"));
    }
    // Delete local copy and update the main one from the file
    delete m_conf;
    m_conf = 0;
    m_rclconf->updateMainConfig();

    hide();
}

void ConfIndexW::rejectChanges()
{
    LOGDEB(("ConfIndexW::rejectChanges()\n"));
    // Discard local changes.
    delete m_conf;
    m_conf = 0;
    hide();
}

void ConfIndexW::reloadPanels()
{
    if ((m_conf = m_rclconf->cloneMainConfig()) == 0) 
	return;
    m_conf->holdWrites(true);
    tabWidget->clear();
    m_widgets.clear();

    QWidget *w = new ConfTopPanelW(this, m_conf);
    m_widgets.push_back(w);
    tabWidget->addTab(w, QObject::tr("Global parameters"));
	
    w = new ConfSubPanelW(this, m_conf, m_rclconf);
    m_widgets.push_back(w);
    tabWidget->addTab(w, QObject::tr("Local parameters"));

    w = new ConfBeaglePanelW(this, m_conf);
    m_widgets.push_back(w);
    tabWidget->addTab(w, QObject::tr("Web history"));

    w = new ConfSearchPanelW(this, m_conf);
    m_widgets.push_back(w);
    tabWidget->addTab(w, QObject::tr("Search parameters"));
}

ConfBeaglePanelW::ConfBeaglePanelW(QWidget *parent, ConfNull *config)
    : QWidget(parent)
{
    QVBoxLayout *vboxLayout = new QVBoxLayout(this);
    vboxLayout->setSpacing(spacing);
    vboxLayout->setMargin(margin);

    ConfLink lnk1(new ConfLinkRclRep(config, "processwebqueue"));
    ConfParamBoolW* cp1 = 
        new ConfParamBoolW(this, lnk1, tr("Process the WEB history queue"),
                           tr("Enables indexing Firefox visited pages.<br>"
			     "(you need also install the Firefox Recoll plugin)"
			  ));
    vboxLayout->addWidget(cp1);

    ConfLink lnk2(new ConfLinkRclRep(config, "webcachedir"));
    ConfParamFNW* cp2 = 
        new ConfParamFNW(this, lnk2, tr("Web page store directory name"),
		     tr("The name for a directory where to store the copies "
			"of visited web pages.<br>"
                        "A non-absolute path is taken relative to the "
			"configuration directory."), true);
    cp2->setEnabled(cp1->m_cb->isChecked());
    connect(cp1->m_cb, SIGNAL(toggled(bool)), cp2, SLOT(setEnabled(bool)));
    vboxLayout->addWidget(cp2);

    ConfLink lnk3(new ConfLinkRclRep(config, "webcachemaxmbs"));
    ConfParamIntW *cp3 =
        new ConfParamIntW(this, lnk3, tr("Max. size for the web store (MB)"),
		      tr("Entries will be recycled once the size is reached"),
                          -1, 1000);
    cp3->setEnabled(cp1->m_cb->isChecked());
    connect(cp1->m_cb, SIGNAL(toggled(bool)), cp3, SLOT(setEnabled(bool)));
    vboxLayout->addWidget(cp3);
    vboxLayout->insertStretch(-1);
}

ConfSearchPanelW::ConfSearchPanelW(QWidget *parent, ConfNull *config)
    : QWidget(parent)
{
    QVBoxLayout *vboxLayout = new QVBoxLayout(this);
    vboxLayout->setSpacing(spacing);
    vboxLayout->setMargin(margin);

    if (!o_index_stripchars) {
	ConfLink lnk1(new ConfLinkRclRep(config, "autodiacsens"));
	ConfParamBoolW* cp1 = 
	    new ConfParamBoolW(this, lnk1, tr("Automatic diacritics sensitivity"),
			       tr("<p>Automatically trigger diacritics sensitivity "
				  "if the search term has accented characters "
				  "(not in unac_except_trans). Else you need to "
				  "use the query language and the <i>D</i> "
				  "modifier to specify "
				  "diacritics sensitivity."
				   ));
	vboxLayout->addWidget(cp1);

	ConfLink lnk2(new ConfLinkRclRep(config, "autocasesens"));
	ConfParamBoolW* cp2 = 
	    new ConfParamBoolW(this, lnk2, 
			       tr("Automatic character case sensitivity"),
			       tr("<p>Automatically trigger character case "
				  "sensitivity if the entry has upper-case "
				  "characters in any but the first position. "
				  "Else you need to use the query language and "
				  "the <i>C</i> modifier to specify character-case "
				  "sensitivity."
				   ));
	vboxLayout->addWidget(cp2);
    }

    ConfLink lnk3(new ConfLinkRclRep(config, "maxTermExpand"));
    ConfParamIntW* cp3 = 
        new ConfParamIntW(this, lnk3, 
			   tr("Maximum term expansion count"),
			   tr("<p>Maximum expansion count for a single term "
			      "(e.g.: when using wildcards). The default "
			      "of 10 000 is reasonable and will avoid "
			      "queries that appear frozen while the engine is "
			      "walking the term list."
			       ));
    vboxLayout->addWidget(cp3);


    ConfLink lnk4(new ConfLinkRclRep(config, "maxXapianClauses"));
    ConfParamIntW* cp4 = 
        new ConfParamIntW(this, lnk4, 
			   tr("Maximum Xapian clauses count"),
			   tr("<p>Maximum number of elementary clauses we "
			      "add to a single Xapian query. In some cases, "
			      "the result of term expansion can be "
			      "multiplicative, and we want to avoid using "
			      "excessive memory. The default of 100 000 "
			      "should be both high enough in most cases "
			      "and compatible with current typical hardware "
			      "configurations."
			       ));
    vboxLayout->addWidget(cp4);

    vboxLayout->insertStretch(-1);
}

ConfTopPanelW::ConfTopPanelW(QWidget *parent, ConfNull *config)
    : QWidget(parent)
{
    QWidget *w = 0;

    QGridLayout *gl1 = new QGridLayout(this);
    gl1->setSpacing(spacing);
    gl1->setMargin(margin);

    int gridrow = 0;

    w = new ConfParamDNLW(this, 
                          ConfLink(new ConfLinkRclRep(config, "topdirs")), 
                          tr("Top directories"),
                          tr("The list of directories where recursive "
                             "indexing starts. Default: your home."));
    setSzPol(w, QSizePolicy::Preferred, QSizePolicy::Preferred, 1, 3);
    gl1->addWidget(w, gridrow++, 0, 1, 2);

    ConfParamSLW *eskp = new 
	ConfParamSLW(this, 
                     ConfLink(new ConfLinkRclRep(config, "skippedPaths")), 
                     tr("Skipped paths"),
		     tr("These are names of directories which indexing "
			"will not enter.<br> May contain wildcards. "
			"Must match "
			"the paths seen by the indexer (ie: if topdirs "
			"includes '/home/me' and '/home' is actually a link "
			"to '/usr/home', a correct skippedPath entry "
			"would be '/home/me/tmp*', not '/usr/home/me/tmp*')"));
    eskp->setFsEncoding(true);
    setSzPol(eskp, QSizePolicy::Preferred, QSizePolicy::Preferred, 1, 3);
    gl1->addWidget(eskp, gridrow++, 0, 1, 2);

    vector<string> cstemlangs = Rcl::Db::getStemmerNames();
    QStringList stemlangs;
    for (vector<string>::const_iterator it = cstemlangs.begin(); 
	 it != cstemlangs.end(); it++) {
	stemlangs.push_back(QString::fromUtf8(it->c_str()));
    }
    w = new 
	ConfParamCSLW(this, 
                      ConfLink(new ConfLinkRclRep(config, 
                                                  "indexstemminglanguages")),
                      tr("Stemming languages"),
		      tr("The languages for which stemming expansion<br>"
			 "dictionaries will be built."), stemlangs);
    setSzPol(w, QSizePolicy::Preferred, QSizePolicy::Preferred, 1, 1);
    gl1->addWidget(w, gridrow, 0);
    

    w = new ConfParamFNW(this, 
                         ConfLink(new ConfLinkRclRep(config, "logfilename")), 
                         tr("Log file name"),
                         tr("The file where the messages will be written.<br>"
                            "Use 'stderr' for terminal output"), false);
    gl1->addWidget(w, gridrow++, 1);

    
    w = new ConfParamIntW(this, 
                          ConfLink(new ConfLinkRclRep(config, "loglevel")), 
                          tr("Log verbosity level"),
                          tr("This value adjusts the amount of "
                             "messages,<br>from only errors to a "
                             "lot of debugging data."), 0, 6);
    gl1->addWidget(w, gridrow, 0);

    w = new ConfParamIntW(this, 
                          ConfLink(new ConfLinkRclRep(config, "idxflushmb")),
                          tr("Index flush megabytes interval"),
                          tr("This value adjust the amount of "
			 "data which is indexed between flushes to disk.<br>"
			 "This helps control the indexer memory usage. "
			 "Default 10MB "), 0, 1000);
    gl1->addWidget(w, gridrow++, 1);

    w = new ConfParamIntW(this, 
                          ConfLink(new ConfLinkRclRep(config, "maxfsoccuppc")),
                          tr("Max disk occupation (%)"),
                          tr("This is the percentage of disk occupation where "
                             "indexing will fail and stop (to avoid filling up "
                             "your disk).<br>0 means no limit "
                             "(this is the default)."), 0, 100);
    gl1->addWidget(w, gridrow++, 0);

    ConfParamBoolW* cpasp =
        new ConfParamBoolW(this, 
                           ConfLink(new ConfLinkRclRep(config, "noaspell")), 
                           tr("No aspell usage"),
                           tr("Disables use of aspell to generate spelling "
                              "approximation in the term explorer tool.<br> "
                              "Useful if aspell is absent or does not work. "));
    gl1->addWidget(cpasp, gridrow, 0);

    ConfParamStrW* cpaspl = new 
        ConfParamStrW(this, 
                      ConfLink(new ConfLinkRclRep(config, "aspellLanguage")),
                      tr("Aspell language"),
                      tr("The language for the aspell dictionary. "
                         "This should look like 'en' or 'fr' ...<br>"
                         "If this value is not set, the NLS environment "
			 "will be used to compute it, which usually works. "
			 "To get an idea of what is installed on your system, "
			 "type 'aspell config' and look for .dat files inside "
			 "the 'data-dir' directory. "));
    cpaspl->setEnabled(!cpasp->m_cb->isChecked());
    connect(cpasp->m_cb, SIGNAL(toggled(bool)), cpaspl,SLOT(setDisabled(bool)));
    gl1->addWidget(cpaspl, gridrow++, 1);

    w = new 
        ConfParamFNW(this, 
                     ConfLink(new ConfLinkRclRep(config, "dbdir")),
                     tr("Database directory name"),
                     tr("The name for a directory where to store the index<br>"
			"A non-absolute path is taken relative to the "
			"configuration directory. The default is 'xapiandb'."
			), true);
    gl1->addWidget(w, gridrow++, 0, 1, 2);
    
    w = new 
	ConfParamStrW(this, 
                      ConfLink(new ConfLinkRclRep(config, "unac_except_trans")),
                      tr("Unac exceptions"),
                      tr("<p>These are exceptions to the unac mechanism "
                         "which, by default, removes all diacritics, "
                         "and performs canonic decomposition. You can override "
                         "unaccenting for some characters, depending on your "
                         "language, and specify additional decompositions, "
                         "e.g. for ligatures. In each space-separated entry, "
                         "the first character is the source one, and the rest "
                         "is the translation."
                          ));
    gl1->addWidget(w, gridrow++, 0, 1, 2);
}

ConfSubPanelW::ConfSubPanelW(QWidget *parent, ConfNull *config, 
                             RclConfig *rclconf)
    : QWidget(parent), m_config(config)
{
    QVBoxLayout *vboxLayout = new QVBoxLayout(this);
    vboxLayout->setSpacing(spacing);
    vboxLayout->setMargin(margin);

    m_subdirs = new 
	ConfParamDNLW(this, 
                      ConfLink(new ConfLinkNullRep()), 
		      QObject::tr("<b>Customised subtrees"),
		      QObject::tr("The list of subdirectories in the indexed "
				  "hierarchy <br>where some parameters need "
				  "to be redefined. Default: empty."));
    m_subdirs->getListBox()->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_subdirs->getListBox(), 
	    SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
	    this, 
	    SLOT(subDirChanged(QListWidgetItem *, QListWidgetItem *)));
    connect(m_subdirs, SIGNAL(entryDeleted(QString)),
	    this, SLOT(subDirDeleted(QString)));

    // We only retrieve the subkeys from the user's config (shallow),
    // no use to confuse the user by showing the subtrees which are
    // customized in the system config like .thunderbird or
    // .purple. This doesn't prevent them to add and customize them
    // further.
    vector<string> allkeydirs = config->getSubKeys(true); 

    QStringList qls;
    for (vector<string>::const_iterator it = allkeydirs.begin(); 
	 it != allkeydirs.end(); it++) {
	qls.push_back(QString::fromUtf8(it->c_str()));
    }
    m_subdirs->getListBox()->insertItems(0, qls);
    vboxLayout->addWidget(m_subdirs);

    QFrame *line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    vboxLayout->addWidget(line2);

    QLabel *explain = new QLabel(this);
    explain->setText(
		     QObject::
		     tr("<i>The parameters that follow are set either at the "
			"top level, if nothing<br>"
			"or an empty line is selected in the listbox above, "
			"or for the selected subdirectory.<br>"
			"You can add or remove directories by clicking "
			"the +/- buttons."));
    vboxLayout->addWidget(explain);


    m_groupbox = new QGroupBox(this);
    setSzPol(m_groupbox, QSizePolicy::Preferred, QSizePolicy::Preferred, 1, 3);

    QGridLayout *gl1 = new QGridLayout(m_groupbox);
    gl1->setSpacing(spacing);
    gl1->setMargin(margin);
    int gridy = 0;

    ConfParamSLW *eskn = new ConfParamSLW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "skippedNames", &m_sk)),
        QObject::tr("Skipped names"),
        QObject::tr("These are patterns for file or directory "
                    " names which should not be indexed."));
    eskn->setFsEncoding(true);
    m_widgets.push_back(eskn);
    gl1->addWidget(eskn, gridy, 0);

    vector<string> amimes = rclconf->getAllMimeTypes();
    QStringList amimesq;
    for (vector<string>::const_iterator it = amimes.begin(); 
	 it != amimes.end(); it++) {
	amimesq.push_back(QString::fromUtf8(it->c_str()));
    }

    ConfParamCSLW *eincm = new ConfParamCSLW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "indexedmimetypes", &m_sk)),
        tr("Only mime types"),
        tr("An exclusive list of indexed mime types.<br>Nothing "
           "else will be indexed. Normally empty and inactive"), amimesq);
    m_widgets.push_back(eincm);
    gl1->addWidget(eincm, gridy++, 1);

    ConfParamCSLW *eexcm = new ConfParamCSLW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "excludedmimetypes", &m_sk)),
        tr("Exclude mime types"),
        tr("Mime types not to be indexed"), amimesq);
    m_widgets.push_back(eexcm);
    gl1->addWidget(eexcm, gridy, 0);

    vector<string> args;
    args.push_back("-l");
    ExecCmd ex;
    string icout;
    string cmd = "iconv";
    int status = ex.doexec(cmd, args, 0, &icout);
    if (status) {
	LOGERR(("Can't get list of charsets from 'iconv -l'"));
    }
    icout = neutchars(icout, ",");
    list<string> ccsets;
    stringToStrings(icout, ccsets);
    QStringList charsets;
    charsets.push_back("");
    for (list<string>::const_iterator it = ccsets.begin(); 
	 it != ccsets.end(); it++) {
	charsets.push_back(QString::fromUtf8(it->c_str()));
    }

    ConfParamCStrW *e21 = new ConfParamCStrW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "defaultcharset", &m_sk)), 
        QObject::tr("Default<br>character set"),
        QObject::tr("Character set used for reading files "
                    "which do not identify the character set "
                    "internally, for example pure text files.<br>"
                    "The default value is empty, "
                    "and the value from the NLS environnement is used."
            ), charsets);
    m_widgets.push_back(e21);
    gl1->addWidget(e21, gridy++, 1);

    ConfParamBoolW *e3 = new ConfParamBoolW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "followLinks", &m_sk)), 
        QObject::tr("Follow symbolic links"),
        QObject::tr("Follow symbolic links while "
                    "indexing. The default is no, "
                    "to avoid duplicate indexing"));
    m_widgets.push_back(e3);
    gl1->addWidget(e3, gridy, 0);

    ConfParamBoolW *eafln = new ConfParamBoolW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "indexallfilenames", &m_sk)), 
        QObject::tr("Index all file names"),
        QObject::tr("Index the names of files for which the contents "
                    "cannot be identified or processed (no or "
                    "unsupported mime type). Default true"));
    m_widgets.push_back(eafln);
    gl1->addWidget(eafln, gridy++, 1);

    ConfParamIntW *ezfmaxkbs = new ConfParamIntW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "compressedfilemaxkbs", &m_sk)), 
        tr("Max. compressed file size (KB)"),
        tr("This value sets a threshold beyond which compressed"
           "files will not be processed. Set to -1 for no "
           "limit, to 0 for no decompression ever."), -1, 1000000, -1);
    m_widgets.push_back(ezfmaxkbs);
    gl1->addWidget(ezfmaxkbs, gridy, 0);

    ConfParamIntW *etxtmaxmbs = new ConfParamIntW(
        m_groupbox,
        ConfLink(new ConfLinkRclRep(config, "textfilemaxmbs", &m_sk)), 
        tr("Max. text file size (MB)"),
        tr("This value sets a threshold beyond which text "
           "files will not be processed. Set to -1 for no "
           "limit. \nThis is for excluding monster "
           "log files from the index."), -1, 1000000);
    m_widgets.push_back(etxtmaxmbs);
    gl1->addWidget(etxtmaxmbs, gridy++, 1);

    ConfParamIntW *etxtpagekbs = new ConfParamIntW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "textfilepagekbs", &m_sk)),
        tr("Text file page size (KB)"),
        tr("If this value is set (not equal to -1), text "
           "files will be split in chunks of this size for "
           "indexing.\nThis will help searching very big text "
           " files (ie: log files)."), -1, 1000000);
    m_widgets.push_back(etxtpagekbs);
    gl1->addWidget(etxtpagekbs, gridy, 0);

    ConfParamIntW *efiltmaxsecs = new ConfParamIntW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "filtermaxseconds", &m_sk)), 
        tr("Max. filter exec. time (S)"),
        tr("External filters working longer than this will be "
           "aborted. This is for the rare case (ie: postscript) "
           "where a document could cause a filter to loop. "
           "Set to -1 for no limit.\n"), -1, 10000);
    m_widgets.push_back(efiltmaxsecs);
    gl1->addWidget(efiltmaxsecs, gridy++, 1);

    vboxLayout->addWidget(m_groupbox);
    subDirChanged(0, 0);
}

void ConfSubPanelW::reloadAll()
{
    for (list<ConfParamW*>::iterator it = m_widgets.begin();
	 it != m_widgets.end(); it++) {
	(*it)->loadValue();
    }
}

void ConfSubPanelW::subDirChanged(QListWidgetItem *current, QListWidgetItem *)
{
    LOGDEB(("ConfSubPanelW::subDirChanged\n"));
	
    if (current == 0 || current->text() == "") {
	m_sk = "";
	m_groupbox->setTitle(tr("Global"));
    } else {
	m_sk = (const char *) current->text().toUtf8();
	m_groupbox->setTitle(current->text());
    }
    LOGDEB(("ConfSubPanelW::subDirChanged: now [%s]\n", m_sk.c_str()));
    reloadAll();
}

void ConfSubPanelW::subDirDeleted(QString sbd)
{
    LOGDEB(("ConfSubPanelW::subDirDeleted(%s)\n", (const char *)sbd.toUtf8()));
    if (sbd == "") {
	// Can't do this, have to reinsert it
	QTimer::singleShot(0, this, SLOT(restoreEmpty()));
	return;
    }
    // Have to delete all entries for submap
    m_config->eraseKey((const char *)sbd.toUtf8());
}

void ConfSubPanelW::restoreEmpty()
{
    LOGDEB(("ConfSubPanelW::restoreEmpty()\n"));
    m_subdirs->getListBox()->insertItem(0, "");
}

} // Namespace confgui
