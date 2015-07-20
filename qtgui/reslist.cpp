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

#include <time.h>
#include <stdlib.h>

#include <qapplication.h>
#include <qvariant.h>
#include <qevent.h>
#include <qmenu.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qtimer.h>
#include <qmessagebox.h>
#include <qimage.h>
#include <qscrollbar.h>
#include <QTextBlock>
#include <QShortcut>
#ifndef __APPLE__
//#include <qx11info_x11.h>
#endif

#include "debuglog.h"
#include "smallut.h"
#include "recoll.h"
#include "guiutils.h"
#include "pathut.h"
#include "docseq.h"
#include "pathut.h"
#include "mimehandler.h"
#include "plaintorich.h"
#include "refcntr.h"
#include "internfile.h"
#include "indexer.h"
#include "snippets_w.h"
#include "listdialog.h"
#include "reslist.h"
#include "moc_reslist.cpp"
#include "rclhelp.h"
#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif
#include "appformime.h"
#include "respopup.h"

static const QKeySequence quitKeySeq("Ctrl+q");
static const QKeySequence closeKeySeq("Ctrl+w");

#ifndef RESLIST_TEXTBROWSER
#include <QWebFrame>
#include <QWebElement>
#include <QWebSettings>
#endif

// Decide if we set font family and style with a css section in the
// html <head> or with qwebsettings setfont... calls.  We currently do
// it with websettings because this gives an instant redisplay, and
// the css has a tendancy to not find some system fonts. Otoh,
// SetFontSize() needs a strange offset of 3, not needed with css.
#undef SETFONT_WITH_HEADSTYLE

class QtGuiResListPager : public ResListPager {
public:
    QtGuiResListPager(ResList *p, int ps) 
	: ResListPager(ps), m_reslist(p) 
    {}
    virtual bool append(const string& data);
    virtual bool append(const string& data, int idx, const Rcl::Doc& doc);
    virtual string trans(const string& in);
    virtual string detailsLink();
    virtual const string &parFormat();
    virtual const string &dateFormat();
    virtual string nextUrl();
    virtual string prevUrl();
    virtual string headerContent();
    virtual void suggest(const vector<string>uterms, 
			 map<string, vector<string> >& sugg);
    virtual string absSep() {return (const char *)(prefs.abssep.toUtf8());}
    virtual string iconUrl(RclConfig *, Rcl::Doc& doc);
private:
    ResList *m_reslist;
};

#if 0
FILE *fp;
void logdata(const char *data)
{
    if (fp == 0)
	fp = fopen("/tmp/recolltoto.html", "a");
    if (fp)
	fprintf(fp, "%s", data);
}
#else
#define logdata(X)
#endif

//////////////////////////////
// /// QtGuiResListPager methods:
bool QtGuiResListPager::append(const string& data)
{
    LOGDEB2(("QtGuiReslistPager::appendString   : %s\n", data.c_str()));
    logdata(data.c_str());
    m_reslist->append(QString::fromUtf8(data.c_str()));
    return true;
}

bool QtGuiResListPager::append(const string& data, int docnum, 
			       const Rcl::Doc&)
{
    LOGDEB2(("QtGuiReslistPager::appendDoc: blockCount %d, %s\n",
	    m_reslist->document()->blockCount(), data.c_str()));
    logdata(data.c_str());
#ifdef RESLIST_TEXTBROWSER
    int blkcnt0 = m_reslist->document()->blockCount();
    m_reslist->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
    m_reslist->textCursor().insertBlock();
    m_reslist->insertHtml(QString::fromUtf8(data.c_str()));
    m_reslist->moveCursor(QTextCursor::Start, QTextCursor::MoveAnchor);
    m_reslist->ensureCursorVisible();
    int blkcnt1 = m_reslist->document()->blockCount();
    for (int block = blkcnt0; block < blkcnt1; block++) {
	m_reslist->m_pageParaToReldocnums[block] = docnum;
    }
#else
    QString sdoc = QString("<div class=\"rclresult\" rcldocnum=\"%1\">").arg(docnum);
    m_reslist->append(sdoc);
    m_reslist->append(QString::fromUtf8(data.c_str()));
    m_reslist->append("</div>");
#endif
    return true;
}

string QtGuiResListPager::trans(const string& in)
{
    return string((const char*)ResList::tr(in.c_str()).toUtf8());
}

