/* Copyright (C) 2005 J.F.Dockes
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

#include <sys/stat.h>

#include <string>
#include <algorithm>
#include <list>

#include <qfontdialog.h>
#include <qspinbox.h>
#include <qmessagebox.h>
#include <qvariant.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#include <qlistwidget.h>
#include <qwidget.h>
#include <qlabel.h>
#include <qspinbox.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qtextedit.h>
#include <qlist.h>
#include <QTimer>
#include <QListWidget>

#include "recoll.h"
#include "guiutils.h"
#include "rcldb.h"
#include "rclconfig.h"
#include "pathut.h"
#include "uiprefs_w.h"
#include "viewaction_w.h"
#include "debuglog.h"
#include "editdialog.h"
#include "rclmain_w.h"
#include "ptrans_w.h"

void UIPrefsDialog::init()
{
    m_viewAction = 0;

    connect(viewActionPB, SIGNAL(clicked()), this, SLOT(showViewAction()));
    connect(reslistFontPB, SIGNAL(clicked()), this, SLOT(showFontDialog()));
    connect(resetFontPB, SIGNAL(clicked()), this, SLOT(resetReslistFont()));

    connect(stylesheetPB, SIGNAL(clicked()),this, SLOT(showStylesheetDialog()));
    connect(resetSSPB, SIGNAL(clicked()), this, SLOT(resetStylesheet()));
    connect(snipCssPB, SIGNAL(clicked()),this, SLOT(showSnipCssDialog()));
    connect(resetSnipCssPB, SIGNAL(clicked()), this, SLOT(resetSnipCss()));

    connect(idxLV, SIGNAL(itemSelectionChanged()),
	    this, SLOT(extradDbSelectChanged()));
    connect(ptransPB, SIGNAL(clicked()),
	    this, SLOT(extraDbEditPtrans()));
    connect(addExtraDbPB, SIGNAL(clicked()), 
	    this, SLOT(addExtraDbPB_clicked()));
    connect(delExtraDbPB, SIGNAL(clicked()), 
	    this, SLOT(delExtraDbPB_clicked()));
    connect(togExtraDbPB, SIGNAL(clicked()), 
	    this, SLOT(togExtraDbPB_clicked()));
    connect(actAllExtraDbPB, SIGNAL(clicked()), 
	    this, SLOT(actAllExtraDbPB_clicked()));
    connect(unacAllExtraDbPB, SIGNAL(clicked()), 
	    this, SLOT(unacAllExtraDbPB_clicked()));
    connect(CLEditPara, SIGNAL(clicked()), this, SLOT(editParaFormat()));
    connect(CLEditHeader, SIGNAL(clicked()), this, SLOT(editHeaderText()));
    connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
    connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(buildAbsCB, SIGNAL(toggled(bool)), 
	    replAbsCB, SLOT(setEnabled(bool)));
    connect(ssAutoAllCB, SIGNAL(toggled(bool)), 
	    ssAutoSpaceCB, SLOT(setDisabled(bool)));
    connect(ssAutoAllCB, SIGNAL(toggled(bool)), 
	    ssAutoSpaceCB, SLOT(setChecked(bool)));
    setFromPrefs();
}

// Update dialog state from stored prefs
void UIPrefsDialog::setFromPrefs()
{
    // Entries per result page spinbox
    pageLenSB->setValue(prefs.respagesize);
    collapseDupsCB->setChecked(prefs.collapseDuplicates);
    maxHLTSB->setValue(prefs.maxhltextmbs);

    switch (prefs.filterCtlStyle) {
    case PrefsPack::FCS_MN:
	filterMN_RB->setChecked(1);
	break;
    case PrefsPack::FCS_CMB:
	filterCMB_RB->setChecked(1);
	break;
    case PrefsPack::FCS_BT:
    default:
        fprintf(stderr, "filter ctl style %d\n", prefs.filterCtlStyle);
	filterBT_RB->setChecked(1);
	break;
    }
    ssAutoSpaceCB->setChecked(prefs.ssearchOnWS);
    ssNoCompleteCB->setChecked(prefs.ssearchNoComplete);
    ssAutoAllCB->setChecked(prefs.ssearchAsYouType);
    syntlenSB->setValue(prefs.syntAbsLen);
    syntctxSB->setValue(prefs.syntAbsCtx);

    initStartAdvCB->setChecked(prefs.startWithAdvSearchOpen);

    keepSortCB->setChecked(prefs.keepSort);
    showTrayIconCB->setChecked(prefs.showTrayIcon);
    closeToTrayCB->setChecked(prefs.closeToTray);
    previewHtmlCB->setChecked(prefs.previewHtml);
    switch (prefs.previewPlainPre) {
    case PrefsPack::PP_BR:
	plainBRRB->setChecked(1);
	break;
    case PrefsPack::PP_PRE:
	plainPRERB->setChecked(1);
	break;
    case PrefsPack::PP_PREWRAP:
    default:
	plainPREWRAPRB->setChecked(1);
	break;
    }
    // Query terms color
    qtermColorLE->setText(prefs.qtermcolor);
    // Abstract snippet separator string
    abssepLE->setText(prefs.abssep);
    dateformatLE->setText(prefs.reslistdateformat);

    // Result list font family and size
    reslistFontFamily = prefs.reslistfontfamily;
    reslistFontSize = prefs.reslistfontsize;
    setupReslistFontPB();

    // Style sheet
    qssFile = prefs.qssFile;
    if (qssFile.isEmpty()) {
	stylesheetPB->setText(tr("Choose"));
    } else {
	string nm = path_getsimple((const char *)qssFile.toLocal8Bit());
	stylesheetPB->setText(QString::fromLocal8Bit(nm.c_str()));
    }

    snipCssFile = prefs.snipCssFile;
    if (snipCssFile.isEmpty()) {
	snipCssPB->setText(tr("Choose"));
    } else {
	string nm = path_getsimple((const char *)snipCssFile.toLocal8Bit());
	snipCssPB->setText(QString::fromLocal8Bit(nm.c_str()));
    }

    paraFormat = prefs.reslistformat;
    headerText = prefs.reslistheadertext;

    // Stemming language combobox
    stemLangCMB->clear();
    stemLangCMB->addItem(g_stringNoStem);
    stemLangCMB->addItem(g_stringAllStem);
    vector<string> langs;
    if (!getStemLangs(langs)) {
	QMessageBox::warning(0, "Recoll", 
			     tr("error retrieving stemming languages"));
    }
    int cur = prefs.queryStemLang == ""  ? 0 : 1;
    for (vector<string>::const_iterator it = langs.begin(); 
	 it != langs.end(); it++) {
	stemLangCMB->
	    addItem(QString::fromUtf8(it->c_str(), it->length()));
	if (cur == 0 && !strcmp((const char*)prefs.queryStemLang.toUtf8(), 
				it->c_str())) {
	    cur = stemLangCMB->count();
	}
    }
    stemLangCMB->setCurrentIndex(cur);

    autoPhraseCB->setChecked(prefs.ssearchAutoPhrase);
    autoPThreshSB->setValue(prefs.ssearchAutoPhraseThreshPC);

    buildAbsCB->setChecked(prefs.queryBuildAbstract);
    replAbsCB->setEnabled(prefs.queryBuildAbstract);
    replAbsCB->setChecked(prefs.queryReplaceAbstract);

    autoSuffsCB->setChecked(prefs.autoSuffsEnable);
    autoSuffsLE->setText(prefs.autoSuffs);

    // Initialize the extra indexes listboxes
    idxLV->clear();
    for (list<string>::iterator it = prefs.allExtraDbs.begin(); 
	 it != prefs.allExtraDbs.end(); it++) {
	QListWidgetItem *item = 
	    new QListWidgetItem(QString::fromLocal8Bit(it->c_str()), 
				idxLV);
	if (item) 
	    item->setCheckState(Qt::Unchecked);
    }
    for (list<string>::iterator it = prefs.activeExtraDbs.begin(); 
	 it != prefs.activeExtraDbs.end(); it++) {
	QList<QListWidgetItem *>items =
	     idxLV->findItems (QString::fromLocal8Bit(it->c_str()), 
			       Qt::MatchFixedString|Qt::MatchCaseSensitive);
	for (QList<QListWidgetItem *>::iterator it = items.begin(); 
	     it != items.end(); it++) {
	    (*it)->setCheckState(Qt::Checked);
	}
    }
    idxLV->sortItems();
}

void UIPrefsDialog::setupReslistFontPB()
{
    QString s;
    if (reslistFontFamily.length() == 0) {
	reslistFontPB->setText(tr("Default QtWebkit font"));
    } else {
	reslistFontPB->setText(reslistFontFamily + "-" +
			       s.setNum(reslistFontSize));
    }
}

void UIPrefsDialog::accept()
{
    prefs.ssearchOnWS = ssAutoSpaceCB->isChecked();
    prefs.ssearchNoComplete = ssNoCompleteCB->isChecked();
    prefs.ssearchAsYouType = ssAutoAllCB->isChecked();

    if (filterMN_RB->isChecked()) {
	prefs.filterCtlStyle = PrefsPack::FCS_MN;
    } else if (filterCMB_RB->isChecked()) {
	prefs.filterCtlStyle = PrefsPack::FCS_CMB;
    } else {
	prefs.filterCtlStyle = PrefsPack::FCS_BT;
    }
    m_mainWindow->setFilterCtlStyle(prefs.filterCtlStyle);

    prefs.respagesize = pageLenSB->value();
    prefs.collapseDuplicates = collapseDupsCB->isChecked();
    prefs.maxhltextmbs = maxHLTSB->value();

    prefs.qtermcolor = qtermColorLE->text();
    prefs.abssep = abssepLE->text();
    prefs.reslistdateformat = dateformatLE->text();
    prefs.creslistdateformat = (const char*)prefs.reslistdateformat.toUtf8();

    prefs.reslistfontfamily = reslistFontFamily;
    prefs.reslistfontsize = reslistFontSize;
    prefs.qssFile = qssFile;
    QTimer::singleShot(0, m_mainWindow, SLOT(applyStyleSheet()));
    prefs.snipCssFile = snipCssFile;
    prefs.reslistformat =  paraFormat;
    prefs.reslistheadertext =  headerText;
    if (prefs.reslistformat.trimmed().isEmpty()) {
	prefs.reslistformat = prefs.dfltResListFormat;
	paraFormat = prefs.reslistformat;
    }

    prefs.creslistformat = (const char*)prefs.reslistformat.toUtf8();

    if (stemLangCMB->currentIndex() == 0) {
	prefs.queryStemLang = "";
    } else if (stemLangCMB->currentIndex() == 1) {
	prefs.queryStemLang = "ALL";
    } else {
	prefs.queryStemLang = stemLangCMB->currentText();
    }
    prefs.ssearchAutoPhrase = autoPhraseCB->isChecked();
    prefs.ssearchAutoPhraseThreshPC = autoPThreshSB->value();
    prefs.queryBuildAbstract = buildAbsCB->isChecked();
    prefs.queryReplaceAbstract = buildAbsCB->isChecked() && 
	replAbsCB->isChecked();

    prefs.startWithAdvSearchOpen = initStartAdvCB->isChecked();

    prefs.keepSort = keepSortCB->isChecked();
    prefs.showTrayIcon = showTrayIconCB->isChecked();
    prefs.closeToTray = closeToTrayCB->isChecked();
    prefs.previewHtml = previewHtmlCB->isChecked();

    if (plainBRRB->isChecked()) {
	prefs.previewPlainPre = PrefsPack::PP_BR;
    } else if (plainPRERB->isChecked()) {
	prefs.previewPlainPre = PrefsPack::PP_PRE;
    } else {
	prefs.previewPlainPre = PrefsPack::PP_PREWRAP;
    }

    prefs.syntAbsLen = syntlenSB->value();
    prefs.syntAbsCtx = syntctxSB->value();

    
    prefs.autoSuffsEnable = autoSuffsCB->isChecked();
    prefs.autoSuffs = autoSuffsLE->text();

    prefs.allExtraDbs.clear();
    prefs.activeExtraDbs.clear();
    for (int i = 0; i < idxLV->count(); i++) {
	QListWidgetItem *item = idxLV->item(i);
	if (item) {
	    prefs.allExtraDbs.push_back((const char *)item->text().toLocal8Bit());
	    if (item->checkState() == Qt::Checked) {
		prefs.activeExtraDbs.push_back((const char *)
					       item->text().toLocal8Bit());
	    }
	}
    }

    rwSettings(true);
    string reason;
    maybeOpenDb(reason, true);
    emit uiprefsDone();
    QDialog::accept();
}

void UIPrefsDialog::editParaFormat()
{
    EditDialog dialog(this);
    dialog.setWindowTitle(tr("Result list paragraph format "
			     "(erase all to reset to default)"));
    dialog.plainTextEdit->setPlainText(paraFormat);
    int result = dialog.exec();
    if (result == QDialog::Accepted)
	paraFormat = dialog.plainTextEdit->toPlainText();
}
void UIPrefsDialog::editHeaderText()
{
    EditDialog dialog(this);
    dialog.setWindowTitle(tr("Result list header (default is empty)"));
    dialog.plainTextEdit->setPlainText(headerText);
    int result = dialog.exec();
    if (result == QDialog::Accepted)
	headerText = dialog.plainTextEdit->toPlainText();
}

void UIPrefsDialog::reject()
{
    setFromPrefs();
    QDialog::reject();
}

void UIPrefsDialog::setStemLang(const QString& lang)
{
    int cur = 0;
    if (lang == "") {
	cur = 0;
    } else if (lang == "ALL") {
	cur = 1;
    } else {
	for (int i = 1; i < stemLangCMB->count(); i++) {
	    if (lang == stemLangCMB->itemText(i)) {
		cur = i;
		break;
	    }
	}
    }
    stemLangCMB->setCurrentIndex(cur);
}

void UIPrefsDialog::showFontDialog()
{
    bool ok;
    QFont font;
    if (prefs.reslistfontfamily.length()) {
	font.setFamily(prefs.reslistfontfamily);
	font.setPointSize(prefs.reslistfontsize);
    }

    font = QFontDialog::getFont(&ok, font, this);
    if (ok) {
	// We used to check if the default font was set, in which case
	// we erased the preference, but this would result in letting
	// webkit make a choice of default font which it usually seems
	// to do wrong. So now always set the font. There is still a
	// way for the user to let webkit choose the default though:
	// click reset, then the font name and size will be empty.
        reslistFontFamily = font.family();
        reslistFontSize = font.pointSize();
        setupReslistFontPB();
    }
}

void UIPrefsDialog::showStylesheetDialog()
{
    qssFile = myGetFileName(false, "Select stylesheet file", true);
    string nm = path_getsimple((const char *)qssFile.toLocal8Bit());
    stylesheetPB->setText(QString::fromLocal8Bit(nm.c_str()));
}

void UIPrefsDialog::resetStylesheet()
{
    qssFile = "";
    stylesheetPB->setText(tr("Choose"));
}
void UIPrefsDialog::showSnipCssDialog()
{
    snipCssFile = myGetFileName(false, "Select snippets window CSS file", true);
    string nm = path_getsimple((const char *)snipCssFile.toLocal8Bit());
    snipCssPB->setText(QString::fromLocal8Bit(nm.c_str()));
}
void UIPrefsDialog::resetSnipCss()
{
    snipCssFile = "";
    snipCssPB->setText(tr("Choose"));
}

void UIPrefsDialog::resetReslistFont()
{
    reslistFontFamily = "";
    reslistFontSize = 0;
    setupReslistFontPB();
}

void UIPrefsDialog::showViewAction()
{
    if (m_viewAction== 0) {
	m_viewAction = new ViewAction(0);
    } else {
	// Close and reopen, in hope that makes us visible...
	m_viewAction->close();
    }
    m_viewAction->show();
}
void UIPrefsDialog::showViewAction(const QString& mt)
{
    showViewAction();
    m_viewAction->selectMT(mt);
}

////////////////////////////////////////////
// External / extra search indexes setup

void UIPrefsDialog::extradDbSelectChanged()
{
    if (idxLV->selectedItems().size() <= 1) 
	ptransPB->setEnabled(true);
    else
	ptransPB->setEnabled(false);
}

void UIPrefsDialog::extraDbEditPtrans()
{
    string dbdir;
    if (idxLV->selectedItems().size() == 0) {
	dbdir = theconfig->getDbDir();
    } else if (idxLV->selectedItems().size() == 1) {
	QListWidgetItem *item = idxLV->selectedItems()[0];
	QString qd = item->data(Qt::DisplayRole).toString();
	dbdir = (const char *)qd.toLocal8Bit();
    } else {
	QMessageBox::warning(
	    0, "Recoll", tr("At most one index should be selected"));
	return;
    }
    dbdir = path_canon(dbdir);
    EditTrans *etrans = new EditTrans(dbdir, this);
    etrans->show();
}

void UIPrefsDialog::togExtraDbPB_clicked()
{
    for (int i = 0; i < idxLV->count(); i++) {
	QListWidgetItem *item = idxLV->item(i);
	if (item->isSelected()) {
	    if (item->checkState() == Qt::Checked) {
		item->setCheckState(Qt::Unchecked);
	    } else {
		item->setCheckState(Qt::Checked);
	    }
	}
    }
}
void UIPrefsDialog::actAllExtraDbPB_clicked()
{
    for (int i = 0; i < idxLV->count(); i++) {
	QListWidgetItem *item = idxLV->item(i);
	    item->setCheckState(Qt::Checked);
    }
}
void UIPrefsDialog::unacAllExtraDbPB_clicked()
{
    for (int i = 0; i < idxLV->count(); i++) {
	QListWidgetItem *item = idxLV->item(i);
	    item->setCheckState(Qt::Unchecked);
    }
}

void UIPrefsDialog::delExtraDbPB_clicked()
{
    QList<QListWidgetItem *> items = idxLV->selectedItems();
    for (QList<QListWidgetItem *>::iterator it = items.begin(); 
	 it != items.end(); it++) {
	delete *it;
    }
}

static bool samedir(const string& dir1, const string& dir2)
{
    struct stat st1, st2;
    if (stat(dir1.c_str(), &st1))
	return false;
    if (stat(dir2.c_str(), &st2))
	return false;
    if (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino) {
	return true;
    }
    return false;
}

void UIPrefsDialog::on_showTrayIconCB_clicked()
{
    closeToTrayCB->setEnabled(showTrayIconCB->checkState());
}

/** 
 * Browse to add another index.
 * We do a textual comparison to check for duplicates, except for
 * the main db for which we check inode numbers. 
 */
