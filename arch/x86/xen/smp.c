// SPDX-License-Identifier: GPL-2.0
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>

<<<<<<< HEAD
=======
#include <asm/paravirt.h>
#include <asm/desc.h>
#include <asm/pgtable.h>
#include <asm/cpu.h>

#include <xen/interface/xen.h>
#include <xen/interface/vcpu.h>
#include <xen/interface/xenpmu.h>

#include <asm/spec-ctrl.h>
#include <asm/xen/interface.h>
#include <asm/xen/hypercall.h>

#include <xen/xen.h>
#include <xen/page.h>
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#include <xen/events.h>

#include <xen/hvc-console.h>
#include "xen-ops.h"
#include "smp.h"

static DEFINE_PER_CPU(struct xen_common_irq, xen_resched_irq) = { .irq = -1 };
static DEFINE_PER_CPU(struct xen_common_irq, xen_callfunc_irq) = { .irq = -1 };
static DEFINE_PER_CPU(struct xen_common_irq, xen_callfuncsingle_irq) = { .irq = -1 };
static DEFINE_PER_CPU(struct xen_common_irq, xen_debug_irq) = { .irq = -1 };

static irqreturn_t xen_call_function_interrupt(int irq, void *dev_id);
static irqreturn_t xen_call_function_single_interrupt(int irq, void *dev_id);

/*
 * Reschedule call back.
 */
static irqreturn_t xen_reschedule_interrupt(int irq, void *dev_id)
{
	inc_irq_stat(irq_resched_count);
	scheduler_ipi();

	return IRQ_HANDLED;
}

<<<<<<< HEAD
void xen_smp_intr_free(unsigned int cpu)
=======
static void cpu_bringup(void)
{
	int cpu;

	cpu_init();
	touch_softlockup_watchdog();
	preempt_disable();

	/* PVH runs in ring 0 and allows us to do native syscalls. Yay! */
	if (!xen_feature(XENFEAT_supervisor_mode_kernel)) {
		xen_enable_sysenter();
		xen_enable_syscall();
	}
	cpu = smp_processor_id();
	smp_store_cpu_info(cpu);
	cpu_data(cpu).x86_max_cores = 1;
	set_cpu_sibling_map(cpu);

	speculative_store_bypass_ht_init();

	xen_setup_cpu_clockevents();

	notify_cpu_starting(cpu);

	set_cpu_online(cpu, true);

	cpu_set_state_online(cpu);  /* Implies full memory barrier. */

	/* We can take interrupts now: we're officially "up". */
	local_irq_enable();
}

/*
 * Note: cpu parameter is only relevant for PVH. The reason for passing it
 * is we can't do smp_processor_id until the percpu segments are loaded, for
 * which we need the cpu number! So we pass it in rdi as first parameter.
 */
asmlinkage __visible void cpu_bringup_and_idle(int cpu)
{
#ifdef CONFIG_XEN_PVH
	if (xen_feature(XENFEAT_auto_translated_physmap) &&
	    xen_feature(XENFEAT_supervisor_mode_kernel))
		xen_pvh_secondary_vcpu_init(cpu);
#endif
	cpu_bringup();
	cpu_startup_entry(CPUHP_ONLINE);
}

static void xen_smp_intr_free(unsigned int cpu)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
{
	if (per_cpu(xen_resched_irq, cpu).irq >= 0) {
		unbind_from_irqhandler(per_cpu(xen_resched_irq, cpu).irq, NULL);
		per_cpu(xen_resched_irq, cpu).irq = -1;
		kfree(per_cpu(xen_resched_irq, cpu).name);
		per_cpu(xen_resched_irq, cpu).name = NULL;
	}
	if (per_cpu(xen_callfunc_irq, cpu).irq >= 0) {
		unbind_from_irqhandler(per_cpu(xen_callfunc_irq, cpu).irq, NULL);
		per_cpu(xen_callfunc_irq, cpu).irq = -1;
		kfree(per_cpu(xen_callfunc_irq, cpu).name);
		per_cpu(xen_callfunc_irq, cpu).name = NULL;
	}
	if (per_cpu(xen_debug_irq, cpu).irq >= 0) {
		unbind_from_irqhandler(per_cpu(xen_debug_irq, cpu).irq, NULL);
		per_cpu(xen_debug_irq, cpu).irq = -1;
		kfree(per_cpu(xen_debug_irq, cpu).name);
		per_cpu(xen_debug_irq, cpu).name = NULL;
	}
	if (per_cpu(xen_callfuncsingle_irq, cpu).irq >= 0) {
		unbind_from_irqhandler(per_cpu(xen_callfuncsingle_irq, cpu).irq,
				       NULL);
		per_cpu(xen_callfuncsingle_irq, cpu).irq = -1;
		kfree(per_cpu(xen_callfuncsingle_irq, cpu).name);
		per_cpu(xen_callfuncsingle_irq, cpu).name = NULL;
	}
}

