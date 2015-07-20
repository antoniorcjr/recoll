depth = ..
include $(depth)/mk/sysconf

# Only test executables get build in here
PROGS = synfamily stoplist

all: librecoll $(PROGS) 

STOPLIST_OBJS= trstoplist.o
stoplist : $(STOPLIST_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o stoplist $(STOPLIST_OBJS) \
            $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trstoplist.o : stoplist.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_STOPLIST -c -o trstoplist.o \
	       stoplist.cpp

SYNFAMILY_OBJS= trsynfamily.o
synfamily : $(SYNFAMILY_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o synfamily $(SYNFAMILY_OBJS) \
        $(LIBRECOLL)  $(LIBICONV) $(LIBXAPIAN) $(LIBSYS)
trsynfamily.o : synfamily.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_SYNFAMILY -c -o trsynfamily.o \
	       synfamily.cpp

include $(depth)/mk/commontargets
