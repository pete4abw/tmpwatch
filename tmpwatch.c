/*
 * tmpwatch.c -- remove files in a directory, but do it carefully.
 * Copyright (c) 1997-2001, Red Hat, Inc.
 * Licensed under terms of the GPL.
 *
 * Authors: Erik Troan <ewt@redhat.com>
 *          Preston Brown <pbrown@redhat.com>
 *
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

#ifndef __hpux
#include <getopt.h>
#endif

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
#include <paths.h>
#include <mntent.h>

#define LOG_REALDEBUG	1
#define LOG_DEBUG	2
#define LOG_VERBOSE	3
#define LOG_NORMAL	4
#define LOG_ERROR	5
#define LOG_FATAL	6

#define FLAG_FORCE	(1 << 0)
#define FLAG_ALLFILES	(1 << 1)   /* normally just files, dirs are removed */
#define FLAG_TEST	(1 << 2)
#define FLAG_ATIME     (1 << 3)
#define FLAG_MTIME     (1 << 4)
#define FLAG_FUSER     (1 << 5)
#define FLAG_CTIME     (1 << 6)
#define FLAG_NODIRS    (1 << 7)

/* Do not remove lost+found directories if owned by this UID */
#define LOSTFOUND_UID 0

int logLevel = LOG_NORMAL;

void message(int level, char * format, ...)
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

