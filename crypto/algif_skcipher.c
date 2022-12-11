// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * algif_skcipher: User-space interface for skcipher algorithms
 *
 * This file provides the user-space API for symmetric key ciphers.
 *
 * Copyright (c) 2010 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * The following concept of the memory management is used:
 *
 * The kernel maintains two SGLs, the TX SGL and the RX SGL. The TX SGL is
 * filled by user space with the data submitted via sendpage/sendmsg. Filling
 * up the TX SGL does not cause a crypto operation -- the data will only be
 * tracked by the kernel. Upon receipt of one recvmsg call, the caller must
 * provide a buffer which is tracked with the RX SGL.
 *
 * During the processing of the recvmsg operation, the cipher request is
 * allocated and prepared. As part of the recvmsg operation, the processed
 * TX buffers are extracted from the TX SGL into a separate SGL.
 *
 * After the completion of the crypto operation, the RX SGL and the cipher
 * request is released. The extracted TX SGL parts are released together with
 * the RX SGL release.
 */

#include <crypto/scatterwalk.h>
#include <crypto/skcipher.h>
#include <crypto/if_alg.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/net.h>
#include <net/sock.h>

<<<<<<< HEAD
=======
struct skcipher_sg_list {
	struct list_head list;

	int cur;

	struct scatterlist sg[0];
};

struct skcipher_tfm {
	struct crypto_skcipher *skcipher;
	bool has_key;
};

struct skcipher_ctx {
	struct list_head tsgl;
	struct af_alg_sgl rsgl;

	void *iv;

	struct af_alg_completion completion;

	atomic_t inflight;
	unsigned used;

	unsigned int len;
	bool more;
	bool merge;
	bool enc;

	struct skcipher_request req;
};

struct skcipher_async_rsgl {
	struct af_alg_sgl sgl;
	struct list_head list;
};

struct skcipher_async_req {
	struct kiocb *iocb;
	struct skcipher_async_rsgl first_sgl;
	struct list_head list;
	struct scatterlist *tsg;
	atomic_t *inflight;
	struct skcipher_request req;
};

#define MAX_SGL_ENTS ((4096 - sizeof(struct skcipher_sg_list)) / \
		      sizeof(struct scatterlist) - 1)

static void skcipher_free_async_sgls(struct skcipher_async_req *sreq)
{
	struct skcipher_async_rsgl *rsgl, *tmp;
	struct scatterlist *sgl;
	struct scatterlist *sg;
	int i, n;

	list_for_each_entry_safe(rsgl, tmp, &sreq->list, list) {
		af_alg_free_sg(&rsgl->sgl);
		if (rsgl != &sreq->first_sgl)
			kfree(rsgl);
	}
	sgl = sreq->tsg;
	n = sg_nents(sgl);
	for_each_sg(sgl, sg, n, i) {
		struct page *page = sg_page(sg);

		/* some SGs may not have a page mapped */
		if (page && atomic_read(&page->_count))
			put_page(page);
	}

	kfree(sreq->tsg);
}

static void skcipher_async_cb(struct crypto_async_request *req, int err)
{
	struct skcipher_async_req *sreq = req->data;
	struct kiocb *iocb = sreq->iocb;

	atomic_dec(sreq->inflight);
	skcipher_free_async_sgls(sreq);
	kzfree(sreq);
	iocb->ki_complete(iocb, err, err);
}

static inline int skcipher_sndbuf(struct sock *sk)
{
	struct alg_sock *ask = alg_sk(sk);
	struct skcipher_ctx *ctx = ask->private;

	return max_t(int, max_t(int, sk->sk_sndbuf & PAGE_MASK, PAGE_SIZE) -
			  ctx->used, 0);
}

static inline bool skcipher_writable(struct sock *sk)
{
	return PAGE_SIZE <= skcipher_sndbuf(sk);
}

