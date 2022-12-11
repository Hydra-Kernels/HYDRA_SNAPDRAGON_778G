// SPDX-License-Identifier: GPL-2.0-only
#include <linux/etherdevice.h>
#include <linux/if_macvlan.h>
#include <linux/if_tap.h>
#include <linux/if_vlan.h>
#include <linux/interrupt.h>
#include <linux/nsproxy.h>
#include <linux/compat.h>
#include <linux/if_tun.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/cache.h>
#include <linux/sched/signal.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/idr.h>
#include <linux/fs.h>
#include <linux/uio.h>

#include <net/net_namespace.h>
#include <net/rtnetlink.h>
#include <net/sock.h>
#include <linux/virtio_net.h>
#include <linux/skb_array.h>

struct macvtap_dev {
	struct macvlan_dev vlan;
	struct tap_dev    tap;
};

/*
 * Variables for dealing with macvtaps device numbers.
 */
static dev_t macvtap_major;

static const void *macvtap_net_namespace(struct device *d)
{
	struct net_device *dev = to_net_dev(d->parent);
	return dev_net(dev);
}

static struct class macvtap_class = {
	.name = "macvtap",
	.owner = THIS_MODULE,
	.ns_type = &net_ns_type_operations,
	.namespace = macvtap_net_namespace,
};
static struct cdev macvtap_cdev;

#define TUN_OFFLOADS (NETIF_F_HW_CSUM | NETIF_F_TSO_ECN | NETIF_F_TSO | \
		      NETIF_F_TSO6)

static void macvtap_count_tx_dropped(struct tap_dev *tap)
{
	struct macvtap_dev *vlantap = container_of(tap, struct macvtap_dev, tap);
	struct macvlan_dev *vlan = &vlantap->vlan;

	this_cpu_inc(vlan->pcpu_stats->tx_dropped);
}

static void macvtap_count_rx_dropped(struct tap_dev *tap)
{
	struct macvtap_dev *vlantap = container_of(tap, struct macvtap_dev, tap);
	struct macvlan_dev *vlan = &vlantap->vlan;

	macvlan_count_rx(vlan, 0, 0, 0);
}

static void macvtap_update_features(struct tap_dev *tap,
				    netdev_features_t features)
{
	struct macvtap_dev *vlantap = container_of(tap, struct macvtap_dev, tap);
	struct macvlan_dev *vlan = &vlantap->vlan;

	vlan->set_features = features;
	netdev_update_features(vlan->dev);
}

static int macvtap_newlink(struct net *src_net, struct net_device *dev,
			   struct nlattr *tb[], struct nlattr *data[],
			   struct netlink_ext_ack *extack)
{
	struct macvtap_dev *vlantap = netdev_priv(dev);
	int err;

	INIT_LIST_HEAD(&vlantap->tap.queue_list);

	/* Since macvlan supports all offloads by default, make
	 * tap support all offloads also.
	 */
	vlantap->tap.tap_features = TUN_OFFLOADS;

	/* Register callbacks for rx/tx drops accounting and updating
	 * net_device features
	 */
	vlantap->tap.count_tx_dropped = macvtap_count_tx_dropped;
	vlantap->tap.count_rx_dropped = macvtap_count_rx_dropped;
	vlantap->tap.update_features  = macvtap_update_features;

	err = netdev_rx_handler_register(dev, tap_handle_frame, &vlantap->tap);
	if (err)
		return err;

	/* Don't put anything that may fail after macvlan_common_newlink
	 * because we can't undo what it does.
	 */
	err = macvlan_common_newlink(src_net, dev, tb, data, extack);
	if (err) {
		netdev_rx_handler_unregister(dev);
		return err;
	}

	vlantap->tap.dev = vlantap->vlan.dev;

	return 0;
}

static void macvtap_dellink(struct net_device *dev,
			    struct list_head *head)
{
	struct macvtap_dev *vlantap = netdev_priv(dev);

	netdev_rx_handler_unregister(dev);
	tap_del_queues(&vlantap->tap);
	macvlan_dellink(dev, head);
}

static void macvtap_setup(struct net_device *dev)
{
	macvlan_common_setup(dev);
	dev->tx_queue_len = TUN_READQ_SIZE;
}

