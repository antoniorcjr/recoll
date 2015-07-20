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

#include <stdio.h>

#include <vector>
#include <utility>
#include <string>

using namespace std;

#include <qpushbutton.h>
#include <qtimer.h>

#include <qlistwidget.h>

#include <qmessagebox.h>
#include <qinputdialog.h>
#include <qlayout.h>

#include "recoll.h"
#include "debuglog.h"
#include "guiutils.h"

#include "viewaction_w.h"

void ViewAction::init()
{
    selSamePB->setEnabled(false);
    connect(closePB, SIGNAL(clicked()), this, SLOT(close()));
    connect(chgActPB, SIGNAL(clicked()), this, SLOT(editActions()));
    connect(actionsLV,SIGNAL(itemDoubleClicked(QTableWidgetItem *)),
	    this, SLOT(onItemDoubleClicked(QTableWidgetItem *)));
    connect(actionsLV,SIGNAL(itemClicked(QTableWidgetItem *)),
	    this, SLOT(onItemClicked(QTableWidgetItem *)));
    useDesktopCB->setChecked(prefs.useDesktopOpen);
    onUseDesktopCBToggled(prefs.useDesktopOpen);
    connect(useDesktopCB, SIGNAL(stateChanged(int)), 
	    this, SLOT(onUseDesktopCBToggled(int)));
    connect(setExceptCB, SIGNAL(stateChanged(int)), 
	    this, SLOT(onSetExceptCBToggled(int)));
    connect(selSamePB, SIGNAL(clicked()),
	    this, SLOT(onSelSameClicked()));
    resize(QSize(640, 480).expandedTo(minimumSizeHint()));
}
	
void ViewAction::onUseDesktopCBToggled(int onoff)
{
    prefs.useDesktopOpen = onoff != 0;
    fillLists();
    setExceptCB->setEnabled(prefs.useDesktopOpen);
}

void ViewAction::onSetExceptCBToggled(int onoff)
{
    newActionLE->setEnabled(onoff != 0);
}

void ViewAction::fillLists()
{
    currentLBL->clear();
    actionsLV->clear();
    actionsLV->verticalHeader()->setDefaultSectionSize(20); 
    vector<pair<string, string> > defs;
    theconfig->getMimeViewerDefs(defs);
    actionsLV->setRowCount(defs.size());
    int row = 0;

    set<string> viewerXs;
    if (prefs.useDesktopOpen) {
	string s = theconfig->getMimeViewerAllEx();
	stringToStrings(s, viewerXs);
    }
    for (vector<pair<string, string> >::const_iterator it = defs.begin();
	 it != defs.end(); it++) {
	actionsLV->setItem(row, 0, 
	   new QTableWidgetItem(QString::fromUtf8(it->first.c_str())));
	if (!prefs.useDesktopOpen ||
	    viewerXs.find(it->first) != viewerXs.end()) {
	    actionsLV->setItem(
		row, 1, 
		new QTableWidgetItem(QString::fromUtf8(it->second.c_str())));
	} else {
	    actionsLV->setItem(
		row, 1, new QTableWidgetItem(tr("Desktop Default")));
	}
	row++;
    }
    QStringList labels(tr("MIME type"));
    labels.push_back(tr("Command"));
    actionsLV->setHorizontalHeaderLabels(labels);
}

void ViewAction::selectMT(const QString& mt)
{
    actionsLV->clearSelection();
    QList<QTableWidgetItem *>items = 
	actionsLV->findItems(mt, Qt::MatchFixedString|Qt::MatchCaseSensitive);
    for (QList<QTableWidgetItem *>::iterator it = items.begin();
	 it != items.end(); it++) {
	(*it)->setSelected(true);
	actionsLV->setCurrentItem(*it, QItemSelectionModel::Columns);
    }
}

void ViewAction::onSelSameClicked()
{
    actionsLV->clearSelection();
    QString value = currentLBL->text();
    if (value.isEmpty())
	return;
    string action = qs2utf8s(value);
    fprintf(stderr, "value: %s\n", action.c_str());
    vector<pair<string, string> > defs;
    theconfig->getMimeViewerDefs(defs);
    for (unsigned int i = 0; i < defs.size(); i++) {
	if (defs[i].second == action) {
	    QList<QTableWidgetItem *>items = 
		actionsLV->findItems(QString::fromUtf8(defs[i].first.c_str()), 
				  Qt::MatchFixedString|Qt::MatchCaseSensitive);
	    for (QList<QTableWidgetItem *>::iterator it = items.begin();
		 it != items.end(); it++) {
		(*it)->setSelected(true);
		QTableWidgetItem *item1 = actionsLV->item((*it)->row(), 1);
		item1->setSelected(true);
	    }
	}
    }
}

// Fill the input fields with the row's values when the user clicks
void ViewAction::onItemClicked(QTableWidgetItem * item)
{
    QTableWidgetItem *item0 = actionsLV->item(item->row(), 0);
    string mtype = (const char *)item0->text().toLocal8Bit();

    vector<pair<string, string> > defs;
    theconfig->getMimeViewerDefs(defs);
    for (unsigned int i = 0; i < defs.size(); i++) {
	if (defs[i].first == mtype) {
	    currentLBL->setText(QString::fromUtf8(defs[i].second.c_str()));
	    selSamePB->setEnabled(true);
	    return;
	}
    }
    currentLBL->clear();
    selSamePB->setEnabled(false);
}

void ViewAction::onItemDoubleClicked(QTableWidgetItem * item)
{
    actionsLV->clearSelection();
    item->setSelected(true);
    QTableWidgetItem *item0 = actionsLV->item(item->row(), 0);
    item0->setSelected(true);
    editActions();
}

void ViewAction::editActions()
{
    QString action0;
    int except0 = -1;

    set<string> viewerXs;
    string s = theconfig->getMimeViewerAllEx();
    stringToStrings(s, viewerXs);

    list<string> mtypes;
    bool dowarnmultiple = true;
    for (int row = 0; row < actionsLV->rowCount(); row++) {
	QTableWidgetItem *item0 = actionsLV->item(row, 0);
	if (!item0->isSelected())
	    continue;
	string mtype = (const char *)item0->text().toLocal8Bit();
	mtypes.push_back(mtype);
	QTableWidgetItem *item1 = actionsLV->item(row, 1);
	QString action = item1->text();
	int except = viewerXs.find(mtype) != viewerXs.end();
	if (action0.isEmpty()) {
	    action0 = action;
	    except0 = except;
	} else {
	    if ((action != action0 || except != except0) && dowarnmultiple) {
		switch (QMessageBox::warning(0, "Recoll",
					     tr("Changing entries with "
						"different current values"),
					     "Continue",
					     "Cancel",
					     0, 0, 1)) {
		case 0: dowarnmultiple = false;break;
		case 1: return;
		}
	    }
	}
    }
    if (action0.isEmpty())
	return;

    string sact = (const char *)newActionLE->text().toLocal8Bit();
    trimstring(sact);
    for (list<string>::const_iterator mit = mtypes.begin(); 
	 mit != mtypes.end(); mit++) {
	set<string>::iterator xit = viewerXs.find(*mit);
	if (setExceptCB->isChecked()) {
	    if (xit == viewerXs.end()) {
		viewerXs.insert(*mit);
	    }
	} else {
	    if (xit != viewerXs.end()) {
		viewerXs.erase(xit);
	    }
	}
	// An empty action will restore the default (erase from
	// topmost conftree)
	theconfig->setMimeViewerDef(*mit, sact);
    }

    s = stringsToString(viewerXs);
    theconfig->setMimeViewerAllEx(s);
    fillLists();
}
