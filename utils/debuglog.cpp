/* Copyright (C) 2006 J.F.Dockes 
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
#ifndef TEST_DEBUGLOG

#define __USE_GNU
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#ifdef INCLUDE_NEW_H
#include <new.h>
#endif

#include <string>
#include <set>
using std::set;
using std::string;

#include "debuglog.h"
#include "pathut.h"
#include "smallut.h"
#include "ptmutex.h"

#ifndef freeZ 
#define freeZ(X) {if (X) {free(X);X=0;}}
#endif

#ifndef NO_NAMESPACES
using namespace std;
namespace DebugLog {

#endif // NO_NAMESPACES

bool DebugLog::isspecialname(const char *logname) 
{
    return !strcmp(logname, "stdout") || !strcmp(logname, "stderr");
}

class DebugLogWriter {
  public:
    virtual ~DebugLogWriter() {}
    virtual int put(const char *s) = 0;
};

class DLFWImpl {
    char *filename;
    FILE *fp;
    int truncate;
 public:
    // Open output file if needed, return 0 if ok
    void maybeopenfp() {
	if (fp)
	    return;
	if (filename == 0)
	    return;
	if (!strcmp(filename, "stdout")) {
	    fp = stdout;
	} else if (!strcmp(filename, "stderr")) {
	    fp = stderr;
	} else {
	    fp = fopen(filename, (truncate) ? "w" : "a");
	    if (fp) {
		setvbuf(fp, 0, _IOLBF, 0);
#ifdef O_APPEND
		{
		    int flgs = 0;
		    fcntl(fileno(fp), F_GETFL, &flgs);
		    fcntl(fileno(fp), F_SETFL, flgs|O_APPEND);
		}
#endif
	    }
	}
	return;
    }

    void maybeclosefp() { 
#ifdef DEBUGDEBUG
	fprintf(stderr, "DebugLogImpl::maybeclosefp: filename %p, fp %p\n",
		filename, fp);
#endif
	// Close current file if open, and not stdout/stderr
	if (fp && (filename == 0 || 
		   (strcmp(filename, "stdout") && 
		    strcmp(filename, "stderr")))) {
	    fclose(fp);
	}
	fp = 0; 
	freeZ(filename);
    }

 public:

    DLFWImpl() 
	: filename(0), fp(0), truncate(1) 
    {
	setfilename("stderr", 0);
    }
    ~DLFWImpl() { 
	maybeclosefp();
    }
    int setfilename(const char *fn, int trnc) {
	maybeclosefp();
	filename = strdup(fn);
	truncate = trnc;
	maybeopenfp();
	return 0;
    }
    const char *getfilename() {
	return filename;
    }
    int put(const char *s) {
	maybeopenfp();
	if (fp)
	    return fputs(s, fp);
	return -1;
    }
};

class DebugLogFileWriter : public DebugLogWriter {
    DLFWImpl *impl;
    PTMutexInit loglock;
  public:
    DebugLogFileWriter()
    {
	impl = new DLFWImpl;
    }

    virtual ~DebugLogFileWriter() 
    { 
	delete impl;
    }

    virtual int setfilename(const char *fn, int trnc) 
    {
	PTMutexLocker lock(loglock);
	return impl ? impl->setfilename(fn, trnc) : -1;
    }
    virtual const char *getfilename() 
    {
	PTMutexLocker lock(loglock);
	return impl ? impl->getfilename() : 0;
    }
    virtual int reopen()
    {
	PTMutexLocker lock(loglock);
	if (!impl)
	    return -1;
	string fn = impl->getfilename();
	return impl->setfilename(fn.c_str(), 1);
    }
    virtual int put(const char *s) 
    {
	PTMutexLocker lock(loglock);
	return impl ? impl->put(s) : -1;
    };
};


static set<string> yesfiles;
static void initfiles()
{
    const char *cp = getenv("DEBUGLOG_FILES");
    if (!cp)
	return;
    vector<string> files;
    stringToTokens(cp, files, ",");
    yesfiles.insert(files.begin(), files.end());
}
static bool fileInFiles(const string& file)
{
    string sf = path_getsimple(file);
    if (yesfiles.find(sf) != yesfiles.end()) {
	//fprintf(stderr, "Debug ON: %s \n", file.c_str());
	return true;
    }
    //fprintf(stderr, "Debug OFF: %s \n", file.c_str());
    return false;
}

#ifdef _WINDOWS
#include <windows.h>
static void datestring(char *d, int sz) {
    SYSTEMTIME buf;
    GetLocalTime(&buf);
    int year = buf.wYear % 100;

    snprintf(d, sz, "%02d%02d%02d%02d%02d%02d", year, int(buf.wMonth),
	    int(buf.wDay), int(buf.wHour), int(buf.wMinute), int(buf.wSecond));
}
#define vsnprintf _vsnprintf

#else // !WINDOWS ->

#include <time.h>
static void datestring(char *d, int sz)
{
    struct tm *tmp;
    time_t tim = time((time_t*)0);
    tmp = localtime(&tim);
    int year = tmp->tm_year % 100;
    snprintf(d, sz, "%02d%02d%02d%02d%02d%02d", year, tmp->tm_mon+1,
	    tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
}

#endif // !WINDOWS

void 
DebugLog::prolog(int lev, const char *f, int line) 
{
    if (!writer)
	return;
    if (!yesfiles.empty() && !fileInFiles(f)) {
	fileyes = false;
	return;
    } else {
	fileyes = true;
    }
    if (dodate) {
	char dts[100];
	datestring(dts, 100);
	writer->put(dts);
    }
    char buf[100];
    sprintf(buf, ":%d:", lev);
    writer->put(buf);
#if DEBUGLOG_SHOW_PID
    sprintf(buf, "%d:", getpid());
    writer->put(buf);
#endif
#if DEBUGLOG_SHOW_THREAD
    sprintf(buf, "%lx:", (unsigned long)pthread_self());
    writer->put(buf);
#endif
    writer->put(f);
    sprintf(buf, ":%d:", line);
    writer->put(buf);
}

void 
DebugLog::log(const char *s ...)
{
    if (!writer || !fileyes)
	return;
    va_list ap;
    va_start(ap,s);

#ifdef HAVE_VASPRINTF_nono // not sure vasprintf is really such a great idea
    char *buf;
    vasprintf(&buf, s, ap);
    if (buf) {
#else
    char buf[4096];
    // It's possible that they also wouldn't have vsnprintf but what then ?
    vsnprintf(buf, 4096, s, ap);
    {
#endif
	writer->put(buf);
    }

#ifdef HAVE_VASPRINTF_nono
    if (buf)
	free(buf);
#endif
}

void 
DebugLog::setloglevel(int lev)
{
    debuglevel = lev;
    while (!levels.empty())
	levels.pop();
    pushlevel(lev);
}

void DebugLog::pushlevel(int lev)
{
    debuglevel = lev;
    levels.push(lev);
}

void DebugLog::poplevel()
{
    if (levels.empty()) 
	debuglevel = 0;
    if (levels.size() > 1)
	levels.pop();
    debuglevel =  levels.top();
}


////////////////////////////////////////////////////////////
// Global functions
//////////////////////////////////////
static DebugLogFileWriter lwriter;
static DebugLogFileWriter *theWriter = &lwriter;

const char *getfilename() 
{
    return theWriter ? theWriter->getfilename() : 0;
}

int setfilename(const char *fname, int trnc)
{
    return theWriter ? theWriter->setfilename(fname, trnc) : -1;
}

int reopen()
{
    return theWriter ? theWriter->reopen() : -1;

}

#if DEBUGLOG_USE_THREADS
#include <pthread.h>
static pthread_key_t dbl_key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

static void thrdatadel(void *data)
{
    //    fprintf(stderr, "DebugLog:: thrdatadel: %p\n", data);
    DebugLog *dbl = (DebugLog *)data;
    delete dbl;
    pthread_setspecific(dbl_key, 0);
}
static void once_routine(void)
{
    int status;
    status = pthread_key_create(&dbl_key, thrdatadel);
    if (status != 0) {
	fprintf(stderr, "debuglog: cant initialize pthread "
		"thread private storage key\n");
	abort();
    }
}

DebugLog *getdbl() 
{
    int status = pthread_once(&key_once, once_routine);
    if (status != 0) {
	fprintf(stderr, "debuglog: cant initialize pthread "
		"thread private storage key (pthread_once)\n");
	abort();
    }
    DebugLog *dbl;
    if (!(dbl = (DebugLog *)pthread_getspecific(dbl_key))) {
	if ((dbl = new DebugLog) == 0) {
	    fprintf(stderr, "debuglog: new DebugLog returned 0! ");
	    abort();
	}

	dbl->setwriter(theWriter);
	initfiles();
	status = pthread_setspecific(dbl_key, dbl);
	if (status) {
	    fprintf(stderr, "debuglog: cant initialize pthread "
		    "thread private storage key (pthread_setspecific)\n");
	    abort();
	}
    }
    return dbl;
}

#else // No threads ->

static DebugLog *dbl;
DebugLog *getdbl() 
{
    if (!dbl) {
	dbl = new DebugLog;
	dbl->setwriter(theWriter);
	initfiles();
    }
    return dbl;
}
#endif

#ifndef NO_NAMESPACES
}
#endif // NO_NAMESPACES

////////////////////////////////////////// TEST DRIVER //////////////////
#else /* TEST_DEBUGLOG */

