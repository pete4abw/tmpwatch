VERSION=$(shell awk '/Version:/ { print $$2 }' tmpwatch.spec)
CVSTAG = r$(subst .,-,$(VERSION))

CFLAGS=$(RPM_OPT_FLAGS) -Wall -DVERSION=\"$(VERSION)\" -D_GNU_SOURCE

ifdef HP_UX
    BASEDIR=/usr/local
    MANDIR=$(BASEDIR)/man
    INSTALL=./install-sh
    CC=gcc
else
    BASEDIR=/usr
    MANDIR=$(BASEDIR)/man
    INSTALL=install
endif

all: tmpwatch

install: all
	[ -d $(PREFIX)$(SBINDIR) ] || mkdir -p $(PREFIX)$(SBINDIR)
	[ -d $(PREFIX)$(MANDIR)/man8 ] || mkdir -p $(PREFIX)$(MANDIR)/man8
	$(INSTALL) -s -m 755 tmpwatch $(PREFIX)$(SBINDIR)/tmpwatch
	$(INSTALL) -m 644 tmpwatch.8 $(PREFIX)$(MANDIR)/man8/tmpwatch.8

clean:
	rm -f tmpwatch

archive:
	cvs tag -F $(CVSTAG) .
	@rm -rf /tmp/tmpwatch-$(VERSION) /tmp/tmpwatch
	@cd /tmp; cvs export -r$(CVSTAG) tmpwatch
	mv /tmp/tmpwatch /tmp/tmpwatch-$(VERSION)
	@dir=$$PWD; cd /tmp; tar cvzf $$dir/tmpwatch-$(VERSION).tar.gz tmpwatch-$(VERSION)
	@rm -rf /tmp/tmpwatch-$(VERSION)
	@echo "The archive is in tmpwatch-$(VERSION).tar.gz"

