VERSION=$(shell awk '/define version/ { print $$3 }' tmpwatch.spec)
CVSTAG = r$(subst .,-,$(VERSION))

CFLAGS=$(RPM_OPT_FLAGS) -Wall -DVERSION=\"$(VERSION)\"

ifdef HP_UX
    BASEDIR=/usr/local
    INSTALL=./install-sh
    CC=gcc
else
    BASEDIR=/usr
    INSTALL=install
endif

all: tmpwatch

install: all
	[ -d $(PREFIX)$(BASEDIR)/sbin ] || mkdir -p $(PREFIX)$(BASEDIR)/sbin
	[ -d $(PREFIX)$(BASEDIR)/man/man8 ] || mkdir -p $(PREFIX)$(BASEDIR)/man/man8
	$(INSTALL) -s -m 755 tmpwatch $(PREFIX)$(BASEDIR)/sbin/tmpwatch
	$(INSTALL) -m 644 tmpwatch.8 $(PREFIX)$(BASEDIR)/man/man8/tmpwatch.8

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