static struct rtnl_link_ops macvtap_link_ops __read_mostly = {
	.kind		= "macvtap",
	.setup		= macvtap_setup,
	.newlink	= macvtap_newlink,
	.dellink	= macvtap_dellink,
	.priv_size      = sizeof(struct macvtap_dev),
};

<<<<<<< HEAD
=======

static void macvtap_sock_write_space(struct sock *sk)
{
	wait_queue_head_t *wqueue;

	if (!sock_writeable(sk) ||
	    !test_and_clear_bit(SOCKWQ_ASYNC_NOSPACE, &sk->sk_socket->flags))
		return;

	wqueue = sk_sleep(sk);
	if (wqueue && waitqueue_active(wqueue))
		wake_up_interruptible_poll(wqueue, POLLOUT | POLLWRNORM | POLLWRBAND);
}

static void macvtap_sock_destruct(struct sock *sk)
{
	skb_queue_purge(&sk->sk_receive_queue);
}

static int macvtap_open(struct inode *inode, struct file *file)
{
	struct net *net = current->nsproxy->net_ns;
	struct net_device *dev;
	struct macvtap_queue *q;
	int err = -ENODEV;

	rtnl_lock();
	dev = dev_get_by_macvtap_minor(iminor(inode));
	if (!dev)
		goto out;

	err = -ENOMEM;
	q = (struct macvtap_queue *)sk_alloc(net, AF_UNSPEC, GFP_KERNEL,
					     &macvtap_proto, 0);
	if (!q)
		goto out;

	RCU_INIT_POINTER(q->sock.wq, &q->wq);
	init_waitqueue_head(&q->wq.wait);
	q->sock.type = SOCK_RAW;
	q->sock.state = SS_CONNECTED;
	q->sock.file = file;
	q->sock.ops = &macvtap_socket_ops;
	sock_init_data(&q->sock, &q->sk);
	q->sk.sk_write_space = macvtap_sock_write_space;
	q->sk.sk_destruct = macvtap_sock_destruct;
	q->flags = IFF_VNET_HDR | IFF_NO_PI | IFF_TAP;
	q->vnet_hdr_sz = sizeof(struct virtio_net_hdr);

	/*
	 * so far only KVM virtio_net uses macvtap, enable zero copy between
	 * guest kernel and host kernel when lower device supports zerocopy
	 *
	 * The macvlan supports zerocopy iff the lower device supports zero
	 * copy so we don't have to look at the lower device directly.
	 */
	if ((dev->features & NETIF_F_HIGHDMA) && (dev->features & NETIF_F_SG))
		sock_set_flag(&q->sk, SOCK_ZEROCOPY);

	err = macvtap_set_queue(dev, file, q);
	if (err)
		sock_put(&q->sk);

out:
	if (dev)
		dev_put(dev);

	rtnl_unlock();
	return err;
}

static int macvtap_release(struct inode *inode, struct file *file)
{
	struct macvtap_queue *q = file->private_data;
	macvtap_put_queue(q);
	return 0;
}

static unsigned int macvtap_poll(struct file *file, poll_table * wait)
{
	struct macvtap_queue *q = file->private_data;
	unsigned int mask = POLLERR;

	if (!q)
		goto out;

	mask = 0;
	poll_wait(file, &q->wq.wait, wait);

	if (!skb_queue_empty(&q->sk.sk_receive_queue))
		mask |= POLLIN | POLLRDNORM;

	if (sock_writeable(&q->sk) ||
	    (!test_and_set_bit(SOCKWQ_ASYNC_NOSPACE, &q->sock.flags) &&
	     sock_writeable(&q->sk)))
		mask |= POLLOUT | POLLWRNORM;

out:
	return mask;
}

static inline struct sk_buff *macvtap_alloc_skb(struct sock *sk, size_t prepad,
						size_t len, size_t linear,
						int noblock, int *err)
{
	struct sk_buff *skb;

	/* Under a page?  Don't bother with paged skb. */
	if (prepad + len < PAGE_SIZE || !linear)
		linear = len;

	skb = sock_alloc_send_pskb(sk, prepad + linear, len - linear, noblock,
				   err, 0);
	if (!skb)
		return NULL;

	skb_reserve(skb, prepad);
	skb_put(skb, linear);
	skb->data_len = len - linear;
	skb->len += len - linear;

	return skb;
}

