/* Copyright (C) 2012 J.F.Dockes
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
#ifndef _SNIPPETS_W_H_INCLUDED_
#define _SNIPPETS_W_H_INCLUDED_
#include <QString>

#include "rcldoc.h"
#include "refcntr.h"
#include "docseq.h"
#include "rclmain_w.h"

#include "ui_snippets.h"

class SnippetsW : public QWidget, public Ui::Snippets
{
    Q_OBJECT
public:
    SnippetsW(Rcl::Doc doc, RefCntr<DocSequence> source, QWidget* parent = 0) 
	: QWidget(parent), m_doc(doc), m_source(source)
    {
	setupUi((QDialog*)this);
	init();
    }

protected slots:
    virtual void linkWasClicked(const QUrl &);
    virtual void slotEditFind();
    virtual void slotEditFindNext();
    virtual void slotEditFindPrevious();
    virtual void slotSearchTextChanged(const QString&);
signals:
    void startNativeViewer(Rcl::Doc, int pagenum, QString term);
	
private:
    void init();
    Rcl::Doc m_doc;
    RefCntr<DocSequence> m_source;
};

#endif /* _SNIPPETS_W_H_INCLUDED_ */
