VERSION=1.0

CFLAGS=$(RPM_OPT_FLAGS) -DVERSION=\"$(VERSION)\"

all: tmpwatch

install:
	install -s -m 644 tmpwatch /usr/sbin/tmpwatch

clean:
	rm -f tmpwatch