/*
 * macvtap_skb_from_vnet_hdr and macvtap_skb_to_vnet_hdr should
 * be shared with the tun/tap driver.
 */
static int macvtap_skb_from_vnet_hdr(struct macvtap_queue *q,
				     struct sk_buff *skb,
				     struct virtio_net_hdr *vnet_hdr)
{
	unsigned short gso_type = 0;
	if (vnet_hdr->gso_type != VIRTIO_NET_HDR_GSO_NONE) {
		switch (vnet_hdr->gso_type & ~VIRTIO_NET_HDR_GSO_ECN) {
		case VIRTIO_NET_HDR_GSO_TCPV4:
			gso_type = SKB_GSO_TCPV4;
			break;
		case VIRTIO_NET_HDR_GSO_TCPV6:
			gso_type = SKB_GSO_TCPV6;
			break;
		case VIRTIO_NET_HDR_GSO_UDP:
			gso_type = SKB_GSO_UDP;
			break;
		default:
			return -EINVAL;
		}

		if (vnet_hdr->gso_type & VIRTIO_NET_HDR_GSO_ECN)
			gso_type |= SKB_GSO_TCP_ECN;

		if (vnet_hdr->gso_size == 0)
			return -EINVAL;
	}

	if (vnet_hdr->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM) {
		if (!skb_partial_csum_set(skb, macvtap16_to_cpu(q, vnet_hdr->csum_start),
					  macvtap16_to_cpu(q, vnet_hdr->csum_offset)))
			return -EINVAL;
	}

	if (vnet_hdr->gso_type != VIRTIO_NET_HDR_GSO_NONE) {
		skb_shinfo(skb)->gso_size = macvtap16_to_cpu(q, vnet_hdr->gso_size);
		skb_shinfo(skb)->gso_type = gso_type;

		/* Header must be checked, and gso_segs computed. */
		skb_shinfo(skb)->gso_type |= SKB_GSO_DODGY;
		skb_shinfo(skb)->gso_segs = 0;
	}
	return 0;
}

static void macvtap_skb_to_vnet_hdr(struct macvtap_queue *q,
				    const struct sk_buff *skb,
				    struct virtio_net_hdr *vnet_hdr)
{
	memset(vnet_hdr, 0, sizeof(*vnet_hdr));

	if (skb_is_gso(skb)) {
		struct skb_shared_info *sinfo = skb_shinfo(skb);

		/* This is a hint as to how much should be linear. */
		vnet_hdr->hdr_len = cpu_to_macvtap16(q, skb_headlen(skb));
		vnet_hdr->gso_size = cpu_to_macvtap16(q, sinfo->gso_size);
		if (sinfo->gso_type & SKB_GSO_TCPV4)
			vnet_hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
		else if (sinfo->gso_type & SKB_GSO_TCPV6)
			vnet_hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV6;
		else if (sinfo->gso_type & SKB_GSO_UDP)
			vnet_hdr->gso_type = VIRTIO_NET_HDR_GSO_UDP;
		else
			BUG();
		if (sinfo->gso_type & SKB_GSO_TCP_ECN)
			vnet_hdr->gso_type |= VIRTIO_NET_HDR_GSO_ECN;
	} else
		vnet_hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		vnet_hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
		if (skb_vlan_tag_present(skb))
			vnet_hdr->csum_start = cpu_to_macvtap16(q,
				skb_checksum_start_offset(skb) + VLAN_HLEN);
		else
			vnet_hdr->csum_start = cpu_to_macvtap16(q,
				skb_checksum_start_offset(skb));
		vnet_hdr->csum_offset = cpu_to_macvtap16(q, skb->csum_offset);
	} else if (skb->ip_summed == CHECKSUM_UNNECESSARY) {
		vnet_hdr->flags = VIRTIO_NET_HDR_F_DATA_VALID;
	} /* else everything is zero */
}

/* Neighbour code has some assumptions on HH_DATA_MOD alignment */
#define MACVTAP_RESERVE HH_DATA_OFF(ETH_HLEN)

