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
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

#include <list>
#include <utility>
#ifndef NO_NAMESPACES
using std::pair;
#endif /* NO_NAMESPACES */

#include <qmessagebox.h>
#include <qthread.h>
#include <qvariant.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#include <qprinter.h>
#include <qprintdialog.h>
#include <qscrollbar.h>
#include <qmenu.h>
#include <qtextedit.h>
#include <qtextbrowser.h>
#include <qprogressdialog.h>
#include <qevent.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qimage.h>
#include <qurl.h>

#include "debuglog.h"
#include "pathut.h"
#include "internfile.h"
#include "recoll.h"
#include "plaintorich.h"
#include "smallut.h"
#include "wipedir.h"
#include "cancelcheck.h"
#include "preview_w.h"
#include "guiutils.h"
#include "docseqhist.h"
#include "rclhelp.h"

// Subclass plainToRich to add <termtag>s and anchors to the preview text
class PlainToRichQtPreview : public PlainToRich {
public:

    PlainToRichQtPreview()
	: m_curanchor(1), m_lastanchor(0)
    {
    }    

    bool haveAnchors()
    {
	return m_lastanchor != 0;
    }

    virtual string header() 
    {
	if (!m_inputhtml) {
	    switch (prefs.previewPlainPre) {
	    case PrefsPack::PP_BR:
		m_eolbr = true;
		return "<qt><head><title></title></head><body>";
	    case PrefsPack::PP_PRE:
		m_eolbr = false;
		return "<qt><head><title></title></head><body><pre>";
	    case PrefsPack::PP_PREWRAP:
		m_eolbr = false;
		return "<qt><head><title></title></head><body>"
		    "<pre style=\"white-space: pre-wrap\">";
	    }
	}
	return cstr_null;
    }

    virtual string startMatch(unsigned int grpidx)
    {
	LOGDEB2(("startMatch, grpidx %u\n", grpidx));
	grpidx = m_hdata->grpsugidx[grpidx];
	LOGDEB2(("startMatch, ugrpidx %u\n", grpidx));
	m_groupanchors[grpidx].push_back(++m_lastanchor);
	m_groupcuranchors[grpidx] = 0; 
	return string("<span style='color: ").
	    append((const char *)(prefs.qtermcolor.toUtf8())).
	    append(";font-weight: bold;").
	    append("'>").
	    append("<a name=\"").
	    append(termAnchorName(m_lastanchor)).
	    append("\">");
    }

    virtual string endMatch() 
    {
	return string("</a></span>");
    }

    virtual string termAnchorName(int i) const
    {
	static const char *termAnchorNameBase = "TRM";
	char acname[sizeof(termAnchorNameBase) + 20];
	sprintf(acname, "%s%d", termAnchorNameBase, i);
	return string(acname);
    }

    virtual string startChunk() 
    { 
	return "<pre>";
    }

    int nextAnchorNum(int grpidx)
    {
	LOGDEB2(("nextAnchorNum: group %d\n", grpidx));
	map<unsigned int, unsigned int>::iterator curit = 
	    m_groupcuranchors.find(grpidx);
	map<unsigned int, vector<int> >::iterator vecit = 
	    m_groupanchors.find(grpidx);
	if (grpidx == -1 || curit == m_groupcuranchors.end() ||
	    vecit == m_groupanchors.end()) {
	    if (m_curanchor >= m_lastanchor)
		m_curanchor = 1;
	    else
		m_curanchor++;
	} else {
	    if (curit->second >= vecit->second.size() -1)
		m_groupcuranchors[grpidx] = 0;
	    else 
		m_groupcuranchors[grpidx]++;
	    m_curanchor = vecit->second[m_groupcuranchors[grpidx]];
	    LOGDEB2(("nextAnchorNum: curanchor now %d\n", m_curanchor));
	}
	return m_curanchor;
    }

    int prevAnchorNum(int grpidx)
    {
	map<unsigned int, unsigned int>::iterator curit = 
	    m_groupcuranchors.find(grpidx);
	map<unsigned int, vector<int> >::iterator vecit = 
	    m_groupanchors.find(grpidx);
	if (grpidx == -1 || curit == m_groupcuranchors.end() ||
	    vecit == m_groupanchors.end()) {
	    if (m_curanchor <= 1)
		m_curanchor = m_lastanchor;
	    else
		m_curanchor--;
	} else {
	    if (curit->second <= 0)
		m_groupcuranchors[grpidx] = vecit->second.size() -1;
	    else 
		m_groupcuranchors[grpidx]--;
	    m_curanchor = vecit->second[m_groupcuranchors[grpidx]];
	}
	return m_curanchor;
    }

    QString curAnchorName() const
    {
	return QString::fromUtf8(termAnchorName(m_curanchor).c_str());
    }

private:
    int m_curanchor;
    int m_lastanchor;
    // Lists of anchor numbers (match locations) for the term (groups)
    // in the query (the map key is and index into HighlightData.groups).
    map<unsigned int, vector<int> > m_groupanchors;
    map<unsigned int, unsigned int> m_groupcuranchors;
};

