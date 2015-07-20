/*-*-mode:c++-*-*/
/*  --------------------------------------------------------------------
 *  Filename:
 *    src/iofactory.h
 *  
 *  Description:
 *    Declaration of the IOFactory class.
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
#ifndef IOFACTORY_H_INCLUDED
#define IOFACTORY_H_INCLUDED
#include <map>
#include <string>

#include "iodevice.h"

namespace Binc {
  class IOFactory {
  public:
    ~IOFactory(void);

    static void addDevice(IODevice *dev);
    static IOFactory &getInstance(void);
    static IODevice &getClient(void);
    static IODevice &getLogger(void);

  private:
    IOFactory(void);
    
    std::map<std::string, IODevice *> devices;
  };
}

#define bincClient \
  IOFactory::getClient()

#if !defined (DEBUG)
#define bincError if (false) std::cout
#define bincWarning if (false) std::cout
#define bincDebug if (false) std::cout
#else
#define bincError \
  IOFactory::getLogger().setOutputLevel(IODevice::ErrorLevel);IOFactory::getLogger()
#define bincWarning \
  IOFactory::getLogger().setOutputLevel(IODevice::WarningLevel);IOFactory::getLogger()
#define bincDebug \
  IOFactory::getLogger().setOutputLevel(IODevice::DebugLevel);IOFactory::getLogger()
#endif

#define bincInfo \
  IOFactory::getLogger().setOutputLevel(IODevice::InfoLevel);IOFactory::getLogger()

#endif
