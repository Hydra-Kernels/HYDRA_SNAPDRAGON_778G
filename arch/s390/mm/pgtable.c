// SPDX-License-Identifier: GPL-2.0
/*
 *    Copyright IBM Corp. 2007, 2011
 *    Author(s): Martin Schwidefsky <schwidefsky@de.ibm.com>
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/swapops.h>
#include <linux/sysctl.h>
#include <linux/ksm.h>
#include <linux/mman.h>

#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>
#include <asm/mmu_context.h>
#include <asm/page-states.h>

static inline void ptep_ipte_local(struct mm_struct *mm, unsigned long addr,
				   pte_t *ptep, int nodat)
{
	unsigned long opt, asce;

<<<<<<< HEAD
	if (MACHINE_HAS_TLB_GUEST) {
		opt = 0;
		asce = READ_ONCE(mm->context.gmap_asce);
		if (asce == 0UL || nodat)
			opt |= IPTE_NODAT;
		if (asce != -1UL) {
			asce = asce ? : mm->context.asce;
			opt |= IPTE_GUEST_ASCE;
		}
		__ptep_ipte(addr, ptep, opt, asce, IPTE_LOCAL);
	} else {
		__ptep_ipte(addr, ptep, 0, 0, IPTE_LOCAL);
	}
}

static inline void ptep_ipte_global(struct mm_struct *mm, unsigned long addr,
				    pte_t *ptep, int nodat)
=======
	if (!page)
		return NULL;
	return (unsigned long *) page_to_phys(page);
}

void crst_table_free(struct mm_struct *mm, unsigned long *table)
{
	free_pages((unsigned long) table, 2);
}

static void __crst_table_upgrade(void *arg)
{
	struct mm_struct *mm = arg;

	if (current->active_mm == mm) {
		clear_user_asce();
		set_user_asce(mm);
	}
	__tlb_flush_local();
}

int crst_table_upgrade(struct mm_struct *mm)
{
	unsigned long *table, *pgd;

	/* upgrade should only happen from 3 to 4 levels */
	BUG_ON(mm->context.asce_limit != (1UL << 42));

	table = crst_table_alloc(mm);
	if (!table)
		return -ENOMEM;

	spin_lock_bh(&mm->page_table_lock);
	pgd = (unsigned long *) mm->pgd;
	crst_table_init(table, _REGION2_ENTRY_EMPTY);
	pgd_populate(mm, (pgd_t *) table, (pud_t *) pgd);
	mm->pgd = (pgd_t *) table;
	mm->context.asce_limit = 1UL << 53;
	mm->context.asce = __pa(mm->pgd) | _ASCE_TABLE_LENGTH |
			   _ASCE_USER_BITS | _ASCE_TYPE_REGION2;
	mm->task_size = mm->context.asce_limit;
	spin_unlock_bh(&mm->page_table_lock);

	on_each_cpu(__crst_table_upgrade, mm, 0);
	return 0;
}

void crst_table_downgrade(struct mm_struct *mm)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
{
	unsigned long opt, asce;

<<<<<<< HEAD
	if (MACHINE_HAS_TLB_GUEST) {
		opt = 0;
		asce = READ_ONCE(mm->context.gmap_asce);
		if (asce == 0UL || nodat)
			opt |= IPTE_NODAT;
		if (asce != -1UL) {
			asce = asce ? : mm->context.asce;
			opt |= IPTE_GUEST_ASCE;
		}
		__ptep_ipte(addr, ptep, opt, asce, IPTE_GLOBAL);
	} else {
		__ptep_ipte(addr, ptep, 0, 0, IPTE_GLOBAL);
	}
}

static inline pte_t ptep_flush_direct(struct mm_struct *mm,
				      unsigned long addr, pte_t *ptep,
				      int nodat)
{
	pte_t old;

	old = *ptep;
	if (unlikely(pte_val(old) & _PAGE_INVALID))
		return old;
	atomic_inc(&mm->context.flush_count);
	if (MACHINE_HAS_TLB_LC &&
	    cpumask_equal(mm_cpumask(mm), cpumask_of(smp_processor_id())))
		ptep_ipte_local(mm, addr, ptep, nodat);
	else
		ptep_ipte_global(mm, addr, ptep, nodat);
	atomic_dec(&mm->context.flush_count);
	return old;
}

static inline pte_t ptep_flush_lazy(struct mm_struct *mm,
				    unsigned long addr, pte_t *ptep,
				    int nodat)
{
	pte_t old;

	old = *ptep;
	if (unlikely(pte_val(old) & _PAGE_INVALID))
		return old;
	atomic_inc(&mm->context.flush_count);
	if (cpumask_equal(&mm->context.cpu_attach_mask,
			  cpumask_of(smp_processor_id()))) {
		pte_val(*ptep) |= _PAGE_INVALID;
		mm->context.flush_mm = 1;
	} else
		ptep_ipte_global(mm, addr, ptep, nodat);
	atomic_dec(&mm->context.flush_count);
	return old;
}

static inline pgste_t pgste_get_lock(pte_t *ptep)
{
	unsigned long new = 0;
#ifdef CONFIG_PGSTE
	unsigned long old;

	asm(
		"	lg	%0,%2\n"
		"0:	lgr	%1,%0\n"
		"	nihh	%0,0xff7f\n"	/* clear PCL bit in old */
		"	oihh	%1,0x0080\n"	/* set PCL bit in new */
		"	csg	%0,%1,%2\n"
		"	jl	0b\n"
		: "=&d" (old), "=&d" (new), "=Q" (ptep[PTRS_PER_PTE])
		: "Q" (ptep[PTRS_PER_PTE]) : "cc", "memory");
#endif
	return __pgste(new);
}

static inline void pgste_set_unlock(pte_t *ptep, pgste_t pgste)
{
#ifdef CONFIG_PGSTE
	asm(
		"	nihh	%1,0xff7f\n"	/* clear PCL bit */
		"	stg	%1,%0\n"
		: "=Q" (ptep[PTRS_PER_PTE])
		: "d" (pgste_val(pgste)), "Q" (ptep[PTRS_PER_PTE])
		: "cc", "memory");
#endif
}

static inline pgste_t pgste_get(pte_t *ptep)
{
	unsigned long pgste = 0;
#ifdef CONFIG_PGSTE
	pgste = *(unsigned long *)(ptep + PTRS_PER_PTE);
#endif
	return __pgste(pgste);
}

static inline void pgste_set(pte_t *ptep, pgste_t pgste)
{
#ifdef CONFIG_PGSTE
	*(pgste_t *)(ptep + PTRS_PER_PTE) = pgste;
#endif
}

static inline pgste_t pgste_update_all(pte_t pte, pgste_t pgste,
				       struct mm_struct *mm)
{
#ifdef CONFIG_PGSTE
	unsigned long address, bits, skey;

	if (!mm_uses_skeys(mm) || pte_val(pte) & _PAGE_INVALID)
		return pgste;
	address = pte_val(pte) & PAGE_MASK;
	skey = (unsigned long) page_get_storage_key(address);
	bits = skey & (_PAGE_CHANGED | _PAGE_REFERENCED);
	/* Transfer page changed & referenced bit to guest bits in pgste */
	pgste_val(pgste) |= bits << 48;		/* GR bit & GC bit */
	/* Copy page access key and fetch protection bit to pgste */
	pgste_val(pgste) &= ~(PGSTE_ACC_BITS | PGSTE_FP_BIT);
	pgste_val(pgste) |= (skey & (_PAGE_ACC_BITS | _PAGE_FP_BIT)) << 56;
#endif
	return pgste;

}

static inline void pgste_set_key(pte_t *ptep, pgste_t pgste, pte_t entry,
				 struct mm_struct *mm)
{
#ifdef CONFIG_PGSTE
	unsigned long address;
	unsigned long nkey;

