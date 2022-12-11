// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * net/switchdev/switchdev.c - Switch device API
 * Copyright (c) 2014-2015 Jiri Pirko <jiri@resnulli.us>
 * Copyright (c) 2014-2015 Scott Feldman <sfeldma@gmail.com>
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_bridge.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/if_vlan.h>
#include <linux/rtnetlink.h>
<<<<<<< HEAD
=======
#include <net/ip_fib.h>
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#include <net/switchdev.h>

static LIST_HEAD(deferred);
static DEFINE_SPINLOCK(deferred_lock);

typedef void switchdev_deferred_func_t(struct net_device *dev,
				       const void *data);

struct switchdev_deferred_item {
	struct list_head list;
	struct net_device *dev;
	switchdev_deferred_func_t *func;
	unsigned long data[0];
};

static struct switchdev_deferred_item *switchdev_deferred_dequeue(void)
{
	struct switchdev_deferred_item *dfitem;

	spin_lock_bh(&deferred_lock);
	if (list_empty(&deferred)) {
		dfitem = NULL;
		goto unlock;
	}
	dfitem = list_first_entry(&deferred,
				  struct switchdev_deferred_item, list);
	list_del(&dfitem->list);
unlock:
	spin_unlock_bh(&deferred_lock);
	return dfitem;
}

/**
 *	switchdev_deferred_process - Process ops in deferred queue
 *
 *	Called to flush the ops currently queued in deferred ops queue.
 *	rtnl_lock must be held.
 */
void switchdev_deferred_process(void)
{
	struct switchdev_deferred_item *dfitem;

	ASSERT_RTNL();

	while ((dfitem = switchdev_deferred_dequeue())) {
		dfitem->func(dfitem->dev, dfitem->data);
		dev_put(dfitem->dev);
		kfree(dfitem);
	}
}
EXPORT_SYMBOL_GPL(switchdev_deferred_process);

static void switchdev_deferred_process_work(struct work_struct *work)
{
	rtnl_lock();
	switchdev_deferred_process();
	rtnl_unlock();
}

static DECLARE_WORK(deferred_process_work, switchdev_deferred_process_work);

static int switchdev_deferred_enqueue(struct net_device *dev,
				      const void *data, size_t data_len,
				      switchdev_deferred_func_t *func)
{
	struct switchdev_deferred_item *dfitem;

	dfitem = kmalloc(sizeof(*dfitem) + data_len, GFP_ATOMIC);
	if (!dfitem)
		return -ENOMEM;
	dfitem->dev = dev;
	dfitem->func = func;
	memcpy(dfitem->data, data, data_len);
	dev_hold(dev);
	spin_lock_bh(&deferred_lock);
	list_add_tail(&dfitem->list, &deferred);
	spin_unlock_bh(&deferred_lock);
	schedule_work(&deferred_process_work);
	return 0;
}

static int switchdev_port_attr_notify(enum switchdev_notifier_type nt,
				      struct net_device *dev,
				      const struct switchdev_attr *attr,
				      struct switchdev_trans *trans)
{
	int err;
	int rc;

	struct switchdev_notifier_port_attr_info attr_info = {
		.attr = attr,
		.trans = trans,
		.handled = false,
	};

	rc = call_switchdev_blocking_notifiers(nt, dev,
					       &attr_info.info, NULL);
	err = notifier_to_errno(rc);
	if (err) {
		WARN_ON(!attr_info.handled);
		return err;
	}

	if (!attr_info.handled)
		return -EOPNOTSUPP;

	return 0;
}

static int switchdev_port_attr_set_now(struct net_device *dev,
				       const struct switchdev_attr *attr)
{
	struct switchdev_trans trans;
	int err;

	/* Phase I: prepare for attr set. Driver/device should fail
	 * here if there are going to be issues in the commit phase,
	 * such as lack of resources or support.  The driver/device
	 * should reserve resources needed for the commit phase here,
	 * but should not commit the attr.
	 */

	trans.ph_prepare = true;
	err = switchdev_port_attr_notify(SWITCHDEV_PORT_ATTR_SET, dev, attr,
					 &trans);
	if (err)
		return err;

	/* Phase II: commit attr set.  This cannot fail as a fault
	 * of driver/device.  If it does, it's a bug in the driver/device
	 * because the driver said everythings was OK in phase I.
	 */

	trans.ph_prepare = false;
	err = switchdev_port_attr_notify(SWITCHDEV_PORT_ATTR_SET, dev, attr,
					 &trans);
	WARN(err, "%s: Commit of attribute (id=%d) failed.\n",
	     dev->name, attr->id);

	return err;
}

