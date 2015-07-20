/* Copyright (C) 2004 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef TEST_EXECMD
#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include <vector>
#include <string>
#include <sstream>
#include <iostream>

#include "execmd.h"

#include "netcon.h"
#include "closefrom.h"

using namespace std;

extern char **environ;

bool ExecCmd::o_useVfork = false;

#ifdef RECOLL_DATADIR
#include "debuglog.h"
#include "smallut.h"

#else
// If compiling outside of recoll, make the file as standalone as reasonable.

#define LOGFATAL(X)
#define LOGERR(X)
#define LOGINFO(X)
#define LOGDEB(X)
#define LOGDEB0(X)
#define LOGDEB1(X)
#define LOGDEB2(X)
#define LOGDEB3(X)
#define LOGDEB4(X)

#ifndef MIN
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#endif

static void stringToTokens(const string &s, vector<string> &tokens, 
                    const string &delims = " \t", bool skipinit=true);

static void stringToTokens(const string& str, vector<string>& tokens,
		    const string& delims, bool skipinit)
{
    string::size_type startPos = 0, pos;

    // Skip initial delims, return empty if this eats all.
    if (skipinit && 
	(startPos = str.find_first_not_of(delims, 0)) == string::npos) {
	return;
    }
    while (startPos < str.size()) { 
        // Find next delimiter or end of string (end of token)
        pos = str.find_first_of(delims, startPos);

        // Add token to the vector and adjust start
	if (pos == string::npos) {
	    tokens.push_back(str.substr(startPos));
	    break;
	} else if (pos == startPos) {
	    // Dont' push empty tokens after first
	    if (tokens.empty())
		tokens.push_back(string());
	    startPos = ++pos;
	} else {
	    tokens.push_back(str.substr(startPos, pos - startPos));
	    startPos = ++pos;
	}
    }
}
#endif // RECOLL_DATADIR

/* From FreeBSD's which command */
static bool exec_is_there(const char *candidate)
{
    struct stat fin;

    /* XXX work around access(2) false positives for superuser */
    if (access(candidate, X_OK) == 0 &&
	stat(candidate, &fin) == 0 &&
	S_ISREG(fin.st_mode) &&
	(getuid() != 0 ||
	 (fin.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0)) {
	return true;
    }
    return false;
}

bool ExecCmd::which(const string& cmd, string& exepath, const char* path)
{
    if (cmd.empty()) 
	return false;
    if (cmd[0] == '/') {
	if (exec_is_there(cmd.c_str())) {
	    exepath = cmd;
	    return true;
	} else {
	    return false;
	}
    }

    const char *pp;
    if (path) {
	pp = path;
    } else {
	pp = getenv("PATH");
    }
    if (pp == 0)
	return false;

    vector<string> pels;
    stringToTokens(pp, pels, ":");
    for (vector<string>::iterator it = pels.begin(); it != pels.end(); it++) {
	if (it->empty())
	    *it = ".";
	string candidate = (it->empty() ? string(".") : *it) + "/" + cmd;
	if (exec_is_there(candidate.c_str())) {
	    exepath = candidate;
	    return true;
	}
    }
    return false;
}

void  ExecCmd::putenv(const string &ea)
{
    m_env.push_back(ea);
}

void  ExecCmd::putenv(const string &name, const string& value)
{
    string ea = name + "=" + value;
    putenv(ea);
}

static void msleep(int millis)
{
    struct timespec spec;
    spec.tv_sec = millis / 1000;
    spec.tv_nsec = (millis % 1000) * 1000000;
    nanosleep(&spec, 0);
}

/** A resource manager to ensure that execcmd cleans up if an exception is 
 *  raised in the callback, or at different places on errors occurring
 *  during method executions */
class ExecCmdRsrc {
public:
    ExecCmdRsrc(ExecCmd *parent) : m_parent(parent), m_active(true) {}
    void inactivate() {m_active = false;}
    ~ExecCmdRsrc() {
	if (!m_active || !m_parent)
	    return;
	LOGDEB1(("~ExecCmdRsrc: working. mypid: %d\n", (int)getpid()));

	// Better to close the descs first in case the child is waiting in read
	if (m_parent->m_pipein[0] >= 0)
	    close(m_parent->m_pipein[0]);
	if (m_parent->m_pipein[1] >= 0)
	    close(m_parent->m_pipein[1]);
	if (m_parent->m_pipeout[0] >= 0)
	    close(m_parent->m_pipeout[0]);
	if (m_parent->m_pipeout[1] >= 0)
	    close(m_parent->m_pipeout[1]);

	// It's apparently possible for m_pid to be > 0 and getpgid to fail. In
	// this case, we have to conclude that the child process does 
	// not exist. Not too sure what causes this, but the previous code
	// definitely tried to call killpg(-1,) from time to time.
	pid_t grp;
	if (m_parent->m_pid > 0 && (grp = getpgid(m_parent->m_pid)) > 0) {
	    LOGDEB(("ExecCmd: killpg(%d, SIGTERM)\n", grp));
            int ret = killpg(grp, SIGTERM);
	    if (ret == 0) {
		for (int i = 0; i < 3; i++) {
		    msleep(i == 0 ? 5 : (i == 1 ? 100 : 2000));
		    int status;
		    (void)waitpid(m_parent->m_pid, &status, WNOHANG);
		    if (kill(m_parent->m_pid, 0) != 0)
			break;
		    if (i == 2) {
			LOGDEB(("ExecCmd: killpg(%d, SIGKILL)\n", grp));
			killpg(grp, SIGKILL);
			(void)waitpid(m_parent->m_pid, &status, WNOHANG);
		    }
		}
	    } else {
                LOGERR(("ExecCmd: error killing process group %d: %d\n",
                        grp, errno));
            }
	}
	m_parent->m_tocmd.release();
	m_parent->m_fromcmd.release();
	pthread_sigmask(SIG_UNBLOCK, &m_parent->m_blkcld, 0);
	m_parent->reset();
    }
private:
    ExecCmd *m_parent;
    bool    m_active;
};

ExecCmd::~ExecCmd()
{
    ExecCmdRsrc(this);
}

// In child process. Set up pipes and exec command. 
// This must not return. _exit() on error.
// *** This can be called after a vfork, so no modification of the
//     process memory at all is allowed ***
// The LOGXX calls should not be there, but they occur only after "impossible"
// errors, which we would most definitely want to have a hint about.
//
// Note that any of the LOGXX calls could block on a mutex set in the
// father process, so that only absolutely exceptional conditions, 
// should be logged, for debugging and post-mortem purposes
// If one of the calls block, the problem manifests itself by 20mn
// (filter timeout) of looping on "ExecCmd::doexec: selectloop
// returned 1', because the father is waiting on the read descriptor
inline void ExecCmd::dochild(const string &cmd, const char **argv,
			     const char **envv,
			     bool has_input, bool has_output)
{
    // Start our own process group
    if (setpgid(0, getpid())) {
	LOGINFO(("ExecCmd::DOCHILD: setpgid(0, %d) failed: errno %d\n",
		 getpid(), errno));
    }

    // Restore SIGTERM to default. Really, signal handling should be
    // specified when creating the execmd. Help Recoll get rid of its
    // filter children though. To be fixed one day... Not sure that
    // all of this is needed. But an ignored sigterm and the masks are
    // normally inherited.
    if (signal(SIGTERM, SIG_DFL) == SIG_ERR) {
	//LOGERR(("ExecCmd::DOCHILD: signal() failed, errno %d\n", errno));
    }
    sigset_t sset;
    sigfillset(&sset);
    pthread_sigmask(SIG_UNBLOCK, &sset, 0);
    sigprocmask(SIG_UNBLOCK, &sset, 0);

    if (has_input) {
	close(m_pipein[1]);
	if (m_pipein[0] != 0) {
	    dup2(m_pipein[0], 0);
	    close(m_pipein[0]);
	}
    }
    if (has_output) {
	close(m_pipeout[0]);
	if (m_pipeout[1] != 1) {
	    if (dup2(m_pipeout[1], 1) < 0) {
		LOGERR(("ExecCmd::DOCHILD: dup2() failed. errno %d\n", errno));
	    }
	    if (close(m_pipeout[1]) < 0) {
		LOGERR(("ExecCmd::DOCHILD: close() failed. errno %d\n", errno));
	    }
	}
    }
    // Do we need to redirect stderr ?
    if (!m_stderrFile.empty()) {
	int fd = open(m_stderrFile.c_str(), O_WRONLY|O_CREAT
#ifdef O_APPEND
		      |O_APPEND
#endif
		      , 0600);
	if (fd < 0) {
	    close(2);
	} else {
	    if (fd != 2) {
		dup2(fd, 2);
	    }
	    lseek(2, 0, 2);
	}
    }

    // Close all descriptors except 0,1,2
    libclf_closefrom(3);

    execve(cmd.c_str(), (char *const*)argv, (char *const*)envv);
    // Hu ho. This should never happened as we checked the existence of the
    // executable before calling dochild... Until we did this, this was 
    // the chief cause of LOG mutex deadlock
    LOGERR(("ExecCmd::DOCHILD: execve(%s) failed. errno %d\n", cmd.c_str(),
	    errno));
    _exit(127);
}

int ExecCmd::startExec(const string &cmd, const vector<string>& args,
		       bool has_input, bool has_output)
{
    { // Debug and logging
	string command = cmd + " ";
	for (vector<string>::const_iterator it = args.begin();it != args.end();
	     it++) {
	    command += "{" + *it + "} ";
	}
	LOGDEB(("ExecCmd::startExec: (%d|%d) %s\n", 
		has_input, has_output, command.c_str()));
    }

    // The resource manager ensures resources are freed if we return early
    ExecCmdRsrc e(this);

    if (has_input && pipe(m_pipein) < 0) {
	LOGERR(("ExecCmd::startExec: pipe(2) failed. errno %d\n", errno));
	return -1;
    }
    if (has_output && pipe(m_pipeout) < 0) {
	LOGERR(("ExecCmd::startExec: pipe(2) failed. errno %d\n", errno));
	return -1;
    }


//////////// vfork setup section
    // We do here things that we could/should do after a fork(), but
    // not a vfork(). Does no harm to do it here in both cases, except
    // that it needs cleanup (as compared to doing it just before
    // exec()).

    // Allocate arg vector (2 more for arg0 + final 0)
    typedef const char *Ccharp;
    Ccharp *argv;
    argv = (Ccharp *)malloc((args.size()+2) * sizeof(char *));
    if (argv == 0) {
	LOGERR(("ExecCmd::doexec: malloc() failed. errno %d\n",	errno));
        return -1;
    }
    // Fill up argv
    argv[0] = cmd.c_str();
    int i = 1;
    vector<string>::const_iterator it;
    for (it = args.begin(); it != args.end(); it++) {
	argv[i++] = it->c_str();
    }
    argv[i] = 0;

    Ccharp *envv;
    int envsize;
    for (envsize = 0; ; envsize++) 
	if (environ[envsize] == 0)
	    break;
    envv = (Ccharp *)malloc((envsize + m_env.size() + 2) * sizeof(char *));
    if (envv == 0) {
	LOGERR(("ExecCmd::doexec: malloc() failed. errno %d\n",	errno));
        free(argv);
        return -1;
    }
    int eidx;
    for (eidx = 0; eidx < envsize; eidx++)
	envv[eidx] = environ[eidx];
    for (vector<string>::const_iterator it = m_env.begin(); 
	 it != m_env.end(); it++) {
	envv[eidx++] = it->c_str();
    }
    envv[eidx] = 0;

    // As we are going to use execve, not execvp, do the PATH thing.
    string exe;
    if (!which(cmd, exe)) {
        LOGERR(("ExecCmd::startExec: %s not found\n", cmd.c_str()));
        free(argv);
        free(envv);
        return -1;
    }
////////////////////////////////

    if (o_useVfork) {
	m_pid = vfork();
    } else {
	m_pid = fork();
    }
    if (m_pid < 0) {
	LOGERR(("ExecCmd::startExec: fork(2) failed. errno %d\n", errno));
	return -1;
    }
    if (m_pid == 0) {
	// e.inactivate() is not needed. As we do not return, the call
	// stack won't be unwound and destructors of local objects
	// won't be called.
	dochild(exe, argv, envv, has_input, has_output);
	// dochild does not return. Just in case...
	_exit(1);
    }

    // Father process

////////////////////
    // Vfork cleanup section
    free(argv);
    free(envv);
///////////////////

    // Set the process group for the child. This is also done in the
    // child process see wikipedia(Process_group)
    if (setpgid(m_pid, m_pid)) {
        // This can fail with EACCES if the son has already done execve 
        // (linux at least)
        LOGDEB2(("ExecCmd: father setpgid(son)(%d,%d) errno %d (ok)\n",
                 m_pid, m_pid, errno));
    }

    sigemptyset(&m_blkcld);
    sigaddset(&m_blkcld, SIGCHLD);
    pthread_sigmask(SIG_BLOCK, &m_blkcld, 0);

    if (has_input) {
	close(m_pipein[0]);
	m_pipein[0] = -1;
	NetconCli *iclicon = new NetconCli();
	iclicon->setconn(m_pipein[1]);
	m_tocmd = NetconP(iclicon);
    }
    if (has_output) {
	close(m_pipeout[1]);
	m_pipeout[1] = -1;
	NetconCli *oclicon = new NetconCli();
	oclicon->setconn(m_pipeout[0]);
	m_fromcmd = NetconP(oclicon);
    }

    /* Don't want to undo what we just did ! */
    e.inactivate();

    return 0;
}

// Netcon callback. Send data to the command's input
class ExecWriter : public NetconWorker {
public:
    ExecWriter(const string *input, ExecCmdProvide *provide) 
	: m_input(input), m_cnt(0), m_provide(provide)
    {}				    
    virtual int data(NetconData *con, Netcon::Event reason)
    {
	if (!m_input) return -1;
	LOGDEB1(("ExecWriter: input m_cnt %d input length %d\n", m_cnt, 
		 m_input->length()));
	if (m_cnt >= m_input->length()) {
	    // Fd ready for more but we got none.
	    if (m_provide) {
		m_provide->newData();
		if (m_input->empty()) {
		    return 0;
		} else {
		    m_cnt = 0;
		}
		LOGDEB2(("ExecWriter: provide m_cnt %d input length %d\n", 
			 m_cnt, m_input->length()));
	    } else {
		return 0;
	    }
	}
	int ret = con->send(m_input->c_str() + m_cnt, 
			    m_input->length() - m_cnt);
	LOGDEB2(("ExecWriter: wrote %d to command\n", ret));
	if (ret <= 0) {
	    LOGERR(("ExecWriter: data: can't write\n"));
	    return -1;
	}
	m_cnt += ret;
	return ret;
    }
private:
    const string   *m_input;
    unsigned int    m_cnt; // Current offset inside m_input
    ExecCmdProvide *m_provide;
};

// Netcon callback. Get data from the command output.
class ExecReader : public NetconWorker {
public:
    ExecReader(string *output, ExecCmdAdvise *advise) 
	: m_output(output), m_advise(advise)
    {}				    
    virtual int data(NetconData *con, Netcon::Event reason)
    {
	char buf[8192];
	int n = con->receive(buf, 8192);
	LOGDEB1(("ExecReader: got %d from command\n", n));
	if (n < 0) {
	    LOGERR(("ExecCmd::doexec: receive failed. errno %d\n", errno));
	} else if (n > 0) {
	    m_output->append(buf, n);
	    if (m_advise)
		m_advise->newData(n);
	} // else n == 0, just return
	return n;
    }
private:
    string        *m_output;
    ExecCmdAdvise *m_advise;
};


int ExecCmd::doexec(const string &cmd, const vector<string>& args,
		    const string *input, string *output)
{

    if (startExec(cmd, args, input != 0, output != 0) < 0) {
	return -1;
    }

    // Cleanup in case we return early
    ExecCmdRsrc e(this);
    SelectLoop myloop;
    int ret = 0;
    if (input || output) {
        // Setup output
	if (output) {
	    NetconCli *oclicon = dynamic_cast<NetconCli *>(m_fromcmd.getptr());
	    if (!oclicon) {
		LOGERR(("ExecCmd::doexec: no connection from command\n"));
		return -1;
	    }
	    oclicon->setcallback(RefCntr<NetconWorker>
				 (new ExecReader(output, m_advise)));
	    myloop.addselcon(m_fromcmd, Netcon::NETCONPOLL_READ);
	    // Give up ownership 
	    m_fromcmd.release();
	} 
        // Setup input
	if (input) {
	    NetconCli *iclicon = dynamic_cast<NetconCli *>(m_tocmd.getptr());
	    if (!iclicon) {
		LOGERR(("ExecCmd::doexec: no connection from command\n"));
		return -1;
	    }
	    iclicon->setcallback(RefCntr<NetconWorker>
				 (new ExecWriter(input, m_provide)));
	    myloop.addselcon(m_tocmd, Netcon::NETCONPOLL_WRITE);
	    // Give up ownership 
	    m_tocmd.release();
	}

        // Do the actual reading/writing/waiting
	myloop.setperiodichandler(0, 0, m_timeoutMs);
	while ((ret = myloop.doLoop()) > 0) {
	    LOGDEB(("ExecCmd::doexec: selectloop returned %d\n", ret));
	    if (m_advise)
		m_advise->newData(0);
	    if (m_killRequest) {
		LOGINFO(("ExecCmd::doexec: cancel request\n"));
		break;
	    }
	}
	LOGDEB0(("ExecCmd::doexec: selectloop returned %d\n", ret));
        // Check for interrupt request: we won't want to waitpid()
        if (m_advise)
            m_advise->newData(0);

        // The netcons don't take ownership of the fds: we have to close them
        // (have to do it before wait, this may be the signal the child is 
        // waiting for exiting).
        if (input) {
            close(m_pipein[1]);
            m_pipein[1] = -1;
        }
        if (output) {
            close(m_pipeout[0]);
            m_pipeout[0] = -1;
        }
    }

    // Normal return: deactivate cleaner, wait() will do the cleanup
    e.inactivate();

    int ret1 = ExecCmd::wait();
    if (ret)
	return -1;
    return ret1;
}

int ExecCmd::send(const string& data)
{
    NetconCli *con = dynamic_cast<NetconCli *>(m_tocmd.getptr());
    if (con == 0) {
	LOGERR(("ExecCmd::send: outpipe is closed\n"));
	return -1;
    }
    unsigned int nwritten = 0;
    while (nwritten < data.length()) {
	if (m_killRequest)
	    break;
	int n = con->send(data.c_str() + nwritten, data.length() - nwritten);
	if (n < 0) {
	    LOGERR(("ExecCmd::send: send failed\n"));
	    return -1;
	}
	nwritten += n;
    }
    return nwritten;
}

int ExecCmd::receive(string& data, int cnt)
{
    NetconCli *con = dynamic_cast<NetconCli *>(m_fromcmd.getptr());
    if (con == 0) {
	LOGERR(("ExecCmd::receive: inpipe is closed\n"));
	return -1;
    }
    const int BS = 4096;
    char buf[BS];
    int ntot = 0;
    do {
        int toread = cnt > 0 ? MIN(cnt - ntot, BS) : BS;
        int n = con->receive(buf, toread);
        if (n < 0) {
            LOGERR(("ExecCmd::receive: error\n"));
            return -1;
        } else if (n > 0) {
            ntot += n;
            data.append(buf, n);
        } else {
            LOGDEB(("ExecCmd::receive: got 0\n"));
            break;
        }
    } while (cnt > 0 && ntot < cnt);
    return ntot;
}

int ExecCmd::getline(string& data)
{
    NetconCli *con = dynamic_cast<NetconCli *>(m_fromcmd.getptr());
    if (con == 0) {
	LOGERR(("ExecCmd::receive: inpipe is closed\n"));
	return -1;
    }
    const int BS = 1024;
    char buf[BS];
    int n = con->getline(buf, BS);
    if (n < 0) {
	LOGERR(("ExecCmd::getline: error\n"));
    } else if (n > 0) {
	data.append(buf, n);
    } else {
	LOGDEB(("ExecCmd::getline: got 0\n"));
    }
    return n;
}

// Wait for command status and clean up all resources.
int ExecCmd::wait()
{
    ExecCmdRsrc e(this);
    int status = -1;
    if (!m_killRequest && m_pid > 0) {
	if (waitpid(m_pid, &status, 0) < 0) {
	    LOGERR(("ExecCmd::waitpid: returned -1 errno %d\n", errno));
	    status = -1;
	}
        LOGDEB(("ExecCmd::wait: got status 0x%x\n", status));
	m_pid = -1;
    }
    // Let the ExecCmdRsrc cleanup
    return status;
}

bool ExecCmd::maybereap(int *status)
{
    ExecCmdRsrc e(this);
    *status = -1;

    if (m_pid <= 0) {
	// Already waited for ??
	return true;
    }

    pid_t pid = waitpid(m_pid, status, WNOHANG);
    if (pid < 0) {
        LOGERR(("ExecCmd::maybereap: returned -1 errno %d\n", errno));
	m_pid = -1;
	return true;
    } else if (pid == 0) {
	LOGDEB1(("ExecCmd::maybereap: not exited yet\n"));
	e.inactivate();
	return false;
    } else {
        LOGDEB(("ExecCmd::maybereap: got status 0x%x\n", status));
	m_pid = -1;
	return true;
    }
}

// Static
bool ExecCmd::backtick(const std::vector<std::string> cmd, std::string& out)
{
    vector<string>::const_iterator it = cmd.begin();
    it++;
    vector<string> args(it, cmd.end());
    ExecCmd mexec;
    int status = mexec.doexec(*cmd.begin(), args, 0, &out);
    return status == 0;
}

/// ReExec class methods ///////////////////////////////////////////////////
ReExec::ReExec(int argc, char *args[])
{
    init(argc, args);
}

void ReExec::init(int argc, char *args[])
{
    for (int i = 0; i < argc; i++) {
	m_argv.push_back(args[i]);
    }
    m_cfd = open(".", 0);
    char *cd = getcwd(0, 0);
    if (cd) 
	m_curdir = cd;
    free(cd);
}

void ReExec::insertArgs(const vector<string>& args, int idx)
{
    vector<string>::iterator it, cit;
    unsigned int cmpoffset = (unsigned int)-1;

    if (idx == -1 || string::size_type(idx) >= m_argv.size()) {
	it = m_argv.end();
	if (m_argv.size() >= args.size()) {
	    cmpoffset = m_argv.size() - args.size();
	}
    } else {
	it = m_argv.begin() + idx;
	if (idx + args.size() <= m_argv.size()) {
	    cmpoffset = idx;
	}
    }

    // Check that the option is not already there
    if (cmpoffset != (unsigned int)-1) {
	bool allsame = true;
	for (unsigned int i = 0; i < args.size(); i++) {
	    if (m_argv[cmpoffset + i] != args[i]) {
		allsame = false;
		break;
	    }
	}
	if (allsame)
	    return;
    }

    m_argv.insert(it, args.begin(), args.end());
}

void ReExec::removeArg(const string& arg)
{
    for (vector<string>::iterator it = m_argv.begin(); 
	 it != m_argv.end(); it++) {
	if (*it == arg)
	    it = m_argv.erase(it);
    }
}

// Reexecute myself, as close as possible to the initial exec
void ReExec::reexec()
{

#if 0
    char *cwd;
    cwd = getcwd(0,0);
    FILE *fp = stdout; //fopen("/tmp/exectrace", "w");
    if (fp) {
	fprintf(fp, "reexec: pwd: [%s] args: ", cwd?cwd:"getcwd failed");
	for (vector<string>::const_iterator it = m_argv.begin();
	     it != m_argv.end(); it++) {
	    fprintf(fp, "[%s] ", it->c_str());
	}
	fprintf(fp, "\n");
    }
#endif

    // Execute the atexit funcs
    while (!m_atexitfuncs.empty()) {
	(m_atexitfuncs.top())();
	m_atexitfuncs.pop();
    }

    // Try to get back to the initial working directory
    if (m_cfd < 0 || fchdir(m_cfd) < 0) {
	LOGINFO(("ReExec::reexec: fchdir failed, trying chdir\n"));
	if (!m_curdir.empty() && chdir(m_curdir.c_str())) {
	    LOGERR(("ReExec::reexec: chdir failed\n"));
	}
    }

    // Close all descriptors except 0,1,2
    libclf_closefrom(3);

    // Allocate arg vector (1 more for final 0)
    typedef const char *Ccharp;
    Ccharp *argv;
    argv = (Ccharp *)malloc((m_argv.size()+1) * sizeof(char *));
    if (argv == 0) {
	LOGERR(("ExecCmd::doexec: malloc() failed. errno %d\n",	errno));
	return;
    }
	
    // Fill up argv
    int i = 0;
    vector<string>::const_iterator it;
    for (it = m_argv.begin(); it != m_argv.end(); it++) {
	argv[i++] = it->c_str();
    }
    argv[i] = 0;
    execvp(m_argv[0].c_str(), (char *const*)argv);
}


////////////////////////////////////////////////////////////////////
#else // TEST
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <string>
#include <iostream>
#include <vector>
using namespace std;

#include "debuglog.h"
#include "cancelcheck.h"
#include "execmd.h"

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_b	  0x4 
#define OPT_w     0x8
#define OPT_c     0x10
#define OPT_r     0x20

const char *data = "Une ligne de donnees\n";
class MEAdv : public ExecCmdAdvise {
public:
    ExecCmd *cmd;
    void newData(int cnt) {
	if (op_flags & OPT_c) {
	    static int  callcnt;
	    if (callcnt++ == 3) {
		throw CancelExcept();
	    }
	}
	cerr << "newData(" << cnt << ")" << endl;
	//	CancelCheck::instance().setCancel();
	//	CancelCheck::instance().checkCancel();
	//	cmd->setCancel();
    }
};

class MEPv : public ExecCmdProvide {
public:
    FILE *m_fp;
    string *m_input;
    MEPv(string *i) 
	: m_input(i)
    {
	m_fp = fopen("/etc/group", "r");
    }
    ~MEPv() {
	if (m_fp)
	    fclose(m_fp);
    }
    void newData() {
	char line[1024];
	if (m_fp && fgets(line, 1024, m_fp)) {
	    m_input->assign((const char *)line);
	} else {
	    m_input->erase();
	}
    }
};


static char *thisprog;
static char usage [] =
"trexecmd [-c|-r] cmd [arg1 arg2 ...]\n" 
" -c : test cancellation (ie: trexecmd -c sleep 1000)\n"
" -r : test reexec\n"
"trexecmd -w cmd : do the which thing\n"
;
static void Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

ReExec reexec;

int main(int argc, char *argv[])
{
    reexec.init(argc, argv);

    if (0) {
	vector<string> newargs;
	newargs.push_back("newarg");
	newargs.push_back("newarg1");
	newargs.push_back("newarg2");
	newargs.push_back("newarg3");
	newargs.push_back("newarg4");
	reexec.insertArgs(newargs, 2);
    }

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'c':	op_flags |= OPT_c; break;
	    case 'r':	op_flags |= OPT_r; break;
	    case 'w':	op_flags |= OPT_w; break;
	    default: Usage();	break;
	    }
    b1: argc--; argv++;
    }

    if (argc < 1)
	Usage();

    string cmd = *argv++; argc--;
    vector<string> l;
    while (argc > 0) {
	l.push_back(*argv++); argc--;
    }
    DebugLog::getdbl()->setloglevel(DEBDEB1);
    DebugLog::setfilename("stderr");
    signal(SIGPIPE, SIG_IGN);

    if (op_flags & OPT_r) {
	chdir("/");
        argv[0] = strdup("");
	sleep(1);
        reexec.reexec();
    }

    if (op_flags & OPT_w) {
	string path;
	if (ExecCmd::which(cmd, path)) {
	    cout << path << endl;
	    exit(0);
	} 
	exit(1);
    }
    ExecCmd mexec;
    MEAdv adv;
    adv.cmd = &mexec;
    mexec.setAdvise(&adv);
    mexec.setTimeout(5);
    mexec.setStderr("/tmp/trexecStderr");
    mexec.putenv("TESTVARIABLE1=TESTVALUE1");
    mexec.putenv("TESTVARIABLE2=TESTVALUE2");
    mexec.putenv("TESTVARIABLE3=TESTVALUE3");

    string input, output;
    //    input = data;
    string *ip = 0;
    ip = &input;

    MEPv  pv(&input);
    mexec.setProvide(&pv);

    int status = -1;
    try {
	status = mexec.doexec(cmd, l, ip, &output);
    } catch (CancelExcept) {
	cerr << "CANCELLED" << endl;
    }

    fprintf(stderr, "Status: 0x%x\n", status);
    cout << output;
    exit (status >> 8);
}
#endif // TEST
