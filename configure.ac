AC_INIT([xambit], [0.1], [aaron.styx@baesystems.com])
AC_PREREQ([2.68])
AC_CONFIG_SRCDIR([src/xambit.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.11 subdir-objects -Wall -Werror])


# Pkgconfig
#PKG_CHECK_MODULES([DEPS], [list-of-libs])

AC_PROG_CC
AM_PROG_AR

AC_CHECK_HEADERS(zlib.h, [], [AC_ERROR([A working zlib is required])])
AC_SEARCH_LIBS(crc32, z, [], [AC_ERROR([A working zlib is required])])

AC_ENABLE_STATIC
AC_ENABLE_SHARED
LT_INIT

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
