VERSION=$(shell awk '/Version:/ { print $$2 }' tmpwatch.spec)
CVSTAG = r$(subst .,-,$(VERSION))
CVSROOT = $(shell cat CVS/Root)

CFLAGS=$(RPM_OPT_FLAGS) -Wall -DVERSION=\"$(VERSION)\" -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE

ifdef HP_UX
    ROOT=/
    PREFIX=/usr/local
    MANDIR=$(PREFIX)/man
    SBINDIR=$(PREFIX)/sbin
    INSTALL=./install-sh
    CC=gcc
else
    ROOT=/
    PREFIX=/usr
    SBINDIR=$(PREFIX)/sbin
    MANDIR=$(PREFIX)/share/man
    INSTALL=install
endif

all: tmpwatch

install: all
	[ -d $(ROOT)$(SBINDIR) ] || mkdir -p $(ROOT)$(SBINDIR)
	[ -d $(ROOT)$(MANDIR)/man8 ] || mkdir -p $(ROOT)$(MANDIR)/man8
	$(INSTALL) -s -m 755 tmpwatch $(ROOT)$(SBINDIR)/tmpwatch
	$(INSTALL) -m 644 tmpwatch.8 $(ROOT)$(MANDIR)/man8/tmpwatch.8

clean:
	rm -f tmpwatch

tag:
	cvs tag -F $(CVSTAG) .

archive:
	@rm -rf /tmp/tmpwatch-$(VERSION) /tmp/tmpwatch
	@cd /tmp; cvs -d $(CVSROOT) export -r$(CVSTAG) tmpwatch
	mv /tmp/tmpwatch /tmp/tmpwatch-$(VERSION)
	@dir=$$PWD; cd /tmp; tar cvzf $$dir/tmpwatch-$(VERSION).tar.gz tmpwatch-$(VERSION)
	@rm -rf /tmp/tmpwatch-$(VERSION)
	@echo "The archive is in tmpwatch-$(VERSION).tar.gz"

