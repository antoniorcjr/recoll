/* Copyright (C) 2002 Jean-Francois Dockes */

// Wrapper classes for the socket interface


#ifndef TEST_NETCON
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _AIX
#include <strings.h>
#endif // _AIX

#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <map>

#ifndef NO_NAMESPACES
using namespace std;
#endif

#include "debuglog.h"
#include "netcon.h"

#ifndef SOCKLEN_T
#define SOCKLEN_T socklen_t
#endif

// Need &one, &zero for setsockopt...
static const int one = 1;
static const int zero = 0;

#define LOGSYSERR(who, call, spar)					\
    LOGERR(("%s: %s(%s) errno %d (%s)\n", who, call,			\
	    spar, errno, strerror(errno)))

#ifndef MIN
#define MIN(a,b) (a<b?a:b)
#endif
#ifndef MAX
#define MAX(a,b) (a>b?a:b)
#endif
#ifndef freeZ
#define freeZ(X) if (X) {free(X);X=0;}
#endif

#define MILLIS(OLD, NEW) ( (long)(((NEW).tv_sec - (OLD).tv_sec) * 1000 + \
				  ((NEW).tv_usec - (OLD).tv_usec) / 1000))

// Static method
// Simplified interface to 'select()'. Only use one fd, for either
// reading or writing. This is only used when not using the
// selectloop() style of network i/o.
// Note that timeo == 0 does NOT mean wait forever but no wait at all.
int Netcon::select1(int fd, int timeo, int write)
{
    int ret;
    struct timeval tv;
    fd_set rd;
    tv.tv_sec = timeo;
    tv.tv_usec =  0;
    FD_ZERO(&rd);
    FD_SET(fd, &rd);
    if (write) {
	ret = select(fd+1, 0, &rd, 0, &tv);
    } else {
	ret = select(fd+1, &rd, 0, 0, &tv);
    }
    if (!FD_ISSET(fd, &rd)) {
	LOGERR(("Netcon::select1: fd not ready after select ??\n"));
	return -1;
    }
    return ret;
}

void SelectLoop::setperiodichandler(int (*handler)(void *), void *p, int ms)
{
    m_periodichandler = handler;
    m_periodicparam = p;
    m_periodicmillis = ms;
    if (m_periodicmillis > 0)
	gettimeofday(&m_lasthdlcall, 0);
}

// Compute the appropriate timeout so that the select call returns in
// time to call the periodic routine.
void SelectLoop::periodictimeout(struct timeval *tv)
{
    // If periodic not set, the select call times out and we loop
    // after a very long time (we'd need to pass NULL to select for an
    // infinite wait, and I'm too lazy to handle it)
    if (m_periodicmillis <= 0) {
	tv->tv_sec = 10000;
	tv->tv_usec = 0;
	return;
    }

    struct timeval mtv;
    gettimeofday(&mtv, 0);
    int millis = m_periodicmillis - MILLIS(m_lasthdlcall, mtv);

    // millis <= 0 means we should have already done the thing. *dont* set the 
    // tv to 0, which means no timeout at all !
    if (millis <= 0)
	millis = 1;
    tv->tv_sec = millis / 1000;
    tv->tv_usec = (millis % 1000) * 1000;
}

// Check if it's time to call the handler. selectloop will return to
// caller if it or we return 0
int SelectLoop::maybecallperiodic()
{
    if (m_periodicmillis <= 0)
	return 1;
    struct timeval mtv;
    gettimeofday(&mtv, 0);
    int millis = m_periodicmillis - MILLIS(m_lasthdlcall, mtv);
    if (millis <= 0) {
	gettimeofday(&m_lasthdlcall, 0);
	if (m_periodichandler)
	    return m_periodichandler(m_periodicparam);
	else 
	    return 0;
    }
    return 1;
}