static void switchdev_port_attr_set_deferred(struct net_device *dev,
					     const void *data)
{
	const struct switchdev_attr *attr = data;
	int err;

	err = switchdev_port_attr_set_now(dev, attr);
	if (err && err != -EOPNOTSUPP)
		netdev_err(dev, "failed (err=%d) to set attribute (id=%d)\n",
			   err, attr->id);
	if (attr->complete)
		attr->complete(dev, err, attr->complete_priv);
}

static int switchdev_port_attr_set_defer(struct net_device *dev,
					 const struct switchdev_attr *attr)
{
	return switchdev_deferred_enqueue(dev, attr, sizeof(*attr),
					  switchdev_port_attr_set_deferred);
}

/**
 *	switchdev_port_attr_set - Set port attribute
 *
 *	@dev: port device
 *	@attr: attribute to set
 *
 *	Use a 2-phase prepare-commit transaction model to ensure
 *	system is not left in a partially updated state due to
 *	failure from driver/device.
 *
 *	rtnl_lock must be held and must not be in atomic section,
 *	in case SWITCHDEV_F_DEFER flag is not set.
 */
int switchdev_port_attr_set(struct net_device *dev,
			    const struct switchdev_attr *attr)
{
	if (attr->flags & SWITCHDEV_F_DEFER)
		return switchdev_port_attr_set_defer(dev, attr);
	ASSERT_RTNL();
	return switchdev_port_attr_set_now(dev, attr);
}
EXPORT_SYMBOL_GPL(switchdev_port_attr_set);

static size_t switchdev_obj_size(const struct switchdev_obj *obj)
{
	switch (obj->id) {
	case SWITCHDEV_OBJ_ID_PORT_VLAN:
		return sizeof(struct switchdev_obj_port_vlan);
	case SWITCHDEV_OBJ_ID_PORT_MDB:
		return sizeof(struct switchdev_obj_port_mdb);
	case SWITCHDEV_OBJ_ID_HOST_MDB:
		return sizeof(struct switchdev_obj_port_mdb);
	default:
		BUG();
	}
	return 0;
}

static int switchdev_port_obj_notify(enum switchdev_notifier_type nt,
				     struct net_device *dev,
				     const struct switchdev_obj *obj,
				     struct switchdev_trans *trans,
				     struct netlink_ext_ack *extack)
{
	int rc;
	int err;

	struct switchdev_notifier_port_obj_info obj_info = {
		.obj = obj,
		.trans = trans,
		.handled = false,
	};

	rc = call_switchdev_blocking_notifiers(nt, dev, &obj_info.info, extack);
	err = notifier_to_errno(rc);
	if (err) {
		WARN_ON(!obj_info.handled);
		return err;
	}
	if (!obj_info.handled)
		return -EOPNOTSUPP;
	return 0;
}

static int switchdev_port_obj_add_now(struct net_device *dev,
				      const struct switchdev_obj *obj,
				      struct netlink_ext_ack *extack)
{
	struct switchdev_trans trans;
	int err;

	ASSERT_RTNL();

	/* Phase I: prepare for obj add. Driver/device should fail
	 * here if there are going to be issues in the commit phase,
	 * such as lack of resources or support.  The driver/device
	 * should reserve resources needed for the commit phase here,
	 * but should not commit the obj.
	 */

	trans.ph_prepare = true;
	err = switchdev_port_obj_notify(SWITCHDEV_PORT_OBJ_ADD,
					dev, obj, &trans, extack);
	if (err)
		return err;

	/* Phase II: commit obj add.  This cannot fail as a fault
	 * of driver/device.  If it does, it's a bug in the driver/device
	 * because the driver said everythings was OK in phase I.
	 */

	trans.ph_prepare = false;
	err = switchdev_port_obj_notify(SWITCHDEV_PORT_OBJ_ADD,
					dev, obj, &trans, extack);
	WARN(err, "%s: Commit of object (id=%d) failed.\n", dev->name, obj->id);

	return err;
}

static void switchdev_port_obj_add_deferred(struct net_device *dev,
					    const void *data)
{
	const struct switchdev_obj *obj = data;
	int err;

	err = switchdev_port_obj_add_now(dev, obj, NULL);
	if (err && err != -EOPNOTSUPP)
		netdev_err(dev, "failed (err=%d) to add object (id=%d)\n",
			   err, obj->id);
	if (obj->complete)
		obj->complete(dev, err, obj->complete_priv);
}