string QtGuiResListPager::detailsLink()
{
    string chunk = "<a href=\"H-1\">";
    chunk += trans("(show query)");
    chunk += "</a>";
    return chunk;
}

const string& QtGuiResListPager::parFormat()
{
    return prefs.creslistformat;
}
const string& QtGuiResListPager::dateFormat()
{
    return prefs.creslistdateformat;
}

string QtGuiResListPager::nextUrl()
{
    return "n-1";
}

string QtGuiResListPager::prevUrl()
{
    return "p-1";
}

string QtGuiResListPager::headerContent() 
{
    string out;
#ifdef SETFONT_WITH_HEADSTYLE
    out = "<style type=\"text/css\">\nbody,table,select,input {\n";
    char ftsz[30];
    sprintf(ftsz, "%d", prefs.reslistfontsize);
    out += string("font-family: \"") + qs2utf8s(prefs.reslistfontfamily)
        + "\";\n";
    out += string("font-size: ") + ftsz + "pt;\n";
    out += string("}\n</style>\n");
#endif
    out += qs2utf8s(prefs.reslistheadertext);
    return out;
}

void QtGuiResListPager::suggest(const vector<string>uterms, 
				map<string, vector<string> >& sugg)
{
    sugg.clear();
#ifdef RCL_USE_ASPELL
    bool noaspell = false;
    theconfig->getConfParam("noaspell", &noaspell);
    if (noaspell)
        return;
    if (!aspell) {
        LOGERR(("QtGuiResListPager:: aspell not initialized\n"));
        return;
    }

    bool issimple = m_reslist && m_reslist->m_rclmain && 
	m_reslist->m_rclmain->lastSearchSimple();

    for (vector<string>::const_iterator uit = uterms.begin();
         uit != uterms.end(); uit++) {
        list<string> asuggs;
        string reason;

	// If the term is in the dictionary, Aspell::suggest won't
	// list alternatives. In fact we may want to check the
	// frequencies and propose something anyway if a possible
	// variation is much more common (as google does) ?
        if (!aspell->suggest(*rcldb, *uit, asuggs, reason)) {
            LOGERR(("QtGuiResListPager::suggest: aspell failed: %s\n", 
                    reason.c_str()));
            continue;
        }

	// We should check that the term stems differently from the
	// base word (else it's not useful to expand the search). Or
	// is it ? This should depend if stemming is turned on or not

        if (!asuggs.empty()) {
            sugg[*uit] = vector<string>(asuggs.begin(), asuggs.end());
	    if (sugg[*uit].size() > 5)
		sugg[*uit].resize(5);
	    // Set up the links as a <href="Sold|new">. 
	    for (vector<string>::iterator it = sugg[*uit].begin();
		 it != sugg[*uit].end(); it++) {
		if (issimple) {
		    *it = string("<a href=\"S") + *uit + "|" + *it + "\">" +
			*it + "</a>";
		}
	    }
        }
    }
#endif

}

string QtGuiResListPager::iconUrl(RclConfig *config, Rcl::Doc& doc)
{
    if (doc.ipath.empty()) {
	vector<Rcl::Doc> docs;
	docs.push_back(doc);
	vector<string> paths;
	ConfIndexer::docsToPaths(docs, paths);
	if (!paths.empty()) {
	    string path;
	    LOGDEB0(("ResList::iconUrl: source path [%s]\n", paths[0].c_str()));
	    if (thumbPathForUrl(cstr_fileu + paths[0], 128, path)) {
		LOGDEB0(("ResList::iconUrl: icon path [%s]\n", path.c_str()));
		return cstr_fileu + path;
	    } else {
		LOGDEB0(("ResList::iconUrl: no icon: path [%s]\n", 
			 path.c_str()));
	    }
	} else {
	    LOGDEB(("ResList::iconUrl: docsToPaths failed\n"));
	}
    }
    return ResListPager::iconUrl(config, doc);
}

/////// /////// End reslistpager methods

