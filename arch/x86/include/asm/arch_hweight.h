/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_HWEIGHT_H
#define _ASM_X86_HWEIGHT_H

#include <asm/cpufeatures.h>

#ifdef CONFIG_64BIT
<<<<<<< HEAD
=======
/* popcnt %edi, %eax */
#define POPCNT32 ".byte 0xf3,0x0f,0xb8,0xc7"
/* popcnt %rdi, %rax */
#define POPCNT64 ".byte 0xf3,0x48,0x0f,0xb8,0xc7"
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#define REG_IN "D"
#define REG_OUT "a"
#else
#define REG_IN "a"
#define REG_OUT "a"
#endif

<<<<<<< HEAD
=======
#define __HAVE_ARCH_SW_HWEIGHT

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
static __always_inline unsigned int __arch_hweight32(unsigned int w)
{
	unsigned int res;

<<<<<<< HEAD
	asm (ALTERNATIVE("call __sw_hweight32", "popcntl %1, %0", X86_FEATURE_POPCNT)
=======
	asm (ALTERNATIVE("call __sw_hweight32", POPCNT32, X86_FEATURE_POPCNT)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
			 : "="REG_OUT (res)
			 : REG_IN (w));

	return res;
}

static inline unsigned int __arch_hweight16(unsigned int w)
{
	return __arch_hweight32(w & 0xffff);
}

static inline unsigned int __arch_hweight8(unsigned int w)
{
	return __arch_hweight32(w & 0xff);
}

#ifdef CONFIG_X86_32
static inline unsigned long __arch_hweight64(__u64 w)
{
	return  __arch_hweight32((u32)w) +
		__arch_hweight32((u32)(w >> 32));
}
#else
static __always_inline unsigned long __arch_hweight64(__u64 w)
{
	unsigned long res;

<<<<<<< HEAD
	asm (ALTERNATIVE("call __sw_hweight64", "popcntq %1, %0", X86_FEATURE_POPCNT)
=======
	asm (ALTERNATIVE("call __sw_hweight64", POPCNT64, X86_FEATURE_POPCNT)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
			 : "="REG_OUT (res)
			 : REG_IN (w));

	return res;
}
#endif /* CONFIG_X86_32 */

#endif
