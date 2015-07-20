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

#include <stdio.h>
#include <unistd.h>

#include <algorithm>
#include <list>
#include <map>
#include <string>
using std::list;
using std::multimap;
using std::string;

#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qcombobox.h>
#include <QTableWidget>
#include <QHeaderView>
#include <QClipboard>
#include <QKeyEvent>

#include "debuglog.h"
#include "recoll.h"
#include "spell_w.h"
#include "guiutils.h"
#include "rcldb.h"
#include "searchdata.h"
#include "rclquery.h"
#include "rclhelp.h"
#include "wasatorcl.h"
#include "execmd.h"

#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif

void SpellW::init()
{
    m_c2t.clear();
    expTypeCMB->addItem(tr("Wildcards"));
    m_c2t.push_back(TYPECMB_WILD);
    expTypeCMB->addItem(tr("Regexp"));
    m_c2t.push_back(TYPECMB_REG);
    expTypeCMB->addItem(tr("Stem expansion"));
    m_c2t.push_back(TYPECMB_STEM);
#ifdef RCL_USE_ASPELL
    bool noaspell = false;
    theconfig->getConfParam("noaspell", &noaspell);
    if (!noaspell) {
	expTypeCMB->addItem(tr("Spelling/Phonetic"));
	m_c2t.push_back(TYPECMB_ASPELL);
    }
#endif
    expTypeCMB->addItem(tr("Show index statistics"));
    m_c2t.push_back(TYPECMB_STATS);

    int typ = prefs.termMatchType;
    vector<comboboxchoice>::const_iterator it = 
	std::find(m_c2t.begin(), m_c2t.end(), typ);
    if (it == m_c2t.end())
	it = m_c2t.begin();
    int cmbidx = it - m_c2t.begin();

    expTypeCMB->setCurrentIndex(cmbidx);

    // Stemming language combobox
    stemLangCMB->clear();
    vector<string> langs;
    if (!getStemLangs(langs)) {
	QMessageBox::warning(0, "Recoll", 
			     tr("error retrieving stemming languages"));
    }
    for (vector<string>::const_iterator it = langs.begin(); 
	 it != langs.end(); it++) {
	stemLangCMB->
	    addItem(QString::fromUtf8(it->c_str(), it->length()));
    }

    (void)new HelpClient(this);
    HelpClient::installMap((const char *)this->objectName().toUtf8(), 
			   "RCL.SEARCH.TERMEXPLORER");

    // signals and slots connections
    connect(baseWordLE, SIGNAL(textChanged(const QString&)), 
	    this, SLOT(wordChanged(const QString&)));
    connect(baseWordLE, SIGNAL(returnPressed()), this, SLOT(doExpand()));
    connect(expandPB, SIGNAL(clicked()), this, SLOT(doExpand()));
    connect(dismissPB, SIGNAL(clicked()), this, SLOT(close()));
    connect(expTypeCMB, SIGNAL(activated(int)), this, SLOT(modeSet(int)));

    resTW->setShowGrid(0);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    resTW->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
#else
    resTW->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
#endif
    resTW->verticalHeader()->setDefaultSectionSize(20); 
    connect(resTW,
	   SIGNAL(cellDoubleClicked(int, int)),
            this, SLOT(textDoubleClicked(int, int)));

    resTW->setColumnWidth(0, 200);
    resTW->setColumnWidth(1, 150);
    resTW->installEventFilter(this);

    if (o_index_stripchars) {
	caseSensCB->setEnabled(false);
	caseSensCB->setEnabled(false);
    }
    modeSet(cmbidx);
}

static const int maxexpand = 10000;

