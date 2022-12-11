// SPDX-License-Identifier: GPL-2.0
/*
 * bsg.c - block layer implementation of the sg v4 interface
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/blkdev.h>
#include <linux/cdev.h>
#include <linux/jiffies.h>
#include <linux/percpu.h>
#include <linux/idr.h>
#include <linux/bsg.h>
#include <linux/slab.h>

#include <scsi/scsi.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_driver.h>
#include <scsi/sg.h>

#define BSG_DESCRIPTION	"Block layer SCSI generic (bsg) driver"
#define BSG_VERSION	"0.4"

#define bsg_dbg(bd, fmt, ...) \
	pr_debug("%s: " fmt, (bd)->name, ##__VA_ARGS__)

struct bsg_device {
	struct request_queue *queue;
	spinlock_t lock;
	struct hlist_node dev_list;
	refcount_t ref_count;
	char name[20];
	int max_queue;
};

#define BSG_DEFAULT_CMDS	64
#define BSG_MAX_DEVS		32768

static DEFINE_MUTEX(bsg_mutex);
static DEFINE_IDR(bsg_minor_idr);

#define BSG_LIST_ARRAY_SIZE	8
static struct hlist_head bsg_device_list[BSG_LIST_ARRAY_SIZE];

static struct class *bsg_class;
static int bsg_major;

static inline struct hlist_head *bsg_dev_idx_hash(int index)
{
	return &bsg_device_list[index & (BSG_LIST_ARRAY_SIZE - 1)];
}

#define uptr64(val) ((void __user *)(uintptr_t)(val))

static int bsg_scsi_check_proto(struct sg_io_v4 *hdr)
{
	if (hdr->protocol != BSG_PROTOCOL_SCSI  ||
	    hdr->subprotocol != BSG_SUB_PROTOCOL_SCSI_CMD)
		return -EINVAL;
	return 0;
}

static int bsg_scsi_fill_hdr(struct request *rq, struct sg_io_v4 *hdr,
		fmode_t mode)
{
	struct scsi_request *sreq = scsi_req(rq);

	if (hdr->dout_xfer_len && hdr->din_xfer_len) {
		pr_warn_once("BIDI support in bsg has been removed.\n");
		return -EOPNOTSUPP;
	}

	sreq->cmd_len = hdr->request_len;
	if (sreq->cmd_len > BLK_MAX_CDB) {
		sreq->cmd = kzalloc(sreq->cmd_len, GFP_KERNEL);
		if (!sreq->cmd)
			return -ENOMEM;
	}

	if (copy_from_user(sreq->cmd, uptr64(hdr->request), sreq->cmd_len))
		return -EFAULT;
	if (blk_verify_command(sreq->cmd, mode))
		return -EPERM;
	return 0;
}

static int bsg_scsi_complete_rq(struct request *rq, struct sg_io_v4 *hdr)
{
	struct scsi_request *sreq = scsi_req(rq);
	int ret = 0;

	/*
	 * fill in all the output members
	 */
	hdr->device_status = sreq->result & 0xff;
	hdr->transport_status = host_byte(sreq->result);
	hdr->driver_status = driver_byte(sreq->result);
	hdr->info = 0;
	if (hdr->device_status || hdr->transport_status || hdr->driver_status)
		hdr->info |= SG_INFO_CHECK;
	hdr->response_len = 0;

	if (sreq->sense_len && hdr->response) {
		int len = min_t(unsigned int, hdr->max_response_len,
					sreq->sense_len);

		if (copy_to_user(uptr64(hdr->response), sreq->sense, len))
			ret = -EFAULT;
		else
			hdr->response_len = len;
	}

	if (rq_data_dir(rq) == READ)
		hdr->din_resid = sreq->resid_len;
	else
		hdr->dout_resid = sreq->resid_len;

	return ret;
}

static void bsg_scsi_free_rq(struct request *rq)
{
	scsi_req_free_cmd(scsi_req(rq));
}

static const struct bsg_ops bsg_scsi_ops = {
	.check_proto		= bsg_scsi_check_proto,
	.fill_hdr		= bsg_scsi_fill_hdr,
	.complete_rq		= bsg_scsi_complete_rq,
	.free_rq		= bsg_scsi_free_rq,
};

