/* Copyright (C) 2012 J.F.Dockes
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

#ifdef USE_ZEITGEIST
#include "rclzg.h"

#include "debuglog.h"
#include "pathut.h"

#include <QString>
#include <QDateTime>
#include <QtZeitgeist/QtZeitgeist>
#include <QtZeitgeist/Log>
#include <QtZeitgeist/Interpretation>
#include <QtZeitgeist/Manifestation>
#include <QtZeitgeist/DataModel/Event>

// Can't see no reason why our logger couldn'
static QtZeitgeist::Log zglogger;

void zg_send_event(ZgSendType, const Rcl::Doc& doc)
{
  static int needinit = 1;
  if (needinit) {
    QtZeitgeist::init();
    needinit = 0;
  }

  // The subject is about the document
  QtZeitgeist::DataModel::Subject subject;

  subject.setUri(QString::fromLocal8Bit(doc.url.c_str()));

  // TODO: refine these
  subject.setInterpretation(QtZeitgeist::Interpretation::Subject::NFODocument);
  if (doc.ipath.empty()) 
      subject.setManifestation(QtZeitgeist::Manifestation::Subject::NFOFileDataObject);
  else
      subject.setManifestation(QtZeitgeist::Manifestation::Subject::NFOEmbeddedFileDataObject);      

  subject.setOrigin(QString::fromLocal8Bit(path_getfather(doc.url).c_str()));

  subject.setMimeType(doc.mimetype.c_str());

  string titleOrFilename;
  doc.getmeta(Rcl::Doc::keytt, &titleOrFilename);
  if (titleOrFilename.empty()) {
      doc.getmeta(Rcl::Doc::keyfn, &titleOrFilename);
  }
  subject.setText(QString::fromUtf8(titleOrFilename.c_str()));


  QtZeitgeist::DataModel::Event event;
  event.setTimestamp(QDateTime::currentDateTime());
  event.addSubject(subject);

  event.setInterpretation(QtZeitgeist::Interpretation::Event::ZGAccessEvent);
  event.setManifestation(QtZeitgeist::Manifestation::Event::ZGUserActivity);
  event.setActor("app://recoll.desktop");


  QtZeitgeist::DataModel::EventList events;
  events.push_back(event);

  LOGDEB(("zg_send_event, sending for %s %s\n", 
	  doc.mimetype.c_str(), doc.url.c_str()));
  zglogger.insertEvents(events);
}

#endif