class PlainToRichQtReslist : public PlainToRich {
public:
    virtual string startMatch(unsigned int idx)
    {
	if (m_hdata) {
	    string s1, s2;
	    stringsToString<vector<string> >(m_hdata->groups[idx], s1); 
	    stringsToString<vector<string> >(m_hdata->ugroups[m_hdata->grpsugidx[idx]], s2);
	    LOGDEB(("Reslist startmatch: group %s user group %s\n", s1.c_str(), s2.c_str()));
	}
		
	return string("<span class='rclmatch' style='color: ")
	    + qs2utf8s(prefs.qtermcolor) + string("'>");
    }
    virtual string endMatch() 
    {
	return string("</span>");
    }
};
static PlainToRichQtReslist g_hiliter;

/////////////////////////////////////

ResList::ResList(QWidget* parent, const char* name)
    : RESLIST_PARENTCLASS(parent), m_curPvDoc(-1), m_lstClckMod(0), 
      m_listId(0), m_rclmain(0), m_ismainres(true)
{
    if (!name)
	setObjectName("resList");
    else 
	setObjectName(name);
#ifdef RESLIST_TEXTBROWSER
    LOGDEB(("Reslist: using QTextBrowser\n"));
    setReadOnly(TRUE);
    setUndoRedoEnabled(FALSE);
    setOpenLinks(FALSE);
    setTabChangesFocus(true);
    // signals and slots connections
    connect(this, SIGNAL(anchorClicked(const QUrl &)), 
	    this, SLOT(linkWasClicked(const QUrl &)));
#else
    LOGDEB(("Reslist: using QWebView\n"));
    // signals and slots connections
    connect(this, SIGNAL(linkClicked(const QUrl &)), 
	    this, SLOT(linkWasClicked(const QUrl &)));
    page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    settings()->setAttribute(QWebSettings::JavascriptEnabled, true);
#endif

    setFont();
    languageChange();

    (void)new HelpClient(this);
    HelpClient::installMap(qs2utf8s(this->objectName()), "RCL.SEARCH.RESLIST");

#if 0
    // See comments in "highlighted
    connect(this, SIGNAL(highlighted(const QString &)), 
	    this, SLOT(highlighted(const QString &)));
#endif

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),
	    this, SLOT(createPopupMenu(const QPoint&)));

    m_pager = new QtGuiResListPager(this, prefs.respagesize);
    m_pager->setHighLighter(&g_hiliter);
}

ResList::~ResList()
{
    // These have to exist somewhere for translations to work
#ifdef __GNUC__
    __attribute__((unused))
#endif
    static const char* strings[] = {
	QT_TR_NOOP("<p><b>No results found</b><br>"),
	QT_TR_NOOP("Documents"),
	QT_TR_NOOP("out of at least"),
	QT_TR_NOOP("for"),
	QT_TR_NOOP("Previous"),
	QT_TR_NOOP("Next"),
	QT_TR_NOOP("Unavailable document"),
	QT_TR_NOOP("Preview"),
	QT_TR_NOOP("Open"),
	QT_TR_NOOP("(show query)"),
        QT_TR_NOOP("<p><i>Alternate spellings (accents suppressed): </i>"),
        QT_TR_NOOP("<p><i>Alternate spellings: </i>"),
    };
}

void ResList::setRclMain(RclMain *m, bool ismain) 
{
    m_rclmain = m;
    m_ismainres = ismain;
    if (!m_ismainres) {
	connect(new QShortcut(closeKeySeq, this), SIGNAL (activated()), 
		this, SLOT (close()));
	connect(new QShortcut(quitKeySeq, this), SIGNAL (activated()), 
		m_rclmain, SLOT (fileExit()));
	connect(this, SIGNAL(previewRequested(Rcl::Doc)), 
		m_rclmain, SLOT(startPreview(Rcl::Doc)));
	connect(this, SIGNAL(docSaveToFileClicked(Rcl::Doc)), 
		m_rclmain, SLOT(saveDocToFile(Rcl::Doc)));
	connect(this, SIGNAL(editRequested(Rcl::Doc)), 
		m_rclmain, SLOT(startNativeViewer(Rcl::Doc)));
    }
}