	if (!mm_uses_skeys(mm) || pte_val(entry) & _PAGE_INVALID)
		return;
	VM_BUG_ON(!(pte_val(*ptep) & _PAGE_INVALID));
	address = pte_val(entry) & PAGE_MASK;
	/*
	 * Set page access key and fetch protection bit from pgste.
	 * The guest C/R information is still in the PGSTE, set real
	 * key C/R to 0.
	 */
	nkey = (pgste_val(pgste) & (PGSTE_ACC_BITS | PGSTE_FP_BIT)) >> 56;
	nkey |= (pgste_val(pgste) & (PGSTE_GR_BIT | PGSTE_GC_BIT)) >> 48;
	page_set_storage_key(address, nkey, 0);
#endif
}

static inline pgste_t pgste_set_pte(pte_t *ptep, pgste_t pgste, pte_t entry)
{
#ifdef CONFIG_PGSTE
	if ((pte_val(entry) & _PAGE_PRESENT) &&
	    (pte_val(entry) & _PAGE_WRITE) &&
	    !(pte_val(entry) & _PAGE_INVALID)) {
		if (!MACHINE_HAS_ESOP) {
			/*
			 * Without enhanced suppression-on-protection force
			 * the dirty bit on for all writable ptes.
			 */
			pte_val(entry) |= _PAGE_DIRTY;
			pte_val(entry) &= ~_PAGE_PROTECT;
		}
		if (!(pte_val(entry) & _PAGE_PROTECT))
			/* This pte allows write access, set user-dirty */
			pgste_val(pgste) |= PGSTE_UC_BIT;
	}
#endif
	*ptep = entry;
	return pgste;
}

static inline pgste_t pgste_pte_notify(struct mm_struct *mm,
				       unsigned long addr,
				       pte_t *ptep, pgste_t pgste)
{
#ifdef CONFIG_PGSTE
	unsigned long bits;

	bits = pgste_val(pgste) & (PGSTE_IN_BIT | PGSTE_VSIE_BIT);
	if (bits) {
		pgste_val(pgste) ^= bits;
		ptep_notify(mm, addr, ptep, bits);
	}
#endif
	return pgste;
}

static inline pgste_t ptep_xchg_start(struct mm_struct *mm,
				      unsigned long addr, pte_t *ptep)
{
	pgste_t pgste = __pgste(0);

	if (mm_has_pgste(mm)) {
		pgste = pgste_get_lock(ptep);
		pgste = pgste_pte_notify(mm, addr, ptep, pgste);
	}
	return pgste;
}

static inline pte_t ptep_xchg_commit(struct mm_struct *mm,
				    unsigned long addr, pte_t *ptep,
				    pgste_t pgste, pte_t old, pte_t new)
{
	if (mm_has_pgste(mm)) {
		if (pte_val(old) & _PAGE_INVALID)
			pgste_set_key(ptep, pgste, new, mm);
		if (pte_val(new) & _PAGE_INVALID) {
			pgste = pgste_update_all(old, pgste, mm);
			if ((pgste_val(pgste) & _PGSTE_GPS_USAGE_MASK) ==
			    _PGSTE_GPS_USAGE_UNUSED)
				pte_val(old) |= _PAGE_UNUSED;
		}
		pgste = pgste_set_pte(ptep, pgste, new);
		pgste_set_unlock(ptep, pgste);
	} else {
		*ptep = new;
	}
	return old;
}

pte_t ptep_xchg_direct(struct mm_struct *mm, unsigned long addr,
		       pte_t *ptep, pte_t new)
{
	pgste_t pgste;
	pte_t old;
	int nodat;

	preempt_disable();
	pgste = ptep_xchg_start(mm, addr, ptep);
	nodat = !!(pgste_val(pgste) & _PGSTE_GPS_NODAT);
	old = ptep_flush_direct(mm, addr, ptep, nodat);
	old = ptep_xchg_commit(mm, addr, ptep, pgste, old, new);
	preempt_enable();
	return old;
}
EXPORT_SYMBOL(ptep_xchg_direct);

pte_t ptep_xchg_lazy(struct mm_struct *mm, unsigned long addr,
		     pte_t *ptep, pte_t new)
{
	pgste_t pgste;
	pte_t old;
	int nodat;

	preempt_disable();
	pgste = ptep_xchg_start(mm, addr, ptep);
	nodat = !!(pgste_val(pgste) & _PGSTE_GPS_NODAT);
	old = ptep_flush_lazy(mm, addr, ptep, nodat);
	old = ptep_xchg_commit(mm, addr, ptep, pgste, old, new);
	preempt_enable();
	return old;
}
EXPORT_SYMBOL(ptep_xchg_lazy);

pte_t ptep_modify_prot_start(struct vm_area_struct *vma, unsigned long addr,
			     pte_t *ptep)
{
	pgste_t pgste;
	pte_t old;
	int nodat;
	struct mm_struct *mm = vma->vm_mm;

	preempt_disable();
	pgste = ptep_xchg_start(mm, addr, ptep);
	nodat = !!(pgste_val(pgste) & _PGSTE_GPS_NODAT);
	old = ptep_flush_lazy(mm, addr, ptep, nodat);
	if (mm_has_pgste(mm)) {
		pgste = pgste_update_all(old, pgste, mm);
		pgste_set(ptep, pgste);
	}
	return old;
}

void ptep_modify_prot_commit(struct vm_area_struct *vma, unsigned long addr,
			     pte_t *ptep, pte_t old_pte, pte_t pte)
{
	pgste_t pgste;
	struct mm_struct *mm = vma->vm_mm;

	if (!MACHINE_HAS_NX)
		pte_val(pte) &= ~_PAGE_NOEXEC;
	if (mm_has_pgste(mm)) {
		pgste = pgste_get(ptep);
		pgste_set_key(ptep, pgste, pte, mm);
		pgste = pgste_set_pte(ptep, pgste, pte);
		pgste_set_unlock(ptep, pgste);
	} else {
		*ptep = pte;
	}
	preempt_enable();
}

static inline void pmdp_idte_local(struct mm_struct *mm,
				   unsigned long addr, pmd_t *pmdp)
{
	if (MACHINE_HAS_TLB_GUEST)
		__pmdp_idte(addr, pmdp, IDTE_NODAT | IDTE_GUEST_ASCE,
			    mm->context.asce, IDTE_LOCAL);
	else
		__pmdp_idte(addr, pmdp, 0, 0, IDTE_LOCAL);
	if (mm_has_pgste(mm) && mm->context.allow_gmap_hpage_1m)
		gmap_pmdp_idte_local(mm, addr);
}

static inline void pmdp_idte_global(struct mm_struct *mm,
				    unsigned long addr, pmd_t *pmdp)
{
	if (MACHINE_HAS_TLB_GUEST) {
		__pmdp_idte(addr, pmdp, IDTE_NODAT | IDTE_GUEST_ASCE,
			    mm->context.asce, IDTE_GLOBAL);
		if (mm_has_pgste(mm) && mm->context.allow_gmap_hpage_1m)
			gmap_pmdp_idte_global(mm, addr);
	} else if (MACHINE_HAS_IDTE) {
		__pmdp_idte(addr, pmdp, 0, 0, IDTE_GLOBAL);
		if (mm_has_pgste(mm) && mm->context.allow_gmap_hpage_1m)
			gmap_pmdp_idte_global(mm, addr);
	} else {
		__pmdp_csp(pmdp);
		if (mm_has_pgste(mm) && mm->context.allow_gmap_hpage_1m)
			gmap_pmdp_csp(mm, addr);
	}
}

static inline pmd_t pmdp_flush_direct(struct mm_struct *mm,
				      unsigned long addr, pmd_t *pmdp)
{
	pmd_t old;

	old = *pmdp;
	if (pmd_val(old) & _SEGMENT_ENTRY_INVALID)
		return old;
	atomic_inc(&mm->context.flush_count);
	if (MACHINE_HAS_TLB_LC &&
	    cpumask_equal(mm_cpumask(mm), cpumask_of(smp_processor_id())))
		pmdp_idte_local(mm, addr, pmdp);
	else
		pmdp_idte_global(mm, addr, pmdp);
	atomic_dec(&mm->context.flush_count);
	return old;
}

