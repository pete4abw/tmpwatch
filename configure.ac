# Process this file with autoconf to produce a configure script.
AC_INIT([tmpwatch], [2.10], [https://fedorahosted.org/tmpwatch/])
AC_PREREQ([2.63])
AC_CONFIG_SRCDIR([tmpwatch.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([admin])
AM_INIT_AUTOMAKE([dist-bzip2 no-define -Wall])

# Checks for programs.
AC_PROG_CC
gl_EARLY
AC_PROG_RANLIB

gl_INIT

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([mntent.h paths.h])

# Checks for header files.

# Checks for types.

# Checks for structures.

# Checks for compiler characteristics.

# Checks for library functions.
AC_FUNC_GETMNTENT

# Checks for system services.
AC_SYS_LARGEFILE

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

# Miscellanous hackery.

# Modify CFLAGS after all tests are run (some of them would fail because
# of the -Werror).
if test "$GCC" = yes ; then
    AC_CACHE_CHECK([for available GCC warning flags],
		   [mitr_cv_prog_gcc_warnings],
    [mitr_cv_prog_gcc_warnings=""
    for i in -Wall -W -Wcast-align -Wmissing-noreturn -Wnested-externs -Wpointer-arith dnl
 -Wshadow -Wstrict-prototypes -Wwrite-strings; do
        oldcflags=$CFLAGS
        CFLAGS="$CFLAGS $i"
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
		[mitr_cv_prog_gcc_warnings="$mitr_cv_prog_gcc_warnings $i"])
	CFLAGS=$oldcflags
     done])
     CFLAGS="$CFLAGS $mitr_cv_prog_gcc_warnings"
     AC_MSG_CHECKING([whether we should use -Werror])
     AC_ARG_ENABLE([Werror],
		   AS_HELP_STRING([--enable-Werror],
			[use -Werror if compiling with gcc (default is NO)]),
		   [], enable_Werror=no)
     if test "x$enable_Werror" != xno; then
	CFLAGS="$CFLAGS -Werror"
     fi
     AC_MSG_RESULT([$enable_Werror])
fi

AC_CONFIG_FILES([Makefile lib/Makefile])
AC_OUTPUT
