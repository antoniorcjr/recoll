PHP_ARG_ENABLE(recoll,
    [Whether to enable the "recoll" extension],
    [  --enable-recoll      Enable "recoll" extension support])

if test $PHP_RECOLL != "no"; then
    PHP_REQUIRE_CXX()
    PHP_SUBST(RECOLL_SHARED_LIBADD)
    PHP_ADD_INCLUDE(../../utils)
    PHP_ADD_INCLUDE(../../common)
    PHP_ADD_INCLUDE(../../rcldb)
    PHP_ADD_INCLUDE(../../query)
    PHP_ADD_INCLUDE(../../unac)
    PHP_ADD_INCLUDE(../../internfile)
    PHP_ADD_LIBRARY_WITH_PATH(recoll, ../../lib, RECOLL_SHARED_LIBADD)
    PHP_ADD_LIBRARY(xapian, , RECOLL_SHARED_LIBADD)
    PHP_ADD_LIBRARY(stdc++, 1, RECOLL_SHARED_LIBADD)
    PHP_NEW_EXTENSION(recoll, recoll.cpp, $ext_shared)
fi
