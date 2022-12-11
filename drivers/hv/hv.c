// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2009, Microsoft Corporation.
 *
 * Authors:
 *   Haiyang Zhang <haiyangz@microsoft.com>
 *   Hank Janssen  <hjanssen@microsoft.com>
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/hyperv.h>
#include <linux/version.h>
#include <linux/random.h>
#include <linux/clockchips.h>
#include <clocksource/hyperv_timer.h>
#include <asm/mshyperv.h>
#include <asm/nospec-branch.h>
#include "hyperv_vmbus.h"

/* The one and only */
<<<<<<< HEAD
struct hv_context hv_context;
=======
struct hv_context hv_context = {
	.synic_initialized	= false,
	.hypercall_page		= NULL,
};

#define HV_TIMER_FREQUENCY (10 * 1000 * 1000) /* 100ns period */
#define HV_MAX_MAX_DELTA_TICKS 0xffffffff
#define HV_MIN_DELTA_TICKS 1

/*
 * query_hypervisor_info - Get version info of the windows hypervisor
 */
unsigned int host_info_eax;
unsigned int host_info_ebx;
unsigned int host_info_ecx;
unsigned int host_info_edx;

static int query_hypervisor_info(void)
{
	unsigned int eax;
	unsigned int ebx;
	unsigned int ecx;
	unsigned int edx;
	unsigned int max_leaf;
	unsigned int op;

	/*
	* Its assumed that this is called after confirming that Viridian
	* is present. Query id and revision.
	*/
	eax = 0;
	ebx = 0;
	ecx = 0;
	edx = 0;
	op = HVCPUID_VENDOR_MAXFUNCTION;
	cpuid(op, &eax, &ebx, &ecx, &edx);

	max_leaf = eax;

	if (max_leaf >= HVCPUID_VERSION) {
		eax = 0;
		ebx = 0;
		ecx = 0;
		edx = 0;
		op = HVCPUID_VERSION;
		cpuid(op, &eax, &ebx, &ecx, &edx);
		host_info_eax = eax;
		host_info_ebx = ebx;
		host_info_ecx = ecx;
		host_info_edx = edx;
	}
	return max_leaf;
}

/*
 * do_hypercall- Invoke the specified hypercall
 */
static u64 do_hypercall(u64 control, void *input, void *output)
{
	u64 input_address = (input) ? virt_to_phys(input) : 0;
	u64 output_address = (output) ? virt_to_phys(output) : 0;
	void *hypercall_page = hv_context.hypercall_page;
#ifdef CONFIG_X86_64
	u64 hv_status = 0;

	if (!hypercall_page)
		return (u64)ULLONG_MAX;

	__asm__ __volatile__("mov %0, %%r8" : : "r" (output_address) : "r8");
	__asm__ __volatile__(CALL_NOSPEC :
			     "=a" (hv_status) :
			     "c" (control), "d" (input_address),
			     THUNK_TARGET(hypercall_page));

	return hv_status;

#else

	u32 control_hi = control >> 32;
	u32 control_lo = control & 0xFFFFFFFF;
	u32 hv_status_hi = 1;
	u32 hv_status_lo = 1;
	u32 input_address_hi = input_address >> 32;
	u32 input_address_lo = input_address & 0xFFFFFFFF;
	u32 output_address_hi = output_address >> 32;
	u32 output_address_lo = output_address & 0xFFFFFFFF;

	if (!hypercall_page)
		return (u64)ULLONG_MAX;

	__asm__ __volatile__ (CALL_NOSPEC : "=d"(hv_status_hi),
			      "=a"(hv_status_lo) : "d" (control_hi),
			      "a" (control_lo), "b" (input_address_hi),
			      "c" (input_address_lo), "D"(output_address_hi),
			      "S"(output_address_lo),
			      THUNK_TARGET(hypercall_page));

	return hv_status_lo | ((u64)hv_status_hi << 32);
#endif /* !x86_64 */
}

