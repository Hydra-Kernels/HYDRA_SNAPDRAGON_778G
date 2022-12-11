// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * Copyright (c) 2016-2018 Christoph Hellwig.
 * All Rights Reserved.
 */
#include "xfs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_iomap.h"
#include "xfs_trace.h"
#include "xfs_bmap.h"
#include "xfs_bmap_util.h"
#include "xfs_reflink.h"

/*
 * structure owned by writepages passed to individual writepage calls
 */
struct xfs_writepage_ctx {
	struct xfs_bmbt_irec    imap;
	int			fork;
	unsigned int		data_seq;
	unsigned int		cow_seq;
	struct xfs_ioend	*ioend;
};

struct block_device *
xfs_find_bdev_for_inode(
	struct inode		*inode)
{
	struct xfs_inode	*ip = XFS_I(inode);
	struct xfs_mount	*mp = ip->i_mount;

	if (XFS_IS_REALTIME_INODE(ip))
		return mp->m_rtdev_targp->bt_bdev;
	else
		return mp->m_ddev_targp->bt_bdev;
}

struct dax_device *
xfs_find_daxdev_for_inode(
	struct inode		*inode)
{
	struct xfs_inode	*ip = XFS_I(inode);
	struct xfs_mount	*mp = ip->i_mount;

	if (XFS_IS_REALTIME_INODE(ip))
		return mp->m_rtdev_targp->bt_daxdev;
	else
		return mp->m_ddev_targp->bt_daxdev;
}

static void
xfs_finish_page_writeback(
	struct inode		*inode,
	struct bio_vec	*bvec,
	int			error)
{
	struct iomap_page	*iop = to_iomap_page(bvec->bv_page);

	if (error) {
		SetPageError(bvec->bv_page);
		mapping_set_error(inode->i_mapping, -EIO);
	}

	ASSERT(iop || i_blocksize(inode) == PAGE_SIZE);
	ASSERT(!iop || atomic_read(&iop->write_count) > 0);

	if (!iop || atomic_dec_and_test(&iop->write_count))
		end_page_writeback(bvec->bv_page);
}

/*
 * We're now finished for good with this ioend structure.  Update the page
 * state, release holds on bios, and finally free up memory.  Do not use the
 * ioend after this.
 */
STATIC void
xfs_destroy_ioend(
	struct xfs_ioend	*ioend,
	int			error)
{
	struct inode		*inode = ioend->io_inode;
	struct bio		*bio = &ioend->io_inline_bio;
	struct bio		*last = ioend->io_bio, *next;
	u64			start = bio->bi_iter.bi_sector;
	bool			quiet = bio_flagged(bio, BIO_QUIET);

	for (bio = &ioend->io_inline_bio; bio; bio = next) {
		struct bio_vec	*bvec;
		struct bvec_iter_all iter_all;

		/*
		 * For the last bio, bi_private points to the ioend, so we
		 * need to explicitly end the iteration here.
		 */
		if (bio == last)
			next = NULL;
		else
			next = bio->bi_private;

		/* walk each page on bio, ending page IO on them */
		bio_for_each_segment_all(bvec, bio, iter_all)
			xfs_finish_page_writeback(inode, bvec, error);
		bio_put(bio);
	}

	if (unlikely(error && !quiet)) {
		xfs_err_ratelimited(XFS_I(inode)->i_mount,
			"writeback error on sector %llu", start);
	}
}

/*
 * Fast and loose check if this write could update the on-disk inode size.
 */
static inline bool xfs_ioend_is_append(struct xfs_ioend *ioend)
{
	return ioend->io_offset + ioend->io_size >
		XFS_I(ioend->io_inode)->i_d.di_size;
}

STATIC int
xfs_setfilesize_trans_alloc(
	struct xfs_ioend	*ioend)
{
	struct xfs_mount	*mp = XFS_I(ioend->io_inode)->i_mount;
	struct xfs_trans	*tp;
	int			error;

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_fsyncts, 0, 0, 0, &tp);
	if (error)
		return error;

	ioend->io_append_trans = tp;

	/*
	 * We may pass freeze protection with a transaction.  So tell lockdep
	 * we released it.
	 */
	__sb_writers_release(ioend->io_inode->i_sb, SB_FREEZE_FS);
	/*
	 * We hand off the transaction to the completion thread now, so
	 * clear the flag here.
	 */
	current_restore_flags_nested(&tp->t_pflags, PF_MEMALLOC_NOFS);
	return 0;
}

/*
 * Update on-disk file size now that data has been written to disk.
 */
STATIC int
__xfs_setfilesize(
	struct xfs_inode	*ip,
	struct xfs_trans	*tp,
	xfs_off_t		offset,
	size_t			size)
{
	xfs_fsize_t		isize;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	isize = xfs_new_eof(ip, offset + size);
	if (!isize) {
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		xfs_trans_cancel(tp);
		return 0;
	}

	trace_xfs_setfilesize(ip, offset, size);

	ip->i_d.di_size = isize;
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	return xfs_trans_commit(tp);
}

int
xfs_setfilesize(
	struct xfs_inode	*ip,
	xfs_off_t		offset,
	size_t			size)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	int			error;

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_fsyncts, 0, 0, 0, &tp);
	if (error)
		return error;

	return __xfs_setfilesize(ip, tp, offset, size);
}

STATIC int
xfs_setfilesize_ioend(
	struct xfs_ioend	*ioend,
	int			error)
{
	struct xfs_inode	*ip = XFS_I(ioend->io_inode);
	struct xfs_trans	*tp = ioend->io_append_trans;

	/*
	 * The transaction may have been allocated in the I/O submission thread,
	 * thus we need to mark ourselves as being in a transaction manually.
	 * Similarly for freeze protection.
	 */
	current_set_flags_nested(&tp->t_pflags, PF_MEMALLOC_NOFS);
	__sb_writers_acquired(VFS_I(ip)->i_sb, SB_FREEZE_FS);

	/* we abort the update if there was an IO error */
	if (error) {
		xfs_trans_cancel(tp);
		return error;
	}

	return __xfs_setfilesize(ip, tp, ioend->io_offset, ioend->io_size);
}

/*
 * IO write completion.
 */
STATIC void
xfs_end_ioend(
	struct xfs_ioend	*ioend)
{
	struct list_head	ioend_list;
	struct xfs_inode	*ip = XFS_I(ioend->io_inode);
	xfs_off_t		offset = ioend->io_offset;
	size_t			size = ioend->io_size;
	unsigned int		nofs_flag;
	int			error;

	/*
	 * We can allocate memory here while doing writeback on behalf of
	 * memory reclaim.  To avoid memory allocation deadlocks set the
	 * task-wide nofs context for the following operations.
	 */
	nofs_flag = memalloc_nofs_save();

	/*
	 * Just clean up the in-memory strutures if the fs has been shut down.
	 */
	if (XFS_FORCED_SHUTDOWN(ip->i_mount)) {
		error = -EIO;
		goto done;
	}

	/*
	 * Clean up any COW blocks on an I/O error.
	 */
	error = blk_status_to_errno(ioend->io_bio->bi_status);
	if (unlikely(error)) {
		if (ioend->io_fork == XFS_COW_FORK)
			xfs_reflink_cancel_cow_range(ip, offset, size, true);
		goto done;
	}

	/*
	 * Success: commit the COW or unwritten blocks if needed.
	 */
	if (ioend->io_fork == XFS_COW_FORK)
		error = xfs_reflink_end_cow(ip, offset, size);
	else if (ioend->io_state == XFS_EXT_UNWRITTEN)
		error = xfs_iomap_write_unwritten(ip, offset, size, false);
	else
		ASSERT(!xfs_ioend_is_append(ioend) || ioend->io_append_trans);

done:
	if (ioend->io_append_trans)
		error = xfs_setfilesize_ioend(ioend, error);
	list_replace_init(&ioend->io_list, &ioend_list);
	xfs_destroy_ioend(ioend, error);

	while (!list_empty(&ioend_list)) {
		ioend = list_first_entry(&ioend_list, struct xfs_ioend,
				io_list);
		list_del_init(&ioend->io_list);
		xfs_destroy_ioend(ioend, error);
	}

	memalloc_nofs_restore(nofs_flag);
}

/*
 * We can merge two adjacent ioends if they have the same set of work to do.
 */
static bool
xfs_ioend_can_merge(
	struct xfs_ioend	*ioend,
	struct xfs_ioend	*next)
{
	if (ioend->io_bio->bi_status != next->io_bio->bi_status)
		return false;
	if ((ioend->io_fork == XFS_COW_FORK) ^ (next->io_fork == XFS_COW_FORK))
		return false;
	if ((ioend->io_state == XFS_EXT_UNWRITTEN) ^
	    (next->io_state == XFS_EXT_UNWRITTEN))
		return false;
	if (ioend->io_offset + ioend->io_size != next->io_offset)
		return false;
	return true;
}