void UIPrefsDialog::addExtraDbPB_clicked()
{
    QString input = myGetFileName(true, 
				  tr("Select recoll config directory or "
				     "xapian index directory "
				     "(e.g.: /home/me/.recoll or "
				     "/home/me/.recoll/xapiandb)"));

    if (input.isEmpty())
	return;
    string dbdir = (const char *)input.toLocal8Bit();
    if (access(path_cat(dbdir, "recoll.conf").c_str(), 0) == 0) {
	// Chosen dir is config dir.
	RclConfig conf(&dbdir);
	dbdir = conf.getDbDir();
	if (dbdir.empty()) {
	    QMessageBox::warning(
		0, "Recoll", tr("The selected directory looks like a Recoll "
				"configuration directory but the configuration "
				"could not be read"));
	    return;
	}
    }

    LOGDEB(("ExtraDbDial: got: [%s]\n", dbdir.c_str()));
    path_catslash(dbdir);
    bool stripped;
    if (!Rcl::Db::testDbDir(dbdir, &stripped)) {
	QMessageBox::warning(0, "Recoll", 
        tr("The selected directory does not appear to be a Xapian index"));
	return;
    }
    if (o_index_stripchars != stripped) {
	QMessageBox::warning(0, "Recoll", 
			     tr("Cant add index with different case/diacritics"
				" stripping option"));
	return;
    }
    if (samedir(dbdir, theconfig->getDbDir())) {
	QMessageBox::warning(0, "Recoll", 
			     tr("This is the main/local index!"));
	return;
    }

    for (int i = 0; i < idxLV->count(); i++) {
	QListWidgetItem *item = idxLV->item(i);
	string existingdir = (const char *)item->text().toLocal8Bit();
	if (samedir(dbdir, existingdir)) {
	    QMessageBox::warning(
		0, "Recoll", tr("The selected directory is already in the "
				"index list"));
	    return;
	}
    }

    QListWidgetItem *item = 
	new QListWidgetItem(QString::fromLocal8Bit(dbdir.c_str()), idxLV);
    item->setCheckState(Qt::Checked);
    idxLV->sortItems();
}
