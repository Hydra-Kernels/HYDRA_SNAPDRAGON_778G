/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _ASM_POWERPC_EXCEPTION_H
#define _ASM_POWERPC_EXCEPTION_H
/*
 * Extracted from head_64.S
 *
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Rewritten by Cort Dougan (cort@cs.nmt.edu) for PReP
 *    Copyright (C) 1996 Cort Dougan <cort@cs.nmt.edu>
 *  Adapted for Power Macintosh by Paul Mackerras.
 *  Low-level exception handlers and MMU support
 *  rewritten by Paul Mackerras.
 *    Copyright (C) 1996 Paul Mackerras.
 *
 *  Adapted for 64bit PowerPC by Dave Engebretsen, Peter Bergner, and
 *    Mike Corrigan {engebret|bergner|mikejc}@us.ibm.com
 *
 *  This file contains the low-level support and setup for the
 *  PowerPC-64 platform, including trap and interrupt dispatch.
 */
/*
 * The following macros define the code that appears as
 * the prologue to each of the exception handlers.  They
 * are split into two parts to allow a single kernel binary
 * to be used for pSeries and iSeries.
 *
 * We make as much of the exception code common between native
 * exception handlers (including pSeries LPAR) and iSeries LPAR
 * implementations as possible.
 */
#include <asm/feature-fixups.h>

<<<<<<< HEAD
/* PACA save area size in u64 units (exgen, exmc, etc) */
=======
#define EX_R9		0
#define EX_R10		8
#define EX_R11		16
#define EX_R12		24
#define EX_R13		32
#define EX_SRR0		40
#define EX_DAR		48
#define EX_DSISR	56
#define EX_CCR		60
#define EX_R3		64
#define EX_LR		72
#define EX_CFAR		80
#define EX_PPR		88	/* SMT thread status register (priority) */
#define EX_CTR		96

/*
 * Macros for annotating the expected destination of (h)rfid
 *
 * The nop instructions allow us to insert one or more instructions to flush the
 * L1-D cache when returning to userspace or a guest.
 */
#define RFI_FLUSH_SLOT							\
	RFI_FLUSH_FIXUP_SECTION;					\
	nop;								\
	nop;								\
	nop

#define RFI_TO_KERNEL							\
	rfid

#define RFI_TO_USER							\
	RFI_FLUSH_SLOT;							\
	rfid;								\
	b	rfi_flush_fallback

#define RFI_TO_USER_OR_KERNEL						\
	RFI_FLUSH_SLOT;							\
	rfid;								\
	b	rfi_flush_fallback

#define RFI_TO_GUEST							\
	RFI_FLUSH_SLOT;							\
	rfid;								\
	b	rfi_flush_fallback

#define HRFI_TO_KERNEL							\
	hrfid

#define HRFI_TO_USER							\
	RFI_FLUSH_SLOT;							\
	hrfid;								\
	b	hrfi_flush_fallback

#define HRFI_TO_USER_OR_KERNEL						\
	RFI_FLUSH_SLOT;							\
	hrfid;								\
	b	hrfi_flush_fallback

#define HRFI_TO_GUEST							\
	RFI_FLUSH_SLOT;							\
	hrfid;								\
	b	hrfi_flush_fallback

#define HRFI_TO_UNKNOWN							\
	RFI_FLUSH_SLOT;							\
	hrfid;								\
	b	hrfi_flush_fallback

#ifdef CONFIG_RELOCATABLE
#define __EXCEPTION_RELON_PROLOG_PSERIES_1(label, h)			\
	ld	r12,PACAKBASE(r13);	/* get high part of &label */	\
	mfspr	r11,SPRN_##h##SRR0;	/* save SRR0 */			\
	LOAD_HANDLER(r12,label);					\
	mtctr	r12;							\
	mfspr	r12,SPRN_##h##SRR1;	/* and SRR1 */			\
	li	r10,MSR_RI;						\
	mtmsrd 	r10,1;			/* Set RI (EE=0) */		\
	bctr;