static int skcipher_alloc_sgl(struct sock *sk)
{
	struct alg_sock *ask = alg_sk(sk);
	struct skcipher_ctx *ctx = ask->private;
	struct skcipher_sg_list *sgl;
	struct scatterlist *sg = NULL;

	sgl = list_entry(ctx->tsgl.prev, struct skcipher_sg_list, list);
	if (!list_empty(&ctx->tsgl))
		sg = sgl->sg;

	if (!sg || sgl->cur >= MAX_SGL_ENTS) {
		sgl = sock_kmalloc(sk, sizeof(*sgl) +
				       sizeof(sgl->sg[0]) * (MAX_SGL_ENTS + 1),
				   GFP_KERNEL);
		if (!sgl)
			return -ENOMEM;

		sg_init_table(sgl->sg, MAX_SGL_ENTS + 1);
		sgl->cur = 0;

		if (sg) {
			sg_chain(sg, MAX_SGL_ENTS + 1, sgl->sg);
			sg_unmark_end(sg + (MAX_SGL_ENTS - 1));
		}

		list_add_tail(&sgl->list, &ctx->tsgl);
	}

	return 0;
}

static void skcipher_pull_sgl(struct sock *sk, int used, int put)
{
	struct alg_sock *ask = alg_sk(sk);
	struct skcipher_ctx *ctx = ask->private;
	struct skcipher_sg_list *sgl;
	struct scatterlist *sg;
	int i;

	while (!list_empty(&ctx->tsgl)) {
		sgl = list_first_entry(&ctx->tsgl, struct skcipher_sg_list,
				       list);
		sg = sgl->sg;

		for (i = 0; i < sgl->cur; i++) {
			int plen = min_t(int, used, sg[i].length);

			if (!sg_page(sg + i))
				continue;

			sg[i].length -= plen;
			sg[i].offset += plen;

			used -= plen;
			ctx->used -= plen;

			if (sg[i].length)
				return;
			if (put)
				put_page(sg_page(sg + i));
			sg_assign_page(sg + i, NULL);
		}

		list_del(&sgl->list);
		sock_kfree_s(sk, sgl,
			     sizeof(*sgl) + sizeof(sgl->sg[0]) *
					    (MAX_SGL_ENTS + 1));
	}

	if (!ctx->used)
		ctx->merge = 0;
}

static void skcipher_free_sgl(struct sock *sk)
{
	struct alg_sock *ask = alg_sk(sk);
	struct skcipher_ctx *ctx = ask->private;

	skcipher_pull_sgl(sk, ctx->used, 1);
}

static int skcipher_wait_for_wmem(struct sock *sk, unsigned flags)
{
	long timeout;
	DEFINE_WAIT(wait);
	int err = -ERESTARTSYS;

	if (flags & MSG_DONTWAIT)
		return -EAGAIN;

	sk_set_bit(SOCKWQ_ASYNC_NOSPACE, sk);

	for (;;) {
		if (signal_pending(current))
			break;
		prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);
		timeout = MAX_SCHEDULE_TIMEOUT;
		if (sk_wait_event(sk, &timeout, skcipher_writable(sk))) {
			err = 0;
			break;
		}
	}
	finish_wait(sk_sleep(sk), &wait);

	return err;
}

static void skcipher_wmem_wakeup(struct sock *sk)
{
	struct socket_wq *wq;

	if (!skcipher_writable(sk))
		return;

	rcu_read_lock();
	wq = rcu_dereference(sk->sk_wq);
	if (wq_has_sleeper(wq))
		wake_up_interruptible_sync_poll(&wq->wait, POLLIN |
							   POLLRDNORM |
							   POLLRDBAND);
	sk_wake_async(sk, SOCK_WAKE_WAITD, POLL_IN);
	rcu_read_unlock();
}

static int skcipher_wait_for_data(struct sock *sk, unsigned flags)
{
	struct alg_sock *ask = alg_sk(sk);
	struct skcipher_ctx *ctx = ask->private;
	long timeout;
	DEFINE_WAIT(wait);
	int err = -ERESTARTSYS;

	if (flags & MSG_DONTWAIT) {
		return -EAGAIN;
	}

	sk_set_bit(SOCKWQ_ASYNC_WAITDATA, sk);

	for (;;) {
		if (signal_pending(current))
			break;
		prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);
		timeout = MAX_SCHEDULE_TIMEOUT;
		if (sk_wait_event(sk, &timeout, ctx->used)) {
			err = 0;
			break;
		}
	}
	finish_wait(sk_sleep(sk), &wait);

	sk_clear_bit(SOCKWQ_ASYNC_WAITDATA, sk);

	return err;
}

