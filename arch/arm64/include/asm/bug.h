/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2015  ARM Limited
 * Author: Dave Martin <Dave.Martin@arm.com>
 */

#ifndef _ARCH_ARM64_ASM_BUG_H
#define _ARCH_ARM64_ASM_BUG_H

#include <linux/stringify.h>

<<<<<<< HEAD
#include <asm/asm-bug.h>

#define __BUG_FLAGS(flags)				\
	asm volatile (__stringify(ASM_BUG_FLAGS(flags)));

=======
#ifdef CONFIG_DEBUG_BUGVERBOSE
#define _BUGVERBOSE_LOCATION(file, line) __BUGVERBOSE_LOCATION(file, line)
#define __BUGVERBOSE_LOCATION(file, line)				\
		".pushsection .rodata.str,\"aMS\",@progbits,1\n"	\
	"2:	.string \"" file "\"\n\t"				\
		".popsection\n\t"					\
									\
		".long 2b - 0b\n\t"					\
		".short " #line "\n\t"
#else
#define _BUGVERBOSE_LOCATION(file, line)
#endif

#ifdef CONFIG_GENERIC_BUG

#define __BUG_ENTRY(flags) 				\
		".pushsection __bug_table,\"a\"\n\t"	\
		".align 2\n\t"				\
	"0:	.long 1f - 0b\n\t"			\
_BUGVERBOSE_LOCATION(__FILE__, __LINE__)		\
		".short " #flags "\n\t"			\
		".popsection\n"				\
	"1:	"
#else
#define __BUG_ENTRY(flags) ""
#endif

#define __BUG_FLAGS(flags)				\
	asm volatile (					\
		__BUG_ENTRY(flags)			\
		"brk %[imm]" :: [imm] "i" (BUG_BRK_IMM)	\
	);


>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#define BUG() do {					\
	__BUG_FLAGS(0);					\
	unreachable();					\
} while (0)

<<<<<<< HEAD
#define __WARN_FLAGS(flags) __BUG_FLAGS(BUGFLAG_WARNING|(flags))
=======
#define __WARN_TAINT(taint) 				\
	__BUG_FLAGS(BUGFLAG_TAINT(taint))
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

#define HAVE_ARCH_BUG

#include <asm-generic/bug.h>

#endif /* ! _ARCH_ARM64_ASM_BUG_H */
