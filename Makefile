VERSION=1.2

CFLAGS=$(RPM_OPT_FLAGS) -Wall -DVERSION=\"$(VERSION)\"

all: tmpwatch

install:
	install -s -m 755 tmpwatch $(PREFIX)/usr/sbin/tmpwatch
	install -s -m 644 tmpwatch.8 $(PREFIX)/usr/man/man8/tmpwatch.8

clean:
	rm -f tmpwatch
