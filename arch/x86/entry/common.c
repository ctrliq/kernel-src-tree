// SPDX-License-Identifier: GPL-2.0-only
/*
 * common.c - C code for kernel entry and exit
 * Copyright (c) 2015 Andrew Lutomirski
 *
 * Based on asm and ptrace code by many authors.  The code here originated
 * in ptrace.c and signal.c.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/entry-common.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/export.h>
#include <linux/nospec.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/init.h>

#ifdef CONFIG_XEN_PV
#include <xen/xen-ops.h>
#include <xen/events.h>
#endif

#include <asm/apic.h>
#include <asm/desc.h>
#include <asm/traps.h>
#include <asm/vdso.h>
#include <asm/cpufeature.h>
#include <asm/fpu/api.h>
#include <asm/nospec-branch.h>
#include <asm/io_bitmap.h>
#include <asm/syscall.h>
#include <asm/irq_stack.h>

SYSCALL_DEFINE0(ni_syscall)
{
	return -ENOSYS;
}

#ifdef CONFIG_XEN_PV
#ifndef CONFIG_PREEMPTION
/*
 * Some hypercalls issued by the toolstack can take many 10s of
 * seconds. Allow tasks running hypercalls via the privcmd driver to
 * be voluntarily preempted even if full kernel preemption is
 * disabled.
 *
 * Such preemptible hypercalls are bracketed by
 * xen_preemptible_hcall_begin() and xen_preemptible_hcall_end()
 * calls.
 */
DEFINE_PER_CPU(bool, xen_in_preemptible_hcall);
EXPORT_SYMBOL_GPL(xen_in_preemptible_hcall);

/*
 * In case of scheduling the flag must be cleared and restored after
 * returning from schedule as the task might move to a different CPU.
 */
static __always_inline bool get_and_clear_inhcall(void)
{
	bool inhcall = __this_cpu_read(xen_in_preemptible_hcall);

	__this_cpu_write(xen_in_preemptible_hcall, false);
	return inhcall;
}

static __always_inline void restore_inhcall(bool inhcall)
{
	__this_cpu_write(xen_in_preemptible_hcall, inhcall);
}
#else
static __always_inline bool get_and_clear_inhcall(void) { return false; }
static __always_inline void restore_inhcall(bool inhcall) { }
#endif

static void __xen_pv_evtchn_do_upcall(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	inc_irq_stat(irq_hv_callback_count);

	xen_evtchn_do_upcall();

	set_irq_regs(old_regs);
}

__visible noinstr void xen_pv_evtchn_do_upcall(struct pt_regs *regs)
{
	irqentry_state_t state = irqentry_enter(regs);
	bool inhcall;

	instrumentation_begin();
	run_sysvec_on_irqstack_cond(__xen_pv_evtchn_do_upcall, regs);

	inhcall = get_and_clear_inhcall();
	if (inhcall && !WARN_ON_ONCE(state.exit_rcu)) {
		irqentry_exit_cond_resched();
		instrumentation_end();
		restore_inhcall(inhcall);
	} else {
		instrumentation_end();
		irqentry_exit(regs, state);
	}
}
#endif /* CONFIG_XEN_PV */