void ResList::setFont()
{
#ifdef RESLIST_TEXTBROWSER
    if (prefs.reslistfontfamily.length()) {
	QFont nfont(prefs.reslistfontfamily, prefs.reslistfontsize);
	QTextBrowser::setFont(nfont);
    } else {
	QTextBrowser::setFont(QFont());
    }
#else
#ifndef SETFONT_WITH_HEADSTYLE
    QWebSettings *websettings = settings();
    if (prefs.reslistfontfamily.length()) {
        // For some reason there is (12-2014) an offset of 3 between what
        // we request from webkit and what we get.
	websettings->setFontSize(QWebSettings::DefaultFontSize, 
				 prefs.reslistfontsize + 3);
	websettings->setFontFamily(QWebSettings::StandardFont, 
			       prefs.reslistfontfamily);
    } else {
	websettings->resetFontSize(QWebSettings::DefaultFontSize);
	websettings->resetFontFamily(QWebSettings::StandardFont);
    }
#endif
#endif
}

int ResList::newListId()
{
    static int id;
    return ++id;
}

extern "C" int XFlush(void *);

void ResList::setDocSource(RefCntr<DocSequence> nsource)
{
    LOGDEB(("ResList::setDocSource()\n"));
    m_source = RefCntr<DocSequence>(new DocSource(theconfig, nsource));
}

// A query was executed, or the filtering/sorting parameters changed,
// re-read the results.
void ResList::readDocSource()
{
    LOGDEB(("ResList::readDocSource()\n"));
    resetView();
    if (m_source.isNull())
	return;
    m_listId = newListId();

    // Reset the page size in case the preference was changed
    m_pager->setPageSize(prefs.respagesize);
    m_pager->setDocSource(m_source);
    resultPageNext();
    emit hasResults(m_source->getResCnt());
}

void ResList::resetList() 
{
    LOGDEB(("ResList::resetList()\n"));
    setDocSource(RefCntr<DocSequence>());
    resetView();
}

void ResList::resetView() 
{
    m_curPvDoc = -1;
    // There should be a progress bar for long searches but there isn't 
    // We really want the old result list to go away, otherwise, for a
    // slow search, the user will wonder if anything happened. The
    // following helps making sure that the textedit is really
    // blank. Else, there are often icons or text left around
#ifdef RESLIST_TEXTBROWSER
    m_pageParaToReldocnums.clear();
    clear();
    QTextBrowser::append(".");
    clear();
#ifndef __APPLE__
//    XFlush(QX11Info::display());
#endif
#else
    m_text = "";
    setHtml("<html><body></body></html>");
#endif

}

bool ResList::displayingHistory()
{
    // We want to reset the displayed history if it is currently
    // shown. Using the title value is an ugly hack
    string htstring = string((const char *)tr("Document history").toUtf8());
    if (m_source.isNull() || m_source->title().empty())
	return false;
    return m_source->title().find(htstring) == 0;
}

void ResList::languageChange()
{
    setWindowTitle(tr("Result list"));
}

#ifdef RESLIST_TEXTBROWSER    
// Get document number from text block number
int ResList::docnumfromparnum(int block)
{
    if (m_pager->pageNumber() < 0)
	return -1;

    // Try to find the first number < input and actually in the map
    // (result blocks can be made of several text blocks)
    std::map<int,int>::iterator it;
    do {
	it = m_pageParaToReldocnums.find(block);
	if (it != m_pageParaToReldocnums.end())
	    return pageFirstDocNum() + it->second;
    } while (--block >= 0);
    return -1;
}

// Get range of paragraph numbers which make up the result for document number
pair<int,int> ResList::parnumfromdocnum(int docnum)
{
    LOGDEB(("parnumfromdocnum: docnum %d\n", docnum));
    if (m_pager->pageNumber() < 0) {
	LOGDEB(("parnumfromdocnum: no page return -1,-1\n"));
	return pair<int,int>(-1,-1);
    }
    int winfirst = pageFirstDocNum();
    if (docnum - winfirst < 0) {
	LOGDEB(("parnumfromdocnum: docnum %d < winfirst %d return -1,-1\n",
		docnum, winfirst));
	return pair<int,int>(-1,-1);
    }
    docnum -= winfirst;
    for (std::map<int,int>::iterator it = m_pageParaToReldocnums.begin();
	 it != m_pageParaToReldocnums.end(); it++) {
	if (docnum == it->second) {
	    int first = it->first;
	    int last = first+1;
	    std::map<int,int>::iterator it1;
	    while ((it1 = m_pageParaToReldocnums.find(last)) != 
		   m_pageParaToReldocnums.end() && it1->second == docnum) {
		last++;
	    }
	    LOGDEB(("parnumfromdocnum: return %d,%d\n", first, last));
	    return pair<int,int>(first, last);
	}
    }
    LOGDEB(("parnumfromdocnum: not found return -1,-1\n"));
    return pair<int,int>(-1,-1);
}
#endif // TEXTBROWSER

