// SPDX-License-Identifier: GPL-2.0-only
/*
 * Fast user context implementation of clock_gettime, gettimeofday, and time.
 *
 * Copyright 2006 Andi Kleen, SUSE Labs.
 * Copyright 2019 ARM Limited
 *
 * 32 Bit compat layer by Stefani Seibold <stefani@seibold.net>
 *  sponsored by Rohde & Schwarz GmbH & Co. KG Munich/Germany
 */
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include "../../../../lib/vdso/gettimeofday.c"

extern int __vdso_gettimeofday(struct __kernel_old_timeval *tv, struct timezone *tz);
extern time_t __vdso_time(time_t *t);

int __vdso_gettimeofday(struct __kernel_old_timeval *tv, struct timezone *tz)
{
<<<<<<< HEAD
	return __cvdso_gettimeofday(tv, tz);
}

int gettimeofday(struct __kernel_old_timeval *, struct timezone *)
=======
	return *(const volatile u32 *)(&hpet_page + HPET_COUNTER);
}
#endif

#ifdef CONFIG_PARAVIRT_CLOCK
extern u8 pvclock_page
	__attribute__((visibility("hidden")));
#endif

#ifndef BUILD_VDSO32

#include <linux/kernel.h>
#include <asm/vsyscall.h>
#include <asm/fixmap.h>
#include <asm/pvclock.h>

notrace static long vdso_fallback_gettime(long clock, struct timespec *ts)
{
	long ret;
	asm("syscall" : "=a" (ret) :
	    "0" (__NR_clock_gettime), "D" (clock), "S" (ts) : "memory");
	return ret;
}

notrace static long vdso_fallback_gtod(struct timeval *tv, struct timezone *tz)
{
	long ret;

	asm("syscall" : "=a" (ret) :
	    "0" (__NR_gettimeofday), "D" (tv), "S" (tz) : "memory");
	return ret;
}

#ifdef CONFIG_PARAVIRT_CLOCK

static notrace const struct pvclock_vsyscall_time_info *get_pvti0(void)
{
	return (const struct pvclock_vsyscall_time_info *)&pvclock_page;
}

static notrace cycle_t vread_pvclock(int *mode)
{
	const struct pvclock_vcpu_time_info *pvti = &get_pvti0()->pvti;
	cycle_t ret;
	u64 tsc, pvti_tsc;
	u64 last, delta, pvti_system_time;
	u32 version, pvti_tsc_to_system_mul, pvti_tsc_shift;

	/*
	 * Note: The kernel and hypervisor must guarantee that cpu ID
	 * number maps 1:1 to per-CPU pvclock time info.
	 *
	 * Because the hypervisor is entirely unaware of guest userspace
	 * preemption, it cannot guarantee that per-CPU pvclock time
	 * info is updated if the underlying CPU changes or that that
	 * version is increased whenever underlying CPU changes.
	 *
	 * On KVM, we are guaranteed that pvti updates for any vCPU are
	 * atomic as seen by *all* vCPUs.  This is an even stronger
	 * guarantee than we get with a normal seqlock.
	 *
	 * On Xen, we don't appear to have that guarantee, but Xen still
	 * supplies a valid seqlock using the version field.

	 * We only do pvclock vdso timing at all if
	 * PVCLOCK_TSC_STABLE_BIT is set, and we interpret that bit to
	 * mean that all vCPUs have matching pvti and that the TSC is
	 * synced, so we can just look at vCPU 0's pvti.
	 */

	if (unlikely(!(pvti->flags & PVCLOCK_TSC_STABLE_BIT))) {
		*mode = VCLOCK_NONE;
		return 0;
	}

	do {
		version = pvti->version;

		/* This is also a read barrier, so we'll read version first. */
		tsc = rdtsc_ordered();

		pvti_tsc_to_system_mul = pvti->tsc_to_system_mul;
		pvti_tsc_shift = pvti->tsc_shift;
		pvti_system_time = pvti->system_time;
		pvti_tsc = pvti->tsc_timestamp;

		/* Make sure that the version double-check is last. */
		smp_rmb();
	} while (unlikely((version & 1) || version != pvti->version));

	delta = tsc - pvti_tsc;
	ret = pvti_system_time +
		pvclock_scale_delta(delta, pvti_tsc_to_system_mul,
				    pvti_tsc_shift);

	/* refer to tsc.c read_tsc() comment for rationale */
	last = gtod->cycle_last;

	if (likely(ret >= last))
		return ret;

	return last;
}
#endif