/* Get packet from user space buffer */
static ssize_t macvtap_get_user(struct macvtap_queue *q, struct msghdr *m,
				struct iov_iter *from, int noblock)
{
	int good_linear = SKB_MAX_HEAD(MACVTAP_RESERVE);
	struct sk_buff *skb;
	struct macvlan_dev *vlan;
	unsigned long total_len = iov_iter_count(from);
	unsigned long len = total_len;
	int err;
	struct virtio_net_hdr vnet_hdr = { 0 };
	int vnet_hdr_len = 0;
	int copylen = 0;
	int depth;
	bool zerocopy = false;
	size_t linear;
	ssize_t n;

	if (q->flags & IFF_VNET_HDR) {
		vnet_hdr_len = READ_ONCE(q->vnet_hdr_sz);

		err = -EINVAL;
		if (len < vnet_hdr_len)
			goto err;
		len -= vnet_hdr_len;

		err = -EFAULT;
		n = copy_from_iter(&vnet_hdr, sizeof(vnet_hdr), from);
		if (n != sizeof(vnet_hdr))
			goto err;
		iov_iter_advance(from, vnet_hdr_len - sizeof(vnet_hdr));
		if ((vnet_hdr.flags & VIRTIO_NET_HDR_F_NEEDS_CSUM) &&
		     macvtap16_to_cpu(q, vnet_hdr.csum_start) +
		     macvtap16_to_cpu(q, vnet_hdr.csum_offset) + 2 >
			     macvtap16_to_cpu(q, vnet_hdr.hdr_len))
			vnet_hdr.hdr_len = cpu_to_macvtap16(q,
				 macvtap16_to_cpu(q, vnet_hdr.csum_start) +
				 macvtap16_to_cpu(q, vnet_hdr.csum_offset) + 2);
		err = -EINVAL;
		if (macvtap16_to_cpu(q, vnet_hdr.hdr_len) > len)
			goto err;
	}

	err = -EINVAL;
	if (unlikely(len < ETH_HLEN))
		goto err;

	if (m && m->msg_control && sock_flag(&q->sk, SOCK_ZEROCOPY)) {
		struct iov_iter i;

		copylen = vnet_hdr.hdr_len ?
			macvtap16_to_cpu(q, vnet_hdr.hdr_len) : GOODCOPY_LEN;
		if (copylen > good_linear)
			copylen = good_linear;
		else if (copylen < ETH_HLEN)
			copylen = ETH_HLEN;
		linear = copylen;
		i = *from;
		iov_iter_advance(&i, copylen);
		if (iov_iter_npages(&i, INT_MAX) <= MAX_SKB_FRAGS)
			zerocopy = true;
	}

	if (!zerocopy) {
		copylen = len;
		linear = macvtap16_to_cpu(q, vnet_hdr.hdr_len);
		if (linear > good_linear)
			linear = good_linear;
		else if (linear < ETH_HLEN)
			linear = ETH_HLEN;
	}

	skb = macvtap_alloc_skb(&q->sk, MACVTAP_RESERVE, copylen,
				linear, noblock, &err);
	if (!skb)
		goto err;

	if (zerocopy)
		err = zerocopy_sg_from_iter(skb, from);
	else {
		err = skb_copy_datagram_from_iter(skb, 0, from, len);
		if (!err && m && m->msg_control) {
			struct ubuf_info *uarg = m->msg_control;
			uarg->callback(uarg, false);
		}
	}

	if (err)
		goto err_kfree;

	skb_set_network_header(skb, ETH_HLEN);
	skb_reset_mac_header(skb);
	skb->protocol = eth_hdr(skb)->h_proto;

	if (vnet_hdr_len) {
		err = macvtap_skb_from_vnet_hdr(q, skb, &vnet_hdr);
		if (err)
			goto err_kfree;
	}

	skb_probe_transport_header(skb, ETH_HLEN);

	/* Move network header to the right position for VLAN tagged packets */
	if ((skb->protocol == htons(ETH_P_8021Q) ||
	     skb->protocol == htons(ETH_P_8021AD)) &&
	    __vlan_get_protocol(skb, skb->protocol, &depth) != 0)
		skb_set_network_header(skb, depth);