int SelectLoop::doLoop()
{
    for (;;) {
	if (m_selectloopDoReturn) {
	    m_selectloopDoReturn = false;
	    LOGDEB(("Netcon::selectloop: returning on request\n"));
	    return m_selectloopReturnValue;
	}
	int nfds;
	fd_set rd, wd;
	FD_ZERO(&rd);
	FD_ZERO(&wd);
	
	// Walk the netcon map and set up the read and write fd_sets
	// for select()
	nfds = 0;
	for (map<int,NetconP>::iterator it = m_polldata.begin();
	     it != m_polldata.end(); it++) {
	    NetconP &pll = it->second;
	    int fd  = it->first;
	    LOGDEB2(("Selectloop: fd %d flags 0x%x\n",fd, pll->m_wantedEvents));
	    if (pll->m_wantedEvents & Netcon::NETCONPOLL_READ) {
		FD_SET(fd, &rd);
		nfds = MAX(nfds, fd + 1);
	    }
	    if (pll->m_wantedEvents & Netcon::NETCONPOLL_WRITE) {
		FD_SET(fd, &wd);
		nfds = MAX(nfds, fd + 1);
	    }
	}

	if (nfds == 0) {
	    // This should never happen in a server as we should at least
	    // always monitor the main listening server socket. For a
	    // client, it's up to client code to avoid or process this
	    // condition.

	    // Just in case there would still be open fds in there
	    // (with no r/w flags set). Should not be needed, but safer
	    m_polldata.clear();
	    LOGDEB1(("Netcon::selectloop: no fds\n"));
	    return 0;
	}

	LOGDEB2(("Netcon::selectloop: selecting, nfds = %d\n", nfds));

	// Compute the next timeout according to what might need to be
	// done apart from waiting for data
	struct timeval tv;
	periodictimeout(&tv);
	// Wait for something to happen
	int ret = select(nfds, &rd, &wd, 0, &tv);
	LOGDEB2(("Netcon::selectloop: select returns %d\n", ret));
	if (ret < 0) {
	    LOGSYSERR("Netcon::selectloop", "select", "");
	    return -1;
	}
	if (m_periodicmillis > 0)
	    if (maybecallperiodic() <= 0)
		return 1;

	// Timeout, do it again.
	if (ret == 0) 
	    continue;

	// We don't start the fd sweep at 0, else some fds would be advantaged.
	// Note that we do an fd sweep, not a map sweep. This is
	// inefficient because the fd array may be very sparse. Otoh, the
	// map may change between 2 sweeps, so that we'd have to be smart
	// with the iterator. As the cost per unused fd is low (just 2 bit
	// flag tests), we keep it like this for now
	if (m_placetostart >= nfds) 
	    m_placetostart = 0;
	int i, fd;
	for (i = 0, fd = m_placetostart; i < nfds;i++, fd++) {
	    if (fd >= nfds)
		fd = 0;

	    int canread = FD_ISSET(fd, &rd);
	    int canwrite = FD_ISSET(fd, &wd);
	    bool none = !canread && !canwrite;
	    LOGDEB2(("Netcon::selectloop: fd %d %s %s %s\n", fd,
		    none ? "blocked" : "can" , canread ? "read" : "", 
		    canwrite ? "write" : ""));
	    if (none)
		continue;

	    map<int,NetconP>::iterator it = m_polldata.find(fd);
	    if (it == m_polldata.end()) {
		/// This should not happen actually
		LOGDEB2(("Netcon::selectloop: fd %d not found\n", fd));
		continue;
	    }

	    // Next start will be one beyond last serviced (modulo nfds)
	    m_placetostart = fd + 1;
	    NetconP &pll = it->second;
	    if (canread && pll->cando(Netcon::NETCONPOLL_READ) <= 0)
		pll->m_wantedEvents &= ~Netcon::NETCONPOLL_READ;
	    if (canwrite && pll->cando(Netcon::NETCONPOLL_WRITE) <= 0)
		pll->m_wantedEvents &= ~Netcon::NETCONPOLL_WRITE;
	    if (!(pll->m_wantedEvents & (Netcon::NETCONPOLL_WRITE|Netcon::NETCONPOLL_READ))) {
		LOGDEB0(("Netcon::selectloop: fd %d has 0x%x mask, erasing\n", 
			 it->first, it->second->m_wantedEvents));
		m_polldata.erase(it);
	    }
	} // fd sweep

    } // forever loop
    LOGERR(("SelectLoop::doLoop: got out of loop !\n"));
    return -1;
}

// Add a connection to the monitored set.
int SelectLoop::addselcon(NetconP con, int events)
{
    if (con.isNull()) return -1;
    LOGDEB1(("Netcon::addselcon: fd %d\n", con->m_fd));
    con->set_nonblock(1);
    con->setselevents(events);
    m_polldata[con->m_fd] = con;
    con->setloop(this);
    return 0;
}

// Remove a connection from the monitored set.
int SelectLoop::remselcon(NetconP con)
{
    if (con.isNull()) return -1;
    LOGDEB1(("Netcon::remselcon: fd %d\n", con->m_fd));
    map<int,NetconP>::iterator it = m_polldata.find(con->m_fd);
    if (it == m_polldata.end()) {
	LOGDEB1(("Netcon::remselcon: con not found for fd %d\n", con->m_fd));
	return -1;
    }
    con->setloop(0);
    m_polldata.erase(it);
    return 0;
}

