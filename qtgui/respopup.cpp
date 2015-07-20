/*
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

#include <qapplication.h>
#include <qmenu.h>
#include <qclipboard.h>

#include "debuglog.h"
#include "smallut.h"
#include "recoll.h"
#include "docseq.h"
#include "respopup.h"
#include "appformime.h"

namespace ResultPopup {

QMenu *create(QWidget *me, int opts, RefCntr<DocSequence> source, Rcl::Doc& doc)
{
    QMenu *popup = new QMenu(me);

    LOGDEB(("ResultPopup::create: opts %x haspages %d %s %s\n", opts, 
	    doc.haspages, source.isNull() ? 
	    "Source is Null" : "Source not null",
	    source.isNull() ? "" : source->snippetsCapable() ? 
	    "snippetsCapable" : "not snippetsCapable"));

    string apptag;
    doc.getmeta(Rcl::Doc::keyapptg, &apptag);

    popup->addAction(QWidget::tr("&Preview"), me, SLOT(menuPreview()));

    if (!theconfig->getMimeViewerDef(doc.mimetype, apptag, 0).empty()) {
	popup->addAction(QWidget::tr("&Open"), me, SLOT(menuEdit()));
    }

    bool needopenwith = true;
    if (!doc.ipath.empty())
        needopenwith = false;
    if (needopenwith) {
        string backend;
        doc.getmeta(Rcl::Doc::keybcknd, &backend);
        if (!backend.empty() && backend.compare("FS"))
            needopenwith = false;
    }
            
    if (needopenwith) {
        vector<DesktopDb::AppDef> aps;
        DesktopDb *ddb = DesktopDb::getDb();
        if (ddb && ddb->appForMime(doc.mimetype, &aps) && 
            !aps.empty()) {
            QMenu *sub = popup->addMenu(QWidget::tr("Open With"));
            if (sub) {
                for (vector<DesktopDb::AppDef>::const_iterator it = aps.begin();
                     it != aps.end(); it++) {
                    QAction *act = new 
                        QAction(QString::fromUtf8(it->name.c_str()), me);
                    QVariant v(QString::fromUtf8(it->command.c_str()));
                    act->setData(v);
                    sub->addAction(act);
                }
                sub->connect(sub, SIGNAL(triggered(QAction *)), me, 
                             SLOT(menuOpenWith(QAction *)));
            }
        }

        // See if there are any desktop files in $RECOLL_CONFDIR/scripts
        // and possibly create a "run script" menu.
        aps.clear();
        ddb = new DesktopDb(path_cat(theconfig->getConfDir(), "scripts"));
        if (ddb && ddb->allApps(&aps) && !aps.empty()) {
            QMenu *sub = popup->addMenu(QWidget::tr("Run Script"));
            if (sub) {
                for (vector<DesktopDb::AppDef>::const_iterator it = aps.begin();
                     it != aps.end(); it++) {
                    QAction *act = new 
                        QAction(QString::fromUtf8(it->name.c_str()), me);
                    QVariant v(QString::fromUtf8(it->command.c_str()));
                    act->setData(v);
                    sub->addAction(act);
                }
                sub->connect(sub, SIGNAL(triggered(QAction *)), me, 
                             SLOT(menuOpenWith(QAction *)));
            }
        }
        delete ddb;
    }

    popup->addAction(QWidget::tr("Copy &File Name"), me, SLOT(menuCopyFN()));
    popup->addAction(QWidget::tr("Copy &URL"), me, SLOT(menuCopyURL()));

    if ((opts&showSaveOne) && !doc.ipath.empty())
	popup->addAction(QWidget::tr("&Write to File"), me, SLOT(menuSaveToFile()));

    if ((opts&showSaveSel))
	popup->addAction(QWidget::tr("Save selection to files"), 
			 me, SLOT(menuSaveSelection()));

    Rcl::Doc pdoc;
    if (source.isNotNull() && source->getEnclosing(doc, pdoc)) {
	popup->addAction(QWidget::tr("Preview P&arent document/folder"), 
			 me, SLOT(menuPreviewParent()));
    }
    // Open parent is useful even if there is no parent because we open
    // the enclosing folder.
    popup->addAction(QWidget::tr("&Open Parent document/folder"), 
		     me, SLOT(menuOpenParent()));

    if (opts & showExpand)
	popup->addAction(QWidget::tr("Find &similar documents"), 
			 me, SLOT(menuExpand()));

    if (doc.haspages && source.isNotNull() && source->snippetsCapable()) 
	popup->addAction(QWidget::tr("Open &Snippets window"), 
			 me, SLOT(menuShowSnippets()));

    if ((opts & showSubs) && rcldb && rcldb->hasSubDocs(doc)) 
	popup->addAction(QWidget::tr("Show subdocuments / attachments"), 
			 me, SLOT(menuShowSubDocs()));

    return popup;
}

Rcl::Doc getParent(RefCntr<DocSequence> source, Rcl::Doc& doc)
{
    Rcl::Doc pdoc;
    if (source.isNull() || !source->getEnclosing(doc, pdoc)) {
	// No parent doc: show enclosing folder with app configured for
	// directories
        pdoc.url = url_parentfolder(doc.url);
	pdoc.meta[Rcl::Doc::keychildurl] = doc.url;
	pdoc.meta[Rcl::Doc::keyapptg] = "parentopen";
	pdoc.mimetype = "inode/directory";
    }
    return pdoc;
}

void copyFN(const Rcl::Doc &doc)
{
    // Our urls currently always begin with "file://" 
    //
    // Problem: setText expects a QString. Passing a (const char*)
    // as we used to do causes an implicit conversion from
    // latin1. File are binary and the right approach would be no
    // conversion, but it's probably better (less worse...) to
    // make a "best effort" tentative and try to convert from the
    // locale's charset than accept the default conversion.
    QString qfn = QString::fromLocal8Bit(doc.url.c_str()+7);
    QApplication::clipboard()->setText(qfn, QClipboard::Selection);
    QApplication::clipboard()->setText(qfn, QClipboard::Clipboard);
}

void copyURL(const Rcl::Doc &doc)
{
    string url =  url_encode(doc.url, 7);
    QApplication::clipboard()->setText(url.c_str(), 
				       QClipboard::Selection);
    QApplication::clipboard()->setText(url.c_str(), 
				       QClipboard::Clipboard);
}

}