int xen_smp_intr_init(unsigned int cpu)
{
	int rc;
	char *resched_name, *callfunc_name, *debug_name;

	resched_name = kasprintf(GFP_KERNEL, "resched%d", cpu);
	rc = bind_ipi_to_irqhandler(XEN_RESCHEDULE_VECTOR,
				    cpu,
				    xen_reschedule_interrupt,
				    IRQF_PERCPU|IRQF_NOBALANCING,
				    resched_name,
				    NULL);
	if (rc < 0)
		goto fail;
	per_cpu(xen_resched_irq, cpu).irq = rc;
	per_cpu(xen_resched_irq, cpu).name = resched_name;

	callfunc_name = kasprintf(GFP_KERNEL, "callfunc%d", cpu);
	rc = bind_ipi_to_irqhandler(XEN_CALL_FUNCTION_VECTOR,
				    cpu,
				    xen_call_function_interrupt,
				    IRQF_PERCPU|IRQF_NOBALANCING,
				    callfunc_name,
				    NULL);
	if (rc < 0)
		goto fail;
	per_cpu(xen_callfunc_irq, cpu).irq = rc;
	per_cpu(xen_callfunc_irq, cpu).name = callfunc_name;

	debug_name = kasprintf(GFP_KERNEL, "debug%d", cpu);
	rc = bind_virq_to_irqhandler(VIRQ_DEBUG, cpu, xen_debug_interrupt,
				     IRQF_PERCPU | IRQF_NOBALANCING,
				     debug_name, NULL);
	if (rc < 0)
		goto fail;
	per_cpu(xen_debug_irq, cpu).irq = rc;
	per_cpu(xen_debug_irq, cpu).name = debug_name;

	callfunc_name = kasprintf(GFP_KERNEL, "callfuncsingle%d", cpu);
	rc = bind_ipi_to_irqhandler(XEN_CALL_FUNCTION_SINGLE_VECTOR,
				    cpu,
				    xen_call_function_single_interrupt,
				    IRQF_PERCPU|IRQF_NOBALANCING,
				    callfunc_name,
				    NULL);
	if (rc < 0)
		goto fail;
	per_cpu(xen_callfuncsingle_irq, cpu).irq = rc;
	per_cpu(xen_callfuncsingle_irq, cpu).name = callfunc_name;

	return 0;

 fail:
	xen_smp_intr_free(cpu);
	return rc;
}

