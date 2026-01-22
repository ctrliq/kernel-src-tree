// SPDX-License-Identifier: GPL-2.0
/*
 * Code for replacing ftrace calls with jumps.
 *
 * Copyright (C) 2007-2008 Steven Rostedt <srostedt@redhat.com>
 *
 * Thanks goes out to P.A. Semi, Inc for supplying me with a PPC64 box.
 *
 * Added function graph tracer code, taken from x86 that was written
 * by Frederic Weisbecker, and ported to PPC by Steven Rostedt.
 *
 */

#define pr_fmt(fmt) "ftrace-powerpc: " fmt

#include <linux/spinlock.h>
#include <linux/hardirq.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/ftrace.h>
#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/list.h>

#include <asm/asm-prototypes.h>
#include <asm/cacheflush.h>
#include <asm/code-patching.h>
#include <asm/ftrace.h>
#include <asm/syscall.h>
#include <asm/inst.h>


#ifdef CONFIG_DYNAMIC_FTRACE

#define	NUM_FTRACE_TRAMPS	2
static unsigned long ftrace_tramps[NUM_FTRACE_TRAMPS];

static ppc_inst_t ftrace_create_branch_inst(unsigned long ip, unsigned long addr, int link)
{
	ppc_inst_t op;

	WARN_ON(!is_offset_in_branch_range(addr - ip));
	create_branch(&op, (u32 *)ip, addr, link ? BRANCH_SET_LINK : 0);

	return op;
}

static ppc_inst_t
ftrace_call_replace(unsigned long ip, unsigned long addr, int link)
{
	ppc_inst_t op;

	addr = ppc_function_entry((void *)addr);

	/* if (link) set op to 'bl' else 'b' */
	create_branch(&op, (u32 *)ip, addr, link ? 1 : 0);

	return op;
}

static inline int ftrace_read_inst(unsigned long ip, ppc_inst_t *op)
{
	if (copy_inst_from_kernel_nofault(op, (void *)ip)) {
		pr_err("0x%lx: fetching instruction failed\n", ip);
		return -EFAULT;
	}

	return 0;
}

static inline int ftrace_validate_inst(unsigned long ip, ppc_inst_t inst)
{
	ppc_inst_t op;
	int ret;

	ret = ftrace_read_inst(ip, &op);
	if (!ret && !ppc_inst_equal(op, inst)) {
		pr_err("0x%lx: expected (%08lx) != found (%08lx)\n",
		       ip, ppc_inst_as_ulong(inst), ppc_inst_as_ulong(op));
		ret = -EINVAL;
	}

	return ret;
}

static inline int ftrace_modify_code(unsigned long ip, ppc_inst_t old, ppc_inst_t new)
{
	int ret = ftrace_validate_inst(ip, old);

	if (!ret)
		ret = patch_instruction((u32 *)ip, new);

	return ret;
}

/*
 * Helper functions that are the same for both PPC64 and PPC32.
 */
static int test_24bit_addr(unsigned long ip, unsigned long addr)
{
	ppc_inst_t op;
	addr = ppc_function_entry((void *)addr);

	/* use the create_branch to verify that this offset can be branched */
	return create_branch(&op, (u32 *)ip, addr, 0) == 0;
}

static int is_bl_op(ppc_inst_t op)
{
	return (ppc_inst_val(op) & 0xfc000003) == 0x48000001;
}

static unsigned long find_bl_target(unsigned long ip, ppc_inst_t op)
{
	int offset;

	offset = (ppc_inst_val(op) & 0x03fffffc);
	/* make it signed */
	if (offset & 0x02000000)
		offset |= 0xfe000000;

	return ip + (long)offset;
}

static unsigned long find_ftrace_tramp(unsigned long ip)
{
	int i;
	ppc_inst_t instr;

	for (i = 0; i < NUM_FTRACE_TRAMPS; i++)
		if (!ftrace_tramps[i])
			continue;
		else if (create_branch(&instr, (void *)ip,
				       ftrace_tramps[i], 0) == 0)
			return ftrace_tramps[i];

	return 0;
}

