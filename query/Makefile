depth = ..
include $(depth)/mk/sysconf

PROGS = xadump recollq #trhist qtry qxtry 
SRCS = xadump.cpp 

all: depend librecoll $(PROGS)

XADUMP_OBJS= xadump.o 
xadump : $(XADUMP_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o xadump $(XADUMP_OBJS) \
	       $(depth)/lib/librecoll.a $(LIBICONV) $(LIBXAPIAN) $(LIBSYS)
xadump.o : xadump.cpp 
	$(CXX) $(ALL_CXXFLAGS) -c xadump.cpp

RECOLLQ_OBJS= recollq.o 
recollq : $(RECOLLQ_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o recollq $(RECOLLQ_OBJS) \
	       $(BSTATIC) $(LIBRECOLL) $(LIBXAPIAN) $(LIBICONV) $(BDYNAMIC) \
               $(LIBSYS)
recollq.o : recollq.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_RECOLLQ -c recollq.cpp

HISTORY_OBJS= trhist.o
trhist : $(HISTORY_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o trhist $(HISTORY_OBJS) \
	       $(LIBICONV) $(LIBXAPIAN)
trhist.o : history.cpp history.h
	$(CXX) $(ALL_CXXFLAGS) -DTEST_HISTORY -c -o trhist.o history.cpp

WASASTRINGTOQUERY_OBJS= trwasastrtoq.o  
trwasastrtoq : $(WASASTRINGTOQUERY_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o trwasastrtoq $(WASASTRINGTOQUERY_OBJS) \
             $(LIBRECOLL) $(LIBICONV) $(LIBXAPIAN)
trwasastrtoq.o : wasastringtoquery.cpp wasastringtoquery.h
	$(CXX) $(ALL_CXXFLAGS) -DTEST_WASASTRINGTOQUERY -c \
             -o trwasastrtoq.o wasastringtoquery.cpp

include $(depth)/mk/commontargets

include alldeps
