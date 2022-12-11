/* SPDX-License-Identifier: GPL-2.0-only */
/* The industrial I/O core - generic buffer interfaces.
 *
 * Copyright (c) 2008 Jonathan Cameron
 */

#ifndef _IIO_BUFFER_GENERIC_H_
#define _IIO_BUFFER_GENERIC_H_
#include <linux/sysfs.h>
#include <linux/iio/iio.h>

struct iio_buffer;

void iio_buffer_set_attrs(struct iio_buffer *buffer,
			 const struct attribute **attrs);

<<<<<<< HEAD
=======
	int (*request_update)(struct iio_buffer *buffer);

	int (*set_bytes_per_datum)(struct iio_buffer *buffer, size_t bpd);
	int (*set_length)(struct iio_buffer *buffer, unsigned int length);

	void (*release)(struct iio_buffer *buffer);

	unsigned int modes;
};

/**
 * struct iio_buffer - general buffer structure
 * @length:		[DEVICE] number of datums in buffer
 * @bytes_per_datum:	[DEVICE] size of individual datum including timestamp
 * @scan_el_attrs:	[DRIVER] control of scan elements if that scan mode
 *			control method is used
 * @scan_mask:		[INTERN] bitmask used in masking scan mode elements
 * @scan_timestamp:	[INTERN] does the scan mode include a timestamp
 * @access:		[DRIVER] buffer access functions associated with the
 *			implementation.
 * @scan_el_dev_attr_list:[INTERN] list of scan element related attributes.
 * @scan_el_group:	[DRIVER] attribute group for those attributes not
 *			created from the iio_chan_info array.
 * @pollq:		[INTERN] wait queue to allow for polling on the buffer.
 * @stufftoread:	[INTERN] flag to indicate new data.
 * @demux_list:		[INTERN] list of operations required to demux the scan.
 * @demux_bounce:	[INTERN] buffer for doing gather from incoming scan.
 * @buffer_list:	[INTERN] entry in the devices list of current buffers.
 * @ref:		[INTERN] reference count of the buffer.
 * @watermark:		[INTERN] number of datums to wait for poll/read.
 */
struct iio_buffer {
	unsigned int				length;
	size_t					bytes_per_datum;
	struct attribute_group			*scan_el_attrs;
	long					*scan_mask;
	bool					scan_timestamp;
	const struct iio_buffer_access_funcs	*access;
	struct list_head			scan_el_dev_attr_list;
	struct attribute_group			buffer_group;
	struct attribute_group			scan_el_group;
	wait_queue_head_t			pollq;
	bool					stufftoread;
	const struct attribute			**attrs;
	struct list_head			demux_list;
	void					*demux_bounce;
	struct list_head			buffer_list;
	struct kref				ref;
	unsigned int				watermark;
};

/**
 * iio_update_buffers() - add or remove buffer from active list
 * @indio_dev:		device to add buffer to
 * @insert_buffer:	buffer to insert
 * @remove_buffer:	buffer_to_remove
 *
 * Note this will tear down the all buffering and build it up again
 */
int iio_update_buffers(struct iio_dev *indio_dev,
		       struct iio_buffer *insert_buffer,
		       struct iio_buffer *remove_buffer);

/**
 * iio_buffer_init() - Initialize the buffer structure
 * @buffer:		buffer to be initialized
 **/
void iio_buffer_init(struct iio_buffer *buffer);

int iio_scan_mask_query(struct iio_dev *indio_dev,
			struct iio_buffer *buffer, int bit);

/**
 * iio_push_to_buffers() - push to a registered buffer.
 * @indio_dev:		iio_dev structure for device.
 * @data:		Full scan.
 */
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
int iio_push_to_buffers(struct iio_dev *indio_dev, const void *data);

/**
 * iio_push_to_buffers_with_timestamp() - push data and timestamp to buffers
 * @indio_dev:		iio_dev structure for device.
 * @data:		sample data
 * @timestamp:		timestamp for the sample data
 *
 * Pushes data to the IIO device's buffers. If timestamps are enabled for the
 * device the function will store the supplied timestamp as the last element in
 * the sample data buffer before pushing it to the device buffers. The sample
 * data buffer needs to be large enough to hold the additional timestamp
 * (usually the buffer should be indio->scan_bytes bytes large).
 *
 * Returns 0 on success, a negative error code otherwise.
 */
static inline int iio_push_to_buffers_with_timestamp(struct iio_dev *indio_dev,
	void *data, int64_t timestamp)
{
	if (indio_dev->scan_timestamp) {
		size_t ts_offset = indio_dev->scan_bytes / sizeof(int64_t) - 1;
		((int64_t *)data)[ts_offset] = timestamp;
	}

	return iio_push_to_buffers(indio_dev, data);
}

bool iio_validate_scan_mask_onehot(struct iio_dev *indio_dev,
				   const unsigned long *mask);

void iio_device_attach_buffer(struct iio_dev *indio_dev,
			      struct iio_buffer *buffer);

#endif /* _IIO_BUFFER_GENERIC_H_ */