int safe_chdir(const char *fulldirname, const char *reldirname,
	       dev_t st_dev, ino_t st_ino)
{
    struct stat64 sb1, sb2;

    if (lstat64(reldirname, &sb1)) {
	message(LOG_ERROR, "lstat() of directory %s failed: %s\n",
		fulldirname, strerror(errno));
	return 1;
    }

    if (! S_ISDIR(sb1.st_mode)) {
	message(LOG_ERROR, "directory %s changed right under us!!!",
		fulldirname);
	message(LOG_FATAL, "this indicates a possible intrusion attempt\n");
	return 1;
    }

    /* Check if the directory changed between cleanupDirectory and
     * safe_chdir
     */
    if (st_dev != sb1.st_dev || st_ino != sb1.st_ino) {
	message(LOG_ERROR, "directory %s changed right under us!!!",
		fulldirname);
	message(LOG_FATAL, "this indicates a possible intrusion attempt\n");
	return 1;
    }

    if (chdir(reldirname)) {
	message(LOG_ERROR, "chdir to directory %s failed: %s\n",
		fulldirname, strerror(errno));
	return 1;
    }

    if (lstat64(".", &sb2)) {
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

int check_fuser(const char *dirname, const char *filename)
{
    int ret;
    char dir[FILENAME_MAX];
    int pid;
    extern int errno;

    /* should we close all unnecessary file descriptors here? */
    
    snprintf(dir, sizeof(dir), "%s", filename);
    pid = fork();
    if (pid == 0) {
    	ret = execle("/sbin/fuser", "/sbin/fuser", "-s", dir, NULL, NULL);
    } else {
	waitpid(pid, &ret, 0);
    }

    /* flip-flop: fuser returns zero if the device or file is being accessed */
    return (WIFEXITED(ret) && (WEXITSTATUS(ret) == 0));
}

time_t *max( time_t *x, time_t *y )
{
    if ( x==0 ) return y;
    if ( y==0 ) return x;

    return (*x>=*y) ? x : y;
}

int cleanupDirectory(const char * fulldirname, const char *reldirname,
		     unsigned int killTime, int flags,
		     dev_t st_dev, ino_t st_ino)
{
    DIR *dir;
    struct dirent *ent;
    struct stat64 sb;
    time_t *significant_time;
    struct stat64 here;
    struct utimbuf utb;

    message(LOG_DEBUG, "cleaning up directory %s\n", fulldirname);
  
    if (safe_chdir(fulldirname, reldirname, st_dev, st_ino))
	return 0;

    if (lstat64(".", &here)) {
	message(LOG_ERROR, "error statting current directory %s: %s",
		fulldirname, strerror(errno));
	return 0;
    }

    /* Don't cross filesystems */
    if (here.st_dev != st_dev) {
	message(LOG_ERROR, "directory %s device changed right under us!!!\n");
	message(LOG_FATAL, "this indicates a possible intrustion attempt\n");
	return 1;
    }

    /* Check '.' and expected inode */
    if (here.st_ino != st_ino) {
	message(LOG_ERROR, "directory %s inode changed right under us!!!\n");
	message(LOG_FATAL, "this indicates a possible intrusion attempt\n");
	return 1;
    }

    if ((dir = opendir(".")) == NULL) {
	message(LOG_ERROR, "opendir error on current directory %s: %s",
		fulldirname, strerror(errno));
	return 0;
    }

    do {
	errno = 0;
	ent = readdir(dir);
	if (errno) {
	    message(LOG_ERROR, "error reading directory entry: %s\n", 
		    strerror(errno));
	    return 0;
	}
	if (!ent) break;

	if (lstat64(ent->d_name, &sb)) {
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

	significant_time = 0;
	/* Set significant_time to point at the significant field of sb -
	 * either st_atime or st_mtime depending on the flag selected. - alh */
	if (flags & FLAG_ATIME)
	    significant_time = max(significant_time, &sb.st_atime);
	if (flags & FLAG_MTIME)
	    significant_time = max(significant_time, &sb.st_mtime);
	if (flags & FLAG_CTIME) {
	    /* Even when we were told to use ctime, for directories we use
	       mtime, because when a file in a directory is deleted, its
	       ctime will change, and there's no way we can change it
	       back.  Therefore, we use mtime rather than ctime so that
	       directories won't hang around for along time after their
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
	    /* One more check for different device.  Try hard tno to go
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
		fchdir(dd);
		close(dd);
	    } else {
		message(LOG_ERROR, "could not perform cleanup in %s/%s: %s\n",
			fulldirname, ent->d_name, strerror(errno));
	    }

	    if (*significant_time >= killTime)
		continue;

	    if ((flags & FLAG_FUSER) &&
		(access("/sbin/fuser", R_OK | X_OK) == 0) &&
		check_fuser(fulldirname, ent->d_name)) {
		message(LOG_VERBOSE, "file is already in use or open: %s\n",
			ent->d_name);
		continue;
	    }

	    /* we should try to remove the directory after cleaning up its
	       contents, as it should contain no files.  Skip if we have
	       specified the "no directories" flag. */
	    if (!(flags & FLAG_NODIRS)) {
		message(LOG_VERBOSE, "removing directory %s/%s\n",
			fulldirname, ent->d_name);

		if (!(flags & FLAG_TEST)) {
		    if (rmdir(ent->d_name)) {
			if (errno != ENOTEMPTY) {
			    message(LOG_ERROR, "failed to rmdir %s/%s: %s", 
				    fulldirname, ent->d_name, strerror(errno));
			}
		    }
		}
	    }
	} else {
	    if (*significant_time >= killTime)
		continue;

	    /* check if it is an ext3 journal file */
	    if ((strcmp(ent->d_name, ".journal") == 0) &&
		(sb.st_uid == 0)) {
		FILE *fp;
		struct mntent *mnt;
		int foundflag = 0;
		
		if ((fp = setmntent(_PATH_MOUNTED, "r")) != 0) {
		    message(LOG_ERROR, "failed to open %s for reading\n",
			    _PATH_MOUNTED);
		    continue;
		}
		
		while ((mnt = getmntent(fp)) != NULL) {
		    /* is it in the root of the filesystem? */
		    if (strcmp(mnt->mnt_dir, fulldirname) == 0) {
			foundflag = 1;
			break;
		    }
		}
		endmntent(fp);

		if (foundflag) {
		    message(LOG_VERBOSE, "skipping ext3 journal file: %s/%s\n",
			    fulldirname, ent->d_name);
		    continue;
		}
	    }
	    
	    if ((flags & FLAG_ALLFILES) ||
		S_ISREG(sb.st_mode) ||
		S_ISLNK(sb.st_mode)) {
		if (flags & FLAG_FUSER && !access("/sbin/fuser", R_OK|X_OK) &&
		    check_fuser(fulldirname, ent->d_name)) {
		    message(LOG_VERBOSE, "file is already in use or open: %s/%s\n",
			    fulldirname, ent->d_name);
		    continue;
		}
	    
		message(LOG_VERBOSE, "removing file %s/%s\n",
			fulldirname, ent->d_name);
	    
		if (!(flags & FLAG_TEST)) {
		    if (unlink(ent->d_name)) 
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

void printCopyright(void) {
    fprintf(stderr, "tmpwatch " VERSION " - (c) 1997-2001 Red Hat, Inc.\n");
    fprintf(stderr, "This may be freely redistributed under the terms of "
	    "the GNU Public License.\n");
}


void usage(void) {
    printCopyright();
    fprintf(stderr, "\n");
#ifndef __hpux
    fprintf(stderr, "tmpwatch [-u|-m|-c] [-adfqtv] [--verbose] [--force] [--all] [--nodirs] [--test] [--quiet] [--atime|--mtime|--ctime] <hours-untouched> <dirs>\n");
#else
    fprintf(stderr, "tmpwatch [-u|-m|-c] [-adfqtv] <hours-untouched> <dirs>\n");
#endif
    exit(1);
}

int main(int argc, char ** argv) {
    unsigned int grace;
    unsigned int killTime, long_index;
    int flags = 0, arg;
    struct stat64 sb;
#ifndef __hpux
    struct option options[] = {
	{ "all", 0, 0, 'a' },
	{ "nodirs", 0, 0, 'd' },
	{ "force", 0, 0, 'f' },
	{ "mtime", 0, 0, 'm' },
	{ "atime", 0, 0, 'u' },
	{ "ctime", 0, 0, 'c' },
	{ "quiet", 0, 0, 'q' },
	{ "fuser", 0, 0, 's' },
	{ "test", 0, 0, 't' },
	{ "verbose", 0, 0, 'v' },
	{ 0, 0, 0, 0 }, 
    };
#endif
    const char optstring[] = "adcfmqstuv";

    if (argc == 1) usage();

    while (1) {
	long_index = 0;

#ifndef __hpux
	arg = getopt_long(argc, argv, optstring, options, &long_index);
#else
	arg = getopt(argc, argv, optstring);
#endif
	if (arg == -1) break;

	switch (arg) {
	case 'a':
	    flags |= FLAG_ALLFILES;
	    break;

	case 'd':
	    flags |= FLAG_NODIRS;
	    break;
	case 'f':
	    flags |= FLAG_FORCE;
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
	case '?':
	    usage();
	}
    }
  
    /* Default to atime if neither was specified. - alh */
    if (!(flags & (FLAG_ATIME | FLAG_MTIME | FLAG_CTIME)))
	flags |= FLAG_ATIME;

    if (optind == argc) {
	message(LOG_FATAL, "time (in hours) must be given\n");
    }

    if ((sscanf(argv[optind], "%d", &grace) != 1) || (grace < 0)) {
	message(LOG_FATAL, "bad time argument %s\n", argv[optind]);
    }

    optind++;
    if (optind == argc) {
	message(LOG_FATAL, "directory name(s) expected\n");
    }

    grace = grace * 3600;			/* to seconds from hours */

    message(LOG_DEBUG, "grace period is %u\n", grace);

    killTime = time(NULL) - grace;

    /* set stdout line buffered so it is flushed before each fork */
    setvbuf(stdout, NULL, _IOLBF, 0);
      
    while (optind < argc) {
	if (lstat64(argv[optind], &sb)) {
	    message(LOG_ERROR, "lstat() of directory %s failed: %s\n",
		    argv[optind], strerror(errno));
	    exit(1);
	}

	if (S_ISLNK(sb.st_mode)) {
	    message(LOG_DEBUG, "initial directory %s is a symlink -- "
		    "skipping\n", argv[optind]);
	} else {
	    if(cleanupDirectory(argv[optind], argv[optind],
				killTime, flags,
				sb.st_dev, sb.st_ino) == 0) {
		message(LOG_ERROR, "cleanup failed in %s: %s\n", argv[optind],
			strerror(errno));
	    }
	}
	optind++;
    }

    return 0;
}