// Return doc from current or adjacent result pages. We can get called
// for a document not in the current page if the user browses through
// results inside a result window (with shift-arrow). This can only
// result in a one-page change.
bool ResList::getDoc(int docnum, Rcl::Doc &doc)
{
    LOGDEB(("ResList::getDoc: docnum %d winfirst %d\n", docnum, 
	    pageFirstDocNum()));
    int winfirst = pageFirstDocNum();
    int winlast = m_pager->pageLastDocNum();
    if (docnum < 0 ||  winfirst < 0 || winlast < 0)
	return false;

    // Is docnum in current page ? Then all Ok
    if (docnum >= winfirst && docnum <= winlast) {
	return m_pager->getDoc(docnum, doc);
    }

    // Else we accept to page down or up but not further
    if (docnum < winfirst && docnum >= winfirst - prefs.respagesize) {
	resultPageBack();
    } else if (docnum < winlast + 1 + prefs.respagesize) {
	resultPageNext();
    }
    winfirst = pageFirstDocNum();
    winlast = m_pager->pageLastDocNum();
    if (docnum >= winfirst && docnum <= winlast) {
	return m_pager->getDoc(docnum, doc);
    }
    return false;
}

void ResList::keyPressEvent(QKeyEvent * e)
{
    if ((e->modifiers() & Qt::ShiftModifier)) {
	if (e->key() == Qt::Key_PageUp) {
	    // Shift-PageUp -> first page of results
	    resultPageFirst();
	    return;
	} 
    } else {
	if (e->key() == Qt::Key_PageUp || e->key() == Qt::Key_Backspace) {
	    resPageUpOrBack();
	    return;
	} else if (e->key() == Qt::Key_PageDown || e->key() == Qt::Key_Space) {
	    resPageDownOrNext();
	    return;
	}
    }
    RESLIST_PARENTCLASS::keyPressEvent(e);
}

void ResList::mouseReleaseEvent(QMouseEvent *e)
{
    m_lstClckMod = 0;
    if (e->modifiers() & Qt::ControlModifier) {
	m_lstClckMod |= Qt::ControlModifier;
    } 
    if (e->modifiers() & Qt::ShiftModifier) {
	m_lstClckMod |= Qt::ShiftModifier;
    }
    RESLIST_PARENTCLASS::mouseReleaseEvent(e);
}

void ResList::highlighted(const QString& )
{
    // This is supposedly called when a link is preactivated (hover or tab
    // traversal, but is not actually called for tabs. We would have liked to
    // give some kind of visual feedback for tab traversal
}

// Page Up/Down: we don't try to check if current paragraph is last or
// first. We just page up/down and check if viewport moved. If it did,
// fair enough, else we go to next/previous result page.
void ResList::resPageUpOrBack()
{
#ifdef RESLIST_TEXTBROWSER
    int vpos = verticalScrollBar()->value();
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
    if (vpos == verticalScrollBar()->value())
	resultPageBack();
#else
    if (scrollIsAtTop()) {
	resultPageBack();
    } else {
	QWebFrame *frame = page()->mainFrame();
	frame->scroll(0, -int(0.9*geometry().height()));
    }
    setupArrows();
#endif
}

void ResList::resPageDownOrNext()
{
#ifdef RESLIST_TEXTBROWSER
    int vpos = verticalScrollBar()->value();
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    LOGDEB(("ResList::resPageDownOrNext: vpos before %d, after %d\n",
	    vpos, verticalScrollBar()->value()));
    if (vpos == verticalScrollBar()->value()) 
	resultPageNext();
#else
    if (scrollIsAtBottom()) {
	resultPageNext();
    } else {
	QWebFrame *frame = page()->mainFrame();
	frame->scroll(0, int(0.9*geometry().height()));
    }
    setupArrows();
#endif
}

void ResList::setupArrows()
{
    emit prevPageAvailable(m_pager->hasPrev() || !scrollIsAtTop());
    emit nextPageAvailable(m_pager->hasNext() || !scrollIsAtBottom());
}