/*
 * If the to be merged ioend has a preallocated transaction for file
 * size updates we need to ensure the ioend it is merged into also
 * has one.  If it already has one we can simply cancel the transaction
 * as it is guaranteed to be clean.
 */
static void
xfs_ioend_merge_append_transactions(
	struct xfs_ioend	*ioend,
	struct xfs_ioend	*next)
{
<<<<<<< HEAD
	if (!ioend->io_append_trans) {
		ioend->io_append_trans = next->io_append_trans;
		next->io_append_trans = NULL;
	} else {
		xfs_setfilesize_ioend(next, -ECANCELED);
=======
	struct xfs_inode	*ip = XFS_I(inode);
	struct xfs_mount	*mp = ip->i_mount;
	ssize_t			count = i_blocksize(inode);
	xfs_fileoff_t		offset_fsb, end_fsb;
	int			error = 0;
	int			bmapi_flags = XFS_BMAPI_ENTIRE;
	int			nimaps = 1;

	if (XFS_FORCED_SHUTDOWN(mp))
		return -EIO;

	if (type == XFS_IO_UNWRITTEN)
		bmapi_flags |= XFS_BMAPI_IGSTATE;

	if (!xfs_ilock_nowait(ip, XFS_ILOCK_SHARED)) {
		if (nonblocking)
			return -EAGAIN;
		xfs_ilock(ip, XFS_ILOCK_SHARED);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	}
}

/* Try to merge adjacent completions. */
STATIC void
xfs_ioend_try_merge(
	struct xfs_ioend	*ioend,
	struct list_head	*more_ioends)
{
	struct xfs_ioend	*next_ioend;

<<<<<<< HEAD
	while (!list_empty(more_ioends)) {
		next_ioend = list_first_entry(more_ioends, struct xfs_ioend,
				io_list);
		if (!xfs_ioend_can_merge(ioend, next_ioend))
			break;
		list_move_tail(&next_ioend->io_list, &ioend->io_list);
		ioend->io_size += next_ioend->io_size;
		if (next_ioend->io_append_trans)
			xfs_ioend_merge_append_transactions(ioend, next_ioend);
=======
	if ((xfs_ufsize_t)offset + count > mp->m_super->s_maxbytes)
		count = mp->m_super->s_maxbytes - offset;
	end_fsb = XFS_B_TO_FSB(mp, (xfs_ufsize_t)offset + count);
	offset_fsb = XFS_B_TO_FSBT(mp, offset);
	error = xfs_bmapi_read(ip, offset_fsb, end_fsb - offset_fsb,
				imap, &nimaps, bmapi_flags);
	xfs_iunlock(ip, XFS_ILOCK_SHARED);

	if (error)
		return error;

	if (type == XFS_IO_DELALLOC &&
	    (!nimaps || isnullstartblock(imap->br_startblock))) {
		error = xfs_iomap_write_allocate(ip, offset, imap);
		if (!error)
			trace_xfs_map_blocks_alloc(ip, offset, count, type, imap);
		return error;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	}
}

/* list_sort compare function for ioends */
static int
xfs_ioend_compare(
	void			*priv,
	struct list_head	*a,
	struct list_head	*b)
{
	struct xfs_ioend	*ia;
	struct xfs_ioend	*ib;

	ia = container_of(a, struct xfs_ioend, io_list);
	ib = container_of(b, struct xfs_ioend, io_list);
	if (ia->io_offset < ib->io_offset)
		return -1;
	else if (ia->io_offset > ib->io_offset)
		return 1;
	return 0;
}

/* Finish all pending io completions. */
void
xfs_end_io(
	struct work_struct	*work)
{
	struct xfs_inode	*ip;
	struct xfs_ioend	*ioend;
	struct list_head	completion_list;
	unsigned long		flags;

	ip = container_of(work, struct xfs_inode, i_ioend_work);

	spin_lock_irqsave(&ip->i_ioend_lock, flags);
	list_replace_init(&ip->i_ioend_list, &completion_list);
	spin_unlock_irqrestore(&ip->i_ioend_lock, flags);

	list_sort(NULL, &completion_list, xfs_ioend_compare);

	while (!list_empty(&completion_list)) {
		ioend = list_first_entry(&completion_list, struct xfs_ioend,
				io_list);
		list_del_init(&ioend->io_list);
		xfs_ioend_try_merge(ioend, &completion_list);
		xfs_end_ioend(ioend);
	}
}

STATIC void
xfs_end_bio(
	struct bio		*bio)
{
	struct xfs_ioend	*ioend = bio->bi_private;
	struct xfs_inode	*ip = XFS_I(ioend->io_inode);
	struct xfs_mount	*mp = ip->i_mount;
	unsigned long		flags;