static int bsg_sg_io(struct request_queue *q, fmode_t mode, void __user *uarg)
{
	struct request *rq;
	struct bio *bio;
	struct sg_io_v4 hdr;
	int ret;

	if (copy_from_user(&hdr, uarg, sizeof(hdr)))
		return -EFAULT;

	if (!q->bsg_dev.class_dev)
		return -ENXIO;

	if (hdr.guard != 'Q')
		return -EINVAL;
	ret = q->bsg_dev.ops->check_proto(&hdr);
	if (ret)
		return ret;

	rq = blk_get_request(q, hdr.dout_xfer_len ?
			REQ_OP_SCSI_OUT : REQ_OP_SCSI_IN, 0);
	if (IS_ERR(rq))
		return PTR_ERR(rq);

	ret = q->bsg_dev.ops->fill_hdr(rq, &hdr, mode);
	if (ret) {
		blk_put_request(rq);
		return ret;
	}

	rq->timeout = msecs_to_jiffies(hdr.timeout);
	if (!rq->timeout)
		rq->timeout = q->sg_timeout;
	if (!rq->timeout)
		rq->timeout = BLK_DEFAULT_SG_TIMEOUT;
	if (rq->timeout < BLK_MIN_SG_TIMEOUT)
		rq->timeout = BLK_MIN_SG_TIMEOUT;

	if (hdr.dout_xfer_len) {
		ret = blk_rq_map_user(q, rq, NULL, uptr64(hdr.dout_xferp),
				hdr.dout_xfer_len, GFP_KERNEL);
	} else if (hdr.din_xfer_len) {
		ret = blk_rq_map_user(q, rq, NULL, uptr64(hdr.din_xferp),
				hdr.din_xfer_len, GFP_KERNEL);
	}

	if (ret)
		goto out_free_rq;

	bio = rq->bio;

	blk_execute_rq(q, NULL, rq, !(hdr.flags & BSG_FLAG_Q_AT_TAIL));
	ret = rq->q->bsg_dev.ops->complete_rq(rq, &hdr);
	blk_rq_unmap_user(bio);

out_free_rq:
	rq->q->bsg_dev.ops->free_rq(rq);
	blk_put_request(rq);
	if (!ret && copy_to_user(uarg, &hdr, sizeof(hdr)))
		return -EFAULT;
	return ret;
}

<<<<<<< HEAD
=======
static bool bsg_complete(struct bsg_device *bd)
{
	bool ret = false;
	bool spin;

	do {
		spin_lock_irq(&bd->lock);

		BUG_ON(bd->done_cmds > bd->queued_cmds);

		/*
		 * All commands consumed.
		 */
		if (bd->done_cmds == bd->queued_cmds)
			ret = true;

		spin = !test_bit(BSG_F_BLOCK, &bd->flags);

		spin_unlock_irq(&bd->lock);
	} while (!ret && spin);

	return ret;
}

static int bsg_complete_all_commands(struct bsg_device *bd)
{
	struct bsg_command *bc;
	int ret, tret;

	dprintk("%s: entered\n", bd->name);

	/*
	 * wait for all commands to complete
	 */
	io_wait_event(bd->wq_done, bsg_complete(bd));

	/*
	 * discard done commands
	 */
	ret = 0;
	do {
		spin_lock_irq(&bd->lock);
		if (!bd->queued_cmds) {
			spin_unlock_irq(&bd->lock);
			break;
		}
		spin_unlock_irq(&bd->lock);

		bc = bsg_get_done_cmd(bd);
		if (IS_ERR(bc))
			break;

		tret = blk_complete_sgv4_hdr_rq(bc->rq, &bc->hdr, bc->bio,
						bc->bidi_bio);
		if (!ret)
			ret = tret;

		bsg_free_command(bc);
	} while (1);

	return ret;
}

static int
__bsg_read(char __user *buf, size_t count, struct bsg_device *bd,
	   const struct iovec *iov, ssize_t *bytes_read)
{
	struct bsg_command *bc;
	int nr_commands, ret;

	if (count % sizeof(struct sg_io_v4))
		return -EINVAL;

	ret = 0;
	nr_commands = count / sizeof(struct sg_io_v4);
	while (nr_commands) {
		bc = bsg_get_done_cmd(bd);
		if (IS_ERR(bc)) {
			ret = PTR_ERR(bc);
			break;
		}

		/*
		 * this is the only case where we need to copy data back
		 * after completing the request. so do that here,
		 * bsg_complete_work() cannot do that for us
		 */
		ret = blk_complete_sgv4_hdr_rq(bc->rq, &bc->hdr, bc->bio,
					       bc->bidi_bio);

		if (copy_to_user(buf, &bc->hdr, sizeof(bc->hdr)))
			ret = -EFAULT;

		bsg_free_command(bc);

		if (ret)
			break;

		buf += sizeof(struct sg_io_v4);
		*bytes_read += sizeof(struct sg_io_v4);
		nr_commands--;
	}

	return ret;
}