bool ResList::scrollIsAtBottom()
{
#ifdef RESLIST_TEXTBROWSER
    return false;
#else
    QWebFrame *frame = page()->mainFrame();
    bool ret;
    if (!frame || frame->scrollBarGeometry(Qt::Vertical).isEmpty()) {
	ret = true;
    } else {
	int max = frame->scrollBarMaximum(Qt::Vertical);
	int cur = frame->scrollBarValue(Qt::Vertical); 
	ret = (max != 0) && (cur == max);
	LOGDEB2(("Scrollatbottom: cur %d max %d\n", cur, max));
    }
    LOGDEB2(("scrollIsAtBottom: returning %d\n", ret));
    return ret;
#endif
}

bool ResList::scrollIsAtTop()
{
#ifdef RESLIST_TEXTBROWSER
    return false;
#else
    QWebFrame *frame = page()->mainFrame();
    bool ret;
    if (!frame || frame->scrollBarGeometry(Qt::Vertical).isEmpty()) {
	ret = true;
    } else {
	int cur = frame->scrollBarValue(Qt::Vertical);
	int min = frame->scrollBarMinimum(Qt::Vertical);
	LOGDEB(("Scrollattop: cur %d min %d\n", cur, min));
	ret = (cur == min);
    }
    LOGDEB2(("scrollIsAtTop: returning %d\n", ret));
    return ret;
#endif
}

// Show previous page of results. We just set the current number back
// 2 pages and show next page.
void ResList::resultPageBack()
{
    if (m_pager->hasPrev()) {
	m_pager->resultPageBack();
	displayPage();
    }
}

// Go to the first page
void ResList::resultPageFirst()
{
    // In case the preference was changed
    m_pager->setPageSize(prefs.respagesize);
    m_pager->resultPageFirst();
    displayPage();
}

// Fill up result list window with next screen of hits
void ResList::resultPageNext()
{
    if (m_pager->hasNext()) {
	m_pager->resultPageNext();
	displayPage();
    }
}

void ResList::resultPageFor(int docnum)
{
    m_pager->resultPageFor(docnum);
    displayPage();
}

void ResList::append(const QString &text)
{
    LOGDEB2(("QtGuiReslistPager::appendQString  : %s\n", 
	    (const char*)text.toUtf8()));
#ifdef RESLIST_TEXTBROWSER
    QTextBrowser::append(text);
#else
    m_text += text;
#endif
}

void ResList::displayPage()
{
    resetView();

    m_pager->displayPage(theconfig);

#ifndef RESLIST_TEXTBROWSER
    setHtml(m_text);
#endif

    LOGDEB0(("ResList::displayPg: hasNext %d atBot %d hasPrev %d at Top %d \n",
	     m_pager->hasPrev(), scrollIsAtBottom(), 
	     m_pager->hasNext(), scrollIsAtTop()));
    setupArrows();

    // Possibly color paragraph of current preview if any
    previewExposed(m_curPvDoc);
}

// Color paragraph (if any) of currently visible preview
void ResList::previewExposed(int docnum)
{
    LOGDEB(("ResList::previewExposed: doc %d\n", docnum));

    // Possibly erase old one to white
    if (m_curPvDoc != -1) {
#ifdef RESLIST_TEXTBROWSER
	pair<int,int> blockrange = parnumfromdocnum(m_curPvDoc);
	if (blockrange.first != -1) {
	    for (int blockn = blockrange.first;
		 blockn < blockrange.second; blockn++) {
		QTextBlock block = document()->findBlockByNumber(blockn);
		QTextCursor cursor(block);
		QTextBlockFormat format = cursor.blockFormat();
		format.clearBackground();
		cursor.setBlockFormat(format);
	    }
	}
#else
	QString sel = 
	   QString("div[rcldocnum=\"%1\"]").arg(m_curPvDoc - pageFirstDocNum());
	LOGDEB2(("Searching for element, selector: [%s]\n", 
			 qs2utf8s(sel).c_str()));
	QWebElement elt = page()->mainFrame()->findFirstElement(sel);
	if (!elt.isNull()) {
	    LOGDEB2(("Found\n"));
	    elt.removeAttribute("style");
	} else {
	    LOGDEB2(("Not Found\n"));
	}
#endif
	m_curPvDoc = -1;
    }

    // Set background for active preview's doc entry
    m_curPvDoc = docnum;

#ifdef RESLIST_TEXTBROWSER
    pair<int,int>  blockrange = parnumfromdocnum(docnum);

    // Maybe docnum is -1 or not in this window, 
    if (blockrange.first < 0)
	return;
    // Color the new active paragraph
    QColor color("LightBlue");
    for (int blockn = blockrange.first+1;
	 blockn < blockrange.second; blockn++) {
	QTextBlock block = document()->findBlockByNumber(blockn);
	QTextCursor cursor(block);
	QTextBlockFormat format;
	format.setBackground(QBrush(color));
	cursor.mergeBlockFormat(format);
	setTextCursor(cursor);
	ensureCursorVisible();
    }
#else
    QString sel = 
	QString("div[rcldocnum=\"%1\"]").arg(docnum - pageFirstDocNum());
    LOGDEB2(("Searching for element, selector: [%s]\n", 
			 qs2utf8s(sel).c_str()));
    QWebElement elt = page()->mainFrame()->findFirstElement(sel);
    if (!elt.isNull()) {
	LOGDEB2(("Found\n"));
	elt.setAttribute("style", "background: LightBlue;}");
    } else {
	LOGDEB2(("Not Found\n"));
    }
#endif
}

