dnl Process this file with autoconf to produce a configure script.

AC_INIT(chrpath, 0.16, chrpath-devel@lists.alioth.debian.org)

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE("chrpath", "0.16")

AC_CONFIG_SRCDIR(chrpath.c)
AM_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

CHRPATH_LDRPATH_OPTION

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

dnl Checks for libraries.

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([getopt.h elf.h fcntl.h unistd.h link.h sys/link.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_BIGENDIAN
AC_CHECK_SIZEOF(void *)

dnl Checks for library functions.
AC_CHECK_FUNCS(getopt_long)

if eval "test x$GCC = xyes"; then
  for flag in \
      -W \
      -Wall \
      -Wcast-align \
      -Wcast-qual \
      -Wcomments \
      -Wmissing-declarations \
      -Wmissing-prototypes \
      -Wpointer-arith \
      -Wreturn-type \
      -Wstrict-prototypes 
  do
    JAPHAR_GREP_CFLAGS($flag, [ CFLAGS="$CFLAGS $flag" ])
  done
fi

AC_ARG_WITH(coverage, [Enable coverage testing],
[
    JAPHAR_GREP_CFLAGS(-g, [ CFLAGS="$CFLAGS -g" ])
    JAPHAR_GREP_CFLAGS(-fprofile-arcs, [ CFLAGS="$CFLAGS -fprofile-arcs" ])
    JAPHAR_GREP_CFLAGS(-ftest-coverage, [ CFLAGS="$CFLAGS -ftest-coverage" ])
], [])

AC_CONFIG_FILES([
        Makefile \
        testsuite/Makefile \
        deb/Makefile \
        ])
AC_OUTPUT

if test ! -d debian ; then
    ln -s deb debian
fi

echo "Configure status:"
echo "-----------------"
echo " CC     = $CC"
echo " CFLAGS = $CFLAGS"
echo " LDRPATH= $LDRPATH"
echo
