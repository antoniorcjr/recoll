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
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <string>
using std::string;

#include <QCheckBox>
#include <QMessageBox>

#include "recoll.h"
#include "rtitool.h"
#include "smallut.h"
#include "pathut.h"
#include "copyfile.h"
#include "readfile.h"
#include "execmd.h"

static const char *rautostartfile = ".config/autostart/recollindex.desktop";

// Just in case we don't find the file in the shared dir, have a
// default text ready
static const char *desktopfiletext = 
    "[Desktop Entry]\n"
    "Name=Recoll real time indexer\n"
    "Comment=Runs in background to extract and index text from modified "
     "documents\n"
    "Icon=system-run\n"
    "Exec=recollindex -w 60 -m\n"
    "Terminal=false\n"
    "TerminalOptions=\n"
    "Type=Application\n"
    "Categories=Utility;Filesystem;Database;\n"
    "NoDisplay=true\n"
    "X-GNOME-Autostart-enabled=true\n"
    "X-KDE-autostart-after=panel\n"
    "X-KDE-UniqueApplet=true\n"
    ;

void RTIToolW::init()
{
    connect(this->sesCB, SIGNAL(clicked(bool)), 
	    this, SLOT(sesclicked(bool)));
    string autostartfile = path_cat(path_home(), rautostartfile);
    if (access(autostartfile.c_str(), 0) == 0) {
	sesCB->setChecked(true);
    }
}

void RTIToolW::sesclicked(bool on)
{
    nowCB->setEnabled(on);
    if (!on)
	nowCB->setChecked(false);
}

void RTIToolW::accept()
{
    bool exitdial = false;
    string autostartfile = path_cat(path_home(), rautostartfile);

    if (sesCB->isChecked()) {
	// Setting up daemon indexing autostart

	if (::access(autostartfile.c_str(), 0) == 0) {
	    QString msg = tr("Replacing: ") + 
		QString::fromLocal8Bit(autostartfile.c_str());
	
	    QMessageBox::Button rep = 
		QMessageBox::question(this, tr("Replacing file"), msg,
				      QMessageBox::Ok | QMessageBox::Cancel);
	    if (rep != QMessageBox::Ok) {
		goto out;
	    }
	}

	string text;
	if (theconfig) {
	    string sourcefile = path_cat(theconfig->getDatadir(), "examples");
	    sourcefile = path_cat(sourcefile, "recollindex.desktop");
	    if (::access(sourcefile.c_str(), 0) == 0) {
		file_to_string(sourcefile, text);
	    }
	}
	if (text.empty())
	    text = desktopfiletext;

	// Try to create .config and autostart anyway. If they exists this will 
	// do nothing. An error will be detected when we try to create the file
	string dir = path_cat(path_home(), ".config");
	mkdir(dir.c_str(), 0700);
	dir = path_cat(dir, "autostart");
	mkdir(dir.c_str(), 0700);

	int fd = ::open(autostartfile.c_str(), O_WRONLY|O_CREAT, 0644);
	if (fd < 0 || ::write(fd, text.c_str(), size_t(text.size())) 
	    != ssize_t(text.size()) || ::close(fd) != 0) {
	    if (fd >=0)
		::close(fd);
	    QString msg = tr("Can't create: ") + 
		QString::fromLocal8Bit(autostartfile.c_str());
	    QMessageBox::warning(0, tr("Warning"), msg, QMessageBox::Ok);
	    return;
	}
	::close(fd);

	if (nowCB->isChecked()) {
	    ExecCmd cmd;
	    vector<string> args; 
	    int status;

	    args.push_back("-m");
	    args.push_back("-w");
	    args.push_back("0");
	    status = cmd.doexec("recollindex", args, 0, 0);
	    if (status) {
		QMessageBox::warning(0, tr("Warning"), 
				     tr("Could not execute recollindex"), 
				     QMessageBox::Ok);
		goto out;
	    }
	}

	exitdial = true;
    } else {
	// Turning autostart off
	if (::access(autostartfile.c_str(), 0) == 0) {
	    QString msg = tr("Deleting: ") + 
		QString::fromLocal8Bit(autostartfile.c_str());
	
	    QMessageBox::Button rep = 
		QMessageBox::question(this, tr("Deleting file"), msg,
				      QMessageBox::Ok | QMessageBox::Cancel);
	    if (rep == QMessageBox::Ok) {
		exitdial = true;
		unlink(autostartfile.c_str());
		if (theconfig) {
		    Pidfile pidfile(theconfig->getPidfile());
		    pid_t pid;
		    if ((pid = pidfile.open()) != 0) {
			QMessageBox::Button rep = 
			    QMessageBox::question(this, 
 	                     tr("Removing autostart"), 
		       tr("Autostart file deleted. Kill current process too ?"),
					  QMessageBox::Yes | QMessageBox::No);
			if (rep == QMessageBox::Yes) {
			    kill(pid, SIGTERM);
			}
		    }
		}
	    }
	} else {
	    exitdial = true;
	}
    }

out:
    if (exitdial)
	QDialog::accept();
}
