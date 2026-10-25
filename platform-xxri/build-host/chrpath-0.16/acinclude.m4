dnl
dnl Add macros
dnl	JAPHAR_GREP_CFLAGS
dnl

dnl
dnl JAPHAR_GREP_CFLAGS(flag, cmd_if_missing, cmd_if_present)
dnl
dnl From Japhar.  Report changes to japhar@hungry.com
dnl
AC_DEFUN([JAPHAR_GREP_CFLAGS],
[case "$CFLAGS" in
"$1" | "$1 "* | *" $1" | *" $1 "* )
  ifelse($#, 3, [$3], [:])
  ;;
*)
  $2
  ;;
esac
])

AC_DEFUN([CHRPATH_LDRPATH_OPTION],
[AC_REQUIRE([AC_CANONICAL_TARGET])[]dnl
case "$target" in
	*-linux*|*-gnu*|*-k*bsd*-gnu)
		LDRPATH=-Wl,-rpath,
		;;
	*-*-irix*|*-*-osf*)
		LDRPATH="-rpath "
		;;
	*-*-hpux*)
		# Hm, this seem to set runpath on ia64, not rpath
		LDRPATH="-Wl,+b,"
		;;
	*-*-darwin*)
		# No idea how to add rpath/runpath on Darwin
		LDRPATH=""
		;;
	*)
		LDRPATH="-R "
		;;
esac
AC_SUBST(LDRPATH)
])
