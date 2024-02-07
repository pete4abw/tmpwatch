#!/bin/sh

# Fetch gnulib-tool app. If not found, clone gnulib
if [ ! -x gnulib/gnulib-tool ] ; then
	# Is this a git repo or a tarball extracted?
	if [ -d .git ]; then
		# we are in a git repo, so fetch the gnulib submodule
		echo "Executing git submodule update --init"
		git submodule update --init gnulib
		if [ $? -ne 0 ] ; then
			echo "Could not clone submodule"
			exit 1
		fi
	else
		# we are not in a git repo so clone the gnulib repo
		echo "Cloning git clone https://git.savannah.gnu.org/git/gnulib.git"
		git clone https://git.savannah.gnu.org/git/gnulib.git
		if [ $? -ne 0 ] ; then
			echo "Could not clone submodule"
			exit 1
		fi
	fi
fi

echo "Fetching gnulib modules"
gnulib/gnulib-tool --import --dir=. --lib=libgnu --source-base=lib --m4-base=m4 --doc-base=doc --aux-dir=admin --no-libtool --macro-prefix=gl clock-time getopt-gnu progname stpcpy strtoimax xalloc

echo 'Running autoreconf -if...'
autoreconf -if || exit 1
if test -z "$NOCONFIGURE" ; then
	echo 'Configuring...'
	./configure $@
fi
