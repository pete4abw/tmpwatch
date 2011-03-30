/* bind-mount.h -- bind mount detection
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
#ifndef BIND_MOUNT_H__
#define BIND_MOUNT_H__

#include <config.h>

#include <stdbool.h>

/* Use the same condition as in bind-mount.c! */
#ifdef __linux

/* Return true if PATH is a destination of a bind mount.
   (Bind mounts "to self" are ignored.) */
extern bool is_bind_mount(const char *path);

/* Initialize state for is_bind_mount(). */
extern void bind_mount_init(void);

#else /* !(defined(HAVE_MNTENT_H) && defined(HAVE_PATHS_H)) */

static bool is_bind_mount(const char *path)
{
    (void)path;
    return false;
}

static void bind_mount_init(void)
{
}

#endif /* __linux */

#endif