#else
/* If not relocatable, we can jump directly -- and save messing with LR */
#define __EXCEPTION_RELON_PROLOG_PSERIES_1(label, h)			\
	mfspr	r11,SPRN_##h##SRR0;	/* save SRR0 */			\
	mfspr	r12,SPRN_##h##SRR1;	/* and SRR1 */			\
	li	r10,MSR_RI;						\
	mtmsrd 	r10,1;			/* Set RI (EE=0) */		\
	b	label;
#endif
#define EXCEPTION_RELON_PROLOG_PSERIES_1(label, h)			\
	__EXCEPTION_RELON_PROLOG_PSERIES_1(label, h)			\

/*
 * As EXCEPTION_PROLOG_PSERIES(), except we've already got relocation on
 * so no need to rfid.  Save lr in case we're CONFIG_RELOCATABLE, in which
 * case EXCEPTION_RELON_PROLOG_PSERIES_1 will be using lr.
 */
#define EXCEPTION_RELON_PROLOG_PSERIES(area, label, h, extra, vec)	\
	EXCEPTION_PROLOG_0(area);					\
	EXCEPTION_PROLOG_1(area, extra, vec);				\
	EXCEPTION_RELON_PROLOG_PSERIES_1(label, h)

/*
 * We're short on space and time in the exception prolog, so we can't
 * use the normal SET_REG_IMMEDIATE macro. Normally we just need the
 * low halfword of the address, but for Kdump we need the whole low
 * word.
 */
#define LOAD_HANDLER(reg, label)					\
	/* Handlers must be within 64K of kbase, which must be 64k aligned */ \
	ori	reg,reg,(label)-_stext;	/* virt addr of handler ... */

/* Exception register prefixes */
#define EXC_HV	H
#define EXC_STD

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#if defined(CONFIG_RELOCATABLE)
#define EX_SIZE		10
#else
#define EX_SIZE		9
#endif

/*
 * maximum recursive depth of MCE exceptions
 */
#define MAX_MCE_DEPTH	4

#ifdef __ASSEMBLY__

#define STF_ENTRY_BARRIER_SLOT						\
	STF_ENTRY_BARRIER_FIXUP_SECTION;				\
	nop;								\
	nop;								\
	nop

#define STF_EXIT_BARRIER_SLOT						\
	STF_EXIT_BARRIER_FIXUP_SECTION;					\
	nop;								\
	nop;								\
	nop;								\
	nop;								\
	nop;								\
	nop

#define ENTRY_FLUSH_SLOT						\
	ENTRY_FLUSH_FIXUP_SECTION;					\
	nop;								\
	nop;								\
	nop;

/*
 * r10 must be free to use, r13 must be paca
 */
#define INTERRUPT_TO_KERNEL						\
	STF_ENTRY_BARRIER_SLOT;						\
	ENTRY_FLUSH_SLOT

/*
<<<<<<< HEAD
 * Macros for annotating the expected destination of (h)rfid
=======
 * Get an SPR into a register if the CPU has the given feature
 */
#define OPT_GET_SPR(ra, spr, ftr)					\
BEGIN_FTR_SECTION_NESTED(943)						\
	mfspr	ra,spr;							\
END_FTR_SECTION_NESTED(ftr,ftr,943)

/*
 * Set an SPR from a register if the CPU has the given feature
 */
#define OPT_SET_SPR(ra, spr, ftr)					\
BEGIN_FTR_SECTION_NESTED(943)						\
	mtspr	spr,ra;							\
END_FTR_SECTION_NESTED(ftr,ftr,943)

/*
 * Save a register to the PACA if the CPU has the given feature
 */
#define OPT_SAVE_REG_TO_PACA(offset, ra, ftr)				\
BEGIN_FTR_SECTION_NESTED(943)						\
	std	ra,offset(r13);						\
