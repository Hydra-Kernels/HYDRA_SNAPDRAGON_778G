/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_SWITCH_TO_H
#define _ASM_X86_SWITCH_TO_H

<<<<<<< HEAD
#include <linux/sched/task_stack.h>
=======
#include <asm/nospec-branch.h>
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

struct task_struct; /* one of the stranger aspects of C forward declarations */

struct task_struct *__switch_to_asm(struct task_struct *prev,
				    struct task_struct *next);

__visible struct task_struct *__switch_to(struct task_struct *prev,
					  struct task_struct *next);

/* This runs runs on the previous thread's stack. */
static inline void prepare_switch_to(struct task_struct *next)
{
#ifdef CONFIG_VMAP_STACK
	/*
	 * If we switch to a stack that has a top-level paging entry
	 * that is not present in the current mm, the resulting #PF will
	 * will be promoted to a double-fault and we'll panic.  Probe
	 * the new stack now so that vmalloc_fault can fix up the page
	 * tables if needed.  This can only happen if we use a stack
	 * in vmap space.
	 *
	 * We assume that the stack is aligned so that it never spans
	 * more than one top-level paging entry.
	 *
	 * To minimize cache pollution, just follow the stack pointer.
	 */
	READ_ONCE(*(unsigned char *)next->thread.sp);
#endif
}

asmlinkage void ret_from_fork(void);

#ifdef CONFIG_RETPOLINE
	/*
	 * When switching from a shallower to a deeper call stack
	 * the RSB may either underflow or use entries populated
	 * with userspace addresses. On CPUs where those concerns
	 * exist, overwrite the RSB with entries which capture
	 * speculative execution to prevent attack.
	 */
#define __retpoline_fill_return_buffer					\
	ALTERNATIVE("jmp 910f",						\
		__stringify(__FILL_RETURN_BUFFER(%%ebx, RSB_CLEAR_LOOPS, %%esp)),\
		X86_FEATURE_RSB_CTXSW)					\
	"910:\n\t"
#else
#define __retpoline_fill_return_buffer
#endif

/*
 * This is the structure pointed to by thread.sp for an inactive task.  The
 * order of the fields must match the code in __switch_to_asm().
 */
struct inactive_task_frame {
#ifdef CONFIG_X86_64
	unsigned long r15;
	unsigned long r14;
	unsigned long r13;
	unsigned long r12;
#else
	unsigned long flags;
	unsigned long si;
	unsigned long di;
#endif
	unsigned long bx;

	/*
	 * These two fields must be together.  They form a stack frame header,
	 * needed by get_frame_pointer().
	 */
	unsigned long bp;
	unsigned long ret_addr;
};

struct fork_frame {
	struct inactive_task_frame frame;
	struct pt_regs regs;
};

#define switch_to(prev, next, last)					\
do {									\
	prepare_switch_to(next);					\
									\
<<<<<<< HEAD
	((last) = __switch_to_asm((prev), (next)));			\
=======
	asm volatile("pushfl\n\t"		/* save    flags */	\
		     "pushl %%ebp\n\t"		/* save    EBP   */	\
		     "movl %%esp,%[prev_sp]\n\t"	/* save    ESP   */ \
		     "movl %[next_sp],%%esp\n\t"	/* restore ESP   */ \
		     "movl $1f,%[prev_ip]\n\t"	/* save    EIP   */	\
		     "pushl %[next_ip]\n\t"	/* restore EIP   */	\
		     __switch_canary					\
		     __retpoline_fill_return_buffer			\
		     "jmp __switch_to\n"	/* regparm call  */	\
		     "1:\t"						\
		     "popl %%ebp\n\t"		/* restore EBP   */	\
		     "popfl\n"			/* restore flags */	\
									\
		     /* output parameters */				\
		     : [prev_sp] "=m" (prev->thread.sp),		\
		       [prev_ip] "=m" (prev->thread.ip),		\
		       "=a" (last),					\
									\
		       /* clobbered output registers: */		\
		       "=b" (ebx), "=c" (ecx), "=d" (edx),		\
		       "=S" (esi), "=D" (edi)				\
		       							\
		       __switch_canary_oparam				\
									\
		       /* input parameters: */				\
		     : [next_sp]  "m" (next->thread.sp),		\
		       [next_ip]  "m" (next->thread.ip),		\
		       							\
		       /* regparm parameters for __switch_to(): */	\
		       [prev]     "a" (prev),				\
		       [next]     "d" (next)				\
									\
		       __switch_canary_iparam				\
									\
		     : /* reloaded segment registers */			\
			"memory");					\
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
} while (0)

