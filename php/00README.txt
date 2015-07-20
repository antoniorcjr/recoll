Recoll PHP extension:

  Author:  Wenqiang Song (wsong.cn, google mail)

This has minimum features for now. 

The php/sample/ subdirectory has a minimal script which demonstrates the
interface.

Building the extension needs the librcl.a library (recoll-xxx/lib/librcl.a)
to have been built with PIC objects. 

Use "configure --enable-pic" in the top Recoll directory, then
"make". There is no significant disadvantage in using PIC objects for
everything so you need not bother having different builds for the
extensions and the programs.

The php/recoll/ subdirectory has the C++ code and the build script
(make.sh). You'll need to "make install" after building.

If you want to clean up the php/recoll/ directory, you can run phpize --clean
in there.