void Preview::init()
{
    setObjectName("Preview");
    QVBoxLayout* previewLayout = new QVBoxLayout(this);

    pvTab = new QTabWidget(this);

    // Create the first tab. Should be possible to use addEditorTab
    // but this causes a pb with the sizeing
    QWidget *unnamed = new QWidget(pvTab);
    QVBoxLayout *unnamedLayout = new QVBoxLayout(unnamed);
    PreviewTextEdit *pvEdit = new PreviewTextEdit(unnamed, "pvEdit", this);
    pvEdit->setReadOnly(true);
    pvEdit->setUndoRedoEnabled(false);
    unnamedLayout->addWidget(pvEdit);
    pvTab->addTab(unnamed, "");

    previewLayout->addWidget(pvTab);

    // Create the buttons and entry field
    QHBoxLayout *layout3 = new QHBoxLayout(0); 
    searchLabel = new QLabel(this);
    layout3->addWidget(searchLabel);

    searchTextCMB = new QComboBox(this);
    searchTextCMB->setEditable(true);
    searchTextCMB->setInsertPolicy(QComboBox::NoInsert);
    searchTextCMB->setDuplicatesEnabled(false);
    for (unsigned int i = 0; i < m_hData.ugroups.size(); i++) {
	QString s;
	for (unsigned int j = 0; j < m_hData.ugroups[i].size(); j++) {
	    s.append(QString::fromUtf8(m_hData.ugroups[i][j].c_str()));
	    if (j != m_hData.ugroups[i].size()-1)
		s.append(" ");
	}
	searchTextCMB->addItem(s);
    }
    searchTextCMB->setEditText("");
    searchTextCMB->setCompleter(0);

    layout3->addWidget(searchTextCMB);

    nextButton = new QPushButton(this);
    nextButton->setEnabled(true);
    layout3->addWidget(nextButton);
    prevButton = new QPushButton(this);
    prevButton->setEnabled(true);
    layout3->addWidget(prevButton);
    clearPB = new QPushButton(this);
    clearPB->setEnabled(false);
    layout3->addWidget(clearPB);
    matchCheck = new QCheckBox(this);
    layout3->addWidget(matchCheck);

    previewLayout->addLayout(layout3);

    resize(QSize(640, 480).expandedTo(minimumSizeHint()));

    // buddies
    searchLabel->setBuddy(searchTextCMB);

    searchLabel->setText(tr("&Search for:"));
    nextButton->setText(tr("&Next"));
    prevButton->setText(tr("&Previous"));
    clearPB->setText(tr("Clear"));
    matchCheck->setText(tr("Match &Case"));

    QPushButton * bt = new QPushButton(tr("Close Tab"), this);
    pvTab->setCornerWidget(bt);

    (void)new HelpClient(this);
    HelpClient::installMap((const char *)objectName().toUtf8(), 
			   "RCL.SEARCH.PREVIEW");

    // signals and slots connections
    connect(searchTextCMB, SIGNAL(activated(int)), 
	    this, SLOT(searchTextFromIndex(int)));
    connect(searchTextCMB, SIGNAL(editTextChanged(const QString&)), 
	    this, SLOT(searchTextChanged(const QString&)));
    connect(nextButton, SIGNAL(clicked()), this, SLOT(nextPressed()));
    connect(prevButton, SIGNAL(clicked()), this, SLOT(prevPressed()));
    connect(clearPB, SIGNAL(clicked()), searchTextCMB, SLOT(clearEditText()));
    connect(pvTab, SIGNAL(currentChanged(int)), 
	    this, SLOT(currentChanged(int)));
    connect(bt, SIGNAL(clicked()), this, SLOT(closeCurrentTab()));

    m_dynSearchActive = false;
    m_canBeep = true;
    if (prefs.pvwidth > 100) {
	resize(prefs.pvwidth, prefs.pvheight);
    }
    m_loading = false;
    currentChanged(pvTab->currentIndex());
    m_justCreated = true;
}

void Preview::closeEvent(QCloseEvent *e)
{
    LOGDEB(("Preview::closeEvent. m_loading %d\n", m_loading));
    if (m_loading) {
	CancelCheck::instance().setCancel();
	e->ignore();
	return;
    }
    prefs.pvwidth = width();
    prefs.pvheight = height();

    /* Release all temporary files (but maybe none is actually set) */
    for (int i = 0; i < pvTab->count(); i++) {
        QWidget *tw = pvTab->widget(i);
        if (tw) {
	    PreviewTextEdit *edit = 
		tw->findChild<PreviewTextEdit*>("pvEdit");
            if (edit) {
		forgetTempFile(edit->m_tmpfilename);
            }
        }
    }
    emit previewExposed(this, m_searchId, -1);
    emit previewClosed(this);
    QWidget::closeEvent(e);
}

extern const char *eventTypeToStr(int tp);

