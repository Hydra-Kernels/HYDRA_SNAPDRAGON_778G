/*
 * Copyright (c) 2012 - 2019 Intel Corporation.  All rights reserved.
 * Copyright (c) 2006 - 2012 QLogic Corporation.  * All rights reserved.
 * Copyright (c) 2005, 2006 PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/err.h>
#include <linux/vmalloc.h>
#include <rdma/rdma_vt.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/seq_file.h>
#endif

#include "qib.h"

<<<<<<< HEAD
static inline unsigned mk_qpn(struct rvt_qpn_table *qpt,
			      struct rvt_qpn_map *map, unsigned off)
=======
#define RVT_BITS_PER_PAGE           (PAGE_SIZE*BITS_PER_BYTE)
#define RVT_BITS_PER_PAGE_MASK      (RVT_BITS_PER_PAGE-1)

static inline unsigned mk_qpn(struct qib_qpn_table *qpt,
			      struct qpn_map *map, unsigned off)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
{
	return (map - qpt->map) * RVT_BITS_PER_PAGE + off;
}

static inline unsigned find_next_offset(struct rvt_qpn_table *qpt,
					struct rvt_qpn_map *map, unsigned off,
					unsigned n, u16 qpt_mask)
{
	if (qpt_mask) {
		off++;
<<<<<<< HEAD
		if (((off & qpt_mask) >> 1) >= n)
			off = (off | qpt_mask) + 2;
	} else {
		off = find_next_zero_bit(map->page, RVT_BITS_PER_PAGE, off);
	}
=======
		if (((off & qpt->mask) >> 1) >= n)
			off = (off | qpt->mask) + 2;
	} else
		off = find_next_zero_bit(map->page, RVT_BITS_PER_PAGE, off);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	return off;
}

const struct rvt_operation_params qib_post_parms[RVT_OPERATION_MAX] = {
[IB_WR_RDMA_WRITE] = {
	.length = sizeof(struct ib_rdma_wr),
	.qpt_support = BIT(IB_QPT_UC) | BIT(IB_QPT_RC),
},

[IB_WR_RDMA_READ] = {
	.length = sizeof(struct ib_rdma_wr),
	.qpt_support = BIT(IB_QPT_RC),
	.flags = RVT_OPERATION_ATOMIC,
},

[IB_WR_ATOMIC_CMP_AND_SWP] = {
	.length = sizeof(struct ib_atomic_wr),
	.qpt_support = BIT(IB_QPT_RC),
	.flags = RVT_OPERATION_ATOMIC | RVT_OPERATION_ATOMIC_SGE,
},

[IB_WR_ATOMIC_FETCH_AND_ADD] = {
	.length = sizeof(struct ib_atomic_wr),
	.qpt_support = BIT(IB_QPT_RC),
	.flags = RVT_OPERATION_ATOMIC | RVT_OPERATION_ATOMIC_SGE,
},

[IB_WR_RDMA_WRITE_WITH_IMM] = {
	.length = sizeof(struct ib_rdma_wr),
	.qpt_support = BIT(IB_QPT_UC) | BIT(IB_QPT_RC),
},

[IB_WR_SEND] = {
	.length = sizeof(struct ib_send_wr),
	.qpt_support = BIT(IB_QPT_UD) | BIT(IB_QPT_SMI) | BIT(IB_QPT_GSI) |
		       BIT(IB_QPT_UC) | BIT(IB_QPT_RC),
},

[IB_WR_SEND_WITH_IMM] = {
	.length = sizeof(struct ib_send_wr),
	.qpt_support = BIT(IB_QPT_UD) | BIT(IB_QPT_SMI) | BIT(IB_QPT_GSI) |
		       BIT(IB_QPT_UC) | BIT(IB_QPT_RC),
},

};

<<<<<<< HEAD
static void get_map_page(struct rvt_qpn_table *qpt, struct rvt_qpn_map *map)
=======
static void get_map_page(struct qib_qpn_table *qpt, struct qpn_map *map,
			 gfp_t gfp)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
{
	unsigned long page = get_zeroed_page(gfp);

	/*
	 * Free the page if someone raced with us installing it.
	 */

	spin_lock(&qpt->lock);
	if (map->page)
		free_page(page);
	else
		map->page = (void *)page;
	spin_unlock(&qpt->lock);
}

/*
 * Allocate the next available QPN or
 * zero/one for QP type IB_QPT_SMI/IB_QPT_GSI.
 */