#else

notrace static long vdso_fallback_gettime(long clock, struct timespec *ts)
{
	long ret;

	asm(
		"mov %%ebx, %%edx \n"
		"mov %2, %%ebx \n"
		"call __kernel_vsyscall \n"
		"mov %%edx, %%ebx \n"
		: "=a" (ret)
		: "0" (__NR_clock_gettime), "g" (clock), "c" (ts)
		: "memory", "edx");
	return ret;
}

notrace static long vdso_fallback_gtod(struct timeval *tv, struct timezone *tz)
{
	long ret;

	asm(
		"mov %%ebx, %%edx \n"
		"mov %2, %%ebx \n"
		"call __kernel_vsyscall \n"
		"mov %%edx, %%ebx \n"
		: "=a" (ret)
		: "0" (__NR_gettimeofday), "g" (tv), "c" (tz)
		: "memory", "edx");
	return ret;
}

#ifdef CONFIG_PARAVIRT_CLOCK

static notrace cycle_t vread_pvclock(int *mode)
{
	*mode = VCLOCK_NONE;
	return 0;
}
#endif

#endif

notrace static cycle_t vread_tsc(void)
{
	cycle_t ret = (cycle_t)rdtsc_ordered();
	u64 last = gtod->cycle_last;

	if (likely(ret >= last))
		return ret;

	/*
	 * GCC likes to generate cmov here, but this branch is extremely
	 * predictable (it's just a funciton of time and the likely is
	 * very likely) and there's a data dependence, so force GCC
	 * to generate a branch instead.  I don't barrier() because
	 * we don't actually need a barrier, and if this function
	 * ever gets inlined it will generate worse code.
	 */
	asm volatile ("");
	return last;
}

notrace static inline u64 vgetsns(int *mode)
{
	u64 v;
	cycles_t cycles;

	if (gtod->vclock_mode == VCLOCK_TSC)
		cycles = vread_tsc();
#ifdef CONFIG_HPET_TIMER
	else if (gtod->vclock_mode == VCLOCK_HPET)
		cycles = vread_hpet();
#endif
#ifdef CONFIG_PARAVIRT_CLOCK
	else if (gtod->vclock_mode == VCLOCK_PVCLOCK)
		cycles = vread_pvclock(mode);
#endif
	else
		return 0;
	v = (cycles - gtod->cycle_last) & gtod->mask;
	return v * gtod->mult;
}

/* Code size doesn't matter (vdso is 4k anyway) and this is faster. */
notrace static int __always_inline do_realtime(struct timespec *ts)
{
	unsigned long seq;
	u64 ns;
	int mode;

	do {
		seq = gtod_read_begin(gtod);
		mode = gtod->vclock_mode;
		ts->tv_sec = gtod->wall_time_sec;
		ns = gtod->wall_time_snsec;
		ns += vgetsns(&mode);
		ns >>= gtod->shift;
	} while (unlikely(gtod_read_retry(gtod, seq)));

	ts->tv_sec += __iter_div_u64_rem(ns, NSEC_PER_SEC, &ns);
	ts->tv_nsec = ns;

	return mode;
}

notrace static int __always_inline do_monotonic(struct timespec *ts)
{
	unsigned long seq;
	u64 ns;
	int mode;

	do {
		seq = gtod_read_begin(gtod);
		mode = gtod->vclock_mode;
		ts->tv_sec = gtod->monotonic_time_sec;
		ns = gtod->monotonic_time_snsec;
		ns += vgetsns(&mode);
		ns >>= gtod->shift;
	} while (unlikely(gtod_read_retry(gtod, seq)));

	ts->tv_sec += __iter_div_u64_rem(ns, NSEC_PER_SEC, &ns);
	ts->tv_nsec = ns;

	return mode;
}

