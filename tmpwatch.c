#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* tmpwatch.c -- remove files in a directory, but do it carefully */

#define LOG_REALDEBUG	1
#define LOG_DEBUG	2
#define LOG_VERBOSE	3
#define LOG_NORMAL	4
#define LOG_ERROR	5
#define LOG_FATAL	6

#define FLAGS_FORCE	(1 << 0)
#define FLAGS_ALLFILES	(1 << 1)   /* normally just files, dirs are removed */
#define FLAGS_TEST	(1 << 2)

#define GETOPT_TEST	1000

int logLevel = LOG_NORMAL;

void message(int level, char * format, ...) {
    va_list args;
    FILE * where = stdout;

    if (level >= logLevel) {
	va_start(args, format);

	if (level > LOG_NORMAL) {
	    where = stderr;
	    fprintf(stderr, "error: ");
	}

	vfprintf(stdout, format, args);

	if (level == LOG_FATAL) exit(1);
    }
}

int safe_chdir(char * dirname) {
    struct stat sb1, sb2;

    if (lstat(dirname, &sb1)) {
	message(LOG_ERROR, "lstat() of directory %s failed: %s\n",
		dirname, strerror(errno));
	return 1;
    }

    if (chdir(dirname)) {
	message(LOG_ERROR, "chdir to directory %s failed: %s\n",
		dirname, strerror(errno));
	return 1;
    }

    if (lstat(".", &sb2)) {
	message(LOG_ERROR, "second lstat() of directory %s failed: %s\n",
		dirname, strerror(errno));
	return 1;
    }

    if (sb1.st_ino != sb2.st_ino) {
	message(LOG_ERROR, "inode information changed for %s!!!",
		strerror(errno));
	message(LOG_FATAL, "this indicates a possible intrusion attempt\n");
    } else if (sb1.st_dev != sb2.st_dev) {
	message(LOG_ERROR, "device information changed for %s!!!",
		strerror(errno));
	message(LOG_FATAL, "this indicates a possible intrusion attempt\n");
    }

    return 0;
}

int cleanupDirectory(char * dirname, unsigned int killTime, int flags) {
    DIR * dir;
    struct dirent * ent;
    struct stat sb;
    int status, pid;
    struct stat here;

    message(LOG_DEBUG, "cleaning up directory %s\n", dirname);

    /* Do everything in a child process so we don't have to chdir(".."),
       which would lead to a race condition. fork() on Linux is very efficient
       so this shouldn't be a big deal (probably just a exception on one page
       of stack, not bad). I should probably just keep a directory stack
       and fchdir() back up it, but it's not worth changing now. */

    if (!(pid = fork())) {
	if (safe_chdir(dirname)) return 1;
	dir = opendir(".");

	if (lstat(".", &here)) {
	    message(LOG_ERROR, "error statting current directory %s: %s",
		    dirname, strerror(errno));
	    exit(1);
	}

	if (!dir) {
	    message(LOG_ERROR, "error opening directory %s: %s\n", dirname,
		    strerror(errno));
	    exit(1);
	}

	do {
	    errno = 0;
	    ent = readdir(dir);
	    if (errno) {
		message(LOG_ERROR, "error reading directory entry: %s\n", 
			strerror(errno));
		exit(1);
	    }
	    if (!ent) break;

	    if ((ent->d_name[0] == '.' && (ent->d_name[1] == '\0' || 
		((ent->d_name[1] == '.') && (ent->d_name[2] == '\0'))))) 
		continue;

	    message(LOG_REALDEBUG, "found directory entry %s\n", ent->d_name);

	    if (lstat(ent->d_name, &sb)) {
		message(LOG_ERROR, "failed to lstat %s/%s: %s\n", dirname, 
			ent->d_name, strerror(errno));
		continue;
	    }

	    if (!sb.st_uid && !(flags & FLAGS_FORCE) && 
			!(sb.st_mode & S_IWUSR)) {
		message(LOG_DEBUG, "non-writeable file owned by root "
			"skipped: %s\n", ent->d_name);;
		continue;
	    } else if (sb.st_dev != here.st_dev) {
		message(LOG_VERBOSE, "file on different device skipped: %s\n",
			ent->d_name);
		continue;
	    } else if (S_ISDIR(sb.st_mode)) {
		cleanupDirectory(ent->d_name, killTime, flags);

		if (sb.st_atime >= killTime) continue;

		message(LOG_VERBOSE, "removing directory %s\n", ent->d_name);

		if (!(flags & FLAGS_TEST)) {
		    if (flags & FLAGS_ALLFILES) {
			if (rmdir(ent->d_name)) {
			    message(LOG_ERROR, "failed to rmdir %s: %s\n", 
					dirname, ent->d_name);
			}
		    } else {
			rmdir(ent->d_name);
		    }
		}
	    } else {
		if (sb.st_atime >= killTime) continue;

		if ((flags & FLAGS_ALLFILES) || S_ISREG(sb.st_mode)) {
		    message(LOG_VERBOSE, "removing file %s/%s\n", 
			    dirname, ent->d_name);

		    if (!(flags & FLAGS_TEST)) {
			if (unlink(ent->d_name)) 
			    message(LOG_ERROR, "failed to unlink %s: %s\n", 
					dirname, ent->d_name);
		    }
		}
	    }

	} while (ent);

	closedir(dir);

	exit(0);
    }

    waitpid(pid, &status, 0);

    if (WIFEXITED(status))
	return WEXITSTATUS(status);

    return 0;
}

