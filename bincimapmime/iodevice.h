/*-*-mode:c++;c-basic-offset:2-*-*/
/*  --------------------------------------------------------------------
 *  Filename:
 *    src/iodevice.h
 *  
 *  Description:
 *    Declaration of the IODevice class.
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
#ifndef iodevice_h_included
#define iodevice_h_included
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "convert.h" // BincStream
#include <string>
#include <unistd.h> // ::write

namespace Binc {
  /*!
    \class IODevice
    \brief The IODevice class provides a framework for reading and
    writing to device.

    Implement new devices by inheriting this class and overloading all
    virtual methods.

    service() returns the service that the specific device is used
    for. Two values are "log" and "client".

    \sa IOFactory, MultilogDevice, SyslogDevice, StdIODevice, SSLDevice
  */
  class IODevice {
  public:
    /*!
      Standard options for an IODevice.
    */
    enum Flags {
      None = 0,
      FlushesOnEndl = 1 << 0,
      HasInputLimit = 1 << 1,
      HasOutputLimit = 1 << 2,
      IsEnabled = 1 << 3,
      HasTimeout = 1 << 4
    };

    /*!
      Errors from when an operation returned false.
    */
    enum Error {
      Unknown,
      Timeout
    };

    /*!
      Constructs an invalid IODevice.      

      Instances of IODevice perform no operations, and all boolean
      functions always return false. This constructor is only useful
      if called from a subclass that reimplements all virtual methods.
    */
    IODevice(int f = 0);

    /*!
      Destructs an IODevice; does nothing.
    */
    virtual ~IODevice(void);

    /*!
      Clears all data in the input and output buffers.
    */
    void clear(void);

    /*!
      Sets one or more flags.
      \param f A bitwise OR of flags from the Flags enum.
    */
    void setFlags(unsigned int f);

    /*!
      Clears one or more flags.
      \param f A bitwise OR of flags from the Flags enum.
    */
    void clearFlags(unsigned int f);

    /*!
      Sets the maximum allowed input buffer size. If this size is
      non-zero and exceeded, reading from the device will fail. This
      functionality is used to prevent clients from forcing this class
      to consume so much memory that the program crashes.

      Setting the max input buffer size to 0 disables the input size
      limit.

      \param max The maximum input buffer size in bytes.
    */
    void setMaxInputBufferSize(unsigned int max);

    /*!
      Sets the maximum allowed output buffer size. If this size is
      non-zero and exceeded, flush() is called implicitly.

      Setting the max output buffer size to 0 disables the output size
      limit. This is generally discouraged.

      As a contrast to setMaxInputBufferSize(), this function is used
      to bundle up consequent write calls, allowing more efficient use
      of the underlying device as larger blocks of data are written at
      a time.

      \param max The maximum output buffer size in bytes.
    */
    void setMaxOutputBufferSize(unsigned int max);

    /*!
      Sets the device's internal timeout in seconds. This timeout is
      used both when waiting for data to read and for waiting for the
      ability to write.

      If this timeout is exceeded, the read or write function that
      triggered the timeout will fail.

      Setting the timeout to 0 disables the timeout.

      \param t The timeout in seconds.
      \sa getTimeout()
    */
    void setTimeout(unsigned int t);

    /*!
      Returns the timeout in seconds, or 0 if there is no timeout.

      \sa setTimeout()
    */
    unsigned int getTimeout(void) const;

    enum LogLevel {
      ErrorLevel,
      InfoLevel,
      WarningLevel,
      DebugLevel
    };
    
    /*!
      Sets the output level for the following write operations on this
      device.

      The output level is a number which gives the following write
      operations a priority. You can use setOutputLevelLimit() to
      filter the write operations valid for different operating modes.
      This enables you to have certain write operations ignored.

      For instance, if the output level is set to 0, then "Hello" is
      written, and the output level is set to 1, followed by writing
      "Daisy", the output level limit value will decive wether only
      "Hello" is written, or if also "Daisy" is written.

      A low value of the level gives higher priority, and a high level
      will give low priority. The default value is 0, and write
      operations that are done with output level 0 are never ignored.

      \param level The output level
      \sa getOutputLevel(), setOutputLevelLimit()
    */
    void setOutputLevel(LogLevel level);

    /*!
      Returns the current output level.

      \sa setOutputLevel()
    */
    LogLevel getOutputLevel(void) const;

    /*!
      Sets the current output level limit. Write operations with a
      level higher than the output level limit are ignored.

      \param level The output level limit
      \sa setOutputLevel()
    */
    void setOutputLevelLimit(LogLevel level);

    /*!
      Returns the current output level limit.

      \sa setOutputLevelLimit()
    */
    LogLevel getOutputLevelLimit(void) const;

    /*!
      Returns the number of bytes that have been read from this device
      since it was created.
    */
    unsigned int getReadCount(void) const;

    /*!
      Returns the number of bytes that have been written to this
      device since it was created.
    */
    unsigned int getWriteCount(void) const;

    /*!
      Calling this function enables the built-in protocol dumping feature in
      the device. All input and output to this device will be dumped to a file
      in /tmp.
    */
    void enableProtocolDumping(void);

    /*!
      Writes data to the device. Depending on the value of the max
      output buffer size, the data may not be written immediately.

      \sa setMaxOutputBufferSize()
    */
    template <class T> IODevice &operator <<(const T &source);

    /*!
      Writes data to the device. This function specializes on standard
      ostream derivates, such as std::endl.
     */
    IODevice &operator <<(std::ostream &(*source)(std::ostream &));

    /*!
      Returns true if data can be read from the device; otherwise
      returns false.
    */
    virtual bool canRead(void) const;

    /*!
      Reads data from the device, and stores this in a string. Returns
      true on success; otherwise returns false.
      
      \param dest The incoming data is stored in this string.
      \param max No more than this number of bytes is read from the
      device.
    */
    bool readStr(std::string *dest, unsigned int max = 0);

    /*!
      Reads exactly one byte from the device and stores this in a
      char. Returns true on success; otherwise returns false.

      \param dest The incoming byte is stored in this char.
    */
    bool readChar(char *dest = 0);

    /*!
      FIXME: add docs
    */
    void unreadChar(char c);

    /*!
      FIXME: add docs
    */
    void unreadStr(const std::string &s);

    /*!
      Reads characters from the device, until and including one
      certain character is found. All read characters are discarded.

      This function can be used to skip to the beginning of a line,
      with the terminating character being '\n'.

      \param The certain character.
    */
    bool skipTo(char c);

    /*!
      Flushes the output buffer. Writes all data in the output buffer
      to the device.
    */
    bool flush(void);

    /*!
      Returns the type of error that most recently occurred.
    */
    Error getLastError(void) const;

    /*!
      Returns a human readable description of the error that most
      recently occurred. If no known error has occurred, this method
      returns "Unknown error".
    */
    std::string getLastErrorString(void) const;

    /*!
      Returns the type of service provided by this device. Two valid
      return values are "client" and "log".
    */
    virtual std::string service(void) const;
    
  protected:
    /*!
      Waits until data can be written to the device. If the timeout is
      0, this function waits indefinitely. Otherwise, it waits until
      the timeout has expired.

      If this function returns true, data can be written to the
      device; otherwise, getLastError() must be checked to determine
      whether a timeout occurred or whether an error with the device
      prevents further writing.
    */
    virtual bool waitForWrite(void) const;

    /*!
      Waits until data can be read from the device.

      \sa waitForWrite()
    */
    virtual bool waitForRead(void) const;

    /*!
      Types of results from a write. 
    */
    enum WriteResult {
      WriteWait = 0,
      WriteDone = 1 << 0,
      WriteError = 1 << 1
    };

    /*!
      Writes as much data as possible to the device. If some but not
      all data was written, returns WriteWait. If all data was
      written, returns WriteDone.  If an error occurred, returns
      WriteError.
    */
    virtual WriteResult write(void);

    /*!
      Reads data from the device, and stores it in the input buffer.
      Returns true on success; otherwise returns false.
      
      This method will fail if there is no more data available, if a
      timeout occurred or if an error with the device prevents more
      data from being read.

      The number of bytes read from the device is undefined.
    */
    virtual bool fillInputBuffer(void);

    BincStream inputBuffer;
    BincStream outputBuffer;

  protected:
    unsigned int flags;
    unsigned int maxInputBufferSize;
    unsigned int maxOutputBufferSize;

    unsigned int timeout;

    unsigned int readCount;
    unsigned int writeCount;

    LogLevel outputLevel;
    LogLevel outputLevelLimit;

    mutable Error error;
    mutable std::string errorString;

    int dumpfd;
  };

  //----------------------------------------------------------------------
  template <class T> IODevice &IODevice::operator <<(const T &source)
  {
    if ((flags & IsEnabled) && outputLevel <= outputLevelLimit) {
      outputBuffer << source;

      if (dumpfd) {
	BincStream ss;
	ss << source;
	::write(dumpfd, ss.str().c_str(), ss.getSize());
      }

      if (flags & HasInputLimit)
	if (outputBuffer.getSize() > maxOutputBufferSize)
	  flush();
    }

    return *this;
  }
}

#endif