static inline pmd_t pmdp_flush_lazy(struct mm_struct *mm,
				    unsigned long addr, pmd_t *pmdp)
{
	pmd_t old;

	old = *pmdp;
	if (pmd_val(old) & _SEGMENT_ENTRY_INVALID)
		return old;
	atomic_inc(&mm->context.flush_count);
	if (cpumask_equal(&mm->context.cpu_attach_mask,
			  cpumask_of(smp_processor_id()))) {
		pmd_val(*pmdp) |= _SEGMENT_ENTRY_INVALID;
		mm->context.flush_mm = 1;
		if (mm_has_pgste(mm))
			gmap_pmdp_invalidate(mm, addr);
	} else {
		pmdp_idte_global(mm, addr, pmdp);
	}
	atomic_dec(&mm->context.flush_count);
	return old;
=======
	/* downgrade should only happen from 3 to 2 levels (compat only) */
	BUG_ON(mm->context.asce_limit != (1UL << 42));

	if (current->active_mm == mm) {
		clear_user_asce();
		__tlb_flush_mm(mm);
	}

	pgd = mm->pgd;
	mm->pgd = (pgd_t *) (pgd_val(*pgd) & _REGION_ENTRY_ORIGIN);
	mm->context.asce_limit = 1UL << 31;
	mm->context.asce = __pa(mm->pgd) | _ASCE_TABLE_LENGTH |
			   _ASCE_USER_BITS | _ASCE_TYPE_SEGMENT;
	mm->task_size = mm->context.asce_limit;
	crst_table_free(mm, (unsigned long *) pgd);

	if (current->active_mm == mm)
		set_user_asce(mm);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}

#ifdef CONFIG_PGSTE
static pmd_t *pmd_alloc_map(struct mm_struct *mm, unsigned long addr)
{
<<<<<<< HEAD
=======
	struct gmap *gmap;
	struct page *page;
	unsigned long *table;
	unsigned long etype, atype;

	if (limit < (1UL << 31)) {
		limit = (1UL << 31) - 1;
		atype = _ASCE_TYPE_SEGMENT;
		etype = _SEGMENT_ENTRY_EMPTY;
	} else if (limit < (1UL << 42)) {
		limit = (1UL << 42) - 1;
		atype = _ASCE_TYPE_REGION3;
		etype = _REGION3_ENTRY_EMPTY;
	} else if (limit < (1UL << 53)) {
		limit = (1UL << 53) - 1;
		atype = _ASCE_TYPE_REGION2;
		etype = _REGION2_ENTRY_EMPTY;
	} else {
		limit = -1UL;
		atype = _ASCE_TYPE_REGION1;
		etype = _REGION1_ENTRY_EMPTY;
	}
	gmap = kzalloc(sizeof(struct gmap), GFP_KERNEL);
	if (!gmap)
		goto out;
	INIT_LIST_HEAD(&gmap->crst_list);
	INIT_RADIX_TREE(&gmap->guest_to_host, GFP_KERNEL);
	INIT_RADIX_TREE(&gmap->host_to_guest, GFP_ATOMIC);
	spin_lock_init(&gmap->guest_table_lock);
	gmap->mm = mm;
	page = alloc_pages(GFP_KERNEL, 2);
	if (!page)
		goto out_free;
	page->index = 0;
	list_add(&page->lru, &gmap->crst_list);
	table = (unsigned long *) page_to_phys(page);
	crst_table_init(table, etype);
	gmap->table = table;
	gmap->asce = atype | _ASCE_TABLE_LENGTH |
		_ASCE_USER_BITS | __pa(table);
	gmap->asce_end = limit;
	down_write(&mm->mmap_sem);
	list_add(&gmap->list, &mm->context.gmap_list);
	up_write(&mm->mmap_sem);
	return gmap;

out_free:
	kfree(gmap);
out:
	return NULL;
}
EXPORT_SYMBOL_GPL(gmap_alloc);

static void gmap_flush_tlb(struct gmap *gmap)
{
	if (MACHINE_HAS_IDTE)
		__tlb_flush_idte(gmap->asce);
	else
		__tlb_flush_global();
}

static void gmap_radix_tree_free(struct radix_tree_root *root)
{
	struct radix_tree_iter iter;
	unsigned long indices[16];
	unsigned long index;
	void **slot;
	int i, nr;

	/* A radix tree is freed by deleting all of its entries */
	index = 0;
	do {
		nr = 0;
		radix_tree_for_each_slot(slot, root, &iter, index) {
			indices[nr] = iter.index;
			if (++nr == 16)
				break;
		}
		for (i = 0; i < nr; i++) {
			index = indices[i];
			radix_tree_delete(root, index);
		}
	} while (nr > 0);
}

/**
 * gmap_free - free a guest address space
 * @gmap: pointer to the guest address space structure
 */
void gmap_free(struct gmap *gmap)
{
	struct page *page, *next;

	/* Flush tlb. */
	if (MACHINE_HAS_IDTE)
		__tlb_flush_idte(gmap->asce);
	else
		__tlb_flush_global();

	/* Free all segment & region tables. */
	list_for_each_entry_safe(page, next, &gmap->crst_list, lru)
		__free_pages(page, 2);
	gmap_radix_tree_free(&gmap->guest_to_host);
	gmap_radix_tree_free(&gmap->host_to_guest);
	down_write(&gmap->mm->mmap_sem);
	list_del(&gmap->list);
	up_write(&gmap->mm->mmap_sem);
	kfree(gmap);
}
EXPORT_SYMBOL_GPL(gmap_free);

/**
 * gmap_enable - switch primary space to the guest address space
 * @gmap: pointer to the guest address space structure
 */
void gmap_enable(struct gmap *gmap)
{
	S390_lowcore.gmap = (unsigned long) gmap;
}
EXPORT_SYMBOL_GPL(gmap_enable);

/**
 * gmap_disable - switch back to the standard primary address space
 * @gmap: pointer to the guest address space structure
 */
void gmap_disable(struct gmap *gmap)
{
	S390_lowcore.gmap = 0UL;
}
EXPORT_SYMBOL_GPL(gmap_disable);

/*
 * gmap_alloc_table is assumed to be called with mmap_sem held
 */
static int gmap_alloc_table(struct gmap *gmap, unsigned long *table,
			    unsigned long init, unsigned long gaddr)
{
	struct page *page;
	unsigned long *new;

	/* since we dont free the gmap table until gmap_free we can unlock */
	page = alloc_pages(GFP_KERNEL, 2);
	if (!page)
		return -ENOMEM;
	new = (unsigned long *) page_to_phys(page);
	crst_table_init(new, init);
	spin_lock(&gmap->mm->page_table_lock);
	if (*table & _REGION_ENTRY_INVALID) {
		list_add(&page->lru, &gmap->crst_list);
		*table = (unsigned long) new | _REGION_ENTRY_LENGTH |
			(*table & _REGION_ENTRY_TYPE_MASK);
		page->index = gaddr;
		page = NULL;
	}
	spin_unlock(&gmap->mm->page_table_lock);
	if (page)
		__free_pages(page, 2);
	return 0;
}

/**
 * __gmap_segment_gaddr - find virtual address from segment pointer
 * @entry: pointer to a segment table entry in the guest address space
 *
 * Returns the virtual address in the guest address space for the segment
 */
static unsigned long __gmap_segment_gaddr(unsigned long *entry)
{
	struct page *page;
	unsigned long offset, mask;

	offset = (unsigned long) entry / sizeof(unsigned long);
	offset = (offset & (PTRS_PER_PMD - 1)) * PMD_SIZE;
	mask = ~(PTRS_PER_PMD * sizeof(pmd_t) - 1);
	page = virt_to_page((void *)((unsigned long) entry & mask));
	return page->index + offset;
}

