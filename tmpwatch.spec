Summary: A utility for removing files based on when they were last accessed.
Name: tmpwatch
Version: 2.5
Release: 1
Source: tmpwatch-%{version}.tar.gz
Copyright: GPL
Group: System Environment/Base
BuildRoot: /var/tmp/%{name}-root
Requires: psmisc

%description
The tmpwatch utility recursively searches through specified
directories and removes files which have not been accessed in a
specified period of time.  Tmpwatch is normally used to clean up
directories which are used for temporarily holding files (for example,
/tmp).  Tmpwatch ignores symlinks, won't switch filesystems and only
removes empty directories and regular files.

%prep
%setup -q

%build
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT
make PREFIX=$RPM_BUILD_ROOT install

( cd $RPM_BUILD_ROOT
  mkdir -p ./etc/cron.daily
  echo '/usr/sbin/tmpwatch 240 /tmp /var/tmp' \
	>> ./etc/cron.daily/tmpwatch
  echo '[ -d /var/cache/man ] && /usr/sbin/tmpwatch -f 240 /var/cache/man/{X11R6/cat?,cat?,local/cat?}' \
	>> ./etc/cron.daily/tmpwatch
  echo '[ -d /var/catman ] && /usr/sbin/tmpwatch -f 240 /var/catman/{X11R6/cat?,cat?,local/cat?}' \
	>> ./etc/cron.daily/tmpwatch
  chmod +x ./etc/cron.daily/tmpwatch
)

%clean
rm -rf $RPM_BUILD_ROOT

%files
/usr/sbin/tmpwatch
/usr/man/man8/tmpwatch.8*
%config /etc/cron.daily/tmpwatch

%changelog
* Thu May 18 2000 Preston Brown <pbrown@redhat.com>
- don't complain about failure to remove non-empty directories.

* Wed May 17 2000 Preston Brown <pbrown@redhat.com>
- support /var/cache/man and /var/catman (FHS 2.1 compliance).

* Fri May 05 2000 Preston Brown <pbrown@redhat.com>
- support for CTIME from jik@kamens.brookline.ma.us
- fixes for fuser checks from Ian Burrell <iburrell@digital-integrity.com>.
- remove directories when empty without --all flag, to be consistent w/docs.

* Mon Feb 14 2000 Preston Brown <pbrown@redhat.com>
- option to use fuser to see if file in use before removing

* Wed Feb 02 2000 Cristian Gafton <gafton@redhat.com>
- fix description
- man pages are compressed

* Tue Jan 18 2000 Preston Brown <pbrown@redhat.com>
- null terminal opt struct (#7836)
- test flag implies verbose (#2383)

* Wed Jan 12 2000 Paul Gear <paulgear@bigfoot.com>
- HP-UX port (including doco update)
- Tweaked Makefile to allow installation into different base directory
- Got rid of GETOPT_... defines which didn't do anything, so that short
  equivalents for all long options could be defined.
- Fixed bug in message() where 'where' file handle was set but unused
- Changed most fprintf() calls to message()

* Mon Aug 30 1999 Preston Brown <pbrown@redhat.com>
- skip lost+found directories
- option to use file's atime instead of mtime (# 4178)

* Mon Jun  7 1999 Jeff Johnson <jbj@redhat.com>
- cleanup more man pages, this time adding in cvs (#224).

* Thu Apr 08 1999 Preston Brown <pbrown@redhat.com>
- I am the new maintainer
- fixed cleanup of directories
- added --quiet flag
- freshen manpage
- nice patch from Kevin Vajk <kvajk@ricochet.net> integrated

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
- Rebuilt to get right permissions on the Alpha (though I have no idea
  how they ended up wrong).
