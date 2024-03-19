#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT([tmpwatch], [2.13], [pete@peterhyman.com])
AC_CONFIG_SRCDIR([tmpwatch.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

AC_ARG_WITH([fuser],
       AS_HELP_STRING([--with-fuser=PATH_TO_FUSER],
                      [full path to fuser or "no" (search PATH by default)]),
[case "$withval" in
      no) FUSER='' ;;
      /* | yes) FUSER=$withval ;;
      *) AC_MSG_ERROR([Set --with-fuser to a full path or yes/no]) ;;
esac], [FUSER=yes])
if test "$FUSER" = yes; then
   AC_PATH_PROG([FUSER], [fuser], [],
               [$PATH$PATH_SEPARATOR/usr/sbin$PATH_SEPARATOR/sbin])
fi
AC_ARG_VAR([FUSER], [Path to fuser])
AC_MSG_CHECKING([whether to use fuser])
if test "x$FUSER" == x; then
   AC_MSG_RESULT([no])
else
   $FUSER / >/dev/null 2>&1
   if test $? -ne 0; then
      AC_MSG_RESULT([$FUSER does not work])
   else
      AC_MSG_RESULT([yes])
      AC_DEFINE_UNQUOTED([FUSER], ["$FUSER"], [Path to fuser])

      AC_MSG_CHECKING([whether fuser supports -s])
      $FUSER -s / >/dev/null 2>&1
      if test $? -eq 0; then
        AC_MSG_RESULT([yes])
        AC_DEFINE([FUSER_ACCEPTS_S], [1], [Define to 1 if fuser supports -s.])
      else
        AC_MSG_RESULT([no])
      fi
   fi
fi

# Check for shred
AC_PATH_PROG([SHRED],[shred],[],[$PATH])
if test "x$SHRED" != x; then
   AC_DEFINE_UNQUOTED([SHRED], ["$SHRED"], [Path to shred])
fi

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h mntent.h obstack.h paths.h sys/time.h unistd.h utime.h])

# Check for system services
AC_SYS_LARGEFILE

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_FUNC_OBSTACK
AC_TYPE_SIZE_T
AC_TYPE_UID_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_GETMNTENT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([clock_gettime fchdir getmntent realpath rmdir stpcpy strdup strerror strrchr utime])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