bool Preview::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() != QEvent::KeyPress) {
#if 0
    LOGDEB(("Preview::eventFilter(): %s\n", eventTypeToStr(event->type())));
	if (event->type() == QEvent::MouseButtonRelease) {
	    QMouseEvent *mev = (QMouseEvent *)event;
	    LOGDEB(("Mouse: GlobalY %d y %d\n", mev->globalY(),
		    mev->y()));
	}
#endif
	return false;
    }
    
    LOGDEB2(("Preview::eventFilter: keyEvent\n"));

    PreviewTextEdit *edit = currentEditor();
    QKeyEvent *keyEvent = (QKeyEvent *)event;
    if (keyEvent->key() == Qt::Key_Escape) {
	close();
	return true;
    } else if (keyEvent->key() == Qt::Key_Down &&
	       (keyEvent->modifiers() & Qt::ShiftModifier)) {
	LOGDEB2(("Preview::eventFilter: got Shift-Up\n"));
	if (edit) 
	    emit(showNext(this, m_searchId, edit->m_docnum));
	return true;
    } else if (keyEvent->key() == Qt::Key_Up &&
	       (keyEvent->modifiers() & Qt::ShiftModifier)) {
	LOGDEB2(("Preview::eventFilter: got Shift-Down\n"));
	if (edit) 
	    emit(showPrev(this, m_searchId, edit->m_docnum));
	return true;
    } else if (keyEvent->key() == Qt::Key_W &&
	       (keyEvent->modifiers() & Qt::ControlModifier)) {
	LOGDEB2(("Preview::eventFilter: got ^W\n"));
	closeCurrentTab();
	return true;
    } else if (keyEvent->key() == Qt::Key_P &&
	       (keyEvent->modifiers() & Qt::ControlModifier)) {
	LOGDEB2(("Preview::eventFilter: got ^P\n"));
	emit(printCurrentPreviewRequest());
	return true;
    } else if (m_dynSearchActive) {
	if (keyEvent->key() == Qt::Key_F3) {
	    LOGDEB2(("Preview::eventFilter: got F3\n"));
	    doSearch(searchTextCMB->currentText(), true, false);
	    return true;
	}
	if (target != searchTextCMB)
	    return QApplication::sendEvent(searchTextCMB, event);
    } else {
	if (edit && 
	    (target == edit || target == edit->viewport())) {
	    if (keyEvent->key() == Qt::Key_Slash ||
		(keyEvent->key() == Qt::Key_F &&
		 (keyEvent->modifiers() & Qt::ControlModifier))) {
		LOGDEB2(("Preview::eventFilter: got / or C-F\n"));
		searchTextCMB->setFocus();
		m_dynSearchActive = true;
		return true;
	    } else if (keyEvent->key() == Qt::Key_Space) {
		LOGDEB2(("Preview::eventFilter: got Space\n"));
		int value = edit->verticalScrollBar()->value();
		value += edit->verticalScrollBar()->pageStep();
		edit->verticalScrollBar()->setValue(value);
		return true;
	    } else if (keyEvent->key() == Qt::Key_Backspace) {
		LOGDEB2(("Preview::eventFilter: got Backspace\n"));
		int value = edit->verticalScrollBar()->value();
		value -= edit->verticalScrollBar()->pageStep();
		edit->verticalScrollBar()->setValue(value);
		return true;
	    }
	}
    }

    return false;
}

void Preview::searchTextChanged(const QString & text)
{
    LOGDEB1(("Search line text changed. text: '%s'\n", 
	     (const char *)text.toUtf8()));
    m_searchTextFromIndex = -1;
    if (text.isEmpty()) {
	m_dynSearchActive = false;
	clearPB->setEnabled(false);
    } else {
	m_dynSearchActive = true;
	clearPB->setEnabled(true);
	doSearch(text, false, false);
    }
}

void Preview::searchTextFromIndex(int idx)
{
    LOGDEB1(("search line from index %d\n", idx));
    m_searchTextFromIndex = idx;
}

PreviewTextEdit *Preview::currentEditor()
{
    LOGDEB2(("Preview::currentEditor()\n"));
    QWidget *tw = pvTab->currentWidget();
    PreviewTextEdit *edit = 0;
    if (tw) {
	edit = tw->findChild<PreviewTextEdit*>("pvEdit");
    }
    return edit;
}

// Save current document to file
void Preview::emitSaveDocToFile()
{
    PreviewTextEdit *ce = currentEditor();
    if (ce && !ce->m_dbdoc.url.empty()) {
	emit saveDocToFile(ce->m_dbdoc);
    }
}

