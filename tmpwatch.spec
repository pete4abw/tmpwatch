Summary: Cleans up files in directories based on their age
Name: tmpwatch
%define version 1.5.1
Version: %{version}
Release: 1
Source: tmpwatch-%{version}.tar.gz
Copyright: GPL
Group: Utilities/System
BuildRoot: /var/tmp/tmpwatch-root

%description
This package provides a program that can be used to clean out directories. It
recursively searches the directory (ignoring symlinks) and removes files that
haven't been accessed in a user-specified amount of time.

%changelog

* Wed Jun 10 1998 Erik Troan <ewt@redhat.com>

- make /etc/cron.daily/tmpwatch executable

* Tue Jan 13 1998 Erik Troan <ewt@redhat.com>

- version 1.5
- fixed flags passing
- cleaned up message()

* Wed Oct 22 1997 Erik Troan <ewt@redhat.com>

- added man page to package
- uses a buildroot and %attr
- fixed error message generation for directories
- fixed flag propagation

* Mon Mar 24 1997 Erik Troan <ewt@redhat.com>

- Don't follow symlinks which are specified on the command line
- Added a man page

* Sun Mar 09 1997 Erik Troan <ewt@redhat.com>

Rebuilt to get right permissions on the Alpha (though I have no idea
how they ended up wrong).

%prep
%setup

%build
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT
make PREFIX=$RPM_BUILD_ROOT install

mkdir -p $RPM_BUILD_ROOT/etc/cron.daily
echo '/usr/sbin/tmpwatch 240 /tmp /var/tmp /var/catman/cat?' \
	> $RPM_BUILD_ROOT/etc/cron.daily/tmpwatch
chmod +x $RPM_BUILD_ROOT/etc/cron.daily/tmpwatch

%clean
rm -rf $RPM_BUILD_ROOT

%files
/usr/sbin/tmpwatch
/usr/man/man8/tmpwatch.8
%config /etc/cron.daily/tmpwatch
