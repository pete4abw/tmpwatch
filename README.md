TMPWATCH
========

v 2.13

Fetching and Building
---------------------

```
git clone https://github.com/pete4abw/tmpwatch

./augtogen.sh

make
```
as root, `make install`

gnulib submodule has been removed. Functions used are all part of most distros.

About
-----
The tmpwatch utility recursively searches through specified directories and
removes files which have not been accessed in a specified period of time.
tmpwatch is normally used to clean up directories which are used for
temporarily holding files (for example, /tmp).

Past releases will be available at **https://pagure.io/tmpwatch** .
This fork includes enhancements, updated autotools, and removal of gnulib.

Bugs
----
Please consider reporting bugs as an Issue here.

Or, you may report bugs at **https://pagure.io/tmpwatch** .  Pull requests are
especially welcome.

THIS RELEASE, 2.13
===================
2024 Removal of gnulib functions.\
2019 Updates the program to include shredding capabilities.

Peter Hyman
pete@peterhyman.com
