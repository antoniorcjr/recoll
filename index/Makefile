depth = ..
include $(depth)/mk/sysconf

PROGS = recollindex 
SRCS = recollindex.cpp rclmonrcv.cpp rclmonprc.cpp

all: depend librecoll $(PROGS)

RECOLLINDEX_OBJS= recollindex.o rclmonrcv.o rclmonprc.o 
recollindex : $(RECOLLINDEX_OBJS)
	$(CXX) $(ALL_CXXFLAGS) $(RECOLL_LDFLAGS) -o recollindex \
               $(RECOLLINDEX_OBJS) \
	       $(BSTATIC) $(LIBRECOLL) $(LIBXAPIAN) $(LIBXAPIANSTATICEXTRA) \
	       $(LIBICONV) $(BDYNAMIC) \
	       $(LIBFAM) \
	       $(X_LIBS) $(X_PRE_LIBS) $(X_LIBX11) $(X_EXTRA_LIBS) \
	       $(LIBSYS) $(LIBTHREADS)
recollindex.o : recollindex.cpp
	$(CXX) $(ALL_CXXFLAGS) -c -o recollindex.o $<
rclmonrcv.o : rclmonrcv.cpp 
	$(CXX) $(ALL_CXXFLAGS) -c -o rclmonrcv.o $<
rclmonprc.o : rclmonprc.cpp 
	$(CXX) $(ALL_CXXFLAGS) -c -o rclmonprc.o $<

SUBTREELIST_OBJS= subtreelist.o
subtreelist : $(SUBTREELIST_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o subtreelist $(SUBTREELIST_OBJS) \
	       $(LIBRECOLL) $(LIBICONV) $(LIBXAPIAN) $(LIBSYS)
subtreelist.o : subtreelist.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_SUBTREELIST -c subtreelist.cpp

MIMETYPE_OBJS= trmimetype.o
mimetype : $(MIMETYPE_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o mimetype $(MIMETYPE_OBJS) \
	       $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trmimetype.o : mimetype.cpp
	$(CXX) $(ALL_CXXFLAGS) -DTEST_MIMETYPE -c -o trmimetype.o \
	       mimetype.cpp

include $(depth)/mk/commontargets

include alldeps
