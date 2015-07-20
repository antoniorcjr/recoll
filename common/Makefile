depth = ..
include $(depth)/mk/sysconf

# Only test executables get build in here
PROGS = unacpp textsplit rclconfig

all: librecoll $(PROGS) 

UNACPP_OBJS= trunacpp.o
unacpp : $(UNACPP_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o unacpp $(UNACPP_OBJS) \
	       $(LIBRECOLL)  $(LIBICONV)
trunacpp.o : unacpp.cpp unacpp.h
	$(CXX) $(ALL_CXXFLAGS) -DTEST_UNACPP -c -o trunacpp.o unacpp.cpp

TEXTSPLIT_OBJS= trtextsplit.o  
textsplit : $(TEXTSPLIT_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o textsplit $(TEXTSPLIT_OBJS) \
        $(LIBRECOLL) $(LIBICONV)
trtextsplit.o : textsplit.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_TEXTSPLIT -c -o trtextsplit.o \
	       textsplit.cpp

RCLCONFIG_OBJS= trrclconfig.o 
rclconfig : $(RCLCONFIG_OBJS) 
	$(CXX) $(ALL_CXXFLAGS) -o rclconfig $(RCLCONFIG_OBJS) \
	       $(LIBRECOLL) $(LIBICONV) $(LIBSYS)
trrclconfig.o : rclconfig.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_RCLCONFIG -c -o trrclconfig.o \
	       rclconfig.cpp

include $(depth)/mk/commontargets

