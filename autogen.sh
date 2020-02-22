#!/bin/sh

if [ ! -x gnulib/gnulib-tool ] ; then
	echo "gnulib-tool not found. Was gnulib git fetched?"
	echo "aborting autogen.sh."
	exit 1
fi

echo "Fetching gnulib modules"
gnulib/gnulib-tool --import --dir=. --lib=libgnu --source-base=lib --m4-base=m4 --doc-base=doc --aux-dir=admin --no-libtool --macro-prefix=gl clock-time getopt progname stpcpy strtoimax xalloc

echo 'Running autoreconf -if...'
autoreconf -if || exit 1
if test -z "$NOCONFIGURE" ; then
	echo 'Configuring...'
	./configure $@
fi
