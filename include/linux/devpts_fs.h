/* SPDX-License-Identifier: GPL-2.0-or-later */
/* -*- linux-c -*- --------------------------------------------------------- *
 *
 * linux/include/linux/devpts_fs.h
 *
 *  Copyright 1998-2004 H. Peter Anvin -- All Rights Reserved
 *
 * ------------------------------------------------------------------------- */

#ifndef _LINUX_DEVPTS_FS_H
#define _LINUX_DEVPTS_FS_H

#include <linux/errno.h>

struct pts_fs_info;

#ifdef CONFIG_UNIX98_PTYS

<<<<<<< HEAD
struct pts_fs_info;

struct vfsmount *devpts_mntget(struct file *, struct pts_fs_info *);
struct pts_fs_info *devpts_acquire(struct file *);
void devpts_release(struct pts_fs_info *);
=======
/* Look up a pts fs info and get a ref to it */
struct pts_fs_info *devpts_get_ref(struct inode *, struct file *);
void devpts_put_ref(struct pts_fs_info *);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

int devpts_new_index(struct pts_fs_info *);
void devpts_kill_index(struct pts_fs_info *, int);

/* mknod in devpts */
<<<<<<< HEAD
struct dentry *devpts_pty_new(struct pts_fs_info *, int, void *);
=======
struct inode *devpts_pty_new(struct pts_fs_info *, dev_t, int, void *);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
/* get private structure */
void *devpts_get_priv(struct dentry *);
/* unlink */
void devpts_pty_kill(struct dentry *);

/* in pty.c */
int ptm_open_peer(struct file *master, struct tty_struct *tty, int flags);

<<<<<<< HEAD
#else
static inline int
ptm_open_peer(struct file *master, struct tty_struct *tty, int flags)
{
	return -EIO;
}
=======
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#endif


#endif /* _LINUX_DEVPTS_FS_H */
