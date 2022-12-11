/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_IRQ_H
#define _ASM_X86_IRQ_H
/*
 *	(C) 1992, 1993 Linus Torvalds, (C) 1997 Ingo Molnar
 *
 *	IRQ/IPI changes taken from work by Thomas Radke
 *	<tomsoft@informatik.tu-chemnitz.de>
 */

#include <asm/apicdef.h>
#include <asm/irq_vectors.h>

static inline int irq_canonicalize(int irq)
{
	return ((irq == 2) ? 9 : irq);
}

extern int irq_init_percpu_irqstack(unsigned int cpu);

#define __ARCH_HAS_DO_SOFTIRQ

struct irq_desc;

<<<<<<< HEAD
extern void fixup_irqs(void);
=======
#ifdef CONFIG_HOTPLUG_CPU
#include <linux/cpumask.h>
extern int check_irq_vectors_for_cpu_disable(void);
extern void fixup_irqs(void);
extern void irq_force_complete_move(struct irq_desc *desc);
#endif
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

#ifdef CONFIG_HAVE_KVM
extern void kvm_set_posted_intr_wakeup_handler(void (*handler)(void));
extern __visible void smp_kvm_posted_intr_ipi(struct pt_regs *regs);
extern __visible void smp_kvm_posted_intr_wakeup_ipi(struct pt_regs *regs);
extern __visible void smp_kvm_posted_intr_nested_ipi(struct pt_regs *regs);
#endif

extern void (*x86_platform_ipi_callback)(void);
extern void native_init_IRQ(void);

<<<<<<< HEAD
extern void handle_irq(struct irq_desc *desc, struct pt_regs *regs);
=======
extern bool handle_irq(struct irq_desc *desc, struct pt_regs *regs);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

extern __visible unsigned int do_IRQ(struct pt_regs *regs);

extern void init_ISA_irqs(void);

extern void __init init_IRQ(void);

#ifdef CONFIG_X86_LOCAL_APIC
void arch_trigger_cpumask_backtrace(const struct cpumask *mask,
				    bool exclude_self);

extern __visible void smp_x86_platform_ipi(struct pt_regs *regs);
#define arch_trigger_cpumask_backtrace arch_trigger_cpumask_backtrace
#endif

#endif /* _ASM_X86_IRQ_H */
