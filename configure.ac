# miniexpect
# Copyright (C) 2014 Red Hat Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

AC_INIT([miniexpect],[1.0])

AM_INIT_AUTOMAKE(foreign) dnl NB: Do not [quote] this parameter.

m4_ifndef([AM_SILENT_RULES], [m4_define([AM_SILENT_RULES],[])])
AM_SILENT_RULES([yes]) # make --enable-silent-rules the default.

AC_CONFIG_MACRO_DIR([m4])

AM_PROG_LIBTOOL

dnl Check for basic C environment.
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_CPP

AC_C_PROTOTYPES
test "x$U" != "x" && AC_MSG_ERROR([Compiler not ANSI compliant])

AM_PROG_CC_C_O

dnl Enable system extensions (_GNU_SOURCE etc).
AC_USE_SYSTEM_EXTENSIONS

dnl Check support for 64 bit file offsets.
AC_SYS_LARGEFILE

dnl The only dependency is libpcre (Perl Compatible Regular Expressions).
PKG_CHECK_MODULES([PCRE], [libpcre])

dnl Optional for building the manual page.  This is part of Perl.
AC_CHECK_PROG([POD2MAN], [pod2man], [pod2man], [no])
AM_CONDITIONAL([HAVE_POD2MAN], [test "x$POD2MAN" != "xno"])

dnl Produce output files.
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
