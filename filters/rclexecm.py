#!/usr/bin/env python

###########################################
## Generic recoll multifilter communication code
import sys
import os

class RclExecM:
    noteof  = 0
    eofnext = 1
    eofnow = 2

    noerror = 0
    subdocerror = 1
    fileerror = 2
    
    def __init__(self):
        self.myname = os.path.basename(sys.argv[0])
        self.mimetype = ""

        if os.environ.get("RECOLL_FILTER_MAXMEMBERKB"):
            self.maxmembersize = \
            int(os.environ.get("RECOLL_FILTER_MAXMEMBERKB"))
        else:
            self.maxmembersize = 50 * 1024
        self.maxmembersize = self.maxmembersize * 1024

    def rclog(self, s, doexit = 0, exitvalue = 1):
        print >> sys.stderr, "RCLMFILT:", self.myname, ":", s
        if doexit:
            sys.exit(exitvalue)

    # Note: tried replacing this with a multiple replacer according to
    #  http://stackoverflow.com/a/15221068, which was **10 times** slower
    def htmlescape(self, txt):
        # This must stay first (it somehow had managed to skip after
        # the next line, with rather interesting results)
        txt = txt.replace("&", "&amp;")

        txt = txt.replace("<", "&lt;")
        txt = txt.replace(">", "&gt;")
        txt = txt.replace('"', "&quot;")
        return txt

    # Our worker sometimes knows the mime types of the data it sends
    def setmimetype(self, mt):
        self.mimetype = mt

    # Read single parameter from process input: line with param name and size
    # followed by data.
    def readparam(self):
        s = sys.stdin.readline()
        if s == '':
            sys.exit(0)
#           self.rclog(": EOF on input", 1, 0)

        s = s.rstrip("\n")

        if s == "":
            return ("","")
        l = s.split()
        if len(l) != 2:
            self.rclog("bad line: [" + s + "]", 1, 1)

        paramname = l[0].lower()
        paramsize = int(l[1])
        if paramsize > 0:
            paramdata = sys.stdin.read(paramsize)
            if len(paramdata) != paramsize:
                self.rclog("Bad read: wanted %d, got %d" %
                      (paramsize, len(paramdata)), 1,1)
        else:
            paramdata = ""
    
        #self.rclog("paramname [%s] paramsize %d value [%s]" %
        #          (paramname, paramsize, paramdata))
        return (paramname, paramdata)

    # Send answer: document, ipath, possible eof.
    def answer(self, docdata, ipath, iseof = noteof, iserror = noerror):

        if iserror != RclExecM.fileerror and iseof != RclExecM.eofnow:
            if isinstance(docdata, unicode):
                self.rclog("GOT UNICODE for ipath [%s]" % (ipath,))
                docdata = docdata.encode("UTF-8")

            print "Document:", len(docdata)
            sys.stdout.write(docdata)

            if len(ipath):
                print "Ipath:", len(ipath)
                sys.stdout.write(ipath)

            if len(self.mimetype):
                print "Mimetype:", len(self.mimetype)
                sys.stdout.write(self.mimetype)

        # If we're at the end of the contents, say so
        if iseof == RclExecM.eofnow:
            print "Eofnow: 0"
        elif iseof == RclExecM.eofnext:
            print "Eofnext: 0"
        if iserror == RclExecM.subdocerror:
            print "Subdocerror: 0"
        elif iserror == RclExecM.fileerror:
            print "Fileerror: 0"
  
        # End of message
        print
        sys.stdout.flush()
        #self.rclog("done writing data")

    def processmessage(self, processor, params):

        # We must have a filename entry (even empty). Else exit
        if not params.has_key("filename:"):
            self.rclog("no filename ??", 1, 1)

        # If we're given a file name, open it. 
        if len(params["filename:"]) != 0:
            try:
                if not processor.openfile(params):
                    self.answer("", "", iserror = RclExecM.fileerror)
                    return
            except Exception, err:
                self.rclog("processmessage: openfile raised: [%s]" % err)
                self.answer("", "", iserror = RclExecM.fileerror)
                return

        # If we have an ipath, that's what we look for, else ask for next entry
        ipath = ""
        eof = True
        self.mimetype = ""
        try:
            if params.has_key("ipath:") and len(params["ipath:"]):
                ok, data, ipath, eof = processor.getipath(params)
            else:
                ok, data, ipath, eof = processor.getnext(params)
        except Exception, err:
            self.answer("", "", eof, RclExecM.fileerror)
            return

        #self.rclog("processmessage: ok %s eof %s ipath %s"%(ok, eof, ipath))
        if ok:
            self.answer(data, ipath, eof)
        else:
            self.answer("", "", eof, RclExecM.subdocerror)

    # Loop on messages from our master
    def mainloop(self, processor):
        while 1:
            #self.rclog("waiting for command")

            params = dict()

            # Read at most 10 parameters (normally 1 or 2), stop at empty line
            # End of message is signalled by empty paramname
            for i in range(10):
                paramname, paramdata = self.readparam()
                if paramname == "":
                    break
                params[paramname] = paramdata

            # Got message, act on it
            self.processmessage(processor, params)


# Common main routine for all python execm filters: either run the
# normal protocol engine or a local loop to test without recollindex
def main(proto, extract):
    if len(sys.argv) == 1:
        proto.mainloop(extract)
    else:
        # Got a file name parameter: TESTING without an execm parent
        # Loop on all entries or get specific ipath
        params = {'filename:':sys.argv[1]}
        if not extract.openfile(params):
            print "Open error"
            sys.exit(1)
        ipath = ""
        if len(sys.argv) == 3:
            ipath = sys.argv[2]

        if ipath != "":
            params['ipath:'] = ipath
            ok, data, ipath, eof = extract.getipath(params)
            if ok:
                print "== Found entry for ipath %s (mimetype [%s]):" % \
                      (ipath, proto.mimetype)
                if isinstance(data, unicode):
                    bdata = data.encode("UTF-8")
                else:
                    bdata = data
                sys.stdout.write(bdata)
                print
            else:
                print "Got error, eof %d"%eof
            sys.exit(0)

        ecnt = 0   
        while 1:
            ok, data, ipath, eof = extract.getnext(params)
            if ok:
                ecnt = ecnt + 1
                print "== Entry %d ipath %s (mimetype [%s]):" % \
                      (ecnt, ipath, proto.mimetype)
                if isinstance(data, unicode):
                    bdata = data.encode("UTF-8")
                else:
                    bdata = data
                #sys.stdout.write(bdata)
                print
                if eof != RclExecM.noteof:
                    break
            else:
                print "Not ok, eof %d" % eof
                break