	if (ioend->io_fork == XFS_COW_FORK ||
	    ioend->io_state == XFS_EXT_UNWRITTEN ||
	    ioend->io_append_trans != NULL) {
		spin_lock_irqsave(&ip->i_ioend_lock, flags);
		if (list_empty(&ip->i_ioend_list))
			WARN_ON_ONCE(!queue_work(mp->m_unwritten_workqueue,
						 &ip->i_ioend_work));
		list_add_tail(&ioend->io_list, &ip->i_ioend_list);
		spin_unlock_irqrestore(&ip->i_ioend_lock, flags);
	} else
		xfs_destroy_ioend(ioend, blk_status_to_errno(bio->bi_status));
}

/*
 * Fast revalidation of the cached writeback mapping. Return true if the current
 * mapping is valid, false otherwise.
 */
static bool
xfs_imap_valid(
	struct xfs_writepage_ctx	*wpc,
	struct xfs_inode		*ip,
	xfs_fileoff_t			offset_fsb)
{
	if (offset_fsb < wpc->imap.br_startoff ||
	    offset_fsb >= wpc->imap.br_startoff + wpc->imap.br_blockcount)
		return false;
	/*
	 * If this is a COW mapping, it is sufficient to check that the mapping
	 * covers the offset. Be careful to check this first because the caller
	 * can revalidate a COW mapping without updating the data seqno.
	 */
	if (wpc->fork == XFS_COW_FORK)
		return true;

	/*
	 * This is not a COW mapping. Check the sequence number of the data fork
	 * because concurrent changes could have invalidated the extent. Check
	 * the COW fork because concurrent changes since the last time we
	 * checked (and found nothing at this offset) could have added
	 * overlapping blocks.
	 */
	if (wpc->data_seq != READ_ONCE(ip->i_df.if_seq))
		return false;
	if (xfs_inode_has_cow_data(ip) &&
	    wpc->cow_seq != READ_ONCE(ip->i_cowfp->if_seq))
		return false;
	return true;
}

/*
 * Pass in a dellalloc extent and convert it to real extents, return the real
 * extent that maps offset_fsb in wpc->imap.
 *
 * The current page is held locked so nothing could have removed the block
 * backing offset_fsb, although it could have moved from the COW to the data
 * fork by another thread.
 */
static int
xfs_convert_blocks(
	struct xfs_writepage_ctx *wpc,
	struct xfs_inode	*ip,
	xfs_fileoff_t		offset_fsb)
{
	int			error;

	/*
	 * Attempt to allocate whatever delalloc extent currently backs
	 * offset_fsb and put the result into wpc->imap.  Allocate in a loop
	 * because it may take several attempts to allocate real blocks for a
	 * contiguous delalloc extent if free space is sufficiently fragmented.
	 */
	do {
		error = xfs_bmapi_convert_delalloc(ip, wpc->fork, offset_fsb,
				&wpc->imap, wpc->fork == XFS_COW_FORK ?
					&wpc->cow_seq : &wpc->data_seq);
		if (error)
			return error;
	} while (wpc->imap.br_startoff + wpc->imap.br_blockcount <= offset_fsb);

	return 0;
}

STATIC int
xfs_map_blocks(
	struct xfs_writepage_ctx *wpc,
	struct inode		*inode,
	loff_t			offset)
{
	struct xfs_inode	*ip = XFS_I(inode);
	struct xfs_mount	*mp = ip->i_mount;
	ssize_t			count = i_blocksize(inode);
	xfs_fileoff_t		offset_fsb = XFS_B_TO_FSBT(mp, offset);
	xfs_fileoff_t		end_fsb = XFS_B_TO_FSB(mp, offset + count);
	xfs_fileoff_t		cow_fsb = NULLFILEOFF;
	struct xfs_bmbt_irec	imap;
	struct xfs_iext_cursor	icur;
	int			retries = 0;
	int			error = 0;

	if (XFS_FORCED_SHUTDOWN(mp))
		return -EIO;

	/*
	 * COW fork blocks can overlap data fork blocks even if the blocks
	 * aren't shared.  COW I/O always takes precedent, so we must always
	 * check for overlap on reflink inodes unless the mapping is already a
	 * COW one, or the COW fork hasn't changed from the last time we looked
	 * at it.
	 *
	 * It's safe to check the COW fork if_seq here without the ILOCK because
	 * we've indirectly protected against concurrent updates: writeback has
	 * the page locked, which prevents concurrent invalidations by reflink
	 * and directio and prevents concurrent buffered writes to the same
	 * page.  Changes to if_seq always happen under i_lock, which protects
	 * against concurrent updates and provides a memory barrier on the way
	 * out that ensures that we always see the current value.
	 */
	if (xfs_imap_valid(wpc, ip, offset_fsb))
		return 0;

	/*
	 * If we don't have a valid map, now it's time to get a new one for this
	 * offset.  This will convert delayed allocations (including COW ones)
	 * into real extents.  If we return without a valid map, it means we
	 * landed in a hole and we skip the block.
	 */
retry:
	xfs_ilock(ip, XFS_ILOCK_SHARED);
	ASSERT(ip->i_d.di_format != XFS_DINODE_FMT_BTREE ||
	       (ip->i_df.if_flags & XFS_IFEXTENTS));

	/*
	 * Check if this is offset is covered by a COW extents, and if yes use
	 * it directly instead of looking up anything in the data fork.
	 */
	if (xfs_inode_has_cow_data(ip) &&
	    xfs_iext_lookup_extent(ip, ip->i_cowfp, offset_fsb, &icur, &imap))
		cow_fsb = imap.br_startoff;
	if (cow_fsb != NULLFILEOFF && cow_fsb <= offset_fsb) {
		wpc->cow_seq = READ_ONCE(ip->i_cowfp->if_seq);
		xfs_iunlock(ip, XFS_ILOCK_SHARED);

		wpc->fork = XFS_COW_FORK;
		goto allocate_blocks;
	}

	/*
	 * No COW extent overlap. Revalidate now that we may have updated
	 * ->cow_seq. If the data mapping is still valid, we're done.
	 */
	if (xfs_imap_valid(wpc, ip, offset_fsb)) {
		xfs_iunlock(ip, XFS_ILOCK_SHARED);
		return 0;
	}

	/*
	 * If we don't have a valid map, now it's time to get a new one for this
	 * offset.  This will convert delayed allocations (including COW ones)
	 * into real extents.
	 */
	if (!xfs_iext_lookup_extent(ip, &ip->i_df, offset_fsb, &icur, &imap))
		imap.br_startoff = end_fsb;	/* fake a hole past EOF */
	wpc->data_seq = READ_ONCE(ip->i_df.if_seq);
	xfs_iunlock(ip, XFS_ILOCK_SHARED);

	wpc->fork = XFS_DATA_FORK;

	/* landed in a hole or beyond EOF? */
	if (imap.br_startoff > offset_fsb) {
		imap.br_blockcount = imap.br_startoff - offset_fsb;
		imap.br_startoff = offset_fsb;
		imap.br_startblock = HOLESTARTBLOCK;
		imap.br_state = XFS_EXT_NORM;
	}

	/*
	 * Truncate to the next COW extent if there is one.  This is the only
	 * opportunity to do this because we can skip COW fork lookups for the
	 * subsequent blocks in the mapping; however, the requirement to treat
	 * the COW range separately remains.
	 */
	if (cow_fsb != NULLFILEOFF &&
	    cow_fsb < imap.br_startoff + imap.br_blockcount)
		imap.br_blockcount = cow_fsb - imap.br_startoff;

	/* got a delalloc extent? */
	if (imap.br_startblock != HOLESTARTBLOCK &&
	    isnullstartblock(imap.br_startblock))
		goto allocate_blocks;

	wpc->imap = imap;
	trace_xfs_map_blocks_found(ip, offset, count, wpc->fork, &imap);
	return 0;
allocate_blocks:
	error = xfs_convert_blocks(wpc, ip, offset_fsb);
	if (error) {
		/*
		 * If we failed to find the extent in the COW fork we might have
		 * raced with a COW to data fork conversion or truncate.
		 * Restart the lookup to catch the extent in the data fork for
		 * the former case, but prevent additional retries to avoid
		 * looping forever for the latter case.
		 */
		if (error == -EAGAIN && wpc->fork == XFS_COW_FORK && !retries++)
			goto retry;
		ASSERT(error != -EAGAIN);
		return error;
	}

	/*
	 * Due to merging the return real extent might be larger than the
	 * original delalloc one.  Trim the return extent to the next COW
	 * boundary again to force a re-lookup.
	 */
	if (wpc->fork != XFS_COW_FORK && cow_fsb != NULLFILEOFF &&
	    cow_fsb < wpc->imap.br_startoff + wpc->imap.br_blockcount)
		wpc->imap.br_blockcount = cow_fsb - wpc->imap.br_startoff;

	ASSERT(wpc->imap.br_startoff <= offset_fsb);
	ASSERT(wpc->imap.br_startoff + wpc->imap.br_blockcount > offset_fsb);
	trace_xfs_map_blocks_alloc(ip, offset, count, wpc->fork, &imap);
	return 0;
}

/*
 * Submit the bio for an ioend. We are passed an ioend with a bio attached to
 * it, and we submit that bio. The ioend may be used for multiple bio
 * submissions, so we only want to allocate an append transaction for the ioend
 * once. In the case of multiple bio submission, each bio will take an IO
 * reference to the ioend to ensure that the ioend completion is only done once
 * all bios have been submitted and the ioend is really done.
 *
 * If @status is non-zero, it means that we have a situation where some part of
 * the submission process has failed after we have marked paged for writeback
 * and unlocked them. In this situation, we need to fail the bio and ioend
 * rather than submit it to IO. This typically only happens on a filesystem
 * shutdown.
 */
STATIC int
xfs_submit_ioend(
	struct writeback_control *wbc,
	struct xfs_ioend	*ioend,
	int			status)
{
	unsigned int		nofs_flag;

	/*
	 * We can allocate memory here while doing writeback on behalf of
	 * memory reclaim.  To avoid memory allocation deadlocks set the
	 * task-wide nofs context for the following operations.
	 */
	nofs_flag = memalloc_nofs_save();

	/* Convert CoW extents to regular */
	if (!status && ioend->io_fork == XFS_COW_FORK) {
		status = xfs_reflink_convert_cow(XFS_I(ioend->io_inode),
				ioend->io_offset, ioend->io_size);
	}

	/* Reserve log space if we might write beyond the on-disk inode size. */
	if (!status &&
	    (ioend->io_fork == XFS_COW_FORK ||
	     ioend->io_state != XFS_EXT_UNWRITTEN) &&
	    xfs_ioend_is_append(ioend) &&
	    !ioend->io_append_trans)
		status = xfs_setfilesize_trans_alloc(ioend);

	memalloc_nofs_restore(nofs_flag);

	ioend->io_bio->bi_private = ioend;
	ioend->io_bio->bi_end_io = xfs_end_bio;

	/*
	 * If we are failing the IO now, just mark the ioend with an
	 * error and finish it. This will run IO completion immediately
	 * as there is only one reference to the ioend at this point in
	 * time.
	 */
	if (status) {
		ioend->io_bio->bi_status = errno_to_blk_status(status);
		bio_endio(ioend->io_bio);
		return status;
	}

	submit_bio(ioend->io_bio);
	return 0;
}

static struct xfs_ioend *
xfs_alloc_ioend(
	struct inode		*inode,
	int			fork,
	xfs_exntst_t		state,
	xfs_off_t		offset,
	struct block_device	*bdev,
	sector_t		sector,
	struct writeback_control *wbc)
{
	struct xfs_ioend	*ioend;
	struct bio		*bio;

	bio = bio_alloc_bioset(GFP_NOFS, BIO_MAX_PAGES, &xfs_ioend_bioset);
	bio_set_dev(bio, bdev);
	bio->bi_iter.bi_sector = sector;
	bio->bi_opf = REQ_OP_WRITE | wbc_to_write_flags(wbc);
	bio->bi_write_hint = inode->i_write_hint;
	wbc_init_bio(wbc, bio);

