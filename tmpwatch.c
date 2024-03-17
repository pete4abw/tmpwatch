/*
 * tmpwatch.c -- remove files in a directory, but do it carefully.
 *
 * Copyright (C) 1997-2001, 2004-2009 Red Hat, Inc.  All rights reserved.
 * Copyright (C) 2019-2024 Peter Hyman
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions of the
 * GNU General Public License v.2.  This program is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY expressed or implied,
 * including the implied warranties of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  Any Red Hat
 * trademarks that are incorporated in the source code or documentation are not
 * subject to the GNU General Public License and may only be used or replicated
 * with the express permission of Red Hat, Inc.
 *
 * Red Hat Author(s): Erik Troan <ewt@redhat.com>
 *                    Preston Brown <pbrown@redhat.com>
 *                    Mike A. Harris <mharris@redhat.com>
 *                    Miloslav Trmac <mitr@redhat.com>
 */
#include <config.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <utime.h>
#include <unistd.h>

#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#include "bind-mount.h"

#ifdef __GNUC__
#define attribute__(X) __attribute__ (X)
#else
#define attribute__(X)
#endif

#define LOG_REALDEBUG	1
#define LOG_DEBUG	2
#define LOG_VERBOSE	3
#define LOG_NORMAL	4
#define LOG_ERROR	5
#define LOG_FATAL	6

#define FLAG_FORCE	(1 << 0)
/* normally just files, dirs, and sockets older than (boot time - grace period)
   are removed */
#define FLAG_ALLFILES	(1 << 1)
#define FLAG_TEST	(1 << 2)
#define FLAG_ATIME	(1 << 3)
#define FLAG_MTIME	(1 << 4)
#define FLAG_FUSER	(1 << 5)
#define FLAG_CTIME	(1 << 6)
#define FLAG_NODIRS	(1 << 7)
#define FLAG_NOSYMLINKS (1 << 8)
#define FLAG_DIRMTIME	(1 << 9)
#define FLAG_SHRED	(1 <<10)

/* Do not remove lost+found directories if owned by this UID */
#define LOSTFOUND_UID 0

struct exclusion
{
    struct exclusion *next;
    const char *dir, *file;
};

static struct exclusion *exclusions /* = NULL */;
static struct exclusion **exclusions_tail = &exclusions;

struct excluded_pattern
{
    struct excluded_pattern *next;
    char *pattern;
};

static struct excluded_pattern *excluded_patterns; /* = NULL; */
static struct excluded_pattern **excluded_patterns_tail = &excluded_patterns;

struct excluded_uid
{
    struct excluded_uid *next;
    uid_t uid;
};

static struct excluded_uid *excluded_uids /* = NULL */;
static struct excluded_uid **excluded_uids_tail = &excluded_uids;

static time_t kill_time;
static time_t socket_kill_time; /* 0 = never */

static int config_flags; /* = 0; */

static int logLevel = LOG_NORMAL;

static void attribute__((format(printf, 2, 3)))
  message(int level, const char *format, ...)
{
    va_list args;
    FILE * where = stdout;

    if (level >= logLevel) {
	if (level > LOG_NORMAL) {
	    where = stderr;
	    fprintf(where, "error: ");
	}

	va_start(args, format);
	vfprintf(where, format, args);
	va_end(args);

	if (level == LOG_FATAL) exit(1);
    }
}

static char *
absolute_path(const char *path, int allow_nonexistent)
{
    char buf[PATH_MAX + 1];
    const char *src;

    src = realpath(path, buf);
    if (src == NULL) {
	if (allow_nonexistent != 0)
	    src = path;
	else
	    message(LOG_FATAL, "cannot resolve %s: %s\n", path,
		    strerror(errno));
    }
    return strdup(src);
}

