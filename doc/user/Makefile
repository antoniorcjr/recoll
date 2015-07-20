# Wherever docbook.xsl and chunk.xsl live
# Fbsd
#XSLDIR="/usr/local/share/xsl/docbook/"
# Mac
#XSLDIR="/opt/local/share/xsl/docbook-xsl/"
#Linux
XSLDIR="/usr/share/xml/docbook/stylesheet/docbook-xsl/"


# Options common to the single-file and chunked versions
commonoptions=--stringparam section.autolabel 1 \
  --stringparam section.autolabel.max.depth 3 \
  --stringparam section.label.includes.component.label 1 \
  --stringparam autotoc.label.in.hyperlink 0 \
  --stringparam abstract.notitle.enabled 1 \
  --stringparam html.stylesheet docbook-xsl.css \
  --stringparam generate.toc "book toc,title,figure,table,example,equation"


all: usermanual.html index.html usermanual.pdf

usermanual.html: usermanual.xml
	xsltproc ${commonoptions} \
            -o tmpfile.html "${XSLDIR}/html/docbook.xsl" usermanual.xml
	-tidy -indent tmpfile.html > usermanual.html
	rm -f tmpfile.html

index.html: usermanual.xml
	xsltproc ${commonoptions} \
            --stringparam use.id.as.filename 1 \
            --stringparam root.filename index \
            "${XSLDIR}/html/chunk.xsl" usermanual.xml

usermanual.pdf: usermanual.xml
	dblatex usermanual.xml 

clean:
	rm -f RCL.*.html usermanual.pdf usermanual.html index.html tmpfile.html
