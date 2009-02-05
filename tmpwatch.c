/*
 * tmpwatch.c -- remove files in a directory, but do it carefully.
 *
 * Copyright (C) 1997-2001, 2004-2008 Red Hat, Inc.  All rights reserved.
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

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
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

#ifdef __sun
#include <sys/mntent.h>
#else
#include <mntent.h>
#endif

#ifdef __linux
#include <paths.h>
#endif

#ifdef __GNUC__
#define attribute__(X) __attribute__ (X)
#else
#define attribute__(X)
#endif

#define FUSER_PATH "/sbin/fuser"
#define FUSER_ARGS "-s"

#define LOG_REALDEBUG	1
#define LOG_DEBUG	2
#define LOG_VERBOSE	3
#define LOG_NORMAL	4
#define LOG_ERROR	5
#define LOG_FATAL	6

#define FLAG_FORCE	(1 << 0)
#define FLAG_ALLFILES	(1 << 1)   /* normally just files, dirs are removed */
#define FLAG_TEST	(1 << 2)
#define FLAG_ATIME	(1 << 3)
#define FLAG_MTIME	(1 << 4)
#define FLAG_FUSER	(1 << 5)
#define FLAG_CTIME	(1 << 6)
#define FLAG_NODIRS	(1 << 7)
#define FLAG_NOSYMLINKS (1 << 8)
#define FLAG_DIRMTIME	(1 << 9)

/* Do not remove lost+found directories if owned by this UID */
#define LOSTFOUND_UID 0

struct exclusion
{
    struct exclusion *next;
    char *dir, *file;
};

static struct exclusion *exclusions /* = NULL */;
static struct exclusion **exclusions_tail = &exclusions;

struct excluded_uid
{
    struct excluded_uid *next;
    uid_t uid;
};

static struct excluded_uid *excluded_uids /* = NULL */;
static struct excluded_uid **excluded_uids_tail = &excluded_uids;

int logLevel = LOG_NORMAL;

void attribute__((format(printf, 2, 3)))
  message(int level, char * format, ...)
{
    va_list args;
    FILE * where = stdout;

    if (level >= logLevel) {
	va_start(args, format);

	if (level > LOG_NORMAL) {
	    where = stderr;
	    fprintf(where, "error: ");
	}

	vfprintf(where, format, args);

	if (level == LOG_FATAL) exit(1);
    }
}

static void *
xmalloc(size_t size)
{
    void *p;

    p = malloc (size);
    if (p != NULL || size == 0)
	return p;
    message(LOG_FATAL, "can not allocate memory\n");
    /* NOTREACHED */
    return NULL;
}

static char *
absolute_path(const char *path, int allow_nonexistent)
{
    char buf[PATH_MAX + 1], *res;
    const char *src;

    src = realpath(path, buf);
    if (src == NULL) {
	if (allow_nonexistent != 0)
	    src = path;
	else
	    message(LOG_FATAL, "cannot resolve %s: %s\n", path,
		    strerror(errno));
    }
    res = strdup(src);
    if (res == NULL)
	message(LOG_FATAL, "can not allocate memory\n");
    return res;
}