static void skcipher_data_wakeup(struct sock *sk)
{
	struct alg_sock *ask = alg_sk(sk);
	struct skcipher_ctx *ctx = ask->private;
	struct socket_wq *wq;

	if (!ctx->used)
		return;

	rcu_read_lock();
	wq = rcu_dereference(sk->sk_wq);
	if (wq_has_sleeper(wq))
		wake_up_interruptible_sync_poll(&wq->wait, POLLOUT |
							   POLLRDNORM |
							   POLLRDBAND);
	sk_wake_async(sk, SOCK_WAKE_SPACE, POLL_OUT);
	rcu_read_unlock();
}

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
static int skcipher_sendmsg(struct socket *sock, struct msghdr *msg,
			    size_t size)
{
	struct sock *sk = sock->sk;
	struct alg_sock *ask = alg_sk(sk);
	struct sock *psk = ask->parent;
	struct alg_sock *pask = alg_sk(psk);
<<<<<<< HEAD
	struct crypto_skcipher *tfm = pask->private;
=======
	struct skcipher_ctx *ctx = ask->private;
	struct skcipher_tfm *skc = pask->private;
	struct crypto_skcipher *tfm = skc->skcipher;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	unsigned ivsize = crypto_skcipher_ivsize(tfm);

	return af_alg_sendmsg(sock, msg, size, ivsize);
}

static int _skcipher_recvmsg(struct socket *sock, struct msghdr *msg,
			     size_t ignored, int flags)
{
	struct sock *sk = sock->sk;
	struct alg_sock *ask = alg_sk(sk);
	struct sock *psk = ask->parent;
	struct alg_sock *pask = alg_sk(psk);
	struct af_alg_ctx *ctx = ask->private;
	struct crypto_skcipher *tfm = pask->private;
	unsigned int bs = crypto_skcipher_chunksize(tfm);
	struct af_alg_async_req *areq;
	int err = 0;
	size_t len = 0;

	if (!ctx->init || (ctx->more && ctx->used < bs)) {
		err = af_alg_wait_for_data(sk, flags, bs);
		if (err)
			return err;
	}

	/* Allocate cipher request for current operation. */
	areq = af_alg_alloc_areq(sk, sizeof(struct af_alg_async_req) +
				     crypto_skcipher_reqsize(tfm));
	if (IS_ERR(areq))
		return PTR_ERR(areq);

<<<<<<< HEAD
	/* convert iovecs of output buffers into RX SGL */
	err = af_alg_get_rsgl(sk, msg, flags, areq, ctx->used, &len);
=======
	lock_sock(sk);
	if (!ctx->more && ctx->used)
		goto unlock;

	if (init) {
		ctx->enc = enc;
		if (con.iv)
			memcpy(ctx->iv, con.iv->iv, ivsize);
	}

	while (size) {
		struct scatterlist *sg;
		unsigned long len = size;
		int plen;

		if (ctx->merge) {
			sgl = list_entry(ctx->tsgl.prev,
					 struct skcipher_sg_list, list);
			sg = sgl->sg + sgl->cur - 1;
			len = min_t(unsigned long, len,
				    PAGE_SIZE - sg->offset - sg->length);

			err = memcpy_from_msg(page_address(sg_page(sg)) +
					      sg->offset + sg->length,
					      msg, len);
			if (err)
				goto unlock;

			sg->length += len;
			ctx->merge = (sg->offset + sg->length) &
				     (PAGE_SIZE - 1);

			ctx->used += len;
			copied += len;
			size -= len;
			continue;
		}

		if (!skcipher_writable(sk)) {
			err = skcipher_wait_for_wmem(sk, msg->msg_flags);
			if (err)
				goto unlock;
		}

		len = min_t(unsigned long, len, skcipher_sndbuf(sk));

		err = skcipher_alloc_sgl(sk);
		if (err)
			goto unlock;

		sgl = list_entry(ctx->tsgl.prev, struct skcipher_sg_list, list);
		sg = sgl->sg;
		if (sgl->cur)
			sg_unmark_end(sg + sgl->cur - 1);
		do {
			i = sgl->cur;
			plen = min_t(int, len, PAGE_SIZE);

			sg_assign_page(sg + i, alloc_page(GFP_KERNEL));
			err = -ENOMEM;
			if (!sg_page(sg + i))
				goto unlock;

			err = memcpy_from_msg(page_address(sg_page(sg + i)),
					      msg, plen);
			if (err) {
				__free_page(sg_page(sg + i));
				sg_assign_page(sg + i, NULL);
				goto unlock;
			}

			sg[i].length = plen;
			len -= plen;
			ctx->used += plen;
			copied += plen;
			size -= plen;
			sgl->cur++;
		} while (len && sgl->cur < MAX_SGL_ENTS);

		if (!size)
			sg_mark_end(sg + sgl->cur - 1);

		ctx->merge = plen & (PAGE_SIZE - 1);
	}

	err = 0;

	ctx->more = msg->msg_flags & MSG_MORE;

unlock:
	skcipher_data_wakeup(sk);
	release_sock(sk);

	return copied ?: err;
}