/* Returns 0 if OK, 2 on ENOENT, 1 on other errors */
static int
safe_chdir(const char *fulldirname, const char *reldirname, dev_t st_dev,
	   ino_t st_ino)
{
    struct stat sb1, sb2;

    if (lstat(reldirname, &sb1)) {
	if (errno == ENOENT)
	    return 2;
	message(LOG_ERROR, "lstat() of directory %s failed: %s\n",
		fulldirname, strerror(errno));
	return 1;
    }

    if (! S_ISDIR(sb1.st_mode)) {
	message(LOG_ERROR, "directory %s changed right under us!!!\n",
		fulldirname);
	message(LOG_FATAL, "this indicates a possible intrusion attempt\n");
	return 1;
    }

    /* Check if the directory changed between cleanupDirectory and
     * safe_chdir
     */
    if (st_dev != sb1.st_dev || st_ino != sb1.st_ino) {
	message(LOG_ERROR, "directory %s changed right under us!!!\n",
		fulldirname);
	message(LOG_FATAL, "this indicates a possible intrusion attempt\n");
	return 1;
    }

    if (chdir(reldirname) != 0) {
	if (errno == ENOENT)
	    return 2;
	message(LOG_ERROR, "chdir to directory %s failed: %s\n",
		fulldirname, strerror(errno));
	return 1;
    }

    if (lstat(".", &sb2) != 0) {
	message(LOG_ERROR, "second lstat() of directory %s failed: %s\n",
		fulldirname, strerror(errno));
	return 1;
    }

    if (sb1.st_dev != sb2.st_dev) {
	message(LOG_ERROR, "device information changed for %s: %s!!!\n",
		fulldirname, strerror(errno));
	message(LOG_FATAL, "this indicates a possible intrusion attempt\n");
	return 1;
    } else if (sb1.st_ino != sb2.st_ino) {
	message(LOG_ERROR, "inode information changed for %s: %s!!!\n",
		fulldirname, strerror(errno));
	message(LOG_FATAL, "this indicates a possible intrustion attempt\n");
	return 1;
    }

    return 0;
}

/* check user function returns 0 if OK, 1 if file in use */
#ifdef FUSER
static int
check_fuser(const char *filename)
{
    static int fuser_exists = -1;
    static char *const empty_environ[] = { NULL };

    int ret = 0;			/* assume no file in use */
    int wstatus;			/* store return from waitpid */
    char dir[2 + PATH_MAX];
    int pid;

    if (fuser_exists == -1)
	fuser_exists = access(FUSER, R_OK | X_OK) == 0;
    if (!fuser_exists)
	return 0;

    /* should we close all unnecessary file descriptors here? */

    /* Use "./" to protect against filenames starting with '-' */
    snprintf(dir, sizeof(dir), "./%s", filename);
    pid = fork();
    if (pid == 0) {
#ifdef FUSER_ACCEPTS_S
	execle(FUSER, FUSER, "-s", dir, NULL, empty_environ);
#else
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);
	execle(FUSER, FUSER, dir, NULL, empty_environ);
#endif
	_exit(127);
    } else {
	waitpid(pid, &wstatus, 0);
    }

    /* assume return is 0 */
    /* Even if a file is in use, WIFEXITED can still return a normal termiination */
    if (WIFEXITED(wstatus))		/* child returned normally */
    {
        if (! WEXITSTATUS(wstatus))	/* if some error returned */
	    ret = 1;
    } else
	ret = 1;			/* abnormal exit for child */

    return ret;

}
#else
#define check_fuser(FILENAME) 0
#endif

static time_t *
max(time_t *x, time_t *y)
{
    if ( x==0 ) return y;
    if ( y==0 ) return x;

    return (*x>=*y) ? x : y;
}