// Double click in res list: add selection to simple search
void ResList::mouseDoubleClickEvent(QMouseEvent *event)
{
    RESLIST_PARENTCLASS::mouseDoubleClickEvent(event);
#ifdef RESLIST_TEXTBROWSER
    if (textCursor().hasSelection())
	emit(wordSelect(textCursor().selectedText()));
#else
    emit(wordSelect(selectedText()));
#endif
}

void ResList::showQueryDetails()
{
    if (m_source.isNull())
	return;
    string oq = breakIntoLines(m_source->getDescription(), 100, 50);
    QString str;
    QString desc = tr("Result count (est.)") + ": " + 
	str.setNum(m_source->getResCnt()) + "<br>";
    desc += tr("Query details") + ": " + QString::fromUtf8(oq.c_str());
    QMessageBox::information(this, tr("Query details"), desc);
}

void ResList::linkWasClicked(const QUrl &url)
{
    string ascurl = qs2utf8s(url.toString());
    LOGDEB(("ResList::linkWasClicked: [%s]\n", ascurl.c_str()));

    int what = ascurl[0];
    switch (what) {

    // Open abstract/snippets window
    case 'A':
    {
	if (m_source.isNull()) 
	    return;
	int i = atoi(ascurl.c_str()+1) - 1;
	Rcl::Doc doc;
	if (!getDoc(i, doc)) {
	    LOGERR(("ResList::linkWasClicked: can't get doc for %d\n", i));
	    return;
	}
	emit(showSnippets(doc));
    }
    break;

    // Show duplicates
    case 'D':
    {
	if (m_source.isNull()) 
	    return;
	int i = atoi(ascurl.c_str()+1) - 1;
	Rcl::Doc doc;
	if (!getDoc(i, doc)) {
	    LOGERR(("ResList::linkWasClicked: can't get doc for %d\n", i));
	    return;
	}
	vector<Rcl::Doc> dups;
	if (m_source->docDups(doc, dups) && m_rclmain) {
	    m_rclmain->newDupsW(doc, dups);
	}
    }
    break;

    // Open parent folder
    case 'F':
    {
	int i = atoi(ascurl.c_str()+1) - 1;
	Rcl::Doc doc;
	if (!getDoc(i, doc)) {
	    LOGERR(("ResList::linkWasClicked: can't get doc for %d\n", i));
	    return;
	}
        emit editRequested(ResultPopup::getParent(RefCntr<DocSequence>(),
                                                  doc));
    }
    break;

    // Show query details
    case 'H': 
    {
	showQueryDetails();
	break;
    }

    // Preview and edit
    case 'P': 
    case 'E': 
    {
	int i = atoi(ascurl.c_str()+1) - 1;
	Rcl::Doc doc;
	if (!getDoc(i, doc)) {
	    LOGERR(("ResList::linkWasClicked: can't get doc for %d\n", i));
	    return;
	}
	if (what == 'P') {
	    if (m_ismainres) {
		emit docPreviewClicked(i, doc, m_lstClckMod);
	    } else {
		emit previewRequested(doc);
	    }
	} else {
	    emit editRequested(doc);
	}
    }
    break;

    // Next/prev page
    case 'n':
	resultPageNext();
	break;
    case 'p':
	resultPageBack();
	break;

	// Run script. Link format Rnn|Script Name
    case 'R':
    {
	int i = atoi(ascurl.c_str() + 1) - 1;
	QString s = url.toString();
	int bar = s.indexOf("|");
	if (bar == -1 || bar >= s.size()-1)
            break;
        string cmdname = qs2utf8s(s.right(s.size() - (bar + 1)));
        DesktopDb ddb(path_cat(theconfig->getConfDir(), "scripts"));
        DesktopDb::AppDef app;
        if (ddb.appByName(cmdname, app)) {
            QAction act(QString::fromUtf8(app.name.c_str()), this);
            QVariant v(QString::fromUtf8(app.command.c_str()));
            act.setData(v);
            m_popDoc = i;
            menuOpenWith(&act);
        }
    }
    break;

	// Spelling: replacement suggestion clicked
    case 'S':
    {
	QString s = url.toString();
	if (!s.isEmpty())
	    s = s.right(s.size()-1);
	int bar = s.indexOf("|");
	if (bar != -1 && bar < s.size()-1) {
	    QString o = s.left(bar);
	    QString n = s.right(s.size() - (bar+1));
	    emit wordReplace(o, n);
	}
    }
    break;

    default: 
	LOGERR(("ResList::linkWasClicked: bad link [%s]\n", ascurl.c_str()));
	break;// ?? 
    }
}

