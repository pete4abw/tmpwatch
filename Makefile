VERSION=$(shell awk '/define version/ { print $$3 }' tmpwatch.spec)
CVSTAG = r$(subst .,-,$(VERSION))

CFLAGS=$(RPM_OPT_FLAGS) -Wall -DVERSION=\"$(VERSION)\"

all: tmpwatch

install:
	[ -d $(PREFIX)/usr/sbin ] || mkdir -p $(PREFIX)/usr/sbin
	[ -d $(PREFIX)/usr/man/man8 ] || mkdir -p $(PREFIX)/usr/man/man8
	install -s -m 755 tmpwatch $(PREFIX)/usr/sbin/tmpwatch
	install -s -m 644 tmpwatch.8 $(PREFIX)/usr/man/man8/tmpwatch.8

clean:
	rm -f tmpwatch

archive:
	cvs tag -F $(CVSTAG) .
	@rm -rf /tmp/tmpwatch-$(VERSION)
	@cd /tmp; cvs export -r$(CVSTAG) -d /tmp/tmpwatch-$(VERSION) tmpwatch
	@dir=$$PWD; cd /tmp; tar cvzf $$dir/tmpwatch-$(VERSION).tar.gz tmpwatch-$(VERSION)
	@rm -rf /tmp/tmpwatch-$(VERSION)
	@echo "The archive is in tmpwatch-$(VERSION).tar.gz"