/**
 * __gmap_unlink_by_vmaddr - unlink a single segment via a host address
 * @gmap: pointer to the guest address space structure
 * @vmaddr: address in the host process address space
 *
 * Returns 1 if a TLB flush is required
 */
static int __gmap_unlink_by_vmaddr(struct gmap *gmap, unsigned long vmaddr)
{
	unsigned long *entry;
	int flush = 0;

	spin_lock(&gmap->guest_table_lock);
	entry = radix_tree_delete(&gmap->host_to_guest, vmaddr >> PMD_SHIFT);
	if (entry) {
		flush = (*entry != _SEGMENT_ENTRY_INVALID);
		*entry = _SEGMENT_ENTRY_INVALID;
	}
	spin_unlock(&gmap->guest_table_lock);
	return flush;
}

/**
 * __gmap_unmap_by_gaddr - unmap a single segment via a guest address
 * @gmap: pointer to the guest address space structure
 * @gaddr: address in the guest address space
 *
 * Returns 1 if a TLB flush is required
 */
static int __gmap_unmap_by_gaddr(struct gmap *gmap, unsigned long gaddr)
{
	unsigned long vmaddr;

	vmaddr = (unsigned long) radix_tree_delete(&gmap->guest_to_host,
						   gaddr >> PMD_SHIFT);
	return vmaddr ? __gmap_unlink_by_vmaddr(gmap, vmaddr) : 0;
}

/**
 * gmap_unmap_segment - unmap segment from the guest address space
 * @gmap: pointer to the guest address space structure
 * @to: address in the guest address space
 * @len: length of the memory area to unmap
 *
 * Returns 0 if the unmap succeeded, -EINVAL if not.
 */
int gmap_unmap_segment(struct gmap *gmap, unsigned long to, unsigned long len)
{
	unsigned long off;
	int flush;

	if ((to | len) & (PMD_SIZE - 1))
		return -EINVAL;
	if (len == 0 || to + len < to)
		return -EINVAL;

	flush = 0;
	down_write(&gmap->mm->mmap_sem);
	for (off = 0; off < len; off += PMD_SIZE)
		flush |= __gmap_unmap_by_gaddr(gmap, to + off);
	up_write(&gmap->mm->mmap_sem);
	if (flush)
		gmap_flush_tlb(gmap);
	return 0;
}
EXPORT_SYMBOL_GPL(gmap_unmap_segment);

/**
 * gmap_mmap_segment - map a segment to the guest address space
 * @gmap: pointer to the guest address space structure
 * @from: source address in the parent address space
 * @to: target address in the guest address space
 * @len: length of the memory area to map
 *
 * Returns 0 if the mmap succeeded, -EINVAL or -ENOMEM if not.
 */
int gmap_map_segment(struct gmap *gmap, unsigned long from,
		     unsigned long to, unsigned long len)
{
	unsigned long off;
	int flush;

	if ((from | to | len) & (PMD_SIZE - 1))
		return -EINVAL;
	if (len == 0 || from + len < from || to + len < to ||
	    from + len > TASK_MAX_SIZE || to + len > gmap->asce_end)
		return -EINVAL;

	flush = 0;
	down_write(&gmap->mm->mmap_sem);
	for (off = 0; off < len; off += PMD_SIZE) {
		/* Remove old translation */
		flush |= __gmap_unmap_by_gaddr(gmap, to + off);
		/* Store new translation */
		if (radix_tree_insert(&gmap->guest_to_host,
				      (to + off) >> PMD_SHIFT,
				      (void *) from + off))
			break;
	}
	up_write(&gmap->mm->mmap_sem);
	if (flush)
		gmap_flush_tlb(gmap);
	if (off >= len)
		return 0;
	gmap_unmap_segment(gmap, to, len);
	return -ENOMEM;
}
EXPORT_SYMBOL_GPL(gmap_map_segment);

/**
 * __gmap_translate - translate a guest address to a user space address
 * @gmap: pointer to guest mapping meta data structure
 * @gaddr: guest address
 *
 * Returns user space address which corresponds to the guest address or
 * -EFAULT if no such mapping exists.
 * This function does not establish potentially missing page table entries.
 * The mmap_sem of the mm that belongs to the address space must be held
 * when this function gets called.
 */
unsigned long __gmap_translate(struct gmap *gmap, unsigned long gaddr)
{
	unsigned long vmaddr;

	vmaddr = (unsigned long)
		radix_tree_lookup(&gmap->guest_to_host, gaddr >> PMD_SHIFT);
	return vmaddr ? (vmaddr | (gaddr & ~PMD_MASK)) : -EFAULT;
}
EXPORT_SYMBOL_GPL(__gmap_translate);

/**
 * gmap_translate - translate a guest address to a user space address
 * @gmap: pointer to guest mapping meta data structure
 * @gaddr: guest address
 *
 * Returns user space address which corresponds to the guest address or
 * -EFAULT if no such mapping exists.
 * This function does not establish potentially missing page table entries.
 */
unsigned long gmap_translate(struct gmap *gmap, unsigned long gaddr)
{
	unsigned long rc;

	down_read(&gmap->mm->mmap_sem);
	rc = __gmap_translate(gmap, gaddr);
	up_read(&gmap->mm->mmap_sem);
	return rc;
}
EXPORT_SYMBOL_GPL(gmap_translate);

/**
 * gmap_unlink - disconnect a page table from the gmap shadow tables
 * @gmap: pointer to guest mapping meta data structure
 * @table: pointer to the host page table
 * @vmaddr: vm address associated with the host page table
 */
static void gmap_unlink(struct mm_struct *mm, unsigned long *table,
			unsigned long vmaddr)
{
	struct gmap *gmap;
	int flush;

	list_for_each_entry(gmap, &mm->context.gmap_list, list) {
		flush = __gmap_unlink_by_vmaddr(gmap, vmaddr);
		if (flush)
			gmap_flush_tlb(gmap);
	}
}

/**
 * gmap_link - set up shadow page tables to connect a host to a guest address
 * @gmap: pointer to guest mapping meta data structure
 * @gaddr: guest address
 * @vmaddr: vm address
 *
 * Returns 0 on success, -ENOMEM for out of memory conditions, and -EFAULT
 * if the vm address is already mapped to a different guest segment.
 * The mmap_sem of the mm that belongs to the address space must be held
 * when this function gets called.
 */
int __gmap_link(struct gmap *gmap, unsigned long gaddr, unsigned long vmaddr)
{
	struct mm_struct *mm;
	unsigned long *table;
	spinlock_t *ptl;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;

	pgd = pgd_offset(mm, addr);
	p4d = p4d_alloc(mm, pgd, addr);
	if (!p4d)
		return NULL;
	pud = pud_alloc(mm, p4d, addr);
	if (!pud)
		return NULL;
	pmd = pmd_alloc(mm, pud, addr);
	return pmd;
}
#endif

pmd_t pmdp_xchg_direct(struct mm_struct *mm, unsigned long addr,
		       pmd_t *pmdp, pmd_t new)
{
	pmd_t old;

	preempt_disable();
	old = pmdp_flush_direct(mm, addr, pmdp);
	*pmdp = new;
	preempt_enable();
	return old;
}
EXPORT_SYMBOL(pmdp_xchg_direct);

pmd_t pmdp_xchg_lazy(struct mm_struct *mm, unsigned long addr,
		     pmd_t *pmdp, pmd_t new)
{
	pmd_t old;

	preempt_disable();
	old = pmdp_flush_lazy(mm, addr, pmdp);
	*pmdp = new;
	preempt_enable();
	return old;
}
EXPORT_SYMBOL(pmdp_xchg_lazy);

static inline void pudp_idte_local(struct mm_struct *mm,
				   unsigned long addr, pud_t *pudp)
{
	if (MACHINE_HAS_TLB_GUEST)
		__pudp_idte(addr, pudp, IDTE_NODAT | IDTE_GUEST_ASCE,
			    mm->context.asce, IDTE_LOCAL);
	else
		__pudp_idte(addr, pudp, 0, 0, IDTE_LOCAL);
}

