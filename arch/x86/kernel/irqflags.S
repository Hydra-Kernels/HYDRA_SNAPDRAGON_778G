/* SPDX-License-Identifier: GPL-2.0 */

#include <asm/asm.h>
<<<<<<< HEAD
#include <asm/export.h>
=======
#include <asm-generic/export.h>
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#include <linux/linkage.h>

/*
 * unsigned long native_save_fl(void)
 */
ENTRY(native_save_fl)
	pushf
	pop %_ASM_AX
	ret
ENDPROC(native_save_fl)
EXPORT_SYMBOL(native_save_fl)

/*
 * void native_restore_fl(unsigned long flags)
 * %eax/%rdi: flags
 */
ENTRY(native_restore_fl)
	push %_ASM_ARG1
	popf
	ret
ENDPROC(native_restore_fl)
EXPORT_SYMBOL(native_restore_fl)
