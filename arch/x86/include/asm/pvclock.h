/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_PVCLOCK_H
#define _ASM_X86_PVCLOCK_H

#include <asm/clocksource.h>
#include <asm/pvclock-abi.h>

#ifdef CONFIG_KVM_GUEST
extern struct pvclock_vsyscall_time_info *pvclock_pvti_cpu0_va(void);
#else
static inline struct pvclock_vsyscall_time_info *pvclock_pvti_cpu0_va(void)
{
	return NULL;
}
#endif

/* some helper functions for xen and kvm pv clock sources */
u64 pvclock_clocksource_read(struct pvclock_vcpu_time_info *src);
u8 pvclock_read_flags(struct pvclock_vcpu_time_info *src);
void pvclock_set_flags(u8 flags);
unsigned long pvclock_tsc_khz(struct pvclock_vcpu_time_info *src);
void pvclock_read_wallclock(struct pvclock_wall_clock *wall,
			    struct pvclock_vcpu_time_info *vcpu,
			    struct timespec64 *ts);
void pvclock_resume(void);

void pvclock_touch_watchdogs(void);

static __always_inline
unsigned pvclock_read_begin(const struct pvclock_vcpu_time_info *src)
{
	unsigned version = src->version & ~1;
	/* Make sure that the version is read before the data. */
	virt_rmb();
	return version;
}

static __always_inline
bool pvclock_read_retry(const struct pvclock_vcpu_time_info *src,
			unsigned version)
{
	/* Make sure that the version is re-read after the data. */
	virt_rmb();
	return unlikely(version != src->version);
}

/*
 * Scale a 64-bit delta by scaling and multiplying by a 32-bit fraction,
 * yielding a 64-bit result.
 */
static inline u64 pvclock_scale_delta(u64 delta, u32 mul_frac, int shift)
{
	u64 product;
#ifdef __i386__
	u32 tmp1, tmp2;
#else
	ulong tmp;
#endif

	if (shift < 0)
		delta >>= -shift;
	else
		delta <<= shift;

#ifdef __i386__
	__asm__ (
		"mul  %5       ; "
		"mov  %4,%%eax ; "
		"mov  %%edx,%4 ; "
		"mul  %5       ; "
		"xor  %5,%5    ; "
		"add  %4,%%eax ; "
		"adc  %5,%%edx ; "
		: "=A" (product), "=r" (tmp1), "=r" (tmp2)
		: "a" ((u32)delta), "1" ((u32)(delta >> 32)), "2" (mul_frac) );
#elif defined(__x86_64__)
	__asm__ (
		"mulq %[mul_frac] ; shrd $32, %[hi], %[lo]"
		: [lo]"=a"(product),
		  [hi]"=d"(tmp)
		: "0"(delta),
		  [mul_frac]"rm"((u64)mul_frac));
#else
#error implement me!
#endif

	return product;
}

static __always_inline
u64 __pvclock_read_cycles(const struct pvclock_vcpu_time_info *src, u64 tsc)
{
<<<<<<< HEAD
	u64 delta = tsc - src->tsc_timestamp;
	u64 offset = pvclock_scale_delta(delta, src->tsc_to_system_mul,
					     src->tsc_shift);
	return src->system_time + offset;
=======
	u64 delta = rdtsc_ordered() - src->tsc_timestamp;
	return pvclock_scale_delta(delta, src->tsc_to_system_mul,
				   src->tsc_shift);
}

static __always_inline
unsigned __pvclock_read_cycles(const struct pvclock_vcpu_time_info *src,
			       cycle_t *cycles, u8 *flags)
{
	unsigned version;
	cycle_t ret, offset;
	u8 ret_flags;

	version = src->version;
	/* Make the latest version visible */
	smp_rmb();

	offset = pvclock_get_nsec_offset(src);
	ret = src->system_time + offset;
	ret_flags = src->flags;

	*cycles = ret;
	*flags = ret_flags;
	return version;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}

struct pvclock_vsyscall_time_info {
	struct pvclock_vcpu_time_info pvti;
} __attribute__((__aligned__(SMP_CACHE_BYTES)));

#define PVTI_SIZE sizeof(struct pvclock_vsyscall_time_info)

#ifdef CONFIG_PARAVIRT_CLOCK
void pvclock_set_pvti_cpu0_va(struct pvclock_vsyscall_time_info *pvti);
struct pvclock_vsyscall_time_info *pvclock_get_pvti_cpu0_va(void);
#else
static inline struct pvclock_vsyscall_time_info *pvclock_get_pvti_cpu0_va(void)
{
	return NULL;
}
#endif

#endif /* _ASM_X86_PVCLOCK_H */
