#!/bin/sh
#set -x
# A shell-script to make a recoll static binary distribution:

fatal()
{
    echo $*;exit 1
}

TAR=tar
 
targetdir=${targetdir-/tmp}

if test ! -d qtgui;then
    echo "Should be executed in the master recoll directory"
    exit 1
fi

version=`cat VERSION`
sys=`uname -s`
sysrel=`uname -r`

qtguiassign=`egrep '^QTGUI=' recollinstall`
stripassign=`egrep '^STRIP=' recollinstall`
test ! -z "$qtguiassign" || fatal "Can't find qt version"
test ! -z "$stripassign" || fatal "Can't find strip string"
eval $qtguiassign
eval $stripassign
echo "QTGUI: " $QTGUI "STRIP: " $STRIP

topdirsimple=recoll-${version}-${sys}-${sysrel}
topdir=$targetdir/$topdirsimple

tarfile=$targetdir/recoll-${version}-${sys}-${sysrel}.tgz

if test ! -d $topdir ; then
    mkdir $topdir || exit 1
else 
    echo "Removing everything under $topdir Ok ? (y/n)"
    read rep 
    if test $rep = 'y';then
    	rm -rf $topdir/*
    else
	exit 1
    fi
fi

rm -f index/recollindex ${QTGUI}/recoll

make static || exit 1

${STRIP} index/recollindex ${QTGUI}/recoll

files="
COPYING 
INSTALL 
Makefile 
README 
VERSION 
desktop 
desktop/recoll-searchgui.desktop
desktop/recoll.png 
doc/man
doc/user 
filters 
index/rclmon.sh 
index/recollindex 
qtgui/i18n/*.qm 
qtgui/images
qtgui/mtpics/*.png 
qtgui/recoll 
recollinstall
sampleconf 
"

$TAR chf - $files  | (cd $topdir; $TAR xf -)

# Remove any install dependancy
chmod +w $topdir/Makefile || exit 1
sed -e '/^install:/c\
install: ' < $topdir/Makefile > $topdir/toto && \
	 mv $topdir/toto $topdir/Makefile

# Clean up .svn directories from target. This would be easier with a
# --exclude tar option, but we want this to work with non-gnu tars
cd $topdir || exit 1
svndirs=`find . -name .svn -print`
echo "In: `pwd`. Removing $svndirs ok ?"
read rep 
test "$rep" = 'y' -o "$rep" = 'Y' && rm -rf $svndirs

cd $targetdir

(cd $targetdir ; \
    $TAR chf - $topdirsimple | \
    	gzip > $tarfile)

echo $tarfile created
