// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Dynamic Ftrace based Kprobes Optimization
 *
 * Copyright (C) Hitachi Ltd., 2012
 */
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/hardirq.h>
#include <linux/preempt.h>
#include <linux/ftrace.h>

#include "common.h"

<<<<<<< HEAD
=======
static nokprobe_inline
void __skip_singlestep(struct kprobe *p, struct pt_regs *regs,
		      struct kprobe_ctlblk *kcb, unsigned long orig_ip)
{
	/*
	 * Emulate singlestep (and also recover regs->ip)
	 * as if there is a 5byte nop
	 */
	regs->ip = (unsigned long)p->addr + MCOUNT_INSN_SIZE;
	if (unlikely(p->post_handler)) {
		kcb->kprobe_status = KPROBE_HIT_SSDONE;
		p->post_handler(p, regs, 0);
	}
	__this_cpu_write(current_kprobe, NULL);
	if (orig_ip)
		regs->ip = orig_ip;
}

int skip_singlestep(struct kprobe *p, struct pt_regs *regs,
		    struct kprobe_ctlblk *kcb)
{
	if (kprobe_ftrace(p)) {
		__skip_singlestep(p, regs, kcb, 0);
		preempt_enable_no_resched();
		return 1;
	}
	return 0;
}
NOKPROBE_SYMBOL(skip_singlestep);

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
/* Ftrace callback handler for kprobes -- called under preepmt disabed */
void kprobe_ftrace_handler(unsigned long ip, unsigned long parent_ip,
			   struct ftrace_ops *ops, struct pt_regs *regs)
{
	struct kprobe *p;
	struct kprobe_ctlblk *kcb;

	/* Preempt is disabled by ftrace */
	p = get_kprobe((kprobe_opcode_t *)ip);
	if (unlikely(!p) || kprobe_disabled(p))
		return;

	kcb = get_kprobe_ctlblk();
	if (kprobe_running()) {
		kprobes_inc_nmissed_count(p);
	} else {
		unsigned long orig_ip = regs->ip;
		/* Kprobe handler expects regs->ip = ip + 1 as breakpoint hit */
		regs->ip = ip + sizeof(kprobe_opcode_t);

		/* To emulate trap based kprobes, preempt_disable here */
		preempt_disable();
		__this_cpu_write(current_kprobe, p);
		kcb->kprobe_status = KPROBE_HIT_ACTIVE;
		if (!p->pre_handler || !p->pre_handler(p, regs)) {
<<<<<<< HEAD
			/*
			 * Emulate singlestep (and also recover regs->ip)
			 * as if there is a 5byte nop
			 */
			regs->ip = (unsigned long)p->addr + MCOUNT_INSN_SIZE;
			if (unlikely(p->post_handler)) {
				kcb->kprobe_status = KPROBE_HIT_SSDONE;
				p->post_handler(p, regs, 0);
			}
			regs->ip = orig_ip;
		}
		/*
		 * If pre_handler returns !0, it changes regs->ip. We have to
		 * skip emulating post_handler.
=======
			__skip_singlestep(p, regs, kcb, orig_ip);
			preempt_enable_no_resched();
		}
		/*
		 * If pre_handler returns !0, it sets regs->ip and
		 * resets current kprobe, and keep preempt count +1.
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
		 */
		__this_cpu_write(current_kprobe, NULL);
	}
}
NOKPROBE_SYMBOL(kprobe_ftrace_handler);

int arch_prepare_kprobe_ftrace(struct kprobe *p)
{
	p->ainsn.insn = NULL;
	p->ainsn.boostable = false;
	return 0;
}