#ifdef __linux
static int
is_mount_point(const char *path)
{
    FILE *fp;
    struct mntent *mnt;
    int ret;

    if ((fp = setmntent(_PATH_MOUNTED, "r")) == NULL) {
	message(LOG_ERROR, "failed to open %s for reading\n", _PATH_MOUNTED);
	return -1;
    }

    ret = 0;
    while ((mnt = getmntent(fp)) != NULL) {
	if (strcmp(mnt->mnt_dir, path) == 0) {
	    ret = 1;
	    break;
	}
    }
    endmntent(fp);

    return ret;
}
#endif

/* added shredpath */
static int
cleanupDirectory(const char * fulldirname, const char *reldirname, dev_t st_dev,
		 ino_t st_ino, const char *shredpath)
{
    DIR *dir;
    struct dirent *ent;
    struct stat sb, here;
    time_t *significant_time;
    struct utimbuf utb;
    int res;
    int pid;

    message(LOG_DEBUG, "cleaning up directory %s\n", fulldirname);

    res = safe_chdir(fulldirname, reldirname, st_dev, st_ino);
    switch (res) {
    case 0: /* OK */
	break;
	
    case 1: /* Error */
	return 0;

    case 2: /* ENOENT, silently do nothing */
	return 1;
    }

    if (lstat(".", &here) != 0) {
	message(LOG_ERROR, "error stat()ing current directory %s: %s\n",
		fulldirname, strerror(errno));
	return 0;
    }

    /* Don't cross filesystems */
    if (here.st_dev != st_dev) {
	message(LOG_ERROR, "directory %s device changed right under us!!!\n",
		fulldirname);
	message(LOG_FATAL, "this indicates a possible intrustion attempt\n");
	return 1;
    }

    /* Check '.' and expected inode */
    if (here.st_ino != st_ino) {
	message(LOG_ERROR, "directory %s inode changed right under us!!!\n",
		fulldirname);
	message(LOG_FATAL, "this indicates a possible intrusion attempt\n");
	return 1;
    }

    if ((dir = opendir(".")) == NULL) {
	message(LOG_ERROR, "opendir error on current directory %s: %s\n",
		fulldirname, strerror(errno));
	return 0;
    }

    for (;;) {
	struct exclusion *e;

	errno = 0;
	ent = readdir(dir);
	if (errno != 0) {
	    message(LOG_ERROR, "error reading directory entry: %s\n",
		    strerror(errno));
	    (void)closedir(dir);
	    return 0;
	}
	if (ent == NULL)
	    break;

	if (lstat(ent->d_name, &sb) != 0) {
	    /* FUSE mounts by different users return EACCES by default. */
	    if (errno != ENOENT && errno != EACCES)
		message(LOG_ERROR, "failed to lstat %s/%s: %s\n",
			fulldirname, ent->d_name, strerror(errno));
	    continue;
	}
	
	/* don't go crazy with the current directory or its parent */
	if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
	    continue;

	/*
	 * skip over directories named lost+found that are owned by
	 * LOSTFOUND_UID (root)
	 */
	if (strcmp(ent->d_name, "lost+found") == 0 && S_ISDIR(sb.st_mode)
	    && sb.st_uid == LOSTFOUND_UID)
	    continue;

	message(LOG_REALDEBUG, "found directory entry %s\n", ent->d_name);

	for (e = exclusions; e != NULL; e = e->next) {
	    if (strcmp(fulldirname, e->dir) == 0
		&& strcmp(ent->d_name, e->file) == 0) {
		message(LOG_REALDEBUG, "in exclusion list, skipping\n");
		break;
	    }
	}
	if (e != NULL)
	    continue;

	if (excluded_patterns != NULL) {
	    const struct excluded_pattern *ep;
	    char *full, *p;

	    full = malloc(strlen(fulldirname) + strlen(ent->d_name) + 2);
	    if (full == NULL)
		continue;
	    p = stpcpy(full, fulldirname);
	    p = stpcpy(p, "/");
	    stpcpy(p, ent->d_name);
	    for (ep = excluded_patterns; ep != NULL; ep = ep->next) {
		if (fnmatch(ep->pattern, full,
			    FNM_PATHNAME | FNM_PERIOD) == 0) {
		    message(LOG_REALDEBUG,
			    "matches exclusion pattern, skipping\n");
		    break;
		}
	    }
	    free(full);
	    if (ep != NULL)
		continue;
	}

	significant_time = 0;
	/* Set significant_time to point at the significant field of sb -
	 * either st_atime or st_mtime depending on the flag selected. - alh */
	if ((config_flags & FLAG_DIRMTIME) != 0 && S_ISDIR(sb.st_mode))
	    significant_time = max(significant_time, &sb.st_mtime);
	/* The else here (and not elsewhere) is intentional */
	else if ((config_flags & FLAG_ATIME) != 0)
	    significant_time = max(significant_time, &sb.st_atime);
	if ((config_flags & FLAG_MTIME) != 0)
	    significant_time = max(significant_time, &sb.st_mtime);
	if ((config_flags & FLAG_CTIME) != 0) {
	    /* Even when we were told to use ctime, for directories we use
	       mtime, because when a file in a directory is deleted, its
	       ctime will change, and there's no way we can change it
	       back.  Therefore, we use mtime rather than ctime so that
	       directories won't hang around for a long time after their
	       contents are removed. */
	    if (S_ISDIR(sb.st_mode))
		significant_time = max(significant_time, &sb.st_mtime);
	    else
		significant_time = max(significant_time, &sb.st_ctime);
	}
	/* What? One or the other should be set by now... */
	if (significant_time == 0) {
	    message(LOG_FATAL, "error in cleanupDirectory: no selection method "
		    "was specified\n");
	}

	message(LOG_REALDEBUG, "taking as significant time: %s",
		ctime(significant_time));

	if (sb.st_uid == 0 && (config_flags & FLAG_FORCE) == 0
	    && (sb.st_mode & S_IWUSR) == 0) {
	    message(LOG_DEBUG, "non-writeable file owned by root "
		    "skipped: %s\n", ent->d_name);;
	    continue;
	}
	/* One more check for a different device.  Try hard not to go onto a
	   different device. */
	if (sb.st_dev != st_dev || here.st_dev != st_dev) {
	    message(LOG_VERBOSE, "file on different device skipped: %s\n",
		    ent->d_name);
	    continue;
	}
	if (S_ISDIR(sb.st_mode)) {
	    int dd;

	    if ((dd = open(".", O_RDONLY)) != -1) {
		char *full_subdir;

		full_subdir = malloc(strlen(fulldirname) + strlen(ent->d_name)
				     + 2);

		if (full_subdir != NULL) {
		    strcpy(full_subdir, fulldirname);
		    strcat(full_subdir, "/");
		    strcat(full_subdir, ent->d_name);
		    if (!is_bind_mount(full_subdir)
			&& cleanupDirectory(full_subdir, ent->d_name, st_dev,
					    sb.st_ino, shredpath) == 0)
			message(LOG_ERROR, "cleanup failed in %s: %s\n",
				full_subdir, strerror(errno));
		    free(full_subdir);
		}
		if (fchdir(dd) != 0) {
		    message(LOG_ERROR, "can not return to %s: %s\n",
			    fulldirname, strerror(errno));
		    close(dd);
		    break;
		}
		close(dd);
	    } else {
		message(LOG_ERROR, "could not perform cleanup in %s/%s: %s\n",
			fulldirname, ent->d_name, strerror(errno));
	    }

	    if (*significant_time >= kill_time)
		continue;

	    if ((config_flags & FLAG_FUSER) != 0 && check_fuser(ent->d_name)) {
		message(LOG_VERBOSE, "file is already in use or open: %s\n",
			ent->d_name);
		continue;
	    }

	    /* we should try to remove the directory after cleaning up its
	       contents, as it should contain no files.  Skip if we have
	       specified the "no directories" flag. */
	    if ((config_flags & FLAG_NODIRS) == 0) {
		message(LOG_VERBOSE, "removing directory %s/%s if empty\n",
			fulldirname, ent->d_name);

		if ((config_flags & FLAG_TEST) == 0) {
		    if (rmdir(ent->d_name)) {
			/* EBUSY is returned for a mount point. */
			if (errno != ENOENT && errno != ENOTEMPTY
			    && errno != EBUSY) {
			    message(LOG_ERROR, "failed to rmdir %s/%s: %s\n",
				    fulldirname, ent->d_name, strerror(errno));
			}
		    }
		}
	    }
	} else {
	    if (S_ISSOCK(sb.st_mode)) {
		if (socket_kill_time == 0
		    || *significant_time >= socket_kill_time)
		    continue;
	    } else { /* Not a socket */
		if (*significant_time >= kill_time)
		    continue;
	    }

#ifdef __linux
	    /* check if it is an ext3 journal file */
	    if (strcmp(ent->d_name, ".journal") == 0 && sb.st_uid == 0) {
		int mount;

		mount = is_mount_point(fulldirname);
		if (mount == -1)
		    continue;
		if (mount != 0) {
		    message(LOG_VERBOSE, "skipping ext3 journal file: %s/%s\n",
			    fulldirname, ent->d_name);
		    continue;
		}
	    }
	    if (strcmp(ent->d_name, "aquota.user") == 0 ||
		strcmp(ent->d_name, "aquota.group") == 0) {
		int mount;

		mount = is_mount_point(fulldirname);
		if (mount == -1)
		    continue;
		if (mount != 0) {
		    message(LOG_VERBOSE, "skipping quota file: %s/%s\n",
			    fulldirname, ent->d_name);
		    continue;
		}
	    }
#endif

	    if ((config_flags & FLAG_ALLFILES) != 0
		|| S_ISREG(sb.st_mode) || S_ISSOCK(sb.st_mode)
		|| ((config_flags & FLAG_NOSYMLINKS) == 0
		    && S_ISLNK(sb.st_mode))) {
		const struct excluded_uid *u;

		if ((config_flags & FLAG_FUSER) != 0
		    && check_fuser(ent->d_name)) {
		    message(LOG_VERBOSE, "file is already in use or open: %s/%s\n",
			    fulldirname, ent->d_name);
		    continue;
		}

                for (u = excluded_uids; u != NULL; u = u->next) {
                    if (sb.st_uid == u->uid) {
	                message(LOG_REALDEBUG,
				"file owner excluded, skipping\n");
                        break;
                    }
                }
                if (u != NULL)
                    continue;

		if ((config_flags & FLAG_TEST) == 0) {
		    /* shred files if requested */
		    if ((config_flags & FLAG_SHRED) != 0) {
		        pid = fork();
			if (pid == 0) {
			    message(LOG_VERBOSE, "shredding file %s/%s\n",
				fulldirname, ent->d_name);
			    /* use shred verbosity according to logLevel */
			    /* force file shred if required */
			    int shredoptionsindex=0;
			    char shredoptions[4]; /* v, f, vf or nothing*/
			    if (logLevel < LOG_NORMAL)
			        shredoptionsindex=1;
			    if (config_flags & FLAG_FORCE)
				shredoptionsindex+=2;

			    switch ( shredoptionsindex ) {
			    case 1: strcpy( shredoptions, "-v" );
				    break;
			    case 2: strcpy( shredoptions, "-f" );
				    break;
			    case 3: strcpy( shredoptions, "-vf" );
				    break;
			    default:
				    break;
			    }
			    /* check if v or f required */
			    if (shredoptionsindex)
				execl( shredpath, "shred", shredoptions, ent->d_name, (char *) NULL );
			    else
				execl( shredpath, "shred", ent->d_name, (char *) NULL );

			    /* something went wrong */
			    message(LOG_ERROR, "shred program not found. No shredding will occur for files.\n");
			    _exit(-1);
			} else
			    wait(0);
		    }
		    message(LOG_VERBOSE, "removing file %s/%s\n",
			fulldirname, ent->d_name);

		    if (unlink(ent->d_name) != 0 && errno != ENOENT)
			message(LOG_ERROR, "failed to unlink %s: %s\n",
			    fulldirname, ent->d_name);
		}
	    }
	}
    }

    if (closedir(dir) == -1) {
	message(LOG_ERROR, "closedir of %s failed: %s\n",
		fulldirname, strerror(errno));
	return 0;
    }

    /* restore access time on this directory to its original time */
    utb.actime = here.st_atime; /* atime */
    utb.modtime = here.st_mtime; /* mtime */

    if (utime(".", &utb) == -1)
	message(LOG_DEBUG, "unable to reset atime/mtime for %s\n",
		fulldirname);

    return 1;
}