#ifdef CONFIG_X86_64
static cycle_t read_hv_clock_tsc(struct clocksource *arg)
{
	cycle_t current_tick;
	struct ms_hyperv_tsc_page *tsc_pg = hv_context.tsc_page;

	if (tsc_pg->tsc_sequence != -1) {
		/*
		 * Use the tsc page to compute the value.
		 */

		while (1) {
			cycle_t tmp;
			u32 sequence = tsc_pg->tsc_sequence;
			u64 cur_tsc;
			u64 scale = tsc_pg->tsc_scale;
			s64 offset = tsc_pg->tsc_offset;

			rdtscll(cur_tsc);
			/* current_tick = ((cur_tsc *scale) >> 64) + offset */
			asm("mulq %3"
				: "=d" (current_tick), "=a" (tmp)
				: "a" (cur_tsc), "r" (scale));

			current_tick += offset;
			if (tsc_pg->tsc_sequence == sequence)
				return current_tick;

			if (tsc_pg->tsc_sequence != -1)
				continue;
			/*
			 * Fallback using MSR method.
			 */
			break;
		}
	}
	rdmsrl(HV_X64_MSR_TIME_REF_COUNT, current_tick);
	return current_tick;
}

static struct clocksource hyperv_cs_tsc = {
		.name           = "hyperv_clocksource_tsc_page",
		.rating         = 425,
		.read           = read_hv_clock_tsc,
		.mask           = CLOCKSOURCE_MASK(64),
		.flags          = CLOCK_SOURCE_IS_CONTINUOUS,
};
#endif

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

/*
 * hv_init - Main initialization routine.
 *
 * This routine must be called before any other routines in here are called
 */
int hv_init(void)
{
<<<<<<< HEAD
	hv_context.cpu_context = alloc_percpu(struct hv_per_cpu_context);
	if (!hv_context.cpu_context)
		return -ENOMEM;
	return 0;
=======
	int max_leaf;
	union hv_x64_msr_hypercall_contents hypercall_msr;
	void *virtaddr = NULL;

	memset(hv_context.synic_event_page, 0, sizeof(void *) * NR_CPUS);
	memset(hv_context.synic_message_page, 0,
	       sizeof(void *) * NR_CPUS);
	memset(hv_context.post_msg_page, 0,
	       sizeof(void *) * NR_CPUS);
	memset(hv_context.vp_index, 0,
	       sizeof(int) * NR_CPUS);
	memset(hv_context.event_dpc, 0,
	       sizeof(void *) * NR_CPUS);
	memset(hv_context.clk_evt, 0,
	       sizeof(void *) * NR_CPUS);

	max_leaf = query_hypervisor_info();

	/*
	 * Write our OS ID.
	 */
	hv_context.guestid = generate_guest_id(0, LINUX_VERSION_CODE, 0);
	wrmsrl(HV_X64_MSR_GUEST_OS_ID, hv_context.guestid);

	/* See if the hypercall page is already set */
	rdmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);

	virtaddr = __vmalloc(PAGE_SIZE, GFP_KERNEL, PAGE_KERNEL_RX);

	if (!virtaddr)
		goto cleanup;

	hypercall_msr.enable = 1;

	hypercall_msr.guest_physical_address = vmalloc_to_pfn(virtaddr);
	wrmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);

	/* Confirm that hypercall page did get setup. */
	hypercall_msr.as_uint64 = 0;
	rdmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);

	if (!hypercall_msr.enable)
		goto cleanup;

	hv_context.hypercall_page = virtaddr;

#ifdef CONFIG_X86_64
	if (ms_hyperv.features & HV_X64_MSR_REFERENCE_TSC_AVAILABLE) {
		union hv_x64_msr_hypercall_contents tsc_msr;
		void *va_tsc;

		va_tsc = __vmalloc(PAGE_SIZE, GFP_KERNEL, PAGE_KERNEL);
		if (!va_tsc)
			goto cleanup;
		hv_context.tsc_page = va_tsc;

		rdmsrl(HV_X64_MSR_REFERENCE_TSC, tsc_msr.as_uint64);

		tsc_msr.enable = 1;
		tsc_msr.guest_physical_address = vmalloc_to_pfn(va_tsc);

		wrmsrl(HV_X64_MSR_REFERENCE_TSC, tsc_msr.as_uint64);
		clocksource_register_hz(&hyperv_cs_tsc, NSEC_PER_SEC/100);
	}
