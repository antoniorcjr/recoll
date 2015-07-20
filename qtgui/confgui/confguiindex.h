/* Copyright (C) 2007 J.F.Dockes
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

#ifndef _confguiindex_h_included_
#define _confguiindex_h_included_

/**
 * Classes to handle the gui for the indexing configuration. These group 
 * confgui elements, linked to configuration parameters, into panels.
 */

#include <QWidget>
#include <QString>
#include <QGroupBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QListWidgetItem>

#include <string>
#include <list>
using std::string;
using std::list;

class ConfNull;
class RclConfig;
class ConfParamW;
class ConfParamDNLW;

namespace confgui {

class ConfIndexW : public QDialog {
    Q_OBJECT
public:
    ConfIndexW(QWidget *parent, RclConfig *config);
public slots:
    void acceptChanges();
    void rejectChanges();
    void reloadPanels();
private:
    RclConfig *m_rclconf;
    ConfNull  *m_conf;
    list<QWidget *> m_widgets;
    QTabWidget       *tabWidget;
    QDialogButtonBox *buttonBox;
};

/** 
 * A panel with the top-level parameters which can't be redefined in 
 * subdirectoriess:
 */
class ConfTopPanelW : public QWidget {
    Q_OBJECT
public:
    ConfTopPanelW(QWidget *parent, ConfNull *config);
};

/**
 * A panel for the parameters that can be changed in subdirectories:
 */
class ConfSubPanelW : public QWidget {
    Q_OBJECT
public:
    ConfSubPanelW(QWidget *parent, ConfNull *config, RclConfig *rclconf);

private slots:
    void subDirChanged(QListWidgetItem *, QListWidgetItem *);
    void subDirDeleted(QString);
    void restoreEmpty();
private:
    string            m_sk;
    ConfParamDNLW    *m_subdirs;
    list<ConfParamW*> m_widgets;
    ConfNull         *m_config;
    QGroupBox        *m_groupbox;
    void reloadAll();
};

class ConfBeaglePanelW : public QWidget {
    Q_OBJECT
public:
    ConfBeaglePanelW(QWidget *parent, ConfNull *config);
};

class ConfSearchPanelW : public QWidget {
    Q_OBJECT
public:
    ConfSearchPanelW(QWidget *parent, ConfNull *config);
};

} // Namespace confgui

#endif /* _confguiindex_h_included_ */