static void
printCopyright(void)
{
    fprintf(stderr, "tmpwatch " PACKAGE_VERSION
	    " - (C) 1997-2009 Red Hat, Inc. "
	    "All rights reserved.\n"
	    "2019-2024 Peter Hyman.\n"
	    "This program may be freely redistributed under the terms of the\n"
	    "GNU General Public License version 2.\n");
}


static void attribute__((noreturn))
usage(void)
{
    static const char msg[] = "tmpwatch [-u|-m|-c] [-MUXSadfqtvx] [--verbose] "
	"[--force] [--all] [--nodirs] [--nosymlinks] [--test] [--quiet] "
	"[--atime|--mtime|--ctime] [--dirmtime] [--exclude <path>] "
	"[--exclude-user <user>] [--exclude-pattern <pattern>] [--shred ]"
#ifdef FUSER
	"[--fuser] "
#endif
	"<hours-untouched> <dirs>\n";

    printCopyright();
    fprintf(stderr, "\n");
    fprintf(stderr, msg);
    exit(1);
}

/* Set up kill_time and socket_kill_time for GRACE_MINUTES.

   Connecting to an AF_UNIX socket does not update any of its times, so we
   can't blindly remove a socket with old times - but any process listening on
   that socket can not survive a reboot, so sockets that predate the time of
   our boot are fair game.  We still add the grace period, both as a layer of
   extra protection, and to let users examine contents of the temporary
   directory for at least for some time.
 */
