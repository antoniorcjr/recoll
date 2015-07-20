depth = ..
include $(depth)/mk/sysconf

# Use a static link for some of the utility programs so that they can be
# executed independantly of the installed version of recoll. None of them
# are part of the official distrib anyway
LIBRECOLL = ../lib/librecoll.a

PROGS = pxattr trecrontab \
      trnetcon trcopyfile trcircache trmd5 trreadfile trfileudi \
      trconftree wipedir smallut  trfstreewalk trpathut transcode trbase64 \
      trmimeparse trexecmd utf8iter idfile

all: librecoll

PXATTROBJS = trpxattr.o pxattr.o 
pxattr:  $(PXATTROBJS)
	$(CXX) -o pxattr $(PXATTROBJS)
trpxattr.o : pxattr.cpp
	$(CXX) -c $(CXXFLAGS) -DTEST_PXATTR -o $@ pxattr.cpp

ECRONTAB_OBJS= trecrontab.o 
trecrontab : $(ECRONTAB_OBJS)
	$(CXX) -o trecrontab $(ECRONTAB_OBJS) \
            $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trecrontab.o : ecrontab.cpp ecrontab.h
	$(CXX) -o trecrontab.o -c $(ALL_CXXFLAGS) \
	       -DTEST_ECRONTAB ecrontab.cpp

FSTREEWALK_OBJS= trfstreewalk.o 
trfstreewalk : $(FSTREEWALK_OBJS)
	$(CXX) -o trfstreewalk $(FSTREEWALK_OBJS) \
            $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trfstreewalk.o : fstreewalk.cpp fstreewalk.h
	$(CXX) -o trfstreewalk.o -c $(ALL_CXXFLAGS) \
	       -DTEST_FSTREEWALK fstreewalk.cpp

APPFORMIME_OBJS= trappformime.o 
trappformime : $(APPFORMIME_OBJS)
	$(CXX) -o trappformime $(APPFORMIME_OBJS) \
            $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trappformime.o : appformime.cpp
	$(CXX) -o trappformime.o -c $(ALL_CXXFLAGS) \
	       -DTEST_APPFORMIME appformime.cpp

READFILE_OBJS= trreadfile.o 
trreadfile : $(READFILE_OBJS)
	$(CXX) -o trreadfile $(READFILE_OBJS) $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trreadfile.o : readfile.cpp readfile.h
	$(CXX) -o trreadfile.o -c $(ALL_CXXFLAGS) \
	       -DTEST_READFILE readfile.cpp

CPUCONF_OBJS= trcpuconf.o 
trcpuconf : $(CPUCONF_OBJS)
	$(CXX) -o trcpuconf $(CPUCONF_OBJS) $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trcpuconf.o : cpuconf.cpp cpuconf.h
	$(CXX) -o trcpuconf.o -c $(ALL_CXXFLAGS) -DTEST_CPUCONF cpuconf.cpp

CIRCACHE_OBJS= trcircache.o 
trcircache : $(CIRCACHE_OBJS)
	$(CXX) -o trcircache $(CIRCACHE_OBJS) \
               $(LIBRECOLL) $(LIBICONV) $(LIBSYS) -lz
trcircache.o : circache.cpp circache.h
	$(CXX) -o trcircache.o -c $(ALL_CXXFLAGS) \
	       -DTEST_CIRCACHE circache.cpp

COPYFILE_OBJS= trcopyfile.o 
trcopyfile : $(COPYFILE_OBJS)
	$(CXX) -o trcopyfile $(COPYFILE_OBJS) $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trcopyfile.o : copyfile.cpp copyfile.h
	$(CXX) -o trcopyfile.o -c $(ALL_CXXFLAGS) \
	       -DTEST_COPYFILE copyfile.cpp

MD5_OBJS= trmd5.o 
trmd5 : $(MD5_OBJS)
	$(CXX) -o trmd5 $(MD5_OBJS) $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trmd5.o : md5.cpp md5.h
	$(CXX) -o trmd5.o -c $(ALL_CXXFLAGS) -DTEST_MD5 md5.cpp

PATHUT_OBJS= trpathut.o  
trpathut : $(PATHUT_OBJS) 
	$(CXX) $(ALL_CXXFLAGS) -o trpathut $(PATHUT_OBJS)  \
               $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trpathut.o : pathut.cpp pathut.h
	$(CXX) -o trpathut.o -c $(ALL_CXXFLAGS) -DTEST_PATHUT pathut.cpp