void printCopyright(void) {
    fprintf(stderr, "tmpwatch " VERSION " - (c) 1997 Red Hat Software\n");
    fprintf(stderr, "This may be freely redistributed under the terms of "
			"the GNU Public License.\n");
}

void usage(void) {
    printCopyright();
    fprintf(stderr, "\n");
    fprintf(stderr, "tmpwatch [-fav] [--verbose] [--force] [--all] [--test] <hours-untouched> <dirs>\n");
    exit(1);
}

int main(int argc, char ** argv) {
    unsigned int grace;
    unsigned int killTime, long_index;
    int flags = 0, arg;
    struct stat sb;
    struct option options[] = {
	    { "all", 0, 0, 'a' },
	    { "force", 0, 0, 'f' },
	    { "test", 0, 0, GETOPT_TEST },
	    { "verbose", 0, 0, 'v' },
    };

    if (argc == 1) usage();

    while (1) {
	long_index = 0;

	arg = getopt_long(argc, argv, "afv", options, 
			  &long_index);
	if (arg == -1) break;

	switch (arg) {
	  case 'a':
	    flags |= FLAGS_ALLFILES;
	    break;

	  case 'f':
	    flags |= FLAGS_FORCE;
	    break;

	  case GETOPT_TEST:
	    flags |= FLAGS_TEST;
	    /* fallthrough */
	  case 'v':
	    logLevel ? logLevel -= 1 : 0;
	    break;

	  case '?':
	    exit(1);
	}
    }

    if (optind == argc) {
	fprintf(stderr, "error: time (in hours) must be given\n");
	exit(1);
    }

    if ((sscanf(argv[optind], "%d", &grace) != 1) || (grace < 0)) {
	fprintf(stderr, "error: bad time argument %s\n", argv[optind]);
	exit(1);
    }

    optind++;
    if (optind == argc) {
	fprintf(stderr, "error: directory name(s) expected\n");
	exit(1);
    }

    grace = grace * 3600;			/* to seconds from hours */

    message(LOG_DEBUG, "grace period is %u\n", grace);

    killTime = time(NULL) - grace;

    while (optind < argc) {
	if (lstat(argv[optind], &sb)) {
	    message(LOG_ERROR, "lstat() of directory %s failed: %s\n",
		    argv[optind], strerror(errno));
	    exit(1);
	}

	if (S_ISLNK(sb.st_mode)) {
	    message(LOG_DEBUG, "initial directory %s is a symlink -- "
			"skipping\n", argv[optind]);
	} else {
	    cleanupDirectory(argv[optind], killTime, flags);
	}

	optind++;
    }

    return 0;
}