static void
compute_kill_times(int grace_minutes)
{
    int grace_seconds;

    grace_seconds = grace_minutes * 60;
    message(LOG_DEBUG, "grace period is %d seconds\n", grace_seconds);

    kill_time = time(NULL) - grace_seconds;
    if ((config_flags & FLAG_ALLFILES) != 0)
	socket_kill_time = kill_time;
    else {
/* We want __linux because the behavior of CLOCK_BOOTTIME is not standardized.
   On Linux, it is the time since system boot, including the time spent in
   sleep mode or hibernation. */
#if defined (HAVE_CLOCK_GETTIME) && defined (CLOCK_BOOTTIME) && defined (__linux)
	struct timespec real_clock, boot_clock;
	time_t boot_time;

	if (clock_gettime(CLOCK_REALTIME, &real_clock) != 0
	    || clock_gettime(CLOCK_BOOTTIME, &boot_clock) != 0)
	    message(LOG_FATAL, "Error determining boot time: %s\n",
		    strerror(errno));
	boot_time = real_clock.tv_sec - boot_clock.tv_sec;
	if (real_clock.tv_nsec < boot_clock.tv_nsec)
	    boot_time--;
	/* We don't get the values of the two clocks at exactly the same moment,
	   let's add a few seconds to be extra sure. */
	boot_time -= 2;

	socket_kill_time = boot_time - grace_seconds;
#else
	socket_kill_time = 0; /* Never remove sockets */
#endif
    }
}

