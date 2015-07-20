TEMPLATE	= app
LANGUAGE	= C++

CONFIG	+= qt warn_on thread release debug

HEADERS	+= confgui.h confguiindex.h

SOURCES	+= main.cpp confgui.cpp confguiindex.cpp

#FORMS	= 

unix {
  UI_DIR = .ui
  MOC_DIR = .moc
  OBJECTS_DIR = .obj

  DEFINES += RECOLL_DATADIR='\\"/usr/local/share/recoll\\"'
  LIBS += ../../lib/librcl.a -lxapian -liconv -lz 

  INCLUDEPATH += ../../common ../../utils ../../rcldb
#../index ../internfile ../query ../unac \
#	      ../aspell ../rcldb
  POST_TARGETDEPS = ../../lib/librcl.a
}

UNAME = $$system(uname -s)
contains( UNAME, [lL]inux ) {
          LIBS -= -liconv
}

#The following line was inserted by qt3to4
QT +=  qt3support 