#ifdef CONFIG_DYNAMIC_FTRACE_WITH_REGS
#ifdef CONFIG_MODULES
static int
__ftrace_modify_call(struct dyn_ftrace *rec, unsigned long old_addr,
					unsigned long addr)
{
	ppc_inst_t op;
	unsigned long ip = rec->ip;
	unsigned long entry, ptr, tramp;
	struct module *mod = rec->arch.mod;

	/* If we never set up ftrace trampolines, then bail */
	if (!mod->arch.tramp || !mod->arch.tramp_regs) {
		pr_err("No ftrace trampoline\n");
		return -EINVAL;
	}

	/* read where this goes */
	if (copy_inst_from_kernel_nofault(&op, (void *)ip)) {
		pr_err("Fetching opcode failed.\n");
		return -EFAULT;
	}

	/* Make sure that this is still a 24bit jump */
	if (!is_bl_op(op)) {
		pr_err("Not expected bl: opcode is %s\n", ppc_inst_as_str(op));
		return -EINVAL;
	}

	/* lets find where the pointer goes */
	tramp = find_bl_target(ip, op);
	entry = ppc_global_function_entry((void *)old_addr);

	pr_devel("ip:%lx jumps to %lx", ip, tramp);

	if (tramp != entry) {
		/* old_addr is not within range, so we must have used a trampoline */
		if (module_trampoline_target(mod, tramp, &ptr)) {
			pr_err("Failed to get trampoline target\n");
			return -EFAULT;
		}

		pr_devel("trampoline target %lx", ptr);

		/* This should match what was called */
		if (ptr != entry) {
			pr_err("addr %lx does not match expected %lx\n", ptr, entry);
			return -EINVAL;
		}
	}

	/* The new target may be within range */
	if (test_24bit_addr(ip, addr)) {
		/* within range */
		if (patch_branch((u32 *)ip, addr, BRANCH_SET_LINK)) {
			pr_err("REL24 out of range!\n");
			return -EINVAL;
		}

		return 0;
	}

	if (rec->flags & FTRACE_FL_REGS)
		tramp = mod->arch.tramp_regs;
	else
		tramp = mod->arch.tramp;

	if (module_trampoline_target(mod, tramp, &ptr)) {
		pr_err("Failed to get trampoline target\n");
		return -EFAULT;
	}

	pr_devel("trampoline target %lx", ptr);

	entry = ppc_global_function_entry((void *)addr);
	/* This should match what was called */
	if (ptr != entry) {
		pr_err("addr %lx does not match expected %lx\n", ptr, entry);
		return -EINVAL;
	}

	/* Ensure branch is within 24 bits */
	if (create_branch(&op, (u32 *)ip, tramp, BRANCH_SET_LINK)) {
		pr_err("Branch out of range\n");
		return -EINVAL;
	}

	if (patch_branch((u32 *)ip, tramp, BRANCH_SET_LINK)) {
		pr_err("REL24 out of range!\n");
		return -EINVAL;
	}

	return 0;
}
#endif

int ftrace_modify_call(struct dyn_ftrace *rec, unsigned long old_addr,
			unsigned long addr)
{
	unsigned long ip = rec->ip;
	ppc_inst_t old, new;

	/*
	 * If the calling address is more that 24 bits away,
	 * then we had to use a trampoline to make the call.
	 * Otherwise just update the call site.
	 */
	if (test_24bit_addr(ip, addr) && test_24bit_addr(ip, old_addr)) {
		/* within range */
		old = ftrace_call_replace(ip, old_addr, 1);
		new = ftrace_call_replace(ip, addr, 1);
		return ftrace_modify_code(ip, old, new);
	} else if (core_kernel_text(ip)) {
		/*
		 * We always patch out of range locations to go to the regs
		 * variant, so there is nothing to do here
		 */
		return 0;
	}

#ifdef CONFIG_MODULES
	/*
	 * Out of range jumps are called from modules.
	 */
	if (!rec->arch.mod) {
		pr_err("No module loaded\n");
		return -EINVAL;
	}

	return __ftrace_modify_call(rec, old_addr, addr);
#else
	/* We should not get here without modules */
	return -EINVAL;
#endif /* CONFIG_MODULES */
}
#endif

int ftrace_make_call(struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned long tramp, ip = rec->ip;
	ppc_inst_t old, new;
	struct module *mod;

	old = ppc_inst(PPC_RAW_NOP());
	if (is_offset_in_branch_range(addr - ip)) {
		/* Within range */
		new = ftrace_create_branch_inst(ip, addr, 1);
		return ftrace_modify_code(ip, old, new);
	} else if (core_kernel_text(ip)) {
		/* We would be branching to one of our ftrace tramps */
		tramp = find_ftrace_tramp(ip);
		if (!tramp) {
			pr_err("0x%lx: No ftrace trampolines reachable\n", ip);
			return -EINVAL;
		}
		new = ftrace_create_branch_inst(ip, tramp, 1);
		return ftrace_modify_code(ip, old, new);
	} else if (IS_ENABLED(CONFIG_MODULES)) {
		/* Module code would be going to one of the module stubs */
		mod = rec->arch.mod;
		tramp = (addr == (unsigned long)ftrace_caller ? mod->arch.tramp : mod->arch.tramp_regs);
		new = ftrace_create_branch_inst(ip, tramp, 1);
		return ftrace_modify_code(ip, old, new);
	}

	return -EINVAL;
}