	rcu_read_lock();
	vlan = rcu_dereference(q->vlan);
	/* copy skb_ubuf_info for callback when skb has no error */
	if (zerocopy) {
		skb_shinfo(skb)->destructor_arg = m->msg_control;
		skb_shinfo(skb)->tx_flags |= SKBTX_DEV_ZEROCOPY;
		skb_shinfo(skb)->tx_flags |= SKBTX_SHARED_FRAG;
	}
	if (vlan) {
		skb->dev = vlan->dev;
		dev_queue_xmit(skb);
	} else {
		kfree_skb(skb);
	}
	rcu_read_unlock();

	return total_len;

err_kfree:
	kfree_skb(skb);

err:
	rcu_read_lock();
	vlan = rcu_dereference(q->vlan);
	if (vlan)
		this_cpu_inc(vlan->pcpu_stats->tx_dropped);
	rcu_read_unlock();

	return err;
}

static ssize_t macvtap_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *file = iocb->ki_filp;
	struct macvtap_queue *q = file->private_data;

	return macvtap_get_user(q, NULL, from, file->f_flags & O_NONBLOCK);
}

/* Put packet to the user space buffer */
static ssize_t macvtap_put_user(struct macvtap_queue *q,
				const struct sk_buff *skb,
				struct iov_iter *iter)
{
	int ret;
	int vnet_hdr_len = 0;
	int vlan_offset = 0;
	int total;

	if (q->flags & IFF_VNET_HDR) {
		struct virtio_net_hdr vnet_hdr;
		vnet_hdr_len = READ_ONCE(q->vnet_hdr_sz);
		if (iov_iter_count(iter) < vnet_hdr_len)
			return -EINVAL;

		macvtap_skb_to_vnet_hdr(q, skb, &vnet_hdr);

		if (copy_to_iter(&vnet_hdr, sizeof(vnet_hdr), iter) !=
		    sizeof(vnet_hdr))
			return -EFAULT;

		iov_iter_advance(iter, vnet_hdr_len - sizeof(vnet_hdr));
	}
	total = vnet_hdr_len;
	total += skb->len;

	if (skb_vlan_tag_present(skb)) {
		struct {
			__be16 h_vlan_proto;
			__be16 h_vlan_TCI;
		} veth;
		veth.h_vlan_proto = skb->vlan_proto;
		veth.h_vlan_TCI = htons(skb_vlan_tag_get(skb));

		vlan_offset = offsetof(struct vlan_ethhdr, h_vlan_proto);
		total += VLAN_HLEN;

		ret = skb_copy_datagram_iter(skb, 0, iter, vlan_offset);
		if (ret || !iov_iter_count(iter))
			goto done;

		ret = copy_to_iter(&veth, sizeof(veth), iter);
		if (ret != sizeof(veth) || !iov_iter_count(iter))
			goto done;
	}

	ret = skb_copy_datagram_iter(skb, vlan_offset, iter,
				     skb->len - vlan_offset);

done:
	return ret ? ret : total;
}

static ssize_t macvtap_do_read(struct macvtap_queue *q,
			       struct iov_iter *to,
			       int noblock)
{
	DEFINE_WAIT(wait);
	struct sk_buff *skb;
	ssize_t ret = 0;

	if (!iov_iter_count(to))
		return 0;

	while (1) {
		if (!noblock)
			prepare_to_wait(sk_sleep(&q->sk), &wait,
					TASK_INTERRUPTIBLE);

		/* Read frames from the queue */
		skb = skb_dequeue(&q->sk.sk_receive_queue);
		if (skb)
			break;
		if (noblock) {
			ret = -EAGAIN;
			break;
		}
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
		/* Nothing to read, let's sleep */
		schedule();
	}
	if (!noblock)
		finish_wait(sk_sleep(&q->sk), &wait);

	if (skb) {
		ret = macvtap_put_user(q, skb, to);
		if (unlikely(ret < 0))
			kfree_skb(skb);
		else
			consume_skb(skb);
	}
	return ret;
}

static ssize_t macvtap_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	struct file *file = iocb->ki_filp;
	struct macvtap_queue *q = file->private_data;
	ssize_t len = iov_iter_count(to), ret;

	ret = macvtap_do_read(q, to, file->f_flags & O_NONBLOCK);
	ret = min_t(ssize_t, ret, len);
	if (ret > 0)
		iocb->ki_pos = ret;
	return ret;
}

static struct macvlan_dev *macvtap_get_vlan(struct macvtap_queue *q)
{
	struct macvlan_dev *vlan;

