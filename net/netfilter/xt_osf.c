// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2003+ Evgeniy Polyakov <zbr@ioremap.net>
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/capability.h>
#include <linux/if.h>
#include <linux/inetdevice.h>
#include <linux/ip.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/tcp.h>

#include <net/ip.h>
#include <net/tcp.h>

#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/x_tables.h>
#include <net/netfilter/nf_log.h>
#include <linux/netfilter/xt_osf.h>

<<<<<<< HEAD
=======
struct xt_osf_finger {
	struct rcu_head			rcu_head;
	struct list_head		finger_entry;
	struct xt_osf_user_finger	finger;
};

enum osf_fmatch_states {
	/* Packet does not match the fingerprint */
	FMATCH_WRONG = 0,
	/* Packet matches the fingerprint */
	FMATCH_OK,
	/* Options do not match the fingerprint, but header does */
	FMATCH_OPT_WRONG,
};

/*
 * Indexed by dont-fragment bit.
 * It is the only constant value in the fingerprint.
 */
static struct list_head xt_osf_fingers[2];

static const struct nla_policy xt_osf_policy[OSF_ATTR_MAX + 1] = {
	[OSF_ATTR_FINGER]	= { .len = sizeof(struct xt_osf_user_finger) },
};

static int xt_osf_add_callback(struct sock *ctnl, struct sk_buff *skb,
			       const struct nlmsghdr *nlh,
			       const struct nlattr * const osf_attrs[])
{
	struct xt_osf_user_finger *f;
	struct xt_osf_finger *kf = NULL, *sf;
	int err = 0;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!osf_attrs[OSF_ATTR_FINGER])
		return -EINVAL;

	if (!(nlh->nlmsg_flags & NLM_F_CREATE))
		return -EINVAL;

	f = nla_data(osf_attrs[OSF_ATTR_FINGER]);

	kf = kmalloc(sizeof(struct xt_osf_finger), GFP_KERNEL);
	if (!kf)
		return -ENOMEM;

	memcpy(&kf->finger, f, sizeof(struct xt_osf_user_finger));

	list_for_each_entry(sf, &xt_osf_fingers[!!f->df], finger_entry) {
		if (memcmp(&sf->finger, f, sizeof(struct xt_osf_user_finger)))
			continue;

		kfree(kf);
		kf = NULL;

		if (nlh->nlmsg_flags & NLM_F_EXCL)
			err = -EEXIST;
		break;
	}

	/*
	 * We are protected by nfnl mutex.
	 */
	if (kf)
		list_add_tail_rcu(&kf->finger_entry, &xt_osf_fingers[!!f->df]);

	return err;
}

static int xt_osf_remove_callback(struct sock *ctnl, struct sk_buff *skb,
				  const struct nlmsghdr *nlh,
				  const struct nlattr * const osf_attrs[])
{
	struct xt_osf_user_finger *f;
	struct xt_osf_finger *sf;
	int err = -ENOENT;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!osf_attrs[OSF_ATTR_FINGER])
		return -EINVAL;

	f = nla_data(osf_attrs[OSF_ATTR_FINGER]);

	list_for_each_entry(sf, &xt_osf_fingers[!!f->df], finger_entry) {
		if (memcmp(&sf->finger, f, sizeof(struct xt_osf_user_finger)))
			continue;

		/*
		 * We are protected by nfnl mutex.
		 */
		list_del_rcu(&sf->finger_entry);
		kfree_rcu(sf, rcu_head);

		err = 0;
		break;
	}

	return err;
}

static const struct nfnl_callback xt_osf_nfnetlink_callbacks[OSF_MSG_MAX] = {
	[OSF_MSG_ADD]	= {
		.call		= xt_osf_add_callback,
		.attr_count	= OSF_ATTR_MAX,
		.policy		= xt_osf_policy,
	},
	[OSF_MSG_REMOVE]	= {
		.call		= xt_osf_remove_callback,
		.attr_count	= OSF_ATTR_MAX,
		.policy		= xt_osf_policy,
	},
};

static const struct nfnetlink_subsystem xt_osf_nfnetlink = {
	.name			= "osf",
	.subsys_id		= NFNL_SUBSYS_OSF,
	.cb_count		= OSF_MSG_MAX,
	.cb			= xt_osf_nfnetlink_callbacks,
};

static inline int xt_osf_ttl(const struct sk_buff *skb, const struct xt_osf_info *info,
			    unsigned char f_ttl)
{
	const struct iphdr *ip = ip_hdr(skb);

	if (info->flags & XT_OSF_TTL) {
		if (info->ttl == XT_OSF_TTL_TRUE)
			return ip->ttl == f_ttl;
		if (info->ttl == XT_OSF_TTL_NOCHECK)
			return 1;
		else if (ip->ttl <= f_ttl)
			return 1;
		else {
			struct in_device *in_dev = __in_dev_get_rcu(skb->dev);
			int ret = 0;

			for_ifa(in_dev) {
				if (inet_ifa_match(ip->saddr, ifa)) {
					ret = (ip->ttl == f_ttl);
					break;
				}
			}
			endfor_ifa(in_dev);

			return ret;
		}
	}

	return ip->ttl == f_ttl;
}

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
static bool
xt_osf_match_packet(const struct sk_buff *skb, struct xt_action_param *p)
{
	return nf_osf_match(skb, xt_family(p), xt_hooknum(p), xt_in(p),
			    xt_out(p), p->matchinfo, xt_net(p), nf_osf_fingers);
}

static struct xt_match xt_osf_match = {
	.name 		= "osf",
	.revision	= 0,
	.family		= NFPROTO_IPV4,
	.proto		= IPPROTO_TCP,
	.hooks      	= (1 << NF_INET_LOCAL_IN) |
				(1 << NF_INET_PRE_ROUTING) |
				(1 << NF_INET_FORWARD),
	.match 		= xt_osf_match_packet,
	.matchsize	= sizeof(struct xt_osf_info),
	.me		= THIS_MODULE,
};

static int __init xt_osf_init(void)
{
	int err;

	err = xt_register_match(&xt_osf_match);
	if (err) {
		pr_err("Failed to register OS fingerprint "
		       "matching module (%d)\n", err);
		return err;
	}

	return 0;
}

static void __exit xt_osf_fini(void)
{
	xt_unregister_match(&xt_osf_match);
}

module_init(xt_osf_init);
module_exit(xt_osf_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Evgeniy Polyakov <zbr@ioremap.net>");
MODULE_DESCRIPTION("Passive OS fingerprint matching.");
MODULE_ALIAS("ipt_osf");
MODULE_ALIAS("ip6t_osf");
MODULE_ALIAS_NFNL_SUBSYS(NFNL_SUBSYS_OSF);