static inline void pudp_idte_global(struct mm_struct *mm,
				    unsigned long addr, pud_t *pudp)
{
	if (MACHINE_HAS_TLB_GUEST)
		__pudp_idte(addr, pudp, IDTE_NODAT | IDTE_GUEST_ASCE,
			    mm->context.asce, IDTE_GLOBAL);
	else if (MACHINE_HAS_IDTE)
		__pudp_idte(addr, pudp, 0, 0, IDTE_GLOBAL);
	else
		/*
		 * Invalid bit position is the same for pmd and pud, so we can
		 * re-use _pmd_csp() here
		 */
		__pmdp_csp((pmd_t *) pudp);
}

static inline pud_t pudp_flush_direct(struct mm_struct *mm,
				      unsigned long addr, pud_t *pudp)
{
	pud_t old;

	old = *pudp;
	if (pud_val(old) & _REGION_ENTRY_INVALID)
		return old;
	atomic_inc(&mm->context.flush_count);
	if (MACHINE_HAS_TLB_LC &&
	    cpumask_equal(mm_cpumask(mm), cpumask_of(smp_processor_id())))
		pudp_idte_local(mm, addr, pudp);
	else
		pudp_idte_global(mm, addr, pudp);
	atomic_dec(&mm->context.flush_count);
	return old;
}

pud_t pudp_xchg_direct(struct mm_struct *mm, unsigned long addr,
		       pud_t *pudp, pud_t new)
{
	pud_t old;

	preempt_disable();
	old = pudp_flush_direct(mm, addr, pudp);
	*pudp = new;
	preempt_enable();
	return old;
}
EXPORT_SYMBOL(pudp_xchg_direct);

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
<<<<<<< HEAD
=======
static inline void thp_split_vma(struct vm_area_struct *vma)
{
	unsigned long addr;

	for (addr = vma->vm_start; addr < vma->vm_end; addr += PAGE_SIZE)
		follow_page(vma, addr, FOLL_SPLIT);
}

static inline void thp_split_mm(struct mm_struct *mm)
{
	struct vm_area_struct *vma;

	for (vma = mm->mmap; vma != NULL; vma = vma->vm_next) {
		thp_split_vma(vma);
		vma->vm_flags &= ~VM_HUGEPAGE;
		vma->vm_flags |= VM_NOHUGEPAGE;
	}
	mm->def_flags |= VM_NOHUGEPAGE;
}
#else
static inline void thp_split_mm(struct mm_struct *mm)
{
}
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */

/*
 * switch on pgstes for its userspace process (for kvm)
 */
int s390_enable_sie(void)
{
	struct mm_struct *mm = current->mm;

	/* Do we have pgstes? if yes, we are done */
	if (mm_has_pgste(mm))
		return 0;
	/* Fail if the page tables are 2K */
	if (!mm_alloc_pgste(mm))
		return -EINVAL;
	down_write(&mm->mmap_sem);
	mm->context.has_pgste = 1;
	/* split thp mappings and disable thp for future mappings */
	thp_split_mm(mm);
	up_write(&mm->mmap_sem);
	return 0;
}
EXPORT_SYMBOL_GPL(s390_enable_sie);

/*
 * Enable storage key handling from now on and initialize the storage
 * keys with the default key.
 */
static int __s390_enable_skey(pte_t *pte, unsigned long addr,
			      unsigned long next, struct mm_walk *walk)
{
	unsigned long ptev;
	pgste_t pgste;

	pgste = pgste_get_lock(pte);
	/*
	 * Remove all zero page mappings,
	 * after establishing a policy to forbid zero page mappings
	 * following faults for that page will get fresh anonymous pages
	 */
	if (is_zero_pfn(pte_pfn(*pte))) {
		ptep_flush_direct(walk->mm, addr, pte);
		pte_val(*pte) = _PAGE_INVALID;
	}
	/* Clear storage key */
	pgste_val(pgste) &= ~(PGSTE_ACC_BITS | PGSTE_FP_BIT |
			      PGSTE_GR_BIT | PGSTE_GC_BIT);
	ptev = pte_val(*pte);
	if (!(ptev & _PAGE_INVALID) && (ptev & _PAGE_WRITE))
		page_set_storage_key(ptev & PAGE_MASK, PAGE_DEFAULT_KEY, 1);
	pgste_set_unlock(pte, pgste);
	return 0;
}

int s390_enable_skey(void)
{
	struct mm_walk walk = { .pte_entry = __s390_enable_skey };
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	int rc = 0;

	down_write(&mm->mmap_sem);
	if (mm_use_skey(mm))
		goto out_up;

	mm->context.use_skey = 1;
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (ksm_madvise(vma, vma->vm_start, vma->vm_end,
				MADV_UNMERGEABLE, &vma->vm_flags)) {
			mm->context.use_skey = 0;
			rc = -ENOMEM;
			goto out_up;
		}
	}
	mm->def_flags &= ~VM_MERGEABLE;

	walk.mm = mm;
	walk_page_range(0, TASK_SIZE, &walk);

out_up:
	up_write(&mm->mmap_sem);
	return rc;
}
EXPORT_SYMBOL_GPL(s390_enable_skey);

/*
 * Reset CMMA state, make all pages stable again.
 */
static int __s390_reset_cmma(pte_t *pte, unsigned long addr,
			     unsigned long next, struct mm_walk *walk)
{
	pgste_t pgste;

	pgste = pgste_get_lock(pte);
	pgste_val(pgste) &= ~_PGSTE_GPS_USAGE_MASK;
	pgste_set_unlock(pte, pgste);
	return 0;
}

void s390_reset_cmma(struct mm_struct *mm)
{
	struct mm_walk walk = { .pte_entry = __s390_reset_cmma };

	down_write(&mm->mmap_sem);
	walk.mm = mm;
	walk_page_range(0, TASK_SIZE, &walk);
	up_write(&mm->mmap_sem);
}
EXPORT_SYMBOL_GPL(s390_reset_cmma);

/*
 * Test and reset if a guest page is dirty
 */
bool gmap_test_and_clear_dirty(unsigned long address, struct gmap *gmap)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	spinlock_t *ptl;
	bool dirty = false;

	pgd = pgd_offset(gmap->mm, address);
	pud = pud_alloc(gmap->mm, pgd, address);
	if (!pud)
		return false;
	pmd = pmd_alloc(gmap->mm, pud, address);
	if (!pmd)
		return false;
	/* We can't run guests backed by huge pages, but userspace can
	 * still set them up and then try to migrate them without any
	 * migration support.
	 */
	if (pmd_large(*pmd))
		return true;

	pte = pte_alloc_map_lock(gmap->mm, pmd, address, &ptl);
	if (unlikely(!pte))
		return false;

	if (ptep_test_and_clear_user_dirty(gmap->mm, address, pte))
		dirty = true;

	spin_unlock(ptl);
	return dirty;
}
EXPORT_SYMBOL_GPL(gmap_test_and_clear_dirty);

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
int pmdp_clear_flush_young(struct vm_area_struct *vma, unsigned long address,
			   pmd_t *pmdp)
{
	VM_BUG_ON(address & ~HPAGE_PMD_MASK);
	/* No need to flush TLB
	 * On s390 reference bits are in storage key and never in TLB */
	return pmdp_test_and_clear_young(vma, address, pmdp);
}

int pmdp_set_access_flags(struct vm_area_struct *vma,
			  unsigned long address, pmd_t *pmdp,
			  pmd_t entry, int dirty)
{
	VM_BUG_ON(address & ~HPAGE_PMD_MASK);

	entry = pmd_mkyoung(entry);
	if (dirty)
		entry = pmd_mkdirty(entry);
	if (pmd_same(*pmdp, entry))
		return 0;
	pmdp_invalidate(vma, address, pmdp);
	set_pmd_at(vma->vm_mm, address, pmdp, entry);
	return 1;
}

static void pmdp_splitting_flush_sync(void *arg)
{
	/* Simply deliver the interrupt */
}