/* return fullpath if shred application found
 * return NULL if not */
static char * test_for_shred( void )
{
	/* check for shred existence along path
	 * then check if executable */
	char *path;
	char *curpath;
	char *fullpath=NULL;
	int accessstatus;

	path = getenv("PATH");
	if (path == NULL) return(0);

	curpath=strtok(path, ":");

	while ( curpath != NULL ) {
	    if ( (fullpath=malloc(strlen(curpath) + 7)) == NULL )
	        message(LOG_FATAL, "error allocating memory\n.");
	    strcpy(fullpath,curpath);
	    strcat(fullpath, "/shred");
	    if ( (accessstatus=access( fullpath, X_OK) ) == 0 )
		/* found shred */
		break;
	    free(fullpath);
	    fullpath=NULL;
	    curpath=strtok(NULL, ":");
	}

	return( fullpath );
}

int main(int argc, char ** argv)
{
    static const struct option options[] = {
	{ "all", 0, 0, 'a' },
	{ "nodirs", 0, 0, 'd' },
	{ "nosymlinks", 0, 0, 'l' },
	{ "force", 0, 0, 'f' },
	{ "mtime", 0, 0, 'm' },
	{ "atime", 0, 0, 'u' },
	{ "ctime", 0, 0, 'c' },
	{ "dirmtime", 0, 0, 'M' },
	{ "quiet", 0, 0, 'q' },
#ifdef FUSER
	{ "fuser", 0, 0, 's' },
#endif
	{ "test", 0, 0, 't' },
	{ "exclude-user", required_argument, 0, 'U' },
	{ "verbose", 0, 0, 'v' },
	{ "exclude", required_argument, 0, 'x' },
	{ "exclude-pattern", required_argument, 0, 'X' },
	{ "shred", 0, 0, 'S' },
	{ 0, 0, 0, 0 },
    };
    const char optstring[] = "MU:acdflmqstuvx:X:S";

    int grace;
    char units, garbage;
    int orig_dir;
    struct stat sb;
    char *shredpath=NULL; /* placeholder for shred executable, if requested */

    // set_program_name(argv[0]);
    if (argc == 1) usage();

    bind_mount_init();

    while (1) {
	int arg, long_index;

	long_index = 0;

	arg = getopt_long(argc, argv, optstring, options, &long_index);
	if (arg == -1) break;

	switch (arg) {
	case 'M':
	    config_flags |= FLAG_DIRMTIME;
	    break;
	case 'a':
	    config_flags |= FLAG_ALLFILES;
	    break;
	case 'd':
	    config_flags |= FLAG_NODIRS;
	    break;
	case 'f':
	    config_flags |= FLAG_FORCE;
	    break;
	case 'l':
	    config_flags |= FLAG_NOSYMLINKS;
	    break;
	case 's':
	    config_flags |= FLAG_FUSER;
	    break;
	case 't':
	    config_flags |= FLAG_TEST;
	    /* fallthrough */
	case 'v':
	    logLevel > 0 ? logLevel -= 1 : 0;
	    break;
	case 'q':
	    logLevel = LOG_FATAL;
	    break;
	case 'u':
	    config_flags |= FLAG_ATIME;
	    break;
	case 'm':
	    config_flags |= FLAG_MTIME;
	    break;
	case 'c':
	    config_flags |= FLAG_CTIME;
	    break;
	case 'U': {
	    struct excluded_uid *u;
	    struct passwd *pwd;

	    if ( (u = malloc(sizeof (*u))) == NULL )
	        message(LOG_FATAL, "error allocating memory\n.");
	    pwd = getpwnam(optarg);
	    if (pwd != NULL)
	        u->uid = pwd->pw_uid;
	    else {
		intmax_t imax;
		char *p;

		errno = 0;
		imax = strtoimax(optarg, &p, 10);
		if (errno != 0 || *p != 0 || p == optarg
		    || (uid_t)imax != imax)
		    message(LOG_FATAL, "unknown user %s\n", optarg);
		u->uid = imax;
	    }
	    u->next = NULL;
	    *excluded_uids_tail = u;
	    excluded_uids_tail = &u->next;
	    break;
	}
	case 'x': {
	    struct exclusion *e;
	    char *path, *p;

	    if ( (e = malloc(sizeof (*e))) == NULL )
	        message(LOG_FATAL, "error allocating memory\n.");
	    path = absolute_path(optarg, 1);
	    if (*path != '/') {
		message(LOG_ERROR, "%s is not an absolute path\n", path);
		usage();
	    }
	    p = strrchr(path, '/');
	    assert (p != NULL);
	    e->file = p + 1;
	    if (p == path)
		e->dir = "/";
	    else {
		*p = 0;
		e->dir = path;
	    }
	    e->next = NULL;
	    *exclusions_tail = e;
	    exclusions_tail = &e->next;
	    break;
	}
	case 'X': {
	    struct excluded_pattern *p;

	    if ( (p = malloc(sizeof (*p))) == NULL )
	        message(LOG_FATAL, "error allocating memory\n.");
	    p->pattern = optarg;
	    p->next = NULL;
	    *excluded_patterns_tail = p;
	    excluded_patterns_tail = &p->next;
	    break;
	}
	case 'S': {
	    /* shred files */
	    if ( (shredpath=test_for_shred()) !=NULL )
	        config_flags |= FLAG_SHRED;
	    else
		message(LOG_ERROR, "shred application not found. No file shredding will occur.\n");
	    break;
	}
	case '?':
	default:
	    usage();
	}
    }

    /* Default to atime if neither was specified. - alh */
    if ((config_flags & (FLAG_ATIME | FLAG_MTIME | FLAG_CTIME)) == 0)
	config_flags |= FLAG_ATIME;

    if (optind == argc) {
	message(LOG_FATAL, "time (in hours) must be given\n");
    }

    switch (sscanf(argv[optind], "%d%c%c", &grace, &units, &garbage)) {
    case 1:
	grace *= 60;
	break; /* hours by default */
    case 2:
	switch (units) {
	case 'd':
	    grace *= 24 * 60; /* days to minutes */
	    break;
	case 'h':
	    grace *= 60; /* hours to minutes */
	    break;
	case 'm':
	    break; /* minutes */
	default:
	    grace = -1;  /* invalid */
	}
	break;
    default:
	grace = -1; /* invalid */
    }

    if (grace < 0)
	message(LOG_FATAL, "bad time argument %s\n", argv[optind]);

    compute_kill_times(grace);

    optind++;
    if (optind == argc) {
	message(LOG_FATAL, "directory name(s) expected\n");
    }

    /* set stdout line buffered so it is flushed before each fork */
    setvbuf(stdout, NULL, _IOLBF, 0);

    orig_dir = open(".", O_RDONLY);
    if (orig_dir == -1)
	message(LOG_FATAL, "cannot open current directory\n");
    while (optind < argc) {
	char *path;

	path = absolute_path(argv[optind], 0);
	if (lstat(path, &sb) != 0) {
	    message(LOG_ERROR, "lstat() of directory %s failed: %s\n", path,
		    strerror(errno));
	    exit(1);
	}

	if (S_ISLNK(sb.st_mode)) {
	    message(LOG_DEBUG, "initial directory %s is a symlink -- "
		    "skipping\n", path);
	} else {
	    /* add shred path to call */
	    if (cleanupDirectory(path, path, sb.st_dev, sb.st_ino, shredpath) == 0)
		message(LOG_ERROR, "cleanup failed in %s: %s\n", path,
			strerror(errno));
	    if (fchdir(orig_dir) != 0) {
		message(LOG_FATAL, "can not return to original working "
			"directory: %s\n", strerror(errno));
	    }
	}
	optind++;
    }
    close(orig_dir);

    return 0;
}
