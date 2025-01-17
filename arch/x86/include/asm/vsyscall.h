/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_VSYSCALL_H
#define _ASM_X86_VSYSCALL_H

#include <linux/seqlock.h>
#include <uapi/asm/vsyscall.h>

#ifdef CONFIG_X86_VSYSCALL_EMULATION
extern void map_vsyscall(void);
extern void set_vsyscall_pgtable_user_bits(pgd_t *root);

/*
 * Called on instruction fetch fault in vsyscall page.
 * Returns true if handled.
 */
<<<<<<< HEAD
extern bool emulate_vsyscall(unsigned long error_code,
			     struct pt_regs *regs, unsigned long address);
=======
extern bool emulate_vsyscall(struct pt_regs *regs, unsigned long address);
extern bool vsyscall_enabled(void);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#else
static inline void map_vsyscall(void) {}
static inline bool emulate_vsyscall(unsigned long error_code,
				    struct pt_regs *regs, unsigned long address)
{
	return false;
}
static inline bool vsyscall_enabled(void) { return false; }
#endif
extern unsigned long vsyscall_pgprot;

#endif /* _ASM_X86_VSYSCALL_H */
