/* bind-mount.c -- is_bind_mount()
 *
 * Copyright (C) 2005, 2007, 2008 Red Hat, Inc.  All rights reserved.
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
 * Red Hat Author(s): Miloslav Trmač <mitr@redhat.com>
 */
#include <config.h>

#if defined(HAVE_MNTENT_H) && defined(HAVE_PATHS_H)

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <mntent.h>
#include <paths.h>
#include "obstack.h"
#include "stat-time.h"
#include "timespec.h"
#include "xalloc.h"

#include "bind-mount.h"

 /* Utilities */

/* Used by obstack code */
struct _obstack_chunk *
obstack_chunk_alloc(long size)
{
    return xmalloc(size);
}
#define obstack_chunk_free free

/* A list of strings */
struct string_list
{
    char **entries;
    size_t len;			/* Number of valid entries */
    /* Number of allocated entries, usually invalid after the list is "finished"
       and ENTRIES are reallocated to the exact size. */
    size_t allocated;
};

/* Append STRING to LIST */
static void
string_list_append(struct string_list *list, char *string)
{
    if (list->allocated == list->len)
	list->entries = x2nrealloc(list->entries, &list->allocated,
				   sizeof (*list->entries));
    list->entries[list->len] = string;
    list->len++;
}

 /* Bind mount handling */

/* Known bind mount paths */
static struct string_list bind_mount_paths; /* = { 0, }; */

/* Next bind_mount_paths entry */
static size_t bind_mount_paths_index; /* = 0; */

/* _PATH_MOUNTED mtime at the time of last rebuild_bind_mount_paths() call */
static struct timespec last_path_mounted_mtime; /* = { 0, }; */

static struct obstack bind_mount_paths_obstack;
static void *bind_mount_paths_mark;

/* Compare two string pointers using strcmp() */
static int
cmp_string_pointers(const void *xa, const void *xb)
{
    char *const *a, *const *b;

    a = xa;
    b = xb;
    return strcmp(*a, *b);
}

/* Compare a string with a string pointer using strcmp() */
static int
cmp_string_pointer(const void *xa, const void *xb)
{
  const char *a;
  char *const *b;

  a = xa;
  b = xb;

  return strcmp(a, *b);
}

/* Rebuild bind_mount_paths */
static void
rebuild_bind_mount_paths(void)
{
    FILE *f;
    struct mntent *me;

    obstack_free(&bind_mount_paths_obstack, bind_mount_paths_mark);
    bind_mount_paths_mark = obstack_alloc(&bind_mount_paths_obstack, 0);
    bind_mount_paths.len = 0;
    f = setmntent(_PATH_MOUNTED, "r");
    if (f == NULL)
	goto err;
    while ((me = getmntent(f)) != NULL) {
	/* Bind mounts "to self" can be used (e.g. by policycoreutils-sandbox)
	   to create another mount point to which Linux name space privacy
	   flags can be attached.  Such a bind mount is not duplicating any
	   part of the directory tree, so it should not be excluded. */
	if (hasmntopt(me, "bind") != NULL
	    && strcmp(me->mnt_fsname, me->mnt_dir) != 0) {
	    char dbuf[PATH_MAX], *dir;

	    dir = realpath(me->mnt_dir, dbuf);
	    if (dir == NULL)
		dir = me->mnt_dir;
	    dir = obstack_copy(&bind_mount_paths_obstack, dir, strlen(dir) + 1);
	    string_list_append(&bind_mount_paths, dir);
	}
    }
    endmntent(f);
    /* Fall through */
err:
    qsort(bind_mount_paths.entries, bind_mount_paths.len,
	  sizeof (*bind_mount_paths.entries), cmp_string_pointers);
}

/* Return true if PATH is a destination of a bind mount.
   (Bind mounts "to self" are ignored.) */
bool
is_bind_mount(const char *path)
{
    struct timespec path_mounted_mtime;
    struct stat st;

    /* Unfortunately (mount --bind $path $path/subdir) would leave st_dev
       unchanged between $path and $path/subdir, so we must keep reparsing
       _PATH_MOUNTED (not PROC_MOUNTS_PATH) each time it changes. */
    if (stat(_PATH_MOUNTED, &st) != 0)
	return false;
    path_mounted_mtime = get_stat_mtime(&st);
    if (timespec_cmp(last_path_mounted_mtime, path_mounted_mtime) < 0) {
	rebuild_bind_mount_paths();
	last_path_mounted_mtime = path_mounted_mtime;
	bind_mount_paths_index = 0;
    }
    return bsearch(path, bind_mount_paths.entries, bind_mount_paths.len,
		   sizeof (*bind_mount_paths.entries), cmp_string_pointer)
	!= NULL;
}

/* Initialize $PRUNE_BIND_MOUNTS state */
void
init_bind_mount_paths(void)
{
  struct stat st;

  obstack_init(&bind_mount_paths_obstack);
  obstack_alignment_mask(&bind_mount_paths_obstack) = 0;
  bind_mount_paths_mark = obstack_alloc(&bind_mount_paths_obstack, 0);
  if (stat(_PATH_MOUNTED, &st) != 0)
    return;
  rebuild_bind_mount_paths();
  last_path_mounted_mtime = get_stat_mtime(&st);
}
#endif /* !(defined(HAVE_MNTENT_H) && defined(HAVE_PATHS_H)) */