<<<<<<< HEAD
int qib_alloc_qpn(struct rvt_dev_info *rdi, struct rvt_qpn_table *qpt,
		  enum ib_qp_type type, u8 port)
=======
static int alloc_qpn(struct qib_devdata *dd, struct qib_qpn_table *qpt,
		     enum ib_qp_type type, u8 port, gfp_t gfp)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
{
	u32 i, offset, max_scan, qpn;
	struct rvt_qpn_map *map;
	u32 ret;
	struct qib_ibdev *verbs_dev = container_of(rdi, struct qib_ibdev, rdi);
	struct qib_devdata *dd = container_of(verbs_dev, struct qib_devdata,
					      verbs_dev);
	u16 qpt_mask = dd->qpn_mask;

	if (type == IB_QPT_SMI || type == IB_QPT_GSI) {
		unsigned n;

		ret = type == IB_QPT_GSI;
		n = 1 << (ret + 2 * (port - 1));
		spin_lock(&qpt->lock);
		if (qpt->flags & n)
			ret = -EINVAL;
		else
			qpt->flags |= n;
		spin_unlock(&qpt->lock);
		goto bail;
	}

	qpn = qpt->last + 2;
	if (qpn >= RVT_QPN_MAX)
		qpn = 2;
<<<<<<< HEAD
	if (qpt_mask && ((qpn & qpt_mask) >> 1) >= dd->n_krcv_queues)
		qpn = (qpn | qpt_mask) + 2;
=======
	if (qpt->mask && ((qpn & qpt->mask) >> 1) >= dd->n_krcv_queues)
		qpn = (qpn | qpt->mask) + 2;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	offset = qpn & RVT_BITS_PER_PAGE_MASK;
	map = &qpt->map[qpn / RVT_BITS_PER_PAGE];
	max_scan = qpt->nmaps - !offset;
	for (i = 0;;) {
		if (unlikely(!map->page)) {
			get_map_page(qpt, map, gfp);
			if (unlikely(!map->page))
				break;
		}
		do {
			if (!test_and_set_bit(offset, map->page)) {
				qpt->last = qpn;
				ret = qpn;
				goto bail;
			}
			offset = find_next_offset(qpt, map, offset,
				dd->n_krcv_queues, qpt_mask);
			qpn = mk_qpn(qpt, map, offset);
			/*
			 * This test differs from alloc_pidmap().
			 * If find_next_offset() does find a zero
			 * bit, we don't need to check for QPN
			 * wrapping around past our starting QPN.
			 * We just need to be sure we don't loop
			 * forever.
			 */
<<<<<<< HEAD
		} while (offset < RVT_BITS_PER_PAGE && qpn < RVT_QPN_MAX);
=======
		} while (offset < RVT_BITS_PER_PAGE && qpn < QPN_MAX);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
		/*
		 * In order to keep the number of pages allocated to a
		 * minimum, we scan the all existing pages before increasing
		 * the size of the bitmap table.
		 */
		if (++i > max_scan) {
			if (qpt->nmaps == RVT_QPNMAP_ENTRIES)
				break;
			map = &qpt->map[qpt->nmaps++];
			offset = 0;
		} else if (map < &qpt->map[qpt->nmaps]) {
			++map;
			offset = 0;
		} else {
			map = &qpt->map[0];
			offset = 2;
		}
		qpn = mk_qpn(qpt, map, offset);
	}

	ret = -ENOMEM;

bail:
	return ret;
}

<<<<<<< HEAD
=======
static void free_qpn(struct qib_qpn_table *qpt, u32 qpn)
{
	struct qpn_map *map;

	map = qpt->map + qpn / RVT_BITS_PER_PAGE;
	if (map->page)
		clear_bit(qpn & RVT_BITS_PER_PAGE_MASK, map->page);
}

static inline unsigned qpn_hash(struct qib_ibdev *dev, u32 qpn)
{
	return jhash_1word(qpn, dev->qp_rnd) &
		(dev->qp_table_size - 1);
}


/*
 * Put the QP into the hash table.
 * The hash table holds a reference to the QP.
 */