static int switchdev_port_obj_add_defer(struct net_device *dev,
					const struct switchdev_obj *obj)
{
	return switchdev_deferred_enqueue(dev, obj, switchdev_obj_size(obj),
					  switchdev_port_obj_add_deferred);
}

/**
 *	switchdev_port_obj_add - Add port object
 *
 *	@dev: port device
 *	@id: object ID
 *	@obj: object to add
 *
 *	Use a 2-phase prepare-commit transaction model to ensure
 *	system is not left in a partially updated state due to
 *	failure from driver/device.
 *
 *	rtnl_lock must be held and must not be in atomic section,
 *	in case SWITCHDEV_F_DEFER flag is not set.
 */
int switchdev_port_obj_add(struct net_device *dev,
			   const struct switchdev_obj *obj,
			   struct netlink_ext_ack *extack)
{
	if (obj->flags & SWITCHDEV_F_DEFER)
		return switchdev_port_obj_add_defer(dev, obj);
	ASSERT_RTNL();
	return switchdev_port_obj_add_now(dev, obj, extack);
}
EXPORT_SYMBOL_GPL(switchdev_port_obj_add);

static int switchdev_port_obj_del_now(struct net_device *dev,
				      const struct switchdev_obj *obj)
{
	return switchdev_port_obj_notify(SWITCHDEV_PORT_OBJ_DEL,
					 dev, obj, NULL, NULL);
}

static void switchdev_port_obj_del_deferred(struct net_device *dev,
					    const void *data)
{
	const struct switchdev_obj *obj = data;
	int err;

	err = switchdev_port_obj_del_now(dev, obj);
	if (err && err != -EOPNOTSUPP)
		netdev_err(dev, "failed (err=%d) to del object (id=%d)\n",
			   err, obj->id);
	if (obj->complete)
		obj->complete(dev, err, obj->complete_priv);
}

static int switchdev_port_obj_del_defer(struct net_device *dev,
					const struct switchdev_obj *obj)
{
	return switchdev_deferred_enqueue(dev, obj, switchdev_obj_size(obj),
					  switchdev_port_obj_del_deferred);
}

/**
 *	switchdev_port_obj_del - Delete port object
 *
 *	@dev: port device
 *	@id: object ID
 *	@obj: object to delete
 *
 *	rtnl_lock must be held and must not be in atomic section,
 *	in case SWITCHDEV_F_DEFER flag is not set.
 */
int switchdev_port_obj_del(struct net_device *dev,
			   const struct switchdev_obj *obj)
{
	if (obj->flags & SWITCHDEV_F_DEFER)
		return switchdev_port_obj_del_defer(dev, obj);
	ASSERT_RTNL();
	return switchdev_port_obj_del_now(dev, obj);
}
EXPORT_SYMBOL_GPL(switchdev_port_obj_del);

<<<<<<< HEAD
static ATOMIC_NOTIFIER_HEAD(switchdev_notif_chain);
static BLOCKING_NOTIFIER_HEAD(switchdev_blocking_notif_chain);
=======
/**
 *	switchdev_port_obj_dump - Dump port objects
 *
 *	@dev: port device
 *	@id: object ID
 *	@obj: object to dump
 *	@cb: function to call with a filled object
 *
 *	rtnl_lock must be held.
 */
int switchdev_port_obj_dump(struct net_device *dev, struct switchdev_obj *obj,
			    switchdev_obj_dump_cb_t *cb)
{
	const struct switchdev_ops *ops = dev->switchdev_ops;
	struct net_device *lower_dev;
	struct list_head *iter;
	int err = -EOPNOTSUPP;

	ASSERT_RTNL();

	if (ops && ops->switchdev_port_obj_dump)
		return ops->switchdev_port_obj_dump(dev, obj, cb);

	/* Switch device port(s) may be stacked under
	 * bond/team/vlan dev, so recurse down to dump objects on
	 * first port at bottom of stack.
	 */

	netdev_for_each_lower_dev(dev, lower_dev, iter) {
		err = switchdev_port_obj_dump(lower_dev, obj, cb);
		break;
	}

	return err;
}
EXPORT_SYMBOL_GPL(switchdev_port_obj_dump);

static RAW_NOTIFIER_HEAD(switchdev_notif_chain);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

/**
 *	register_switchdev_notifier - Register notifier
 *	@nb: notifier_block
 *
 *	Register switch device notifier.
 */
