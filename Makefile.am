## Process this file with automake to produce Makefile.in

## Settings
ACLOCAL_AMFLAGS = -I m4
# AM_CPPFLAGS = 

sbin_PROGRAMS = tmpwatch
dist_man8_MANS = tmpwatch.8

## Rules
tmpwatch_SOURCES = bind-mount.c bind-mount.h tmpwatch.c
tmpwatch_LDADD = $(LIBINTL) $(LIB_CLOCK_GETTIME)

