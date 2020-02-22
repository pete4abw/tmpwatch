TMPWATCH
========

Fetching and Building
---------------------

gnulib is fetched recursively as a submodule in this release.

git clone --recursive **[git location]**

./autogen.sh **[configure options]**

About
-----
The tmpwatch utility recursively searches through specified directories and
removes files which have not been accessed in a specified period of time.
tmpwatch is normally used to clean up directories which are used for
temporarily holding files (for example, /tmp).

New releases will be available at **https://pagure.io/tmpwatch** .

Bugs
----
Please consider reporting the bug to your distribution's bug tracking system.

Otherwise, report bugs at **https://pagure.io/tmpwatch** .  Pull requests are
especially welcome.

THIS RELEASE, 2.11s
===================
2019. Updates the program to include shredding capabilities.

Peter Hyman
pete@peterhyman.com