notrace static void do_realtime_coarse(struct timespec *ts)
{
	unsigned long seq;
	do {
		seq = gtod_read_begin(gtod);
		ts->tv_sec = gtod->wall_time_coarse_sec;
		ts->tv_nsec = gtod->wall_time_coarse_nsec;
	} while (unlikely(gtod_read_retry(gtod, seq)));
}

notrace static void do_monotonic_coarse(struct timespec *ts)
{
	unsigned long seq;
	do {
		seq = gtod_read_begin(gtod);
		ts->tv_sec = gtod->monotonic_time_coarse_sec;
		ts->tv_nsec = gtod->monotonic_time_coarse_nsec;
	} while (unlikely(gtod_read_retry(gtod, seq)));
}

notrace int __vdso_clock_gettime(clockid_t clock, struct timespec *ts)
{
	switch (clock) {
	case CLOCK_REALTIME:
		if (do_realtime(ts) == VCLOCK_NONE)
			goto fallback;
		break;
	case CLOCK_MONOTONIC:
		if (do_monotonic(ts) == VCLOCK_NONE)
			goto fallback;
		break;
	case CLOCK_REALTIME_COARSE:
		do_realtime_coarse(ts);
		break;
	case CLOCK_MONOTONIC_COARSE:
		do_monotonic_coarse(ts);
		break;
	default:
		goto fallback;
	}

	return 0;
fallback:
	return vdso_fallback_gettime(clock, ts);
}
int clock_gettime(clockid_t, struct timespec *)
	__attribute__((weak, alias("__vdso_clock_gettime")));

notrace int __vdso_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	if (likely(tv != NULL)) {
		if (unlikely(do_realtime((struct timespec *)tv) == VCLOCK_NONE))
			return vdso_fallback_gtod(tv, tz);
		tv->tv_usec /= 1000;
	}
	if (unlikely(tz != NULL)) {
		tz->tz_minuteswest = gtod->tz_minuteswest;
		tz->tz_dsttime = gtod->tz_dsttime;
	}

	return 0;
}
int gettimeofday(struct timeval *, struct timezone *)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	__attribute__((weak, alias("__vdso_gettimeofday")));

time_t __vdso_time(time_t *t)
{
	return __cvdso_time(t);
}

time_t time(time_t *t)	__attribute__((weak, alias("__vdso_time")));


#if defined(CONFIG_X86_64) && !defined(BUILD_VDSO32_64)
/* both 64-bit and x32 use these */
extern int __vdso_clock_gettime(clockid_t clock, struct __kernel_timespec *ts);
extern int __vdso_clock_getres(clockid_t clock, struct __kernel_timespec *res);

int __vdso_clock_gettime(clockid_t clock, struct __kernel_timespec *ts)
{
	return __cvdso_clock_gettime(clock, ts);
}

int clock_gettime(clockid_t, struct __kernel_timespec *)
	__attribute__((weak, alias("__vdso_clock_gettime")));

int __vdso_clock_getres(clockid_t clock,
			struct __kernel_timespec *res)
{
	return __cvdso_clock_getres(clock, res);
}
int clock_getres(clockid_t, struct __kernel_timespec *)
	__attribute__((weak, alias("__vdso_clock_getres")));

#else
/* i386 only */
extern int __vdso_clock_gettime(clockid_t clock, struct old_timespec32 *ts);
extern int __vdso_clock_getres(clockid_t clock, struct old_timespec32 *res);

int __vdso_clock_gettime(clockid_t clock, struct old_timespec32 *ts)
{
	return __cvdso_clock_gettime32(clock, ts);
}

int clock_gettime(clockid_t, struct old_timespec32 *)
	__attribute__((weak, alias("__vdso_clock_gettime")));

int __vdso_clock_gettime64(clockid_t clock, struct __kernel_timespec *ts)
{
	return __cvdso_clock_gettime(clock, ts);
}

int clock_gettime64(clockid_t, struct __kernel_timespec *)
	__attribute__((weak, alias("__vdso_clock_gettime64")));

int __vdso_clock_getres(clockid_t clock, struct old_timespec32 *res)
{
	return __cvdso_clock_getres_time32(clock, res);
}

int clock_getres(clockid_t, struct old_timespec32 *)
	__attribute__((weak, alias("__vdso_clock_getres")));
#endif