#endif
	return 0;

cleanup:
	if (virtaddr) {
		if (hypercall_msr.enable) {
			hypercall_msr.as_uint64 = 0;
			wrmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);
		}

		vfree(virtaddr);
	}

	return -ENOTSUPP;
}

/*
 * hv_cleanup - Cleanup routine.
 *
 * This routine is called normally during driver unloading or exiting.
 */
void hv_cleanup(bool crash)
{
	union hv_x64_msr_hypercall_contents hypercall_msr;

	/* Reset our OS id */
	wrmsrl(HV_X64_MSR_GUEST_OS_ID, 0);

	if (hv_context.hypercall_page) {
		hypercall_msr.as_uint64 = 0;
		wrmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);
		if (!crash)
			vfree(hv_context.hypercall_page);
		hv_context.hypercall_page = NULL;
	}

#ifdef CONFIG_X86_64
	/*
	 * Cleanup the TSC page based CS.
	 */
	if (ms_hyperv.features & HV_X64_MSR_REFERENCE_TSC_AVAILABLE) {
		/*
		 * Crash can happen in an interrupt context and unregistering
		 * a clocksource is impossible and redundant in this case.
		 */
		if (!oops_in_progress) {
			clocksource_change_rating(&hyperv_cs_tsc, 10);
			clocksource_unregister(&hyperv_cs_tsc);
		}

		hypercall_msr.as_uint64 = 0;
		wrmsrl(HV_X64_MSR_REFERENCE_TSC, hypercall_msr.as_uint64);
		if (!crash) {
			vfree(hv_context.tsc_page);
			hv_context.tsc_page = NULL;
		}
	}
#endif
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}

/*
 * hv_post_message - Post a message using the hypervisor message IPC.
 *
 * This involves a hypercall.
 */
int hv_post_message(union hv_connection_id connection_id,
		  enum hv_message_type message_type,
		  void *payload, size_t payload_size)
{
	struct hv_input_post_message *aligned_msg;
	struct hv_per_cpu_context *hv_cpu;
	u64 status;

	if (payload_size > HV_MESSAGE_PAYLOAD_BYTE_COUNT)
		return -EMSGSIZE;

	hv_cpu = get_cpu_ptr(hv_context.cpu_context);
	aligned_msg = hv_cpu->post_msg_page;
	aligned_msg->connectionid = connection_id;
	aligned_msg->reserved = 0;
	aligned_msg->message_type = message_type;
	aligned_msg->payload_size = payload_size;
	memcpy((void *)aligned_msg->payload, payload, payload_size);

	status = hv_do_hypercall(HVCALL_POST_MESSAGE, aligned_msg, NULL);

	/* Preemption must remain disabled until after the hypercall
	 * so some other thread can't get scheduled onto this cpu and
	 * corrupt the per-cpu post_msg_page
	 */
	put_cpu_ptr(hv_cpu);

	return status & 0xFFFF;
}