void ResList::createPopupMenu(const QPoint& pos)
{
    LOGDEB(("ResList::createPopupMenu(%d, %d)\n", pos.x(), pos.y()));
#ifdef RESLIST_TEXTBROWSER
    QTextCursor cursor = cursorForPosition(pos);
    int blocknum = cursor.blockNumber();
    LOGDEB(("ResList::createPopupMenu(): block %d\n", blocknum));
    m_popDoc = docnumfromparnum(blocknum);
#else
    QWebHitTestResult htr = page()->mainFrame()->hitTestContent(pos);
    if (htr.isNull())
	return;
    QWebElement el = htr.enclosingBlockElement();
    while (!el.isNull() && !el.hasAttribute("rcldocnum"))
	el = el.parent();
    if (el.isNull())
	return;
    QString snum = el.attribute("rcldocnum");
    m_popDoc = pageFirstDocNum() + snum.toInt();
#endif

    if (m_popDoc < 0) 
	return;
    Rcl::Doc doc;
    if (!getDoc(m_popDoc, doc))
	return;

    int options =  ResultPopup::showSaveOne;
    if (m_ismainres)
	options |= ResultPopup::isMain;
    QMenu *popup = ResultPopup::create(this, options, m_source, doc);
    popup->popup(mapToGlobal(pos));
}

void ResList::menuPreview()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc)) {
	if (m_ismainres) {
	    emit docPreviewClicked(m_popDoc, doc, 0);
	} else {
	    emit previewRequested(doc);
	}
    }
}

void ResList::menuSaveToFile()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
	emit docSaveToFileClicked(doc);
}

void ResList::menuPreviewParent()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc) && !m_source.isNull())  {
	Rcl::Doc pdoc = ResultPopup::getParent(m_source, doc);
	if (pdoc.mimetype == "inode/directory") {
	    emit editRequested(pdoc);
	} else {
	    emit previewRequested(pdoc);
	}
    }
}

void ResList::menuOpenParent()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc) && m_source.isNotNull()) 
	emit editRequested(ResultPopup::getParent(m_source, doc));
}

void ResList::menuShowSnippets()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
	emit showSnippets(doc);
}

void ResList::menuShowSubDocs()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc)) 
	emit showSubDocs(doc);
}

void ResList::menuEdit()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
	emit editRequested(doc);
}
void ResList::menuOpenWith(QAction *act)
{
    if (act == 0)
        return;
    string cmd = qs2utf8s(act->data().toString());
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
	emit openWithRequested(doc, cmd);
}

void ResList::menuCopyFN()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
	ResultPopup::copyFN(doc);
}

void ResList::menuCopyURL()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
	ResultPopup::copyURL(doc);
}

void ResList::menuExpand()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
	emit docExpand(doc);
}
int ResList::pageFirstDocNum()
{
    return m_pager->pageFirstDocNum();
}