// Perform text search. If next is true, we look for the next match of the
// current search, trying to advance and possibly wrapping around. If next is
// false, the search string has been modified, we search for the new string, 
// starting from the current position
void Preview::doSearch(const QString &_text, bool next, bool reverse, 
		       bool wordOnly)
{
    LOGDEB(("Preview::doSearch: text [%s] idx %d next %d rev %d word %d\n", 
	    (const char *)_text.toUtf8(), m_searchTextFromIndex, int(next), 
	    int(reverse), int(wordOnly)));
    QString text = _text;

    bool matchCase = matchCheck->isChecked();
    PreviewTextEdit *edit = currentEditor();
    if (edit == 0) {
	// ??
	return;
    }

    if (text.isEmpty() || m_searchTextFromIndex != -1) {
	if (!edit->m_plaintorich->haveAnchors()) {
	    LOGDEB(("NO ANCHORS\n"));
	    return;
	}
	// The combobox indices are equal to the search ugroup indices
	// in hldata, that's how we built the list.
	if (reverse) {
	    edit->m_plaintorich->prevAnchorNum(m_searchTextFromIndex);
	} else {
	    edit->m_plaintorich->nextAnchorNum(m_searchTextFromIndex);
	}
	QString aname = edit->m_plaintorich->curAnchorName();
	LOGDEB(("Calling scrollToAnchor(%s)\n", (const char *)aname.toUtf8()));
	edit->scrollToAnchor(aname);
	// Position the cursor approximately at the anchor (top of
	// viewport) so that searches start from here
	QTextCursor cursor = edit->cursorForPosition(QPoint(0, 0));
	edit->setTextCursor(cursor);
	return;
    }

    // If next is false, the user added characters to the current
    // search string.  We need to reset the cursor position to the
    // start of the previous match, else incremental search is going
    // to look for the next occurrence instead of trying to lenghten
    // the current match
    if (!next) {
	QTextCursor cursor = edit->textCursor();
	cursor.setPosition(cursor.anchor(), QTextCursor::KeepAnchor);
	edit->setTextCursor(cursor);
    }
    Chrono chron;
    LOGDEB(("Preview::doSearch: first find call\n"));
    QTextDocument::FindFlags flags = 0;
    if (reverse)
	flags |= QTextDocument::FindBackward;
    if (wordOnly)
	flags |= QTextDocument::FindWholeWords;
    if (matchCase)
	flags |= QTextDocument::FindCaseSensitively;
    bool found = edit->find(text, flags);
    LOGDEB(("Preview::doSearch: first find call return: found %d %.2f S\n", 
            found, chron.secs()));
    // If not found, try to wrap around. 
    if (!found) { 
	LOGDEB(("Preview::doSearch: wrapping around\n"));
	if (reverse) {
	    edit->moveCursor (QTextCursor::End);
	} else {
	    edit->moveCursor (QTextCursor::Start);
	}
	LOGDEB(("Preview::doSearch: 2nd find call\n"));
        chron.restart();
	found = edit->find(text, flags);
	LOGDEB(("Preview::doSearch: 2nd find call return found %d %.2f S\n",
                found, chron.secs()));
    }

    if (found) {
	m_canBeep = true;
    } else {
	if (m_canBeep)
	    QApplication::beep();
	m_canBeep = false;
    }
    LOGDEB(("Preview::doSearch: return\n"));
}

void Preview::nextPressed()
{
    LOGDEB2(("Preview::nextPressed\n"));
    doSearch(searchTextCMB->currentText(), true, false);
}

void Preview::prevPressed()
{
    LOGDEB2(("Preview::prevPressed\n"));
    doSearch(searchTextCMB->currentText(), true, true);
}

// Called when user clicks on tab
void Preview::currentChanged(int index)
{
    LOGDEB2(("PreviewTextEdit::currentChanged\n"));
    QWidget *tw = pvTab->widget(index);
    PreviewTextEdit *edit = 
	tw->findChild<PreviewTextEdit*>("pvEdit");
    LOGDEB1(("Preview::currentChanged(). Editor: %p\n", edit));
    
    if (edit == 0) {
	LOGERR(("Editor child not found\n"));
	return;
    }
    edit->setFocus();
    // Disconnect the print signal and reconnect it to the current editor
    LOGDEB(("Disconnecting reconnecting print signal\n"));
    disconnect(this, SIGNAL(printCurrentPreviewRequest()), 0, 0);
    connect(this, SIGNAL(printCurrentPreviewRequest()), edit, SLOT(print()));
    edit->installEventFilter(this);
    edit->viewport()->installEventFilter(this);
    searchTextCMB->installEventFilter(this);
    emit(previewExposed(this, m_searchId, edit->m_docnum));
}

void Preview::closeCurrentTab()
{
    LOGDEB1(("Preview::closeCurrentTab: m_loading %d\n", m_loading));
    if (m_loading) {
	CancelCheck::instance().setCancel();
	return;
    }
    PreviewTextEdit *e = currentEditor();
    if (e)
	forgetTempFile(e->m_tmpfilename);
    if (pvTab->count() > 1) {
	pvTab->removeTab(pvTab->currentIndex());
    } else {
	close();
    }
}

PreviewTextEdit *Preview::addEditorTab()
{
    LOGDEB1(("PreviewTextEdit::addEditorTab()\n"));
    QWidget *anon = new QWidget((QWidget *)pvTab);
    QVBoxLayout *anonLayout = new QVBoxLayout(anon); 
    PreviewTextEdit *editor = new PreviewTextEdit(anon, "pvEdit", this);
    editor->setReadOnly(true);
    editor->setUndoRedoEnabled(false );
    anonLayout->addWidget(editor);
    pvTab->addTab(anon, "Tab");
    pvTab->setCurrentIndex(pvTab->count() -1);
    return editor;
}