	ASSERT_RTNL();
	vlan = rtnl_dereference(q->vlan);
	if (vlan)
		dev_hold(vlan->dev);

	return vlan;
}

static void macvtap_put_vlan(struct macvlan_dev *vlan)
{
	dev_put(vlan->dev);
}

static int macvtap_ioctl_set_queue(struct file *file, unsigned int flags)
{
	struct macvtap_queue *q = file->private_data;
	struct macvlan_dev *vlan;
	int ret;

	vlan = macvtap_get_vlan(q);
	if (!vlan)
		return -EINVAL;

	if (flags & IFF_ATTACH_QUEUE)
		ret = macvtap_enable_queue(vlan->dev, file, q);
	else if (flags & IFF_DETACH_QUEUE)
		ret = macvtap_disable_queue(q);
	else
		ret = -EINVAL;

	macvtap_put_vlan(vlan);
	return ret;
}

static int set_offload(struct macvtap_queue *q, unsigned long arg)
{
	struct macvlan_dev *vlan;
	netdev_features_t features;
	netdev_features_t feature_mask = 0;

	vlan = rtnl_dereference(q->vlan);
	if (!vlan)
		return -ENOLINK;

	features = vlan->dev->features;

	if (arg & TUN_F_CSUM) {
		feature_mask = NETIF_F_HW_CSUM;

		if (arg & (TUN_F_TSO4 | TUN_F_TSO6)) {
			if (arg & TUN_F_TSO_ECN)
				feature_mask |= NETIF_F_TSO_ECN;
			if (arg & TUN_F_TSO4)
				feature_mask |= NETIF_F_TSO;
			if (arg & TUN_F_TSO6)
				feature_mask |= NETIF_F_TSO6;
		}

		if (arg & TUN_F_UFO)
			feature_mask |= NETIF_F_UFO;
	}

	/* tun/tap driver inverts the usage for TSO offloads, where
	 * setting the TSO bit means that the userspace wants to
	 * accept TSO frames and turning it off means that user space
	 * does not support TSO.
	 * For macvtap, we have to invert it to mean the same thing.
	 * When user space turns off TSO, we turn off GSO/LRO so that
	 * user-space will not receive TSO frames.
	 */
	if (feature_mask & (NETIF_F_TSO | NETIF_F_TSO6 | NETIF_F_UFO))
		features |= RX_OFFLOADS;
	else
		features &= ~RX_OFFLOADS;

	/* tap_features are the same as features on tun/tap and
	 * reflect user expectations.
	 */
	vlan->tap_features = feature_mask;
	vlan->set_features = features;
	netdev_update_features(vlan->dev);

	return 0;
}

/*
 * provide compatibility with generic tun/tap interface
 */
