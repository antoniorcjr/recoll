depth = ..
include $(depth)/mk/sysconf

# Only test executables get build in here
PROGS = mh_mbox internfile

all: librecoll $(PROGS) 

INTERNFILE_OBJS= trinternfile.o  $(BIGLIB) 
internfile : $(INTERNFILE_OBJS) 
	$(CXX) $(ALL_CXXFLAGS) -o internfile $(INTERNFILE_OBJS) \
	       $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trinternfile.o : internfile.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_INTERNFILE -c -o trinternfile.o \
	       internfile.cpp

MH_MBOX_OBJS= trmh_mbox.o  $(BIGLIB) 
mh_mbox : $(MH_MBOX_OBJS) 
	$(CXX) $(ALL_CXXFLAGS) -o mh_mbox $(MH_MBOX_OBJS) \
	       $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trmh_mbox.o : mh_mbox.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_MH_MBOX -c -o trmh_mbox.o \
	       mh_mbox.cpp

include $(depth)/mk/commontargets
