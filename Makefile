VERSION=2.9.15
HGTAG = 'tmpwatch-$(VERSION)'
OS_NAME=$(shell uname -s)

CFLAGS=$(RPM_OPT_FLAGS) -W -Wall -DVERSION=\"$(VERSION)\" -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE

ifeq ($(OS_NAME),HP-UX)
    ROOT=/
    PREFIX=/usr/local
    MANDIR=$(PREFIX)/man
    SBINDIR=$(PREFIX)/sbin
    INSTALL=./install-sh
    CC=gcc
endif
ifeq ($(OS_NAME),SunOS)
    ROOT=/
    PREFIX=/usr/local
    MANDIR=$(PREFIX)/man
    SBINDIR=$(PREFIX)/sbin
    INSTALL=./install-sh
    CC=gcc
endif
ifeq ($(OS_NAME),Linux)
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
	$(INSTALL) -m 755 tmpwatch $(ROOT)$(SBINDIR)/tmpwatch
	$(INSTALL) -m 644 tmpwatch.8 $(ROOT)$(MANDIR)/man8/tmpwatch.8

clean:
	rm -f tmpwatch

force-tag:
	hg tag -f $(HGTAG)

tag:
	hg tag $(HGTAG)

archive:
	rm -rf /tmp/tmpwatch-$(VERSION) /tmp/tmpwatch
	hg clone -r $(HGTAG) . /tmp/tmpwatch-$(VERSION)
	rm -rf /tmp/tmpwatch-$(VERSION)/.hg*
	dir=$$PWD; cd /tmp; tar cvjf $$dir/tmpwatch-$(VERSION).tar.bz2 tmpwatch-$(VERSION)
	rm -rf /tmp/tmpwatch-$(VERSION)
	echo "The archive is in tmpwatch-$(VERSION).tar.bz2"