void pmdp_splitting_flush(struct vm_area_struct *vma, unsigned long address,
			  pmd_t *pmdp)
{
	VM_BUG_ON(address & ~HPAGE_PMD_MASK);
	if (!test_and_set_bit(_SEGMENT_ENTRY_SPLIT_BIT,
			      (unsigned long *) pmdp)) {
		/* need to serialize against gup-fast (IRQ disabled) */
		smp_call_function(pmdp_splitting_flush_sync, NULL, 1);
	}
}

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
void pgtable_trans_huge_deposit(struct mm_struct *mm, pmd_t *pmdp,
				pgtable_t pgtable)
{
	struct list_head *lh = (struct list_head *) pgtable;

	assert_spin_locked(pmd_lockptr(mm, pmdp));

	/* FIFO */
	if (!pmd_huge_pte(mm, pmdp))
		INIT_LIST_HEAD(lh);
	else
		list_add(lh, (struct list_head *) pmd_huge_pte(mm, pmdp));
	pmd_huge_pte(mm, pmdp) = pgtable;
}

pgtable_t pgtable_trans_huge_withdraw(struct mm_struct *mm, pmd_t *pmdp)
{
	struct list_head *lh;
	pgtable_t pgtable;
	pte_t *ptep;

	assert_spin_locked(pmd_lockptr(mm, pmdp));

	/* FIFO */
	pgtable = pmd_huge_pte(mm, pmdp);
	lh = (struct list_head *) pgtable;
	if (list_empty(lh))
		pmd_huge_pte(mm, pmdp) = NULL;
	else {
		pmd_huge_pte(mm, pmdp) = (pgtable_t) lh->next;
		list_del(lh);
	}
	ptep = (pte_t *) pgtable;
	pte_val(*ptep) = _PAGE_INVALID;
	ptep++;
	pte_val(*ptep) = _PAGE_INVALID;
	return pgtable;
}
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */

#ifdef CONFIG_PGSTE
void ptep_set_pte_at(struct mm_struct *mm, unsigned long addr,
		     pte_t *ptep, pte_t entry)
{
	pgste_t pgste;

	/* the mm_has_pgste() check is done in set_pte_at() */
	preempt_disable();
	pgste = pgste_get_lock(ptep);
	pgste_val(pgste) &= ~_PGSTE_GPS_ZERO;
	pgste_set_key(ptep, pgste, entry, mm);
	pgste = pgste_set_pte(ptep, pgste, entry);
	pgste_set_unlock(ptep, pgste);
	preempt_enable();
}

void ptep_set_notify(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	pgste_t pgste;

	preempt_disable();
	pgste = pgste_get_lock(ptep);
	pgste_val(pgste) |= PGSTE_IN_BIT;
	pgste_set_unlock(ptep, pgste);
	preempt_enable();
}

/**
 * ptep_force_prot - change access rights of a locked pte
 * @mm: pointer to the process mm_struct
 * @addr: virtual address in the guest address space
 * @ptep: pointer to the page table entry
 * @prot: indicates guest access rights: PROT_NONE, PROT_READ or PROT_WRITE
 * @bit: pgste bit to set (e.g. for notification)
 *
 * Returns 0 if the access rights were changed and -EAGAIN if the current
 * and requested access rights are incompatible.
 */
int ptep_force_prot(struct mm_struct *mm, unsigned long addr,
		    pte_t *ptep, int prot, unsigned long bit)
{
	pte_t entry;
	pgste_t pgste;
	int pte_i, pte_p, nodat;

	pgste = pgste_get_lock(ptep);
	entry = *ptep;
	/* Check pte entry after all locks have been acquired */
	pte_i = pte_val(entry) & _PAGE_INVALID;
	pte_p = pte_val(entry) & _PAGE_PROTECT;
	if ((pte_i && (prot != PROT_NONE)) ||
	    (pte_p && (prot & PROT_WRITE))) {
		pgste_set_unlock(ptep, pgste);
		return -EAGAIN;
	}
	/* Change access rights and set pgste bit */
	nodat = !!(pgste_val(pgste) & _PGSTE_GPS_NODAT);
	if (prot == PROT_NONE && !pte_i) {
		ptep_flush_direct(mm, addr, ptep, nodat);
		pgste = pgste_update_all(entry, pgste, mm);
		pte_val(entry) |= _PAGE_INVALID;
	}
	if (prot == PROT_READ && !pte_p) {
		ptep_flush_direct(mm, addr, ptep, nodat);
		pte_val(entry) &= ~_PAGE_INVALID;
		pte_val(entry) |= _PAGE_PROTECT;
	}
	pgste_val(pgste) |= bit;
	pgste = pgste_set_pte(ptep, pgste, entry);
	pgste_set_unlock(ptep, pgste);
	return 0;
}

int ptep_shadow_pte(struct mm_struct *mm, unsigned long saddr,
		    pte_t *sptep, pte_t *tptep, pte_t pte)
{
	pgste_t spgste, tpgste;
	pte_t spte, tpte;
	int rc = -EAGAIN;

	if (!(pte_val(*tptep) & _PAGE_INVALID))
		return 0;	/* already shadowed */
	spgste = pgste_get_lock(sptep);
	spte = *sptep;
	if (!(pte_val(spte) & _PAGE_INVALID) &&
	    !((pte_val(spte) & _PAGE_PROTECT) &&
	      !(pte_val(pte) & _PAGE_PROTECT))) {
		pgste_val(spgste) |= PGSTE_VSIE_BIT;
		tpgste = pgste_get_lock(tptep);
		pte_val(tpte) = (pte_val(spte) & PAGE_MASK) |
				(pte_val(pte) & _PAGE_PROTECT);
		/* don't touch the storage key - it belongs to parent pgste */
		tpgste = pgste_set_pte(tptep, tpgste, tpte);
		pgste_set_unlock(tptep, tpgste);
		rc = 1;
	}
	pgste_set_unlock(sptep, spgste);
	return rc;
}

void ptep_unshadow_pte(struct mm_struct *mm, unsigned long saddr, pte_t *ptep)
{
	pgste_t pgste;
	int nodat;

	pgste = pgste_get_lock(ptep);
	/* notifier is called by the caller */
	nodat = !!(pgste_val(pgste) & _PGSTE_GPS_NODAT);
	ptep_flush_direct(mm, saddr, ptep, nodat);
	/* don't touch the storage key - it belongs to parent pgste */
	pgste = pgste_set_pte(ptep, pgste, __pte(_PAGE_INVALID));
	pgste_set_unlock(ptep, pgste);
}

static void ptep_zap_swap_entry(struct mm_struct *mm, swp_entry_t entry)
{
	if (!non_swap_entry(entry))
		dec_mm_counter(mm, MM_SWAPENTS);
	else if (is_migration_entry(entry)) {
		struct page *page = migration_entry_to_page(entry);

		dec_mm_counter(mm, mm_counter(page));
	}
	free_swap_and_cache(entry);
}

void ptep_zap_unused(struct mm_struct *mm, unsigned long addr,
		     pte_t *ptep, int reset)
{
	unsigned long pgstev;
	pgste_t pgste;
	pte_t pte;

	/* Zap unused and logically-zero pages */
	preempt_disable();
	pgste = pgste_get_lock(ptep);
	pgstev = pgste_val(pgste);
	pte = *ptep;
	if (!reset && pte_swap(pte) &&
	    ((pgstev & _PGSTE_GPS_USAGE_MASK) == _PGSTE_GPS_USAGE_UNUSED ||
	     (pgstev & _PGSTE_GPS_ZERO))) {
		ptep_zap_swap_entry(mm, pte_to_swp_entry(pte));
		pte_clear(mm, addr, ptep);
	}
	if (reset)
		pgste_val(pgste) &= ~_PGSTE_GPS_USAGE_MASK;
	pgste_set_unlock(ptep, pgste);
	preempt_enable();
}