static ssize_t skcipher_sendpage(struct socket *sock, struct page *page,
				 int offset, size_t size, int flags)
{
	struct sock *sk = sock->sk;
	struct alg_sock *ask = alg_sk(sk);
	struct skcipher_ctx *ctx = ask->private;
	struct skcipher_sg_list *sgl;
	int err = -EINVAL;

	if (flags & MSG_SENDPAGE_NOTLAST)
		flags |= MSG_MORE;

	lock_sock(sk);
	if (!ctx->more && ctx->used)
		goto unlock;

	if (!size)
		goto done;

	if (!skcipher_writable(sk)) {
		err = skcipher_wait_for_wmem(sk, flags);
		if (err)
			goto unlock;
	}

	err = skcipher_alloc_sgl(sk);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	if (err)
		goto free;

	/*
	 * If more buffers are to be expected to be processed, process only
	 * full block size buffers.
	 */
	if (ctx->more || len < ctx->used)
		len -= len % bs;

	/*
	 * Create a per request TX SGL for this request which tracks the
	 * SG entries from the global TX SGL.
	 */
	areq->tsgl_entries = af_alg_count_tsgl(sk, len, 0);
	if (!areq->tsgl_entries)
		areq->tsgl_entries = 1;
	areq->tsgl = sock_kmalloc(sk, array_size(sizeof(*areq->tsgl),
						 areq->tsgl_entries),
				  GFP_KERNEL);
	if (!areq->tsgl) {
		err = -ENOMEM;
		goto free;
	}
	sg_init_table(areq->tsgl, areq->tsgl_entries);
	af_alg_pull_tsgl(sk, len, areq->tsgl, 0);

<<<<<<< HEAD
	/* Initialize the crypto operation */
	skcipher_request_set_tfm(&areq->cra_u.skcipher_req, tfm);
	skcipher_request_set_crypt(&areq->cra_u.skcipher_req, areq->tsgl,
				   areq->first_rsgl.sgl.sg, len, ctx->iv);

	if (msg->msg_iocb && !is_sync_kiocb(msg->msg_iocb)) {
		/* AIO operation */
		sock_hold(sk);
		areq->iocb = msg->msg_iocb;

		/* Remember output size that will be generated. */
		areq->outlen = len;
=======
static int skcipher_recvmsg_async(struct socket *sock, struct msghdr *msg,
				  int flags)
{
	struct sock *sk = sock->sk;
	struct alg_sock *ask = alg_sk(sk);
	struct sock *psk = ask->parent;
	struct alg_sock *pask = alg_sk(psk);
	struct skcipher_ctx *ctx = ask->private;
	struct skcipher_tfm *skc = pask->private;
	struct crypto_skcipher *tfm = skc->skcipher;
	struct skcipher_sg_list *sgl;
	struct scatterlist *sg;
	struct skcipher_async_req *sreq;
	struct skcipher_request *req;
	struct skcipher_async_rsgl *last_rsgl = NULL;
	unsigned int txbufs = 0, len = 0, tx_nents;
	unsigned int reqsize = crypto_skcipher_reqsize(tfm);
	unsigned int ivsize = crypto_skcipher_ivsize(tfm);
	int err = -ENOMEM;
	bool mark = false;
	char *iv;

	sreq = kzalloc(sizeof(*sreq) + reqsize + ivsize, GFP_KERNEL);
	if (unlikely(!sreq))
		goto out;

	req = &sreq->req;
	iv = (char *)(req + 1) + reqsize;
	sreq->iocb = msg->msg_iocb;
	INIT_LIST_HEAD(&sreq->list);
	sreq->inflight = &ctx->inflight;

	lock_sock(sk);
	tx_nents = skcipher_all_sg_nents(ctx);
	sreq->tsg = kcalloc(tx_nents, sizeof(*sg), GFP_KERNEL);
	if (unlikely(!sreq->tsg))
		goto unlock;
	sg_init_table(sreq->tsg, tx_nents);
	memcpy(iv, ctx->iv, ivsize);
	skcipher_request_set_tfm(req, tfm);
	skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_SLEEP,
				      skcipher_async_cb, sreq);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

		skcipher_request_set_callback(&areq->cra_u.skcipher_req,
					      CRYPTO_TFM_REQ_MAY_SLEEP,
					      af_alg_async_cb, areq);
		err = ctx->enc ?
			crypto_skcipher_encrypt(&areq->cra_u.skcipher_req) :
			crypto_skcipher_decrypt(&areq->cra_u.skcipher_req);

		/* AIO operation in progress */
		if (err == -EINPROGRESS)
			return -EIOCBQUEUED;

		sock_put(sk);
	} else {
		/* Synchronous operation */
		skcipher_request_set_callback(&areq->cra_u.skcipher_req,
					      CRYPTO_TFM_REQ_MAY_SLEEP |
					      CRYPTO_TFM_REQ_MAY_BACKLOG,
					      crypto_req_done, &ctx->wait);
		err = crypto_wait_req(ctx->enc ?
			crypto_skcipher_encrypt(&areq->cra_u.skcipher_req) :
			crypto_skcipher_decrypt(&areq->cra_u.skcipher_req),
						 &ctx->wait);
	}

<<<<<<< HEAD
=======
	if (mark)
		sg_mark_end(sreq->tsg + txbufs - 1);

	skcipher_request_set_crypt(req, sreq->tsg, sreq->first_sgl.sgl.sg,
				   len, iv);
	err = ctx->enc ? crypto_skcipher_encrypt(req) :
			 crypto_skcipher_decrypt(req);
	if (err == -EINPROGRESS) {
		atomic_inc(&ctx->inflight);
		err = -EIOCBQUEUED;
		sreq = NULL;
		goto unlock;
	}
free:
	skcipher_free_async_sgls(sreq);
unlock:
	skcipher_wmem_wakeup(sk);
	release_sock(sk);
	kzfree(sreq);
out:
	return err;
}