int hv_synic_alloc(void)
{
	int cpu;
	struct hv_per_cpu_context *hv_cpu;

	/*
	 * First, zero all per-cpu memory areas so hv_synic_free() can
	 * detect what memory has been allocated and cleanup properly
	 * after any failures.
	 */
	for_each_present_cpu(cpu) {
		hv_cpu = per_cpu_ptr(hv_context.cpu_context, cpu);
		memset(hv_cpu, 0, sizeof(*hv_cpu));
	}

	hv_context.hv_numa_map = kcalloc(nr_node_ids, sizeof(struct cpumask),
					 GFP_KERNEL);
	if (hv_context.hv_numa_map == NULL) {
		pr_err("Unable to allocate NUMA map\n");
		goto err;
	}

	for_each_present_cpu(cpu) {
<<<<<<< HEAD
		hv_cpu = per_cpu_ptr(hv_context.cpu_context, cpu);
=======
		hv_context.event_dpc[cpu] = kmalloc(size, GFP_ATOMIC);
		if (hv_context.event_dpc[cpu] == NULL) {
			pr_err("Unable to allocate event dpc\n");
			goto err;
		}
		tasklet_init(hv_context.event_dpc[cpu], vmbus_on_event, cpu);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

		tasklet_init(&hv_cpu->msg_dpc,
			     vmbus_on_msg_dpc, (unsigned long) hv_cpu);

		hv_cpu->synic_message_page =
			(void *)get_zeroed_page(GFP_ATOMIC);
		if (hv_cpu->synic_message_page == NULL) {
			pr_err("Unable to allocate SYNIC message page\n");
			goto err;
		}

		hv_cpu->synic_event_page = (void *)get_zeroed_page(GFP_ATOMIC);
		if (hv_cpu->synic_event_page == NULL) {
			pr_err("Unable to allocate SYNIC event page\n");
			goto err;
		}

		hv_cpu->post_msg_page = (void *)get_zeroed_page(GFP_ATOMIC);
		if (hv_cpu->post_msg_page == NULL) {
			pr_err("Unable to allocate post msg page\n");
			goto err;
		}

<<<<<<< HEAD
		INIT_LIST_HEAD(&hv_cpu->chan_list);
=======
		INIT_LIST_HEAD(&hv_context.percpu_list[cpu]);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	}

	return 0;
err:
	/*
	 * Any memory allocations that succeeded will be freed when
	 * the caller cleans up by calling hv_synic_free()
	 */
	return -ENOMEM;
}


void hv_synic_free(void)
{
	int cpu;

	for_each_present_cpu(cpu) {
		struct hv_per_cpu_context *hv_cpu
			= per_cpu_ptr(hv_context.cpu_context, cpu);

		free_page((unsigned long)hv_cpu->synic_event_page);
		free_page((unsigned long)hv_cpu->synic_message_page);
		free_page((unsigned long)hv_cpu->post_msg_page);
	}

	kfree(hv_context.hv_numa_map);
<<<<<<< HEAD
=======
	for_each_present_cpu(cpu)
		hv_synic_free_cpu(cpu);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}

/*
 * hv_synic_init - Initialize the Synthetic Interrupt Controller.
 *
 * If it is already initialized by another entity (ie x2v shim), we need to
 * retrieve the initialized message and event pages.  Otherwise, we create and
 * initialize the message and event pages.
 */
void hv_synic_enable_regs(unsigned int cpu)
{
	struct hv_per_cpu_context *hv_cpu
		= per_cpu_ptr(hv_context.cpu_context, cpu);
	union hv_synic_simp simp;
	union hv_synic_siefp siefp;
	union hv_synic_sint shared_sint;
	union hv_synic_scontrol sctrl;

	/* Setup the Synic's message page */
	hv_get_simp(simp.as_uint64);
	simp.simp_enabled = 1;
	simp.base_simp_gpa = virt_to_phys(hv_cpu->synic_message_page)
		>> PAGE_SHIFT;

	hv_set_simp(simp.as_uint64);

	/* Setup the Synic's event page */
	hv_get_siefp(siefp.as_uint64);
	siefp.siefp_enabled = 1;
	siefp.base_siefp_gpa = virt_to_phys(hv_cpu->synic_event_page)
		>> PAGE_SHIFT;

	hv_set_siefp(siefp.as_uint64);

	/* Setup the shared SINT. */
	hv_get_synint_state(VMBUS_MESSAGE_SINT, shared_sint.as_uint64);

	shared_sint.vector = HYPERVISOR_CALLBACK_VECTOR;
	shared_sint.masked = false;
	if (ms_hyperv.hints & HV_DEPRECATING_AEOI_RECOMMENDED)
		shared_sint.auto_eoi = false;
	else
		shared_sint.auto_eoi = true;

	hv_set_synint_state(VMBUS_MESSAGE_SINT, shared_sint.as_uint64);

	/* Enable the global synic bit */
	hv_get_synic_state(sctrl.as_uint64);
	sctrl.enable = 1;

<<<<<<< HEAD
	hv_set_synic_state(sctrl.as_uint64);
=======
	wrmsrl(HV_X64_MSR_SCONTROL, sctrl.as_uint64);

	hv_context.synic_initialized = true;

	/*
	 * Setup the mapping between Hyper-V's notion
	 * of cpuid and Linux' notion of cpuid.
	 * This array will be indexed using Linux cpuid.
	 */
	rdmsrl(HV_X64_MSR_VP_INDEX, vp_index);
	hv_context.vp_index[cpu] = (u32)vp_index;

	/*
	 * Register the per-cpu clockevent source.
	 */
	if (ms_hyperv.features & HV_X64_MSR_SYNTIMER_AVAILABLE)
		clockevents_config_and_register(hv_context.clk_evt[cpu],
						HV_TIMER_FREQUENCY,
						HV_MIN_DELTA_TICKS,
						HV_MAX_MAX_DELTA_TICKS);
	return;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
}

