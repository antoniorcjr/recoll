/*-*-mode:c++-*-*/
/*  --------------------------------------------------------------------
 *  Filename:
 *    src/iofactory.cc
 *  
 *  Description:
 *    Implementation of the IOFactory class.
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
#include "iofactory.h"
#include "iodevice.h"

#ifndef NO_NAMESPACES
using namespace ::Binc;
using namespace ::std;
#endif /* NO_NAMESPACES */

//------------------------------------------------------------------------
IOFactory::IOFactory(void)
{
}

//------------------------------------------------------------------------
IOFactory::~IOFactory(void)
{
}

//------------------------------------------------------------------------
IOFactory &IOFactory::getInstance(void)
{
  static IOFactory ioFactory;
  return ioFactory;
}

//------------------------------------------------------------------------
void IOFactory::addDevice(IODevice *dev)
{
  IODevice *ioDevice = IOFactory::getInstance().devices[dev->service()];

  // FIXME: Delete correct object. Now, only IODevice's destructor is
  // called, and only IODevice's memory is freed.
  if (ioDevice)
    delete ioDevice;

  IOFactory::getInstance().devices[dev->service()] = dev;
}

//------------------------------------------------------------------------
IODevice &IOFactory::getClient(void)
{
  static IODevice nulDevice;

  IOFactory &ioFactory = IOFactory::getInstance();

  if (ioFactory.devices.find("client") != ioFactory.devices.end())
    return *ioFactory.devices["client"];

  return nulDevice;
}

//------------------------------------------------------------------------
IODevice &IOFactory::getLogger(void)
{
  static IODevice nulDevice;

  IOFactory &ioFactory = IOFactory::getInstance();

  if (ioFactory.devices.find("log") != ioFactory.devices.end())
    return *ioFactory.devices["log"];
  return nulDevice;
}