END_FTR_SECTION_NESTED(ftr,ftr,943)

#define EXCEPTION_PROLOG_0(area)					\
	GET_PACA(r13);							\
	std	r9,area+EX_R9(r13);	/* save r9 */			\
	OPT_GET_SPR(r9, SPRN_PPR, CPU_FTR_HAS_PPR);			\
	HMT_MEDIUM;							\
	std	r10,area+EX_R10(r13);	/* save r10 - r12 */		\
	OPT_GET_SPR(r10, SPRN_CFAR, CPU_FTR_CFAR)

#define __EXCEPTION_PROLOG_1(area, extra, vec)				\
	OPT_SAVE_REG_TO_PACA(area+EX_PPR, r9, CPU_FTR_HAS_PPR);		\
	OPT_SAVE_REG_TO_PACA(area+EX_CFAR, r10, CPU_FTR_CFAR);		\
	SAVE_CTR(r10, area);						\
	mfcr	r9;							\
	extra(vec);							\
	std	r11,area+EX_R11(r13);					\
	std	r12,area+EX_R12(r13);					\
	GET_SCRATCH0(r10);						\
	std	r10,area+EX_R13(r13)
#define EXCEPTION_PROLOG_1(area, extra, vec)				\
	__EXCEPTION_PROLOG_1(area, extra, vec)

#define __EXCEPTION_PROLOG_PSERIES_1(label, h)				\
	ld	r12,PACAKBASE(r13);	/* get high part of &label */	\
	ld	r10,PACAKMSR(r13);	/* get MSR value for kernel */	\
	mfspr	r11,SPRN_##h##SRR0;	/* save SRR0 */			\
	LOAD_HANDLER(r12,label)						\
	mtspr	SPRN_##h##SRR0,r12;					\
	mfspr	r12,SPRN_##h##SRR1;	/* and SRR1 */			\
	mtspr	SPRN_##h##SRR1,r10;					\
	h##RFI_TO_KERNEL;						\
	b	.	/* prevent speculative execution */
#define EXCEPTION_PROLOG_PSERIES_1(label, h)				\
	__EXCEPTION_PROLOG_PSERIES_1(label, h)

#define EXCEPTION_PROLOG_PSERIES(area, label, h, extra, vec)		\
	EXCEPTION_PROLOG_0(area);					\
	EXCEPTION_PROLOG_1(area, extra, vec);				\
	EXCEPTION_PROLOG_PSERIES_1(label, h);

#define __KVMTEST(n)							\
	lbz	r10,HSTATE_IN_GUEST(r13);			\
	cmpwi	r10,0;							\
	bne	do_kvm_##n

#ifdef CONFIG_KVM_BOOK3S_HV_POSSIBLE
/*
 * If hv is possible, interrupts come into to the hv version
 * of the kvmppc_interrupt code, which then jumps to the PR handler,
 * kvmppc_interrupt_pr, if the guest is a PR guest.
 */
#define kvmppc_interrupt kvmppc_interrupt_hv
#else
#define kvmppc_interrupt kvmppc_interrupt_pr
#endif

#define __KVM_HANDLER(area, h, n)					\
do_kvm_##n:								\
	BEGIN_FTR_SECTION_NESTED(947)					\
	ld	r10,area+EX_CFAR(r13);					\
	std	r10,HSTATE_CFAR(r13);					\
	END_FTR_SECTION_NESTED(CPU_FTR_CFAR,CPU_FTR_CFAR,947);		\
	BEGIN_FTR_SECTION_NESTED(948)					\
	ld	r10,area+EX_PPR(r13);					\
	std	r10,HSTATE_PPR(r13);					\
	END_FTR_SECTION_NESTED(CPU_FTR_HAS_PPR,CPU_FTR_HAS_PPR,948);	\
	ld	r10,area+EX_R10(r13);					\
	stw	r9,HSTATE_SCRATCH1(r13);				\
	ld	r9,area+EX_R9(r13);					\
	std	r12,HSTATE_SCRATCH0(r13);				\
	li	r12,n;							\
	b	kvmppc_interrupt

