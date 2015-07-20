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
#ifndef _SSEARCH_W_H_INCLUDED_
#define _SSEARCH_W_H_INCLUDED_

#include <QVariant>
#include <QWidget>

class QTimer;

#include "recoll.h"
#include "searchdata.h"
#include "refcntr.h"

#include "ui_ssearchb.h"

class SSearch : public QWidget, public Ui::SSearchBase
{
    Q_OBJECT

public:
    enum SSearchType {SST_ANY = 0, SST_ALL = 1, SST_FNM = 2, SST_LANG = 3};

    SSearch(QWidget* parent = 0, const char * = 0)
	: QWidget(parent) 
    {
	setupUi(this);
	init();
    }

    virtual void init();
    virtual void setAnyTermMode();
    virtual void completion();
    virtual bool eventFilter(QObject *target, QEvent *event);
    virtual bool hasSearchString();
    virtual void setPrefs();				  
public slots:
    virtual void searchTextChanged(const QString & text);
    virtual void searchTypeChanged(int);
    virtual void setSearchString(const QString& text);
    virtual void startSimpleSearch();
    virtual void addTerm(QString);
    virtual void onWordReplace(const QString&, const QString&);
    virtual void completionTermChosen(const QString& text);
    virtual void wrapupCompletion();
    virtual void timerDone();
    virtual void takeFocus();

signals:
    void startSearch(RefCntr<Rcl::SearchData>, bool);
    void clearSearch();
 private:
    bool m_escape;

    bool m_displayingCompletions;
    QString m_chosenCompletion;
    QString m_savedEditText;
    unsigned int m_completedWordStart;

    bool m_disableAutosearch;
    QTimer *m_stroketimeout;
    bool m_keystroke;
    QString m_tstartqs;
    QAbstractItemModel *m_savedModel;

    int partialWord(string& s);
    int completionList(string s, QStringList& lst, int max = 100);
    bool startSimpleSearch(const string& q, int maxexp = -1);
};


#endif /* _SSEARCH_W_H_INCLUDED_ */