#include <stdio.h>
#include "debuglog.h"

#if DEBUGLOG_USE_THREADS
#define TEST_THREADS
#endif

#ifdef TEST_THREADS 
#include <pthread.h>
#endif

const int iloop = 5;
void *thread_test(void *data)
{
    const char *s = (const char *)data;
    int lev = atoi(s);
    DebugLog::getdbl()->setloglevel(DEBDEB);
    for (int i = 1; i < iloop;i++) {
	switch (lev) {
	case 1:	LOGFATAL(("Thread: %s count: %d\n", s, i));break;
	case 2:	LOGERR(("Thread: %s count: %d\n", s, i));break;
	default:
	case 3:	LOGINFO(("Thread: %s count: %d\n", s, i));break;
	}
	sleep(1);
    }
    return 0;
}


int
main(int argc, char **argv)
{
#ifdef TEST_THREADS
    pthread_t t1, t2, t3;

    char name1[20];
    strcpy(name1, "1");
    pthread_create(&t1, 0, thread_test, name1);

    char name2[20];
    strcpy(name2, "2");
    pthread_create(&t2, 0, thread_test, name2);

    char name3[20];
    strcpy(name3, "3");
    pthread_create(&t3, 0, thread_test, name3);

    DebugLog::getdbl()->setloglevel(DEBDEB);
    for (int i = 1; i < iloop;i++) {
	LOGINFO(("LOGGING FROM MAIN\n"));
	sleep(1);
    }
    sleep(2);
    exit(0);
#else
    LOGFATAL(("FATAL\n","Val"));
    DebugLog::getdbl()->logdate(1);
    LOGERR(("ERR\n","Val"));
    LOGINFO(("INFO\n","Val"));
    LOGDEB0(("DEBUG %s\n","valeur"));

    int lev;
    printf("Testing push. Initial level: %d\n", DebugLog::getdbl()->getlevel());
    for (lev = 0; lev < 4;lev++) {
	DebugLog::getdbl()->pushlevel(lev);
	printf("Lev now %d\n", DebugLog::getdbl()->getlevel());
    }
    printf("Testing pop\n");
    for (lev = 0; lev < 7;lev++) {
	DebugLog::getdbl()->poplevel();
	printf("Lev now %d\n", DebugLog::getdbl()->getlevel());
    }
#endif
}


#endif /* TEST_DEBUGLOG */
