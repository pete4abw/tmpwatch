VERSION=1.1

CFLAGS=$(RPM_OPT_FLAGS) -Wall -DVERSION=\"$(VERSION)\"

all: tmpwatch

install:
	install -s -m 755 tmpwatch /usr/sbin/tmpwatch

clean:
	rm -f tmpwatch
