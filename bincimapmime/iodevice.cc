/*-*-mode:c++-*-*/
/*  --------------------------------------------------------------------
 *  Filename:
 *    src/iodevice.cc
 *  
 *  Description:
 *    Implementation of the IODevice class.
 *  --------------------------------------------------------------------
 *  Copyright 2002, 2003 Andreas Aardal Hanssen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *  --------------------------------------------------------------------
 */
#include "iodevice.h"
#include "convert.h" // BincStream
//#include "session.h" // getEnv/hasEnv

#include <stdlib.h>
#include <unistd.h>

#ifndef NO_NAMESPACES
using namespace ::std;
using namespace ::Binc;
#endif /* NO_NAMESPACES */

//------------------------------------------------------------------------
IODevice::IODevice(int f) : flags(f | IsEnabled),
			      maxInputBufferSize(0),
			      maxOutputBufferSize(0),
			      timeout(0), 
			      readCount(0), writeCount(0),
			      outputLevel(ErrorLevel),
			      outputLevelLimit(ErrorLevel),
			      error(Unknown), errorString("Unknown error"),
			      dumpfd(0)
{
}

//------------------------------------------------------------------------
IODevice::~IODevice(void)
{
}

//------------------------------------------------------------------------
IODevice &IODevice::operator <<(ostream &(*source)(ostream &))
{
  if (!(flags & IsEnabled) || outputLevel > outputLevelLimit)
    return *this;

  static std::ostream &(*endl_funcptr)(ostream &) = endl;
  
  if (source != endl_funcptr)
    return *this;

  outputBuffer << "\r\n";

  if (dumpfd)
    ::write(dumpfd, "\r\n", 2);

  if (flags & FlushesOnEndl)
    flush();
  else if (flags & HasOutputLimit)
    if (outputBuffer.getSize() > maxOutputBufferSize)
      flush();

  return *this;
}

//------------------------------------------------------------------------
bool IODevice::canRead(void) const
{
  return false;
}

//------------------------------------------------------------------------
void IODevice::clear()
{
  if (!(flags & IsEnabled))
    return;

  inputBuffer.clear();
  outputBuffer.clear();
}

//------------------------------------------------------------------------
bool IODevice::flush()
{
  if (!(flags & IsEnabled))
    return true;

  WriteResult writeResult = WriteWait;
  do {
    unsigned int s = outputBuffer.getSize();
    if (s == 0)
      break;

    if (!waitForWrite())
      return false;
    
    writeResult = write();
    if (writeResult == WriteError)
      return false;

    writeCount += s - outputBuffer.getSize();
  } while (outputBuffer.getSize() > 0 && writeResult == WriteWait);

  outputBuffer.clear();
  return true;
}

//------------------------------------------------------------------------
void IODevice::setFlags(unsigned int f)
{
  flags |= f;
}

//------------------------------------------------------------------------
void IODevice::clearFlags(unsigned int f)
{
  flags &= ~f;
}

//------------------------------------------------------------------------
void IODevice::setMaxInputBufferSize(unsigned int max)
{
  maxInputBufferSize = max;
}

//------------------------------------------------------------------------
void IODevice::setMaxOutputBufferSize(unsigned int max)
{
  maxOutputBufferSize = max;
}

//------------------------------------------------------------------------
void IODevice::setTimeout(unsigned int t)
{
  timeout = t;

  if (t)
    flags |= HasTimeout;
  else
    flags &= ~HasTimeout;
}

//------------------------------------------------------------------------
unsigned int IODevice::getTimeout(void) const
{
  return timeout;
}

//------------------------------------------------------------------------
void IODevice::setOutputLevel(LogLevel level)
{
  outputLevel = level;
}

//------------------------------------------------------------------------
IODevice::LogLevel IODevice::getOutputLevel(void) const
{
  return outputLevel;
}

//------------------------------------------------------------------------
void IODevice::setOutputLevelLimit(LogLevel level)
{
  outputLevelLimit = level;
}

//------------------------------------------------------------------------
IODevice::LogLevel IODevice::getOutputLevelLimit(void) const
{
  return outputLevelLimit;
}

//------------------------------------------------------------------------
bool IODevice::readStr(string *dest, unsigned int max)
{
  // If max is 0, fill the input buffer once only if it's empty.
  if (!max && inputBuffer.getSize() == 0 && !fillInputBuffer())
    return false;

  // If max is != 0, wait until we have max.
  while (max && inputBuffer.getSize() < max) {
      if (!fillInputBuffer())
	  return false;
  }

  unsigned int bytesToRead = max ? max : inputBuffer.getSize();
  *dest += inputBuffer.str().substr(0, bytesToRead);
  if (dumpfd) {
      ::write(dumpfd, inputBuffer.str().substr(0, bytesToRead).c_str(), 
	      bytesToRead);
  }
  
  inputBuffer.popString(bytesToRead);
  readCount += bytesToRead;
  return true;
}

//------------------------------------------------------------------------
bool IODevice::readChar(char *dest)
{
  if (inputBuffer.getSize() == 0 && !fillInputBuffer())
    return false;

  char c = inputBuffer.popChar();
  if (dest)
    *dest = c;
  if (dumpfd)
    ::write(dumpfd, &c, 1);

  ++readCount;
  return true;
}

//------------------------------------------------------------------------
void IODevice::unreadChar(char c)
{
  inputBuffer.unpopChar(c);
}

//------------------------------------------------------------------------
void IODevice::unreadStr(const string &s)
{
  inputBuffer.unpopStr(s);
}

//------------------------------------------------------------------------
bool IODevice::skipTo(char c)
{
  char dest = '\0';
  do {
    if (!readChar(&dest))
      return false;
    if (dumpfd)
      ::write(dumpfd, &dest, 1);
  } while (c != dest);

  return true;
}

//------------------------------------------------------------------------
string IODevice::service(void) const
{
  return "nul";
}

//------------------------------------------------------------------------
bool IODevice::waitForWrite(void) const
{
  return false;
}

//------------------------------------------------------------------------
bool IODevice::waitForRead(void) const
{
  return false;
}

//------------------------------------------------------------------------
IODevice::WriteResult IODevice::write(void)
{
  return WriteError;
}

//------------------------------------------------------------------------
bool IODevice::fillInputBuffer(void)
{
  return false;
}

//------------------------------------------------------------------------
IODevice::Error IODevice::getLastError(void) const
{
  return error;
}

//------------------------------------------------------------------------
string IODevice::getLastErrorString(void) const
{
  return errorString;
}

//------------------------------------------------------------------------
unsigned int IODevice::getReadCount(void) const
{
  return readCount;
}

//------------------------------------------------------------------------
unsigned int IODevice::getWriteCount(void) const
{
  return writeCount;
}

//------------------------------------------------------------------------
void IODevice::enableProtocolDumping(void)
{
#if 0
  BincStream ss;
  ss << "/tmp/bincimap-dump-" << (int) time(0) << "-" 
     << Session::getInstance().getIP() << "-XXXXXX";
  char *safename = strdup(ss.str().c_str());
  dumpfd = mkstemp(safename);
  if (dumpfd == -1)
    dumpfd = 0;
  delete safename;
#endif
}