void ptep_zap_key(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	unsigned long ptev;
	pgste_t pgste;

	/* Clear storage key ACC and F, but set R/C */
	preempt_disable();
	pgste = pgste_get_lock(ptep);
	pgste_val(pgste) &= ~(PGSTE_ACC_BITS | PGSTE_FP_BIT);
	pgste_val(pgste) |= PGSTE_GR_BIT | PGSTE_GC_BIT;
	ptev = pte_val(*ptep);
	if (!(ptev & _PAGE_INVALID) && (ptev & _PAGE_WRITE))
		page_set_storage_key(ptev & PAGE_MASK, PAGE_DEFAULT_KEY, 1);
	pgste_set_unlock(ptep, pgste);
	preempt_enable();
}

/*
 * Test and reset if a guest page is dirty
 */
bool ptep_test_and_clear_uc(struct mm_struct *mm, unsigned long addr,
		       pte_t *ptep)
{
	pgste_t pgste;
	pte_t pte;
	bool dirty;
	int nodat;

	pgste = pgste_get_lock(ptep);
	dirty = !!(pgste_val(pgste) & PGSTE_UC_BIT);
	pgste_val(pgste) &= ~PGSTE_UC_BIT;
	pte = *ptep;
	if (dirty && (pte_val(pte) & _PAGE_PRESENT)) {
		pgste = pgste_pte_notify(mm, addr, ptep, pgste);
		nodat = !!(pgste_val(pgste) & _PGSTE_GPS_NODAT);
		ptep_ipte_global(mm, addr, ptep, nodat);
		if (MACHINE_HAS_ESOP || !(pte_val(pte) & _PAGE_WRITE))
			pte_val(pte) |= _PAGE_PROTECT;
		else
			pte_val(pte) |= _PAGE_INVALID;
		*ptep = pte;
	}
	pgste_set_unlock(ptep, pgste);
	return dirty;
}
EXPORT_SYMBOL_GPL(ptep_test_and_clear_uc);

int set_guest_storage_key(struct mm_struct *mm, unsigned long addr,
			  unsigned char key, bool nq)
{
	unsigned long keyul, paddr;
	spinlock_t *ptl;
	pgste_t old, new;
	pmd_t *pmdp;
	pte_t *ptep;

	pmdp = pmd_alloc_map(mm, addr);
	if (unlikely(!pmdp))
		return -EFAULT;

	ptl = pmd_lock(mm, pmdp);
	if (!pmd_present(*pmdp)) {
		spin_unlock(ptl);
		return -EFAULT;
	}

	if (pmd_large(*pmdp)) {
		paddr = pmd_val(*pmdp) & HPAGE_MASK;
		paddr |= addr & ~HPAGE_MASK;
		/*
		 * Huge pmds need quiescing operations, they are
		 * always mapped.
		 */
		page_set_storage_key(paddr, key, 1);
		spin_unlock(ptl);
		return 0;
	}
	spin_unlock(ptl);

	ptep = pte_alloc_map_lock(mm, pmdp, addr, &ptl);
	if (unlikely(!ptep))
		return -EFAULT;

	new = old = pgste_get_lock(ptep);
	pgste_val(new) &= ~(PGSTE_GR_BIT | PGSTE_GC_BIT |
			    PGSTE_ACC_BITS | PGSTE_FP_BIT);
	keyul = (unsigned long) key;
	pgste_val(new) |= (keyul & (_PAGE_CHANGED | _PAGE_REFERENCED)) << 48;
	pgste_val(new) |= (keyul & (_PAGE_ACC_BITS | _PAGE_FP_BIT)) << 56;
	if (!(pte_val(*ptep) & _PAGE_INVALID)) {
		unsigned long bits, skey;

		paddr = pte_val(*ptep) & PAGE_MASK;
		skey = (unsigned long) page_get_storage_key(paddr);
		bits = skey & (_PAGE_CHANGED | _PAGE_REFERENCED);
		skey = key & (_PAGE_ACC_BITS | _PAGE_FP_BIT);
		/* Set storage key ACC and FP */
		page_set_storage_key(paddr, skey, !nq);
		/* Merge host changed & referenced into pgste  */
		pgste_val(new) |= bits << 52;
	}
	/* changing the guest storage key is considered a change of the page */
	if ((pgste_val(new) ^ pgste_val(old)) &
	    (PGSTE_ACC_BITS | PGSTE_FP_BIT | PGSTE_GR_BIT | PGSTE_GC_BIT))
		pgste_val(new) |= PGSTE_UC_BIT;

	pgste_set_unlock(ptep, new);
	pte_unmap_unlock(ptep, ptl);
	return 0;
}
EXPORT_SYMBOL(set_guest_storage_key);

/**
 * Conditionally set a guest storage key (handling csske).
 * oldkey will be updated when either mr or mc is set and a pointer is given.
 *
 * Returns 0 if a guests storage key update wasn't necessary, 1 if the guest
 * storage key was updated and -EFAULT on access errors.
 */
int cond_set_guest_storage_key(struct mm_struct *mm, unsigned long addr,
			       unsigned char key, unsigned char *oldkey,
			       bool nq, bool mr, bool mc)
{
	unsigned char tmp, mask = _PAGE_ACC_BITS | _PAGE_FP_BIT;
	int rc;

	/* we can drop the pgste lock between getting and setting the key */
	if (mr | mc) {
		rc = get_guest_storage_key(current->mm, addr, &tmp);
		if (rc)
			return rc;
		if (oldkey)
			*oldkey = tmp;
		if (!mr)
			mask |= _PAGE_REFERENCED;
		if (!mc)
			mask |= _PAGE_CHANGED;
		if (!((tmp ^ key) & mask))
			return 0;
	}
	rc = set_guest_storage_key(current->mm, addr, key, nq);
	return rc < 0 ? rc : 1;
}
EXPORT_SYMBOL(cond_set_guest_storage_key);

/**
 * Reset a guest reference bit (rrbe), returning the reference and changed bit.
 *
 * Returns < 0 in case of error, otherwise the cc to be reported to the guest.
 */
int reset_guest_reference_bit(struct mm_struct *mm, unsigned long addr)
{
	spinlock_t *ptl;
	unsigned long paddr;
	pgste_t old, new;
	pmd_t *pmdp;
	pte_t *ptep;
	int cc = 0;

	pmdp = pmd_alloc_map(mm, addr);
	if (unlikely(!pmdp))
		return -EFAULT;

	ptl = pmd_lock(mm, pmdp);
	if (!pmd_present(*pmdp)) {
		spin_unlock(ptl);
		return -EFAULT;
	}

	if (pmd_large(*pmdp)) {
		paddr = pmd_val(*pmdp) & HPAGE_MASK;
		paddr |= addr & ~HPAGE_MASK;
		cc = page_reset_referenced(paddr);
		spin_unlock(ptl);
		return cc;
	}
	spin_unlock(ptl);

	ptep = pte_alloc_map_lock(mm, pmdp, addr, &ptl);
	if (unlikely(!ptep))
		return -EFAULT;

	new = old = pgste_get_lock(ptep);
	/* Reset guest reference bit only */
	pgste_val(new) &= ~PGSTE_GR_BIT;

	if (!(pte_val(*ptep) & _PAGE_INVALID)) {
		paddr = pte_val(*ptep) & PAGE_MASK;
		cc = page_reset_referenced(paddr);
		/* Merge real referenced bit into host-set */
		pgste_val(new) |= ((unsigned long) cc << 53) & PGSTE_HR_BIT;
	}
	/* Reflect guest's logical view, not physical */
	cc |= (pgste_val(old) & (PGSTE_GR_BIT | PGSTE_GC_BIT)) >> 49;
	/* Changing the guest storage key is considered a change of the page */
	if ((pgste_val(new) ^ pgste_val(old)) & PGSTE_GR_BIT)
		pgste_val(new) |= PGSTE_UC_BIT;

	pgste_set_unlock(ptep, new);
	pte_unmap_unlock(ptep, ptl);
	return cc;
}
EXPORT_SYMBOL(reset_guest_reference_bit);

