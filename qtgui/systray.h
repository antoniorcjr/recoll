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
#ifndef _SYSTRAY_H_INCLUDED_
#define _SYSTRAY_H_INCLUDED_

#include <QSystemTrayIcon>
#include <QIcon>

class RclMain;

class RclTrayIcon : public QSystemTrayIcon {
    Q_OBJECT

    public:

    RclTrayIcon(RclMain *mainw, const QIcon& icon, QObject* parent = 0)
        : QSystemTrayIcon(icon, parent), m_mainw(mainw) { 
        init();
    }
public slots:
    void onRestore();

private:
    void init();
    RclMain *m_mainw;
};

#endif /* _SYSTRAY_H_INCLUDED_ */