int register_switchdev_notifier(struct notifier_block *nb)
{
<<<<<<< HEAD
	return atomic_notifier_chain_register(&switchdev_notif_chain, nb);
=======
	int err;

	rtnl_lock();
	err = raw_notifier_chain_register(&switchdev_notif_chain, nb);
	rtnl_unlock();
	return err;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}
EXPORT_SYMBOL_GPL(register_switchdev_notifier);

/**
 *	unregister_switchdev_notifier - Unregister notifier
 *	@nb: notifier_block
 *
 *	Unregister switch device notifier.
 */
int unregister_switchdev_notifier(struct notifier_block *nb)
{
<<<<<<< HEAD
	return atomic_notifier_chain_unregister(&switchdev_notif_chain, nb);
=======
	int err;

	rtnl_lock();
	err = raw_notifier_chain_unregister(&switchdev_notif_chain, nb);
	rtnl_unlock();
	return err;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}
EXPORT_SYMBOL_GPL(unregister_switchdev_notifier);

/**
 *	call_switchdev_notifiers - Call notifiers
 *	@val: value passed unmodified to notifier function
 *	@dev: port device
 *	@info: notifier information data
 *
<<<<<<< HEAD
 *	Call all network notifier blocks.
=======
 *	Call all network notifier blocks. This should be called by driver
 *	when it needs to propagate hardware event.
 *	Return values are same as for atomic_notifier_call_chain().
 *	rtnl_lock must be held.
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
 */
int call_switchdev_notifiers(unsigned long val, struct net_device *dev,
			     struct switchdev_notifier_info *info,
			     struct netlink_ext_ack *extack)
{
<<<<<<< HEAD
	info->dev = dev;
	info->extack = extack;
	return atomic_notifier_call_chain(&switchdev_notif_chain, val, info);
=======
	int err;

	ASSERT_RTNL();

	info->dev = dev;
	err = raw_notifier_call_chain(&switchdev_notif_chain, val, info);
	return err;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}
EXPORT_SYMBOL_GPL(call_switchdev_notifiers);

int register_switchdev_blocking_notifier(struct notifier_block *nb)
{
	struct blocking_notifier_head *chain = &switchdev_blocking_notif_chain;

	return blocking_notifier_chain_register(chain, nb);
}
EXPORT_SYMBOL_GPL(register_switchdev_blocking_notifier);

int unregister_switchdev_blocking_notifier(struct notifier_block *nb)
{
	struct blocking_notifier_head *chain = &switchdev_blocking_notif_chain;

	return blocking_notifier_chain_unregister(chain, nb);
}
EXPORT_SYMBOL_GPL(unregister_switchdev_blocking_notifier);

int call_switchdev_blocking_notifiers(unsigned long val, struct net_device *dev,
				      struct switchdev_notifier_info *info,
				      struct netlink_ext_ack *extack)
{
	info->dev = dev;
	info->extack = extack;
	return blocking_notifier_call_chain(&switchdev_blocking_notif_chain,
					    val, info);
}
EXPORT_SYMBOL_GPL(call_switchdev_blocking_notifiers);

static int __switchdev_handle_port_obj_add(struct net_device *dev,
			struct switchdev_notifier_port_obj_info *port_obj_info,
			bool (*check_cb)(const struct net_device *dev),
			int (*add_cb)(struct net_device *dev,
				      const struct switchdev_obj *obj,
				      struct switchdev_trans *trans,
				      struct netlink_ext_ack *extack))
{
	struct netlink_ext_ack *extack;
	struct net_device *lower_dev;
	struct list_head *iter;
	int err = -EOPNOTSUPP;

	extack = switchdev_notifier_info_to_extack(&port_obj_info->info);

	if (check_cb(dev)) {
		err = add_cb(dev, port_obj_info->obj, port_obj_info->trans,
			     extack);
		if (err != -EOPNOTSUPP)
			port_obj_info->handled = true;
		return err;
	}

	/* Switch ports might be stacked under e.g. a LAG. Ignore the
	 * unsupported devices, another driver might be able to handle them. But
	 * propagate to the callers any hard errors.
	 *
	 * If the driver does its own bookkeeping of stacked ports, it's not
	 * necessary to go through this helper.
	 */
	netdev_for_each_lower_dev(dev, lower_dev, iter) {
		err = __switchdev_handle_port_obj_add(lower_dev, port_obj_info,
						      check_cb, add_cb);
		if (err && err != -EOPNOTSUPP)
			return err;
	}

	return err;
}