int ftrace_make_nop(struct module *mod, struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned long tramp, ip = rec->ip;
	ppc_inst_t old, new;

	/* Nop-out the ftrace location */
	new = ppc_inst(PPC_RAW_NOP());
	if (is_offset_in_branch_range(addr - ip)) {
		/* Within range */
		old = ftrace_create_branch_inst(ip, addr, 1);
		return ftrace_modify_code(ip, old, new);
	} else if (core_kernel_text(ip)) {
		/* We would be branching to one of our ftrace tramps */
		tramp = find_ftrace_tramp(ip);
		if (!tramp) {
			pr_err("0x%lx: No ftrace trampolines reachable\n", ip);
			return -EINVAL;
		}
		old = ftrace_create_branch_inst(ip, tramp, 1);
		return ftrace_modify_code(ip, old, new);
	} else if (IS_ENABLED(CONFIG_MODULES)) {
		/* Module code would be going to one of the module stubs */
		if (!mod)
			mod = rec->arch.mod;
		tramp = (addr == (unsigned long)ftrace_caller ? mod->arch.tramp : mod->arch.tramp_regs);
		old = ftrace_create_branch_inst(ip, tramp, 1);
		return ftrace_modify_code(ip, old, new);
	}

	return -EINVAL;
}

int ftrace_init_nop(struct module *mod, struct dyn_ftrace *rec)
{
	unsigned long addr, ip = rec->ip;
	ppc_inst_t old, new;
	int ret = 0;

	/* Verify instructions surrounding the ftrace location */
	if (IS_ENABLED(CONFIG_PPC32)) {
		/* Expected sequence: 'mflr r0', 'stw r0,4(r1)', 'bl _mcount' */
		ret = ftrace_validate_inst(ip - 8, ppc_inst(PPC_RAW_MFLR(_R0)));
		if (!ret)
			ret = ftrace_validate_inst(ip - 4, ppc_inst(PPC_RAW_STW(_R0, _R1, 4)));
	} else if (IS_ENABLED(CONFIG_MPROFILE_KERNEL)) {
		/* Expected sequence: 'mflr r0', ['std r0,16(r1)'], 'bl _mcount' */
		ret = ftrace_read_inst(ip - 4, &old);
		if (!ret && !ppc_inst_equal(old, ppc_inst(PPC_RAW_MFLR(_R0)))) {
			ret = ftrace_validate_inst(ip - 8, ppc_inst(PPC_RAW_MFLR(_R0)));
			ret |= ftrace_validate_inst(ip - 4, ppc_inst(PPC_RAW_STD(_R0, _R1, 16)));
		}
	} else {
		return -EINVAL;
	}

	if (ret)
		return ret;

	if (!core_kernel_text(ip)) {
		if (!mod) {
			pr_err("0x%lx: No module provided for non-kernel address\n", ip);
			return -EFAULT;
		}
		rec->arch.mod = mod;
	}

	/* Nop-out the ftrace location */
	new = ppc_inst(PPC_RAW_NOP());
	addr = MCOUNT_ADDR;
	if (is_offset_in_branch_range(addr - ip)) {
		/* Within range */
		old = ftrace_create_branch_inst(ip, addr, 1);
		ret = ftrace_modify_code(ip, old, new);
	} else if (core_kernel_text(ip) || (IS_ENABLED(CONFIG_MODULES) && mod)) {
		/*
		 * We would be branching to a linker-generated stub, or to the module _mcount
		 * stub. Let's just confirm we have a 'bl' here.
		 */
		ret = ftrace_read_inst(ip, &old);
		if (ret)
			return ret;
		if (!is_bl_op(old)) {
			pr_err("0x%lx: expected (bl) != found (%08lx)\n", ip, ppc_inst_as_ulong(old));
			return -EINVAL;
		}
		ret = patch_instruction((u32 *)ip, new);
	} else {
		return -EINVAL;
	}

	return ret;
}

