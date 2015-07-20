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
#include <QAction>
#include <QMenu>

#include "systray.h"
#include "rclmain_w.h"

void RclTrayIcon::init()
{
    QAction *restoreAction = new QAction(tr("Restore"), this);
    QAction *quitAction = new QAction(tr("Quit"), this);
     
    connect(restoreAction, SIGNAL(triggered()), this, SLOT(onRestore()));
    connect(quitAction, SIGNAL(triggered()), m_mainw, SLOT(fileExit()));

    QMenu *trayIconMenu = new QMenu(0);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addAction(quitAction);
    setContextMenu(trayIconMenu);
}

void RclTrayIcon::onRestore()
{
    // Hide and show to restore on current desktop
    m_mainw->hide();
    m_mainw->show();
}
