#!/bin/bash

echo 'Running autoreconf -if...'
autoreconf -if || exit 1
if test -z "$NOCONFIGURE" ; then
	echo 'Configuring...'
	./configure $@
fi