#define __KVM_HANDLER_SKIP(area, h, n)					\
do_kvm_##n:								\
	cmpwi	r10,KVM_GUEST_MODE_SKIP;				\
	ld	r10,area+EX_R10(r13);					\
	beq	89f;							\
	stw	r9,HSTATE_SCRATCH1(r13);			\
	BEGIN_FTR_SECTION_NESTED(948)					\
	ld	r9,area+EX_PPR(r13);					\
	std	r9,HSTATE_PPR(r13);					\
	END_FTR_SECTION_NESTED(CPU_FTR_HAS_PPR,CPU_FTR_HAS_PPR,948);	\
	ld	r9,area+EX_R9(r13);					\
	std	r12,HSTATE_SCRATCH0(r13);			\
	li	r12,n;							\
	b	kvmppc_interrupt;					\
89:	mtocrf	0x80,r9;						\
	ld	r9,area+EX_R9(r13);					\
	b	kvmppc_skip_##h##interrupt

#ifdef CONFIG_KVM_BOOK3S_64_HANDLER
#define KVMTEST(n)			__KVMTEST(n)
#define KVM_HANDLER(area, h, n)		__KVM_HANDLER(area, h, n)
#define KVM_HANDLER_SKIP(area, h, n)	__KVM_HANDLER_SKIP(area, h, n)

#else
#define KVMTEST(n)
#define KVM_HANDLER(area, h, n)
#define KVM_HANDLER_SKIP(area, h, n)
#endif

#ifdef CONFIG_KVM_BOOK3S_PR_POSSIBLE
#define KVMTEST_PR(n)			__KVMTEST(n)
#define KVM_HANDLER_PR(area, h, n)	__KVM_HANDLER(area, h, n)
#define KVM_HANDLER_PR_SKIP(area, h, n)	__KVM_HANDLER_SKIP(area, h, n)

#else
#define KVMTEST_PR(n)
#define KVM_HANDLER_PR(area, h, n)
#define KVM_HANDLER_PR_SKIP(area, h, n)
#endif

#define NOTEST(n)

/*
 * The common exception prolog is used for all except a few exceptions
 * such as a segment miss on a kernel address.  We have to be prepared
 * to take another exception from the point where we first touch the
 * kernel stack onwards.
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
 *
 * The nop instructions allow us to insert one or more instructions to flush the
 * L1-D cache when returning to userspace or a guest.
 */
#define RFI_FLUSH_SLOT							\
	RFI_FLUSH_FIXUP_SECTION;					\
	nop;								\
	nop;								\
	nop

#define RFI_TO_KERNEL							\
	rfid

#define RFI_TO_USER							\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	rfid;								\
	b	rfi_flush_fallback

#define RFI_TO_USER_OR_KERNEL						\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	rfid;								\
	b	rfi_flush_fallback

#define RFI_TO_GUEST							\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	rfid;								\
	b	rfi_flush_fallback

#define HRFI_TO_KERNEL							\
	hrfid

#define HRFI_TO_USER							\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	hrfid;								\
	b	hrfi_flush_fallback

#define HRFI_TO_USER_OR_KERNEL						\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	hrfid;								\
	b	hrfi_flush_fallback

#define HRFI_TO_GUEST							\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	hrfid;								\
	b	hrfi_flush_fallback

#define HRFI_TO_UNKNOWN							\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	hrfid;								\
	b	hrfi_flush_fallback

#else /* __ASSEMBLY__ */
/* Prototype for function defined in exceptions-64s.S */
void do_uaccess_flush(void);
#endif /* __ASSEMBLY__ */

#endif	/* _ASM_POWERPC_EXCEPTION_H */