int switchdev_handle_port_obj_add(struct net_device *dev,
			struct switchdev_notifier_port_obj_info *port_obj_info,
			bool (*check_cb)(const struct net_device *dev),
			int (*add_cb)(struct net_device *dev,
				      const struct switchdev_obj *obj,
				      struct switchdev_trans *trans,
				      struct netlink_ext_ack *extack))
{
	int err;

	err = __switchdev_handle_port_obj_add(dev, port_obj_info, check_cb,
					      add_cb);
	if (err == -EOPNOTSUPP)
		err = 0;
	return err;
}
EXPORT_SYMBOL_GPL(switchdev_handle_port_obj_add);

<<<<<<< HEAD
static int __switchdev_handle_port_obj_del(struct net_device *dev,
			struct switchdev_notifier_port_obj_info *port_obj_info,
			bool (*check_cb)(const struct net_device *dev),
			int (*del_cb)(struct net_device *dev,
				      const struct switchdev_obj *obj))
=======
/**
 *	switchdev_fib_ipv4_add - Add/modify switch IPv4 route entry
 *
 *	@dst: route's IPv4 destination address
 *	@dst_len: destination address length (prefix length)
 *	@fi: route FIB info structure
 *	@tos: route TOS
 *	@type: route type
 *	@nlflags: netlink flags passed in (NLM_F_*)
 *	@tb_id: route table ID
 *
 *	Add/modify switch IPv4 route entry.
 */
int switchdev_fib_ipv4_add(u32 dst, int dst_len, struct fib_info *fi,
			   u8 tos, u8 type, u32 nlflags, u32 tb_id)
{
	struct switchdev_obj_ipv4_fib ipv4_fib = {
		.obj.id = SWITCHDEV_OBJ_ID_IPV4_FIB,
		.dst = dst,
		.dst_len = dst_len,
		.fi = fi,
		.tos = tos,
		.type = type,
		.nlflags = nlflags,
		.tb_id = tb_id,
	};
	struct net_device *dev;
	int err = 0;

	/* Don't offload route if using custom ip rules or if
	 * IPv4 FIB offloading has been disabled completely.
	 */

#ifdef CONFIG_IP_MULTIPLE_TABLES
	if (fi->fib_net->ipv4.fib_has_custom_rules)
		return 0;
#endif

	if (fi->fib_net->ipv4.fib_offload_disabled)
		return 0;

	dev = switchdev_get_dev_by_nhs(fi);
	if (!dev)
		return 0;

	err = switchdev_port_obj_add(dev, &ipv4_fib.obj);
	if (!err)
		fi->fib_flags |= RTNH_F_OFFLOAD;

	return err == -EOPNOTSUPP ? 0 : err;
}
EXPORT_SYMBOL_GPL(switchdev_fib_ipv4_add);

/**
 *	switchdev_fib_ipv4_del - Delete IPv4 route entry from switch
 *
 *	@dst: route's IPv4 destination address
 *	@dst_len: destination address length (prefix length)
 *	@fi: route FIB info structure
 *	@tos: route TOS
 *	@type: route type
 *	@tb_id: route table ID
 *
 *	Delete IPv4 route entry from switch device.
 */
int switchdev_fib_ipv4_del(u32 dst, int dst_len, struct fib_info *fi,
			   u8 tos, u8 type, u32 tb_id)
{
	struct switchdev_obj_ipv4_fib ipv4_fib = {
		.obj.id = SWITCHDEV_OBJ_ID_IPV4_FIB,
		.dst = dst,
		.dst_len = dst_len,
		.fi = fi,
		.tos = tos,
		.type = type,
		.nlflags = 0,
		.tb_id = tb_id,
	};
	struct net_device *dev;
	int err = 0;

	if (!(fi->fib_flags & RTNH_F_OFFLOAD))
		return 0;

	dev = switchdev_get_dev_by_nhs(fi);
	if (!dev)
		return 0;

	err = switchdev_port_obj_del(dev, &ipv4_fib.obj);
	if (!err)
		fi->fib_flags &= ~RTNH_F_OFFLOAD;

	return err == -EOPNOTSUPP ? 0 : err;
}
EXPORT_SYMBOL_GPL(switchdev_fib_ipv4_del);

/**
 *	switchdev_fib_ipv4_abort - Abort an IPv4 FIB operation
 *
 *	@fi: route FIB info structure
 */
