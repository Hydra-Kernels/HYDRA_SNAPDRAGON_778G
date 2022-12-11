/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * KVM/MIPS TLB handling, this file is part of the Linux host kernel so that
 * TLB handlers run from KSEG0
 *
 * Copyright (C) 2012  MIPS Technologies, Inc.  All rights reserved.
 * Authors: Sanjay Lal <sanjayl@kymasys.com>
 */

#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/kvm_host.h>
#include <linux/srcu.h>

#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/mmu_context.h>
#include <asm/pgtable.h>
#include <asm/cacheflush.h>
#include <asm/tlb.h>
#include <asm/tlbdebug.h>

#undef CONFIG_MIPS_MT
#include <asm/r4kcache.h>
#define CONFIG_MIPS_MT

#define KVM_GUEST_PC_TLB    0
#define KVM_GUEST_SP_TLB    1

#ifdef CONFIG_KVM_MIPS_VZ
unsigned long GUESTID_MASK;
EXPORT_SYMBOL_GPL(GUESTID_MASK);
unsigned long GUESTID_FIRST_VERSION;
EXPORT_SYMBOL_GPL(GUESTID_FIRST_VERSION);
unsigned long GUESTID_VERSION_MASK;
EXPORT_SYMBOL_GPL(GUESTID_VERSION_MASK);

static u32 kvm_mips_get_root_asid(struct kvm_vcpu *vcpu)
{
	struct mm_struct *gpa_mm = &vcpu->kvm->arch.gpa_mm;

	if (cpu_has_guestid)
		return 0;
	else
		return cpu_asid(smp_processor_id(), gpa_mm);
}
#endif

static u32 kvm_mips_get_kernel_asid(struct kvm_vcpu *vcpu)
{
	struct mm_struct *kern_mm = &vcpu->arch.guest_kernel_mm;
	int cpu = smp_processor_id();

	return cpu_asid(cpu, kern_mm);
}

static u32 kvm_mips_get_user_asid(struct kvm_vcpu *vcpu)
{
	struct mm_struct *user_mm = &vcpu->arch.guest_user_mm;
	int cpu = smp_processor_id();

	return cpu_asid(cpu, user_mm);
}

/* Structure defining an tlb entry data set. */