//////////////////////////////////////////////////////////
// Base class (Netcon) methods
Netcon::~Netcon() {
    closeconn();
    if (m_peer) {
	free(m_peer);
	m_peer = 0;
    }
}

void Netcon::closeconn() 
{
    if (m_ownfd && m_fd >= 0) {
	close(m_fd);
    }
    m_fd = -1;
    m_ownfd = true;
}

char *Netcon::sterror()
{
    return strerror(errno);
}

void Netcon::setpeer(const char *hostname)
{
    if (m_peer)
	free(m_peer);
    m_peer = strdup(hostname);
}

int Netcon::settcpnodelay(int on)
{
    LOGDEB2(( "Netcon::settcpnodelay\n" ));
    if (m_fd < 0) {
	LOGERR(("Netcon::settcpnodelay: connection not opened\n"));
	return -1;
    }
    char *cp = on ? (char *)&one : (char *)&zero;
    if (setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, cp, sizeof(one)) < 0) {
	LOGSYSERR("NetconCli::settcpnodelay", "setsockopt", "TCP_NODELAY");
	return -1;
    }
    return 0;
}

// Set/reset non-blocking flag on fd
int Netcon::set_nonblock(int onoff)
{
    int  flags = fcntl(m_fd, F_GETFL, 0);
    if (flags != -1 )	{
	int newflags = onoff ? flags | O_NONBLOCK : flags & ~O_NONBLOCK;
	if (newflags != flags)
	    if (fcntl(m_fd, F_SETFL, newflags)< 0)
		return -1;
    }
    return flags;
}

/////////////////////////////////////////////////////////////////////
// Data socket (NetconData) methods

NetconData::~NetconData() {
    freeZ(m_buf);
    m_bufbase = 0;
    m_bufbytes = m_bufsize = 0;
}

int NetconData::send(const char *buf, int cnt, int expedited)
{
    LOGDEB2(("NetconData::send: fd %d cnt %d expe %d\n", m_fd, cnt, expedited));
    int flag = 0;
    if (m_fd < 0) {
	LOGERR(("NetconData::send: connection not opened\n"));
	return -1;
    }
    if (expedited) {
	LOGDEB2(("NetconData::send: expedited data, count %d bytes\n", cnt));
	flag = MSG_OOB;
    }
    int ret;
    // There is a bug in the uthread version of sendto() in FreeBSD at
    // least up to 2.2.7, so avoid using it when possible
    if (flag)
	ret = ::send(m_fd, buf, cnt, flag);
    else 
	ret = ::write(m_fd, buf, cnt);

    // Note: byte count may be different from cnt if fd is non-blocking
    if (ret < 0) {
	char fdcbuf[20];sprintf(fdcbuf, "%d", m_fd);
	LOGSYSERR("NetconData::send", "send", fdcbuf);
    }
    return ret;
}

// Test for data available
int NetconData::readready()
{
    LOGDEB2(("NetconData::readready\n"));
    if (m_fd < 0) {
	LOGERR(("NetconData::readready: connection not opened\n"));
	return -1;
    }
    return select1(m_fd, 0);
}

// Test for writable
int NetconData::writeready()
{
    LOGDEB2(("NetconData::writeready\n"));
    if (m_fd < 0) {
	LOGERR(("NetconData::writeready: connection not opened\n"));
	return -1;
    }
    return select1(m_fd, 0, 1);
}

// Receive at most cnt bytes (maybe less)
int NetconData::receive(char *buf, int cnt, int timeo)
{
    LOGDEB2(("NetconData::receive: cnt %d timeo %d m_buf 0x%x m_bufbytes %d\n",
	     cnt, timeo, m_buf, m_bufbytes));
    if (m_fd < 0) {
	LOGERR(("NetconData::receive: connection not opened\n"));
	return -1;
    }
    int fromibuf = 0;
    // Get whatever might have been left in the buffer by a previous 
    // getline, except if we're called to fill the buffer of course
    if (m_buf && m_bufbytes > 0 && (buf < m_buf || buf > m_buf + m_bufsize)) {
	fromibuf = MIN(m_bufbytes, cnt);
	memcpy(buf, m_bufbase, fromibuf);
	m_bufbytes -= fromibuf;
	m_bufbase += fromibuf;
	cnt -= fromibuf;
        LOGDEB2(("NetconData::receive: transferred %d from mbuf\n", fromibuf));
	if (cnt <= 0)
	    return fromibuf;
    }
    if (timeo > 0) {
	int ret = select1(m_fd, timeo);
	if (ret == 0) {
	    LOGDEB2(("NetconData::receive timed out\n"));
	    m_didtimo = 1;
	    return -1;
	}
	if (ret < 0) {
	    LOGSYSERR("NetconData::receive", "select", "");
	    return -1;
	}
    }
    m_didtimo = 0;
    if ((cnt = read(m_fd, buf + fromibuf, cnt)) < 0) {
	char fdcbuf[20];sprintf(fdcbuf, "%d", m_fd);
	LOGSYSERR("NetconData::receive", "read", fdcbuf);
	return -1;
    }
    LOGDEB2(("NetconData::receive: normal return, cnt %d\n", cnt));
    return fromibuf + cnt;
}