void Preview::setCurTabProps(const Rcl::Doc &doc, int docnum)
{
    LOGDEB1(("Preview::setCurTabProps\n"));
    QString title;
    string ctitle;
    if (doc.getmeta(Rcl::Doc::keytt, &ctitle) && !ctitle.empty()) {
	title = QString::fromUtf8(ctitle.c_str(), ctitle.length());
    } else {
        title = QString::fromLocal8Bit(path_getsimple(doc.url).c_str());
    }
    if (title.length() > 20) {
	title = title.left(10) + "..." + title.right(10);
    }
    int curidx = pvTab->currentIndex();
    pvTab->setTabText(curidx, title);

    char datebuf[100];
    datebuf[0] = 0;
    if (!doc.fmtime.empty() || !doc.dmtime.empty()) {
	time_t mtime = doc.dmtime.empty() ? 
	    atoll(doc.fmtime.c_str()) : atoll(doc.dmtime.c_str());
	struct tm *tm = localtime(&mtime);
	strftime(datebuf, 99, "%Y-%m-%d %H:%M:%S", tm);
    }
    LOGDEB(("Doc.url: [%s]\n", doc.url.c_str()));
    string url;
    printableUrl(theconfig->getDefCharset(), doc.url, url);
    string tiptxt = url + string("\n");
    tiptxt += doc.mimetype + " " + string(datebuf) + "\n";
    if (!ctitle.empty())
	tiptxt += ctitle + "\n";
    pvTab->setTabToolTip(curidx,
			 QString::fromUtf8(tiptxt.c_str(), tiptxt.length()));

    PreviewTextEdit *e = currentEditor();
    if (e) {
	e->m_url = doc.url;
	e->m_ipath = doc.ipath;
	e->m_docnum = docnum;
    }
}

bool Preview::makeDocCurrent(const Rcl::Doc& doc, int docnum, bool sametab)
{
    LOGDEB(("Preview::makeDocCurrent: %s\n", doc.url.c_str()));

    if (m_loading) {
	LOGERR(("Already loading\n"));
	return false;
    }

    /* Check if we already have this page */
    for (int i = 0; i < pvTab->count(); i++) {
        QWidget *tw = pvTab->widget(i);
        if (tw) {
	    PreviewTextEdit *edit = 
		tw->findChild<PreviewTextEdit*>("pvEdit");
            if (edit && !edit->m_url.compare(doc.url) && 
                !edit->m_ipath.compare(doc.ipath)) {
                pvTab->setCurrentIndex(i);
                return true;
            }
        }
    }

    // if just created the first tab was created during init
    if (!sametab && !m_justCreated && !addEditorTab()) {
	return false;
    }
    m_justCreated = false;
    if (!loadDocInCurrentTab(doc, docnum)) {
	closeCurrentTab();
	return false;
    }
    raise();
    return true;
}
void Preview::togglePlainPre()
{
    switch (prefs.previewPlainPre) {
    case PrefsPack::PP_BR:
	prefs.previewPlainPre = PrefsPack::PP_PRE;
	break;
    case PrefsPack::PP_PRE:
	prefs.previewPlainPre = PrefsPack::PP_BR;
	break;
    case PrefsPack::PP_PREWRAP:
    default:
	prefs.previewPlainPre = PrefsPack::PP_PRE;
	break;
    }
    
    PreviewTextEdit *editor = currentEditor();
    if (editor)
	loadDocInCurrentTab(editor->m_dbdoc, editor->m_docnum);
}

void Preview::emitWordSelect(QString word)
{
    emit(wordSelect(word));
}

/*
  Code for loading a file into an editor window. The operations that
  we call have no provision to indicate progression, and it would be
  complicated or impossible to modify them to do so (Ie: for external 
  format converters).

  We implement a complicated and ugly mechanism based on threads to indicate 
  to the user that the app is doing things: lengthy operations are done in 
  threads and we update a progress indicator while they proceed (but we have 
  no estimate of their total duration).
  
  It might be possible, but complicated (need modifications in
  handler) to implement a kind of bucket brigade, to have the
  beginning of the text displayed faster
*/

/* A thread to to the file reading / format conversion */
class LoadThread : public QThread {
    int *statusp;
    Rcl::Doc& out;
    const Rcl::Doc& idoc;
    int loglevel;
 public: 
    string missing;
    TempFile imgtmp;

    LoadThread(int *stp, Rcl::Doc& odoc, const Rcl::Doc& idc) 
	: statusp(stp), out(odoc), idoc(idc)
	{
	    loglevel = DebugLog::getdbl()->getlevel();
	}
    ~LoadThread() {
    }
    virtual void run() {
	DebugLog::getdbl()->setloglevel(loglevel);

	FileInterner interner(idoc, theconfig, FileInterner::FIF_forPreview);
	FIMissingStore mst;
	interner.setMissingStore(&mst);
	// Even when previewHtml is set, we don't set the interner's
	// target mtype to html because we do want the html filter to
	// do its work: we won't use the text/plain, but we want the
	// text/html to be converted to utf-8 (for highlight processing)
	try {
            string ipath = idoc.ipath;
	    FileInterner::Status ret = interner.internfile(out, ipath);
	    if (ret == FileInterner::FIDone || ret == FileInterner::FIAgain) {
		// FIAgain is actually not nice here. It means that the record
		// for the *file* of a multidoc was selected. Actually this
		// shouldn't have had a preview link at all, but we don't know
		// how to handle it now. Better to show the first doc than
		// a mysterious error. Happens when the file name matches a
		// a search term.
		*statusp = 0;
		// If we prefer html and it is available, replace the
		// text/plain document text
		if (prefs.previewHtml && !interner.get_html().empty()) {
		    out.text = interner.get_html();
		    out.mimetype = "text/html";
		}
		imgtmp = interner.get_imgtmp();
	    } else {
		out.mimetype = interner.getMimetype();
		mst.getMissingExternal(missing);
		*statusp = -1;
	    }
	} catch (CancelExcept) {
	    *statusp = -1;
	}
    }
};


// Insert into editor by chunks so that the top becomes visible
// earlier for big texts. This provokes some artifacts (adds empty line),
// so we can't set it too low.
#define CHUNKL 500*1000