int ftrace_update_ftrace_func(ftrace_func_t func)
{
	unsigned long ip = (unsigned long)(&ftrace_call);
	ppc_inst_t old, new;
	int ret;

	old = ppc_inst_read((u32 *)&ftrace_call);
	new = ftrace_call_replace(ip, (unsigned long)func, 1);
	ret = ftrace_modify_code(ip, old, new);

#ifdef CONFIG_DYNAMIC_FTRACE_WITH_REGS
	/* Also update the regs callback function */
	if (!ret) {
		ip = (unsigned long)(&ftrace_regs_call);
		old = ppc_inst_read((u32 *)&ftrace_regs_call);
		new = ftrace_call_replace(ip, (unsigned long)func, 1);
		ret = ftrace_modify_code(ip, old, new);
	}
#endif

	return ret;
}

/*
 * Use the default ftrace_modify_all_code, but without
 * stop_machine().
 */
void arch_ftrace_update_code(int command)
{
	ftrace_modify_all_code(command);
}

#ifdef CONFIG_PPC64
#define PACATOC offsetof(struct paca_struct, kernel_toc)

extern unsigned int ftrace_tramp_text[], ftrace_tramp_init[];

void ftrace_free_init_tramp(void)
{
	int i;

	for (i = 0; i < NUM_FTRACE_TRAMPS && ftrace_tramps[i]; i++)
		if (ftrace_tramps[i] == (unsigned long)ftrace_tramp_init) {
			ftrace_tramps[i] = 0;
			return;
		}
}

static void __init add_ftrace_tramp(unsigned long tramp)
{
	int i;

	for (i = 0; i < NUM_FTRACE_TRAMPS; i++)
		if (!ftrace_tramps[i]) {
			ftrace_tramps[i] = tramp;
			return;
		}
}

int __init ftrace_dyn_arch_init(void)
{
	int i;
	unsigned int *tramp[] = { ftrace_tramp_text, ftrace_tramp_init };
	u32 stub_insns[] = {
		0xe98d0000 | PACATOC,	/* ld      r12,PACATOC(r13)	*/
		0x3d8c0000,		/* addis   r12,r12,<high>	*/
		0x398c0000,		/* addi    r12,r12,<low>	*/
		0x7d8903a6,		/* mtctr   r12			*/
		0x4e800420,		/* bctr				*/
	};
#ifdef CONFIG_DYNAMIC_FTRACE_WITH_REGS
	unsigned long addr = ppc_global_function_entry((void *)ftrace_regs_caller);
#else
	unsigned long addr = ppc_global_function_entry((void *)ftrace_caller);
#endif
	long reladdr = addr - kernel_toc_addr();

	if (reladdr > 0x7FFFFFFF || reladdr < -(0x80000000L)) {
		pr_err("Address of %ps out of range of kernel_toc.\n",
				(void *)addr);
		return -1;
	}

	for (i = 0; i < 2; i++) {
		memcpy(tramp[i], stub_insns, sizeof(stub_insns));
		tramp[i][1] |= PPC_HA(reladdr);
		tramp[i][2] |= PPC_LO(reladdr);
		add_ftrace_tramp((unsigned long)tramp[i]);
	}

	return 0;
}
#else
int __init ftrace_dyn_arch_init(void)
{
	return 0;
}
#endif
#endif /* CONFIG_DYNAMIC_FTRACE */

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
void ftrace_graph_func(unsigned long ip, unsigned long parent_ip,
		       struct ftrace_ops *op, struct ftrace_regs *fregs)
{
	unsigned long sp = fregs->regs.gpr[1];
	int bit;

	if (unlikely(ftrace_graph_is_dead()))
		goto out;

	if (unlikely(atomic_read(&current->tracing_graph_pause)))
		goto out;

	bit = ftrace_test_recursion_trylock(ip, parent_ip);
	if (bit < 0)
		goto out;

	if (!function_graph_enter(parent_ip, ip, 0, (unsigned long *)sp))
		parent_ip = ppc_function_entry(return_to_handler);

	ftrace_test_recursion_unlock(bit);
out:
	fregs->regs.link = parent_ip;
}
#endif /* CONFIG_FUNCTION_GRAPH_TRACER */

#ifdef PPC64_ELF_ABI_v1
char *arch_ftrace_match_adjust(char *str, const char *search)
{
	if (str[0] == '.' && search[0] != '.')
		return str + 1;
	else
		return str;
}
#endif /* PPC64_ELF_ABI_v1 */
