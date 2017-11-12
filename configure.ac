dnl Process this file with autoconf to produce a configure script.

AC_COPYRIGHT([Copyright (C) 2017 Alexandre C Vieira.
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.])

AC_PREREQ(2.59)
AC_INIT([launchapps], [1.0.0], [acamargo.vieira@gmail.com])
AC_PROG_CC
AC_PROG_CXX

AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_SYSTEM

AC_CONFIG_MACRO_DIR([m4])

dnl Initialize automake
AM_INIT_AUTOMAKE

dnl this allows us specify individual liking flags for each target
AM_PROG_CC_C_O 

dnl Initialize Libtool
LT_INIT
AC_PROG_RANLIB

AC_ENABLE_SHARED
AC_DISABLE_STATIC

PKG_CHECK_MODULES(GTK2, [gtk+-2.0])
PKG_CHECK_MODULES(GLIB2, [glib-2.0])
PKG_CHECK_MODULES(LIBFM, [libfm])
PKG_CHECK_MODULES(GIO2, [gio-2.0])
PKG_CHECK_MODULES(MAGICKWAND, [MagickWand])
AC_CHECK_FUNCS([strstr])

ac_version_subst_str=`echo $VERSION | awk -F '.' '/.*/ { printf "VERSION_MAJOR=%d VERSION_MINOR=%d VERSION_MICRO=%d", $1, $2, $3 }'`
eval ${ac_version_subst_str}
AC_SUBST(VERSION_MAJOR)
AC_SUBST(VERSION_MINOR)
AC_SUBST(VERSION_MICRO)

AC_CONFIG_FILES(Makefile src/Makefile images/Makefile)
AC_OUTPUT