#ifdef CONFIG_X86_32
static inline void refresh_sysenter_cs(struct thread_struct *thread)
{
	/* Only happens when SEP is enabled, no need to test "SEP"arately: */
	if (unlikely(this_cpu_read(cpu_tss_rw.x86_tss.ss1) == thread->sysenter_cs))
		return;

	this_cpu_write(cpu_tss_rw.x86_tss.ss1, thread->sysenter_cs);
	wrmsr(MSR_IA32_SYSENTER_CS, thread->sysenter_cs, 0);
}
#endif

/* This is used when switching tasks or entering/exiting vm86 mode. */
static inline void update_task_stack(struct task_struct *task)
{
	/* sp0 always points to the entry trampoline stack, which is constant: */
#ifdef CONFIG_X86_32
	if (static_cpu_has(X86_FEATURE_XENPV))
		load_sp0(task->thread.sp0);
	else
		this_cpu_write(cpu_tss_rw.x86_tss.sp1, task->thread.sp0);
#else
	/*
	 * x86-64 updates x86_tss.sp1 via cpu_current_top_of_stack. That
	 * doesn't work on x86-32 because sp1 and
	 * cpu_current_top_of_stack have different values (because of
	 * the non-zero stack-padding on 32bit).
	 */
	if (static_cpu_has(X86_FEATURE_XENPV))
		load_sp0(task_top_of_stack(task));
#endif

<<<<<<< HEAD
}
=======
#ifdef CONFIG_CC_STACKPROTECTOR
#define __switch_canary							  \
	"movq %P[task_canary](%%rsi),%%r8\n\t"				  \
	"movq %%r8,"__percpu_arg([gs_canary])"\n\t"
#define __switch_canary_oparam						  \
	, [gs_canary] "=m" (irq_stack_union.stack_canary)
#define __switch_canary_iparam						  \
	, [task_canary] "i" (offsetof(struct task_struct, stack_canary))
#else	/* CC_STACKPROTECTOR */
#define __switch_canary
#define __switch_canary_oparam
#define __switch_canary_iparam
#endif	/* CC_STACKPROTECTOR */

#ifdef CONFIG_RETPOLINE
	/*
	 * When switching from a shallower to a deeper call stack
	 * the RSB may either underflow or use entries populated
	 * with userspace addresses. On CPUs where those concerns
	 * exist, overwrite the RSB with entries which capture
	 * speculative execution to prevent attack.
	 */
#define __retpoline_fill_return_buffer					\
	ALTERNATIVE("jmp 910f",						\
		__stringify(__FILL_RETURN_BUFFER(%%r12, RSB_CLEAR_LOOPS, %%rsp)),\
		X86_FEATURE_RSB_CTXSW)					\
	"910:\n\t"
#else
#define __retpoline_fill_return_buffer
#endif

/*
 * There is no need to save or restore flags, because flags are always
 * clean in kernel mode, with the possible exception of IOPL.  Kernel IOPL
 * has no effect.
 */
#define switch_to(prev, next, last) \
	asm volatile(SAVE_CONTEXT					  \
	     "movq %%rsp,%P[threadrsp](%[prev])\n\t" /* save RSP */	  \
	     "movq %P[threadrsp](%[next]),%%rsp\n\t" /* restore RSP */	  \
	     "call __switch_to\n\t"					  \
	     "movq "__percpu_arg([current_task])",%%rsi\n\t"		  \
	     __switch_canary						  \
	     __retpoline_fill_return_buffer				  \
	     "movq %P[thread_info](%%rsi),%%r8\n\t"			  \
	     "movq %%rax,%%rdi\n\t" 					  \
	     "testl  %[_tif_fork],%P[ti_flags](%%r8)\n\t"		  \
	     "jnz   ret_from_fork\n\t"					  \
	     RESTORE_CONTEXT						  \
	     : "=a" (last)					  	  \
	       __switch_canary_oparam					  \
	     : [next] "S" (next), [prev] "D" (prev),			  \
	       [threadrsp] "i" (offsetof(struct task_struct, thread.sp)), \
	       [ti_flags] "i" (offsetof(struct thread_info, flags)),	  \
	       [_tif_fork] "i" (_TIF_FORK),			  	  \
	       [thread_info] "i" (offsetof(struct task_struct, stack)),   \
	       [current_task] "m" (current_task)			  \
	       __switch_canary_iparam					  \
	     : "memory", "cc" __EXTRA_CLOBBER)

#endif /* CONFIG_X86_32 */
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

#endif /* _ASM_X86_SWITCH_TO_H */
