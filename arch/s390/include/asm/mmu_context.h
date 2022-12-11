/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  S390 version
 *
 *  Derived from "include/asm-i386/mmu_context.h"
 */

#ifndef __S390_MMU_CONTEXT_H
#define __S390_MMU_CONTEXT_H

#include <asm/pgalloc.h>
#include <linux/uaccess.h>
#include <linux/mm_types.h>
#include <asm/tlbflush.h>
#include <asm/ctl_reg.h>
#include <asm-generic/mm_hooks.h>

static inline int init_new_context(struct task_struct *tsk,
				   struct mm_struct *mm)
{
<<<<<<< HEAD
	spin_lock_init(&mm->context.lock);
=======
	spin_lock_init(&mm->context.list_lock);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	INIT_LIST_HEAD(&mm->context.pgtable_list);
	INIT_LIST_HEAD(&mm->context.gmap_list);
	cpumask_clear(&mm->context.cpu_attach_mask);
	atomic_set(&mm->context.flush_count, 0);
	mm->context.gmap_asce = 0;
	mm->context.flush_mm = 0;
<<<<<<< HEAD
	mm->context.compat_mm = test_thread_flag(TIF_31BIT);
=======
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#ifdef CONFIG_PGSTE
	mm->context.alloc_pgste = page_table_allocate_pgste ||
		test_thread_flag(TIF_PGSTE) ||
		(current->mm && current->mm->context.alloc_pgste);
	mm->context.has_pgste = 0;
	mm->context.uses_skeys = 0;
	mm->context.uses_cmm = 0;
	mm->context.allow_gmap_hpage_1m = 0;
#endif
	switch (mm->context.asce_limit) {
<<<<<<< HEAD
	case _REGION2_SIZE:
=======
	case 1UL << 42:
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
		/*
		 * forked 3-level task, fall through to set new asce with new
		 * mm->pgd
		 */
	case 0:
		/* context created by exec, set asce limit to 4TB */
		mm->context.asce_limit = STACK_TOP_MAX;
		mm->context.asce = __pa(mm->pgd) | _ASCE_TABLE_LENGTH |
				   _ASCE_USER_BITS | _ASCE_TYPE_REGION3;
		break;
<<<<<<< HEAD
	case -PAGE_SIZE:
		/* forked 5-level task, set new asce with new_mm->pgd */
		mm->context.asce = __pa(mm->pgd) | _ASCE_TABLE_LENGTH |
			_ASCE_USER_BITS | _ASCE_TYPE_REGION1;
		break;
	case _REGION1_SIZE:
=======
	case 1UL << 53:
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
		/* forked 4-level task, set new asce with new mm->pgd */
		mm->context.asce = __pa(mm->pgd) | _ASCE_TABLE_LENGTH |
				   _ASCE_USER_BITS | _ASCE_TYPE_REGION2;
		break;
<<<<<<< HEAD
	case _REGION3_SIZE:
		/* forked 2-level compat task, set new asce with new mm->pgd */
		mm->context.asce = __pa(mm->pgd) | _ASCE_TABLE_LENGTH |
				   _ASCE_USER_BITS | _ASCE_TYPE_SEGMENT;
=======
	case 1UL << 31:
		/* forked 2-level compat task, set new asce with new mm->pgd */
		mm->context.asce = __pa(mm->pgd) | _ASCE_TABLE_LENGTH |
				   _ASCE_USER_BITS | _ASCE_TYPE_SEGMENT;
		/* pgd_alloc() did not increase mm->nr_pmds */
		mm_inc_nr_pmds(mm);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	}
	crst_table_init((unsigned long *) mm->pgd, pgd_entry_type(mm));
	return 0;
}

#define destroy_context(mm)             do { } while (0)

static inline void set_user_asce(struct mm_struct *mm)
{
	S390_lowcore.user_asce = mm->context.asce;
<<<<<<< HEAD
	__ctl_load(S390_lowcore.user_asce, 1, 1);
	clear_cpu_flag(CIF_ASCE_PRIMARY);
=======
	if (current->thread.mm_segment.ar4)
		__ctl_load(S390_lowcore.user_asce, 7, 7);
	set_cpu_flag(CIF_ASCE);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}

static inline void clear_user_asce(void)
{
	S390_lowcore.user_asce = S390_lowcore.kernel_asce;
	__ctl_load(S390_lowcore.kernel_asce, 1, 1);
	set_cpu_flag(CIF_ASCE_PRIMARY);
}

mm_segment_t enable_sacf_uaccess(void);
void disable_sacf_uaccess(mm_segment_t old_fs);

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			     struct task_struct *tsk)
{
	int cpu = smp_processor_id();

	S390_lowcore.user_asce = next->context.asce;
<<<<<<< HEAD
	cpumask_set_cpu(cpu, &next->context.cpu_attach_mask);
	/* Clear previous user-ASCE from CR1 and CR7 */
	if (!test_cpu_flag(CIF_ASCE_PRIMARY)) {
		__ctl_load(S390_lowcore.kernel_asce, 1, 1);
		set_cpu_flag(CIF_ASCE_PRIMARY);
	}
	if (test_cpu_flag(CIF_ASCE_SECONDARY)) {
		__ctl_load(S390_lowcore.vdso_asce, 7, 7);
		clear_cpu_flag(CIF_ASCE_SECONDARY);
	}
	if (prev != next)
=======
	if (prev == next)
		return;
	if (MACHINE_HAS_TLB_LC)
		cpumask_set_cpu(cpu, &next->context.cpu_attach_mask);
	/* Clear old ASCE by loading the kernel ASCE. */
	__ctl_load(S390_lowcore.kernel_asce, 1, 1);
	__ctl_load(S390_lowcore.kernel_asce, 7, 7);
	atomic_inc(&next->context.attach_count);
	atomic_dec(&prev->context.attach_count);
	if (MACHINE_HAS_TLB_LC)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
		cpumask_clear_cpu(cpu, &prev->context.cpu_attach_mask);
}

#define finish_arch_post_lock_switch finish_arch_post_lock_switch
static inline void finish_arch_post_lock_switch(void)
{
	struct task_struct *tsk = current;
	struct mm_struct *mm = tsk->mm;

	if (mm) {
		preempt_disable();
		while (atomic_read(&mm->context.flush_count))
			cpu_relax();
		cpumask_set_cpu(smp_processor_id(), mm_cpumask(mm));
		__tlb_flush_mm_lazy(mm);
		preempt_enable();
	}
	set_fs(current->thread.mm_segment);
}

#define enter_lazy_tlb(mm,tsk)	do { } while (0)
#define deactivate_mm(tsk,mm)	do { } while (0)

static inline void activate_mm(struct mm_struct *prev,
                               struct mm_struct *next)
{
	switch_mm(prev, next, current);
	cpumask_set_cpu(smp_processor_id(), mm_cpumask(next));
	set_user_asce(next);
}

<<<<<<< HEAD
=======
static inline void arch_dup_mmap(struct mm_struct *oldmm,
				 struct mm_struct *mm)
{
}

static inline void arch_exit_mmap(struct mm_struct *mm)
{
}

static inline void arch_unmap(struct mm_struct *mm,
			struct vm_area_struct *vma,
			unsigned long start, unsigned long end)
{
}

static inline void arch_bprm_mm_init(struct mm_struct *mm,
				     struct vm_area_struct *vma)
{
}

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#endif /* __S390_MMU_CONTEXT_H */
