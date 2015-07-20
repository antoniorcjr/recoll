#!/usr/bin/env python
"""An example that uses python tools to parse mbox/rfcxxx format and index
messages. Not supposed to run as-is or be really useful"""

import mailbox
import email.header
import email.utils
#import sys
try:
    from recoll import recoll
except:
    import recoll

import os
import stat

mbfile = os.path.expanduser("~/mbox")
rclconf = os.path.expanduser("~/.recoll")

def header_value(msg, nm, to_utf = False):
    value = msg.get(nm)
    if value == None:
        return ""
    value = value.replace("\n", "")
    value = value.replace("\r", "")
    #print value
    parts = email.header.decode_header(value)
    #print parts
    univalue = u""
    for part in parts:
        if part[1] != None:
            univalue += unicode(part[0], part[1]) + " "
        else:
            univalue += part[0] + " "
    if to_utf:
        return univalue.encode('utf-8')
    else:
        return univalue

class mbox_indexer:
    def __init__(self, mbfile):
        self.mbfile = mbfile
        stdata = os.stat(mbfile)
        self.fmtime = stdata[stat.ST_MTIME]
        self.fbytes = stdata[stat.ST_SIZE]
        self.msgnum = 1

    def sig(self):
        return str(self.fmtime) + ":" + str(self.fbytes)
    def udi(self, msgnum):
        return self.mbfile + ":" + str(msgnum)

    def index(self, db):
        if not db.needUpdate(self.udi(1), self.sig()):
            print("Index is up to date");
            return None
        mb = mailbox.mbox(self.mbfile)
        for msg in mb.values():
            print("Indexing message %d" % self.msgnum);
            self.index_message(db, msg)
            self.msgnum += 1

    def index_message(self, db, msg):
        doc = recoll.Doc()
        doc.author = header_value(msg, "From")
        doc.recipient = header_value(msg, "To") + " " + header_value(msg, "Cc")
        # url
        doc.url = "file://" + self.mbfile
        # utf8fn
        # ipath
        doc.ipath = str(self.msgnum)
        # mimetype
        doc.mimetype = "message/rfc822"
        # mtime
        dte = header_value(msg, "Date")
        tm = email.utils.parsedate_tz(dte)
        if tm == None:
            doc.mtime = str(self.fmtime)
        else:
            doc.mtime = str(email.utils.mktime_tz(tm))
        # origcharset
        # title
        doc.title = header_value(msg, "Subject")
        # keywords
        # abstract
        # author
        # fbytes
        doc.fbytes = str(self.fbytes)
        # text
        text = u""
        text += u"From: " + header_value(msg, "From") + u"\n"
        text += u"To: " + header_value(msg, "To") + u"\n"
        text += u"Subject: " + header_value(msg, "Subject") + u"\n"
        #text += u"Message-ID: " + header_value(msg, "Message-ID") + u"\n"
        text += u"\n"
        for part in msg.walk():
            if part.is_multipart():
                pass 
            else:
                ct = part.get_content_type()
                if ct.lower() == "text/plain":
                    charset = part.get_content_charset("iso-8859-1")
                    #print "charset: ", charset
                    #print "text: ", part.get_payload(None, True)
                    text += unicode(part.get_payload(None, True), charset)
        doc.text = text
        # dbytes
        doc.dbytes = str(len(text))
        # sig
        doc.sig = self.sig()
        udi = self.udi(self.msgnum)
        db.addOrUpdate(udi, doc)


db = recoll.connect(confdir=rclconf, writable=1)

mbidx = mbox_indexer(mbfile)
mbidx.index(db)
