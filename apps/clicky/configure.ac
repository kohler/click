dnl -*- mode: shell-script -*-
dnl 'configure.ac' file for the Clicky GUI
dnl Process this file with autoconf to produce a configure script.
dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a
dnl copy of this software and associated documentation files (the "Software"),
dnl to deal in the Software without restriction, subject to the conditions
dnl listed in the Click LICENSE file. These conditions include: you must
dnl preserve this copyright notice, and you cannot mention the copyright
dnl holders in advertising related to the Software without their permission.
dnl The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
dnl notice is a summary of the Click LICENSE file; the license in that file is
dnl legally binding.

dnl
dnl the usual 'configure.ac' boilerplate
dnl

AC_INIT([clicky], [0.1])
AC_CONFIG_SRCDIR([configure.ac])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE
AC_SUBST(ac_configure_args)

AC_ISC_POSIX
AC_PROG_MAKE_SET
AC_PROG_CC
AM_PROG_CC_STDC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_CXXCPP
AC_PROG_INSTALL
if test -n "$GCC" ; then CC="$CC -W -Wall" ; CXX="$CXX -W -Wall" ; fi

pkg_modules="gtk+-2.0 >= 2.0.0"
PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])
AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

dnl
dnl locate Click install directory
dnl

AC_ARG_WITH(click, [  --with-click[[=DIR]]      Click install path is DIR [[PREFIX]]],
    [clickprefix=$withval; if test -z "$withval" -o "$withval" = yes; then clickprefix=$prefix; fi],
    clickprefix=NONE)

dnl Preset $prefix and $exec_prefix.
test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
test "x$clickprefix" != xNONE -a "x$prefix" = xNONE && prefix="$clickprefix"
test "x$clickprefix" = xNONE && clickprefix="$prefix"

clickdatadir="${clickprefix}/share/click"

if test ! -r $clickdatadir/config.mk; then
    AC_MSG_ERROR([
==============================================

'$clickdatadir/config.mk' not found.
Are you sure '$clickprefix' is a Click install directory?

==============================================])
fi

. $clickdatadir/config.mk

AC_SUBST(clickprefix)
AC_SUBST(clickbindir)
AC_SUBST(clicksbindir)
AC_SUBST(clickdatadir)

CLICKTOOL_LIBS='`$(clickbindir)/click-buildtool --toollibs`'
AC_SUBST(CLICKTOOL_LIBS)

dnl
dnl Output
dnl

AC_OUTPUT(Makefile src/Makefile)
