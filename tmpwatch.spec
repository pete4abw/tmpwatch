Summary: Cleans up files in directories based on their age
Name: tmpwatch
Version: 1.2
Release: 1
Source: tmpwatch-1.2.tar.gz
Copyright: GPL
Group: Utilities/System

%description
This package provides a program that can be used to clean out directories. It
recursively searches the directory (ignoring symlinks) and removes files that
haven't been accessed in a user-specified amount of time.

%changelog

* Mon Mar 24 1997 Erik Troan <ewt@redhat.com>

- Don't follow symlinks which are specified on the command line
- Added a man page

* Sun Mar 09 1997 Erik Troan <ewt@redhat.com>

Rebuilt to get right permissions on the Alpha (though I have no idea
how they ended up wrong).

%prep
%setup -n tmpwatch

%build
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
make install

%files
/usr/sbin/tmpwatch
