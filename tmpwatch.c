/*
 * tmpwatch.c -- remove files in a directory, but do it carefully.
 * Copyright (c) 1997-2000, Red Hat, Inc.
 * Licensed under terms of the GPL.
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
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <utime.h>
#include <unistd.h>

#define LOG_REALDEBUG	1
#define LOG_DEBUG	2
#define LOG_VERBOSE	3
#define LOG_NORMAL	4
#define LOG_ERROR	5
#define LOG_FATAL	6

#define FLAGS_FORCE	(1 << 0)
#define FLAGS_ALLFILES	(1 << 1)   /* normally just files, dirs are removed */
#define FLAGS_TEST	(1 << 2)
#define FLAGS_ATIME     (1 << 3)
#define FLAGS_MTIME     (1 << 4)
#define FLAGS_FUSER     (1 << 5)
#define FLAGS_CTIME     (1 << 6)

int logLevel = LOG_NORMAL;

void message(int level, char * format, ...) {
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

int safe_chdir(char * dirname) {
  struct stat sb1, sb2;

  if (lstat(dirname, &sb1)) {
    message(LOG_ERROR, "lstat() of directory %s failed: %s\n",
	    dirname, strerror(errno));
    return 1;
  }

  if (! S_ISDIR(sb1.st_mode)) {
    message(LOG_ERROR, "directory %s changed right under us!!!",
	    dirname);
    message(LOG_FATAL, "this indicates a possible intrusion attempt\n");
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
    message(LOG_ERROR, "inode information changed for %s: %s!!!\n",
	    dirname, strerror(errno));
    message(LOG_FATAL, "this indicates a possible intrusion attempt\n");
    return 1;
  } else if (sb1.st_dev != sb2.st_dev) {
    message(LOG_ERROR, "device information changed for %s: %s!!!\n",
	    dirname, strerror(errno));
    message(LOG_FATAL, "this indicates a possible intrusion attempt\n");
    return 1;
  }

  return 0;
}

int check_fuser(const char *dirname, const char *filename)
{
    int ret;
    char cmd[255];
    snprintf(cmd, 255, "/sbin/fuser -s \"%s/%s\" > /dev/null 2>&1",
	     dirname, filename);
    ret = system(cmd);

    // flip flop
    return (ret == 0);
}

int cleanupDirectory(char * dirname, unsigned int killTime, int flags)
{
  DIR *dir;
  struct dirent *ent;
  struct stat sb;
  time_t *significant_time = NULL;
  struct stat here;
  struct utimbuf utb;

  message(LOG_DEBUG, "cleaning up directory %s\n", dirname);
  
  if (safe_chdir(dirname))
    return 0;

  if (lstat(".", &here)) {
    message(LOG_ERROR, "error statting current directory %s: %s",
	    dirname, strerror(errno));
    return 0;
  }

  if ((dir = opendir(".")) == NULL) {
    message(LOG_ERROR, "opendir error on current directory %s: %s",
	    dirname, strerror(errno));
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

    /* don't go crazy with the current directory or its parent */
    if ((strcmp(ent->d_name, ".") == 0) || (strcmp(ent->d_name, "..") == 0))
      continue;
      
    /* skip over directories named lost+found */
    if ((strcmp(ent->d_name, "lost+found") == 0) && S_ISDIR(sb.st_mode))
      continue;

    message(LOG_REALDEBUG, "found directory entry %s\n", ent->d_name);

    if (lstat(ent->d_name, &sb)) {
      message(LOG_ERROR, "failed to lstat %s/%s: %s\n", dirname, 
	      ent->d_name, strerror(errno));
      continue;
    }

    /* Set significant_time to point at the significant field of sb -
     * either st_atime or st_mtime depending on the flag selected. - alh */
    if (flags & FLAGS_ATIME)
      significant_time = &sb.st_atime;
    else if (flags & FLAGS_MTIME)
      significant_time = &sb.st_mtime;
    else if (flags & FLAGS_CTIME) {
      /* Even when we were told to use ctime, for directories we use
	 mtime, because when a file in a directory is deleted, its
	 ctime will change, and there's no way we can change it
	 back.  Therefore, we use mtime rather than ctime so that
	 directories won't hang around for along time after their
	 contents are removed. */
      if (S_ISDIR(sb.st_mode))
	significant_time = &sb.st_mtime;
      else
	significant_time = &sb.st_ctime;
    }
    /* What? One or the other should be set by now... */
    else {
      message(LOG_FATAL, "error in cleanupDirectory: no selection method "
	      "was specified\n");
    }

    if (!sb.st_uid && !(flags & FLAGS_FORCE) && !(sb.st_mode & S_IWUSR)) {
      message(LOG_DEBUG, "non-writeable file owned by root "
	      "skipped: %s\n", ent->d_name);;
      continue;
    } else if (sb.st_dev != here.st_dev) {
      message(LOG_VERBOSE, "file on different device skipped: %s\n",
	      ent->d_name);
      continue;
    } else if (S_ISDIR(sb.st_mode)) {
      int dd = open(".", O_RDONLY);
      if(dd != -1) {
        char *dir;
	dir = malloc(strlen(dirname) + strlen(ent->d_name) + 2);
	if(dir != NULL) {
          strcpy(dir, dirname);
          strcat(dir, "/");
	  strcat(dir, ent->d_name);
          if(cleanupDirectory(dir, killTime, flags) == 0) {
	    message(LOG_ERROR, "cleanup failed in %s: %s\n", dir,
		    strerror(errno));
          }
	  free(dir);
	}
        fchdir(dd);
        close(dd);
      } else {
        message(LOG_ERROR, "could not perform cleanup in %s/%s: %s\n",
		dirname, ent->d_name, strerror(errno));
      }

      if (*significant_time >= killTime)
        continue;

      if (flags & FLAGS_FUSER &&
	  !access("/sbin/fuser", R_OK|X_OK) &&
	  check_fuser(dirname, ent->d_name)) {
        message(LOG_VERBOSE, "file is already in use or open: %s\n",
	        ent->d_name);
        continue;
      }

      /* we should try to remove the directory if it contains no files. */
      message(LOG_VERBOSE, "removing directory %s/%s\n", dirname, ent->d_name);

      if (!(flags & FLAGS_TEST)) {
        if (rmdir(ent->d_name)) {
	  if (errno != ENOTEMPTY) {
	    message(LOG_ERROR, "failed to rmdir %s/%s: %s", 
		    dirname, ent->d_name, strerror(errno));
	  }
        }
      } else {
        rmdir(ent->d_name);
      }
    } else {
      if (*significant_time >= killTime)
        continue;

      if ((flags & FLAGS_ALLFILES) ||
	  S_ISREG(sb.st_mode) ||
	  S_ISLNK(sb.st_mode)) {
        if (flags & FLAGS_FUSER && !access("/sbin/fuser", R_OK|X_OK) &&
	  check_fuser(dirname, ent->d_name)) {
	  message(LOG_VERBOSE, "file is already in use or open: %s\n",
		  ent->d_name);
	  continue;
        }

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

  if (closedir(dir) == -1) {
    message(LOG_ERROR, "closedir of %s failed: %s\n",
	    dirname, strerror(errno));
    return 0;
  }

  /* restore access time on this directory to its original time */
  utb.actime = here.st_atime; /* atime */
  utb.modtime = here.st_mtime; /* mtime */
  utime(".", &utb);

  return 1;
}

void printCopyright(void) {
  fprintf(stderr, "tmpwatch " VERSION " - (c) 1997, 1999, 2000 Red Hat, Inc.\n");
  fprintf(stderr, "This may be freely redistributed under the terms of "
	  "the GNU Public License.\n");
}


void usage(void) {
  printCopyright();
  fprintf(stderr, "\n");
#ifndef __hpux
  fprintf(stderr, "tmpwatch [-u|-m|-c] [-afqtv] [--verbose] [--force] [--all] [--test] [--quiet] [--atime|--mtime|--ctime] <hours-untouched> <dirs>\n");
#else
  fprintf(stderr, "tmpwatch [-u|-m|-c] [-afqtv] <hours-untouched> <dirs>\n");
#endif
  exit(1);
}

int main(int argc, char ** argv) {
  unsigned int grace;
  unsigned int killTime, long_index;
  int flags = 0, arg;
  struct stat sb;
#ifndef __hpux
  struct option options[] = {
    { "all", 0, 0, 'a' },
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
  const char optstring[] = "acfmqstuv";

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
      flags |= FLAGS_ALLFILES;
      break;

    case 'f':
      flags |= FLAGS_FORCE;
      break;
    case 's':
	flags |= FLAGS_FUSER;
	break;
	
    case 't':
      flags |= FLAGS_TEST;
      /* fallthrough */
    case 'v':
      logLevel > 0 ? logLevel -= 1 : 0;
      break;
    case 'q':
      logLevel = LOG_FATAL;
      break;
    case 'u':
      flags |= FLAGS_ATIME;
      break;
    case 'm':
      flags |= FLAGS_MTIME;
      break;
    case 'c':
	flags |= FLAGS_CTIME;
	break;
    case '?':
      usage();
    }
  }
  
  /* atime and mtime options are mutually exclusive. - alh */
  if (!!(flags & FLAGS_ATIME) + !!(flags & FLAGS_MTIME) +
      !!(flags & FLAGS_CTIME) > 1) {
    message(LOG_FATAL, "--atime, --mtime, and --ctime options are mutually exclusive\n");
  }
  
  /* Default to atime if neither was specified. - alh */
  if (!(flags & (FLAGS_ATIME | FLAGS_MTIME | FLAGS_CTIME)))
    flags |= FLAGS_ATIME;

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
    if (lstat(argv[optind], &sb)) {
      message(LOG_ERROR, "lstat() of directory %s failed: %s\n",
	      argv[optind], strerror(errno));
      exit(1);
    }

    if (S_ISLNK(sb.st_mode)) {
      message(LOG_DEBUG, "initial directory %s is a symlink -- "
	      "skipping\n", argv[optind]);
    } else {
      if(cleanupDirectory(argv[optind], killTime, flags) == 0) {
	message(LOG_ERROR, "cleanup failed in %s: %s\n", argv[optind],
		strerror(errno));
      }
    }

    optind++;
  }

  return 0;
}
