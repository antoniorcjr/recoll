/*
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
#ifndef _DEBUGLOG_H_
#define _DEBUGLOG_H_
/* Macros for log and debug messages */
#include <stack>

namespace DebugLog {

#ifndef DEBUGLOG_USE_THREADS
#define DEBUGLOG_USE_THREADS 1
#endif

#define DEBFATAL 1
#define DEBERR   2
#define DEBINFO  3
#define DEBDEB   4
#define DEBDEB0  5
#define DEBDEB1  6
#define DEBDEB2  7
#define DEBDEB3  8

#ifndef STATICVERBOSITY
#define STATICVERBOSITY DEBDEB0
#endif

class DebugLogWriter;

class DebugLog {
    std::stack<int> levels;
    int debuglevel;
    int dodate;
    DebugLogWriter *writer;
    bool  fileyes;
  public:
    DebugLog() : debuglevel(10), dodate(0), writer(0), fileyes(true) {}
    DebugLog(DebugLogWriter *w) : debuglevel(-1), dodate(0), writer(w),
				  fileyes(true) {}
    virtual ~DebugLog() {}
    virtual void setwriter(DebugLogWriter *w) {writer = w;}
    virtual DebugLogWriter *getwriter() {return writer;}
    virtual void prolog(int lev, const char *srcfname, int line);
    virtual void log(const char *s ...);
    virtual void setloglevel(int lev);
    inline int getlevel() {return debuglevel;}
    virtual void pushlevel(int lev);
    virtual void poplevel();
    virtual void logdate(int onoff) {dodate = onoff;}
    static bool isspecialname(const char *logname);
};

extern DebugLog *getdbl();
extern const char *getfilename();
extern int setfilename(const char *fname, int trnc = 1);
extern int reopen();

#if STATICVERBOSITY >= DEBFATAL
#define LOGFATAL(X) {if (DebugLog::getdbl()->getlevel()>=DEBFATAL){DebugLog::getdbl()->prolog(DEBFATAL,__FILE__,__LINE__) ;DebugLog::getdbl()->log X;}}
#else
#define LOGFATAL(X)
#endif
#if STATICVERBOSITY >= DEBERR
#define LOGERR(X) {if (DebugLog::getdbl()->getlevel()>=DEBERR){DebugLog::getdbl()->prolog(DEBERR,__FILE__,__LINE__) ;DebugLog::getdbl()->log X;}}
#else
#define LOGERR(X)
#endif
#if STATICVERBOSITY >= DEBINFO
#define LOGINFO(X) {if (DebugLog::getdbl()->getlevel()>=DEBINFO){DebugLog::getdbl()->prolog(DEBINFO,__FILE__,__LINE__) ;DebugLog::getdbl()->log X;}}
#else
#define LOGINFO(X)
#endif
#if STATICVERBOSITY >= DEBDEB
#define LOGDEB(X) {if (DebugLog::getdbl()->getlevel()>=DEBDEB){DebugLog::getdbl()->prolog(DEBDEB,__FILE__,__LINE__) ;DebugLog::getdbl()->log X;}}
#else
#define LOGDEB(X)
#endif
#if STATICVERBOSITY >= DEBDEB0
#define LOGDEB0(X) {if (DebugLog::getdbl()->getlevel()>=DEBDEB0){DebugLog::getdbl()->prolog(DEBDEB0,__FILE__,__LINE__) ;DebugLog::getdbl()->log X;}}
#else
#define LOGDEB0(X)
#endif
#if STATICVERBOSITY >= DEBDEB1
#define LOGDEB1(X) {if (DebugLog::getdbl()->getlevel()>=DEBDEB1){DebugLog::getdbl()->prolog(DEBDEB1,__FILE__,__LINE__) ;DebugLog::getdbl()->log X;}}
#else
#define LOGDEB1(X)
#endif
#if STATICVERBOSITY >= DEBDEB2
#define LOGDEB2(X) {if (DebugLog::getdbl()->getlevel()>=DEBDEB2){DebugLog::getdbl()->prolog(DEBDEB2,__FILE__,__LINE__) ;DebugLog::getdbl()->log X;}}
#else
#define LOGDEB2(X)
#endif
#if STATICVERBOSITY >= DEBDEB3
#define LOGDEB3(X) {if (DebugLog::getdbl()->getlevel()>=DEBDEB3){DebugLog::getdbl()->prolog(DEBDEB3,__FILE__,__LINE__) ;DebugLog::getdbl()->log X;}}
#else
#define LOGDEB3(X)
#endif
}
#endif /* _DEBUGLOG_H_ */
