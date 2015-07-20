depth = ..
include $(depth)/mk/sysconf

PROGS = rclaspell
SRCS = rclaspell.cpp

all: depend $(PROGS) librecoll

RCLASPELL_OBJS= trrclaspell.o 
rclaspell : $(RCLASPELL_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o rclaspell $(RCLASPELL_OBJS) \
          $(LIBRECOLL)  $(LIBXAPIAN) $(LIBICONV)
trrclaspell.o : rclaspell.cpp
	$(CXX) $(ALL_CXXFLAGS) -DTEST_RCLASPELL -c -o trrclaspell.o \
	       rclaspell.cpp

include $(depth)/mk/commontargets

include alldeps