/* Expand term according to current mode */
void SpellW::doExpand()
{
    int idx = expTypeCMB->currentIndex();
    if (idx < 0 || idx >= int(m_c2t.size()))
	idx = 0;
    comboboxchoice mode = m_c2t[idx];

    // Can't clear qt4 table widget: resets column headers too
    resTW->setRowCount(0);
    if (baseWordLE->text().isEmpty() && mode != TYPECMB_STATS) 
	return;

    string reason;
    if (!maybeOpenDb(reason)) {
	QMessageBox::critical(0, "Recoll", QString(reason.c_str()));
	LOGDEB(("SpellW::doExpand: db error: %s\n", reason.c_str()));
	return;
    }

    int mt;
    switch(mode) {
    case TYPECMB_WILD: mt = Rcl::Db::ET_WILD; break;
    case TYPECMB_REG: mt = Rcl::Db::ET_REGEXP; break;
    case TYPECMB_STEM: mt = Rcl::Db::ET_STEM; break;
    default: mt = Rcl::Db::ET_WILD;
    }
    if (caseSensCB->isChecked()) {
	mt |= Rcl::Db::ET_CASESENS;
    }
    if (diacSensCB->isChecked()) {
	mt |= Rcl::Db::ET_DIACSENS;
    }
    Rcl::TermMatchResult res;
    string expr = string((const char *)baseWordLE->text().toUtf8());
    Rcl::DbStats dbs;
    rcldb->dbStats(dbs);

    switch (mode) {
    case TYPECMB_WILD: 
    default:
    case TYPECMB_REG:
    case TYPECMB_STEM:
    {
	string l_stemlang = qs2utf8s(stemLangCMB->currentText());

	if (!rcldb->termMatch(mt, l_stemlang, expr, res, maxexpand)) {
	    LOGERR(("SpellW::doExpand:rcldb::termMatch failed\n"));
	    return;
	}
        statsLBL->setText(tr("Index: %1 documents, average length %2 terms."
			     "%3 results")
                          .arg(dbs.dbdoccount).arg(dbs.dbavgdoclen, 0, 'f', 1)
			  .arg(res.entries.size()));
    }
        
    break;

#ifdef RCL_USE_ASPELL
    case TYPECMB_ASPELL: 
    {
	LOGDEB(("SpellW::doExpand: aspelling\n"));
	if (!aspell) {
	    QMessageBox::warning(0, "Recoll",
				 tr("Aspell init failed. "
				    "Aspell not installed?"));
	    LOGDEB(("SpellW::doExpand: aspell init error\n"));
	    return;
	}
	list<string> suggs;
	if (!aspell->suggest(*rcldb, expr, suggs, reason)) {
	    QMessageBox::warning(0, "Recoll",
				 tr("Aspell expansion error. "));
	    LOGERR(("SpellW::doExpand:suggest failed: %s\n", reason.c_str()));
	}
	for (list<string>::const_iterator it = suggs.begin(); 
	     it != suggs.end(); it++) 
	    res.entries.push_back(Rcl::TermMatchEntry(*it));
#ifdef TESTING_XAPIAN_SPELL
	string rclsugg = rcldb->getSpellingSuggestion(expr);
	if (!rclsugg.empty()) {
	    res.entries.push_back(Rcl::TermMatchEntry("Xapian spelling:"));
	    res.entries.push_back(Rcl::TermMatchEntry(rclsugg));
	}
#endif // TESTING_XAPIAN_SPELL
        statsLBL->setText(tr("%1 results").arg(res.entries.size()));
    }
    break;
#endif // RCL_USE_ASPELL

    case TYPECMB_STATS: 
    {
	showStats();
	return;
    }
    break;
    }


    if (res.entries.empty()) {
        resTW->setItem(0, 0, new QTableWidgetItem(tr("No expansion found")));
    } else {
        int row = 0;

	if (maxexpand > 0 && int(res.entries.size()) >= maxexpand) {
	    resTW->setRowCount(row + 1);
	    resTW->setSpan(row, 0, 1, 2);
	    resTW->setItem(row++, 0, 
			   new QTableWidgetItem(
			       tr("List was truncated alphabetically, "
				  "some frequent "))); 
	    resTW->setRowCount(row + 1);
	    resTW->setSpan(row, 0, 1, 2);
	    resTW->setItem(row++, 0, new QTableWidgetItem(
			       tr("terms may be missing. "
				  "Try using a longer root.")));
	    resTW->setRowCount(row + 1);
	    resTW->setItem(row++, 0, new QTableWidgetItem(""));
	}

	for (vector<Rcl::TermMatchEntry>::iterator it = res.entries.begin(); 
	     it != res.entries.end(); it++) {
	    LOGDEB2(("SpellW::expand: %6d [%s]\n", it->wcf, it->term.c_str()));
	    char num[30];
	    if (it->wcf)
		sprintf(num, "%d / %d",  it->docs, it->wcf);
	    else
		num[0] = 0;
	    resTW->setRowCount(row+1);
            resTW->setItem(row, 0, 
                    new QTableWidgetItem(QString::fromUtf8(it->term.c_str()))); 
            resTW->setItem(row++, 1, 
                             new QTableWidgetItem(QString::fromUtf8(num)));
	}
    }
}