void __init xen_smp_cpus_done(unsigned int max_cpus)
{
	int cpu, rc, count = 0;

	if (xen_hvm_domain())
		native_smp_cpus_done(max_cpus);
	else
		calculate_max_logical_packages();

	if (xen_have_vcpu_info_placement)
		return;

<<<<<<< HEAD
	for_each_online_cpu(cpu) {
		if (xen_vcpu_nr(cpu) < MAX_VIRT_CPUS)
=======
	for (i = 0; i < nr_cpu_ids; i++) {
		rc = HYPERVISOR_vcpu_op(VCPUOP_is_up, i, NULL);
		if (rc >= 0) {
			num_processors++;
			set_cpu_possible(i, true);
		}
	}
}

static void __init xen_filter_cpu_maps(void)
{
	int i, rc;
	unsigned int subtract = 0;

	if (!xen_initial_domain())
		return;

	num_processors = 0;
	disabled_cpus = 0;
	for (i = 0; i < nr_cpu_ids; i++) {
		rc = HYPERVISOR_vcpu_op(VCPUOP_is_up, i, NULL);
		if (rc >= 0) {
			num_processors++;
			set_cpu_possible(i, true);
		} else {
			set_cpu_possible(i, false);
			set_cpu_present(i, false);
			subtract++;
		}
	}
#ifdef CONFIG_HOTPLUG_CPU
	/* This is akin to using 'nr_cpus' on the Linux command line.
	 * Which is OK as when we use 'dom0_max_vcpus=X' we can only
	 * have up to X, while nr_cpu_ids is greater than X. This
	 * normally is not a problem, except when CPU hotplugging
	 * is involved and then there might be more than X CPUs
	 * in the guest - which will not work as there is no
	 * hypercall to expand the max number of VCPUs an already
	 * running guest has. So cap it up to X. */
	if (subtract)
		nr_cpu_ids = nr_cpu_ids - subtract;
#endif

}

static void __init xen_smp_prepare_boot_cpu(void)
{
	BUG_ON(smp_processor_id() != 0);
	native_smp_prepare_boot_cpu();

	if (xen_pv_domain()) {
		if (!xen_feature(XENFEAT_writable_page_tables))
			/* We've switched to the "real" per-cpu gdt, so make
			 * sure the old memory can be recycled. */
			make_lowmem_page_readwrite(xen_initial_gdt);

#ifdef CONFIG_X86_32
		/*
		 * Xen starts us with XEN_FLAT_RING1_DS, but linux code
		 * expects __USER_DS
		 */
		loadsegment(ds, __USER_DS);
		loadsegment(es, __USER_DS);
#endif

		xen_filter_cpu_maps();
		xen_setup_vcpu_info_placement();
	}
	/*
	 * The alternative logic (which patches the unlock/lock) runs before
	 * the smp bootup up code is activated. Hence we need to set this up
	 * the core kernel is being patched. Otherwise we will have only
	 * modules patched but not core code.
	 */
	xen_init_spinlocks();
}

static void __init xen_smp_prepare_cpus(unsigned int max_cpus)
{
	unsigned cpu;
	unsigned int i;

	if (skip_ioapic_setup) {
		char *m = (max_cpus == 0) ?
			"The nosmp parameter is incompatible with Xen; " \
			"use Xen dom0_max_vcpus=1 parameter" :
			"The noapic parameter is incompatible with Xen";

		xen_raw_printk(m);
		panic(m);
	}
	xen_init_lock_cpu(0);

	smp_store_boot_cpu_info();
	cpu_data(0).x86_max_cores = 1;

	for_each_possible_cpu(i) {
		zalloc_cpumask_var(&per_cpu(cpu_sibling_map, i), GFP_KERNEL);
		zalloc_cpumask_var(&per_cpu(cpu_core_map, i), GFP_KERNEL);
		zalloc_cpumask_var(&per_cpu(cpu_llc_shared_map, i), GFP_KERNEL);
	}
	set_cpu_sibling_map(0);

	speculative_store_bypass_ht_init();

	xen_pmu_init(0);

	if (xen_smp_intr_init(0))
		BUG();

	if (!alloc_cpumask_var(&xen_cpu_initialized_map, GFP_KERNEL))
		panic("could not allocate xen_cpu_initialized_map\n");

	cpumask_copy(xen_cpu_initialized_map, cpumask_of(0));

	/* Restrict the possible_map according to max_cpus. */
	while ((num_possible_cpus() > 1) && (num_possible_cpus() > max_cpus)) {
		for (cpu = nr_cpu_ids - 1; !cpu_possible(cpu); cpu--)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
			continue;

		rc = cpu_down(cpu);

		if (rc == 0) {
			/*
			 * Reset vcpu_info so this cpu cannot be onlined again.
			 */
			xen_vcpu_info_reset(cpu);
			count++;
		} else {
			pr_warn("%s: failed to bring CPU %d down, error %d\n",
				__func__, cpu, rc);
		}
	}
	WARN(count, "%s: brought %d CPUs offline\n", __func__, count);
}

void xen_smp_send_reschedule(int cpu)
{
	xen_send_IPI_one(cpu, XEN_RESCHEDULE_VECTOR);
}

static void __xen_send_IPI_mask(const struct cpumask *mask,
			      int vector)
{
	unsigned cpu;

	for_each_cpu_and(cpu, mask, cpu_online_mask)
		xen_send_IPI_one(cpu, vector);
}

void xen_smp_send_call_function_ipi(const struct cpumask *mask)
{
	int cpu;

	__xen_send_IPI_mask(mask, XEN_CALL_FUNCTION_VECTOR);

	/* Make sure other vcpus get a chance to run if they need to. */
	for_each_cpu(cpu, mask) {
		if (xen_vcpu_stolen(cpu)) {
			HYPERVISOR_sched_op(SCHEDOP_yield, NULL);
			break;
		}
	}
}

void xen_smp_send_call_function_single_ipi(int cpu)
{
	__xen_send_IPI_mask(cpumask_of(cpu),
			  XEN_CALL_FUNCTION_SINGLE_VECTOR);
}

static inline int xen_map_vector(int vector)
{
	int xen_vector;

	switch (vector) {
	case RESCHEDULE_VECTOR:
		xen_vector = XEN_RESCHEDULE_VECTOR;
		break;
	case CALL_FUNCTION_VECTOR:
		xen_vector = XEN_CALL_FUNCTION_VECTOR;
		break;
	case CALL_FUNCTION_SINGLE_VECTOR:
		xen_vector = XEN_CALL_FUNCTION_SINGLE_VECTOR;
		break;
	case IRQ_WORK_VECTOR:
		xen_vector = XEN_IRQ_WORK_VECTOR;
		break;
#ifdef CONFIG_X86_64
	case NMI_VECTOR:
	case APIC_DM_NMI: /* Some use that instead of NMI_VECTOR */
		xen_vector = XEN_NMI_VECTOR;
		break;
#endif
	default:
		xen_vector = -1;
		printk(KERN_ERR "xen: vector 0x%x is not implemented\n",
			vector);
	}

	return xen_vector;
}

void xen_send_IPI_mask(const struct cpumask *mask,
			      int vector)
{
	int xen_vector = xen_map_vector(vector);

	if (xen_vector >= 0)
		__xen_send_IPI_mask(mask, xen_vector);
}

void xen_send_IPI_all(int vector)
{
	int xen_vector = xen_map_vector(vector);

	if (xen_vector >= 0)
		__xen_send_IPI_mask(cpu_online_mask, xen_vector);
}

void xen_send_IPI_self(int vector)
{
	int xen_vector = xen_map_vector(vector);

	if (xen_vector >= 0)
		xen_send_IPI_one(smp_processor_id(), xen_vector);
}

void xen_send_IPI_mask_allbutself(const struct cpumask *mask,
				int vector)
{
	unsigned cpu;
	unsigned int this_cpu = smp_processor_id();
	int xen_vector = xen_map_vector(vector);

	if (!(num_online_cpus() > 1) || (xen_vector < 0))
		return;

	for_each_cpu_and(cpu, mask, cpu_online_mask) {
		if (this_cpu == cpu)
			continue;

		xen_send_IPI_one(cpu, xen_vector);
	}
}

void xen_send_IPI_allbutself(int vector)
{
	xen_send_IPI_mask_allbutself(cpu_online_mask, vector);
}

static irqreturn_t xen_call_function_interrupt(int irq, void *dev_id)
{
	irq_enter();
	generic_smp_call_function_interrupt();
	inc_irq_stat(irq_call_count);
	irq_exit();

	return IRQ_HANDLED;
}

static irqreturn_t xen_call_function_single_interrupt(int irq, void *dev_id)
{
	irq_enter();
	generic_smp_call_function_single_interrupt();
	inc_irq_stat(irq_call_count);
	irq_exit();

	return IRQ_HANDLED;
}