static long macvtap_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	struct macvtap_queue *q = file->private_data;
	struct macvlan_dev *vlan;
	void __user *argp = (void __user *)arg;
	struct ifreq __user *ifr = argp;
	unsigned int __user *up = argp;
	unsigned short u;
	int __user *sp = argp;
	struct sockaddr sa;
	int s;
	int ret;

	switch (cmd) {
	case TUNSETIFF:
		/* ignore the name, just look at flags */
		if (get_user(u, &ifr->ifr_flags))
			return -EFAULT;

		ret = 0;
		if ((u & ~MACVTAP_FEATURES) != (IFF_NO_PI | IFF_TAP))
			ret = -EINVAL;
		else
			q->flags = (q->flags & ~MACVTAP_FEATURES) | u;

		return ret;

	case TUNGETIFF:
		rtnl_lock();
		vlan = macvtap_get_vlan(q);
		if (!vlan) {
			rtnl_unlock();
			return -ENOLINK;
		}

		ret = 0;
		u = q->flags;
		if (copy_to_user(&ifr->ifr_name, vlan->dev->name, IFNAMSIZ) ||
		    put_user(u, &ifr->ifr_flags))
			ret = -EFAULT;
		macvtap_put_vlan(vlan);
		rtnl_unlock();
		return ret;

	case TUNSETQUEUE:
		if (get_user(u, &ifr->ifr_flags))
			return -EFAULT;
		rtnl_lock();
		ret = macvtap_ioctl_set_queue(file, u);
		rtnl_unlock();
		return ret;

	case TUNGETFEATURES:
		if (put_user(IFF_TAP | IFF_NO_PI | MACVTAP_FEATURES, up))
			return -EFAULT;
		return 0;

	case TUNSETSNDBUF:
		if (get_user(s, sp))
			return -EFAULT;
		if (s <= 0)
			return -EINVAL;

		q->sk.sk_sndbuf = s;
		return 0;

	case TUNGETVNETHDRSZ:
		s = q->vnet_hdr_sz;
		if (put_user(s, sp))
			return -EFAULT;
		return 0;

	case TUNSETVNETHDRSZ:
		if (get_user(s, sp))
			return -EFAULT;
		if (s < (int)sizeof(struct virtio_net_hdr))
			return -EINVAL;

		q->vnet_hdr_sz = s;
		return 0;

	case TUNGETVNETLE:
		s = !!(q->flags & MACVTAP_VNET_LE);
		if (put_user(s, sp))
			return -EFAULT;
		return 0;

	case TUNSETVNETLE:
		if (get_user(s, sp))
			return -EFAULT;
		if (s)
			q->flags |= MACVTAP_VNET_LE;
		else
			q->flags &= ~MACVTAP_VNET_LE;
		return 0;

	case TUNGETVNETBE:
		return macvtap_get_vnet_be(q, sp);

	case TUNSETVNETBE:
		return macvtap_set_vnet_be(q, sp);

	case TUNSETOFFLOAD:
		/* let the user check for future flags */
		if (arg & ~(TUN_F_CSUM | TUN_F_TSO4 | TUN_F_TSO6 |
			    TUN_F_TSO_ECN | TUN_F_UFO))
			return -EINVAL;

		rtnl_lock();
		ret = set_offload(q, arg);
		rtnl_unlock();
		return ret;

	case SIOCGIFHWADDR:
		rtnl_lock();
		vlan = macvtap_get_vlan(q);
		if (!vlan) {
			rtnl_unlock();
			return -ENOLINK;
		}
		ret = 0;
		u = vlan->dev->type;
		if (copy_to_user(&ifr->ifr_name, vlan->dev->name, IFNAMSIZ) ||
		    copy_to_user(&ifr->ifr_hwaddr.sa_data, vlan->dev->dev_addr, ETH_ALEN) ||
		    put_user(u, &ifr->ifr_hwaddr.sa_family))
			ret = -EFAULT;
		macvtap_put_vlan(vlan);
		rtnl_unlock();
		return ret;

	case SIOCSIFHWADDR:
		if (copy_from_user(&sa, &ifr->ifr_hwaddr, sizeof(sa)))
			return -EFAULT;
		rtnl_lock();
		vlan = macvtap_get_vlan(q);
		if (!vlan) {
			rtnl_unlock();
			return -ENOLINK;
		}
		ret = dev_set_mac_address(vlan->dev, &sa);
		macvtap_put_vlan(vlan);
		rtnl_unlock();
		return ret;

	default:
		return -EINVAL;
	}
}

#ifdef CONFIG_COMPAT
static long macvtap_compat_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	return macvtap_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static const struct file_operations macvtap_fops = {
	.owner		= THIS_MODULE,
	.open		= macvtap_open,
	.release	= macvtap_release,
	.read_iter	= macvtap_read_iter,
	.write_iter	= macvtap_write_iter,
	.poll		= macvtap_poll,
	.llseek		= no_llseek,
	.unlocked_ioctl	= macvtap_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= macvtap_compat_ioctl,
#endif
};

static int macvtap_sendmsg(struct socket *sock, struct msghdr *m,
			   size_t total_len)
{
	struct macvtap_queue *q = container_of(sock, struct macvtap_queue, sock);
	return macvtap_get_user(q, m, &m->msg_iter, m->msg_flags & MSG_DONTWAIT);
}

static int macvtap_recvmsg(struct socket *sock, struct msghdr *m,
			   size_t total_len, int flags)
{
	struct macvtap_queue *q = container_of(sock, struct macvtap_queue, sock);
	int ret;
	if (flags & ~(MSG_DONTWAIT|MSG_TRUNC))
		return -EINVAL;
	ret = macvtap_do_read(q, &m->msg_iter, flags & MSG_DONTWAIT);
	if (ret > total_len) {
		m->msg_flags |= MSG_TRUNC;
		ret = flags & MSG_TRUNC ? ret : total_len;
	}
	return ret;
}

