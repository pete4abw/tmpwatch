Summary: A utility for removing files based on when they were last accessed.
Name: tmpwatch
Version: 2.9.1
Release: 1
Source: %{name}-%{version}.tar.gz
License: GPL
Group: System Environment/Base
BuildRoot: %{_tmppath}/%{name}-%{version}-root
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
make

%install
rm -rf %{buildroot}
make ROOT=%{buildroot} SBINDIR=%{_sbindir} MANDIR=%{_mandir} install

mkdir -p %{buildroot}/etc/cron.daily
cat > %{buildroot}/etc/cron.daily/tmpwatch <<EOF
/usr/sbin/tmpwatch -x /tmp/.X11-unix -x /tmp/.XIM-unix -x /tmp/.font-unix \
	-x /tmp/.ICE-unix -x /tmp/.Test-unix 240 /tmp
/usr/sbin/tmpwatch 720 /var/tmp
for d in /var/{cache/man,catman}/{cat?,X11R6/cat?,local/cat?}; do
    if [ -d "\$d" ]; then
	/usr/sbin/tmpwatch -f 720 \$d
    fi
done
EOF
chmod +x %{buildroot}/etc/cron.daily/tmpwatch

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_sbindir}/tmpwatch
%{_mandir}/man8/tmpwatch.8*
%config(noreplace) /etc/cron.daily/tmpwatch

%changelog
* Sat Aug 14 2004 Miloslav Trmac <mitr@redhat.com> - 2.9.1-1
- Add --exclude, use it to preserve X socket directories (#107069)
- Allow multiple directory arguments with relative paths (#91097)

* Fri May 30 2003 Mike A. Harris <mharris@redhat.com> 2.9.0-1
- Added Solaris/HPUX support to tmpwatch via patch from Paul Gear (#71288)
- Rebuild in rawhide as 2.9.0-1

* Mon Feb 10 2003 Nalin Dahyabhai <nalin@redhat.com> 2.8.4-5
- rebuild

* Fri Feb  7 2003 Nalin Dahyabhai <nalin@redhat.com> 2.8.4-2
- rebuild

* Tue Oct  8 2002 Mike A. Harris <mharris@redhat.com> 2.8.4-4
- All-arch rebuild

* Fri Jun 21 2002 Tim Powers <timp@redhat.com>
- automated rebuild

* Sun May 26 2002 Tim Powers <timp@redhat.com>
- automated rebuild

* Tue May 21 2002 Mike A. Harris <mharris@redhat.com> 2.8.4-1
- Bump release and rebuild in new environment

* Fri Apr 13 2002 Mike A. Harris <mharris@redhat.com> 2.8.3-1
- Added support for large files with 64bit offsets by adding to CFLAGS
  -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 bug (#56961)

* Sun Jan 27 2002 Mike A. Harris <mharris@redhat.com> 2.8.2-1
- Added newlines to logfile messages as per bug #58912

* Thu Nov  8 2001 Preston Brown <pbrown@redhat.com>
- define default SBINDIR in Makefile
- incorrect boolean comparison fix

* Wed Aug 29 2001 Preston Brown <pbrown@redhat.com>
- cron script fix (#52785)

* Tue Aug 28 2001 Preston Brown <pbrown@redhat.com>
- rebuild for 5.x, 6.x, 7.x errata

* Mon Aug 27 2001 Preston Brown <pbrown@redhat.com>
- noreplace /etc/cron.daily/tmpwatch

* Mon Aug  6 2001 Preston Brown <pbrown@redhat.com> 2.8-1
- added a "nodirs" option which inhibits removal of empty directories.
- Integrated race condition fixes from Martin Macok (#50148)
- do not try to remove ext3 journal files (#50522)

* Tue Jul  3 2001 Preston Brown <pbrown@redhat.com> 2.7.4-1
- fix typo in cron script

* Mon Jul  2 2001 Preston Brown <pbrown@redhat.com>
- better checking for directory existence cleaning man cache dirs (#44117)

* Fri May 11 2001 Trond Eivind Glomsrød <teg@redhat.com>
- Handle directories with large files
- fix some warnings during compilation

* Thu Mar 29 2001 Preston Brown <pbrown@redhat.com>
- fixed longstanding bug where directories removed while in test mode.

* Fri Mar  9 2001 Preston Brown <pbrown@redhat.com>
- Patch from enrico.scholz@informatik.tu-chemnitz.de allows concurrent 
  usage of mtime, ctime, and atime checking (#19550).

* Fri Jan 05 2001 Preston Brown <pbrown@redhat.com>
- increased interval for removal to 30 days for /var/tmp per FHS (#19951)

* Tue Sep 12 2000 Nalin Dahyabhai <nalin@redhat.com>
- use execle() instead of system() to get the correct return code, fixes from
  Jeremy Katz <katzj@linuxpower.org>

* Thu Sep  7 2000 Nalin Dahyabhai <nalin@redhat.com>
- rework to not have to fork() (#17286)
- set utime() after we're done reading a directory

* Sat Jun 17 2000 Matt Wilson <msw@redhat.com>
- defattr

* Tue Jun 13 2000 Preston Brown <pbrown@redhat.com>
- FHS compliance

* Thu May 18 2000 Preston Brown <pbrown@redhat.com>
- don't complain about failure to remove non-empty directories.
- fix man page path

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