	ioend = container_of(bio, struct xfs_ioend, io_inline_bio);
	INIT_LIST_HEAD(&ioend->io_list);
	ioend->io_fork = fork;
	ioend->io_state = state;
	ioend->io_inode = inode;
	ioend->io_size = 0;
	ioend->io_offset = offset;
	ioend->io_append_trans = NULL;
	ioend->io_bio = bio;
	return ioend;
}

/*
 * Allocate a new bio, and chain the old bio to the new one.
 *
 * Note that we have to do perform the chaining in this unintuitive order
 * so that the bi_private linkage is set up in the right direction for the
 * traversal in xfs_destroy_ioend().
 */
static struct bio *
xfs_chain_bio(
	struct bio		*prev)
{
	struct bio *new;

	new = bio_alloc(GFP_NOFS, BIO_MAX_PAGES);
	bio_copy_dev(new, prev);/* also copies over blkcg information */
	new->bi_iter.bi_sector = bio_end_sector(prev);
	new->bi_opf = prev->bi_opf;
	new->bi_write_hint = prev->bi_write_hint;

	bio_chain(prev, new);
	bio_get(prev);		/* for xfs_destroy_ioend */
	submit_bio(prev);
	return new;
}

/*
 * Test to see if we have an existing ioend structure that we could append to
 * first, otherwise finish off the current ioend and start another.
 */
STATIC void
xfs_add_to_ioend(
	struct inode		*inode,
	xfs_off_t		offset,
	struct page		*page,
	struct iomap_page	*iop,
	struct xfs_writepage_ctx *wpc,
	struct writeback_control *wbc,
	struct list_head	*iolist)
{
	struct xfs_inode	*ip = XFS_I(inode);
	struct xfs_mount	*mp = ip->i_mount;
	struct block_device	*bdev = xfs_find_bdev_for_inode(inode);
	unsigned		len = i_blocksize(inode);
	unsigned		poff = offset & (PAGE_SIZE - 1);
	bool			merged, same_page = false;
	sector_t		sector;

	sector = xfs_fsb_to_db(ip, wpc->imap.br_startblock) +
		((offset - XFS_FSB_TO_B(mp, wpc->imap.br_startoff)) >> 9);

	if (!wpc->ioend ||
	    wpc->fork != wpc->ioend->io_fork ||
	    wpc->imap.br_state != wpc->ioend->io_state ||
	    sector != bio_end_sector(wpc->ioend->io_bio) ||
	    offset != wpc->ioend->io_offset + wpc->ioend->io_size) {
		if (wpc->ioend)
			list_add(&wpc->ioend->io_list, iolist);
		wpc->ioend = xfs_alloc_ioend(inode, wpc->fork,
				wpc->imap.br_state, offset, bdev, sector, wbc);
	}

	merged = __bio_try_merge_page(wpc->ioend->io_bio, page, len, poff,
			&same_page);

	if (iop && !same_page)
		atomic_inc(&iop->write_count);

	if (!merged) {
		if (bio_full(wpc->ioend->io_bio, len))
			wpc->ioend->io_bio = xfs_chain_bio(wpc->ioend->io_bio);
		bio_add_page(wpc->ioend->io_bio, page, len, poff);
	}

	wpc->ioend->io_size += len;
	wbc_account_cgroup_owner(wbc, page, len);
}

STATIC void
xfs_vm_invalidatepage(
	struct page		*page,
	unsigned int		offset,
	unsigned int		length)
{
	trace_xfs_invalidatepage(page->mapping->host, page, offset, length);
	iomap_invalidatepage(page, offset, length);
}

/*
 * If the page has delalloc blocks on it, we need to punch them out before we
 * invalidate the page.  If we don't, we leave a stale delalloc mapping on the
 * inode that can trip up a later direct I/O read operation on the same region.
 *
 * We prevent this by truncating away the delalloc regions on the page.  Because
 * they are delalloc, we can do this without needing a transaction. Indeed - if
 * we get ENOSPC errors, we have to be able to do this truncation without a
 * transaction as there is no space left for block reservation (typically why we
 * see a ENOSPC in writeback).
 */
STATIC void
xfs_aops_discard_page(
	struct page		*page)
{
	struct inode		*inode = page->mapping->host;
	struct xfs_inode	*ip = XFS_I(inode);
	struct xfs_mount	*mp = ip->i_mount;
	loff_t			offset = page_offset(page);
	xfs_fileoff_t		start_fsb = XFS_B_TO_FSBT(mp, offset);
	int			error;

	if (XFS_FORCED_SHUTDOWN(mp))
		goto out_invalidate;

	xfs_alert(mp,
		"page discard on page "PTR_FMT", inode 0x%llx, offset %llu.",
			page, ip->i_ino, offset);

<<<<<<< HEAD
	error = xfs_bmap_punch_delalloc_range(ip, start_fsb,
			PAGE_SIZE / i_blocksize(inode));
	if (error && !XFS_FORCED_SHUTDOWN(mp))
		xfs_alert(mp, "page discard unable to remove delalloc mapping.");
=======
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	bh = head = page_buffers(page);
	do {
		int		error;
		xfs_fileoff_t	start_fsb;

		if (!buffer_delay(bh))
			goto next_buffer;

		start_fsb = XFS_B_TO_FSBT(ip->i_mount, offset);
		error = xfs_bmap_punch_delalloc_range(ip, start_fsb, 1);
		if (error) {
			/* something screwed, just bail */
			if (!XFS_FORCED_SHUTDOWN(ip->i_mount)) {
				xfs_alert(ip->i_mount,
			"page discard unable to remove delalloc mapping.");
			}
			break;
		}
next_buffer:
		offset += i_blocksize(inode);

	} while ((bh = bh->b_this_page) != head);