void SpellW::showStats()
{
    statsLBL->setText("");
    int row = 0;

    Rcl::DbStats res;
    if (!rcldb->dbStats(res)) {
	LOGERR(("SpellW::doExpand:rcldb::dbStats failed\n"));
	return;
    }

    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("Number of documents")));
    resTW->setItem(row++, 1, new QTableWidgetItem(
		       QString::number(res.dbdoccount)));

    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("Average terms per document")));
    resTW->setItem(row++, 1, new QTableWidgetItem(
		       QString::number(res.dbavgdoclen)));

    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("Smallest document length")));
    resTW->setItem(row++, 1, new QTableWidgetItem(
		       QString::number(res.mindoclen)));

    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("Longest document length")));
    resTW->setItem(row++, 1, new QTableWidgetItem(
		       QString::number(res.maxdoclen)));

    if (!theconfig)
	return;

    ExecCmd cmd;
    vector<string> args; 
    int status;
    args.push_back("-sk");
    args.push_back(theconfig->getDbDir());
    string output;
    status = cmd.doexec("du", args, 0, &output);
    int dbkbytes = 0;
    if (!status) {
	dbkbytes = atoi(output.c_str());
    }
    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("Database directory size")));
    resTW->setItem(row++, 1, new QTableWidgetItem(
		       QString::fromUtf8(
			   displayableBytes(dbkbytes*1024).c_str())));

    vector<string> allmimetypes = theconfig->getAllMimeTypes();
    multimap<int, string> mtbycnt;
    for (vector<string>::const_iterator it = allmimetypes.begin();
	 it != allmimetypes.end(); it++) {
	string reason;
	string q = string("mime:") + *it;
	Rcl::SearchData *sd = wasaStringToRcl(theconfig, "", q, reason);
	RefCntr<Rcl::SearchData> rq(sd);
	Rcl::Query query(rcldb);
	if (!query.setQuery(rq)) {
	    LOGERR(("Query setup failed: %s",query.getReason().c_str()));
	    return;
	}
	int cnt = query.getResCnt();
	mtbycnt.insert(pair<int,string>(cnt,*it));
    }
    resTW->setRowCount(row+1);
    resTW->setItem(row, 0, new QTableWidgetItem(tr("MIME types:")));
    resTW->setItem(row++, 1, new QTableWidgetItem(""));

    for (multimap<int, string>::const_reverse_iterator it = mtbycnt.rbegin();
	 it != mtbycnt.rend(); it++) {
	resTW->setRowCount(row+1);
	resTW->setItem(row, 0, new QTableWidgetItem(
			   QString::fromUtf8(it->second.c_str())));
	resTW->setItem(row++, 1, new QTableWidgetItem(
			   QString::number(it->first)));
    }
}

void SpellW::wordChanged(const QString &text)
{
    if (text.isEmpty()) {
	expandPB->setEnabled(false);
        resTW->setRowCount(0);
    } else {
	expandPB->setEnabled(true);
    }
}

void SpellW::textDoubleClicked() {}
void SpellW::textDoubleClicked(int row, int)
{
    QTableWidgetItem *item = resTW->item(row, 0);
    if (item)
        emit(wordSelect(item->text()));
}

void SpellW::modeSet(int idx)
{
    if (idx < 0 || idx > int(m_c2t.size()))
	return;
    comboboxchoice mode = m_c2t[idx]; 
    resTW->setRowCount(0);
   
    if (mode == TYPECMB_STEM) {
	stemLangCMB->setEnabled(true);
	diacSensCB->setChecked(false);
	diacSensCB->setEnabled(false);
	caseSensCB->setChecked(false);
	caseSensCB->setEnabled(false);
    } else {
	stemLangCMB->setEnabled(false);
	diacSensCB->setEnabled(true);
	caseSensCB->setEnabled(true);
    }
    if (mode == TYPECMB_STATS)
	baseWordLE->setEnabled(false);
    else
	baseWordLE->setEnabled(true);


    if (mode == TYPECMB_STATS) {
	QStringList labels(tr("Item"));
	labels.push_back(tr("Value"));
	resTW->setHorizontalHeaderLabels(labels);
	doExpand();
    } else {
	QStringList labels(tr("Term"));
	labels.push_back(tr("Doc. / Tot."));
	resTW->setHorizontalHeaderLabels(labels);
	prefs.termMatchType = mode;
    }
}

void SpellW::copy()
{
  QItemSelectionModel * selection = resTW->selectionModel();
  QModelIndexList indexes = selection->selectedIndexes();

  if(indexes.size() < 1)
    return;

  // QModelIndex::operator < sorts first by row, then by column. 
  // this is what we need
  std::sort(indexes.begin(), indexes.end());

  // You need a pair of indexes to find the row changes
  QModelIndex previous = indexes.first();
  indexes.removeFirst();
  QString selected_text;
  QModelIndex current;
  Q_FOREACH(current, indexes)
  {
    QVariant data = resTW->model()->data(previous);
    QString text = data.toString();
    // At this point `text` contains the text in one cell
    selected_text.append(text);
    // If you are at the start of the row the row number of the previous index
    // isn't the same.  Text is followed by a row separator, which is a newline.
    if (current.row() != previous.row())
    {
      selected_text.append(QLatin1Char('\n'));
    }
    // Otherwise it's the same row, so append a column separator, which is a tab.
    else
    {
      selected_text.append(QLatin1Char('\t'));
    }
    previous = current;
  }

  // add last element
  selected_text.append(resTW->model()->data(current).toString());
  selected_text.append(QLatin1Char('\n'));
  qApp->clipboard()->setText(selected_text, QClipboard::Selection);
  qApp->clipboard()->setText(selected_text, QClipboard::Clipboard);
}


bool SpellW::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() != QEvent::KeyPress ||
	(target != resTW && target != resTW->viewport())) 
	return false;

    QKeyEvent *keyEvent = (QKeyEvent *)event;
    if(keyEvent->matches(QKeySequence::Copy) )
    {
	copy();
	return true;
    }
    return false;
}
