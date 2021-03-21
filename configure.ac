#
# Configure script for rasterview, a CUPS/PWG Raster viewing program.
#
# Copyright © 2002-2021 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

dnl We need at least autoconf 2.50...
AC_PREREQ(2.60)

AC_INIT([RasterView], [1.8], [https://github.com/michaelrsweet/rasterview/issues], [rasterview], [https://www.msweet.org/rasterview])

dnl Version number...
VERSION="AC_PACKAGE_VERSION"
AC_SUBST(VERSION)
AC_DEFINE_UNQUOTED(VERSION, "rasterview v$VERSION")

dnl Get the operating system and version number...
uname=$(uname)
uversion=$(uname -r | sed -e '1,$s/[[^0-9]]//g')
AS_IF([test "x$uname" = x], [
    # MingW doesn't provide any output when uname is run, even with "-s"...
    uname="CYGWIN"
])

dnl Clear default debugging options and set normal optimization by
dnl default unless the user asks for debugging specifically.
CFLAGS="${CFLAGS:=}"
AC_SUBST([CFLAGS])

CXXFLAGS="${CPPFLAGS:=}"
AC_SUBST([CPPFLAGS])

CXXFLAGS="${CXXFLAGS:=}"
AC_SUBST([CXXFLAGS])

LDFLAGS="${LDFLAGS:=}"
AC_SUBST([LDFLAGS])

LIBS="${LIBS:=}"
AC_SUBST([LIBS])

OPTIM="${OPTIM:=}"
AC_SUBST([OPTIM])

AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [turn on debugging, default=no]))
AS_IF([test x$enable_debug = xyes], [
    OPTIM="$OPTIM -g"
    CSFLAGS=""
], [
    OPTIM="$OPTIM -g -Os"
    CSFLAGS="-o runtime"
])
AC_SUBST([CSFLAGS])

dnl Checks for programs...
AC_PROG_CC
AC_PROG_CXX
AC_PATH_PROGS([CODE_SIGN], [codesign true])
AC_PATH_PROG([CP], [cp])
AC_PATH_PROG([MKDIR], [mkdir])
AC_PATH_PROG([RM], [rm])

dnl Check for FLTK...
AC_PATH_PROG([FLTKCONFIG], [fltk-config])

AS_IF([test "x$FLTKCONFIG" = x], [
    AC_MSG_ERROR([Sorry, rasterview requires FLTK 1.1.x or later.])
], [
    CXXFLAGS="$($FLTKCONFIG --use-images --cflags) $CXXFLAGS"
    LIBS="$($FLTKCONFIG --use-images --ldflags) $LIBS"
])

AC_SUBST([FLTKCONFIG])

dnl Make sure we include zlib (always available via FLTK)
AC_SEARCH_LIBS([gzopen], [z])

dnl Support large files.
AC_SYS_LARGEFILE

dnl Add -Wall for GCC...
AS_IF([test -n "$GCC"], [
    dnl Extended warnings...
    CPPFLAGS="-Wshadow -Winline $CPPFLAGS"
    dnl Standard warnings...
    CPPFLAGS="-Wall -Wunused -Wno-char-subscripts -Wno-format-y2k $CPPFLAGS"
])

dnl When doing a release build on current macOS, build fat for Intel and Apple
dnl Silicon...
AC_ARG_WITH([archflags], AS_HELP_STRING([--with-archflags=...], [Specify architecture options]), [
    ARCHFLAGS="$withval"
], [
    AS_IF([test $uname = Darwin -a $uversion -ge 2000 -a x$enable_debug != xyes], [
        ARCHFLAGS="-mmacosx-version-min=10.14 -arch x86_64 -arch arm64e"
    ], [
        ARCHFLAGS=""
    ])
])
AC_SUBST([ARCHFLAGS])

dnl See if we have the X11 desktop stuff used by GNOME and KDE...
AC_MSG_CHECKING([if GNOME/KDE desktop is in use])
desktopdir=""
for dir in /usr/share/applications /usr/share/applnk /etc/X11/applnk; do
    AS_IF([test -d $dir], [
	desktopdir=$dir
	break
    ])
done

AS_IF([test x$desktopdir = x], [
    AC_MSG_RESULT([no])
    INSTALLDESKTOP=""
    UNINSTALLDESKTOP=""
], [
    AC_MSG_RESULT([yes])
    INSTALLDESKTOP="install-desktop"
    UNINSTALLDESKTOP="uninstall-desktop"
])

AC_SUBST([desktopdir])
AC_SUBST([INSTALLDESKTOP])
AC_SUBST([UNINSTALLDESKTOP])

dnl Output the makefile...
AC_CONFIG_FILES([Makefile rasterview.list])
AC_OUTPUT
