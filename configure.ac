#
# Configure script for rasterview, a CUPS/PWG Raster viewing program.
#
# Copyright 2002-2018 by Michael R Sweet
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License v2 as published
# by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

dnl We need at least autoconf 2.50...
AC_PREREQ(2.60)

AC_INIT([RasterView],[1.6],[https://github.com/michaelrsweet/rasterview/issues],[rasterview],[https://michaelrsweet.github.io/rasterview])

dnl Version number...
VERSION="AC_PACKAGE_VERSION"
AC_SUBST(VERSION)
AC_DEFINE_UNQUOTED(VERSION, "rasterview v$VERSION")

dnl Get the operating system and version number...
uname=`uname`
uversion=`uname -r | sed -e '1,$s/[[^0-9]]//g'`
if test "x$uname" = xIRIX64; then
    uname="IRIX"
fi
if test "x$uname" = x; then
    # MingW doesn't provide any output when uname is run, even with "-s"...
    uname="CYGWIN"
fi

dnl Clear default debugging options and set normal optimization by
dnl default unless the user asks for debugging specifically.
CFLAGS="${CFLAGS:=}"
AC_SUBST(CFLAGS)

CXXFLAGS="${CXXFLAGS:=}"
AC_SUBST(CXXFLAGS)

LDFLAGS="${LDFLAGS:=}"
AC_SUBST(LDFLAGS)

LIBS="${LIBS:=}"
AC_SUBST(LIBS)

AC_ARG_ENABLE(debug, [  --enable-debug          turn on debugging, default=no],
    [if eval "test x$enable_debug = xyes"; then
	CFLAGS="-g $CFLAGS"
	CXXFLAGS="-g $CXXFLAGS"
	LDFLAGS="-g $LDFLAGS"
    fi])

dnl Checks for programs...
AC_PROG_CC
AC_PROG_CXX
AC_PATH_PROG(CP,cp)
AC_PATH_PROG(MKDIR,mkdir)
AC_PATH_PROG(RM,rm)

dnl Check for FLTK...
AC_PATH_PROG(FLTKCONFIG,fltk-config)

if test "x$FLTKCONFIG" = x; then
	AC_MSG_ERROR([Sorry, rasterview requires FLTK 1.1.x or later.])
else
	CXXFLAGS="`$FLTKCONFIG --use-images --cflags` $CXXFLAGS"
	LIBS="`$FLTKCONFIG --use-images --ldflags` $LIBS"
fi

AC_SUBST(FLTKCONFIG)

dnl Make sure we include zlib (always available via FLTK)
AC_SEARCH_LIBS(gzopen, z)

dnl Support large files.
AC_SYS_LARGEFILE

dnl Add -Wall for GCC...
if test -n "$GCC"; then
	dnl Extended warnings...
	CFLAGS="-Wshadow -Winline $CFLAGS"
	dnl Standard warnings...
	CFLAGS="-Wall -Wunused -Wno-char-subscripts -Wno-format-y2k $CFLAGS"
fi

dnl See if we have the X11 desktop stuff used by GNOME and KDE...
AC_MSG_CHECKING(if GNOME/KDE desktop is in use)
desktopdir=""
for dir in /usr/share/applications /usr/share/applnk /etc/X11/applnk; do
	if test -d $dir; then
		desktopdir=$dir
		break
	fi
done

if test x$desktopdir = x; then
	AC_MSG_RESULT(no)
	INSTALLDESKTOP=""
	UNINSTALLDESKTOP=""
else
	AC_MSG_RESULT(yes)
	INSTALLDESKTOP="install-desktop"
	UNINSTALLDESKTOP="uninstall-desktop"
fi

AC_SUBST(desktopdir)
AC_SUBST(INSTALLDESKTOP)
AC_SUBST(UNINSTALLDESKTOP)

dnl Output the makefile...
AC_OUTPUT(Makefile rasterview.list)
