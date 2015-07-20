/* -*- mode:c++;c-basic-offset:2 -*- */
/*  --------------------------------------------------------------------
 *  Filename:
 *    mime-printbody.cc
 *  
 *  Description:
 *    Implementation of main mime parser components
 *  --------------------------------------------------------------------
 *  Copyright 2002-2005 Andreas Aardal Hanssen
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mime.h"
#include "mime-utils.h"
#include "mime-inputsource.h"

#include "convert.h"
#include "iodevice.h"
#include "iofactory.h"

#include <string>
#include <vector>
#include <map>
#include <exception>
#include <iostream>

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

#ifndef NO_NAMESPACES
using namespace ::std;
#endif /* NO_NAMESPACES */

//------------------------------------------------------------------------
void Binc::MimePart::printBody(IODevice &output,
			       unsigned int startoffset,
			       unsigned int length) const
{
  mimeSource->reset();
  mimeSource->seek(bodystartoffsetcrlf + startoffset);

  if (startoffset + length > bodylength)
    length = bodylength - startoffset;

  char c = '\0';
  for (unsigned int i = 0; i < length; ++i) {
    if (!mimeSource->getChar(&c))
      break;

    output << (char)c;
  }
}

void Binc::MimePart::getBody(string &s,
			     unsigned int startoffset,
			     unsigned int length) const
{
  mimeSource->reset();
  mimeSource->seek(bodystartoffsetcrlf + startoffset);
  s.reserve(length);
  if (startoffset + length > bodylength)
    length = bodylength - startoffset;

  char c = '\0';
  for (unsigned int i = 0; i < length; ++i) {
    if (!mimeSource->getChar(&c))
      break;

    s += (char)c;
  }
}
