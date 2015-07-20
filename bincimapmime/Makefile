# @(#$Id: Makefile,v 1.6 2006-01-19 12:01:42 dockes Exp $  (C) 2005 J.F.Dockes

depth = ..
include $(depth)/mk/sysconf

LIBS = libmime.a
PROGS = trbinc

all: depend $(LIBS) 

SRCS = mime-parsefull.cc mime-parseonlyheader.cc \
     mime-printbody.cc mime.cc \
     convert.cc iodevice.cc iofactory.cc

OBJS = mime-parsefull.o mime-parseonlyheader.o \
     mime-printbody.o mime.o  \
     convert.o iodevice.o iofactory.o

libmime.a : $(OBJS)
	 $(AR) ru libmime.a $(OBJS)

.cc.o:
	$(CXX) $(ALL_CXXFLAGS) -c $<

TRBINCOBJS = trbinc.o
trbinc: trbinc.o
	$(CXX) -o trbinc trbinc.o libmime.a 

include $(depth)/mk/commontargets

include alldeps
