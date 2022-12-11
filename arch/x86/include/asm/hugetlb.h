/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_HUGETLB_H
#define _ASM_X86_HUGETLB_H

#include <asm/page.h>
#include <asm-generic/hugetlb.h>

<<<<<<< HEAD
#define hugepages_supported() boot_cpu_has(X86_FEATURE_PSE)
=======
#define hugepages_supported() cpu_has_pse
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

static inline int is_hugepage_only_range(struct mm_struct *mm,
					 unsigned long addr,
					 unsigned long len) {
	return 0;
}

static inline void arch_clear_hugepage_flags(struct page *page)
{
}

#endif /* _ASM_X86_HUGETLB_H */