static void insert_qp(struct qib_ibdev *dev, struct qib_qp *qp)
{
	struct qib_ibport *ibp = to_iport(qp->ibqp.device, qp->port_num);
	unsigned long flags;
	unsigned n = qpn_hash(dev, qp->ibqp.qp_num);

	atomic_inc(&qp->refcount);
	spin_lock_irqsave(&dev->qpt_lock, flags);

	if (qp->ibqp.qp_num == 0)
		rcu_assign_pointer(ibp->qp0, qp);
	else if (qp->ibqp.qp_num == 1)
		rcu_assign_pointer(ibp->qp1, qp);
	else {
		qp->next = dev->qp_table[n];
		rcu_assign_pointer(dev->qp_table[n], qp);
	}

	spin_unlock_irqrestore(&dev->qpt_lock, flags);
}

/*
 * Remove the QP from the table so it can't be found asynchronously by
 * the receive interrupt routine.
 */
static void remove_qp(struct qib_ibdev *dev, struct qib_qp *qp)
{
	struct qib_ibport *ibp = to_iport(qp->ibqp.device, qp->port_num);
	unsigned n = qpn_hash(dev, qp->ibqp.qp_num);
	unsigned long flags;
	int removed = 1;

	spin_lock_irqsave(&dev->qpt_lock, flags);

	if (rcu_dereference_protected(ibp->qp0,
			lockdep_is_held(&dev->qpt_lock)) == qp) {
		RCU_INIT_POINTER(ibp->qp0, NULL);
	} else if (rcu_dereference_protected(ibp->qp1,
			lockdep_is_held(&dev->qpt_lock)) == qp) {
		RCU_INIT_POINTER(ibp->qp1, NULL);
	} else {
		struct qib_qp *q;
		struct qib_qp __rcu **qpp;

		removed = 0;
		qpp = &dev->qp_table[n];
		for (; (q = rcu_dereference_protected(*qpp,
				lockdep_is_held(&dev->qpt_lock))) != NULL;
				qpp = &q->next)
			if (q == qp) {
				RCU_INIT_POINTER(*qpp,
					rcu_dereference_protected(qp->next,
					 lockdep_is_held(&dev->qpt_lock)));
				removed = 1;
				break;
			}
	}

	spin_unlock_irqrestore(&dev->qpt_lock, flags);
	if (removed) {
		synchronize_rcu();
		atomic_dec(&qp->refcount);
	}
}

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
/**
 * qib_free_all_qps - check for QPs still in use
 */
unsigned qib_free_all_qps(struct rvt_dev_info *rdi)
{
	struct qib_ibdev *verbs_dev = container_of(rdi, struct qib_ibdev, rdi);
	struct qib_devdata *dd = container_of(verbs_dev, struct qib_devdata,
					      verbs_dev);
	unsigned n, qp_inuse = 0;

	for (n = 0; n < dd->num_pports; n++) {
		struct qib_ibport *ibp = &dd->pport[n].ibport_data;

		rcu_read_lock();
		if (rcu_dereference(ibp->rvp.qp[0]))
			qp_inuse++;
		if (rcu_dereference(ibp->rvp.qp[1]))
			qp_inuse++;
		rcu_read_unlock();
	}
	return qp_inuse;
}

void qib_notify_qp_reset(struct rvt_qp *qp)
{
	struct qib_qp_priv *priv = qp->priv;

	atomic_set(&priv->s_dma_busy, 0);
}

void qib_notify_error_qp(struct rvt_qp *qp)
{
	struct qib_qp_priv *priv = qp->priv;
	struct qib_ibdev *dev = to_idev(qp->ibqp.device);

	spin_lock(&dev->rdi.pending_lock);
	if (!list_empty(&priv->iowait) && !(qp->s_flags & RVT_S_BUSY)) {
		qp->s_flags &= ~RVT_S_ANY_WAIT_IO;
		list_del_init(&priv->iowait);
	}
	spin_unlock(&dev->rdi.pending_lock);

	if (!(qp->s_flags & RVT_S_BUSY)) {
		qp->s_hdrwords = 0;
		if (qp->s_rdma_mr) {
			rvt_put_mr(qp->s_rdma_mr);
			qp->s_rdma_mr = NULL;
		}
		if (priv->s_tx) {
			qib_put_txreq(priv->s_tx);
			priv->s_tx = NULL;
		}
	}
}

static int mtu_to_enum(u32 mtu)
{
	int enum_mtu;

	switch (mtu) {
	case 4096:
		enum_mtu = IB_MTU_4096;
		break;
	case 2048:
		enum_mtu = IB_MTU_2048;
		break;
	case 1024:
		enum_mtu = IB_MTU_1024;
		break;
	case 512:
		enum_mtu = IB_MTU_512;
		break;
	case 256:
		enum_mtu = IB_MTU_256;
		break;
	default:
		enum_mtu = IB_MTU_2048;
	}
	return enum_mtu;
}