static inline void bsg_set_block(struct bsg_device *bd, struct file *file)
{
	if (file->f_flags & O_NONBLOCK)
		clear_bit(BSG_F_BLOCK, &bd->flags);
	else
		set_bit(BSG_F_BLOCK, &bd->flags);
}

/*
 * Check if the error is a "real" error that we should return.
 */
static inline int err_block_err(int ret)
{
	if (ret && ret != -ENOSPC && ret != -ENODATA && ret != -EAGAIN)
		return 1;

	return 0;
}

static ssize_t
bsg_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct bsg_device *bd = file->private_data;
	int ret;
	ssize_t bytes_read;

	dprintk("%s: read %Zd bytes\n", bd->name, count);

	bsg_set_block(bd, file);

	bytes_read = 0;
	ret = __bsg_read(buf, count, bd, NULL, &bytes_read);
	*ppos = bytes_read;

	if (!bytes_read || err_block_err(ret))
		bytes_read = ret;

	return bytes_read;
}

static int __bsg_write(struct bsg_device *bd, const char __user *buf,
		       size_t count, ssize_t *bytes_written,
		       fmode_t has_write_perm)
{
	struct bsg_command *bc;
	struct request *rq;
	int ret, nr_commands;

	if (count % sizeof(struct sg_io_v4))
		return -EINVAL;

	nr_commands = count / sizeof(struct sg_io_v4);
	rq = NULL;
	bc = NULL;
	ret = 0;
	while (nr_commands) {
		struct request_queue *q = bd->queue;

		bc = bsg_alloc_command(bd);
		if (IS_ERR(bc)) {
			ret = PTR_ERR(bc);
			bc = NULL;
			break;
		}

		if (copy_from_user(&bc->hdr, buf, sizeof(bc->hdr))) {
			ret = -EFAULT;
			break;
		}

		/*
		 * get a request, fill in the blanks, and add to request queue
		 */
		rq = bsg_map_hdr(bd, &bc->hdr, has_write_perm, bc->sense);
		if (IS_ERR(rq)) {
			ret = PTR_ERR(rq);
			rq = NULL;
			break;
		}

		bsg_add_command(bd, q, bc, rq);
		bc = NULL;
		rq = NULL;
		nr_commands--;
		buf += sizeof(struct sg_io_v4);
		*bytes_written += sizeof(struct sg_io_v4);
	}

	if (bc)
		bsg_free_command(bc);

	return ret;
}

static ssize_t
bsg_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct bsg_device *bd = file->private_data;
	ssize_t bytes_written;
	int ret;

	dprintk("%s: write %Zd bytes\n", bd->name, count);

	if (unlikely(segment_eq(get_fs(), KERNEL_DS)))
		return -EINVAL;

	bsg_set_block(bd, file);

	bytes_written = 0;
	ret = __bsg_write(bd, buf, count, &bytes_written,
			  file->f_mode & FMODE_WRITE);

	*ppos = bytes_written;

	/*
	 * return bytes written on non-fatal errors
	 */
	if (!bytes_written || err_block_err(ret))
		bytes_written = ret;

	dprintk("%s: returning %Zd\n", bd->name, bytes_written);
	return bytes_written;
}

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
static struct bsg_device *bsg_alloc_device(void)
{
	struct bsg_device *bd;

	bd = kzalloc(sizeof(struct bsg_device), GFP_KERNEL);
	if (unlikely(!bd))
		return NULL;

	spin_lock_init(&bd->lock);
	bd->max_queue = BSG_DEFAULT_CMDS;
	INIT_HLIST_NODE(&bd->dev_list);
	return bd;
}

static int bsg_put_device(struct bsg_device *bd)
{
	struct request_queue *q = bd->queue;

	mutex_lock(&bsg_mutex);

	if (!refcount_dec_and_test(&bd->ref_count)) {
		mutex_unlock(&bsg_mutex);
		return 0;
	}

	hlist_del(&bd->dev_list);
	mutex_unlock(&bsg_mutex);

	bsg_dbg(bd, "tearing down\n");

	/*
	 * close can always block
	 */
	kfree(bd);
	blk_put_queue(q);
	return 0;
}