// Receive exactly cnt bytes (except for timeout)
int NetconData::doreceive(char *buf, int cnt, int timeo)
{
    int got, cur;
    LOGDEB2(("Netcon::doreceive: cnt %d, timeo %d\n", cnt, timeo));
    cur = 0;
    while (cnt > cur) {
	got = receive(buf, cnt-cur, timeo);
	LOGDEB2(("Netcon::doreceive: got %d\n", got));
	if (got < 0) {
	    return -1;
	}
	if (got == 0) {
	    return cur;
	}
	cur += got;
	buf += got;
    }
    return cur;
}

// Read data until cnt-1 characters are read or a newline is found. Add
// null char at end of buffer and return.
// As we don't know where the newline will be and it would be inefficient to
// read a character at a time, we use a buffer
// Unlike fgets, we return an integer status:
// >0: number of characters returned, not including the final 0
//  0: EOF reached, no chars transferred
// -1: error
static const int defbufsize = 200;
int NetconData::getline(char *buf, int cnt, int timeo)
{
    LOGDEB2(("NetconData::getline: cnt %d, timeo %d\n", cnt, timeo));
    if (m_buf == 0) {
	if ((m_buf = (char *)malloc(defbufsize)) == 0) {
	    LOGSYSERR("NetconData::getline: Out of mem", "malloc", "");
	    return -1;
	}
	m_bufsize = defbufsize;
	m_bufbase = m_buf;
	m_bufbytes = 0;
    }

    char *cp = buf;
    for (;;) {
	// Transfer from buffer. Have to take a lot of care to keep counts and
	// pointers consistant in all end cases
	int maxtransf = MIN(m_bufbytes, cnt-1);
	int nn = maxtransf;
	LOGDEB2(("Before loop, bufbytes %d, maxtransf %d, nn: %d\n", 
		 m_bufbytes, maxtransf, nn));
	for (nn = maxtransf; nn > 0;) {
	    // This is not pretty but we want nn to be decremented for
	    // each byte copied (even newline), and not become -1 if
	    // we go to the end. Better ways welcome!
	    nn--;
	    if ((*cp++ = *m_bufbase++) == '\n')
		break;
	}
	// Update counts
	maxtransf -= nn; // Actual count transferred
	m_bufbytes -= maxtransf;
	cnt -= maxtransf;
	LOGDEB2(("After transfer: actual transf %d cnt %d, m_bufbytes %d\n",
		 maxtransf, cnt, m_bufbytes));

	// Finished ?
	if (cnt <= 1 || (cp > buf && cp[-1] == '\n')) {
	    *cp = 0;
	    return cp - buf;
	}

	// Transfer from net
	m_bufbase = m_buf;
	m_bufbytes = receive(m_buf, m_bufsize, timeo);	
	if (m_bufbytes == 0) {
	    // EOF
	    *cp = 0;
	    return cp - buf;
	}
	if (m_bufbytes < 0) {
	    m_bufbytes = 0;
	    *cp = 0;
	    return -1;
	}
    }
}

// Called when selectloop detects that data can be read or written on
// the connection. The user callback would normally have been set
// up. If it is, call it and return. Else, perform housecleaning: read
// and discard.
int NetconData::cando(Netcon::Event reason)
{
    LOGDEB2(("NetconData::cando\n"));
    if (!m_user.isNull()) {
	return m_user->data(this, reason);
    }

    // No user callback. Clean up by ourselves
    if (reason & NETCONPOLL_READ) {
#define BS 200
	char buf[BS];
	int n;
	if ((n = receive(buf, BS)) < 0) {
	    LOGSYSERR("NetconData::cando", "receive", "");
	    return -1;
	}
	if (n == 0) {
	    // EOF
	    return 0;
	}
    }
    clearselevents(NETCONPOLL_WRITE);
    return 1;
}