int get_guest_storage_key(struct mm_struct *mm, unsigned long addr,
			  unsigned char *key)
{
	unsigned long paddr;
	spinlock_t *ptl;
	pgste_t pgste;
	pmd_t *pmdp;
	pte_t *ptep;

	pmdp = pmd_alloc_map(mm, addr);
	if (unlikely(!pmdp))
		return -EFAULT;

	ptl = pmd_lock(mm, pmdp);
	if (!pmd_present(*pmdp)) {
		/* Not yet mapped memory has a zero key */
		spin_unlock(ptl);
		*key = 0;
		return 0;
	}

	if (pmd_large(*pmdp)) {
		paddr = pmd_val(*pmdp) & HPAGE_MASK;
		paddr |= addr & ~HPAGE_MASK;
		*key = page_get_storage_key(paddr);
		spin_unlock(ptl);
		return 0;
	}
	spin_unlock(ptl);

	ptep = pte_alloc_map_lock(mm, pmdp, addr, &ptl);
	if (unlikely(!ptep))
		return -EFAULT;

	pgste = pgste_get_lock(ptep);
	*key = (pgste_val(pgste) & (PGSTE_ACC_BITS | PGSTE_FP_BIT)) >> 56;
	paddr = pte_val(*ptep) & PAGE_MASK;
	if (!(pte_val(*ptep) & _PAGE_INVALID))
		*key = page_get_storage_key(paddr);
	/* Reflect guest's logical view, not physical */
	*key |= (pgste_val(pgste) & (PGSTE_GR_BIT | PGSTE_GC_BIT)) >> 48;
	pgste_set_unlock(ptep, pgste);
	pte_unmap_unlock(ptep, ptl);
	return 0;
}
EXPORT_SYMBOL(get_guest_storage_key);

/**
 * pgste_perform_essa - perform ESSA actions on the PGSTE.
 * @mm: the memory context. It must have PGSTEs, no check is performed here!
 * @hva: the host virtual address of the page whose PGSTE is to be processed
 * @orc: the specific action to perform, see the ESSA_SET_* macros.
 * @oldpte: the PTE will be saved there if the pointer is not NULL.
 * @oldpgste: the old PGSTE will be saved there if the pointer is not NULL.
 *
 * Return: 1 if the page is to be added to the CBRL, otherwise 0,
 *	   or < 0 in case of error. -EINVAL is returned for invalid values
 *	   of orc, -EFAULT for invalid addresses.
 */
int pgste_perform_essa(struct mm_struct *mm, unsigned long hva, int orc,
			unsigned long *oldpte, unsigned long *oldpgste)
{
	unsigned long pgstev;
	spinlock_t *ptl;
	pgste_t pgste;
	pte_t *ptep;
	int res = 0;

	WARN_ON_ONCE(orc > ESSA_MAX);
	if (unlikely(orc > ESSA_MAX))
		return -EINVAL;
	ptep = get_locked_pte(mm, hva, &ptl);
	if (unlikely(!ptep))
		return -EFAULT;
	pgste = pgste_get_lock(ptep);
	pgstev = pgste_val(pgste);
	if (oldpte)
		*oldpte = pte_val(*ptep);
	if (oldpgste)
		*oldpgste = pgstev;

	switch (orc) {
	case ESSA_GET_STATE:
		break;
	case ESSA_SET_STABLE:
		pgstev &= ~(_PGSTE_GPS_USAGE_MASK | _PGSTE_GPS_NODAT);
		pgstev |= _PGSTE_GPS_USAGE_STABLE;
		break;
	case ESSA_SET_UNUSED:
		pgstev &= ~_PGSTE_GPS_USAGE_MASK;
		pgstev |= _PGSTE_GPS_USAGE_UNUSED;
		if (pte_val(*ptep) & _PAGE_INVALID)
			res = 1;
		break;
	case ESSA_SET_VOLATILE:
		pgstev &= ~_PGSTE_GPS_USAGE_MASK;
		pgstev |= _PGSTE_GPS_USAGE_VOLATILE;
		if (pte_val(*ptep) & _PAGE_INVALID)
			res = 1;
		break;
	case ESSA_SET_POT_VOLATILE:
		pgstev &= ~_PGSTE_GPS_USAGE_MASK;
		if (!(pte_val(*ptep) & _PAGE_INVALID)) {
			pgstev |= _PGSTE_GPS_USAGE_POT_VOLATILE;
			break;
		}
		if (pgstev & _PGSTE_GPS_ZERO) {
			pgstev |= _PGSTE_GPS_USAGE_VOLATILE;
			break;
		}
		if (!(pgstev & PGSTE_GC_BIT)) {
			pgstev |= _PGSTE_GPS_USAGE_VOLATILE;
			res = 1;
			break;
		}
		break;
	case ESSA_SET_STABLE_RESIDENT:
		pgstev &= ~_PGSTE_GPS_USAGE_MASK;
		pgstev |= _PGSTE_GPS_USAGE_STABLE;
		/*
		 * Since the resident state can go away any time after this
		 * call, we will not make this page resident. We can revisit
		 * this decision if a guest will ever start using this.
		 */
		break;
	case ESSA_SET_STABLE_IF_RESIDENT:
		if (!(pte_val(*ptep) & _PAGE_INVALID)) {
			pgstev &= ~_PGSTE_GPS_USAGE_MASK;
			pgstev |= _PGSTE_GPS_USAGE_STABLE;
		}
		break;
	case ESSA_SET_STABLE_NODAT:
		pgstev &= ~_PGSTE_GPS_USAGE_MASK;
		pgstev |= _PGSTE_GPS_USAGE_STABLE | _PGSTE_GPS_NODAT;
		break;
	default:
		/* we should never get here! */
		break;
	}
	/* If we are discarding a page, set it to logical zero */
	if (res)
		pgstev |= _PGSTE_GPS_ZERO;

	pgste_val(pgste) = pgstev;
	pgste_set_unlock(ptep, pgste);
	pte_unmap_unlock(ptep, ptl);
	return res;
}
EXPORT_SYMBOL(pgste_perform_essa);

/**
 * set_pgste_bits - set specific PGSTE bits.
 * @mm: the memory context. It must have PGSTEs, no check is performed here!
 * @hva: the host virtual address of the page whose PGSTE is to be processed
 * @bits: a bitmask representing the bits that will be touched
 * @value: the values of the bits to be written. Only the bits in the mask
 *	   will be written.
 *
 * Return: 0 on success, < 0 in case of error.
 */
int set_pgste_bits(struct mm_struct *mm, unsigned long hva,
			unsigned long bits, unsigned long value)
{
	spinlock_t *ptl;
	pgste_t new;
	pte_t *ptep;

	ptep = get_locked_pte(mm, hva, &ptl);
	if (unlikely(!ptep))
		return -EFAULT;
	new = pgste_get_lock(ptep);

	pgste_val(new) &= ~bits;
	pgste_val(new) |= value & bits;

	pgste_set_unlock(ptep, new);
	pte_unmap_unlock(ptep, ptl);
	return 0;
}
EXPORT_SYMBOL(set_pgste_bits);

/**
 * get_pgste - get the current PGSTE for the given address.
 * @mm: the memory context. It must have PGSTEs, no check is performed here!
 * @hva: the host virtual address of the page whose PGSTE is to be processed
 * @pgstep: will be written with the current PGSTE for the given address.
 *
 * Return: 0 on success, < 0 in case of error.
 */
int get_pgste(struct mm_struct *mm, unsigned long hva, unsigned long *pgstep)
{
	spinlock_t *ptl;
	pte_t *ptep;

	ptep = get_locked_pte(mm, hva, &ptl);
	if (unlikely(!ptep))
		return -EFAULT;
	*pgstep = pgste_val(pgste_get(ptep));
	pte_unmap_unlock(ptep, ptl);
	return 0;
}
EXPORT_SYMBOL(get_pgste);
#endif