int qib_get_pmtu_from_attr(struct rvt_dev_info *rdi, struct rvt_qp *qp,
			   struct ib_qp_attr *attr)
{
	int mtu, pmtu, pidx = qp->port_num - 1;
	struct qib_ibdev *verbs_dev = container_of(rdi, struct qib_ibdev, rdi);
	struct qib_devdata *dd = container_of(verbs_dev, struct qib_devdata,
					      verbs_dev);
	mtu = ib_mtu_enum_to_int(attr->path_mtu);
	if (mtu == -1)
		return -EINVAL;

	if (mtu > dd->pport[pidx].ibmtu)
		pmtu = mtu_to_enum(dd->pport[pidx].ibmtu);
	else
		pmtu = attr->path_mtu;
	return pmtu;
}

int qib_mtu_to_path_mtu(u32 mtu)
{
	return mtu_to_enum(mtu);
}

u32 qib_mtu_from_qp(struct rvt_dev_info *rdi, struct rvt_qp *qp, u32 pmtu)
{
	return ib_mtu_enum_to_int(pmtu);
}

void *qib_qp_priv_alloc(struct rvt_dev_info *rdi, struct rvt_qp *qp)
{
	struct qib_qp_priv *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return ERR_PTR(-ENOMEM);
	priv->owner = qp;

	priv->s_hdr = kzalloc(sizeof(*priv->s_hdr), GFP_KERNEL);
	if (!priv->s_hdr) {
		kfree(priv);
		return ERR_PTR(-ENOMEM);
	}
	init_waitqueue_head(&priv->wait_dma);
	INIT_WORK(&priv->s_work, _qib_do_send);
	INIT_LIST_HEAD(&priv->iowait);

	return priv;
}

void qib_qp_priv_free(struct rvt_dev_info *rdi, struct rvt_qp *qp)
{
	struct qib_qp_priv *priv = qp->priv;

	kfree(priv->s_hdr);
	kfree(priv);
}

void qib_stop_send_queue(struct rvt_qp *qp)
{
	struct qib_qp_priv *priv = qp->priv;

	cancel_work_sync(&priv->s_work);
}

void qib_quiesce_qp(struct rvt_qp *qp)
{
	struct qib_qp_priv *priv = qp->priv;

	wait_event(priv->wait_dma, !atomic_read(&priv->s_dma_busy));
	if (priv->s_tx) {
		qib_put_txreq(priv->s_tx);
		priv->s_tx = NULL;
	}
}

void qib_flush_qp_waiters(struct rvt_qp *qp)
{
	struct qib_qp_priv *priv = qp->priv;
	struct qib_ibdev *dev = to_idev(qp->ibqp.device);

	spin_lock(&dev->rdi.pending_lock);
	if (!list_empty(&priv->iowait))
		list_del_init(&priv->iowait);
	spin_unlock(&dev->rdi.pending_lock);
}

/**
 * qib_check_send_wqe - validate wr/wqe
 * @qp - The qp
 * @wqe - The built wqe
 * @call_send - Determine if the send should be posted or scheduled
 *
 * Returns 0 on success, -EINVAL on failure
 */