	xfs_iunlock(ip, XFS_ILOCK_EXCL);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
out_invalidate:
	xfs_vm_invalidatepage(page, 0, PAGE_SIZE);
}

/*
 * We implement an immediate ioend submission policy here to avoid needing to
 * chain multiple ioends and hence nest mempool allocations which can violate
 * forward progress guarantees we need to provide. The current ioend we are
 * adding blocks to is cached on the writepage context, and if the new block
 * does not append to the cached ioend it will create a new ioend and cache that
 * instead.
 *
 * If a new ioend is created and cached, the old ioend is returned and queued
 * locally for submission once the entire page is processed or an error has been
 * detected.  While ioends are submitted immediately after they are completed,
 * batching optimisations are provided by higher level block plugging.
 *
 * At the end of a writeback pass, there will be a cached ioend remaining on the
 * writepage context that the caller will need to submit.
 */
static int
xfs_writepage_map(
	struct xfs_writepage_ctx *wpc,
	struct writeback_control *wbc,
	struct inode		*inode,
	struct page		*page,
	uint64_t		end_offset)
{
	LIST_HEAD(submit_list);
	struct iomap_page	*iop = to_iomap_page(page);
	unsigned		len = i_blocksize(inode);
	struct xfs_ioend	*ioend, *next;
	uint64_t		file_offset;	/* file offset of page */
	int			error = 0, count = 0, i;

	ASSERT(iop || i_blocksize(inode) == PAGE_SIZE);
	ASSERT(!iop || atomic_read(&iop->write_count) == 0);

	/*
	 * Walk through the page to find areas to write back. If we run off the
	 * end of the current map or find the current map invalid, grab a new
	 * one.
	 */
	for (i = 0, file_offset = page_offset(page);
	     i < (PAGE_SIZE >> inode->i_blkbits) && file_offset < end_offset;
	     i++, file_offset += len) {
		if (iop && !test_bit(i, iop->uptodate))
			continue;

		error = xfs_map_blocks(wpc, inode, file_offset);
		if (error)
			break;
		if (wpc->imap.br_startblock == HOLESTARTBLOCK)
			continue;
		xfs_add_to_ioend(inode, file_offset, page, iop, wpc, wbc,
				 &submit_list);
		count++;
	}

	ASSERT(wpc->ioend || list_empty(&submit_list));
	ASSERT(PageLocked(page));
	ASSERT(!PageWriteback(page));

	/*
	 * On error, we have to fail the ioend here because we may have set
	 * pages under writeback, we have to make sure we run IO completion to
	 * mark the error state of the IO appropriately, so we can't cancel the
	 * ioend directly here.  That means we have to mark this page as under
	 * writeback if we included any blocks from it in the ioend chain so
	 * that completion treats it correctly.
	 *
	 * If we didn't include the page in the ioend, the on error we can
	 * simply discard and unlock it as there are no other users of the page
	 * now.  The caller will still need to trigger submission of outstanding
	 * ioends on the writepage context so they are treated correctly on
	 * error.
	 */
	if (unlikely(error)) {
		if (!count) {
			xfs_aops_discard_page(page);
			ClearPageUptodate(page);
			unlock_page(page);
			goto done;
		}

		/*
		 * If the page was not fully cleaned, we need to ensure that the
		 * higher layers come back to it correctly.  That means we need
		 * to keep the page dirty, and for WB_SYNC_ALL writeback we need
		 * to ensure the PAGECACHE_TAG_TOWRITE index mark is not removed
		 * so another attempt to write this page in this writeback sweep
		 * will be made.
		 */
		set_page_writeback_keepwrite(page);
	} else {
		clear_page_dirty_for_io(page);
		set_page_writeback(page);
	}

	unlock_page(page);

	/*
	 * Preserve the original error if there was one, otherwise catch
	 * submission errors here and propagate into subsequent ioend
	 * submissions.
	 */
	list_for_each_entry_safe(ioend, next, &submit_list, io_list) {
		int error2;

		list_del_init(&ioend->io_list);
		error2 = xfs_submit_ioend(wbc, ioend, error);
		if (error2 && !error)
			error = error2;
	}

	/*
	 * We can end up here with no error and nothing to write only if we race
	 * with a partial page truncate on a sub-page block sized filesystem.
	 */
	if (!count)
		end_page_writeback(page);
done:
	mapping_set_error(page->mapping, error);
	return error;
}

/*
 * Write out a dirty page.
 *
 * For delalloc space on the page we need to allocate space and flush it.
 * For unwritten space on the page we need to start the conversion to
 * regular allocated space.
 */
STATIC int
xfs_do_writepage(
	struct page		*page,
	struct writeback_control *wbc,
	void			*data)
{
	struct xfs_writepage_ctx *wpc = data;
	struct inode		*inode = page->mapping->host;
	loff_t			offset;
	uint64_t              end_offset;
	pgoff_t                 end_index;

	trace_xfs_writepage(inode, page, 0, 0);

	/*
	 * Refuse to write the page out if we are called from reclaim context.
	 *
	 * This avoids stack overflows when called from deeply used stacks in
	 * random callers for direct reclaim or memcg reclaim.  We explicitly
	 * allow reclaim from kswapd as the stack usage there is relatively low.
	 *
	 * This should never happen except in the case of a VM regression so
	 * warn about it.
	 */
	if (WARN_ON_ONCE((current->flags & (PF_MEMALLOC|PF_KSWAPD)) ==
			PF_MEMALLOC))
		goto redirty;

	/*
	 * Given that we do not allow direct reclaim to call us, we should
	 * never be called while in a filesystem transaction.
	 */
	if (WARN_ON_ONCE(current->flags & PF_MEMALLOC_NOFS))
		goto redirty;

	/*
	 * Is this page beyond the end of the file?
	 *
	 * The page index is less than the end_index, adjust the end_offset
	 * to the highest offset that this page should represent.
	 * -----------------------------------------------------
	 * |			file mapping	       | <EOF> |
	 * -----------------------------------------------------
	 * | Page ... | Page N-2 | Page N-1 |  Page N  |       |
	 * ^--------------------------------^----------|--------
	 * |     desired writeback range    |      see else    |
	 * ---------------------------------^------------------|
	 */
	offset = i_size_read(inode);
	end_index = offset >> PAGE_SHIFT;
	if (page->index < end_index)
		end_offset = (xfs_off_t)(page->index + 1) << PAGE_SHIFT;
	else {
		/*
		 * Check whether the page to write out is beyond or straddles
		 * i_size or not.
		 * -------------------------------------------------------
		 * |		file mapping		        | <EOF>  |
		 * -------------------------------------------------------
		 * | Page ... | Page N-2 | Page N-1 |  Page N   | Beyond |
		 * ^--------------------------------^-----------|---------
		 * |				    |      Straddles     |
		 * ---------------------------------^-----------|--------|
		 */
		unsigned offset_into_page = offset & (PAGE_SIZE - 1);

		/*
		 * Skip the page if it is fully outside i_size, e.g. due to a
		 * truncate operation that is in progress. We must redirty the
		 * page so that reclaim stops reclaiming it. Otherwise
		 * xfs_vm_releasepage() is called on it and gets confused.
		 *
		 * Note that the end_index is unsigned long, it would overflow
		 * if the given offset is greater than 16TB on 32-bit system
		 * and if we do check the page is fully outside i_size or not
		 * via "if (page->index >= end_index + 1)" as "end_index + 1"
		 * will be evaluated to 0.  Hence this page will be redirtied
		 * and be written out repeatedly which would result in an
		 * infinite loop, the user program that perform this operation
		 * will hang.  Instead, we can verify this situation by checking
		 * if the page to write is totally beyond the i_size or if it's
		 * offset is just equal to the EOF.
		 */
		if (page->index > end_index ||
		    (page->index == end_index && offset_into_page == 0))
			goto redirty;

		/*
		 * The page straddles i_size.  It must be zeroed out on each
		 * and every writepage invocation because it may be mmapped.
		 * "A file is mapped in multiples of the page size.  For a file
		 * that is not a multiple of the page size, the remaining
		 * memory is zeroed when mapped, and writes to that region are
		 * not written out to the file."
		 */
		zero_user_segment(page, offset_into_page, PAGE_SIZE);

		/* Adjust the end_offset to the end of file */
		end_offset = offset;
	}

	return xfs_writepage_map(wpc, wbc, inode, page, end_offset);

redirty:
	redirty_page_for_writepage(wbc, page);
	unlock_page(page);
	return 0;
}

STATIC int
xfs_vm_writepage(
	struct page		*page,
	struct writeback_control *wbc)
{
	struct xfs_writepage_ctx wpc = { };
	int			ret;

	ret = xfs_do_writepage(page, wbc, &wpc);
	if (wpc.ioend)
		ret = xfs_submit_ioend(wbc, wpc.ioend, ret);
	return ret;
}

STATIC int
xfs_vm_writepages(
	struct address_space	*mapping,
	struct writeback_control *wbc)
{
	struct xfs_writepage_ctx wpc = { };
	int			ret;

