/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_TIME_H
#define _LINUX_TIME_H

# include <linux/cache.h>
# include <linux/seqlock.h>
# include <linux/math64.h>
# include <linux/time64.h>

extern struct timezone sys_tz;

int get_timespec64(struct timespec64 *ts,
		const struct __kernel_timespec __user *uts);
int put_timespec64(const struct timespec64 *ts,
		struct __kernel_timespec __user *uts);
int get_itimerspec64(struct itimerspec64 *it,
			const struct __kernel_itimerspec __user *uit);
int put_itimerspec64(const struct itimerspec64 *it,
			struct __kernel_itimerspec __user *uit);

extern time64_t mktime64(const unsigned int year, const unsigned int mon,
			const unsigned int day, const unsigned int hour,
			const unsigned int min, const unsigned int sec);

<<<<<<< HEAD
=======
/**
 * Deprecated. Use mktime64().
 */
static inline unsigned long mktime(const unsigned int year,
			const unsigned int mon, const unsigned int day,
			const unsigned int hour, const unsigned int min,
			const unsigned int sec)
{
	return mktime64(year, mon, day, hour, min, sec);
}

extern void set_normalized_timespec(struct timespec *ts, time_t sec, s64 nsec);

/*
 * timespec_add_safe assumes both values are positive and checks
 * for overflow. It will return TIME_T_MAX if the reutrn would be
 * smaller then either of the arguments.
 */
extern struct timespec timespec_add_safe(const struct timespec lhs,
					 const struct timespec rhs);


static inline struct timespec timespec_add(struct timespec lhs,
						struct timespec rhs)
{
	struct timespec ts_delta;
	set_normalized_timespec(&ts_delta, lhs.tv_sec + rhs.tv_sec,
				lhs.tv_nsec + rhs.tv_nsec);
	return ts_delta;
}

/*
 * sub = lhs - rhs, in normalized form
 */
static inline struct timespec timespec_sub(struct timespec lhs,
						struct timespec rhs)
{
	struct timespec ts_delta;
	set_normalized_timespec(&ts_delta, lhs.tv_sec - rhs.tv_sec,
				lhs.tv_nsec - rhs.tv_nsec);
	return ts_delta;
}

/*
 * Returns true if the timespec is norm, false if denorm:
 */
static inline bool timespec_valid(const struct timespec *ts)
{
	/* Dates before 1970 are bogus */
	if (ts->tv_sec < 0)
		return false;
	/* Can't have more nanoseconds then a second */
	if ((unsigned long)ts->tv_nsec >= NSEC_PER_SEC)
		return false;
	return true;
}

static inline bool timespec_valid_strict(const struct timespec *ts)
{
	if (!timespec_valid(ts))
		return false;
	/* Disallow values that could overflow ktime_t */
	if ((unsigned long long)ts->tv_sec >= KTIME_SEC_MAX)
		return false;
	return true;
}

static inline bool timeval_valid(const struct timeval *tv)
{
	/* Dates before 1970 are bogus */
	if (tv->tv_sec < 0)
		return false;

	/* Can't have more microseconds then a second */
	if (tv->tv_usec < 0 || tv->tv_usec >= USEC_PER_SEC)
		return false;

	return true;
}

extern struct timespec timespec_trunc(struct timespec t, unsigned gran);

/*
 * Validates if a timespec/timeval used to inject a time offset is valid.
 * Offsets can be postive or negative. The value of the timeval/timespec
 * is the sum of its fields, but *NOTE*: the field tv_usec/tv_nsec must
 * always be non-negative.
 */
static inline bool timeval_inject_offset_valid(const struct timeval *tv)
{
	/* We don't check the tv_sec as it can be positive or negative */

	/* Can't have more microseconds then a second */
	if (tv->tv_usec < 0 || tv->tv_usec >= USEC_PER_SEC)
		return false;
	return true;
}

static inline bool timespec_inject_offset_valid(const struct timespec *ts)
{
	/* We don't check the tv_sec as it can be positive or negative */

	/* Can't have more nanoseconds then a second */
	if (ts->tv_nsec < 0 || ts->tv_nsec >= NSEC_PER_SEC)
		return false;
	return true;
}

#define CURRENT_TIME		(current_kernel_time())
#define CURRENT_TIME_SEC	((struct timespec) { get_seconds(), 0 })

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
/* Some architectures do not supply their own clocksource.
 * This is mainly the case in architectures that get their
 * inter-tick times by reading the counter on their interval
 * timer. Since these timers wrap every tick, they're not really
 * useful as clocksources. Wrapping them to act like one is possible
 * but not very efficient. So we provide a callout these arches
 * can implement for use with the jiffies clocksource to provide
 * finer then tick granular time.
 */
#ifdef CONFIG_ARCH_USES_GETTIMEOFFSET
extern u32 (*arch_gettimeoffset)(void);
#endif

struct itimerval;
extern int do_setitimer(int which, struct itimerval *value,
			struct itimerval *ovalue);
extern int do_getitimer(int which, struct itimerval *value);

extern long do_utimes(int dfd, const char __user *filename, struct timespec64 *times, int flags);

/*
 * Similar to the struct tm in userspace <time.h>, but it needs to be here so
 * that the kernel source is self contained.
 */
struct tm {
	/*
	 * the number of seconds after the minute, normally in the range
	 * 0 to 59, but can be up to 60 to allow for leap seconds
	 */
	int tm_sec;
	/* the number of minutes after the hour, in the range 0 to 59*/
	int tm_min;
	/* the number of hours past midnight, in the range 0 to 23 */
	int tm_hour;
	/* the day of the month, in the range 1 to 31 */
	int tm_mday;
	/* the number of months since January, in the range 0 to 11 */
	int tm_mon;
	/* the number of years since 1900 */
	long tm_year;
	/* the number of days since Sunday, in the range 0 to 6 */
	int tm_wday;
	/* the number of days since January 1, in the range 0 to 365 */
	int tm_yday;
};

void time64_to_tm(time64_t totalsecs, int offset, struct tm *result);

# include <linux/time32.h>

static inline bool itimerspec64_valid(const struct itimerspec64 *its)
{
	if (!timespec64_valid(&(its->it_interval)) ||
		!timespec64_valid(&(its->it_value)))
		return false;

	return true;
}

/**
 * time_after32 - compare two 32-bit relative times
 * @a:	the time which may be after @b
 * @b:	the time which may be before @a
 *
 * time_after32(a, b) returns true if the time @a is after time @b.
 * time_before32(b, a) returns true if the time @b is before time @a.
 *
 * Similar to time_after(), compare two 32-bit timestamps for relative
 * times.  This is useful for comparing 32-bit seconds values that can't
 * be converted to 64-bit values (e.g. due to disk format or wire protocol
 * issues) when it is known that the times are less than 68 years apart.
 */
#define time_after32(a, b)	((s32)((u32)(b) - (u32)(a)) < 0)
#define time_before32(b, a)	time_after32(a, b)

/**
 * time_between32 - check if a 32-bit timestamp is within a given time range
 * @t:	the time which may be within [l,h]
 * @l:	the lower bound of the range
 * @h:	the higher bound of the range
 *
 * time_before32(t, l, h) returns true if @l <= @t <= @h. All operands are
 * treated as 32-bit integers.
 *
 * Equivalent to !(time_before32(@t, @l) || time_after32(@t, @h)).
 */
#define time_between32(t, l, h) ((u32)(h) - (u32)(l) >= (u32)(t) - (u32)(l))

# include <vdso/time.h>

#endif