/* Returns 0 if OK, 2 on ENOENT, 1 on other errors */
int safe_chdir(const char *fulldirname, const char *reldirname,
	       dev_t st_dev, ino_t st_ino)
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

    if (chdir(reldirname)) {
	if (errno == ENOENT)
	    return 2;
	message(LOG_ERROR, "chdir to directory %s failed: %s\n",
		fulldirname, strerror(errno));
	return 1;
    }

    if (lstat(".", &sb2)) {
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

int check_fuser(const char *filename)
{
    int ret;
    char dir[FILENAME_MAX];
    int pid;

    /* should we close all unnecessary file descriptors here? */
    
    snprintf(dir, sizeof(dir), "%s", filename);
    pid = fork();
    if (pid == 0) {
    	execle(FUSER_PATH, FUSER_PATH, FUSER_ARGS, dir, NULL, NULL);
	_exit(127);
    } else {
	waitpid(pid, &ret, 0);
    }

    return (WIFEXITED(ret) && (WEXITSTATUS(ret) == 0));
}

time_t *max( time_t *x, time_t *y )
{
    if ( x==0 ) return y;
    if ( y==0 ) return x;

    return (*x>=*y) ? x : y;
}

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

int cleanupDirectory(const char * fulldirname, const char *reldirname,
		     time_t killTime, int flags, dev_t st_dev, ino_t st_ino)
{
    DIR *dir;
    struct dirent *ent;
    struct stat sb, here;
    time_t *significant_time;
    struct utimbuf utb;
    int res;

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

    if (lstat(".", &here)) {
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

    do {
	struct exclusion *e;

	errno = 0;
	ent = readdir(dir);
	if (errno) {
	    message(LOG_ERROR, "error reading directory entry: %s\n", 
		    strerror(errno));
	    return 0;
	}
	if (!ent) break;

	if (lstat(ent->d_name, &sb)) {
	    if (errno != ENOENT)
		message(LOG_ERROR, "failed to lstat %s/%s: %s\n",
			fulldirname, ent->d_name, strerror(errno));
	    continue;
	}
	
	/* don't go crazy with the current directory or its parent */
	if ((strcmp(ent->d_name, ".") == 0) ||
	    (strcmp(ent->d_name, "..") == 0))
	    continue;
      
	/*
	 * skip over directories named lost+found that are owned by
	 * LOSTFOUND_UID (root)
	 */
	if ((strcmp(ent->d_name, "lost+found") == 0) &&
	    S_ISDIR(sb.st_mode) && (sb.st_uid == LOSTFOUND_UID))
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

	significant_time = 0;
	/* Set significant_time to point at the significant field of sb -
	 * either st_atime or st_mtime depending on the flag selected. - alh */
	if ((flags & FLAG_DIRMTIME) && S_ISDIR(sb.st_mode))
	    significant_time = max(significant_time, &sb.st_mtime);
	/* The else here (and not elsewhere) is intentional */
	else if (flags & FLAG_ATIME)
	    significant_time = max(significant_time, &sb.st_atime);
	if (flags & FLAG_MTIME)
	    significant_time = max(significant_time, &sb.st_mtime);
	if (flags & FLAG_CTIME) {
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

	if (!sb.st_uid && !(flags & FLAG_FORCE) && !(sb.st_mode & S_IWUSR)) {
	    message(LOG_DEBUG, "non-writeable file owned by root "
		    "skipped: %s\n", ent->d_name);;
	    continue;
	} else
	    /* One more check for a different device.  Try hard not to go
	       onto a different device.
	    */
	    if (sb.st_dev != st_dev || here.st_dev != st_dev) {
	    message(LOG_VERBOSE, "file on different device skipped: %s\n",
		    ent->d_name);
	    continue;
	} else if (S_ISDIR(sb.st_mode)) {
	    int dd;

	    if ((dd = open(".", O_RDONLY)) != -1) {
		char *dir;
		
		dir = malloc(strlen(fulldirname) + strlen(ent->d_name) + 2);
		
		if (dir != NULL) {
		    strcpy(dir, fulldirname);
		    strcat(dir, "/");
		    strcat(dir, ent->d_name);
		    if (cleanupDirectory(dir, ent->d_name,
					 killTime, flags,
					 st_dev, sb.st_ino) == 0) {
			message(LOG_ERROR, "cleanup failed in %s: %s\n", dir,
				strerror(errno));
		    }
		    free(dir);
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

	    if (*significant_time >= killTime)
		continue;

	    if ((flags & FLAG_FUSER) &&
		(access(FUSER_PATH, R_OK | X_OK) == 0) &&
		check_fuser(ent->d_name)) {
		message(LOG_VERBOSE, "file is already in use or open: %s\n",
			ent->d_name);
		continue;
	    }

	    /* we should try to remove the directory after cleaning up its
	       contents, as it should contain no files.  Skip if we have
	       specified the "no directories" flag. */
	    if (!(flags & FLAG_NODIRS)) {
		message(LOG_VERBOSE, "removing directory %s/%s if empty\n",
			fulldirname, ent->d_name);

		if (!(flags & FLAG_TEST)) {
		    if (rmdir(ent->d_name)) {
			if (errno != ENOENT && errno != ENOTEMPTY) {
			    message(LOG_ERROR, "failed to rmdir %s/%s: %s\n", 
				    fulldirname, ent->d_name, strerror(errno));
			}
		    }
		}
	    }
	} else {
	    if (*significant_time >= killTime)
		continue;

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

	    if ((flags & FLAG_ALLFILES)
		|| S_ISREG(sb.st_mode)
		|| (!(flags & FLAG_NOSYMLINKS) && S_ISLNK(sb.st_mode))) {
		const struct excluded_uid *u;

		if (flags & FLAG_FUSER && !access(FUSER_PATH, R_OK|X_OK) &&
		    check_fuser(ent->d_name)) {
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

		message(LOG_VERBOSE, "removing file %s/%s\n",
			fulldirname, ent->d_name);
	    
		if (!(flags & FLAG_TEST)) {
		    if (unlink(ent->d_name) && errno != ENOENT) 
			message(LOG_ERROR, "failed to unlink %s: %s\n", 
				fulldirname, ent->d_name);
		}
	    }
	}
    } while (ent);

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

static void printCopyright(void)
{
    fprintf(stderr, "tmpwatch " VERSION " - (C) 1997-2008 Red Hat, Inc. "
	    "All rights reserved.\n"
	    "This program may be freely redistributed under the terms of the\n"
	    "GNU General Public License version 2.\n");
}


void usage(void)
{
    printCopyright();
    fprintf(stderr, "\n");
    fprintf(stderr, "tmpwatch [-u|-m|-c] [-MUadfqtvx] [--verbose] [--force] [--all] [--nodirs] [--nosymlinks] [--test] [--quiet] [--atime|--mtime|--ctime] [--dirmtime] [--exclude <path>] [--exclude-user <user>] <hours-untouched> <dirs>\n");
    exit(1);
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
	{ "fuser", 0, 0, 's' },
	{ "test", 0, 0, 't' },
	{ "exclude-user", required_argument, 0, 'U' },
	{ "verbose", 0, 0, 'v' },
	{ "exclude", required_argument, 0, 'x' },
	{ 0, 0, 0, 0 },
    };
    const char optstring[] = "MU:acdflmqstuvx:";

    int grace;
    char units, garbage;
    time_t killTime;
    int flags = 0, orig_dir;
    struct stat sb;

    if (argc == 1) usage();

    while (1) {
	int arg, long_index;

	long_index = 0;

	arg = getopt_long(argc, argv, optstring, options, &long_index);
	if (arg == -1) break;

	switch (arg) {
	case 'M':
	    flags |= FLAG_DIRMTIME;
	    break;
	case 'a':
	    flags |= FLAG_ALLFILES;
	    break;
	case 'd':
	    flags |= FLAG_NODIRS;
	    break;
	case 'f':
	    flags |= FLAG_FORCE;
	    break;
	case 'l':
	    flags |= FLAG_NOSYMLINKS;
	    break;
	case 's':
	    flags |= FLAG_FUSER;
	    break;
	case 't':
	    flags |= FLAG_TEST;
	    /* fallthrough */
	case 'v':
	    logLevel > 0 ? logLevel -= 1 : 0;
	    break;
	case 'q':
	    logLevel = LOG_FATAL;
	    break;
	case 'u':
	    flags |= FLAG_ATIME;
	    break;
	case 'm':
	    flags |= FLAG_MTIME;
	    break;
	case 'c':
	    flags |= FLAG_CTIME;
	    break;
	case 'U': {
	    struct excluded_uid *u;
	    struct passwd *pwd;

	    u = xmalloc(sizeof (*u));
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

	    e = xmalloc(sizeof (*e));
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
	case '?':
	default:
	    usage();
	}
    }
  
    /* Default to atime if neither was specified. - alh */
    if (!(flags & (FLAG_ATIME | FLAG_MTIME | FLAG_CTIME)))
	flags |= FLAG_ATIME;

    if (optind == argc) {
	message(LOG_FATAL, "time (in hours) must be given\n");
    }

    switch (sscanf(argv[optind], "%d%c%c", &grace, &units, &garbage)) {
    case 1:
	break; /* hours by default */
    case 2:
	switch (units) {
	case 'd':
	    grace *= 24; /* days to hours */
	    break;
	case 'h':
	    break; /* hours */
	default:
	    grace = -1;  /* invalid */
	}
	break;
    default:
	grace = -1; /* invalid */
    }

    if (grace < 0)
	message(LOG_FATAL, "bad time argument %s\n", argv[optind]);

    optind++;
    if (optind == argc) {
	message(LOG_FATAL, "directory name(s) expected\n");
    }

    grace = grace * 3600;			/* to seconds from hours */

    message(LOG_DEBUG, "grace period is %d seconds\n", grace);

    killTime = time(NULL) - grace;

    /* set stdout line buffered so it is flushed before each fork */
    setvbuf(stdout, NULL, _IOLBF, 0);

    orig_dir = open(".", O_RDONLY);
    if (orig_dir == -1)
	message(LOG_FATAL, "cannot open current directory\n");
    while (optind < argc) {
	char *path;

	path = absolute_path(argv[optind], 0);
	if (lstat(path, &sb)) {
	    message(LOG_ERROR, "lstat() of directory %s failed: %s\n", path,
		    strerror(errno));
	    exit(1);
	}

	if (S_ISLNK(sb.st_mode)) {
	    message(LOG_DEBUG, "initial directory %s is a symlink -- "
		    "skipping\n", path);
	} else {
	    if (cleanupDirectory(path, path, killTime, flags, sb.st_dev,
				 sb.st_ino) == 0) {
		message(LOG_ERROR, "cleanup failed in %s: %s\n", path,
			strerror(errno));
	    }
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