static struct bsg_device *bsg_add_device(struct inode *inode,
					 struct request_queue *rq,
					 struct file *file)
{
	struct bsg_device *bd;
	unsigned char buf[32];

	lockdep_assert_held(&bsg_mutex);

	if (!blk_get_queue(rq))
		return ERR_PTR(-ENXIO);

	bd = bsg_alloc_device();
	if (!bd) {
		blk_put_queue(rq);
		return ERR_PTR(-ENOMEM);
	}

	bd->queue = rq;

	refcount_set(&bd->ref_count, 1);
	hlist_add_head(&bd->dev_list, bsg_dev_idx_hash(iminor(inode)));

	strncpy(bd->name, dev_name(rq->bsg_dev.class_dev), sizeof(bd->name) - 1);
	bsg_dbg(bd, "bound to <%s>, max queue %d\n",
		format_dev_t(buf, inode->i_rdev), bd->max_queue);

	return bd;
}

static struct bsg_device *__bsg_get_device(int minor, struct request_queue *q)
{
	struct bsg_device *bd;

	lockdep_assert_held(&bsg_mutex);

	hlist_for_each_entry(bd, bsg_dev_idx_hash(minor), dev_list) {
		if (bd->queue == q) {
			refcount_inc(&bd->ref_count);
			goto found;
		}
	}
	bd = NULL;
found:
	return bd;
}

static struct bsg_device *bsg_get_device(struct inode *inode, struct file *file)
{
	struct bsg_device *bd;
	struct bsg_class_device *bcd;

	/*
	 * find the class device
	 */
	mutex_lock(&bsg_mutex);
	bcd = idr_find(&bsg_minor_idr, iminor(inode));

	if (!bcd) {
		bd = ERR_PTR(-ENODEV);
		goto out_unlock;
	}

	bd = __bsg_get_device(iminor(inode), bcd->queue);
	if (!bd)
		bd = bsg_add_device(inode, bcd->queue, file);

out_unlock:
	mutex_unlock(&bsg_mutex);
	return bd;
}

static int bsg_open(struct inode *inode, struct file *file)
{
	struct bsg_device *bd;

	bd = bsg_get_device(inode, file);

	if (IS_ERR(bd))
		return PTR_ERR(bd);

	file->private_data = bd;
	return 0;
}

static int bsg_release(struct inode *inode, struct file *file)
{
	struct bsg_device *bd = file->private_data;

	file->private_data = NULL;
	return bsg_put_device(bd);
}

static int bsg_get_command_q(struct bsg_device *bd, int __user *uarg)
{
	return put_user(bd->max_queue, uarg);
}

static int bsg_set_command_q(struct bsg_device *bd, int __user *uarg)
{
	int queue;

	if (get_user(queue, uarg))
		return -EFAULT;
	if (queue < 1)
		return -EINVAL;

	spin_lock_irq(&bd->lock);
	bd->max_queue = queue;
	spin_unlock_irq(&bd->lock);
	return 0;
}

