// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2004 IBM Corporation
 * Copyright (C) 2014 Intel Corporation
 *
 * Authors:
 * Leendert van Doorn <leendert@watson.ibm.com>
 * Dave Safford <safford@watson.ibm.com>
 * Reiner Sailer <sailer@watson.ibm.com>
 * Kylene Hall <kjhall@us.ibm.com>
 *
 * Maintained by: <tpmdd-devel@lists.sourceforge.net>
 *
 * Device driver for TCG/TCPA TPM (trusted platform module).
 * Specifications at www.trustedcomputinggroup.org
 *
 * Note, the TPM chip is not interrupt driven (only polling)
 * and can have very long timeouts (minutes!). Hence the unusual
 * calls to msleep.
 */

#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/freezer.h>
#include <linux/tpm_eventlog.h>

#include "tpm.h"

/*
 * Bug workaround - some TPM's don't flush the most
 * recently changed pcr on suspend, so force the flush
 * with an extend to the selected _unused_ non-volatile pcr.
 */
static u32 tpm_suspend_pcr;
module_param_named(suspend_pcr, tpm_suspend_pcr, uint, 0644);
MODULE_PARM_DESC(suspend_pcr,
		 "PCR to use for dummy writes to facilitate flush on suspend.");

/**
 * tpm_calc_ordinal_duration() - calculate the maximum command duration
 * @chip:    TPM chip to use.
 * @ordinal: TPM command ordinal.
 *
 * The function returns the maximum amount of time the chip could take
 * to return the result for a particular ordinal in jiffies.
 *
 * Return: A maximal duration time for an ordinal in jiffies.
 */
unsigned long tpm_calc_ordinal_duration(struct tpm_chip *chip, u32 ordinal)
{
	if (chip->flags & TPM_CHIP_FLAG_TPM2)
		return tpm2_calc_ordinal_duration(chip, ordinal);
	else
		return tpm1_calc_ordinal_duration(chip, ordinal);
}
EXPORT_SYMBOL_GPL(tpm_calc_ordinal_duration);

<<<<<<< HEAD
static ssize_t tpm_try_transmit(struct tpm_chip *chip, void *buf, size_t bufsiz)
=======
/*
 * Internal kernel interface to transmit TPM commands
 */