void switchdev_fib_ipv4_abort(struct fib_info *fi)
{
	/* There was a problem installing this route to the offload
	 * device.  For now, until we come up with more refined
	 * policy handling, abruptly end IPv4 fib offloading for
	 * for entire net by flushing offload device(s) of all
	 * IPv4 routes, and mark IPv4 fib offloading broken from
	 * this point forward.
	 */

	fib_flush_external(fi->fib_net);
	fi->fib_net->ipv4.fib_offload_disabled = true;
}
EXPORT_SYMBOL_GPL(switchdev_fib_ipv4_abort);

static bool switchdev_port_same_parent_id(struct net_device *a,
					  struct net_device *b)
{
	struct switchdev_attr a_attr = {
		.id = SWITCHDEV_ATTR_ID_PORT_PARENT_ID,
		.flags = SWITCHDEV_F_NO_RECURSE,
	};
	struct switchdev_attr b_attr = {
		.id = SWITCHDEV_ATTR_ID_PORT_PARENT_ID,
		.flags = SWITCHDEV_F_NO_RECURSE,
	};

	if (switchdev_port_attr_get(a, &a_attr) ||
	    switchdev_port_attr_get(b, &b_attr))
		return false;

	return netdev_phys_item_id_same(&a_attr.u.ppid, &b_attr.u.ppid);
}

static u32 switchdev_port_fwd_mark_get(struct net_device *dev,
				       struct net_device *group_dev)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
{
	struct net_device *lower_dev;
	struct list_head *iter;
	int err = -EOPNOTSUPP;

	if (check_cb(dev)) {
		err = del_cb(dev, port_obj_info->obj);
		if (err != -EOPNOTSUPP)
			port_obj_info->handled = true;
		return err;
	}

	/* Switch ports might be stacked under e.g. a LAG. Ignore the
	 * unsupported devices, another driver might be able to handle them. But
	 * propagate to the callers any hard errors.
	 *
	 * If the driver does its own bookkeeping of stacked ports, it's not
	 * necessary to go through this helper.
	 */
	netdev_for_each_lower_dev(dev, lower_dev, iter) {
		err = __switchdev_handle_port_obj_del(lower_dev, port_obj_info,
						      check_cb, del_cb);
		if (err && err != -EOPNOTSUPP)
			return err;
	}

	return err;
}

int switchdev_handle_port_obj_del(struct net_device *dev,
			struct switchdev_notifier_port_obj_info *port_obj_info,
			bool (*check_cb)(const struct net_device *dev),
			int (*del_cb)(struct net_device *dev,
				      const struct switchdev_obj *obj))
{
	int err;

	err = __switchdev_handle_port_obj_del(dev, port_obj_info, check_cb,
					      del_cb);
	if (err == -EOPNOTSUPP)
		err = 0;
	return err;
}
EXPORT_SYMBOL_GPL(switchdev_handle_port_obj_del);

static int __switchdev_handle_port_attr_set(struct net_device *dev,
			struct switchdev_notifier_port_attr_info *port_attr_info,
			bool (*check_cb)(const struct net_device *dev),
			int (*set_cb)(struct net_device *dev,
				      const struct switchdev_attr *attr,
				      struct switchdev_trans *trans))
{
	struct net_device *lower_dev;
	struct list_head *iter;
	int err = -EOPNOTSUPP;

	if (check_cb(dev)) {
		err = set_cb(dev, port_attr_info->attr, port_attr_info->trans);
		if (err != -EOPNOTSUPP)
			port_attr_info->handled = true;
		return err;
	}

	/* Switch ports might be stacked under e.g. a LAG. Ignore the
	 * unsupported devices, another driver might be able to handle them. But
	 * propagate to the callers any hard errors.
	 *
	 * If the driver does its own bookkeeping of stacked ports, it's not
	 * necessary to go through this helper.
	 */
	netdev_for_each_lower_dev(dev, lower_dev, iter) {
		err = __switchdev_handle_port_attr_set(lower_dev, port_attr_info,
						       check_cb, set_cb);
		if (err && err != -EOPNOTSUPP)
			return err;
	}

	return err;
}

int switchdev_handle_port_attr_set(struct net_device *dev,
			struct switchdev_notifier_port_attr_info *port_attr_info,
			bool (*check_cb)(const struct net_device *dev),
			int (*set_cb)(struct net_device *dev,
				      const struct switchdev_attr *attr,
				      struct switchdev_trans *trans))
{
	int err;

	err = __switchdev_handle_port_attr_set(dev, port_attr_info, check_cb,
					       set_cb);
	if (err == -EOPNOTSUPP)
		err = 0;
	return err;
}
EXPORT_SYMBOL_GPL(switchdev_handle_port_attr_set);
