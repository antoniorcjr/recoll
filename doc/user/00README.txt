= Building the Recoll user manual

The Recoll user manual used to be written in DocBook SGML and used the
FreeBSD doc toolchain to produce the output formats. This had the advantage
of an easy way to produce all formats including a PDF manual, but presented
two problems:

 - Dependancy on the FreeBSD platform.
 - No support for UTF-8 (last I looked), only latin1.

The manual is now compatible with XML. There is a small script that
converts the SGML (but XML-compatible) manual into XML (changes the header,
mostly). The SGML version is still the primary one.

Beyond fixing a few missing closing tags, the main change that had to be
made was to make the anchors explicitly upper-case because the SGML
toolchain converts them to upper-case and the XML one does not, so the only
way to have compatibility is to make them upper-case in the first place.

We initially had a problem for producing the PDF manual, which motivated
keeping the SGML version for producing the PDF with the FreeBSD SGML
toolchain. This problem is now solved with dblatex, so that the SGML
version now has little reason to persist and it will go away at some point
in the future.

Asciidoc would also be a candidate as the source format, because it can
easily produce docbook, so the future will probably be:

asciidoc->docbook-xml-> html
                     -> pdf