static long bsg_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct bsg_device *bd = file->private_data;
	void __user *uarg = (void __user *) arg;

	switch (cmd) {
	/*
	 * Our own ioctls
	 */
	case SG_GET_COMMAND_Q:
		return bsg_get_command_q(bd, uarg);
	case SG_SET_COMMAND_Q:
		return bsg_set_command_q(bd, uarg);

	/*
	 * SCSI/sg ioctls
	 */
	case SG_GET_VERSION_NUM:
	case SCSI_IOCTL_GET_IDLUN:
	case SCSI_IOCTL_GET_BUS_NUMBER:
	case SG_SET_TIMEOUT:
	case SG_GET_TIMEOUT:
	case SG_GET_RESERVED_SIZE:
	case SG_SET_RESERVED_SIZE:
	case SG_EMULATED_HOST:
	case SCSI_IOCTL_SEND_COMMAND:
		return scsi_cmd_ioctl(bd->queue, NULL, file->f_mode, cmd, uarg);
	case SG_IO:
		return bsg_sg_io(bd->queue, file->f_mode, uarg);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations bsg_fops = {
	.open		=	bsg_open,
	.release	=	bsg_release,
	.unlocked_ioctl	=	bsg_ioctl,
	.owner		=	THIS_MODULE,
	.llseek		=	default_llseek,
};

void bsg_unregister_queue(struct request_queue *q)
{
	struct bsg_class_device *bcd = &q->bsg_dev;

	if (!bcd->class_dev)
		return;

	mutex_lock(&bsg_mutex);
	idr_remove(&bsg_minor_idr, bcd->minor);
	if (q->kobj.sd)
		sysfs_remove_link(&q->kobj, "bsg");
	device_unregister(bcd->class_dev);
	bcd->class_dev = NULL;
	mutex_unlock(&bsg_mutex);
}
EXPORT_SYMBOL_GPL(bsg_unregister_queue);

int bsg_register_queue(struct request_queue *q, struct device *parent,
		const char *name, const struct bsg_ops *ops)
{
	struct bsg_class_device *bcd;
	dev_t dev;
	int ret;
	struct device *class_dev = NULL;

	/*
	 * we need a proper transport to send commands, not a stacked device
	 */
	if (!queue_is_mq(q))
		return 0;

	bcd = &q->bsg_dev;
	memset(bcd, 0, sizeof(*bcd));

	mutex_lock(&bsg_mutex);

	ret = idr_alloc(&bsg_minor_idr, bcd, 0, BSG_MAX_DEVS, GFP_KERNEL);
	if (ret < 0) {
		if (ret == -ENOSPC) {
			printk(KERN_ERR "bsg: too many bsg devices\n");
			ret = -EINVAL;
		}
		goto unlock;
	}

	bcd->minor = ret;
	bcd->queue = q;
	bcd->ops = ops;
	dev = MKDEV(bsg_major, bcd->minor);
	class_dev = device_create(bsg_class, parent, dev, NULL, "%s", name);
	if (IS_ERR(class_dev)) {
		ret = PTR_ERR(class_dev);
		goto idr_remove;
	}
	bcd->class_dev = class_dev;

	if (q->kobj.sd) {
		ret = sysfs_create_link(&q->kobj, &bcd->class_dev->kobj, "bsg");
		if (ret)
			goto unregister_class_dev;
	}

	mutex_unlock(&bsg_mutex);
	return 0;

unregister_class_dev:
	device_unregister(class_dev);
idr_remove:
	idr_remove(&bsg_minor_idr, bcd->minor);
unlock:
	mutex_unlock(&bsg_mutex);
	return ret;
}

int bsg_scsi_register_queue(struct request_queue *q, struct device *parent)
{
	if (!blk_queue_scsi_passthrough(q)) {
		WARN_ONCE(true, "Attempt to register a non-SCSI queue\n");
		return -EINVAL;
	}

	return bsg_register_queue(q, parent, dev_name(parent), &bsg_scsi_ops);
}
EXPORT_SYMBOL_GPL(bsg_scsi_register_queue);

static struct cdev bsg_cdev;

static char *bsg_devnode(struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "bsg/%s", dev_name(dev));
}

static int __init bsg_init(void)
{
	int ret, i;
	dev_t devid;

	for (i = 0; i < BSG_LIST_ARRAY_SIZE; i++)
		INIT_HLIST_HEAD(&bsg_device_list[i]);

	bsg_class = class_create(THIS_MODULE, "bsg");
	if (IS_ERR(bsg_class))
		return PTR_ERR(bsg_class);
	bsg_class->devnode = bsg_devnode;

	ret = alloc_chrdev_region(&devid, 0, BSG_MAX_DEVS, "bsg");
	if (ret)
		goto destroy_bsg_class;

	bsg_major = MAJOR(devid);

	cdev_init(&bsg_cdev, &bsg_fops);
	ret = cdev_add(&bsg_cdev, MKDEV(bsg_major, 0), BSG_MAX_DEVS);
	if (ret)
		goto unregister_chrdev;

	printk(KERN_INFO BSG_DESCRIPTION " version " BSG_VERSION
	       " loaded (major %d)\n", bsg_major);
	return 0;
unregister_chrdev:
	unregister_chrdev_region(MKDEV(bsg_major, 0), BSG_MAX_DEVS);
destroy_bsg_class:
	class_destroy(bsg_class);
	return ret;
}

MODULE_AUTHOR("Jens Axboe");
MODULE_DESCRIPTION(BSG_DESCRIPTION);
MODULE_LICENSE("GPL");

device_initcall(bsg_init);