ssize_t tpm_transmit(struct tpm_chip *chip, const u8 *buf, size_t bufsiz,
		     unsigned int flags)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
{
	struct tpm_header *header = buf;
	int rc;
	ssize_t len = 0;
	u32 count, ordinal;
	unsigned long stop;

	if (bufsiz < TPM_HEADER_SIZE)
		return -EINVAL;

	if (bufsiz > TPM_BUFSIZE)
		bufsiz = TPM_BUFSIZE;

	count = be32_to_cpu(header->length);
	ordinal = be32_to_cpu(header->ordinal);
	if (count == 0)
		return -ENODATA;
	if (count > bufsiz) {
		dev_err(&chip->dev,
			"invalid count value %x %zx\n", count, bufsiz);
		return -E2BIG;
	}

<<<<<<< HEAD
	rc = chip->ops->send(chip, buf, count);
	if (rc < 0) {
		if (rc != -EPIPE)
			dev_err(&chip->dev,
				"%s: send(): error %d\n", __func__, rc);
		return rc;
=======
	if (!(flags & TPM_TRANSMIT_UNLOCKED))
		mutex_lock(&chip->tpm_mutex);

	rc = chip->ops->send(chip, (u8 *) buf, count);
	if (rc < 0) {
		dev_err(&chip->dev,
			"tpm_transmit: tpm_send: error %zd\n", rc);
		goto out;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	}

	/* A sanity check. send() should just return zero on success e.g.
	 * not the command length.
	 */
	if (rc > 0) {
		dev_warn(&chip->dev,
			 "%s: send(): invalid value %d\n", __func__, rc);
		rc = 0;
	}

	if (chip->flags & TPM_CHIP_FLAG_IRQ)
		goto out_recv;

	stop = jiffies + tpm_calc_ordinal_duration(chip, ordinal);
	do {
		u8 status = chip->ops->status(chip);
		if ((status & chip->ops->req_complete_mask) ==
		    chip->ops->req_complete_val)
			goto out_recv;

		if (chip->ops->req_canceled(chip, status)) {
			dev_err(&chip->dev, "Operation Canceled\n");
<<<<<<< HEAD
			return -ECANCELED;
=======
			rc = -ECANCELED;
			goto out;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
		}

		tpm_msleep(TPM_TIMEOUT_POLL);
		rmb();
	} while (time_before(jiffies, stop));

	chip->ops->cancel(chip);
	dev_err(&chip->dev, "Operation Timed out\n");
<<<<<<< HEAD
	return -ETIME;

out_recv:
	len = chip->ops->recv(chip, buf, bufsiz);
	if (len < 0) {
		rc = len;
		dev_err(&chip->dev, "tpm_transmit: tpm_recv: error %d\n", rc);
	} else if (len < TPM_HEADER_SIZE || len != be32_to_cpu(header->length))
		rc = -EFAULT;

	return rc ? rc : len;
}

/**
 * tpm_transmit - Internal kernel interface to transmit TPM commands.
 * @chip:	a TPM chip to use
 * @buf:	a TPM command buffer
 * @bufsiz:	length of the TPM command buffer
 *
 * A wrapper around tpm_try_transmit() that handles TPM2_RC_RETRY returns from
 * the TPM and retransmits the command after a delay up to a maximum wait of
 * TPM2_DURATION_LONG.
 *
 * Note that TPM 1.x never returns TPM2_RC_RETRY so the retry logic is TPM 2.0
 * only.
 *
 * Return:
 * * The response length	- OK
 * * -errno			- A system error
 */
ssize_t tpm_transmit(struct tpm_chip *chip, u8 *buf, size_t bufsiz)
{
	struct tpm_header *header = (struct tpm_header *)buf;
	/* space for header and handles */
	u8 save[TPM_HEADER_SIZE + 3*sizeof(u32)];
	unsigned int delay_msec = TPM2_DURATION_SHORT;
	u32 rc = 0;
	ssize_t ret;
	const size_t save_size = min(sizeof(save), bufsiz);
	/* the command code is where the return code will be */
	u32 cc = be32_to_cpu(header->return_code);

	/*
	 * Subtlety here: if we have a space, the handles will be
	 * transformed, so when we restore the header we also have to
	 * restore the handles.
	 */
	memcpy(save, buf, save_size);

	for (;;) {
		ret = tpm_try_transmit(chip, buf, bufsiz);
		if (ret < 0)
			break;
		rc = be32_to_cpu(header->return_code);
		if (rc != TPM2_RC_RETRY && rc != TPM2_RC_TESTING)
			break;
		/*
		 * return immediately if self test returns test
		 * still running to shorten boot time.
		 */
		if (rc == TPM2_RC_TESTING && cc == TPM2_CC_SELF_TEST)
			break;

		if (delay_msec > TPM2_DURATION_LONG) {
			if (rc == TPM2_RC_RETRY)
				dev_err(&chip->dev, "in retry loop\n");
			else
				dev_err(&chip->dev,
					"self test is still running\n");
			break;
		}
		tpm_msleep(delay_msec);
		delay_msec *= 2;
		memcpy(buf, save, save_size);
	}
	return ret;
}

/**
 * tpm_transmit_cmd - send a tpm command to the device
 * @chip:			a TPM chip to use
 * @buf:			a TPM command buffer
 * @min_rsp_body_length:	minimum expected length of response body
 * @desc:			command description used in the error message
 *
 * Return:
 * * 0		- OK
 * * -errno	- A system error
 * * TPM_RC	- A TPM error
 */
ssize_t tpm_transmit_cmd(struct tpm_chip *chip, struct tpm_buf *buf,
			 size_t min_rsp_body_length, const char *desc)
{
	const struct tpm_header *header = (struct tpm_header *)buf->data;
	int err;
	ssize_t len;

	len = tpm_transmit(chip, buf->data, PAGE_SIZE);
=======
	rc = -ETIME;
	goto out;

out_recv:
	rc = chip->ops->recv(chip, (u8 *) buf, bufsiz);
	if (rc < 0)
		dev_err(&chip->dev,
			"tpm_transmit: tpm_recv: error %zd\n", rc);
out:
	if (!(flags & TPM_TRANSMIT_UNLOCKED))
		mutex_unlock(&chip->tpm_mutex);
	return rc;
}

#define TPM_DIGEST_SIZE 20
#define TPM_RET_CODE_IDX 6

ssize_t tpm_transmit_cmd(struct tpm_chip *chip, const void *cmd,
			 int len, unsigned int flags, const char *desc)
{
	const struct tpm_output_header *header;
	int err;

	len = tpm_transmit(chip, (const u8 *)cmd, len, flags);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	if (len <  0)
		return len;

	err = be32_to_cpu(header->return_code);
<<<<<<< HEAD
	if (err != 0 && err != TPM_ERR_DISABLED && err != TPM_ERR_DEACTIVATED
	    && err != TPM2_RC_TESTING && desc)
=======
	if (err != 0 && desc)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
		dev_err(&chip->dev, "A TPM error (%d) occurred %s\n", err,
			desc);
	if (err)
		return err;

<<<<<<< HEAD
	if (len < min_rsp_body_length + TPM_HEADER_SIZE)
		return -EFAULT;

	return 0;
=======
	return err;
}

#define TPM_INTERNAL_RESULT_SIZE 200
#define TPM_ORD_GET_CAP cpu_to_be32(101)
#define TPM_ORD_GET_RANDOM cpu_to_be32(70)

static const struct tpm_input_header tpm_getcap_header = {
	.tag = TPM_TAG_RQU_COMMAND,
	.length = cpu_to_be32(22),
	.ordinal = TPM_ORD_GET_CAP
};

ssize_t tpm_getcap(struct device *dev, __be32 subcap_id, cap_t *cap,
		   const char *desc)
{
	struct tpm_cmd_t tpm_cmd;
	int rc;
	struct tpm_chip *chip = dev_get_drvdata(dev);

	tpm_cmd.header.in = tpm_getcap_header;
	if (subcap_id == CAP_VERSION_1_1 || subcap_id == CAP_VERSION_1_2) {
		tpm_cmd.params.getcap_in.cap = subcap_id;
		/*subcap field not necessary */
		tpm_cmd.params.getcap_in.subcap_size = cpu_to_be32(0);
		tpm_cmd.header.in.length -= cpu_to_be32(sizeof(__be32));
	} else {
		if (subcap_id == TPM_CAP_FLAG_PERM ||
		    subcap_id == TPM_CAP_FLAG_VOL)
			tpm_cmd.params.getcap_in.cap = TPM_CAP_FLAG;
		else
			tpm_cmd.params.getcap_in.cap = TPM_CAP_PROP;
		tpm_cmd.params.getcap_in.subcap_size = cpu_to_be32(4);
		tpm_cmd.params.getcap_in.subcap = subcap_id;
	}
	rc = tpm_transmit_cmd(chip, &tpm_cmd, TPM_INTERNAL_RESULT_SIZE, 0,
			      desc);
	if (!rc)
		*cap = tpm_cmd.params.getcap_out.cap;
	return rc;
}

void tpm_gen_interrupt(struct tpm_chip *chip)
{
	struct	tpm_cmd_t tpm_cmd;
	ssize_t rc;

	tpm_cmd.header.in = tpm_getcap_header;
	tpm_cmd.params.getcap_in.cap = TPM_CAP_PROP;
	tpm_cmd.params.getcap_in.subcap_size = cpu_to_be32(4);
	tpm_cmd.params.getcap_in.subcap = TPM_CAP_PROP_TIS_TIMEOUT;

	rc = tpm_transmit_cmd(chip, &tpm_cmd, TPM_INTERNAL_RESULT_SIZE, 0,
			      "attempting to determine the timeouts");
}
EXPORT_SYMBOL_GPL(tpm_gen_interrupt);

#define TPM_ORD_STARTUP cpu_to_be32(153)
#define TPM_ST_CLEAR cpu_to_be16(1)
#define TPM_ST_STATE cpu_to_be16(2)
#define TPM_ST_DEACTIVATED cpu_to_be16(3)
static const struct tpm_input_header tpm_startup_header = {
	.tag = TPM_TAG_RQU_COMMAND,
	.length = cpu_to_be32(12),
	.ordinal = TPM_ORD_STARTUP
};

static int tpm_startup(struct tpm_chip *chip, __be16 startup_type)
{
	struct tpm_cmd_t start_cmd;
	start_cmd.header.in = tpm_startup_header;

	start_cmd.params.startup_in.startup_type = startup_type;
	return tpm_transmit_cmd(chip, &start_cmd, TPM_INTERNAL_RESULT_SIZE, 0,
				"attempting to start the TPM");
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}
EXPORT_SYMBOL_GPL(tpm_transmit_cmd);

int tpm_get_timeouts(struct tpm_chip *chip)
{
	if (chip->flags & TPM_CHIP_FLAG_HAVE_TIMEOUTS)
		return 0;

<<<<<<< HEAD
	if (chip->flags & TPM_CHIP_FLAG_TPM2)
		return tpm2_get_timeouts(chip);
	else
		return tpm1_get_timeouts(chip);
=======
	tpm_cmd.header.in = tpm_getcap_header;
	tpm_cmd.params.getcap_in.cap = TPM_CAP_PROP;
	tpm_cmd.params.getcap_in.subcap_size = cpu_to_be32(4);
	tpm_cmd.params.getcap_in.subcap = TPM_CAP_PROP_TIS_TIMEOUT;
	rc = tpm_transmit_cmd(chip, &tpm_cmd, TPM_INTERNAL_RESULT_SIZE, 0,
			      NULL);

	if (rc == TPM_ERR_INVALID_POSTINIT) {
		/* The TPM is not started, we are the first to talk to it.
		   Execute a startup command. */
		dev_info(&chip->dev, "Issuing TPM_STARTUP");
		if (tpm_startup(chip, TPM_ST_CLEAR))
			return rc;

		tpm_cmd.header.in = tpm_getcap_header;
		tpm_cmd.params.getcap_in.cap = TPM_CAP_PROP;
		tpm_cmd.params.getcap_in.subcap_size = cpu_to_be32(4);
		tpm_cmd.params.getcap_in.subcap = TPM_CAP_PROP_TIS_TIMEOUT;
		rc = tpm_transmit_cmd(chip, &tpm_cmd, TPM_INTERNAL_RESULT_SIZE,
				      0, NULL);
	}
	if (rc) {
		dev_err(&chip->dev,
			"A TPM error (%zd) occurred attempting to determine the timeouts\n",
			rc);
		goto duration;
	}

	if (be32_to_cpu(tpm_cmd.header.out.return_code) != 0 ||
	    be32_to_cpu(tpm_cmd.header.out.length)
	    != sizeof(tpm_cmd.header.out) + sizeof(u32) + 4 * sizeof(u32))
		return -EINVAL;

	old_timeout[0] = be32_to_cpu(tpm_cmd.params.getcap_out.cap.timeout.a);
	old_timeout[1] = be32_to_cpu(tpm_cmd.params.getcap_out.cap.timeout.b);
	old_timeout[2] = be32_to_cpu(tpm_cmd.params.getcap_out.cap.timeout.c);
	old_timeout[3] = be32_to_cpu(tpm_cmd.params.getcap_out.cap.timeout.d);
	memcpy(new_timeout, old_timeout, sizeof(new_timeout));

	/*
	 * Provide ability for vendor overrides of timeout values in case
	 * of misreporting.
	 */
	if (chip->ops->update_timeouts != NULL)
		chip->vendor.timeout_adjusted =
			chip->ops->update_timeouts(chip, new_timeout);

	if (!chip->vendor.timeout_adjusted) {
		/* Don't overwrite default if value is 0 */
		if (new_timeout[0] != 0 && new_timeout[0] < 1000) {
			int i;

			/* timeouts in msec rather usec */
			for (i = 0; i != ARRAY_SIZE(new_timeout); i++)
				new_timeout[i] *= 1000;
			chip->vendor.timeout_adjusted = true;
		}
	}

	/* Report adjusted timeouts */
	if (chip->vendor.timeout_adjusted) {
		dev_info(&chip->dev,
			 HW_ERR "Adjusting reported timeouts: A %lu->%luus B %lu->%luus C %lu->%luus D %lu->%luus\n",
			 old_timeout[0], new_timeout[0],
			 old_timeout[1], new_timeout[1],
			 old_timeout[2], new_timeout[2],
			 old_timeout[3], new_timeout[3]);
	}

	chip->vendor.timeout_a = usecs_to_jiffies(new_timeout[0]);
	chip->vendor.timeout_b = usecs_to_jiffies(new_timeout[1]);
	chip->vendor.timeout_c = usecs_to_jiffies(new_timeout[2]);
	chip->vendor.timeout_d = usecs_to_jiffies(new_timeout[3]);

duration:
	tpm_cmd.header.in = tpm_getcap_header;
	tpm_cmd.params.getcap_in.cap = TPM_CAP_PROP;
	tpm_cmd.params.getcap_in.subcap_size = cpu_to_be32(4);
	tpm_cmd.params.getcap_in.subcap = TPM_CAP_PROP_TIS_DURATION;

	rc = tpm_transmit_cmd(chip, &tpm_cmd, TPM_INTERNAL_RESULT_SIZE, 0,
			      "attempting to determine the durations");
	if (rc)
		return rc;

	if (be32_to_cpu(tpm_cmd.header.out.return_code) != 0 ||
	    be32_to_cpu(tpm_cmd.header.out.length)
	    != sizeof(tpm_cmd.header.out) + sizeof(u32) + 3 * sizeof(u32))
		return -EINVAL;

	duration_cap = &tpm_cmd.params.getcap_out.cap.duration;
	chip->vendor.duration[TPM_SHORT] =
	    usecs_to_jiffies(be32_to_cpu(duration_cap->tpm_short));
	chip->vendor.duration[TPM_MEDIUM] =
	    usecs_to_jiffies(be32_to_cpu(duration_cap->tpm_medium));
	chip->vendor.duration[TPM_LONG] =
	    usecs_to_jiffies(be32_to_cpu(duration_cap->tpm_long));

	/* The Broadcom BCM0102 chipset in a Dell Latitude D820 gets the above
	 * value wrong and apparently reports msecs rather than usecs. So we
	 * fix up the resulting too-small TPM_SHORT value to make things work.
	 * We also scale the TPM_MEDIUM and -_LONG values by 1000.
	 */
	if (chip->vendor.duration[TPM_SHORT] < (HZ / 100)) {
		chip->vendor.duration[TPM_SHORT] = HZ;
		chip->vendor.duration[TPM_MEDIUM] *= 1000;
		chip->vendor.duration[TPM_LONG] *= 1000;
		chip->vendor.duration_adjusted = true;
		dev_info(&chip->dev, "Adjusting TPM timeout parameters.");
	}
	return 0;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}
EXPORT_SYMBOL_GPL(tpm_get_timeouts);

/**
 * tpm_is_tpm2 - do we a have a TPM2 chip?
 * @chip:	a &struct tpm_chip instance, %NULL for the default chip
 *
 * Return:
 * 1 if we have a TPM2 chip.
 * 0 if we don't have a TPM2 chip.
 * A negative number for system errors (errno).
 */
int tpm_is_tpm2(struct tpm_chip *chip)
{
	int rc;

<<<<<<< HEAD
	chip = tpm_find_get_ops(chip);
	if (!chip)
=======
	cmd.header.in = continue_selftest_header;
	rc = tpm_transmit_cmd(chip, &cmd, CONTINUE_SELFTEST_RESULT_SIZE, 0,
			      "continue selftest");
	return rc;
}

#define TPM_ORDINAL_PCRREAD cpu_to_be32(21)
#define READ_PCR_RESULT_SIZE 30
static struct tpm_input_header pcrread_header = {
	.tag = TPM_TAG_RQU_COMMAND,
	.length = cpu_to_be32(14),
	.ordinal = TPM_ORDINAL_PCRREAD
};

int tpm_pcr_read_dev(struct tpm_chip *chip, int pcr_idx, u8 *res_buf)
{
	int rc;
	struct tpm_cmd_t cmd;

	cmd.header.in = pcrread_header;
	cmd.params.pcrread_in.pcr_idx = cpu_to_be32(pcr_idx);
	rc = tpm_transmit_cmd(chip, &cmd, READ_PCR_RESULT_SIZE, 0,
			      "attempting to read a pcr value");

	if (rc == 0)
		memcpy(res_buf, cmd.params.pcrread_out.pcr_result,
		       TPM_DIGEST_SIZE);
	return rc;
}

/**
 * tpm_is_tpm2 - is the chip a TPM2 chip?
 * @chip_num:	tpm idx # or ANY
 *
 * Returns < 0 on error, and 1 or 0 on success depending whether the chip
 * is a TPM2 chip.
 */
int tpm_is_tpm2(u32 chip_num)
{
	struct tpm_chip *chip;
	int rc;

	chip = tpm_chip_find_get(chip_num);
	if (chip == NULL)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
		return -ENODEV;

	rc = (chip->flags & TPM_CHIP_FLAG_TPM2) != 0;

	tpm_put_ops(chip);

	return rc;
}
EXPORT_SYMBOL_GPL(tpm_is_tpm2);

/**
 * tpm_pcr_read - read a PCR value from SHA1 bank
 * @chip:	a &struct tpm_chip instance, %NULL for the default chip
 * @pcr_idx:	the PCR to be retrieved
 * @digest:	the PCR bank and buffer current PCR value is written to
 *
 * Return: same as with tpm_transmit_cmd()
 */
int tpm_pcr_read(struct tpm_chip *chip, u32 pcr_idx,
		 struct tpm_digest *digest)
{
	int rc;

	chip = tpm_find_get_ops(chip);
	if (!chip)
		return -ENODEV;

	if (chip->flags & TPM_CHIP_FLAG_TPM2)
		rc = tpm2_pcr_read(chip, pcr_idx, digest, NULL);
	else
<<<<<<< HEAD
		rc = tpm1_pcr_read(chip, pcr_idx, digest->digest);

=======
		rc = tpm_pcr_read_dev(chip, pcr_idx, res_buf);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	tpm_put_ops(chip);
	return rc;
}
EXPORT_SYMBOL_GPL(tpm_pcr_read);

/**
 * tpm_pcr_extend - extend a PCR value in SHA1 bank.
 * @chip:	a &struct tpm_chip instance, %NULL for the default chip
 * @pcr_idx:	the PCR to be retrieved
 * @digests:	array of tpm_digest structures used to extend PCRs
 *
 * Note: callers must pass a digest for every allocated PCR bank, in the same
 * order of the banks in chip->allocated_banks.
 *
 * Return: same as with tpm_transmit_cmd()
 */
int tpm_pcr_extend(struct tpm_chip *chip, u32 pcr_idx,
		   struct tpm_digest *digests)
{
	int rc;
	int i;

	chip = tpm_find_get_ops(chip);
	if (!chip)
		return -ENODEV;

<<<<<<< HEAD
	for (i = 0; i < chip->nr_allocated_banks; i++) {
		if (digests[i].alg_id != chip->allocated_banks[i].alg_id) {
			rc = -EINVAL;
			goto out;
		}
	}

	if (chip->flags & TPM_CHIP_FLAG_TPM2) {
		rc = tpm2_pcr_extend(chip, pcr_idx, digests);
		goto out;
	}

	rc = tpm1_pcr_extend(chip, pcr_idx, digests[0].digest,
			     "attempting extend a PCR value");

out:
=======
	if (chip->flags & TPM_CHIP_FLAG_TPM2) {
		rc = tpm2_pcr_extend(chip, pcr_idx, hash);
		tpm_put_ops(chip);
		return rc;
	}

	cmd.header.in = pcrextend_header;
	cmd.params.pcrextend_in.pcr_idx = cpu_to_be32(pcr_idx);
	memcpy(cmd.params.pcrextend_in.hash, hash, TPM_DIGEST_SIZE);
	rc = tpm_transmit_cmd(chip, &cmd, EXTEND_PCR_RESULT_SIZE, 0,
			      "attempting extend a PCR value");

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	tpm_put_ops(chip);
	return rc;
}
EXPORT_SYMBOL_GPL(tpm_pcr_extend);

/**
 * tpm_send - send a TPM command
 * @chip:	a &struct tpm_chip instance, %NULL for the default chip
 * @cmd:	a TPM command buffer
 * @buflen:	the length of the TPM command buffer
 *
 * Return: same as with tpm_transmit_cmd()
 */
int tpm_send(struct tpm_chip *chip, void *cmd, size_t buflen)
{
<<<<<<< HEAD
	struct tpm_buf buf;
=======
	int rc;
	unsigned int loops;
	unsigned int delay_msec = 100;
	unsigned long duration;
	struct tpm_cmd_t cmd;

	duration = tpm_calc_ordinal_duration(chip, TPM_ORD_CONTINUE_SELFTEST);

	loops = jiffies_to_msecs(duration) / delay_msec;

	rc = tpm_continue_selftest(chip);
	if (rc == TPM_ERR_INVALID_POSTINIT) {
		chip->flags |= TPM_CHIP_FLAG_ALWAYS_POWERED;
		dev_info(&chip->dev, "TPM not ready (%d)\n", rc);
	}
	/* This may fail if there was no TPM driver during a suspend/resume
	 * cycle; some may return 10 (BAD_ORDINAL), others 28 (FAILEDSELFTEST)
	 */
	if (rc)
		return rc;

	do {
		/* Attempt to read a PCR value */
		cmd.header.in = pcrread_header;
		cmd.params.pcrread_in.pcr_idx = cpu_to_be32(0);
		rc = tpm_transmit(chip, (u8 *) &cmd, READ_PCR_RESULT_SIZE, 0);
		/* Some buggy TPMs will not respond to tpm_tis_ready() for
		 * around 300ms while the self test is ongoing, keep trying
		 * until the self test duration expires. */
		if (rc == -ETIME) {
			dev_info(
			    &chip->dev, HW_ERR
			    "TPM command timed out during continue self test");
			msleep(delay_msec);
			continue;
		}

		if (rc < TPM_HEADER_SIZE)
			return -EFAULT;

		rc = be32_to_cpu(cmd.header.out.return_code);
		if (rc == TPM_ERR_DISABLED || rc == TPM_ERR_DEACTIVATED) {
			dev_info(&chip->dev,
				 "TPM is disabled/deactivated (0x%X)\n", rc);
			/* TPM is disabled and/or deactivated; driver can
			 * proceed and TPM does handle commands for
			 * suspend/resume correctly
			 */
			return 0;
		}
		if (rc != TPM_WARN_DOING_SELFTEST)
			return rc;
		msleep(delay_msec);
	} while (--loops > 0);

	return rc;
}
EXPORT_SYMBOL_GPL(tpm_do_selftest);

int tpm_send(u32 chip_num, void *cmd, size_t buflen)
{
	struct tpm_chip *chip;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	int rc;

	chip = tpm_find_get_ops(chip);
	if (!chip)
		return -ENODEV;

<<<<<<< HEAD
	buf.data = cmd;
	rc = tpm_transmit_cmd(chip, &buf, 0, "attempting to a send a command");
=======
	rc = tpm_transmit_cmd(chip, cmd, buflen, 0, "attempting tpm_cmd");
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

	tpm_put_ops(chip);
	return rc;
}
EXPORT_SYMBOL_GPL(tpm_send);

int tpm_auto_startup(struct tpm_chip *chip)
{
	int rc;

	if (!(chip->ops->flags & TPM_OPS_AUTO_STARTUP))
		return 0;

	if (chip->flags & TPM_CHIP_FLAG_TPM2)
		rc = tpm2_auto_startup(chip);
	else
		rc = tpm1_auto_startup(chip);

	return rc;
}

/*
 * We are about to suspend. Save the TPM state
 * so that it can be restored.
 */
int tpm_pm_suspend(struct device *dev)
{
	struct tpm_chip *chip = dev_get_drvdata(dev);
	int rc = 0;

	if (!chip)
		return -ENODEV;

	if (chip->flags & TPM_CHIP_FLAG_ALWAYS_POWERED)
<<<<<<< HEAD
=======
		return 0;

	if (chip->flags & TPM_CHIP_FLAG_TPM2) {
		tpm2_shutdown(chip, TPM2_SU_STATE);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
		return 0;

	if (!tpm_chip_start(chip)) {
		if (chip->flags & TPM_CHIP_FLAG_TPM2)
			tpm2_shutdown(chip, TPM2_SU_STATE);
		else
			rc = tpm1_pm_suspend(chip, tpm_suspend_pcr);

		tpm_chip_stop(chip);
	}

<<<<<<< HEAD
=======
	/* for buggy tpm, flush pcrs with extend to selected dummy */
	if (tpm_suspend_pcr) {
		cmd.header.in = pcrextend_header;
		cmd.params.pcrextend_in.pcr_idx = cpu_to_be32(tpm_suspend_pcr);
		memcpy(cmd.params.pcrextend_in.hash, dummy_hash,
		       TPM_DIGEST_SIZE);
		rc = tpm_transmit_cmd(chip, &cmd, EXTEND_PCR_RESULT_SIZE, 0,
				      "extending dummy pcr before suspend");
	}

	/* now do the actual savestate */
	for (try = 0; try < TPM_RETRY; try++) {
		cmd.header.in = savestate_header;
		rc = tpm_transmit_cmd(chip, &cmd, SAVESTATE_RESULT_SIZE, 0,
				      NULL);

		/*
		 * If the TPM indicates that it is too busy to respond to
		 * this command then retry before giving up.  It can take
		 * several seconds for this TPM to be ready.
		 *
		 * This can happen if the TPM has already been sent the
		 * SaveState command before the driver has loaded.  TCG 1.2
		 * specification states that any communication after SaveState
		 * may cause the TPM to invalidate previously saved state.
		 */
		if (rc != TPM_WARN_RETRY)
			break;
		msleep(TPM_TIMEOUT_RETRY);
	}

	if (rc)
		dev_err(&chip->dev,
			"Error (%d) sending savestate before suspend\n", rc);
	else if (try > 0)
		dev_warn(&chip->dev, "TPM savestate took %dms\n",
			 try * TPM_TIMEOUT_RETRY);

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	return rc;
}
EXPORT_SYMBOL_GPL(tpm_pm_suspend);

/*
 * Resume from a power safe. The BIOS already restored
 * the TPM state.
 */
int tpm_pm_resume(struct device *dev)
{
	struct tpm_chip *chip = dev_get_drvdata(dev);

	if (chip == NULL)
		return -ENODEV;

	return 0;
}
EXPORT_SYMBOL_GPL(tpm_pm_resume);

/**
 * tpm_get_random() - get random bytes from the TPM's RNG
 * @chip:	a &struct tpm_chip instance, %NULL for the default chip
 * @out:	destination buffer for the random bytes
 * @max:	the max number of bytes to write to @out
 *
 * Return: number of random bytes read or a negative error value.
 */
int tpm_get_random(struct tpm_chip *chip, u8 *out, size_t max)
{
	int rc;

	if (!out || max > TPM_MAX_RNG_DATA)
		return -EINVAL;

	chip = tpm_find_get_ops(chip);
	if (!chip)
		return -ENODEV;

<<<<<<< HEAD
	if (chip->flags & TPM_CHIP_FLAG_TPM2)
		rc = tpm2_get_random(chip, out, max);
	else
		rc = tpm1_get_random(chip, out, max);

	tpm_put_ops(chip);
	return rc;
=======
	if (chip->flags & TPM_CHIP_FLAG_TPM2) {
		err = tpm2_get_random(chip, out, max);
		tpm_put_ops(chip);
		return err;
	}

	do {
		tpm_cmd.header.in = tpm_getrandom_header;
		tpm_cmd.params.getrandom_in.num_bytes = cpu_to_be32(num_bytes);

		err = tpm_transmit_cmd(chip, &tpm_cmd,
				       TPM_GETRANDOM_RESULT_SIZE + num_bytes,
				       0, "attempting get random");
		if (err)
			break;

		recd = be32_to_cpu(tpm_cmd.params.getrandom_out.rng_data_len);
		if (recd > num_bytes) {
			total = -EFAULT;
			break;
		}

		memcpy(dest, tpm_cmd.params.getrandom_out.rng_data, recd);

		dest += recd;
		total += recd;
		num_bytes -= recd;
	} while (retries-- && total < max);

	tpm_put_ops(chip);
	return total ? total : -EIO;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}
EXPORT_SYMBOL_GPL(tpm_get_random);

/**
 * tpm_seal_trusted() - seal a trusted key payload
 * @chip:	a &struct tpm_chip instance, %NULL for the default chip
 * @options:	authentication values and other options
 * @payload:	the key data in clear and encrypted form
 *
 * Note: only TPM 2.0 chip are supported. TPM 1.x implementation is located in
 * the keyring subsystem.
 *
 * Return: same as with tpm_transmit_cmd()
 */
int tpm_seal_trusted(struct tpm_chip *chip, struct trusted_key_payload *payload,
		     struct trusted_key_options *options)
{
	int rc;

	chip = tpm_find_get_ops(chip);
	if (!chip || !(chip->flags & TPM_CHIP_FLAG_TPM2))
		return -ENODEV;

	rc = tpm2_seal_trusted(chip, payload, options);

	tpm_put_ops(chip);
	return rc;
}
EXPORT_SYMBOL_GPL(tpm_seal_trusted);

/**
 * tpm_unseal_trusted() - unseal a trusted key
 * @chip:	a &struct tpm_chip instance, %NULL for the default chip
 * @options:	authentication values and other options
 * @payload:	the key data in clear and encrypted form
 *
 * Note: only TPM 2.0 chip are supported. TPM 1.x implementation is located in
 * the keyring subsystem.
 *
 * Return: same as with tpm_transmit_cmd()
 */
int tpm_unseal_trusted(struct tpm_chip *chip,
		       struct trusted_key_payload *payload,
		       struct trusted_key_options *options)
{
	int rc;

	chip = tpm_find_get_ops(chip);
	if (!chip || !(chip->flags & TPM_CHIP_FLAG_TPM2))
		return -ENODEV;

	rc = tpm2_unseal_trusted(chip, payload, options);

	tpm_put_ops(chip);

	return rc;
}
EXPORT_SYMBOL_GPL(tpm_unseal_trusted);

static int __init tpm_init(void)
{
	int rc;

	tpm_class = class_create(THIS_MODULE, "tpm");
	if (IS_ERR(tpm_class)) {
		pr_err("couldn't create tpm class\n");
		return PTR_ERR(tpm_class);
	}

	tpmrm_class = class_create(THIS_MODULE, "tpmrm");
	if (IS_ERR(tpmrm_class)) {
		pr_err("couldn't create tpmrm class\n");
		rc = PTR_ERR(tpmrm_class);
		goto out_destroy_tpm_class;
	}

	rc = alloc_chrdev_region(&tpm_devt, 0, 2*TPM_NUM_DEVICES, "tpm");
	if (rc < 0) {
		pr_err("tpm: failed to allocate char dev region\n");
		goto out_destroy_tpmrm_class;
	}

	rc = tpm_dev_common_init();
	if (rc) {
		pr_err("tpm: failed to allocate char dev region\n");
		goto out_unreg_chrdev;
	}

	return 0;

out_unreg_chrdev:
	unregister_chrdev_region(tpm_devt, 2 * TPM_NUM_DEVICES);
out_destroy_tpmrm_class:
	class_destroy(tpmrm_class);
out_destroy_tpm_class:
	class_destroy(tpm_class);

	return rc;
}

static void __exit tpm_exit(void)
{
	idr_destroy(&dev_nums_idr);
	class_destroy(tpm_class);
	class_destroy(tpmrm_class);
	unregister_chrdev_region(tpm_devt, 2*TPM_NUM_DEVICES);
	tpm_dev_common_exit();
}

subsys_initcall(tpm_init);
module_exit(tpm_exit);

MODULE_AUTHOR("Leendert van Doorn (leendert@watson.ibm.com)");
MODULE_DESCRIPTION("TPM Driver");
MODULE_VERSION("2.0");
MODULE_LICENSE("GPL");
