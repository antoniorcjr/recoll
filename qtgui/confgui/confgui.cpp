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

#include <stdio.h>
#include <stdlib.h>

#include <qglobal.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QListWidget>

#include <qobject.h>
#include <qlayout.h>
#include <qsize.h>
#include <qsizepolicy.h>
#include <qlabel.h>
#include <qspinbox.h>
#include <qtooltip.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qinputdialog.h>
#include <qpushbutton.h>
#include <qstringlist.h>
#include <qcombobox.h>

#include "confgui.h"
#include "smallut.h"
#include "debuglog.h"
#include "rcldb.h"
#include "guiutils.h"

#include <list>
#include <vector>
using std::list;
using std::vector;

namespace confgui {

static const int spacing = 2;
static const int margin = 2;

void ConfParamW::setValue(const QString& value)
{
    if (m_fsencoding)
        m_cflink->set(string((const char *)value.toLocal8Bit()));
    else
        m_cflink->set(string((const char *)value.toUtf8()));
}

void ConfParamW::setValue(int value)
{
    char buf[30];
    sprintf(buf, "%d", value);
    m_cflink->set(string(buf));
}
void ConfParamW::setValue(bool value)
{
    char buf[30];
    sprintf(buf, "%d", value);
    m_cflink->set(string(buf));
}

void setSzPol(QWidget *w, QSizePolicy::Policy hpol, 
		   QSizePolicy::Policy vpol,
		   int hstretch, int vstretch)
{
    QSizePolicy policy(hpol, vpol);
    policy.setHorizontalStretch(hstretch);
    policy.setVerticalStretch(vstretch);
    policy.setHeightForWidth(w->sizePolicy().hasHeightForWidth());
    w->setSizePolicy(policy);
}

bool ConfParamW::createCommon(const QString& lbltxt, const QString& tltptxt)
{
    m_hl = new QHBoxLayout(this);
    m_hl->setSpacing(spacing);

    QLabel *tl = new QLabel(this);
    setSzPol(tl, QSizePolicy::Preferred, QSizePolicy::Fixed, 0, 0);
    tl->setText(lbltxt);
    tl->setToolTip(tltptxt);

    m_hl->addWidget(tl);

    return true;
}

ConfParamIntW::ConfParamIntW(QWidget *parent, ConfLink cflink, 
			     const QString& lbltxt,
			     const QString& tltptxt,
			     int minvalue, 
			     int maxvalue,
                             int defaultvalue)
    : ConfParamW(parent, cflink), m_defaultvalue(defaultvalue)
{
    if (!createCommon(lbltxt, tltptxt))
	return;

    m_sb = new QSpinBox(this);
    m_sb->setMinimum(minvalue);
    m_sb->setMaximum(maxvalue);
    setSzPol(m_sb, QSizePolicy::Fixed, QSizePolicy::Fixed, 0, 0);
    m_hl->addWidget(m_sb);

    QFrame *fr = new QFrame(this);
    setSzPol(fr, QSizePolicy::Preferred, QSizePolicy::Fixed, 0, 0);
    m_hl->addWidget(fr);

    loadValue();
    QObject::connect(m_sb, SIGNAL(valueChanged(int)), 
		     this, SLOT(setValue(int)));
}

void ConfParamIntW::loadValue()
{
    string s;
    if (m_cflink->get(s)) 
        m_sb->setValue(atoi(s.c_str()));
    else
        m_sb->setValue(m_defaultvalue);
}

ConfParamStrW::ConfParamStrW(QWidget *parent, ConfLink cflink, 
			     const QString& lbltxt,
			     const QString& tltptxt)
    : ConfParamW(parent, cflink)
{
    if (!createCommon(lbltxt, tltptxt))
	return;

    m_le = new QLineEdit(this);
    setSzPol(m_le, QSizePolicy::Preferred, QSizePolicy::Fixed, 1, 0);

    m_hl->addWidget(m_le);

    loadValue();
    QObject::connect(m_le, SIGNAL(textChanged(const QString&)), 
		     this, SLOT(setValue(const QString&)));
}

void ConfParamStrW::loadValue()
{
    string s;
    m_cflink->get(s);
    if (m_fsencoding)
        m_le->setText(QString::fromLocal8Bit(s.c_str()));
    else
        m_le->setText(QString::fromUtf8(s.c_str()));
}

ConfParamCStrW::ConfParamCStrW(QWidget *parent, ConfLink cflink, 
			       const QString& lbltxt,
			       const QString& tltptxt,
			       const QStringList &sl
			       )
    : ConfParamW(parent, cflink)
{
    if (!createCommon(lbltxt, tltptxt))
	return;
    m_cmb = new QComboBox(this);
    m_cmb->setEditable(false);
    m_cmb->insertItems(0, sl);

    setSzPol(m_cmb, QSizePolicy::Preferred, QSizePolicy::Fixed, 1, 0);

    m_hl->addWidget(m_cmb);

    loadValue();
    QObject::connect(m_cmb, SIGNAL(activated(const QString&)), 
		     this, SLOT(setValue(const QString&)));
}

void ConfParamCStrW::loadValue()
{
    string s;
    m_cflink->get(s);
    QString cs;
    if (m_fsencoding)
        cs = QString::fromLocal8Bit(s.c_str());
    else
        cs = QString::fromUtf8(s.c_str());
        
    for (int i = 0; i < m_cmb->count(); i++) {
	if (!cs.compare(m_cmb->itemText(i))) {
	    m_cmb->setCurrentIndex(i);
	    break;
	}
    }
}

ConfParamBoolW::ConfParamBoolW(QWidget *parent, ConfLink cflink, 
			       const QString& lbltxt,
			       const QString& tltptxt)
    : ConfParamW(parent, cflink)
{
    // No createCommon because the checkbox has a label
    m_hl = new QHBoxLayout(this);
    m_hl->setSpacing(spacing);

    m_cb = new QCheckBox(lbltxt, this);
    setSzPol(m_cb, QSizePolicy::Fixed, QSizePolicy::Fixed, 0, 0);
    m_cb->setToolTip(tltptxt);
    m_hl->addWidget(m_cb);

    QFrame *fr = new QFrame(this);
    setSzPol(fr, QSizePolicy::Preferred, QSizePolicy::Fixed, 1, 0);
    m_hl->addWidget(fr);

    loadValue();
    QObject::connect(m_cb, SIGNAL(toggled(bool)), this, SLOT(setValue(bool)));
}

void ConfParamBoolW::loadValue()
{
    string s;
    m_cflink->get(s);
    m_cb->setChecked(stringToBool(s));
}

ConfParamFNW::ConfParamFNW(QWidget *parent, ConfLink cflink, 
			   const QString& lbltxt,
			   const QString& tltptxt,
			   bool isdir
			   )
    : ConfParamW(parent, cflink), m_isdir(isdir)
{
    if (!createCommon(lbltxt, tltptxt))
	return;

    m_fsencoding = true;

    m_le = new QLineEdit(this);
    m_le->setMinimumSize(QSize(150, 0 ));
    setSzPol(m_le, QSizePolicy::Preferred, QSizePolicy::Fixed, 1, 0);
    m_hl->addWidget(m_le);

    m_pb = new QPushButton(this);
    
    QString text = tr("Choose");
    m_pb->setText(text);
    int width = m_pb->fontMetrics().boundingRect(text).width() + 15;
    m_pb->setMaximumWidth(width);
    setSzPol(m_pb, QSizePolicy::Minimum, QSizePolicy::Fixed, 0, 0);
    m_hl->addWidget(m_pb);

    loadValue();
    QObject::connect(m_pb, SIGNAL(clicked()), this, SLOT(showBrowserDialog()));
    QObject::connect(m_le, SIGNAL(textChanged(const QString&)), 
		     this, SLOT(setValue(const QString&)));
}

void ConfParamFNW::loadValue()
{
    string s;
    m_cflink->get(s);
    m_le->setText(QString::fromLocal8Bit(s.c_str()));
}

void ConfParamFNW::showBrowserDialog()
{
    QString s = myGetFileName(m_isdir);
    if (!s.isEmpty())
        m_le->setText(s);
}

class SmallerListWidget: public QListWidget 
{
public:
    SmallerListWidget(QWidget *parent)
	: QListWidget(parent) {}
    virtual QSize sizeHint() const {return QSize(150, 40);}
};

ConfParamSLW::ConfParamSLW(QWidget *parent, ConfLink cflink, 
			   const QString& lbltxt,
			   const QString& tltptxt)
    : ConfParamW(parent, cflink)
{
    // Can't use createCommon here cause we want the buttons below the label
    m_hl = new QHBoxLayout(this);
    m_hl->setSpacing(spacing);

    QVBoxLayout *vl1 = new QVBoxLayout();
    QHBoxLayout *hl1 = new QHBoxLayout();

    QLabel *tl = new QLabel(this);
    setSzPol(tl, QSizePolicy::Preferred, QSizePolicy::Fixed, 0, 0);
    tl->setText(lbltxt);
    tl->setToolTip(tltptxt);
    vl1->addWidget(tl);

    QPushButton *pbA = new QPushButton(this);
    QString text = tr("+");
    pbA->setText(text);
    int width = pbA->fontMetrics().boundingRect(text).width() + 15;
    pbA->setMaximumWidth(width);
    setSzPol(pbA, QSizePolicy::Minimum, QSizePolicy::Fixed, 0, 0);
    hl1->addWidget(pbA);

    QPushButton *pbD = new QPushButton(this);
    text = tr("-");
    pbD->setText(text);
    width = pbD->fontMetrics().boundingRect(text).width() + 15;
    pbD->setMaximumWidth(width);
    setSzPol(pbD, QSizePolicy::Minimum, QSizePolicy::Fixed, 0, 0);
    hl1->addWidget(pbD);

    vl1->addLayout(hl1);
    m_hl->addLayout(vl1);

    m_lb = new SmallerListWidget(this);
    m_lb->setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSzPol(m_lb, QSizePolicy::Preferred, QSizePolicy::Preferred, 1, 1);
    m_hl->addWidget(m_lb);

    setSzPol(this, QSizePolicy::Preferred, QSizePolicy::Preferred, 1, 1);
    loadValue();
    QObject::connect(pbA, SIGNAL(clicked()), this, SLOT(showInputDialog()));
    QObject::connect(pbD, SIGNAL(clicked()), this, SLOT(deleteSelected()));
}

void ConfParamSLW::loadValue()
{
    string s;
    m_cflink->get(s);
    list<string> ls; 
    stringToStrings(s, ls);
    QStringList qls;
    for (list<string>::const_iterator it = ls.begin(); it != ls.end(); it++) {
        if (m_fsencoding)
            qls.push_back(QString::fromLocal8Bit(it->c_str()));
        else
            qls.push_back(QString::fromUtf8(it->c_str()));
    }
    m_lb->clear();
    m_lb->insertItems(0, qls);
}

void ConfParamSLW::showInputDialog()
{
    bool ok;
    QString s = QInputDialog::getText (this, 
				       "", // title 
				       "", // label, 
				       QLineEdit::Normal, // EchoMode mode
				       "", // const QString & text 
				       &ok);

    if (ok && !s.isEmpty()) {
	QList<QListWidgetItem *>items = 
	    m_lb->findItems(s, Qt::MatchFixedString|Qt::MatchCaseSensitive);
	if (items.empty()) {
	    m_lb->insertItem(0, s);
	    m_lb->sortItems();
	    listToConf();
	}
    }
}

void ConfParamSLW::listToConf()
{
    list<string> ls;
    for (int i = 0; i < m_lb->count(); i++) {
        // General parameters are encoded as utf-8. File names as
        // local8bit There is no hope for 8bit file names anyway
        // except for luck: the original encoding is unknown.
	QString text = m_lb->item(i)->text();
        if (m_fsencoding)
            ls.push_back((const char *)(text.toLocal8Bit()));
        else
            ls.push_back((const char *)(text.toUtf8()));
    }
    string s;
    stringsToString(ls, s);
    m_cflink->set(s);
}

void ConfParamSLW::deleteSelected()
{
    // We used to repeatedly go through the list and delete the first
    // found selected item (then restart from the beginning). But it
    // seems (probably depends on the qt version), that, when deleting
    // a selected item, qt will keep the selection active at the same
    // index (now containing the next item), so that we'd end up
    // deleting the whole list.
    //
    // Instead, we now build a list of indices, and delete it starting
    // from the top so as not to invalidate lower indices

    vector<int> idxes;
    for (int i = 0; i < m_lb->count(); i++) {
	if (m_lb->item(i)->isSelected()) {
	    idxes.push_back(i);
	}
    }
    for (vector<int>::reverse_iterator it = idxes.rbegin(); 
	 it != idxes.rend(); it++) {
	LOGDEB0(("deleteSelected: %d was selected\n", *it));
	QListWidgetItem *item = m_lb->takeItem(*it);
	emit entryDeleted(item->text());
	delete item;
    }

    listToConf();
}

// "Add entry" dialog for a file name list
void ConfParamDNLW::showInputDialog()
{
    QString s = myGetFileName(true);
    if (!s.isEmpty()) {
	QList<QListWidgetItem *>items = 
	    m_lb->findItems(s, Qt::MatchFixedString|Qt::MatchCaseSensitive);
	if (items.empty()) {
	    m_lb->insertItem(0, s);
	    m_lb->sortItems();
	    QList<QListWidgetItem *>items = 
		m_lb->findItems(s, Qt::MatchFixedString|Qt::MatchCaseSensitive);
	    if (m_lb->selectionMode() == QAbstractItemView::SingleSelection && 
		!items.empty())
		m_lb->setCurrentItem(*items.begin());
	    listToConf();
	}
    }
}

// "Add entry" dialog for a constrained string list
void ConfParamCSLW::showInputDialog()
{
    bool ok;
    QString s = QInputDialog::getItem (this, // parent
				       "", // title 
				       "", // label, 
				       m_sl, // items, 
				       0, // current = 0
				       false, // editable = true, 
				       &ok);

    if (ok && !s.isEmpty()) {
	QList<QListWidgetItem *>items = 
	    m_lb->findItems(s, Qt::MatchFixedString|Qt::MatchCaseSensitive);
	if (items.empty()) {
	    m_lb->insertItem(0, s);
	    m_lb->sortItems();
	    listToConf();
	}
    }
}

} // Namespace confgui