NETCON_OBJS= trnetcon.o  
trnetcon : $(NETCON_OBJS) 
	$(CXX) $(ALL_CXXFLAGS) -o trnetcon $(NETCON_OBJS)  \
               $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trnetcon.o : netcon.cpp netcon.h
	$(CXX) -o trnetcon.o -c $(ALL_CXXFLAGS) -DTEST_NETCON netcon.cpp

FILEUDI_OBJS= trfileudi.o  
trfileudi : $(FILEUDI_OBJS) 
	$(CXX) $(ALL_CXXFLAGS) -o trfileudi $(FILEUDI_OBJS)
trfileudi.o : fileudi.cpp fileudi.h
	$(CXX) -o trfileudi.o -c $(ALL_CXXFLAGS) -DTEST_FILEUDI fileudi.cpp

EXECMD_OBJS= trexecmd.o 
trexecmd : $(EXECMD_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o trexecmd $(EXECMD_OBJS) \
               $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trexecmd.o : execmd.cpp execmd.h
	$(CXX) -o trexecmd.o -c $(ALL_CXXFLAGS) -DTEST_EXECMD execmd.cpp

TRANSCODE_OBJS= trtranscode.o   
transcode : $(TRANSCODE_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o transcode $(TRANSCODE_OBJS) \
	       $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trtranscode.o : transcode.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_TRANSCODE -c -o trtranscode.o \
	       transcode.cpp

IDFILE_OBJS= tridfile.o   
idfile : $(IDFILE_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o idfile $(IDFILE_OBJS) \
	       $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
tridfile.o : idfile.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_IDFILE -c -o tridfile.o idfile.cpp

MIMEPARSE_OBJS= trmimeparse.o   
trmimeparse : $(MIMEPARSE_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o trmimeparse $(MIMEPARSE_OBJS) \
	       $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trmimeparse.o : mimeparse.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_MIMEPARSE -c -o trmimeparse.o \
               mimeparse.cpp

SMALLUT_OBJS= trsmallut.o ../lib/smallut.o
smallut : $(SMALLUT_OBJS) smallut.h
	$(CXX) $(ALL_CXXFLAGS) -o smallut $(SMALLUT_OBJS) \
               $(LIBRECOLL) $(LIBICONV)
trsmallut.o : smallut.cpp smallut.h
	$(CXX) $(ALL_CXXFLAGS) -DTEST_SMALLUT -c -o trsmallut.o smallut.cpp
../lib/smallut.o: smallut.cpp smallut.h
	cd ../lib;make smallut.o

WIPEDIR_OBJS= trwipedir.o  
wipedir : $(WIPEDIR_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o wipedir $(WIPEDIR_OBJS) $(LIBICONV) $(LIBSYS)
trwipedir.o : wipedir.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_WIPEDIR -c -o trwipedir.o wipedir.cpp

UTF8ITER_OBJS= trutf8iter.o 
utf8iter : $(UTF8ITER_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o utf8iter $(UTF8ITER_OBJS)  \
	       $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trutf8iter.o : utf8iter.cpp utf8iter.h
	$(CXX) $(ALL_CXXFLAGS) -DTEST_UTF8ITER -c -o trutf8iter.o utf8iter.cpp

CONFTREE_OBJS= trconftree.o  
trconftree : $(CONFTREE_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o trconftree $(CONFTREE_OBJS) \
	       $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trconftree.o : conftree.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_CONFTREE -c -o trconftree.o conftree.cpp

BASE64_OBJS= trbase64.o  
trbase64 : $(BASE64_OBJS) 
	$(CXX) $(ALL_CXXFLAGS) -o trbase64 $(BASE64_OBJS)
trbase64.o : base64.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_BASE64 -c -o trbase64.o base64.cpp

X11MON_OBJS= trx11mon.o x11mon.o
trx11mon : $(X11MON_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o trx11mon $(X11MON_OBJS) -L/usr/X11R6/lib -lX11
trx11mon.o : x11mon.cpp x11mon.h
	$(CXX) -o trx11mon.o -c $(ALL_CXXFLAGS) -DTEST_X11MON x11mon.cpp
x11mon.o: x11mon.cpp
	$(CXX) -c -I/usr/X11R6/include $(ALL_CXXFLAGS) x11mon.cpp

include $(depth)/mk/commontargets