/* Ops structure to mimic raw sockets with tun */
static const struct proto_ops macvtap_socket_ops = {
	.sendmsg = macvtap_sendmsg,
	.recvmsg = macvtap_recvmsg,
};

/* Get an underlying socket object from tun file.  Returns error unless file is
 * attached to a device.  The returned object works like a packet socket, it
 * can be used for sock_sendmsg/sock_recvmsg.  The caller is responsible for
 * holding a reference to the file for as long as the socket is in use. */
struct socket *macvtap_get_socket(struct file *file)
{
	struct macvtap_queue *q;
	if (file->f_op != &macvtap_fops)
		return ERR_PTR(-EINVAL);
	q = file->private_data;
	if (!q)
		return ERR_PTR(-EBADFD);
	return &q->sock;
}
EXPORT_SYMBOL_GPL(macvtap_get_socket);

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
static int macvtap_device_event(struct notifier_block *unused,
				unsigned long event, void *ptr)
{
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);
	struct macvtap_dev *vlantap;
	struct device *classdev;
	dev_t devt;
	int err;
	char tap_name[IFNAMSIZ];

	if (dev->rtnl_link_ops != &macvtap_link_ops)
		return NOTIFY_DONE;

	snprintf(tap_name, IFNAMSIZ, "tap%d", dev->ifindex);
	vlantap = netdev_priv(dev);

	switch (event) {
	case NETDEV_REGISTER:
		/* Create the device node here after the network device has
		 * been registered but before register_netdevice has
		 * finished running.
		 */
		err = tap_get_minor(macvtap_major, &vlantap->tap);
		if (err)
			return notifier_from_errno(err);

		devt = MKDEV(MAJOR(macvtap_major), vlantap->tap.minor);
		classdev = device_create(&macvtap_class, &dev->dev, devt,
					 dev, tap_name);
		if (IS_ERR(classdev)) {
			tap_free_minor(macvtap_major, &vlantap->tap);
			return notifier_from_errno(PTR_ERR(classdev));
		}
		err = sysfs_create_link(&dev->dev.kobj, &classdev->kobj,
					tap_name);
		if (err)
			return notifier_from_errno(err);
		break;
	case NETDEV_UNREGISTER:
		/* vlan->minor == 0 if NETDEV_REGISTER above failed */
		if (vlantap->tap.minor == 0)
			break;
		sysfs_remove_link(&dev->dev.kobj, tap_name);
		devt = MKDEV(MAJOR(macvtap_major), vlantap->tap.minor);
		device_destroy(&macvtap_class, devt);
		tap_free_minor(macvtap_major, &vlantap->tap);
		break;
	case NETDEV_CHANGE_TX_QUEUE_LEN:
		if (tap_queue_resize(&vlantap->tap))
			return NOTIFY_BAD;
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block macvtap_notifier_block __read_mostly = {
	.notifier_call	= macvtap_device_event,
};

static int macvtap_init(void)
{
	int err;

	err = tap_create_cdev(&macvtap_cdev, &macvtap_major, "macvtap",
			      THIS_MODULE);
	if (err)
		goto out1;

	err = class_register(&macvtap_class);
	if (err)
		goto out2;

	err = register_netdevice_notifier(&macvtap_notifier_block);
	if (err)
		goto out3;

	err = macvlan_link_register(&macvtap_link_ops);
	if (err)
		goto out4;

	return 0;

out4:
	unregister_netdevice_notifier(&macvtap_notifier_block);
out3:
	class_unregister(&macvtap_class);
out2:
	tap_destroy_cdev(macvtap_major, &macvtap_cdev);
out1:
	return err;
}
module_init(macvtap_init);

static void macvtap_exit(void)
{
	rtnl_link_unregister(&macvtap_link_ops);
	unregister_netdevice_notifier(&macvtap_notifier_block);
	class_unregister(&macvtap_class);
	tap_destroy_cdev(macvtap_major, &macvtap_cdev);
}
module_exit(macvtap_exit);

MODULE_ALIAS_RTNL_LINK("macvtap");
MODULE_AUTHOR("Arnd Bergmann <arnd@arndb.de>");
MODULE_LICENSE("GPL");