static int skcipher_recvmsg_sync(struct socket *sock, struct msghdr *msg,
				 int flags)
{
	struct sock *sk = sock->sk;
	struct alg_sock *ask = alg_sk(sk);
	struct sock *psk = ask->parent;
	struct alg_sock *pask = alg_sk(psk);
	struct skcipher_ctx *ctx = ask->private;
	struct skcipher_tfm *skc = pask->private;
	struct crypto_skcipher *tfm = skc->skcipher;
	unsigned bs = crypto_skcipher_blocksize(tfm);
	struct skcipher_sg_list *sgl;
	struct scatterlist *sg;
	int err = -EAGAIN;
	int used;
	long copied = 0;

	lock_sock(sk);
	while (msg_data_left(msg)) {
		if (!ctx->used) {
			err = skcipher_wait_for_data(sk, flags);
			if (err)
				goto unlock;
		}

		used = min_t(unsigned long, ctx->used, msg_data_left(msg));

		used = af_alg_make_sg(&ctx->rsgl, &msg->msg_iter, used);
		err = used;
		if (err < 0)
			goto unlock;

		if (ctx->more || used < ctx->used)
			used -= used % bs;

		err = -EINVAL;
		if (!used)
			goto free;

		sgl = list_first_entry(&ctx->tsgl,
				       struct skcipher_sg_list, list);
		sg = sgl->sg;

		while (!sg->length)
			sg++;

		skcipher_request_set_crypt(&ctx->req, sg, ctx->rsgl.sg, used,
					   ctx->iv);

		err = af_alg_wait_for_completion(
				ctx->enc ?
					crypto_skcipher_encrypt(&ctx->req) :
					crypto_skcipher_decrypt(&ctx->req),
				&ctx->completion);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

free:
	af_alg_free_resources(areq);

	return err ? err : len;
}

static int skcipher_recvmsg(struct socket *sock, struct msghdr *msg,
			    size_t ignored, int flags)
{
	struct sock *sk = sock->sk;
	int ret = 0;

	lock_sock(sk);
	while (msg_data_left(msg)) {
		int err = _skcipher_recvmsg(sock, msg, ignored, flags);

		/*
		 * This error covers -EIOCBQUEUED which implies that we can
		 * only handle one AIO request. If the caller wants to have
		 * multiple AIO requests in parallel, he must make multiple
		 * separate AIO calls.
		 *
		 * Also return the error if no data has been processed so far.
		 */
		if (err <= 0) {
			if (err == -EIOCBQUEUED || !ret)
				ret = err;
			goto out;
		}

		ret += err;
	}

out:
	af_alg_wmem_wakeup(sk);
	release_sock(sk);
	return ret;
}

static struct proto_ops algif_skcipher_ops = {
	.family		=	PF_ALG,

	.connect	=	sock_no_connect,
	.socketpair	=	sock_no_socketpair,
	.getname	=	sock_no_getname,
	.ioctl		=	sock_no_ioctl,
	.listen		=	sock_no_listen,
	.shutdown	=	sock_no_shutdown,
	.getsockopt	=	sock_no_getsockopt,
	.mmap		=	sock_no_mmap,
	.bind		=	sock_no_bind,
	.accept		=	sock_no_accept,
	.setsockopt	=	sock_no_setsockopt,

	.release	=	af_alg_release,
	.sendmsg	=	skcipher_sendmsg,
	.sendpage	=	af_alg_sendpage,
	.recvmsg	=	skcipher_recvmsg,
	.poll		=	af_alg_poll,
};

static int skcipher_check_key(struct socket *sock)
{
	int err = 0;
	struct sock *psk;
	struct alg_sock *pask;
	struct crypto_skcipher *tfm;
	struct sock *sk = sock->sk;
	struct alg_sock *ask = alg_sk(sk);

	lock_sock(sk);
	if (!atomic_read(&ask->nokey_refcnt))
		goto unlock_child;

	psk = ask->parent;
	pask = alg_sk(ask->parent);
	tfm = pask->private;

	err = -ENOKEY;
	lock_sock_nested(psk, SINGLE_DEPTH_NESTING);
	if (crypto_skcipher_get_flags(tfm) & CRYPTO_TFM_NEED_KEY)
		goto unlock;

	atomic_dec(&pask->nokey_refcnt);
	atomic_set(&ask->nokey_refcnt, 0);

	err = 0;

unlock:
	release_sock(psk);
unlock_child:
	release_sock(sk);

	return err;
}

static int skcipher_sendmsg_nokey(struct socket *sock, struct msghdr *msg,
				  size_t size)
{
	int err;

	err = skcipher_check_key(sock);
	if (err)
		return err;

	return skcipher_sendmsg(sock, msg, size);
}

static ssize_t skcipher_sendpage_nokey(struct socket *sock, struct page *page,
				       int offset, size_t size, int flags)
{
	int err;

	err = skcipher_check_key(sock);
	if (err)
		return err;

	return af_alg_sendpage(sock, page, offset, size, flags);
}

static int skcipher_recvmsg_nokey(struct socket *sock, struct msghdr *msg,
				  size_t ignored, int flags)
{
	int err;

	err = skcipher_check_key(sock);
	if (err)
		return err;

	return skcipher_recvmsg(sock, msg, ignored, flags);
}

static struct proto_ops algif_skcipher_ops_nokey = {
	.family		=	PF_ALG,

	.connect	=	sock_no_connect,
	.socketpair	=	sock_no_socketpair,
	.getname	=	sock_no_getname,
	.ioctl		=	sock_no_ioctl,
	.listen		=	sock_no_listen,
	.shutdown	=	sock_no_shutdown,
	.getsockopt	=	sock_no_getsockopt,
	.mmap		=	sock_no_mmap,
	.bind		=	sock_no_bind,
	.accept		=	sock_no_accept,
	.setsockopt	=	sock_no_setsockopt,

	.release	=	af_alg_release,
	.sendmsg	=	skcipher_sendmsg_nokey,
	.sendpage	=	skcipher_sendpage_nokey,
	.recvmsg	=	skcipher_recvmsg_nokey,
	.poll		=	af_alg_poll,
};

static int skcipher_check_key(struct socket *sock)
{
	int err = 0;
	struct sock *psk;
	struct alg_sock *pask;
	struct skcipher_tfm *tfm;
	struct sock *sk = sock->sk;
	struct alg_sock *ask = alg_sk(sk);

	lock_sock(sk);
	if (ask->refcnt)
		goto unlock_child;

	psk = ask->parent;
	pask = alg_sk(ask->parent);
	tfm = pask->private;

	err = -ENOKEY;
	lock_sock_nested(psk, SINGLE_DEPTH_NESTING);
	if (!tfm->has_key)
		goto unlock;

	if (!pask->refcnt++)
		sock_hold(psk);

	ask->refcnt = 1;
	sock_put(psk);

	err = 0;

unlock:
	release_sock(psk);
unlock_child:
	release_sock(sk);

	return err;
}

static int skcipher_sendmsg_nokey(struct socket *sock, struct msghdr *msg,
				  size_t size)
{
	int err;

	err = skcipher_check_key(sock);
	if (err)
		return err;

	return skcipher_sendmsg(sock, msg, size);
}

static ssize_t skcipher_sendpage_nokey(struct socket *sock, struct page *page,
				       int offset, size_t size, int flags)
{
	int err;

	err = skcipher_check_key(sock);
	if (err)
		return err;

	return skcipher_sendpage(sock, page, offset, size, flags);
}

static int skcipher_recvmsg_nokey(struct socket *sock, struct msghdr *msg,
				  size_t ignored, int flags)
{
	int err;

	err = skcipher_check_key(sock);
	if (err)
		return err;

	return skcipher_recvmsg(sock, msg, ignored, flags);
}

static struct proto_ops algif_skcipher_ops_nokey = {
	.family		=	PF_ALG,

	.connect	=	sock_no_connect,
	.socketpair	=	sock_no_socketpair,
	.getname	=	sock_no_getname,
	.ioctl		=	sock_no_ioctl,
	.listen		=	sock_no_listen,
	.shutdown	=	sock_no_shutdown,
	.getsockopt	=	sock_no_getsockopt,
	.mmap		=	sock_no_mmap,
	.bind		=	sock_no_bind,
	.accept		=	sock_no_accept,
	.setsockopt	=	sock_no_setsockopt,

	.release	=	af_alg_release,
	.sendmsg	=	skcipher_sendmsg_nokey,
	.sendpage	=	skcipher_sendpage_nokey,
	.recvmsg	=	skcipher_recvmsg_nokey,
	.poll		=	skcipher_poll,
};

static void *skcipher_bind(const char *name, u32 type, u32 mask)
{
	struct skcipher_tfm *tfm;
	struct crypto_skcipher *skcipher;

	tfm = kzalloc(sizeof(*tfm), GFP_KERNEL);
	if (!tfm)
		return ERR_PTR(-ENOMEM);

	skcipher = crypto_alloc_skcipher(name, type, mask);
	if (IS_ERR(skcipher)) {
		kfree(tfm);
		return ERR_CAST(skcipher);
	}

	tfm->skcipher = skcipher;

	return tfm;
}

static void skcipher_release(void *private)
{
	struct skcipher_tfm *tfm = private;

	crypto_free_skcipher(tfm->skcipher);
	kfree(tfm);
}

static int skcipher_setkey(void *private, const u8 *key, unsigned int keylen)
{
	struct skcipher_tfm *tfm = private;
	int err;

	err = crypto_skcipher_setkey(tfm->skcipher, key, keylen);
	tfm->has_key = !err;

	return err;
}

static void skcipher_sock_destruct(struct sock *sk)
{
	struct alg_sock *ask = alg_sk(sk);
	struct af_alg_ctx *ctx = ask->private;
	struct sock *psk = ask->parent;
	struct alg_sock *pask = alg_sk(psk);
	struct crypto_skcipher *tfm = pask->private;

	af_alg_pull_tsgl(sk, ctx->used, NULL, 0);
	sock_kzfree_s(sk, ctx->iv, crypto_skcipher_ivsize(tfm));
	sock_kfree_s(sk, ctx, ctx->len);
	af_alg_release_parent(sk);
}

static int skcipher_accept_parent_nokey(void *private, struct sock *sk)
{
	struct af_alg_ctx *ctx;
	struct alg_sock *ask = alg_sk(sk);
<<<<<<< HEAD
	struct crypto_skcipher *tfm = private;
	unsigned int len = sizeof(*ctx);
=======
	struct skcipher_tfm *tfm = private;
	struct crypto_skcipher *skcipher = tfm->skcipher;
	unsigned int len = sizeof(*ctx) + crypto_skcipher_reqsize(skcipher);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

	ctx = sock_kmalloc(sk, len, GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	memset(ctx, 0, len);

<<<<<<< HEAD
	ctx->iv = sock_kmalloc(sk, crypto_skcipher_ivsize(tfm),
=======
	ctx->iv = sock_kmalloc(sk, crypto_skcipher_ivsize(skcipher),
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
			       GFP_KERNEL);
	if (!ctx->iv) {
		sock_kfree_s(sk, ctx, len);
		return -ENOMEM;
	}
	memset(ctx->iv, 0, crypto_skcipher_ivsize(tfm));

<<<<<<< HEAD
	INIT_LIST_HEAD(&ctx->tsgl_list);
=======
	memset(ctx->iv, 0, crypto_skcipher_ivsize(skcipher));

	INIT_LIST_HEAD(&ctx->tsgl);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	ctx->len = len;
	crypto_init_wait(&ctx->wait);

	ask->private = ctx;

<<<<<<< HEAD
=======
	skcipher_request_set_tfm(&ctx->req, skcipher);
	skcipher_request_set_callback(&ctx->req, CRYPTO_TFM_REQ_MAY_SLEEP |
						 CRYPTO_TFM_REQ_MAY_BACKLOG,
				      af_alg_complete, &ctx->completion);

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	sk->sk_destruct = skcipher_sock_destruct;

	return 0;
}

static int skcipher_accept_parent(void *private, struct sock *sk)
{
<<<<<<< HEAD
	struct crypto_skcipher *tfm = private;

	if (crypto_skcipher_get_flags(tfm) & CRYPTO_TFM_NEED_KEY)
=======
	struct skcipher_tfm *tfm = private;

	if (!tfm->has_key && crypto_skcipher_has_setkey(tfm->skcipher))
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
		return -ENOKEY;

	return skcipher_accept_parent_nokey(private, sk);
}

static const struct af_alg_type algif_type_skcipher = {
	.bind		=	skcipher_bind,
	.release	=	skcipher_release,
	.setkey		=	skcipher_setkey,
	.accept		=	skcipher_accept_parent,
	.accept_nokey	=	skcipher_accept_parent_nokey,
	.ops		=	&algif_skcipher_ops,
	.ops_nokey	=	&algif_skcipher_ops_nokey,
	.name		=	"skcipher",
	.owner		=	THIS_MODULE
};

static int __init algif_skcipher_init(void)
{
	return af_alg_register_type(&algif_type_skcipher);
}

static void __exit algif_skcipher_exit(void)
{
	int err = af_alg_unregister_type(&algif_type_skcipher);
	BUG_ON(err);
}

module_init(algif_skcipher_init);
module_exit(algif_skcipher_exit);
MODULE_LICENSE("GPL");