///////////////////////////////////////////////////////////////////////
// Methods for a client connection (NetconCli)
int NetconCli::openconn(const char *host, unsigned int port, int timeo)
{
    int ret = -1;
    LOGDEB2(("Netconcli::openconn: host %s, port %d\n", host, port));

    closeconn();

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);

    // Server name may be host name or IP address
    int addr;
    if ((addr = inet_addr(host)) != -1) {
	memcpy(&saddr.sin_addr, &addr, sizeof(addr));
    } else {
	struct hostent *hp;
	if ((hp = gethostbyname(host)) == 0) {
	    LOGERR(("NetconCli::openconn: gethostbyname(%s) failed\n", host));
	    return -1;
	}
	memcpy(&saddr.sin_addr, hp->h_addr, hp->h_length);
    }

    if ((m_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	LOGSYSERR("NetconCli::openconn", "socket", "");
	return -1;
    }
    if (timeo > 0)
	set_nonblock(1);

    if(connect(m_fd,(struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
	if (timeo > 0) {
	    if (errno != EINPROGRESS)
		goto out;
	    if (select1(m_fd, timeo, 1) == 1)
		goto connectok;
	}
	if (m_silentconnectfailure == 0) {
	    LOGSYSERR("NetconCli", "connect", "");
	}
	goto out;
    }
 connectok:
    if (timeo > 0)
	set_nonblock(0);

    LOGDEB2(("NetconCli::connect: setting keepalive\n"));
    if (setsockopt(m_fd, SOL_SOCKET, SO_KEEPALIVE, 
		   (char *)&one, sizeof(one)) < 0) {
	LOGSYSERR("NetconCli::connect", "setsockopt", "KEEPALIVE");
    }
    setpeer(host);
    LOGDEB2(("NetconCli::openconn: connection opened ok\n"));
    ret = 0;
 out:
    if (ret < 0)
	closeconn();
    return ret;
}

// Same as previous, but get the port number from services
int NetconCli::openconn(const char *host, char *serv, int timeo)
{
    LOGDEB2(("Netconcli::openconn: host %s, serv %s\n", host, serv));

    struct servent *sp;
    if ((sp = getservbyname(serv, "tcp")) == 0) {
	LOGERR(("NetconCli::openconn: getservbyname failed for %s\n", serv));
	return -1;
    }
    // Callee expects the port number in host byte order
    return openconn(host, ntohs(sp->s_port), timeo);
}


int NetconCli::setconn(int fd)
{
    LOGDEB2(("Netconcli::setconn: fd %d\n", fd));
    closeconn();

    m_fd = fd;
    m_ownfd = false;
    setpeer("");

    return 0;
}

///////////////////////////////////////////////////////////////////////
// Methods for the main (listening) server connection 

NetconServLis::~NetconServLis() {
#ifdef NETCON_ACCESSCONTROL
    freeZ(okaddrs.intarray);
    freeZ(okmasks.intarray);
#endif 
}

#if 0
// code for dumping a struct servent
static void dump_servent(struct servent *servp) {
    fprintf(stderr, "Official name %s\n", servp->s_name);
    for (char **cpp = servp->s_aliases;*cpp;cpp++)
	fprintf(stderr, "Nickname %s\n", *cpp);
    fprintf(stderr, "Port %d\n", (int)ntohs((short)servp->s_port));
    fprintf(stderr, "Proto %s\n", servp->s_proto);
}
#endif

// Set up service.
int NetconServLis::openservice(char *serv, int backlog)
{
    int	port;
    struct servent	*servp;
    LOGDEB1(("NetconServLis::openservice: serv %s\n", serv));
#ifdef NETCON_ACCESSCONTROL
    if (initperms(serv) < 0) 
	return -1;
#endif
    if ((servp = getservbyname(serv, "tcp")) == 0) {
	LOGERR(("NetconServLis::openservice: getservbyname failed for %s\n",serv));
	return -1;
    }
    port = (int)ntohs((short)servp->s_port);
    return openservice(port, backlog);
}

// Port is a natural host integer value
int NetconServLis::openservice(int port, int backlog)
{
    LOGDEB1(("NetconServLis::openservice: port %d\n", port));
#ifdef NETCON_ACCESSCONTROL
    if (initperms(port) < 0) 
	return -1;
#endif
    int ret = -1;
    struct sockaddr_in	ipaddr;
    if ((m_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	LOGSYSERR("NetconServLis", "socket", "");
	return -1;
    }
    (void) setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR,(char *)&one, sizeof(one));
#ifdef SO_REUSEPORT
    (void) setsockopt(m_fd, SOL_SOCKET, SO_REUSEPORT,(char *)&one, sizeof(one));
#endif /*SO_REUSEPORT*/
    memset(&ipaddr, 0, sizeof(ipaddr));
    ipaddr.sin_family = AF_INET;
    ipaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    ipaddr.sin_port = htons((short)port);
    if (bind(m_fd, (struct sockaddr *)&ipaddr, sizeof(ipaddr)) < 0){
	LOGSYSERR("NetconServLis", "bind", "");
	goto out;
    }
    if (listen(m_fd, backlog) < 0) {
	LOGSYSERR("NetconServLis", "listen", "");
	goto out;
    }

    LOGDEB1(("NetconServLis::openservice: service opened ok\n"));
    ret = 0;
 out:
    if (ret < 0 && m_fd >= 0) {
	close(m_fd);
	m_fd = -1;
    }
    return ret;
}

#ifdef NETCON_ACCESSCONTROL
int NetconServLis::initperms(int port)
{
    if (permsinit)
	return 0;

    char sport[30];
    sprintf(sport, "%d", port);
    return initperms(sport);
}

// Get authorized address lists from parameter file. This is disabled for now
int NetconServLis::initperms(char *serv)
{
    if (permsinit)
	return 0;

    if (serv == 0 || *serv == 0 || strlen(serv) > 80) {
	LOGERR(("NetconServLis::initperms: bad service name %s\n", serv));
	return -1;
    }

    char keyname[100];
    sprintf(keyname, "%s_okaddrs", serv);
    if (genparams->getparam(keyname, &okaddrs, 1) < 0) {
	serv = "default";
	sprintf(keyname, "%s_okaddrs", serv);
	if (genparams->getparam(keyname, &okaddrs) < 0) {
	    LOGERR(("NetconServLis::initperms: no okaddrs found in config file\n"));
	    return -1;
	}
    }
    sprintf(keyname, "%s_okmasks", serv);
    if (genparams->getparam(keyname, &okmasks)) {
	LOGERR(("NetconServLis::initperms: okmasks not found\n"));
	return -1;
    }
    if (okaddrs.len == 0 || okmasks.len == 0) {
	LOGERR(("NetconServLis::initperms: len 0 for okmasks or okaddrs\n"));
	return -1;
    }

    permsinit = 1;
    return 0;
}
#endif /* NETCON_ACCESSCONTROL */

// Sample cando routine for server master connection: delete newly
// accepted connection. What else ?
// This is to be overriden by a derived class method for an application
// using the selectloop thing
int  NetconServLis::cando(Netcon::Event reason)
{
    delete accept();
    return 1;
}

NetconServCon *
NetconServLis::accept(int timeo)
{
    LOGDEB(("NetconServLis::accept\n"));

    if (timeo > 0) {
	int ret = select1(m_fd, timeo);
	if (ret == 0) {
	    LOGDEB2(("NetconServLis::accept timed out\n"));
	    m_didtimo = 1;
	    return 0;
	}
	if (ret < 0) {
	    LOGSYSERR("NetconServLis::accept", "select", "");
	    return 0;
	}
    }
    m_didtimo = 0;

    NetconServCon *con = 0;
    int newfd = -1;
    struct sockaddr_in who;
    SOCKLEN_T clilen = (SOCKLEN_T)sizeof(who);
    if ((newfd = ::accept(m_fd, (struct sockaddr *)&who, &clilen)) < 0) {	
	LOGSYSERR("NetconServCon::accept", "accept", "");
	goto out;
    }
#ifdef NETCON_ACCESSCONTROL
    if (checkperms(&who, clilen) < 0) {
	goto out;
    }
#endif
    con = new NetconServCon(newfd);
    if (con == 0) {
	LOGERR(("NetconServLis::accept: new NetconServCon failed\n"));
	goto out;
    } 
    // Retrieve peer's host name. Errors are non fatal
    struct hostent *hp;
    if ((hp = gethostbyaddr((char *)&(who.sin_addr), sizeof(struct in_addr),
			    AF_INET)) == 0) {
	LOGERR(("NetconServLis::accept: gethostbyaddr failed for addr 0x%lx\n",
		who.sin_addr.s_addr));
	con->setpeer(inet_ntoa(who.sin_addr));
    } else {
	con->setpeer(hp->h_name);
    }
    LOGDEB2(("NetconServLis::accept: setting keepalive\n"));
    if (setsockopt(newfd, SOL_SOCKET, SO_KEEPALIVE, 
		   (char *)&one, sizeof(one)) < 0) {
	LOGSYSERR("NetconServLis::accept", "setsockopt", "KEEPALIVE");
    }
    LOGDEB2(("NetconServLis::accept: got connect from %s\n", con->getpeer()));

 out:
    if (con == 0 && newfd >= 0)
	close(newfd);
    return con;
}

#ifdef NETCON_ACCESSCONTROL
int 
NetconServLis::checkperms(void *cl, int)
{
    // If okmasks and addrs were not initialized, the default is allow to all
    if (okmasks.len <= 0 || okaddrs.len <= 0)
	return 0;

    struct sockaddr *addr = (struct sockaddr *)cl;
    unsigned long ip_addr;

    if (addr->sa_family != AF_INET) {
	LOGERR(("NetconServLis::checkperms: connection from non-INET addr !\n"));
	return -1;
    }

    ip_addr = ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr);
    LOGDEB2(("checkperms: ip_addr: 0x%x\n", ip_addr));
    for (int i = 0; i < okaddrs.len; i++) {
	unsigned int mask;
	if (i < okmasks.len)
	    mask = okmasks.intarray[i];
	else 
	    mask = okmasks.intarray[okmasks.len-1];
	LOGDEB2(("checkperms: trying okaddr 0x%x, mask 0x%x\n", 
		 okaddrs.intarray[i], mask));
	if ((ip_addr & mask) == (okaddrs.intarray[i] & mask)) 
	    return (0);
    }
    LOGERR(("NetconServLis::checkperm: connection from bad address 0x%x\n", 
	    ip_addr));
    return -1;
}
#endif /* NETCON_ACCESSCONTROL */


#else /* !TEST_NETCON */
/////////////////////////////////////////////////////////////////////////
////////// TEST DRIVER
////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include "debuglog.h"
#include "netcon.h"

static char *thisprog;
static char usage[] =
    "-c <host> <service>: Connects to trnetcon server, exchange message, then\n"
    "  sleeps 10 S, except if option -n is given (sleep forever)\n"
    "\n"
    "-s <service>: open service <service>\n"
    ;
static void 
Usage()
{
    fprintf(stderr, "Usage : %s:\n %s", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_s	  0x2 /* Server */
#define OPT_c	  0x4 /* Client */
#define OPT_n	  0x8  /* Client sleeps forever */

extern int trycli(char *host, char *serv);
extern int tryserv(char *serv);

int nloop = 10;

int main(int argc, char **argv)
{
    char *host, *serv;

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 's':
		op_flags |= OPT_s;
		break;
	    case 'c':
		op_flags |= OPT_c;
		break;
	    case 'n':
		op_flags |= OPT_n;
		break;
	    default:
		Usage();
		break;
	    }
	argc--;
	argv++;
    }
    DebugLog::setfilename("stderr");
    DebugLog::getdbl()->setloglevel(DEBDEB2);

    if (op_flags & OPT_c) {
	if (argc != 2) {
	    Usage();
	}
	host = *argv++;argc--;
	serv = *argv++;argc--;
	exit(trycli(host, serv));
    } else if (op_flags & OPT_s) {
	if (argc != 1) {
	    Usage();
	}
	serv = *argv++;argc--;
	exit(tryserv(serv));
    } else {
	Usage();
    }
}


static char buf[1024];
static int buflen = 1023;
static char fromcli[200];

class CliNetconWorker : public NetconWorker {
public:
    CliNetconWorker()  : m_count(0) {}
    int data(NetconData *con, Netcon::Event reason) 
    {
	LOGDEB(("clientdata\n"));
	if (reason & Netcon::NETCONPOLL_WRITE) {
	    sprintf(fromcli, "Bonjour Bonjour client %d, count %d", 
		    getpid(), m_count);
	    con->setselevents(Netcon::NETCONPOLL_READ);
	    if (con->send(fromcli, strlen(fromcli) + 1) < 0) {
		fprintf(stderr, "send failed\n");
		return -1;
	    }
	    m_count++;
	}

	if (reason & Netcon::NETCONPOLL_READ) {
	    con->setselevents(Netcon::NETCONPOLL_WRITE);
	    int n;
	    if ((n = con->receive(buf, buflen)) < 0) {
		fprintf(stderr, "receive failed\n");
		return -1;
	    }
	    if (n == 0) {
		// EOF, close connection
		return -1;
	    }
	    buf[n] = 0;
	    fprintf(stderr, "%d received \"%s\"\n", getpid(), buf);
	    if (op_flags & OPT_n) {
		pause();
	    } else {
		sleep(1);
	    }
	}
	if (m_count >= 10) {
	    fprintf(stderr, "Did 10, enough\n");
	    if (con->getloop())
		con->getloop()->loopReturn(0);
	}
	return 0;
    }
private: 
    int m_count;
};

int trycli(char *host, char *serv)
{
    sprintf(fromcli, "Bonjour Bonjour je suis le client %d", getpid());

    NetconCli *clicon = new NetconCli();
    NetconP con(clicon);
    if (con.isNull()) {
	fprintf(stderr, "new NetconCli failed\n");
	return 1;
    }
    if (clicon->openconn(host, serv) < 0) {
	fprintf(stderr, "openconn(%s, %s) failed\n", host, serv);
	return 1;
    }
    fprintf(stderr, "openconn(%s, %s) ok\n", host, serv);
#ifdef NOSELLOOP
    for (int i = 0;i < nloop;i++) {
	if (con->send(fromcli, strlen(fromcli) + 1) < 0) {
	    fprintf(stderr, "%d: Send failed\n", getpid());
	    return 1;
	}
	if (con->receive(buf, buflen) < 0) {
	    perror("receive:");
	    fprintf(stderr, "%d: Receive failed\n", getpid());
	    return 1;
	}
	fprintf(stderr, "%d Received \"%s\"\n", getpid(), buf);
	if (op_flags & OPT_n) {
	    pause();
	} else {
	    sleep(1);
	}
    }
#else
    RefCntr<NetconWorker> worker = 
	RefCntr<NetconWorker>(new CliNetconWorker());
    clicon->setcallback(worker);
    SelectLoop myloop;
    myloop.addselcon(con, Netcon::NETCONPOLL_WRITE);
    fprintf(stderr, "client ready\n");
    int ret = myloop.doLoop();
    if (ret < 0) {
	fprintf(stderr, "selectloop failed\n");
	exit(1);
    }
    fprintf(stderr, "selectloop returned %d\n", ret);
#endif
    return 0;
}

//////////////////////////////////////////////////////////////////
// Server-side sample code
class ServNetconWorker : public NetconWorker {
public:
    ServNetconWorker() : m_count(0) {}
    int data(NetconData *con, Netcon::Event reason) 
    {
	LOGDEB(("serverdata\n"));
	if (reason & Netcon::NETCONPOLL_WRITE) {
	    con->setselevents(Netcon::NETCONPOLL_READ);
	    char fromserv[200];
	    sprintf(fromserv, 
		    "Du serveur: mon fd pour ce client est  %d, mon compte %d",
		    con->getfd(), ++m_count);
	    if (con->send(fromserv, strlen(fromserv) + 1) < 0) {
		fprintf(stderr, "send failed\n");
		return -1;
	    }
	}
	if (reason & Netcon::NETCONPOLL_READ) {
#define LL 200
	    char buf[LL+1];
	    int n;
	    if ((n = con->receive(buf, LL)) < 0) {
		fprintf(stderr, "receive failed\n");
		return -1;
	    }
	    if (n == 0) {
		return -1;
	    }
	    buf[n] = 0;
	    fprintf(stderr, "%d received \"%s\"\n", getpid(), buf);
	    con->setselevents(Netcon::NETCONPOLL_READ|Netcon::NETCONPOLL_WRITE);
	}
	return 0;
    }
private:
    int m_count;
};

class MyNetconServLis : public NetconServLis {
public:
    MyNetconServLis(SelectLoop &loop)
	: NetconServLis(), m_loop(loop)
    {
    }
protected:
    int cando(Netcon::Event reason) {
	NetconServCon *con = accept();
	if (con == 0)
	    return -1;
	RefCntr<NetconWorker> worker = 
	    RefCntr<NetconWorker>(new ServNetconWorker());
	con->setcallback(worker);
	m_loop.addselcon(NetconP(con), NETCONPOLL_READ);
	return 1;
    }
    SelectLoop& m_loop;
};

NetconP lis;

void
onexit(int sig)
{
    fprintf(stderr, "Onexit: ");
    if (sig == SIGQUIT)
	kill(getpid(), SIGKILL);
    fprintf(stderr, "Exiting\n");
    exit(0);
}

int tryserv(char *serv)
{
    signal(SIGCHLD, SIG_IGN);
    SelectLoop myloop;
    MyNetconServLis *servlis = new MyNetconServLis(myloop);
    lis = NetconP(servlis);
    if (lis.isNull()) {
	fprintf(stderr, "new NetconServLis failed\n");
	return 1;
    }

    // Prepare for cleanup
    struct sigaction sa;
    sa.sa_flags = 0;
    sa.sa_handler = onexit;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGQUIT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);

    if (servlis->openservice(serv) < 0) {
	fprintf(stderr, "openservice(%s) failed\n", serv);
	return 1;
    }
    myloop.addselcon(lis, Netcon::NETCONPOLL_READ);
    fprintf(stderr, "openservice(%s) Ok\n", serv);
    if (myloop.doLoop() < 0) {
	fprintf(stderr, "selectloop failed\n");
	exit(1);
    }
    return 0;
}

#endif /* TEST_NETCON */