int hv_synic_init(unsigned int cpu)
{
	hv_synic_enable_regs(cpu);

	hv_stimer_init(cpu);

	return 0;
}

/*
 * hv_synic_cleanup - Cleanup routine for hv_synic_init().
 */
void hv_synic_disable_regs(unsigned int cpu)
{
	union hv_synic_sint shared_sint;
	union hv_synic_simp simp;
	union hv_synic_siefp siefp;
	union hv_synic_scontrol sctrl;

	hv_get_synint_state(VMBUS_MESSAGE_SINT, shared_sint.as_uint64);

	shared_sint.masked = 1;

	/* Need to correctly cleanup in the case of SMP!!! */
	/* Disable the interrupt */
	hv_set_synint_state(VMBUS_MESSAGE_SINT, shared_sint.as_uint64);

	hv_get_simp(simp.as_uint64);
	simp.simp_enabled = 0;
	simp.base_simp_gpa = 0;

	hv_set_simp(simp.as_uint64);

	hv_get_siefp(siefp.as_uint64);
	siefp.siefp_enabled = 0;
	siefp.base_siefp_gpa = 0;

	hv_set_siefp(siefp.as_uint64);

	/* Disable the global synic bit */
	hv_get_synic_state(sctrl.as_uint64);
	sctrl.enable = 0;
	hv_set_synic_state(sctrl.as_uint64);
}

int hv_synic_cleanup(unsigned int cpu)
{
	struct vmbus_channel *channel, *sc;
	bool channel_found = false;
	unsigned long flags;

	/*
	 * Hyper-V does not provide a way to change the connect CPU once
	 * it is set; we must prevent the connect CPU from going offline
	 * while the VM is running normally. But in the panic or kexec()
	 * path where the vmbus is already disconnected, the CPU must be
	 * allowed to shut down.
	 */
	if (cpu == VMBUS_CONNECT_CPU &&
	    vmbus_connection.conn_state == CONNECTED)
		return -EBUSY;

	/*
	 * Search for channels which are bound to the CPU we're about to
	 * cleanup. In case we find one and vmbus is still connected we need to
	 * fail, this will effectively prevent CPU offlining. There is no way
	 * we can re-bind channels to different CPUs for now.
	 */
	mutex_lock(&vmbus_connection.channel_mutex);
	list_for_each_entry(channel, &vmbus_connection.chn_list, listentry) {
		if (channel->target_cpu == cpu) {
			channel_found = true;
			break;
		}
		spin_lock_irqsave(&channel->lock, flags);
		list_for_each_entry(sc, &channel->sc_list, sc_list) {
			if (sc->target_cpu == cpu) {
				channel_found = true;
				break;
			}
		}
		spin_unlock_irqrestore(&channel->lock, flags);
		if (channel_found)
			break;
	}
	mutex_unlock(&vmbus_connection.channel_mutex);

	if (channel_found && vmbus_connection.conn_state == CONNECTED)
		return -EBUSY;

	hv_stimer_cleanup(cpu);

	hv_synic_disable_regs(cpu);

	return 0;
}
