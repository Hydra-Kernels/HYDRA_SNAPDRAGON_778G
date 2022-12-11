/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * ChromeOS EC multi-function device
 *
 * Copyright (C) 2012 Google, Inc
 */

#ifndef __LINUX_MFD_CROS_EC_H
#define __LINUX_MFD_CROS_EC_H

#include <linux/device.h>

/**
 * struct cros_ec_dev - ChromeOS EC device entry point.
 * @class_dev: Device structure used in sysfs.
 * @ec_dev: cros_ec_device structure to talk to the physical device.
 * @dev: Pointer to the platform device.
 * @debug_info: cros_ec_debugfs structure for debugging information.
 * @has_kb_wake_angle: True if at least 2 accelerometer are connected to the EC.
 * @cmd_offset: Offset to apply for each command.
 * @features: Features supported by the EC.
 */
struct cros_ec_dev {
	struct device class_dev;
	struct cros_ec_device *ec_dev;
	struct device *dev;
	struct cros_ec_debugfs *debug_info;
	bool has_kb_wake_angle;
	u16 cmd_offset;
	u32 features[2];
};

<<<<<<< HEAD
#define to_cros_ec_dev(dev)  container_of(dev, struct cros_ec_dev, class_dev)
=======
/**
 * cros_ec_suspend - Handle a suspend operation for the ChromeOS EC device
 *
 * This can be called by drivers to handle a suspend event.
 *
 * ec_dev: Device to suspend
 * @return 0 if ok, -ve on error
 */
int cros_ec_suspend(struct cros_ec_device *ec_dev);

/**
 * cros_ec_resume - Handle a resume operation for the ChromeOS EC device
 *
 * This can be called by drivers to handle a resume event.
 *
 * @ec_dev: Device to resume
 * @return 0 if ok, -ve on error
 */
int cros_ec_resume(struct cros_ec_device *ec_dev);

/**
 * cros_ec_prepare_tx - Prepare an outgoing message in the output buffer
 *
 * This is intended to be used by all ChromeOS EC drivers, but at present
 * only SPI uses it. Once LPC uses the same protocol it can start using it.
 * I2C could use it now, with a refactor of the existing code.
 *
 * @ec_dev: Device to register
 * @msg: Message to write
 */
int cros_ec_prepare_tx(struct cros_ec_device *ec_dev,
		       struct cros_ec_command *msg);

/**
 * cros_ec_check_result - Check ec_msg->result
 *
 * This is used by ChromeOS EC drivers to check the ec_msg->result for
 * errors and to warn about them.
 *
 * @ec_dev: EC device
 * @msg: Message to check
 */
int cros_ec_check_result(struct cros_ec_device *ec_dev,
			 struct cros_ec_command *msg);

/**
 * cros_ec_cmd_xfer - Send a command to the ChromeOS EC
 *
 * Call this to send a command to the ChromeOS EC.  This should be used
 * instead of calling the EC's cmd_xfer() callback directly.
 *
 * @ec_dev: EC device
 * @msg: Message to write
 */
int cros_ec_cmd_xfer(struct cros_ec_device *ec_dev,
		     struct cros_ec_command *msg);

/**
 * cros_ec_cmd_xfer_status - Send a command to the ChromeOS EC
 *
 * This function is identical to cros_ec_cmd_xfer, except it returns success
 * status only if both the command was transmitted successfully and the EC
 * replied with success status. It's not necessary to check msg->result when
 * using this function.
 *
 * @ec_dev: EC device
 * @msg: Message to write
 * @return: Num. of bytes transferred on success, <0 on failure
 */
int cros_ec_cmd_xfer_status(struct cros_ec_device *ec_dev,
			    struct cros_ec_command *msg);

/**
 * cros_ec_remove - Remove a ChromeOS EC
 *
 * Call this to deregister a ChromeOS EC, then clean up any private data.
 *
 * @ec_dev: Device to register
 * @return 0 if ok, -ve on error
 */
int cros_ec_remove(struct cros_ec_device *ec_dev);

/**
 * cros_ec_register - Register a new ChromeOS EC, using the provided info
 *
 * Before calling this, allocate a pointer to a new device and then fill
 * in all the fields up to the --private-- marker.
 *
 * @ec_dev: Device to register
 * @return 0 if ok, -ve on error
 */
int cros_ec_register(struct cros_ec_device *ec_dev);

/**
 * cros_ec_register -  Query the protocol version supported by the ChromeOS EC
 *
 * @ec_dev: Device to register
 * @return 0 if ok, -ve on error
 */
int cros_ec_query_all(struct cros_ec_device *ec_dev);

/* sysfs stuff */
extern struct attribute_group cros_ec_attr_group;
extern struct attribute_group cros_ec_lightbar_attr_group;
extern struct attribute_group cros_ec_vbc_attr_group;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

#endif /* __LINUX_MFD_CROS_EC_H */
