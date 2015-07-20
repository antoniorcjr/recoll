/* Copyright (C) 2006 J.F.Dockes
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
#include <qapplication.h>
#include <qinputdialog.h>
#include <qvariant.h>
#include <qpushbutton.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qmessagebox.h>
#include <qevent.h>
#include <QTimer>
#include <QCompleter>

#include "debuglog.h"
#include "guiutils.h"
#include "searchdata.h"
#include "ssearch_w.h"
#include "refcntr.h"
#include "textsplit.h"
#include "wasatorcl.h"
#include "rclhelp.h"

// Typing interval after which we consider starting autosearch: no sense to do
// this is user is typing fast and continuously
static const int strokeTimeoutMS = 250;

void SSearch::init()
{
    // See enum above and keep in order !
    searchTypCMB->addItem(tr("Any term"));
    searchTypCMB->addItem(tr("All terms"));
    searchTypCMB->addItem(tr("File name"));
    searchTypCMB->addItem(tr("Query language"));
    
    // We'd like to use QComboBox::InsertAtTop but it doesn't do lru
    // (existing item stays at its place instead of jumping at top)
    queryText->setInsertPolicy(QComboBox::NoInsert);

    queryText->addItems(prefs.ssearchHistory);
    queryText->setEditText("");
    connect(queryText->lineEdit(), SIGNAL(returnPressed()),
	    this, SLOT(startSimpleSearch()));
    connect(queryText->lineEdit(), SIGNAL(textChanged(const QString&)),
	    this, SLOT(searchTextChanged(const QString&)));
    connect(clearqPB, SIGNAL(clicked()), 
	    queryText->lineEdit(), SLOT(clear()));
    connect(searchPB, SIGNAL(clicked()), this, SLOT(startSimpleSearch()));
    connect(searchTypCMB, SIGNAL(activated(int)), this, SLOT(searchTypeChanged(int)));

    queryText->installEventFilter(this);
    queryText->view()->installEventFilter(this);
    queryText->setInsertPolicy(QComboBox::NoInsert);
    // Note: we can't do the obvious and save the completer instead because
    // the combobox lineedit will delete the completer on setCompleter(0).
    // But the model does not belong to the completer so it's not deleted...
    m_savedModel = queryText->completer()->model();
    if (prefs.ssearchNoComplete)
	queryText->completer()->setModel(0);
    // Recoll searches are always case-sensitive because of the use of
    // capitalization to suppress stemming
    queryText->completer()->setCaseSensitivity(Qt::CaseSensitive);
    m_displayingCompletions = false;
    m_escape = false;
    m_disableAutosearch = true;
    m_stroketimeout = new QTimer(this);
    m_stroketimeout->setSingleShot(true);
    connect(m_stroketimeout, SIGNAL(timeout()), this, SLOT(timerDone()));
    m_keystroke = false;
}

void SSearch::takeFocus()
{
    LOGDEB2(("SSearch: take focus\n"));
    queryText->setFocus(Qt::ShortcutFocusReason);
    // If the focus was already in the search entry, the text is not selected.
    // Do it for consistency
    queryText->lineEdit()->selectAll();
}

void SSearch::timerDone()
{
    QString qs = queryText->currentText();
    LOGDEB1(("SSearch::timerDone: qs [%s]\n", qs2utf8s(qs).c_str()));
    searchTextChanged(qs);
}

void SSearch::searchTextChanged(const QString& text)
{
    QString qs = queryText->currentText();
    LOGDEB1(("SSearch::searchTextChanged. ks %d qs [%s]\n", 
	     m_keystroke, qs2utf8s(text).c_str()));
    if (text.isEmpty()) {
	searchPB->setEnabled(false);
	clearqPB->setEnabled(false);
	queryText->setFocus();
	emit clearSearch();
    } else {
	searchPB->setEnabled(true);
	clearqPB->setEnabled(true);
	if (m_keystroke) {
	    m_tstartqs = qs;
	}
	if (prefs.ssearchAsYouType && !m_disableAutosearch && 
	    !m_keystroke && m_tstartqs == qs) {
	    m_disableAutosearch = true;
	    string s;
	    int cs = partialWord(s);
	    LOGDEB1(("SSearch::searchTextChanged: autosearch. cs %d s [%s]\n", 
		     cs, s.c_str()));
	    if (cs < 0) {
		startSimpleSearch();
	    } else if (!m_stroketimeout->isActive() && s.size() >= 2) {
		s = qs2utf8s(queryText->currentText());
		s += "*";
		startSimpleSearch(s, 20);
	    }
	}
    }
    m_keystroke = false;
}

void SSearch::searchTypeChanged(int typ)
{
    LOGDEB(("Search type now %d\n", typ));
    // Adjust context help
    if (typ == SST_LANG)
	HelpClient::installMap((const char *)this->objectName().toUtf8(), 
			       "RCL.SEARCH.LANG");
    else 
	HelpClient::installMap((const char *)this->objectName().toUtf8(), 
			       "RCL.SEARCH.SIMPLE");

    // Also fix tooltips
    switch (typ) {
    case SST_LANG:
        queryText->setToolTip(tr(
"Enter query language expression. Cheat sheet:<br>\n"
"<i>term1 term2</i> : 'term1' and 'term2' in any field.<br>\n"
"<i>field:term1</i> : 'term1' in field 'field'.<br>\n"
" Standard field names/synonyms:<br>\n"
"  title/subject/caption, author/from, recipient/to, filename, ext.<br>\n"
" Pseudo-fields: dir, mime/format, type/rclcat, date.<br>\n"
" Two date interval exemples: 2009-03-01/2009-05-20  2009-03-01/P2M.<br>\n"
"<i>term1 term2 OR term3</i> : term1 AND (term2 OR term3).<br>\n"
"  No actual parentheses allowed.<br>\n"
"<i>\"term1 term2\"</i> : phrase (must occur exactly). Possible modifiers:<br>\n"
"<i>\"term1 term2\"p</i> : unordered proximity search with default distance.<br>\n"
"Use <b>Show Query</b> link when in doubt about result and see manual (&lt;F1>) for more detail.\n"
				  ));
        break;
    case SST_FNM:
        queryText->setToolTip(tr("Enter file name wildcard expression."));
        break;
    case SST_ANY:
    case SST_ALL:
    default:
        queryText->setToolTip(tr(
      "Enter search terms here. Type ESC SPC for completions of current term."
				  ));
    }
}

void SSearch::startSimpleSearch()
{
    QString qs = queryText->currentText();
    LOGDEB(("SSearch::startSimpleSearch(): qs [%s]\n", qs2utf8s(qs).c_str()));
    if (qs.length() == 0)
	return;

    string u8 = (const char *)queryText->currentText().toUtf8();

    trimstring(u8);
    if (u8.length() == 0)
	return;

    if (!startSimpleSearch(u8))
	return;

    LOGDEB(("startSimpleSearch: updating history\n"));
    // Search terms history:
    // We want to have the new text at the top and any older identical
    // entry to be erased. There is no standard qt policy to do this ? 
    // So do it by hand.
    QString txt = queryText->currentText();
    QString txtt = txt.trimmed();
    int index = queryText->findText(txtt);
    if (index > 0) {
	queryText->removeItem(index);
    }
    if (index != 0) {
	queryText->insertItem(0, txtt);
	queryText->setEditText(txt);
    }
    m_disableAutosearch = true;
    m_stroketimeout->stop();

    // Save the current state of the listbox list to the prefs (will
    // go to disk)
    prefs.ssearchHistory.clear();
    for (int index = 0; index < queryText->count(); index++) {
	prefs.ssearchHistory.push_back(queryText->itemText(index));
    }
}
void SSearch::setPrefs()
{
    if (prefs.ssearchNoComplete) {
	queryText->completer()->setModel(0);
    } else {
	queryText->completer()->setModel(m_savedModel);
    }
}

bool SSearch::startSimpleSearch(const string& u8, int maxexp)
{
    LOGDEB(("SSearch::startSimpleSearch(%s)\n", u8.c_str()));
    string stemlang = prefs.stemlang();

    SSearchType tp = (SSearchType)searchTypCMB->currentIndex();
    Rcl::SearchData *sdata = 0;

    if (tp == SST_LANG) {
	string reason;
        if (prefs.autoSuffsEnable)
            sdata = wasaStringToRcl(theconfig, stemlang, u8, reason, 
				    (const char *)prefs.autoSuffs.toUtf8());
        else
            sdata = wasaStringToRcl(theconfig, stemlang, u8, reason);
	if (sdata == 0) {
	    QMessageBox::warning(0, "Recoll", tr("Bad query string") + ": " +
				 QString::fromUtf8(reason.c_str()));
	    return false;
	}
    } else {
	sdata = new Rcl::SearchData(Rcl::SCLT_OR, stemlang);
	if (sdata == 0) {
	    QMessageBox::warning(0, "Recoll", tr("Out of memory"));
	    return false;
	}
	Rcl::SearchDataClause *clp = 0;
	if (tp == SST_FNM) {
	    clp = new Rcl::SearchDataClauseFilename(u8);
	} else {
	    // ANY or ALL, several words.
	    if (tp == SST_ANY) {
		clp = new Rcl::SearchDataClauseSimple(Rcl::SCLT_OR, u8);
	    } else {
		clp = new Rcl::SearchDataClauseSimple(Rcl::SCLT_AND, u8);
	    }
	}
	sdata->addClause(clp);
    }

    if (prefs.ssearchAutoPhrase && rcldb) {
	sdata->maybeAddAutoPhrase(*rcldb, 
				  prefs.ssearchAutoPhraseThreshPC / 100.0);
    }
    if (maxexp != -1) {
	sdata->setMaxExpand(maxexp);
    }
    RefCntr<Rcl::SearchData> rsdata(sdata);
    emit startSearch(rsdata, true);
    return true;
}

void SSearch::setSearchString(const QString& txt)
{
    m_disableAutosearch = true;
    m_stroketimeout->stop();
    queryText->setEditText(txt);
}

bool SSearch::hasSearchString()
{
    return !queryText->lineEdit()->text().isEmpty();
}

// Add term to simple search. Term comes out of double-click in
// reslist or preview. 
// It would probably be better to cleanup in preview.ui.h and
// reslist.cpp and do the proper html stuff in the latter case
// (which is different because it format is explicit richtext
// instead of auto as for preview, needed because it's built by
// fragments?).
static const char* punct = " \t()<>\"'[]{}!^*.,:;\n\r";
void SSearch::addTerm(QString term)
{
    LOGDEB(("SSearch::AddTerm: [%s]\n", (const char *)term.toUtf8()));
    string t = (const char *)term.toUtf8();
    string::size_type pos = t.find_last_not_of(punct);
    if (pos == string::npos)
	return;
    t = t.substr(0, pos+1);
    pos = t.find_first_not_of(punct);
    if (pos != string::npos)
	t = t.substr(pos);
    if (t.empty())
	return;
    term = QString::fromUtf8(t.c_str());

    QString text = queryText->currentText();
    text += QString::fromLatin1(" ") + term;
    queryText->setEditText(text);
    m_disableAutosearch = true;
    m_stroketimeout->stop();
}

void SSearch::onWordReplace(const QString& o, const QString& n)
{
    LOGDEB(("SSearch::onWordReplace: o [%s] n [%s]\n",
	    qs2utf8s(o).c_str(), qs2utf8s(n).c_str()));
    QString txt = queryText->currentText();
    QRegExp exp = QRegExp(QString("\\b") + o + QString("\\b"));
    exp.setCaseSensitivity(Qt::CaseInsensitive);
    txt.replace(exp, n);
    queryText->setEditText(txt);
    m_disableAutosearch = true;
    m_stroketimeout->stop();
    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers ();
    if (mods == Qt::NoModifier)
	startSimpleSearch();
}

void SSearch::setAnyTermMode()
{
    searchTypCMB->setCurrentIndex(SST_ANY);
}

// If text does not end with space, return last (partial) word and >0
// else return -1
int SSearch::partialWord(string& s)
{
    // Extract last word in text
    QString txt = queryText->currentText();
    int cs = txt.lastIndexOf(" ");
    if (cs == -1)
	cs = 0;
    else
	cs++;
    if (txt.size() == 0 || cs == txt.size()) {
	return -1;
    }
    s = qs2utf8s(txt.right(txt.size() - cs));
    return cs;
}

// Create completion list for term by adding a joker at the end and calling
// rcldb->termMatch().
int SSearch::completionList(string s, QStringList& lst, int max)
{
    if (!rcldb)
	return -1;
    if (s.empty())
	return 0;
   // Query database for completions
    s += "*";
    Rcl::TermMatchResult tmres;
    if (!rcldb->termMatch(Rcl::Db::ET_WILD, "", s, tmres, max) || 
	tmres.entries.size() == 0) {
	return 0;
    }
    for (vector<Rcl::TermMatchEntry>::iterator it = tmres.entries.begin(); 
	 it != tmres.entries.end(); it++) {
	lst.push_back(QString::fromUtf8(it->term.c_str()));
    }
    return lst.size();
}

// Complete last word in input by querying db for all possible terms.
void SSearch::completion()
{
    LOGDEB(("SSearch::completion\n"));

    m_disableAutosearch = true;
    m_stroketimeout->stop();

    if (!rcldb)
	return;
    if (searchTypCMB->currentIndex() == SST_FNM) {
	// Filename: no completion
	QApplication::beep();
	return;
    }

    // Extract last word in text
    string s;
    int cs = partialWord(s);
    if (cs < 0) {
	QApplication::beep();
	return;
    }

    // Query database for completions
    QStringList lst;
    const int maxdpy = 80;
    const int maxwalked = 10000;
    if (completionList(s, lst, maxwalked) <= 0) {
	QApplication::beep();
	return;
    }
    if (lst.size() >= maxdpy) {
	LOGDEB0(("SSearch::completion(): truncating list\n"));
	lst = lst.mid(0, maxdpy);
	lst.append("[...]");
    }

    // If list from db is single word, insert it, else popup the listview
    if (lst.size() == 1) {
	QString txt = queryText->currentText();
	txt.truncate(cs);
	txt.append(lst[0]);
	queryText->setEditText(txt);
    } else {
	m_savedEditText = queryText->currentText();
	m_displayingCompletions = true;
	m_chosenCompletion.clear();
	m_completedWordStart = cs;

	queryText->clear();
	queryText->addItems(lst);
	queryText->showPopup();

	connect(queryText, SIGNAL(activated(const QString&)), this,
		SLOT(completionTermChosen(const QString&)));
    }
}

void SSearch::completionTermChosen(const QString& text)
{
    if (text != "[...]")
	m_chosenCompletion = text;
    else 
	m_chosenCompletion.clear();
}

void SSearch::wrapupCompletion()
{
    LOGDEB(("SSearch::wrapupCompletion\n"));

    queryText->clear();
    queryText->addItems(prefs.ssearchHistory);
    if (!m_chosenCompletion.isEmpty()) {
	m_savedEditText.truncate(m_completedWordStart);
	m_savedEditText.append(m_chosenCompletion);
    }
    queryText->setEditText(m_savedEditText);
    m_disableAutosearch = true;
    m_savedEditText.clear();
    m_chosenCompletion.clear();
    m_displayingCompletions = false;
    disconnect(queryText, SIGNAL(activated(const QString&)), this,
	       SLOT(completionTermChosen(const QString&)));
}

#undef SHOWEVENTS
#if defined(SHOWEVENTS)
const char *eventTypeToStr(int tp)
{
    switch (tp) {
    case  0: return "None";
    case  1: return "Timer";
    case  2: return "MouseButtonPress";
    case  3: return "MouseButtonRelease";
    case  4: return "MouseButtonDblClick";
    case  5: return "MouseMove";
    case  6: return "KeyPress";
    case  7: return "KeyRelease";
    case  8: return "FocusIn";
    case  9: return "FocusOut";
    case  10: return "Enter";
    case  11: return "Leave";
    case  12: return "Paint";
    case  13: return "Move";
    case  14: return "Resize";
    case  15: return "Create";
    case  16: return "Destroy";
    case  17: return "Show";
    case  18: return "Hide";
    case  19: return "Close";
    case  20: return "Quit";
    case  21: return "ParentChange";
    case  131: return "ParentAboutToChange";
    case  22: return "ThreadChange";
    case  24: return "WindowActivate";
    case  25: return "WindowDeactivate";
    case  26: return "ShowToParent";
    case  27: return "HideToParent";
    case  31: return "Wheel";
    case  33: return "WindowTitleChange";
    case  34: return "WindowIconChange";
    case  35: return "ApplicationWindowIconChange";
    case  36: return "ApplicationFontChange";
    case  37: return "ApplicationLayoutDirectionChange";
    case  38: return "ApplicationPaletteChange";
    case  39: return "PaletteChange";
    case  40: return "Clipboard";
    case  42: return "Speech";
    case   43: return "MetaCall";
    case  50: return "SockAct";
    case  132: return "WinEventAct";
    case  52: return "DeferredDelete";
    case  60: return "DragEnter";
    case  61: return "DragMove";
    case  62: return "DragLeave";
    case  63: return "Drop";
    case  64: return "DragResponse";
    case  68: return "ChildAdded";
    case  69: return "ChildPolished";
    case  70: return "ChildInserted";
    case  72: return "LayoutHint";
    case  71: return "ChildRemoved";
    case  73: return "ShowWindowRequest";
    case  74: return "PolishRequest";
    case  75: return "Polish";
    case  76: return "LayoutRequest";
    case  77: return "UpdateRequest";
    case  78: return "UpdateLater";
    case  79: return "EmbeddingControl";
    case  80: return "ActivateControl";
    case  81: return "DeactivateControl";
    case  82: return "ContextMenu";
    case  83: return "InputMethod";
    case  86: return "AccessibilityPrepare";
    case  87: return "TabletMove";
    case  88: return "LocaleChange";
    case  89: return "LanguageChange";
    case  90: return "LayoutDirectionChange";
    case  91: return "Style";
    case  92: return "TabletPress";
    case  93: return "TabletRelease";
    case  94: return "OkRequest";
    case  95: return "HelpRequest";
    case  96: return "IconDrag";
    case  97: return "FontChange";
    case  98: return "EnabledChange";
    case  99: return "ActivationChange";
    case  100: return "StyleChange";
    case  101: return "IconTextChange";
    case  102: return "ModifiedChange";
    case  109: return "MouseTrackingChange";
    case  103: return "WindowBlocked";
    case  104: return "WindowUnblocked";
    case  105: return "WindowStateChange";
    case  110: return "ToolTip";
    case  111: return "WhatsThis";
    case  112: return "StatusTip";
    case  113: return "ActionChanged";
    case  114: return "ActionAdded";
    case  115: return "ActionRemoved";
    case  116: return "FileOpen";
    case  117: return "Shortcut";
    case  51: return "ShortcutOverride";
    case  30: return "Accel";
    case  32: return "AccelAvailable";
    case  118: return "WhatsThisClicked";
    case  120: return "ToolBarChange";
    case  121: return "ApplicationActivated";
    case  122: return "ApplicationDeactivated";
    case  123: return "QueryWhatsThis";
    case  124: return "EnterWhatsThisMode";
    case  125: return "LeaveWhatsThisMode";
    case  126: return "ZOrderChange";
    case  127: return "HoverEnter";
    case  128: return "HoverLeave";
    case  129: return "HoverMove";
    case  119: return "AccessibilityHelp";
    case  130: return "AccessibilityDescription";
    case  150: return "EnterEditFocus";
    case  151: return "LeaveEditFocus";
    case  152: return "AcceptDropsChange";
    case  153: return "MenubarUpdated";
    case  154: return "ZeroTimerEvent";
    case  155: return "GraphicsSceneMouseMove";
    case  156: return "GraphicsSceneMousePress";
    case  157: return "GraphicsSceneMouseRelease";
    case  158: return "GraphicsSceneMouseDoubleClick";
    case  159: return "GraphicsSceneContextMenu";
    case  160: return "GraphicsSceneHoverEnter";
    case  161: return "GraphicsSceneHoverMove";
    case  162: return "GraphicsSceneHoverLeave";
    case  163: return "GraphicsSceneHelp";
    case  164: return "GraphicsSceneDragEnter";
    case  165: return "GraphicsSceneDragMove";
    case  166: return "GraphicsSceneDragLeave";
    case  167: return "GraphicsSceneDrop";
    case  168: return "GraphicsSceneWheel";
    case  169: return "KeyboardLayoutChange";
    case  170: return "DynamicPropertyChange";
    case  171: return "TabletEnterProximity";
    case  172: return "TabletLeaveProximity";
    default: return "UnknownEvent";
    }
}
#endif

bool SSearch::eventFilter(QObject *target, QEvent *event)
{
#if defined(SHOWEVENTS)
    if (event->type() == QEvent::Timer || 
	event->type() == QEvent::UpdateRequest ||
	event->type() == QEvent::Paint)
	return false;
    LOGDEB2(("SSearch::eventFilter: target %p (%p) type %s\n", 
	     target, queryText->lineEdit(),
	     eventTypeToStr(event->type())));
#endif

    if (target == queryText->view()) {
	if (event->type() == QEvent::Hide) {
	    // List was closed. If we were displaying completions, need
	    // to reset state.
	    if (m_displayingCompletions) {
		QTimer::singleShot(0, this, SLOT(wrapupCompletion()));
	    }
	}
	return false;
    }

    if (event->type() == QEvent::KeyPress)  {
	QKeyEvent *ke = (QKeyEvent *)event;
	LOGDEB1(("SSearch::eventFilter: keyPress (m_escape %d) key %d\n", 
		 m_escape, ke->key()));
	if (ke->key() == Qt::Key_Escape) {
	    LOGDEB(("Escape\n"));
	    m_escape = true;
	    m_disableAutosearch = true;
	    m_stroketimeout->stop();
	    return true;
	} else if (m_escape && ke->key() == Qt::Key_Space) {
	    LOGDEB(("Escape space\n"));
	    ke->accept();
	    completion();
	    m_escape = false;
	    m_disableAutosearch = true;
	    m_stroketimeout->stop();
	    return true;
	} else if (ke->key() == Qt::Key_Space) {
	    if (prefs.ssearchOnWS)
		startSimpleSearch();
	} else {
	    m_escape = false;
	    m_keystroke = true;
	    if (prefs.ssearchAsYouType) {
		m_disableAutosearch = false;
		QString qs = queryText->currentText();
		LOGDEB0(("SSearch::eventFilter: start timer, qs [%s]\n", 
			 qs2utf8s(qs).c_str()));
		m_stroketimeout->start(strokeTimeoutMS);
	    }
	}
    }
    return false;
}