void kvm_mips_dump_host_tlbs(void)
{
	unsigned long flags;

	local_irq_save(flags);

	kvm_info("HOST TLBs:\n");
	dump_tlb_regs();
	pr_info("\n");
	dump_tlb_all();

	local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(kvm_mips_dump_host_tlbs);

void kvm_mips_dump_guest_tlbs(struct kvm_vcpu *vcpu)
{
	struct mips_coproc *cop0 = vcpu->arch.cop0;
	struct kvm_mips_tlb tlb;
	int i;

	kvm_info("Guest TLBs:\n");
	kvm_info("Guest EntryHi: %#lx\n", kvm_read_c0_guest_entryhi(cop0));

	for (i = 0; i < KVM_MIPS_GUEST_TLB_SIZE; i++) {
		tlb = vcpu->arch.guest_tlb[i];
		kvm_info("TLB%c%3d Hi 0x%08lx ",
			 (tlb.tlb_lo[0] | tlb.tlb_lo[1]) & ENTRYLO_V
							? ' ' : '*',
			 i, tlb.tlb_hi);
		kvm_info("Lo0=0x%09llx %c%c attr %lx ",
			 (u64) mips3_tlbpfn_to_paddr(tlb.tlb_lo[0]),
			 (tlb.tlb_lo[0] & ENTRYLO_D) ? 'D' : ' ',
			 (tlb.tlb_lo[0] & ENTRYLO_G) ? 'G' : ' ',
			 (tlb.tlb_lo[0] & ENTRYLO_C) >> ENTRYLO_C_SHIFT);
		kvm_info("Lo1=0x%09llx %c%c attr %lx sz=%lx\n",
			 (u64) mips3_tlbpfn_to_paddr(tlb.tlb_lo[1]),
			 (tlb.tlb_lo[1] & ENTRYLO_D) ? 'D' : ' ',
			 (tlb.tlb_lo[1] & ENTRYLO_G) ? 'G' : ' ',
			 (tlb.tlb_lo[1] & ENTRYLO_C) >> ENTRYLO_C_SHIFT,
			 tlb.tlb_mask);
	}
}
<<<<<<< HEAD
EXPORT_SYMBOL_GPL(kvm_mips_dump_guest_tlbs);
=======
EXPORT_SYMBOL(kvm_mips_dump_guest_tlbs);

static int kvm_mips_map_page(struct kvm *kvm, gfn_t gfn)
{
	int srcu_idx, err = 0;
	pfn_t pfn;

	if (kvm->arch.guest_pmap[gfn] != KVM_INVALID_PAGE)
		return 0;

	srcu_idx = srcu_read_lock(&kvm->srcu);
	pfn = kvm_mips_gfn_to_pfn(kvm, gfn);

	if (is_error_noslot_pfn(pfn)) {
		kvm_err("Couldn't get pfn for gfn %#" PRIx64 "!\n", gfn);
		err = -EFAULT;
		goto out;
	}

	kvm->arch.guest_pmap[gfn] = pfn;
out:
	srcu_read_unlock(&kvm->srcu, srcu_idx);
	return err;
}

/* Translate guest KSEG0 addresses to Host PA */
unsigned long kvm_mips_translate_guest_kseg0_to_hpa(struct kvm_vcpu *vcpu,
						    unsigned long gva)
{
	gfn_t gfn;
	uint32_t offset = gva & ~PAGE_MASK;
	struct kvm *kvm = vcpu->kvm;

	if (KVM_GUEST_KSEGX(gva) != KVM_GUEST_KSEG0) {
		kvm_err("%s/%p: Invalid gva: %#lx\n", __func__,
			__builtin_return_address(0), gva);
		return KVM_INVALID_PAGE;
	}

	gfn = (KVM_GUEST_CPHYSADDR(gva) >> PAGE_SHIFT);

	if (gfn >= kvm->arch.guest_pmap_npages) {
		kvm_err("%s: Invalid gfn: %#llx, GVA: %#lx\n", __func__, gfn,
			gva);
		return KVM_INVALID_PAGE;
	}

	if (kvm_mips_map_page(vcpu->kvm, gfn) < 0)
		return KVM_INVALID_ADDR;

	return (kvm->arch.guest_pmap[gfn] << PAGE_SHIFT) + offset;
}
EXPORT_SYMBOL(kvm_mips_translate_guest_kseg0_to_hpa);

/* XXXKYMA: Must be called with interrupts disabled */
/* set flush_dcache_mask == 0 if no dcache flush required */
int kvm_mips_host_tlb_write(struct kvm_vcpu *vcpu, unsigned long entryhi,
			    unsigned long entrylo0, unsigned long entrylo1,
			    int flush_dcache_mask)
{
	unsigned long flags;
	unsigned long old_entryhi;
	int idx;

	local_irq_save(flags);

	old_entryhi = read_c0_entryhi();
	write_c0_entryhi(entryhi);
	mtc0_tlbw_hazard();

	tlb_probe();
	tlb_probe_hazard();
	idx = read_c0_index();

	if (idx > current_cpu_data.tlbsize) {
		kvm_err("%s: Invalid Index: %d\n", __func__, idx);
		kvm_mips_dump_host_tlbs();
		local_irq_restore(flags);
		return -1;
	}

	write_c0_entrylo0(entrylo0);
	write_c0_entrylo1(entrylo1);
	mtc0_tlbw_hazard();

	if (idx < 0)
		tlb_write_random();
	else
		tlb_write_indexed();
	tlbw_use_hazard();

	kvm_debug("@ %#lx idx: %2d [entryhi(R): %#lx] entrylo0(R): 0x%08lx, entrylo1(R): 0x%08lx\n",
		  vcpu->arch.pc, idx, read_c0_entryhi(),
		  read_c0_entrylo0(), read_c0_entrylo1());

	/* Flush D-cache */
	if (flush_dcache_mask) {
		if (entrylo0 & MIPS3_PG_V) {
			++vcpu->stat.flush_dcache_exits;
			flush_data_cache_page((entryhi & VPN2_MASK) &
					      ~flush_dcache_mask);
		}
		if (entrylo1 & MIPS3_PG_V) {
			++vcpu->stat.flush_dcache_exits;
			flush_data_cache_page(((entryhi & VPN2_MASK) &
					       ~flush_dcache_mask) |
					      (0x1 << PAGE_SHIFT));
		}
	}

	/* Restore old ASID */
	write_c0_entryhi(old_entryhi);
	mtc0_tlbw_hazard();
	tlbw_use_hazard();
	local_irq_restore(flags);
	return 0;
}

/* XXXKYMA: Must be called with interrupts disabled */
int kvm_mips_handle_kseg0_tlb_fault(unsigned long badvaddr,
				    struct kvm_vcpu *vcpu)
{
	gfn_t gfn;
	pfn_t pfn0, pfn1;
	unsigned long vaddr = 0;
	unsigned long entryhi = 0, entrylo0 = 0, entrylo1 = 0;
	int even;
	struct kvm *kvm = vcpu->kvm;
	const int flush_dcache_mask = 0;

	if (KVM_GUEST_KSEGX(badvaddr) != KVM_GUEST_KSEG0) {
		kvm_err("%s: Invalid BadVaddr: %#lx\n", __func__, badvaddr);
		kvm_mips_dump_host_tlbs();
		return -1;
	}

	gfn = (KVM_GUEST_CPHYSADDR(badvaddr) >> PAGE_SHIFT);
	if ((gfn | 1) >= kvm->arch.guest_pmap_npages) {
		kvm_err("%s: Invalid gfn: %#llx, BadVaddr: %#lx\n", __func__,
			gfn, badvaddr);
		kvm_mips_dump_host_tlbs();
		return -1;
	}
	even = !(gfn & 0x1);
	vaddr = badvaddr & (PAGE_MASK << 1);

	if (kvm_mips_map_page(vcpu->kvm, gfn) < 0)
		return -1;

	if (kvm_mips_map_page(vcpu->kvm, gfn ^ 0x1) < 0)
		return -1;

	if (even) {
		pfn0 = kvm->arch.guest_pmap[gfn];
		pfn1 = kvm->arch.guest_pmap[gfn ^ 0x1];
	} else {
		pfn0 = kvm->arch.guest_pmap[gfn ^ 0x1];
		pfn1 = kvm->arch.guest_pmap[gfn];
	}

	entryhi = (vaddr | kvm_mips_get_kernel_asid(vcpu));
	entrylo0 = mips3_paddr_to_tlbpfn(pfn0 << PAGE_SHIFT) | (0x3 << 3) |
		   (1 << 2) | (0x1 << 1);
	entrylo1 = mips3_paddr_to_tlbpfn(pfn1 << PAGE_SHIFT) | (0x3 << 3) |
		   (1 << 2) | (0x1 << 1);

	return kvm_mips_host_tlb_write(vcpu, entryhi, entrylo0, entrylo1,
				       flush_dcache_mask);
}
EXPORT_SYMBOL(kvm_mips_handle_kseg0_tlb_fault);

int kvm_mips_handle_commpage_tlb_fault(unsigned long badvaddr,
	struct kvm_vcpu *vcpu)
{
	pfn_t pfn0, pfn1;
	unsigned long flags, old_entryhi = 0, vaddr = 0;
	unsigned long entrylo0 = 0, entrylo1 = 0;

	pfn0 = CPHYSADDR(vcpu->arch.kseg0_commpage) >> PAGE_SHIFT;
	pfn1 = 0;
	entrylo0 = mips3_paddr_to_tlbpfn(pfn0 << PAGE_SHIFT) | (0x3 << 3) |
		   (1 << 2) | (0x1 << 1);
	entrylo1 = 0;

	local_irq_save(flags);

	old_entryhi = read_c0_entryhi();
	vaddr = badvaddr & (PAGE_MASK << 1);
	write_c0_entryhi(vaddr | kvm_mips_get_kernel_asid(vcpu));
	mtc0_tlbw_hazard();
	write_c0_entrylo0(entrylo0);
	mtc0_tlbw_hazard();
	write_c0_entrylo1(entrylo1);
	mtc0_tlbw_hazard();
	write_c0_index(kvm_mips_get_commpage_asid(vcpu));
	mtc0_tlbw_hazard();
	tlb_write_indexed();
	mtc0_tlbw_hazard();
	tlbw_use_hazard();

	kvm_debug("@ %#lx idx: %2d [entryhi(R): %#lx] entrylo0 (R): 0x%08lx, entrylo1(R): 0x%08lx\n",
		  vcpu->arch.pc, read_c0_index(), read_c0_entryhi(),
		  read_c0_entrylo0(), read_c0_entrylo1());

	/* Restore old ASID */
	write_c0_entryhi(old_entryhi);
	mtc0_tlbw_hazard();
	tlbw_use_hazard();
	local_irq_restore(flags);

	return 0;
}
EXPORT_SYMBOL(kvm_mips_handle_commpage_tlb_fault);

int kvm_mips_handle_mapped_seg_tlb_fault(struct kvm_vcpu *vcpu,
					 struct kvm_mips_tlb *tlb,
					 unsigned long *hpa0,
					 unsigned long *hpa1)
{
	unsigned long entryhi = 0, entrylo0 = 0, entrylo1 = 0;
	struct kvm *kvm = vcpu->kvm;
	pfn_t pfn0, pfn1;
	gfn_t gfn0, gfn1;
	long tlb_lo[2];

	tlb_lo[0] = tlb->tlb_lo0;
	tlb_lo[1] = tlb->tlb_lo1;

	/*
	 * The commpage address must not be mapped to anything else if the guest
	 * TLB contains entries nearby, or commpage accesses will break.
	 */
	if (!((tlb->tlb_hi ^ KVM_GUEST_COMMPAGE_ADDR) &
			VPN2_MASK & (PAGE_MASK << 1)))
		tlb_lo[(KVM_GUEST_COMMPAGE_ADDR >> PAGE_SHIFT) & 1] = 0;

	gfn0 = mips3_tlbpfn_to_paddr(tlb_lo[0]) >> PAGE_SHIFT;
	gfn1 = mips3_tlbpfn_to_paddr(tlb_lo[1]) >> PAGE_SHIFT;
	if (gfn0 >= kvm->arch.guest_pmap_npages ||
	    gfn1 >= kvm->arch.guest_pmap_npages) {
		kvm_err("%s: Invalid gfn: [%#llx, %#llx], EHi: %#lx\n",
			__func__, gfn0, gfn1, tlb->tlb_hi);
		kvm_mips_dump_guest_tlbs(vcpu);
		return -1;
	}

	if (kvm_mips_map_page(kvm, gfn0) < 0)
		return -1;

	if (kvm_mips_map_page(kvm, gfn1) < 0)
		return -1;

	pfn0 = kvm->arch.guest_pmap[gfn0];
	pfn1 = kvm->arch.guest_pmap[gfn1];

	if (hpa0)
		*hpa0 = pfn0 << PAGE_SHIFT;

	if (hpa1)
		*hpa1 = pfn1 << PAGE_SHIFT;

	/* Get attributes from the Guest TLB */
	entryhi = (tlb->tlb_hi & VPN2_MASK) | (KVM_GUEST_KERNEL_MODE(vcpu) ?
					       kvm_mips_get_kernel_asid(vcpu) :
					       kvm_mips_get_user_asid(vcpu));
	entrylo0 = mips3_paddr_to_tlbpfn(pfn0 << PAGE_SHIFT) | (0x3 << 3) |
		   (tlb_lo[0] & MIPS3_PG_D) | (tlb_lo[0] & MIPS3_PG_V);
	entrylo1 = mips3_paddr_to_tlbpfn(pfn1 << PAGE_SHIFT) | (0x3 << 3) |
		   (tlb_lo[1] & MIPS3_PG_D) | (tlb_lo[1] & MIPS3_PG_V);

	kvm_debug("@ %#lx tlb_lo0: 0x%08lx tlb_lo1: 0x%08lx\n", vcpu->arch.pc,
		  tlb->tlb_lo0, tlb->tlb_lo1);

	return kvm_mips_host_tlb_write(vcpu, entryhi, entrylo0, entrylo1,
				       tlb->tlb_mask);
}
EXPORT_SYMBOL(kvm_mips_handle_mapped_seg_tlb_fault);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

int kvm_mips_guest_tlb_lookup(struct kvm_vcpu *vcpu, unsigned long entryhi)
{
	int i;
	int index = -1;
	struct kvm_mips_tlb *tlb = vcpu->arch.guest_tlb;

	for (i = 0; i < KVM_MIPS_GUEST_TLB_SIZE; i++) {
		if (TLB_HI_VPN2_HIT(tlb[i], entryhi) &&
		    TLB_HI_ASID_HIT(tlb[i], entryhi)) {
			index = i;
			break;
		}
	}

	kvm_debug("%s: entryhi: %#lx, index: %d lo0: %#lx, lo1: %#lx\n",
		  __func__, entryhi, index, tlb[i].tlb_lo[0], tlb[i].tlb_lo[1]);

	return index;
}
EXPORT_SYMBOL_GPL(kvm_mips_guest_tlb_lookup);

static int _kvm_mips_host_tlb_inv(unsigned long entryhi)
{
	int idx;

	write_c0_entryhi(entryhi);
	mtc0_tlbw_hazard();

	tlb_probe();
	tlb_probe_hazard();
	idx = read_c0_index();

	if (idx >= current_cpu_data.tlbsize)
		BUG();

	if (idx >= 0) {
		write_c0_entryhi(UNIQUE_ENTRYHI(idx));
		write_c0_entrylo0(0);
		write_c0_entrylo1(0);
		mtc0_tlbw_hazard();

		tlb_write_indexed();
		tlbw_use_hazard();
	}

	return idx;
}

int kvm_mips_host_tlb_inv(struct kvm_vcpu *vcpu, unsigned long va,
			  bool user, bool kernel)
{
	/*
	 * Initialize idx_user and idx_kernel to workaround bogus
	 * maybe-initialized warning when using GCC 6.
	 */
	int idx_user = 0, idx_kernel = 0;
	unsigned long flags, old_entryhi;

	local_irq_save(flags);

	old_entryhi = read_c0_entryhi();

	if (user)
		idx_user = _kvm_mips_host_tlb_inv((va & VPN2_MASK) |
						  kvm_mips_get_user_asid(vcpu));
	if (kernel)
		idx_kernel = _kvm_mips_host_tlb_inv((va & VPN2_MASK) |
						kvm_mips_get_kernel_asid(vcpu));

	write_c0_entryhi(old_entryhi);
	mtc0_tlbw_hazard();

	local_irq_restore(flags);

	/*
	 * We don't want to get reserved instruction exceptions for missing tlb
	 * entries.
	 */
	if (cpu_has_vtag_icache)
		flush_icache_all();

	if (user && idx_user >= 0)
		kvm_debug("%s: Invalidated guest user entryhi %#lx @ idx %d\n",
			  __func__, (va & VPN2_MASK) |
				    kvm_mips_get_user_asid(vcpu), idx_user);
	if (kernel && idx_kernel >= 0)
		kvm_debug("%s: Invalidated guest kernel entryhi %#lx @ idx %d\n",
			  __func__, (va & VPN2_MASK) |
				    kvm_mips_get_kernel_asid(vcpu), idx_kernel);

	return 0;
}
EXPORT_SYMBOL_GPL(kvm_mips_host_tlb_inv);

#ifdef CONFIG_KVM_MIPS_VZ

/* GuestID management */

/**
 * clear_root_gid() - Set GuestCtl1.RID for normal root operation.
 */
static inline void clear_root_gid(void)
{
	if (cpu_has_guestid) {
		clear_c0_guestctl1(MIPS_GCTL1_RID);
		mtc0_tlbw_hazard();
	}
}

/**
 * set_root_gid_to_guest_gid() - Set GuestCtl1.RID to match GuestCtl1.ID.
 *
 * Sets the root GuestID to match the current guest GuestID, for TLB operation
 * on the GPA->RPA mappings in the root TLB.
 *
 * The caller must be sure to disable HTW while the root GID is set, and
 * possibly longer if TLB registers are modified.
 */
static inline void set_root_gid_to_guest_gid(void)
{
	unsigned int guestctl1;

	if (cpu_has_guestid) {
		back_to_back_c0_hazard();
		guestctl1 = read_c0_guestctl1();
		guestctl1 = (guestctl1 & ~MIPS_GCTL1_RID) |
			((guestctl1 & MIPS_GCTL1_ID) >> MIPS_GCTL1_ID_SHIFT)
						     << MIPS_GCTL1_RID_SHIFT;
		write_c0_guestctl1(guestctl1);
		mtc0_tlbw_hazard();
	}
}

int kvm_vz_host_tlb_inv(struct kvm_vcpu *vcpu, unsigned long va)
{
	int idx;
	unsigned long flags, old_entryhi;

	local_irq_save(flags);
	htw_stop();

	/* Set root GuestID for root probe and write of guest TLB entry */
	set_root_gid_to_guest_gid();

	old_entryhi = read_c0_entryhi();

	idx = _kvm_mips_host_tlb_inv((va & VPN2_MASK) |
				     kvm_mips_get_root_asid(vcpu));

	write_c0_entryhi(old_entryhi);
	clear_root_gid();
	mtc0_tlbw_hazard();

	htw_start();
	local_irq_restore(flags);

	/*
	 * We don't want to get reserved instruction exceptions for missing tlb
	 * entries.
	 */
	if (cpu_has_vtag_icache)
		flush_icache_all();

	if (idx > 0)
		kvm_debug("%s: Invalidated root entryhi %#lx @ idx %d\n",
			  __func__, (va & VPN2_MASK) |
				    kvm_mips_get_root_asid(vcpu), idx);

	return 0;
}
EXPORT_SYMBOL_GPL(kvm_vz_host_tlb_inv);

/**
 * kvm_vz_guest_tlb_lookup() - Lookup a guest VZ TLB mapping.
 * @vcpu:	KVM VCPU pointer.
 * @gpa:	Guest virtual address in a TLB mapped guest segment.
 * @gpa:	Ponter to output guest physical address it maps to.
 *
 * Converts a guest virtual address in a guest TLB mapped segment to a guest
 * physical address, by probing the guest TLB.
 *
 * Returns:	0 if guest TLB mapping exists for @gva. *@gpa will have been
 *		written.
 *		-EFAULT if no guest TLB mapping exists for @gva. *@gpa may not
 *		have been written.
 */
int kvm_vz_guest_tlb_lookup(struct kvm_vcpu *vcpu, unsigned long gva,
			    unsigned long *gpa)
{
	unsigned long o_entryhi, o_entrylo[2], o_pagemask;
	unsigned int o_index;
	unsigned long entrylo[2], pagemask, pagemaskbit, pa;
	unsigned long flags;
	int index;

	/* Probe the guest TLB for a mapping */
	local_irq_save(flags);
	/* Set root GuestID for root probe of guest TLB entry */
	htw_stop();
	set_root_gid_to_guest_gid();

	o_entryhi = read_gc0_entryhi();
	o_index = read_gc0_index();

	write_gc0_entryhi((o_entryhi & 0x3ff) | (gva & ~0xfffl));
	mtc0_tlbw_hazard();
	guest_tlb_probe();
	tlb_probe_hazard();

	index = read_gc0_index();
	if (index < 0) {
		/* No match, fail */
		write_gc0_entryhi(o_entryhi);
		write_gc0_index(o_index);

		clear_root_gid();
		htw_start();
		local_irq_restore(flags);
		return -EFAULT;
	}

	/* Match! read the TLB entry */
	o_entrylo[0] = read_gc0_entrylo0();
	o_entrylo[1] = read_gc0_entrylo1();
	o_pagemask = read_gc0_pagemask();

	mtc0_tlbr_hazard();
	guest_tlb_read();
	tlb_read_hazard();

	entrylo[0] = read_gc0_entrylo0();
	entrylo[1] = read_gc0_entrylo1();
	pagemask = ~read_gc0_pagemask() & ~0x1fffl;

	write_gc0_entryhi(o_entryhi);
	write_gc0_index(o_index);
	write_gc0_entrylo0(o_entrylo[0]);
	write_gc0_entrylo1(o_entrylo[1]);
	write_gc0_pagemask(o_pagemask);

	clear_root_gid();
	htw_start();
	local_irq_restore(flags);

	/* Select one of the EntryLo values and interpret the GPA */
	pagemaskbit = (pagemask ^ (pagemask & (pagemask - 1))) >> 1;
	pa = entrylo[!!(gva & pagemaskbit)];

	/*
	 * TLB entry may have become invalid since TLB probe if physical FTLB
	 * entries are shared between threads (e.g. I6400).
	 */
	if (!(pa & ENTRYLO_V))
		return -EFAULT;

	/*
	 * Note, this doesn't take guest MIPS32 XPA into account, where PFN is
	 * split with XI/RI in the middle.
	 */
	pa = (pa << 6) & ~0xfffl;
	pa |= gva & ~(pagemask | pagemaskbit);

	*gpa = pa;
	return 0;
}
EXPORT_SYMBOL_GPL(kvm_vz_guest_tlb_lookup);

/**
 * kvm_vz_local_flush_roottlb_all_guests() - Flush all root TLB entries for
 * guests.
 *
 * Invalidate all entries in root tlb which are GPA mappings.
 */
void kvm_vz_local_flush_roottlb_all_guests(void)
{
	unsigned long flags;
	unsigned long old_entryhi, old_pagemask, old_guestctl1;
	int entry;

	if (WARN_ON(!cpu_has_guestid))
		return;

	local_irq_save(flags);
	htw_stop();

	/* TLBR may clobber EntryHi.ASID, PageMask, and GuestCtl1.RID */
	old_entryhi = read_c0_entryhi();
	old_pagemask = read_c0_pagemask();
	old_guestctl1 = read_c0_guestctl1();

	/*
	 * Invalidate guest entries in root TLB while leaving root entries
	 * intact when possible.
	 */
	for (entry = 0; entry < current_cpu_data.tlbsize; entry++) {
		write_c0_index(entry);
		mtc0_tlbw_hazard();
		tlb_read();
		tlb_read_hazard();

		/* Don't invalidate non-guest (RVA) mappings in the root TLB */
		if (!(read_c0_guestctl1() & MIPS_GCTL1_RID))
			continue;

		/* Make sure all entries differ. */
		write_c0_entryhi(UNIQUE_ENTRYHI(entry));
		write_c0_entrylo0(0);
		write_c0_entrylo1(0);
		write_c0_guestctl1(0);
		mtc0_tlbw_hazard();
		tlb_write_indexed();
	}

	write_c0_entryhi(old_entryhi);
	write_c0_pagemask(old_pagemask);
	write_c0_guestctl1(old_guestctl1);
	tlbw_use_hazard();

	htw_start();
	local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(kvm_vz_local_flush_roottlb_all_guests);

/**
 * kvm_vz_local_flush_guesttlb_all() - Flush all guest TLB entries.
 *
 * Invalidate all entries in guest tlb irrespective of guestid.
 */
void kvm_vz_local_flush_guesttlb_all(void)
{
	unsigned long flags;
	unsigned long old_index;
	unsigned long old_entryhi;
	unsigned long old_entrylo[2];
	unsigned long old_pagemask;
	int entry;
	u64 cvmmemctl2 = 0;

	local_irq_save(flags);

	/* Preserve all clobbered guest registers */
	old_index = read_gc0_index();
	old_entryhi = read_gc0_entryhi();
	old_entrylo[0] = read_gc0_entrylo0();
	old_entrylo[1] = read_gc0_entrylo1();
	old_pagemask = read_gc0_pagemask();

	switch (current_cpu_type()) {
	case CPU_CAVIUM_OCTEON3:
		/* Inhibit machine check due to multiple matching TLB entries */
		cvmmemctl2 = read_c0_cvmmemctl2();
		cvmmemctl2 |= CVMMEMCTL2_INHIBITTS;
		write_c0_cvmmemctl2(cvmmemctl2);
		break;
	};

	/* Invalidate guest entries in guest TLB */
	write_gc0_entrylo0(0);
	write_gc0_entrylo1(0);
	write_gc0_pagemask(0);
	for (entry = 0; entry < current_cpu_data.guest.tlbsize; entry++) {
		/* Make sure all entries differ. */
		write_gc0_index(entry);
		write_gc0_entryhi(UNIQUE_GUEST_ENTRYHI(entry));
		mtc0_tlbw_hazard();
		guest_tlb_write_indexed();
	}

	if (cvmmemctl2) {
		cvmmemctl2 &= ~CVMMEMCTL2_INHIBITTS;
		write_c0_cvmmemctl2(cvmmemctl2);
	};

	write_gc0_index(old_index);
	write_gc0_entryhi(old_entryhi);
	write_gc0_entrylo0(old_entrylo[0]);
	write_gc0_entrylo1(old_entrylo[1]);
	write_gc0_pagemask(old_pagemask);
	tlbw_use_hazard();

	local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(kvm_vz_local_flush_guesttlb_all);

/**
 * kvm_vz_save_guesttlb() - Save a range of guest TLB entries.
 * @buf:	Buffer to write TLB entries into.
 * @index:	Start index.
 * @count:	Number of entries to save.
 *
 * Save a range of guest TLB entries. The caller must ensure interrupts are
 * disabled.
 */
void kvm_vz_save_guesttlb(struct kvm_mips_tlb *buf, unsigned int index,
			  unsigned int count)
{
	unsigned int end = index + count;
	unsigned long old_entryhi, old_entrylo0, old_entrylo1, old_pagemask;
	unsigned int guestctl1 = 0;
	int old_index, i;

	/* Save registers we're about to clobber */
	old_index = read_gc0_index();
	old_entryhi = read_gc0_entryhi();
	old_entrylo0 = read_gc0_entrylo0();
	old_entrylo1 = read_gc0_entrylo1();
	old_pagemask = read_gc0_pagemask();

	/* Set root GuestID for root probe */
	htw_stop();
	set_root_gid_to_guest_gid();
	if (cpu_has_guestid)
		guestctl1 = read_c0_guestctl1();

	/* Read each entry from guest TLB */
	for (i = index; i < end; ++i, ++buf) {
		write_gc0_index(i);

		mtc0_tlbr_hazard();
		guest_tlb_read();
		tlb_read_hazard();

		if (cpu_has_guestid &&
		    (read_c0_guestctl1() ^ guestctl1) & MIPS_GCTL1_RID) {
			/* Entry invalid or belongs to another guest */
			buf->tlb_hi = UNIQUE_GUEST_ENTRYHI(i);
			buf->tlb_lo[0] = 0;
			buf->tlb_lo[1] = 0;
			buf->tlb_mask = 0;
		} else {
<<<<<<< HEAD
			/* Entry belongs to the right guest */
			buf->tlb_hi = read_gc0_entryhi();
			buf->tlb_lo[0] = read_gc0_entrylo0();
			buf->tlb_lo[1] = read_gc0_entrylo1();
			buf->tlb_mask = read_gc0_pagemask();
=======
			vpn2 = (unsigned long) opc & VPN2_MASK;
			asid = kvm_read_c0_guest_entryhi(cop0) & ASID_MASK;
			index = kvm_mips_guest_tlb_lookup(vcpu, vpn2 | asid);
			if (index < 0) {
				kvm_err("%s: get_user_failed for %p, vcpu: %p, ASID: %#lx\n",
					__func__, opc, vcpu, read_c0_entryhi());
				kvm_mips_dump_host_tlbs();
				local_irq_restore(flags);
				return KVM_INVALID_INST;
			}
			if (kvm_mips_handle_mapped_seg_tlb_fault(vcpu,
						&vcpu->arch.guest_tlb[index],
						NULL, NULL)) {
				kvm_err("%s: handling mapped seg tlb fault failed for %p, index: %u, vcpu: %p, ASID: %#lx\n",
					__func__, opc, index, vcpu,
					read_c0_entryhi());
				kvm_mips_dump_guest_tlbs(vcpu);
				local_irq_restore(flags);
				return KVM_INVALID_INST;
			}
			inst = *(opc);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
		}
	}

	/* Clear root GuestID again */
	clear_root_gid();
	htw_start();

	/* Restore clobbered registers */
	write_gc0_index(old_index);
	write_gc0_entryhi(old_entryhi);
	write_gc0_entrylo0(old_entrylo0);
	write_gc0_entrylo1(old_entrylo1);
	write_gc0_pagemask(old_pagemask);

	tlbw_use_hazard();
}
EXPORT_SYMBOL_GPL(kvm_vz_save_guesttlb);

/**
 * kvm_vz_load_guesttlb() - Save a range of guest TLB entries.
 * @buf:	Buffer to read TLB entries from.
 * @index:	Start index.
 * @count:	Number of entries to load.
 *
 * Load a range of guest TLB entries. The caller must ensure interrupts are
 * disabled.
 */
void kvm_vz_load_guesttlb(const struct kvm_mips_tlb *buf, unsigned int index,
			  unsigned int count)
{
	unsigned int end = index + count;
	unsigned long old_entryhi, old_entrylo0, old_entrylo1, old_pagemask;
	int old_index, i;

	/* Save registers we're about to clobber */
	old_index = read_gc0_index();
	old_entryhi = read_gc0_entryhi();
	old_entrylo0 = read_gc0_entrylo0();
	old_entrylo1 = read_gc0_entrylo1();
	old_pagemask = read_gc0_pagemask();

	/* Set root GuestID for root probe */
	htw_stop();
	set_root_gid_to_guest_gid();

	/* Write each entry to guest TLB */
	for (i = index; i < end; ++i, ++buf) {
		write_gc0_index(i);
		write_gc0_entryhi(buf->tlb_hi);
		write_gc0_entrylo0(buf->tlb_lo[0]);
		write_gc0_entrylo1(buf->tlb_lo[1]);
		write_gc0_pagemask(buf->tlb_mask);

		mtc0_tlbw_hazard();
		guest_tlb_write_indexed();
	}

	/* Clear root GuestID again */
	clear_root_gid();
	htw_start();

	/* Restore clobbered registers */
	write_gc0_index(old_index);
	write_gc0_entryhi(old_entryhi);
	write_gc0_entrylo0(old_entrylo0);
	write_gc0_entrylo1(old_entrylo1);
	write_gc0_pagemask(old_pagemask);

	tlbw_use_hazard();
}
EXPORT_SYMBOL_GPL(kvm_vz_load_guesttlb);

#endif

/**
 * kvm_mips_suspend_mm() - Suspend the active mm.
 * @cpu		The CPU we're running on.
 *
 * Suspend the active_mm, ready for a switch to a KVM guest virtual address
 * space. This is left active for the duration of guest context, including time
 * with interrupts enabled, so we need to be careful not to confuse e.g. cache
 * management IPIs.
 *
 * kvm_mips_resume_mm() should be called before context switching to a different
 * process so we don't need to worry about reference counting.
 *
 * This needs to be in static kernel code to avoid exporting init_mm.
 */
void kvm_mips_suspend_mm(int cpu)
{
	cpumask_clear_cpu(cpu, mm_cpumask(current->active_mm));
	current->active_mm = &init_mm;
}
EXPORT_SYMBOL_GPL(kvm_mips_suspend_mm);

/**
 * kvm_mips_resume_mm() - Resume the current process mm.
 * @cpu		The CPU we're running on.
 *
 * Resume the mm of the current process, after a switch back from a KVM guest
 * virtual address space (see kvm_mips_suspend_mm()).
 */
void kvm_mips_resume_mm(int cpu)
{
	cpumask_set_cpu(cpu, mm_cpumask(current->mm));
	current->active_mm = current->mm;
}
EXPORT_SYMBOL_GPL(kvm_mips_resume_mm);
