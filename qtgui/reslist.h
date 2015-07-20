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

#ifndef _RESLIST_H_INCLUDED_
#define _RESLIST_H_INCLUDED_

#include <list>
#include <utility>

#ifndef NO_NAMESPACES
using std::list;
using std::pair;
#endif

#ifdef RESLIST_TEXTBROWSER
#include <QTextBrowser>
#define RESLIST_PARENTCLASS QTextBrowser
#else
#include <QWebView>
#define RESLIST_PARENTCLASS QWebView
#endif

#include "docseq.h"
#include "sortseq.h"
#include "filtseq.h"
#include "refcntr.h"
#include "rcldoc.h"
#include "reslistpager.h"

class RclMain;
class QtGuiResListPager;

/**
 * Display a list of document records. The data can be out of the history 
 * manager or from an index query, both abstracted as a DocSequence. 
 */
class ResList : public RESLIST_PARENTCLASS
{
    Q_OBJECT;

    friend class QtGuiResListPager;
 public:
    ResList(QWidget* parent = 0, const char* name = 0);
    virtual ~ResList();
    
    // Return document for given docnum. We mostly act as an
    // intermediary to the docseq here, but this has also the
    // side-effect of making the entry current (visible and
    // highlighted), and only works if the num is inside the current
    // page or its immediate neighbours.
    bool getDoc(int docnum, Rcl::Doc &);
    bool displayingHistory();
    int listId() const {return m_listId;}
    int pageFirstDocNum();
    void setFont();
    void setRclMain(RclMain *m, bool ismain);

 public slots:
    virtual void setDocSource(RefCntr<DocSequence> nsource);
    virtual void resetList();     // Erase current list
    virtual void resPageUpOrBack(); // Page up pressed
    virtual void resPageDownOrNext(); // Page down pressed
    virtual void resultPageBack(); // Previous page of results
    virtual void resultPageFirst(); // First page of results
    virtual void resultPageNext(); // Next (or first) page of results
    virtual void resultPageFor(int docnum); // Page containing docnum
    virtual void menuPreview();
    virtual void menuSaveToFile();
    virtual void menuEdit();
    virtual void menuOpenWith(QAction *);
    virtual void menuCopyFN();
    virtual void menuCopyURL();
    virtual void menuExpand();
    virtual void menuPreviewParent();
    virtual void menuOpenParent();
    virtual void menuShowSnippets();
    virtual void menuShowSubDocs();
    virtual void previewExposed(int);
    virtual void append(const QString &text);
    virtual void readDocSource();
    virtual void highlighted(const QString& link);
    virtual void createPopupMenu(const QPoint& pos);
    virtual void showQueryDetails();
	
 signals:
    void nextPageAvailable(bool);
    void prevPageAvailable(bool);
    void docPreviewClicked(int, Rcl::Doc, int);
    void docSaveToFileClicked(Rcl::Doc);
    void previewRequested(Rcl::Doc);
    void showSnippets(Rcl::Doc);
    void showSubDocs(Rcl::Doc);
    void editRequested(Rcl::Doc);
    void openWithRequested(Rcl::Doc, string cmd);
    void docExpand(Rcl::Doc);
    void wordSelect(QString);
    void wordReplace(const QString&, const QString&);
    void hasResults(int);

 protected:
    void keyPressEvent(QKeyEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
    void mouseDoubleClickEvent(QMouseEvent*);

 protected slots:
    virtual void languageChange();
    virtual void linkWasClicked(const QUrl &);

 private:
    QtGuiResListPager  *m_pager;
    RefCntr<DocSequence> m_source;
    int        m_popDoc; // Docnum for the popup menu.
    int        m_curPvDoc;// Docnum for current preview
    int        m_lstClckMod; // Last click modifier. 
    int        m_listId; // query Id for matching with preview windows

#ifdef RESLIST_TEXTBROWSER    
    // Translate from textedit paragraph number to relative
    // docnum. Built while we insert text into the qtextedit
    std::map<int,int>  m_pageParaToReldocnums;
    virtual int docnumfromparnum(int);
    virtual pair<int,int> parnumfromdocnum(int);
#else
    // Webview makes it more difficult to append text incrementally,
    // so we store the page and display it when done.
    QString    m_text; 
#endif
    RclMain   *m_rclmain;
    bool m_ismainres;

    virtual void displayPage(); // Display current page
    static int newListId();
    void resetView();
    bool scrollIsAtTop();
    bool scrollIsAtBottom();
    void setupArrows();
 };


#endif /* _RESLIST_H_INCLUDED_ */
