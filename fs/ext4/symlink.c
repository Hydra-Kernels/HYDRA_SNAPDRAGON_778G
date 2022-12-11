// SPDX-License-Identifier: GPL-2.0
/*
 *  linux/fs/ext4/symlink.c
 *
 * Only fast symlinks left here - the rest is done by generic code. AV, 1999
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/symlink.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  ext4 symlink handling code
 */

#include <linux/fs.h>
#include <linux/namei.h>
#include "ext4.h"
#include "xattr.h"

static const char *ext4_encrypted_get_link(struct dentry *dentry,
					   struct inode *inode,
					   struct delayed_call *done)
{
	struct page *cpage = NULL;
	const void *caddr;
	unsigned int max_size;
	const char *paddr;

	if (!dentry)
		return ERR_PTR(-ECHILD);

	if (ext4_inode_is_fast_symlink(inode)) {
		caddr = EXT4_I(inode)->i_data;
		max_size = sizeof(EXT4_I(inode)->i_data);
	} else {
		cpage = read_mapping_page(inode->i_mapping, 0, NULL);
		if (IS_ERR(cpage))
			return ERR_CAST(cpage);
		caddr = page_address(cpage);
<<<<<<< HEAD
		max_size = inode->i_sb->s_blocksize;
	}

	paddr = fscrypt_get_symlink(inode, caddr, max_size, done);
	if (cpage)
		put_page(cpage);
	return paddr;
}

static int ext4_encrypted_symlink_getattr(const struct path *path,
					  struct kstat *stat, u32 request_mask,
					  unsigned int query_flags)
{
	ext4_getattr(path, stat, request_mask, query_flags);

	return fscrypt_symlink_getattr(path, stat);
=======
		caddr[size] = 0;
	}

	/* Symlink is encrypted */
	sd = (struct ext4_encrypted_symlink_data *)caddr;
	cstr.name = sd->encrypted_path;
	cstr.len  = le16_to_cpu(sd->len);
	if ((cstr.len +
	     sizeof(struct ext4_encrypted_symlink_data) - 1) >
	    max_size) {
		/* Symlink data on the disk is corrupted */
		res = -EFSCORRUPTED;
		goto errout;
	}
	plen = (cstr.len < EXT4_FNAME_CRYPTO_DIGEST_SIZE*2) ?
		EXT4_FNAME_CRYPTO_DIGEST_SIZE*2 : cstr.len;
	paddr = kmalloc(plen + 1, GFP_NOFS);
	if (!paddr) {
		res = -ENOMEM;
		goto errout;
	}
	pstr.name = paddr;
	pstr.len = plen;
	res = _ext4_fname_disk_to_usr(inode, NULL, &cstr, &pstr);
	if (res < 0)
		goto errout;
	/* Null-terminate the name */
	if (res <= plen)
		paddr[res] = '\0';
	if (cpage)
		page_cache_release(cpage);
	return *cookie = paddr;
errout:
	if (cpage)
		page_cache_release(cpage);
	kfree(paddr);
	return ERR_PTR(res);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}

const struct inode_operations ext4_encrypted_symlink_inode_operations = {
	.get_link	= ext4_encrypted_get_link,
	.setattr	= ext4_setattr,
	.getattr	= ext4_encrypted_symlink_getattr,
	.listxattr	= ext4_listxattr,
};

const struct inode_operations ext4_symlink_inode_operations = {
	.get_link	= page_get_link,
	.setattr	= ext4_setattr,
	.getattr	= ext4_getattr,
	.listxattr	= ext4_listxattr,
};

const struct inode_operations ext4_fast_symlink_inode_operations = {
	.get_link	= simple_get_link,
	.setattr	= ext4_setattr,
	.getattr	= ext4_getattr,
	.listxattr	= ext4_listxattr,
};