int qib_check_send_wqe(struct rvt_qp *qp,
		       struct rvt_swqe *wqe, bool *call_send)
{
	struct rvt_ah *ah;

<<<<<<< HEAD
	switch (qp->ibqp.qp_type) {
	case IB_QPT_RC:
	case IB_QPT_UC:
		if (wqe->length > 0x80000000U)
			return -EINVAL;
		if (wqe->length > qp->pmtu)
			*call_send = false;
		break;
=======
	if (qp->ibqp.srq) {
		/*
		 * Shared receive queues don't generate credits.
		 * Set the credit field to the invalid value.
		 */
		aeth |= QIB_AETH_CREDIT_INVAL << QIB_AETH_CREDIT_SHIFT;
	} else {
		u32 min, max, x;
		u32 credits;
		struct qib_rwq *wq = qp->r_rq.wq;
		u32 head;
		u32 tail;

		/* sanity check pointers before trusting them */
		head = wq->head;
		if (head >= qp->r_rq.size)
			head = 0;
		tail = wq->tail;
		if (tail >= qp->r_rq.size)
			tail = 0;
		/*
		 * Compute the number of credits available (RWQEs).
		 * XXX Not holding the r_rq.lock here so there is a small
		 * chance that the pair of reads are not atomic.
		 */
		credits = head - tail;
		if ((int)credits < 0)
			credits += qp->r_rq.size;
		/*
		 * Binary search the credit table to find the code to
		 * use.
		 */
		min = 0;
		max = 31;
		for (;;) {
			x = (min + max) / 2;
			if (credit_table[x] == credits)
				break;
			if (credit_table[x] > credits)
				max = x;
			else if (min == x)
				break;
			else
				min = x;
		}
		aeth |= x << QIB_AETH_CREDIT_SHIFT;
	}
	return cpu_to_be32(aeth);
}

/**
 * qib_create_qp - create a queue pair for a device
 * @ibpd: the protection domain who's device we create the queue pair for
 * @init_attr: the attributes of the queue pair
 * @udata: user data for libibverbs.so
 *
 * Returns the queue pair on success, otherwise returns an errno.
 *
 * Called by the ib_create_qp() core verbs function.
 */
struct ib_qp *qib_create_qp(struct ib_pd *ibpd,
			    struct ib_qp_init_attr *init_attr,
			    struct ib_udata *udata)
{
	struct qib_qp *qp;
	int err;
	struct qib_swqe *swq = NULL;
	struct qib_ibdev *dev;
	struct qib_devdata *dd;
	size_t sz;
	size_t sg_list_sz;
	struct ib_qp *ret;
	gfp_t gfp;


	if (init_attr->cap.max_send_sge > ib_qib_max_sges ||
	    init_attr->cap.max_send_wr > ib_qib_max_qp_wrs ||
	    init_attr->create_flags & ~(IB_QP_CREATE_USE_GFP_NOIO))
		return ERR_PTR(-EINVAL);

	/* GFP_NOIO is applicable in RC QPs only */
	if (init_attr->create_flags & IB_QP_CREATE_USE_GFP_NOIO &&
	    init_attr->qp_type != IB_QPT_RC)
		return ERR_PTR(-EINVAL);

	gfp = init_attr->create_flags & IB_QP_CREATE_USE_GFP_NOIO ?
			GFP_NOIO : GFP_KERNEL;

	/* Check receive queue parameters if no SRQ is specified. */
	if (!init_attr->srq) {
		if (init_attr->cap.max_recv_sge > ib_qib_max_sges ||
		    init_attr->cap.max_recv_wr > ib_qib_max_qp_wrs) {
			ret = ERR_PTR(-EINVAL);
			goto bail;
		}
		if (init_attr->cap.max_send_sge +
		    init_attr->cap.max_send_wr +
		    init_attr->cap.max_recv_sge +
		    init_attr->cap.max_recv_wr == 0) {
			ret = ERR_PTR(-EINVAL);
			goto bail;
		}
	}

