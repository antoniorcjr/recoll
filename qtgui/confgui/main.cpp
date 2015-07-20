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

#include <string>
#include <iostream>

using namespace std;

#include <unistd.h>

#include <qglobal.h>
#include <qobject.h>
#include <QFrame>
#include <qobject.h>
#include <qapplication.h>
#include <qtranslator.h>
#include <qtextcodec.h> 
#include <qthread.h>
#include <qtimer.h>
#include <qlayout.h>
#include <qwidget.h>
#include <qlabel.h>

#include "pathut.h"
#include "confguiindex.h"
#include "debuglog.h"
#include "rclconfig.h"
#include "execmd.h"
#include "conflinkrcl.h"

using namespace confgui;

static const char *thisprog;
static int    op_flags;
#define OPT_MOINS 0x1
#define OPT_h     0x4 
#define OPT_c     0x20
static const char usage [] =
"\n"
"trconf\n"
    "   -h : Print help and exit\n"
    ;
//"   -c <configdir> : specify config directory, overriding $RECOLL_CONFDIR\n"
//;
static void
Usage(void)
{
    FILE *fp = (op_flags & OPT_h) ? stdout : stderr;
    fprintf(fp, "%s: Usage: %s", thisprog, usage);
    exit((op_flags & OPT_h)==0);
}


static RclConfig *config;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    //    fprintf(stderr, "Application created\n");
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'h': op_flags |= OPT_h; Usage();break;
	    }
	//	 b1: 
	argc--; argv++;
    }

    DebugLog::getdbl()->setloglevel(DEBDEB1);
    DebugLog::setfilename("stderr");

    string a_config = "tstconfdir";
    config = new RclConfig(&a_config);
    if (config == 0 || !config->ok()) {
	cerr << "Cant read configuration in: " << a_config << endl;
	exit(1);
    }
    cerr << "Working with configuration file in: " << config->getConfDir() 
	 << endl;

    // Translation file for Qt
    QTranslator qt( 0 );
    qt.load( QString( "qt_" ) + QTextCodec::locale(), "." );
    app.installTranslator( &qt );

    // Translations for Recoll
    string translatdir = path_cat(config->getDatadir(), "translations");
    QTranslator translator( 0 );
    // QTextCodec::locale() returns $LANG
    translator.load( QString("recoll_") + QTextCodec::locale(), 
		     translatdir.c_str() );
    app.installTranslator( &translator );
    //    fprintf(stderr, "Translations installed\n");

    ConfIndexW w(0, config);
    QSize size(0, 0);
    size = size.expandedTo(w.minimumSizeHint());
    w.resize(size);
    //w.setSizeGripEnabled(true);
    w.show();

    // Connect exit handlers etc.. Beware, apparently this must come
    // after mainWindow->show() , else the QMessageBox above never
    // returns.
    app.connect(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));
    //    fprintf(stderr, "Go\n");
    // Let's go
    return app.exec();
}
