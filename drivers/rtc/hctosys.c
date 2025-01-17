// SPDX-License-Identifier: GPL-2.0
/*
 * RTC subsystem, initialize system time on startup
 *
 * Copyright (C) 2005 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/rtc.h>

/* IMPORTANT: the RTC only stores whole seconds. It is arbitrary
 * whether it stores the most close value or the value with partial
 * seconds truncated. However, it is important that we use it to store
 * the truncated value. This is because otherwise it is necessary,
 * in an rtc sync function, to read both xtime.tv_sec and
 * xtime.tv_nsec. On some processors (i.e. ARM), an atomic read
 * of >32bits is not possible. So storing the most close value would
 * slow down the sync API. So here we have the truncated value and
 * the best guess is to add 0.5s.
 */

int rtc_hctosys(void)
{
	int err = -ENODEV;
	struct rtc_time tm;
	struct timespec64 tv64 = {
		.tv_nsec = NSEC_PER_SEC >> 1,
	};
	struct rtc_device *rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);

	if (!rtc) {
		pr_info("unable to open rtc device (%s)\n",
			CONFIG_RTC_HCTOSYS_DEVICE);
		goto err_open;
	}

	err = rtc_read_time(rtc, &tm);
	if (err) {
		dev_err(rtc->dev.parent,
			"hctosys: unable to read the hardware clock\n");
		goto err_read;
	}

	tv64.tv_sec = rtc_tm_to_time64(&tm);

#if BITS_PER_LONG == 32
<<<<<<< HEAD
	if (tv64.tv_sec > INT_MAX) {
		err = -ERANGE;
		goto err_read;
	}
=======
	if (tv64.tv_sec > INT_MAX)
		goto err_read;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#endif

	err = do_settimeofday64(&tv64);

	dev_info(rtc->dev.parent, "setting system clock to %ptR UTC (%lld)\n",
		 &tm, (long long)tv64.tv_sec);

err_read:
	rtc_class_close(rtc);

err_open:
	rtc_hctosys_ret = err;

	return err;
}