/* A thread to convert to rich text (mark search terms) */
class ToRichThread : public QThread {
    string &in;
    const HighlightData &hdata;
    list<string> &out;
    int loglevel;
    PlainToRichQtPreview *ptr;
 public:
    ToRichThread(string &i, const HighlightData& hd, list<string> &o, 
		 PlainToRichQtPreview *_ptr)
	: in(i), hdata(hd), out(o), ptr(_ptr)
    {
	    loglevel = DebugLog::getdbl()->getlevel();
    }
    virtual void run()
    {
	DebugLog::getdbl()->setloglevel(loglevel);
	try {
	    ptr->plaintorich(in, out, hdata, CHUNKL);
	} catch (CancelExcept) {
	}
    }
};

class LoadGuard {
    bool *m_bp;
public:
    LoadGuard(bool *bp) {m_bp = bp ; *m_bp = true;}
    ~LoadGuard() {*m_bp = false; CancelCheck::instance().setCancel(false);}
};

bool Preview::loadDocInCurrentTab(const Rcl::Doc &idoc, int docnum)
{
    LOGDEB1(("Preview::loadDocInCurrentTab()\n"));

    LoadGuard guard(&m_loading);
    CancelCheck::instance().setCancel(false);

    setCurTabProps(idoc, docnum);

    QString msg = QString("Loading: %1 (size %2 bytes)")
	.arg(QString::fromLocal8Bit(idoc.url.c_str()))
	.arg(QString::fromUtf8(idoc.fbytes.c_str()));

    // Create progress dialog and aux objects
    const int nsteps = 20;
    QProgressDialog progress(msg, tr("Cancel"), 0, nsteps, this);
    progress.setMinimumDuration(2000);

    ////////////////////////////////////////////////////////////////////////
    // Load and convert document
    // idoc came out of the index data (main text and some fields missing). 
    // fdoc is the complete one what we are going to extract from storage.
    Rcl::Doc fdoc;
    int status = 1;
    LoadThread lthr(&status, fdoc, idoc);
    lthr.start();
    int prog;
    for (prog = 1;;prog++) {
	if (lthr.wait(100))
	    break;
	progress.setValue(prog);
	qApp->processEvents();
	if (progress.wasCanceled()) {
	    CancelCheck::instance().setCancel();
	}
	if (prog >= 5)
	    sleep(1);
    }

    LOGDEB(("LoadFileInCurrentTab: after file load: cancel %d status %d"
	    " text length %d\n", 
	    CancelCheck::instance().cancelState(), status, fdoc.text.length()));

    if (CancelCheck::instance().cancelState())
	return false;
    if (status != 0) {
        QString explain;
	if (!lthr.missing.empty()) {
            explain = QString::fromUtf8("<br>") +
                tr("Missing helper program: ") +
                QString::fromLocal8Bit(lthr.missing.c_str());
	    QMessageBox::warning(0, "Recoll",
				 tr("Can't turn doc into internal "
				    "representation for ") +
				 fdoc.mimetype.c_str() + explain);
        } else {
	    QMessageBox::warning(0, "Recoll", 
				  tr("Error while loading file"));
	}

	return false;
    }
    // Reset config just in case.
    theconfig->setKeyDir("");

    ////////////////////////////////////////////////////////////////////////
    // Create preview text: highlight search terms
    // We don't do the highlighting for very big texts: too long. We
    // should at least do special char escaping, in case a '&' or '<'
    // somehow slipped through previous processing.
    bool highlightTerms = fdoc.text.length() < 
	(unsigned long)prefs.maxhltextmbs * 1024 * 1024;

    // Final text is produced in chunks so that we can display the top
    // while still inserting at bottom
    list<QString> qrichlst;
    PreviewTextEdit *editor = currentEditor();

    // For an actual html file, if we want to have the images and
    // style loaded in the preview, we need to set the search
    // path. Not too sure this is a good idea as I find them rather
    // distracting when looking for text, esp. with qtextedit
    // relatively limited html support (text sometimes get hidden by
    // images).
#if 0
    string path = fileurltolocalpath(idoc.url);
    if (!path.empty()) {
	path = path_getfather(path);
	QStringList paths(QString::fromLocal8Bit(path.c_str()));
	editor->setSearchPaths(paths);
    }
#endif

    editor->setHtml("");
    editor->m_format = Qt::RichText;
    bool inputishtml = !fdoc.mimetype.compare("text/html");

#if 0
    // For testing qtextedit bugs...
    highlightTerms = true;
    const char *textlist[] =
    {
        "Du plain text avec un\n <termtag>termtag</termtag> fin de ligne:",
        "texte apres le tag\n",
    };
    const int listl = sizeof(textlist) / sizeof(char*);
    for (int i = 0 ; i < listl ; i++)
        qrichlst.push_back(QString::fromUtf8(textlist[i]));
#else
    if (highlightTerms) {
	progress.setLabelText(tr("Creating preview text"));
	qApp->processEvents();

	if (inputishtml) {
	    LOGDEB1(("Preview: got html %s\n", fdoc.text.c_str()));
	    editor->m_plaintorich->set_inputhtml(true);
	} else {
	    LOGDEB1(("Preview: got plain %s\n", fdoc.text.c_str()));
	    editor->m_plaintorich->set_inputhtml(false);
	}
	list<string> richlst;
	ToRichThread rthr(fdoc.text, m_hData, richlst, editor->m_plaintorich);
	rthr.start();

	for (;;prog++) {
	    if (rthr.wait(100))
		break;
	    progress.setValue(nsteps);
	    qApp->processEvents();
	    if (progress.wasCanceled()) {
		CancelCheck::instance().setCancel();
	    }
	    if (prog >= 5)
		sleep(1);
	}

	// Conversion to rich text done
	if (CancelCheck::instance().cancelState()) {
	    if (richlst.size() == 0 || richlst.front().length() == 0) {
		// We can't call closeCurrentTab here as it might delete
		// the object which would be a nasty surprise to our
		// caller.
		return false;
	    } else {
		richlst.back() += "<b>Cancelled !</b>";
	    }
	}
	// Convert C++ string list to QString list
	for (list<string>::iterator it = richlst.begin(); 
	     it != richlst.end(); it++) {
	    qrichlst.push_back(QString::fromUtf8(it->c_str(), it->length()));
	}
    } else {
	LOGDEB(("Preview: no hilighting\n"));
	// No plaintorich() call.  In this case, either the text is
	// html and the html quoting is hopefully correct, or it's
	// plain-text and there is no need to escape special
	// characters. We'd still want to split in chunks (so that the
	// top is displayed faster), but we must not cut tags, and
	// it's too difficult on html. For text we do the splitting on
	// a QString to avoid utf8 issues.
	QString qr = QString::fromUtf8(fdoc.text.c_str(), fdoc.text.length());
	int l = 0;
	if (inputishtml) {
	    qrichlst.push_back(qr);
	} else {
            editor->setPlainText("");
            editor->m_format = Qt::PlainText;
	    for (int pos = 0; pos < (int)qr.length(); pos += l) {
		l = MIN(CHUNKL, qr.length() - pos);
		qrichlst.push_back(qr.mid(pos, l));
	    }
	}
    }
#endif



    ///////////////////////////////////////////////////////////
    // Load text into editor window.
    prog = 2 * nsteps / 3;
    progress.setLabelText(tr("Loading preview text into editor"));
    qApp->processEvents();
    int instep = 0;
    for (list<QString>::iterator it = qrichlst.begin(); 
	 it != qrichlst.end(); it++, prog++, instep++) {
	progress.setValue(prog);
	qApp->processEvents();

	editor->append(*it);
        // We need to save the rich text for printing, the editor does
        // not do it consistently for us.
        editor->m_richtxt.append(*it);

	if (progress.wasCanceled()) {
            editor->append("<b>Cancelled !</b>");
	    LOGDEB(("LoadFileInCurrentTab: cancelled in editor load\n"));
	    break;
	}
    }

    progress.close();
    editor->m_curdsp = PreviewTextEdit::PTE_DSPTXT;

    ////////////////////////////////////////////////////////////////////////
    // Finishing steps

    // Maybe the text was actually empty ? Switch to fields then. Else free-up 
    // the text memory in the loaded document. We still have a copy of the text
    // in editor->m_richtxt
    bool textempty = fdoc.text.empty();
    if (!textempty)
        fdoc.text.clear(); 
    editor->m_fdoc = fdoc;
    editor->m_dbdoc = idoc;
    if (textempty)
        editor->displayFields();

    // If this is an image, display it instead of the text.
    if (!idoc.mimetype.compare(0, 6, "image/")) {
	string fn = fileurltolocalpath(idoc.url);

	// If the command wants a file but this is not a file url, or
	// there is an ipath that it won't understand, we need a temp file:
	theconfig->setKeyDir(path_getfather(fn));
	if (fn.empty() || !idoc.ipath.empty()) {
	    TempFile temp = lthr.imgtmp;
	    if (temp.isNotNull()) {
		LOGDEB1(("Preview: load: got temp file from internfile\n"));
	    } else if (!FileInterner::idocToFile(temp, string(), 
						 theconfig, idoc)) {
		temp.release(); // just in case.
	    }
	    if (temp.isNotNull()) {
		rememberTempFile(temp);
		fn = temp->filename();
		editor->m_tmpfilename = fn;
	    } else {
		editor->m_tmpfilename.erase();
		fn.erase();
	    }
	}

	if (!fn.empty()) {
	    editor->m_image = QImage(fn.c_str());
	    if (!editor->m_image.isNull())
		editor->displayImage();
	}
     }


    // Position the editor so that the first search term is visible
    if (searchTextCMB->currentText().length() != 0) {
	// If there is a current search string, perform the search
	m_canBeep = true;
	doSearch(searchTextCMB->currentText(), true, false);
    } else {
	// Position to the first query term
	if (editor->m_plaintorich->haveAnchors()) {
	    QString aname = editor->m_plaintorich->curAnchorName();
	    LOGDEB2(("Call movetoanchor(%s)\n", (const char *)aname.toUtf8()));
	    editor->scrollToAnchor(aname);
	    // Position the cursor approximately at the anchor (top of
	    // viewport) so that searches start from here
	    QTextCursor cursor = editor->cursorForPosition(QPoint(0, 0));
	    editor->setTextCursor(cursor);
	}
    }


    // Enter document in document history
    string udi;
    if (idoc.getmeta(Rcl::Doc::keyudi, &udi)) {
        historyEnterDoc(g_dynconf, udi);
    }

    editor->setFocus();
    emit(previewExposed(this, m_searchId, docnum));
    LOGDEB(("LoadFileInCurrentTab: returning true\n"));
    return true;
}

