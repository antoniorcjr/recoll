#!/bin/sh
phpize --clean
phpize

# The version of libtool shipped with php at this time (2010-01-29) 
# is buggy (ECHO/echo typo...)
# autoreconf -i should replace them with the system versions
# Another approach would be to ship an autoconf'd version to avoid
# needing autotools on the target compile system. What a mess.
rm -f ltmain.sh libtool
rm -f aclocal.m4
autoreconf -i
./configure --enable-recoll
make -j3