	xfs_iflags_clear(XFS_I(mapping->host), XFS_ITRUNCATED);
	ret = write_cache_pages(mapping, wbc, xfs_do_writepage, &wpc);
	if (wpc.ioend)
		ret = xfs_submit_ioend(wbc, wpc.ioend, ret);
	return ret;
}

STATIC int
xfs_dax_writepages(
	struct address_space	*mapping,
	struct writeback_control *wbc)
{
	xfs_iflags_clear(XFS_I(mapping->host), XFS_ITRUNCATED);
	return dax_writeback_mapping_range(mapping,
			xfs_find_bdev_for_inode(mapping->host), wbc);
}

STATIC int
xfs_vm_releasepage(
	struct page		*page,
	gfp_t			gfp_mask)
{
	trace_xfs_releasepage(page->mapping->host, page, 0, 0);
<<<<<<< HEAD
	return iomap_releasepage(page, gfp_mask);
=======

	xfs_count_page_state(page, &delalloc, &unwritten);

	if (WARN_ON_ONCE(delalloc))
		return 0;
	if (WARN_ON_ONCE(unwritten))
		return 0;

	return try_to_free_buffers(page);
}

/*
 * When we map a DIO buffer, we may need to attach an ioend that describes the
 * type of write IO we are doing. This passes to the completion function the
 * operations it needs to perform. If the mapping is for an overwrite wholly
 * within the EOF then we don't need an ioend and so we don't allocate one.
 * This avoids the unnecessary overhead of allocating and freeing ioends for
 * workloads that don't require transactions on IO completion.
 *
 * If we get multiple mappings in a single IO, we might be mapping different
 * types. But because the direct IO can only have a single private pointer, we
 * need to ensure that:
 *
 * a) i) the ioend spans the entire region of unwritten mappings; or
 *    ii) the ioend spans all the mappings that cross or are beyond EOF; and
 * b) if it contains unwritten extents, it is *permanently* marked as such
 *
 * We could do this by chaining ioends like buffered IO does, but we only
 * actually get one IO completion callback from the direct IO, and that spans
 * the entire IO regardless of how many mappings and IOs are needed to complete
 * the DIO. There is only going to be one reference to the ioend and its life
 * cycle is constrained by the DIO completion code. hence we don't need
 * reference counting here.
 *
 * Note that for DIO, an IO to the highest supported file block offset (i.e.
 * 2^63 - 1FSB bytes) will result in the offset + count overflowing a signed 64
 * bit variable. Hence if we see this overflow, we have to assume that the IO is
 * extending the file size. We won't know for sure until IO completion is run
 * and the actual max write offset is communicated to the IO completion
 * routine.
 *
 * For DAX page faults, we are preparing to never see unwritten extents here,
 * nor should we ever extend the inode size. Hence we will soon have nothing to
 * do here for this case, ensuring we don't have to provide an IO completion
 * callback to free an ioend that we don't actually need for a fault into the
 * page at offset (2^63 - 1FSB) bytes.
 */

static void
xfs_map_direct(
	struct inode		*inode,
	struct buffer_head	*bh_result,
	struct xfs_bmbt_irec	*imap,
	xfs_off_t		offset,
	bool			dax_fault)
{
	struct xfs_ioend	*ioend;
	xfs_off_t		size = bh_result->b_size;
	int			type;

	if (ISUNWRITTEN(imap))
		type = XFS_IO_UNWRITTEN;
	else
		type = XFS_IO_OVERWRITE;

	trace_xfs_gbmap_direct(XFS_I(inode), offset, size, type, imap);

	if (dax_fault) {
		ASSERT(type == XFS_IO_OVERWRITE);
		trace_xfs_gbmap_direct_none(XFS_I(inode), offset, size, type,
					    imap);
		return;
	}

	if (bh_result->b_private) {
		ioend = bh_result->b_private;
		ASSERT(ioend->io_size > 0);
		ASSERT(offset >= ioend->io_offset);
		if (offset + size > ioend->io_offset + ioend->io_size)
			ioend->io_size = offset - ioend->io_offset + size;

		if (type == XFS_IO_UNWRITTEN && type != ioend->io_type)
			ioend->io_type = XFS_IO_UNWRITTEN;

		trace_xfs_gbmap_direct_update(XFS_I(inode), ioend->io_offset,
					      ioend->io_size, ioend->io_type,
					      imap);
	} else if (type == XFS_IO_UNWRITTEN ||
		   offset + size > i_size_read(inode) ||
		   offset + size < 0) {
		ioend = xfs_alloc_ioend(inode, type);
		ioend->io_offset = offset;
		ioend->io_size = size;

		bh_result->b_private = ioend;
		set_buffer_defer_completion(bh_result);

		trace_xfs_gbmap_direct_new(XFS_I(inode), offset, size, type,
					   imap);
	} else {
		trace_xfs_gbmap_direct_none(XFS_I(inode), offset, size, type,
					    imap);
	}
}

/*
 * If this is O_DIRECT or the mpage code calling tell them how large the mapping
 * is, so that we can avoid repeated get_blocks calls.
 *
 * If the mapping spans EOF, then we have to break the mapping up as the mapping
 * for blocks beyond EOF must be marked new so that sub block regions can be
 * correctly zeroed. We can't do this for mappings within EOF unless the mapping
 * was just allocated or is unwritten, otherwise the callers would overwrite
 * existing data with zeros. Hence we have to split the mapping into a range up
 * to and including EOF, and a second mapping for beyond EOF.
 */
static void
xfs_map_trim_size(
	struct inode		*inode,
	sector_t		iblock,
	struct buffer_head	*bh_result,
	struct xfs_bmbt_irec	*imap,
	xfs_off_t		offset,
	ssize_t			size)
{
	xfs_off_t		mapping_size;

	mapping_size = imap->br_startoff + imap->br_blockcount - iblock;
	mapping_size <<= inode->i_blkbits;

	ASSERT(mapping_size > 0);
	if (mapping_size > size)
		mapping_size = size;
	if (offset < i_size_read(inode) &&
	    (xfs_ufsize_t)offset + mapping_size >= i_size_read(inode)) {
		/* limit mapping to block that spans EOF */
		mapping_size = roundup_64(i_size_read(inode) - offset,
					  i_blocksize(inode));
	}
	if (mapping_size > LONG_MAX)
		mapping_size = LONG_MAX;

	bh_result->b_size = mapping_size;
}

STATIC int
__xfs_get_blocks(
	struct inode		*inode,
	sector_t		iblock,
	struct buffer_head	*bh_result,
	int			create,
	bool			direct,
	bool			dax_fault)
{
	struct xfs_inode	*ip = XFS_I(inode);
	struct xfs_mount	*mp = ip->i_mount;
	xfs_fileoff_t		offset_fsb, end_fsb;
	int			error = 0;
	int			lockmode = 0;
	struct xfs_bmbt_irec	imap;
	int			nimaps = 1;
	xfs_off_t		offset;
	ssize_t			size;
	int			new = 0;

	if (XFS_FORCED_SHUTDOWN(mp))
		return -EIO;

	offset = (xfs_off_t)iblock << inode->i_blkbits;
	ASSERT(bh_result->b_size >= i_blocksize(inode));
	size = bh_result->b_size;

	if (!create && direct && offset >= i_size_read(inode))
		return 0;

	/*
	 * Direct I/O is usually done on preallocated files, so try getting
	 * a block mapping without an exclusive lock first.  For buffered
	 * writes we already have the exclusive iolock anyway, so avoiding
	 * a lock roundtrip here by taking the ilock exclusive from the
	 * beginning is a useful micro optimization.
	 */
	if (create && !direct) {
		lockmode = XFS_ILOCK_EXCL;
		xfs_ilock(ip, lockmode);
	} else {
		lockmode = xfs_ilock_data_map_shared(ip);
	}

	ASSERT(offset <= mp->m_super->s_maxbytes);
	if ((xfs_ufsize_t)offset + size > mp->m_super->s_maxbytes)
		size = mp->m_super->s_maxbytes - offset;
	end_fsb = XFS_B_TO_FSB(mp, (xfs_ufsize_t)offset + size);
	offset_fsb = XFS_B_TO_FSBT(mp, offset);

	error = xfs_bmapi_read(ip, offset_fsb, end_fsb - offset_fsb,
				&imap, &nimaps, XFS_BMAPI_ENTIRE);
	if (error)
		goto out_unlock;

	/*
	 * The only time we can ever safely find delalloc blocks on direct I/O
	 * is a dio write to post-eof speculative preallocation. All other
	 * scenarios are indicative of a problem or misuse (such as mixing
	 * direct and mapped I/O).
	 *
	 * The file may be unmapped by the time we get here so we cannot
	 * reliably fail the I/O based on mapping. Instead, fail the I/O if this
	 * is a read or a write within eof. Otherwise, carry on but warn as a
	 * precuation if the file happens to be mapped.
	 */
	if (direct && imap.br_startblock == DELAYSTARTBLOCK) {
	        if (!create || offset < i_size_read(VFS_I(ip))) {
	                WARN_ON_ONCE(1);
	                error = -EIO;
	                goto out_unlock;
	        }
	        WARN_ON_ONCE(mapping_mapped(VFS_I(ip)->i_mapping));
	}

	/* for DAX, we convert unwritten extents directly */
	if (create &&
	    (!nimaps ||
	     (imap.br_startblock == HOLESTARTBLOCK ||
	      imap.br_startblock == DELAYSTARTBLOCK) ||
	     (IS_DAX(inode) && ISUNWRITTEN(&imap)))) {
		if (direct || xfs_get_extsz_hint(ip)) {
			/*
			 * xfs_iomap_write_direct() expects the shared lock. It
			 * is unlocked on return.
			 */
			if (lockmode == XFS_ILOCK_EXCL)
				xfs_ilock_demote(ip, lockmode);

			error = xfs_iomap_write_direct(ip, offset, size,
						       &imap, nimaps);
			if (error)
				return error;
			new = 1;

		} else {
			/*
			 * Delalloc reservations do not require a transaction,
			 * we can go on without dropping the lock here. If we
			 * are allocating a new delalloc block, make sure that
			 * we set the new flag so that we mark the buffer new so
			 * that we know that it is newly allocated if the write
			 * fails.
			 */
			if (nimaps && imap.br_startblock == HOLESTARTBLOCK)
				new = 1;
			error = xfs_iomap_write_delay(ip, offset, size, &imap);
			if (error)
				goto out_unlock;

			xfs_iunlock(ip, lockmode);
		}
		trace_xfs_get_blocks_alloc(ip, offset, size,
				ISUNWRITTEN(&imap) ? XFS_IO_UNWRITTEN
						   : XFS_IO_DELALLOC, &imap);
	} else if (nimaps) {
		trace_xfs_get_blocks_found(ip, offset, size,
				ISUNWRITTEN(&imap) ? XFS_IO_UNWRITTEN
						   : XFS_IO_OVERWRITE, &imap);
		xfs_iunlock(ip, lockmode);
	} else {
		trace_xfs_get_blocks_notfound(ip, offset, size);
		goto out_unlock;
	}

	if (IS_DAX(inode) && create) {
		ASSERT(!ISUNWRITTEN(&imap));
		/* zeroing is not needed at a higher layer */
		new = 0;
	}

	/* trim mapping down to size requested */
	if (direct || size > (1 << inode->i_blkbits))
		xfs_map_trim_size(inode, iblock, bh_result,
				  &imap, offset, size);

	/*
	 * For unwritten extents do not report a disk address in the buffered
	 * read case (treat as if we're reading into a hole).
	 */
	if (imap.br_startblock != HOLESTARTBLOCK &&
	    imap.br_startblock != DELAYSTARTBLOCK &&
	    (create || !ISUNWRITTEN(&imap))) {
		xfs_map_buffer(inode, bh_result, &imap, offset);
		if (ISUNWRITTEN(&imap))
			set_buffer_unwritten(bh_result);
		/* direct IO needs special help */
		if (create && direct)
			xfs_map_direct(inode, bh_result, &imap, offset,
				       dax_fault);
	}

	/*
	 * If this is a realtime file, data may be on a different device.
	 * to that pointed to from the buffer_head b_bdev currently.
	 */
	bh_result->b_bdev = xfs_find_bdev_for_inode(inode);

	/*
	 * If we previously allocated a block out beyond eof and we are now
	 * coming back to use it then we will need to flag it as new even if it
	 * has a disk address.
	 *
	 * With sub-block writes into unwritten extents we also need to mark
	 * the buffer as new so that the unwritten parts of the buffer gets
	 * correctly zeroed.
	 */
	if (create &&
	    ((!buffer_mapped(bh_result) && !buffer_uptodate(bh_result)) ||
	     (offset >= i_size_read(inode)) ||
	     (new || ISUNWRITTEN(&imap))))
		set_buffer_new(bh_result);

	if (imap.br_startblock == DELAYSTARTBLOCK) {
		if (create) {
			set_buffer_uptodate(bh_result);
			set_buffer_mapped(bh_result);
			set_buffer_delay(bh_result);
		}
	}

	return 0;

out_unlock:
	xfs_iunlock(ip, lockmode);
	return error;
}

int
xfs_get_blocks(
	struct inode		*inode,
	sector_t		iblock,
	struct buffer_head	*bh_result,
	int			create)
{
	return __xfs_get_blocks(inode, iblock, bh_result, create, false, false);
}

int
xfs_get_blocks_direct(
	struct inode		*inode,
	sector_t		iblock,
	struct buffer_head	*bh_result,
	int			create)
{
	return __xfs_get_blocks(inode, iblock, bh_result, create, true, false);
}

int
xfs_get_blocks_dax_fault(
	struct inode		*inode,
	sector_t		iblock,
	struct buffer_head	*bh_result,
	int			create)
{
	return __xfs_get_blocks(inode, iblock, bh_result, create, true, true);
}

static void
__xfs_end_io_direct_write(
	struct inode		*inode,
	struct xfs_ioend	*ioend,
	loff_t			offset,
	ssize_t			size)
{
	struct xfs_mount	*mp = XFS_I(inode)->i_mount;

	if (XFS_FORCED_SHUTDOWN(mp) || ioend->io_error)
		goto out_end_io;

	/*
	 * dio completion end_io functions are only called on writes if more
	 * than 0 bytes was written.
	 */
	ASSERT(size > 0);

	/*
	 * The ioend only maps whole blocks, while the IO may be sector aligned.
	 * Hence the ioend offset/size may not match the IO offset/size exactly.
	 * Because we don't map overwrites within EOF into the ioend, the offset
	 * may not match, but only if the endio spans EOF.  Either way, write
	 * the IO sizes into the ioend so that completion processing does the
	 * right thing.
	 */
	ASSERT(offset + size <= ioend->io_offset + ioend->io_size);
	ioend->io_size = size;
	ioend->io_offset = offset;

	/*
	 * The ioend tells us whether we are doing unwritten extent conversion
	 * or an append transaction that updates the on-disk file size. These
	 * cases are the only cases where we should *potentially* be needing
	 * to update the VFS inode size.
	 *
	 * We need to update the in-core inode size here so that we don't end up
	 * with the on-disk inode size being outside the in-core inode size. We
	 * have no other method of updating EOF for AIO, so always do it here
	 * if necessary.
	 *
	 * We need to lock the test/set EOF update as we can be racing with
	 * other IO completions here to update the EOF. Failing to serialise
	 * here can result in EOF moving backwards and Bad Things Happen when
	 * that occurs.
	 */
	spin_lock(&XFS_I(inode)->i_flags_lock);
	if (offset + size > i_size_read(inode))
		i_size_write(inode, offset + size);
	spin_unlock(&XFS_I(inode)->i_flags_lock);

	/*
	 * If we are doing an append IO that needs to update the EOF on disk,
	 * do the transaction reserve now so we can use common end io
	 * processing. Stashing the error (if there is one) in the ioend will
	 * result in the ioend processing passing on the error if it is
	 * possible as we can't return it from here.
	 */
	if (ioend->io_type == XFS_IO_OVERWRITE)
		ioend->io_error = xfs_setfilesize_trans_alloc(ioend);

out_end_io:
	xfs_end_io(&ioend->io_work);
	return;
}

/*
 * Complete a direct I/O write request.
 *
 * The ioend structure is passed from __xfs_get_blocks() to tell us what to do.
 * If no ioend exists (i.e. @private == NULL) then the write IO is an overwrite
 * wholly within the EOF and so there is nothing for us to do. Note that in this
 * case the completion can be called in interrupt context, whereas if we have an
 * ioend we will always be called in task context (i.e. from a workqueue).
 */
STATIC void
xfs_end_io_direct_write(
	struct kiocb		*iocb,
	loff_t			offset,
	ssize_t			size,
	void			*private)
{
	struct inode		*inode = file_inode(iocb->ki_filp);
	struct xfs_ioend	*ioend = private;

	trace_xfs_gbmap_direct_endio(XFS_I(inode), offset, size,
				     ioend ? ioend->io_type : 0, NULL);

	if (!ioend) {
		ASSERT(offset + size <= i_size_read(inode));
		return;
	}

	__xfs_end_io_direct_write(inode, ioend, offset, size);
}

static inline ssize_t
xfs_vm_do_dio(
	struct inode		*inode,
	struct kiocb		*iocb,
	struct iov_iter		*iter,
	loff_t			offset,
	void			(*endio)(struct kiocb	*iocb,
					 loff_t		offset,
					 ssize_t	size,
					 void		*private),
	int			flags)
{
	struct block_device	*bdev;

	if (IS_DAX(inode))
		return dax_do_io(iocb, inode, iter, offset,
				 xfs_get_blocks_direct, endio, 0);

	bdev = xfs_find_bdev_for_inode(inode);
	return  __blockdev_direct_IO(iocb, inode, bdev, iter, offset,
				     xfs_get_blocks_direct, endio, NULL, flags);
}

STATIC ssize_t
xfs_vm_direct_IO(
	struct kiocb		*iocb,
	struct iov_iter		*iter,
	loff_t			offset)
{
	struct inode		*inode = iocb->ki_filp->f_mapping->host;

	if (iov_iter_rw(iter) == WRITE)
		return xfs_vm_do_dio(inode, iocb, iter, offset,
				     xfs_end_io_direct_write, DIO_ASYNC_EXTEND);
	return xfs_vm_do_dio(inode, iocb, iter, offset, NULL, 0);
}

/*
 * Punch out the delalloc blocks we have already allocated.
 *
 * Don't bother with xfs_setattr given that nothing can have made it to disk yet
 * as the page is still locked at this point.
 */
STATIC void
xfs_vm_kill_delalloc_range(
	struct inode		*inode,
	loff_t			start,
	loff_t			end)
{
	struct xfs_inode	*ip = XFS_I(inode);
	xfs_fileoff_t		start_fsb;
	xfs_fileoff_t		end_fsb;
	int			error;

	start_fsb = XFS_B_TO_FSB(ip->i_mount, start);
	end_fsb = XFS_B_TO_FSB(ip->i_mount, end);
	if (end_fsb <= start_fsb)
		return;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	error = xfs_bmap_punch_delalloc_range(ip, start_fsb,
						end_fsb - start_fsb);
	if (error) {
		/* something screwed, just bail */
		if (!XFS_FORCED_SHUTDOWN(ip->i_mount)) {
			xfs_alert(ip->i_mount,
		"xfs_vm_write_failed: unable to clean up ino %lld",
					ip->i_ino);
		}
	}
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
}

STATIC void
xfs_vm_write_failed(
	struct inode		*inode,
	struct page		*page,
	loff_t			pos,
	unsigned		len)
{
	loff_t			block_offset;
	loff_t			block_start;
	loff_t			block_end;
	loff_t			from = pos & (PAGE_CACHE_SIZE - 1);
	loff_t			to = from + len;
	struct buffer_head	*bh, *head;

	/*
	 * The request pos offset might be 32 or 64 bit, this is all fine
	 * on 64-bit platform.  However, for 64-bit pos request on 32-bit
	 * platform, the high 32-bit will be masked off if we evaluate the
	 * block_offset via (pos & PAGE_MASK) because the PAGE_MASK is
	 * 0xfffff000 as an unsigned long, hence the result is incorrect
	 * which could cause the following ASSERT failed in most cases.
	 * In order to avoid this, we can evaluate the block_offset of the
	 * start of the page by using shifts rather than masks the mismatch
	 * problem.
	 */
	block_offset = (pos >> PAGE_CACHE_SHIFT) << PAGE_CACHE_SHIFT;

	ASSERT(block_offset + from == pos);

	head = page_buffers(page);
	block_start = 0;
	for (bh = head; bh != head || !block_start;
	     bh = bh->b_this_page, block_start = block_end,
				   block_offset += bh->b_size) {
		block_end = block_start + bh->b_size;

		/* skip buffers before the write */
		if (block_end <= from)
			continue;

		/* if the buffer is after the write, we're done */
		if (block_start >= to)
			break;

		if (!buffer_delay(bh))
			continue;

		if (!buffer_new(bh) && block_offset < i_size_read(inode))
			continue;

		xfs_vm_kill_delalloc_range(inode, block_offset,
					   block_offset + bh->b_size);

		/*
		 * This buffer does not contain data anymore. make sure anyone
		 * who finds it knows that for certain.
		 */
		clear_buffer_delay(bh);
		clear_buffer_uptodate(bh);
		clear_buffer_mapped(bh);
		clear_buffer_new(bh);
		clear_buffer_dirty(bh);
	}

}

/*
 * This used to call block_write_begin(), but it unlocks and releases the page
 * on error, and we need that page to be able to punch stale delalloc blocks out
 * on failure. hence we copy-n-waste it here and call xfs_vm_write_failed() at
 * the appropriate point.
 */
STATIC int
xfs_vm_write_begin(
	struct file		*file,
	struct address_space	*mapping,
	loff_t			pos,
	unsigned		len,
	unsigned		flags,
	struct page		**pagep,
	void			**fsdata)
{
	pgoff_t			index = pos >> PAGE_CACHE_SHIFT;
	struct page		*page;
	int			status;

	ASSERT(len <= PAGE_CACHE_SIZE);

	page = grab_cache_page_write_begin(mapping, index, flags);
	if (!page)
		return -ENOMEM;

	status = __block_write_begin(page, pos, len, xfs_get_blocks);
	if (unlikely(status)) {
		struct inode	*inode = mapping->host;
		size_t		isize = i_size_read(inode);

		xfs_vm_write_failed(inode, page, pos, len);
		unlock_page(page);

		/*
		 * If the write is beyond EOF, we only want to kill blocks
		 * allocated in this write, not blocks that were previously
		 * written successfully.
		 */
		if (pos + len > isize) {
			ssize_t start = max_t(ssize_t, pos, isize);

			truncate_pagecache_range(inode, start, pos + len);
		}

		page_cache_release(page);
		page = NULL;
	}

	*pagep = page;
	return status;
}

/*
 * On failure, we only need to kill delalloc blocks beyond EOF in the range of
 * this specific write because they will never be written. Previous writes
 * beyond EOF where block allocation succeeded do not need to be trashed, so
 * only new blocks from this write should be trashed. For blocks within
 * EOF, generic_write_end() zeros them so they are safe to leave alone and be
 * written with all the other valid data.
 */
STATIC int
xfs_vm_write_end(
	struct file		*file,
	struct address_space	*mapping,
	loff_t			pos,
	unsigned		len,
	unsigned		copied,
	struct page		*page,
	void			*fsdata)
{
	int			ret;

	ASSERT(len <= PAGE_CACHE_SIZE);

	ret = generic_write_end(file, mapping, pos, len, copied, page, fsdata);
	if (unlikely(ret < len)) {
		struct inode	*inode = mapping->host;
		size_t		isize = i_size_read(inode);
		loff_t		to = pos + len;

		if (to > isize) {
			/* only kill blocks in this write beyond EOF */
			if (pos > isize)
				isize = pos;
			xfs_vm_kill_delalloc_range(inode, isize, to);
			truncate_pagecache_range(inode, isize, to);
		}
	}
	return ret;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}

STATIC sector_t
xfs_vm_bmap(
	struct address_space	*mapping,
	sector_t		block)
{
	struct xfs_inode	*ip = XFS_I(mapping->host);

	trace_xfs_vm_bmap(ip);

	/*
	 * The swap code (ab-)uses ->bmap to get a block mapping and then
	 * bypasses the file system for actual I/O.  We really can't allow
	 * that on reflinks inodes, so we have to skip out here.  And yes,
	 * 0 is the magic code for a bmap error.
	 *
	 * Since we don't pass back blockdev info, we can't return bmap
	 * information for rt files either.
	 */
	if (xfs_is_cow_inode(ip) || XFS_IS_REALTIME_INODE(ip))
		return 0;
	return iomap_bmap(mapping, block, &xfs_iomap_ops);
}

STATIC int
xfs_vm_readpage(
	struct file		*unused,
	struct page		*page)
{
	trace_xfs_vm_readpage(page->mapping->host, 1);
	return iomap_readpage(page, &xfs_iomap_ops);
}

STATIC int
xfs_vm_readpages(
	struct file		*unused,
	struct address_space	*mapping,
	struct list_head	*pages,
	unsigned		nr_pages)
{
	trace_xfs_vm_readpages(mapping->host, nr_pages);
	return iomap_readpages(mapping, pages, nr_pages, &xfs_iomap_ops);
}

static int
xfs_iomap_swapfile_activate(
	struct swap_info_struct		*sis,
	struct file			*swap_file,
	sector_t			*span)
{
<<<<<<< HEAD
	sis->bdev = xfs_find_bdev_for_inode(file_inode(swap_file));
	return iomap_swapfile_activate(sis, swap_file, span, &xfs_iomap_ops);
=======
	struct address_space	*mapping = page->mapping;
	struct inode		*inode = mapping->host;
	loff_t			end_offset;
	loff_t			offset;
	int			newly_dirty;
	struct mem_cgroup	*memcg;

	if (unlikely(!mapping))
		return !TestSetPageDirty(page);

	end_offset = i_size_read(inode);
	offset = page_offset(page);

	spin_lock(&mapping->private_lock);
	if (page_has_buffers(page)) {
		struct buffer_head *head = page_buffers(page);
		struct buffer_head *bh = head;

		do {
			if (offset < end_offset)
				set_buffer_dirty(bh);
			bh = bh->b_this_page;
			offset += i_blocksize(inode);
		} while (bh != head);
	}
	/*
	 * Use mem_group_begin_page_stat() to keep PageDirty synchronized with
	 * per-memcg dirty page counters.
	 */
	memcg = mem_cgroup_begin_page_stat(page);
	newly_dirty = !TestSetPageDirty(page);
	spin_unlock(&mapping->private_lock);

	if (newly_dirty) {
		/* sigh - __set_page_dirty() is static, so copy it here, too */
		unsigned long flags;

		spin_lock_irqsave(&mapping->tree_lock, flags);
		if (page->mapping) {	/* Race with truncate? */
			WARN_ON_ONCE(!PageUptodate(page));
			account_page_dirtied(page, mapping, memcg);
			radix_tree_tag_set(&mapping->page_tree,
					page_index(page), PAGECACHE_TAG_DIRTY);
		}
		spin_unlock_irqrestore(&mapping->tree_lock, flags);
	}
	mem_cgroup_end_page_stat(memcg);
	if (newly_dirty)
		__mark_inode_dirty(mapping->host, I_DIRTY_PAGES);
	return newly_dirty;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}

const struct address_space_operations xfs_address_space_operations = {
	.readpage		= xfs_vm_readpage,
	.readpages		= xfs_vm_readpages,
	.writepage		= xfs_vm_writepage,
	.writepages		= xfs_vm_writepages,
	.set_page_dirty		= iomap_set_page_dirty,
	.releasepage		= xfs_vm_releasepage,
	.invalidatepage		= xfs_vm_invalidatepage,
	.bmap			= xfs_vm_bmap,
	.direct_IO		= noop_direct_IO,
	.migratepage		= iomap_migrate_page,
	.is_partially_uptodate  = iomap_is_partially_uptodate,
	.error_remove_page	= generic_error_remove_page,
	.swap_activate		= xfs_iomap_swapfile_activate,
};

const struct address_space_operations xfs_dax_aops = {
	.writepages		= xfs_dax_writepages,
	.direct_IO		= noop_direct_IO,
	.set_page_dirty		= noop_set_page_dirty,
	.invalidatepage		= noop_invalidatepage,
	.swap_activate		= xfs_iomap_swapfile_activate,
};