PreviewTextEdit::PreviewTextEdit(QWidget* parent, const char* nm, Preview *pv) 
    : QTextBrowser(parent), m_preview(pv), 
      m_plaintorich(new PlainToRichQtPreview()), 
      m_dspflds(false), m_docnum(-1) 
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setObjectName(nm);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),
	    this, SLOT(createPopupMenu(const QPoint&)));
    setOpenExternalLinks(false);
    setOpenLinks(false);
}

PreviewTextEdit::~PreviewTextEdit()
{
    delete m_plaintorich;
}

void PreviewTextEdit::createPopupMenu(const QPoint& pos)
{
    LOGDEB1(("PreviewTextEdit::createPopupMenu()\n"));
    QMenu *popup = new QMenu(this);
    switch (m_curdsp) {
    case PTE_DSPTXT:
	popup->addAction(tr("Show fields"), this, SLOT(displayFields()));
	if (!m_image.isNull())
	    popup->addAction(tr("Show image"), this, SLOT(displayImage()));
	break;
    case PTE_DSPFLDS:
	popup->addAction(tr("Show main text"), this, SLOT(displayText()));
	if (!m_image.isNull())
	    popup->addAction(tr("Show image"), this, SLOT(displayImage()));
	break;
    case PTE_DSPIMG:
    default:
	popup->addAction(tr("Show fields"), this, SLOT(displayFields()));
	popup->addAction(tr("Show main text"), this, SLOT(displayText()));
	break;
    }
    popup->addAction(tr("Select All"), this, SLOT(selectAll()));
    popup->addAction(tr("Copy"), this, SLOT(copy()));
    popup->addAction(tr("Print"), this, SLOT(print()));
    if (prefs.previewPlainPre) {
	popup->addAction(tr("Fold lines"), m_preview, SLOT(togglePlainPre()));
    } else {
	popup->addAction(tr("Preserve indentation"), 
			 m_preview, SLOT(togglePlainPre()));
    }
    // Need to check ipath until we fix the internfile bug that always
    // has it convert to html for top level docs
    if (!m_dbdoc.url.empty() && !m_dbdoc.ipath.empty())
	popup->addAction(tr("Save document to file"), 
			 m_preview, SLOT(emitSaveDocToFile()));
    popup->popup(mapToGlobal(pos));
}

