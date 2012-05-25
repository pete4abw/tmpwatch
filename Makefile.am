## Process this file with automake to produce Makefile.in

## Settings
ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = -I$(top_builddir)/lib -I$(top_srcdir)/lib
HGTAG = 'tmpwatch-$(VERSION)'

## Targets
SUBDIRS = lib

sbin_PROGRAMS = tmpwatch
dist_man8_MANS = tmpwatch.8

## Rules
tmpwatch_SOURCES = bind-mount.c bind-mount.h tmpwatch.c
tmpwatch_LDADD = lib/libgnu.a $(LIBINTL) $(LIB_CLOCK_GETTIME)

force-tag:
	cd $(top_srcdir); hg tag -f $(HGTAG)

tag:
	cd $(top_srcdir); hg tag $(HGTAG)
