<<<<<<< HEAD
/* SPDX-License-Identifier: GPL-2.0 */
#include <asm/ftrace.h>
#include <linux/uaccess.h>
=======
#include <asm/ftrace.h>
#include <asm/uaccess.h>
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#include <asm/string.h>
#include <asm/page.h>
#include <asm/checksum.h>

#include <asm-generic/asm-prototypes.h>

<<<<<<< HEAD
=======
#include <asm/page.h>
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#include <asm/pgtable.h>
#include <asm/special_insns.h>
#include <asm/preempt.h>
#include <asm/asm.h>

#ifndef CONFIG_X86_CMPXCHG64
extern void cmpxchg8b_emu(void);
#endif

#ifdef CONFIG_RETPOLINE
#ifdef CONFIG_X86_32
#define INDIRECT_THUNK(reg) extern asmlinkage void __x86_indirect_thunk_e ## reg(void);
#else
#define INDIRECT_THUNK(reg) extern asmlinkage void __x86_indirect_thunk_r ## reg(void);
INDIRECT_THUNK(8)
INDIRECT_THUNK(9)
INDIRECT_THUNK(10)
INDIRECT_THUNK(11)
INDIRECT_THUNK(12)
INDIRECT_THUNK(13)
INDIRECT_THUNK(14)
INDIRECT_THUNK(15)
#endif
INDIRECT_THUNK(ax)
INDIRECT_THUNK(bx)
INDIRECT_THUNK(cx)
INDIRECT_THUNK(dx)
INDIRECT_THUNK(si)
INDIRECT_THUNK(di)
INDIRECT_THUNK(bp)
#endif /* CONFIG_RETPOLINE */
