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
tmpwatch_LDADD = lib/libgnu.a $(LIBINTL)

force-tag:
	cd $(top_srcdir); hg tag -f $(HGTAG)

tag:
	cd $(top_srcdir); hg tag $(HGTAG)

archive:
	rm -fr /tmp/$(PACKAGE)-export-tmp
	hg clone -r $(HGTAG) $(abs_top_srcdir) /tmp/$(PACKAGE)-export-tmp
	cd /tmp/$(PACKAGE)-export-tmp; autoreconf -fis ; ./configure; make dist
	mv /tmp/$(PACKAGE)-export-tmp/$(PACKAGE)-$(VERSION).tar.bz2 .
	rm -fr /tmp/$(PACKAGE)-export-tmp
	@echo Package is in \"$(PACKAGE)-$(VERSION).tar.bz2\".