// Display main text
void PreviewTextEdit::displayText()
{
    LOGDEB1(("PreviewTextEdit::displayText()\n"));
    if (m_format == Qt::PlainText)
	setPlainText(m_richtxt);
    else
	setHtml(m_richtxt);
    m_curdsp = PTE_DSPTXT;
}

// Display field values
void PreviewTextEdit::displayFields()
{
    LOGDEB1(("PreviewTextEdit::displayFields()\n"));

    QString txt = "<html><head></head><body>\n";
    txt += "<b>" + QString::fromLocal8Bit(m_url.c_str());
    if (!m_ipath.empty())
	txt += "|" + QString::fromUtf8(m_ipath.c_str());
    txt += "</b><br><br>";
    txt += "<dl>\n";
    for (map<string,string>::const_iterator it = m_fdoc.meta.begin();
	 it != m_fdoc.meta.end(); it++) {
	if (!it->second.empty())
	    txt += "<dt>" + QString::fromUtf8(it->first.c_str()) + "</dt> " 
		+ "<dd>" + QString::fromUtf8(escapeHtml(it->second).c_str()) 
		+ "</dd>\n";
    }
    txt += "</dl></body></html>";
    setHtml(txt);
    m_curdsp = PTE_DSPFLDS;
}

void PreviewTextEdit::displayImage()
{
    LOGDEB1(("PreviewTextEdit::displayImage()\n"));
    if (m_image.isNull())
	displayText();

    setPlainText("");
    if (m_image.width() > width() || 
	m_image.height() > height()) {
	m_image = m_image.scaled(width(), height(), Qt::KeepAspectRatio);
    }
    document()->addResource(QTextDocument::ImageResource, QUrl("image"), 
			    m_image);
    QTextCursor cursor = textCursor();
    cursor.insertImage("image");
    m_curdsp = PTE_DSPIMG;
}

void PreviewTextEdit::mouseDoubleClickEvent(QMouseEvent *event)
{
    LOGDEB2(("PreviewTextEdit::mouseDoubleClickEvent\n"));
    QTextEdit::mouseDoubleClickEvent(event);
    if (textCursor().hasSelection() && m_preview)
	m_preview->emitWordSelect(textCursor().selectedText());
}

void PreviewTextEdit::print()
{
    LOGDEB(("PreviewTextEdit::print\n"));
    if (!m_preview)
        return;
	
#ifndef QT_NO_PRINTER
    QPrinter printer;
    QPrintDialog *dialog = new QPrintDialog(&printer, this);
    dialog->setWindowTitle(tr("Print Current Preview"));
    if (dialog->exec() != QDialog::Accepted)
        return;
    QTextEdit::print(&printer);
#endif
}
