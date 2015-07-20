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

#include <qevent.h>
#include <qwidget.h>

#include "recoll.h"
#include "rclhelp.h"
#include "debuglog.h"

map<string, string> HelpClient::helpmap;

void HelpClient::installMap(string wname, string section)
{
    helpmap[wname] = section;
}

HelpClient::HelpClient(QObject *parent, const char *)
    : QObject(parent)
{
    parent->installEventFilter(this);
}
    
bool HelpClient::eventFilter(QObject *obj, QEvent *event)
{
    static time_t last_start;
    if (event->type() == QEvent::KeyPress || 
	event->type() == QEvent::ShortcutOverride) {
	//	LOGDEB(("HelpClient::eventFilter: %d\n", (int)event->type()));
	QKeyEvent *ke = static_cast<QKeyEvent *>(event);
	if (ke->key() == Qt::Key_F1 || ke->key() == Qt::Key_Help) {
	    if (obj->isWidgetType()) {
		QWidget *widget = static_cast<QWidget *>(obj)->focusWidget();
		map<string, string>::iterator it = helpmap.end();
		while (widget) {
		    it = helpmap.find((const char *)widget->objectName().toUtf8());
		    if (it != helpmap.end())
			break;
		    widget = widget->parentWidget();
		}
		if (time(0) - last_start > 5) {
		    last_start = time(0);
		    if (it != helpmap.end()) {
			LOGDEB(("HelpClient::eventFilter: %s->%s\n",
				it->first.c_str(), it->second.c_str()));
			startManual(it->second);
		    } else {
			LOGDEB(("HelpClient::eventFilter: no help section\n"));
			startManual("");
		    }
		}
	    }
	    return true;
	}
    }
    return false;
}