	switch (init_attr->qp_type) {
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	case IB_QPT_SMI:
	case IB_QPT_GSI:
	case IB_QPT_UD:
<<<<<<< HEAD
		ah = rvt_get_swqe_ah(wqe);
		if (wqe->length > (1 << ah->log_pmtu))
			return -EINVAL;
		/* progress hint */
		*call_send = true;
=======
		sz = sizeof(struct qib_sge) *
			init_attr->cap.max_send_sge +
			sizeof(struct qib_swqe);
		swq = __vmalloc((init_attr->cap.max_send_wr + 1) * sz,
				gfp, PAGE_KERNEL);
		if (swq == NULL) {
			ret = ERR_PTR(-ENOMEM);
			goto bail;
		}
		sz = sizeof(*qp);
		sg_list_sz = 0;
		if (init_attr->srq) {
			struct qib_srq *srq = to_isrq(init_attr->srq);

			if (srq->rq.max_sge > 1)
				sg_list_sz = sizeof(*qp->r_sg_list) *
					(srq->rq.max_sge - 1);
		} else if (init_attr->cap.max_recv_sge > 1)
			sg_list_sz = sizeof(*qp->r_sg_list) *
				(init_attr->cap.max_recv_sge - 1);
		qp = kzalloc(sz + sg_list_sz, gfp);
		if (!qp) {
			ret = ERR_PTR(-ENOMEM);
			goto bail_swq;
		}
		RCU_INIT_POINTER(qp->next, NULL);
		qp->s_hdr = kzalloc(sizeof(*qp->s_hdr), gfp);
		if (!qp->s_hdr) {
			ret = ERR_PTR(-ENOMEM);
			goto bail_qp;
		}
		qp->timeout_jiffies =
			usecs_to_jiffies((4096UL * (1UL << qp->timeout)) /
				1000UL);
		if (init_attr->srq)
			sz = 0;
		else {
			qp->r_rq.size = init_attr->cap.max_recv_wr + 1;
			qp->r_rq.max_sge = init_attr->cap.max_recv_sge;
			sz = (sizeof(struct ib_sge) * qp->r_rq.max_sge) +
				sizeof(struct qib_rwqe);
			if (gfp != GFP_NOIO)
				qp->r_rq.wq = vmalloc_user(
						sizeof(struct qib_rwq) +
						qp->r_rq.size * sz);
			else
				qp->r_rq.wq = __vmalloc(
						sizeof(struct qib_rwq) +
						qp->r_rq.size * sz,
						gfp, PAGE_KERNEL);

			if (!qp->r_rq.wq) {
				ret = ERR_PTR(-ENOMEM);
				goto bail_qp;
			}
		}

		/*
		 * ib_create_qp() will initialize qp->ibqp
		 * except for qp->ibqp.qp_num.
		 */
		spin_lock_init(&qp->r_lock);
		spin_lock_init(&qp->s_lock);
		spin_lock_init(&qp->r_rq.lock);
		atomic_set(&qp->refcount, 0);
		init_waitqueue_head(&qp->wait);
		init_waitqueue_head(&qp->wait_dma);
		init_timer(&qp->s_timer);
		qp->s_timer.data = (unsigned long)qp;
		INIT_WORK(&qp->s_work, qib_do_send);
		INIT_LIST_HEAD(&qp->iowait);
		INIT_LIST_HEAD(&qp->rspwait);
		qp->state = IB_QPS_RESET;
		qp->s_wq = swq;
		qp->s_size = init_attr->cap.max_send_wr + 1;
		qp->s_max_sge = init_attr->cap.max_send_sge;
		if (init_attr->sq_sig_type == IB_SIGNAL_REQ_WR)
			qp->s_flags = QIB_S_SIGNAL_REQ_WR;
		dev = to_idev(ibpd->device);
		dd = dd_from_dev(dev);
		err = alloc_qpn(dd, &dev->qpn_table, init_attr->qp_type,
				init_attr->port_num, gfp);
		if (err < 0) {
			ret = ERR_PTR(err);
			vfree(qp->r_rq.wq);
			goto bail_qp;
		}
		qp->ibqp.qp_num = err;
		qp->port_num = init_attr->port_num;
		qib_reset_qp(qp, init_attr->qp_type);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
		break;
	default:
		break;
	}
	return 0;
}

#ifdef CONFIG_DEBUG_FS

static const char * const qp_type_str[] = {
	"SMI", "GSI", "RC", "UC", "UD",
};

/**
 * qib_qp_iter_print - print information to seq_file
 * @s - the seq_file
 * @iter - the iterator
 */
void qib_qp_iter_print(struct seq_file *s, struct rvt_qp_iter *iter)
{
	struct rvt_swqe *wqe;
	struct rvt_qp *qp = iter->qp;
	struct qib_qp_priv *priv = qp->priv;

	wqe = rvt_get_swqe_ptr(qp, qp->s_last);
	seq_printf(s,
		   "N %d QP%u %s %u %u %u f=%x %u %u %u %u %u PSN %x %x %x %x %x (%u %u %u %u %u %u) QP%u LID %x\n",
		   iter->n,
		   qp->ibqp.qp_num,
		   qp_type_str[qp->ibqp.qp_type],
		   qp->state,
		   wqe->wr.opcode,
		   qp->s_hdrwords,
		   qp->s_flags,
		   atomic_read(&priv->s_dma_busy),
		   !list_empty(&priv->iowait),
		   qp->timeout,
		   wqe->ssn,
		   qp->s_lsn,
		   qp->s_last_psn,
		   qp->s_psn, qp->s_next_psn,
		   qp->s_sending_psn, qp->s_sending_hpsn,
		   qp->s_last, qp->s_acked, qp->s_cur,
		   qp->s_tail, qp->s_head, qp->s_size,
		   qp->remote_qpn,
		   rdma_ah_get_dlid(&qp->remote_ah_attr));
}

#endif
