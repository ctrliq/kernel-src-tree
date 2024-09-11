/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 1994 Linus Torvalds
 *
 * Pentium III FXSR, SSE support
 * General FPU state handling cleanups
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 * x86-64 work by Andi Kleen 2002
 */

#ifndef _ASM_X86_FPU_INTERNAL_H
#define _ASM_X86_FPU_INTERNAL_H

#include <linux/compat.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include <asm/user.h>
#include <asm/fpu/api.h>
#include <asm/fpu/xstate.h>
#include <asm/fpu/xcr.h>
#include <asm/cpufeature.h>
#include <asm/trace/fpu.h>

/*
 * High level FPU state handling functions:
 */
extern bool fpu__restore_sig(void __user *buf, int ia32_frame);
extern void fpu__clear_user_states(struct fpu *fpu);
extern int  fpu__exception_code(struct fpu *fpu, int trap_nr);

extern void fpu_sync_fpstate(struct fpu *fpu);

/*
 * Boot time FPU initialization functions:
 */
extern void fpu__init_cpu(void);
extern void fpu__init_system_xstate(void);
extern void fpu__init_cpu_xstate(void);
extern void fpu__init_system(struct cpuinfo_x86 *c);
extern void fpu__init_check_bugs(void);
extern void fpu__resume_cpu(void);

extern union fpregs_state init_fpstate;
extern void fpstate_init_user(union fpregs_state *state);

#ifdef CONFIG_MATH_EMULATION
extern void fpstate_init_soft(struct swregs_state *soft);
#else
static inline void fpstate_init_soft(struct swregs_state *soft) {}
#endif

/* Returns 0 or the negated trap number, which results in -EFAULT for #PF */
#define user_insn(insn, output, input...)				\
({									\
	int err;							\
	asm volatile(ASM_STAC "\n"					\
		     "1: " #insn "\n"					\
		     "2: " ASM_CLAC "\n"				\
		     ".section .fixup,\"ax\"\n"				\
		     "3:  negl %%eax\n"					\
		     "    jmp  2b\n"					\
		     ".previous\n"					\
		     _ASM_EXTABLE_FAULT(1b, 3b)				\
		     : [err] "=a" (err), output				\
		     : "0"(0), input);					\
	err;								\
})

#define kernel_insn_err(insn, output, input...)				\
({									\
	int err;							\
	asm volatile("1:" #insn "\n\t"					\
		     "2:\n"						\
		     ".section .fixup,\"ax\"\n"				\
		     "3:  movl $-1,%[err]\n"				\
		     "    jmp  2b\n"					\
		     ".previous\n"					\
		     _ASM_EXTABLE(1b, 3b)				\
		     : [err] "=r" (err), output				\
		     : "0"(0), input);					\
	err;								\
})

#define kernel_insn(insn, output, input...)				\
	asm volatile("1:" #insn "\n\t"					\
		     "2:\n"						\
		     _ASM_EXTABLE_HANDLE(1b, 2b, ex_handler_fprestore)	\
		     : output : input)

static inline int fnsave_to_user_sigframe(struct fregs_state __user *fx)
{
	return user_insn(fnsave %[fx]; fwait,  [fx] "=m" (*fx), "m" (*fx));
}

static inline int fxsave_to_user_sigframe(struct fxregs_state __user *fx)
{
	if (IS_ENABLED(CONFIG_X86_32))
		return user_insn(fxsave %[fx], [fx] "=m" (*fx), "m" (*fx));
	else
		return user_insn(fxsaveq %[fx], [fx] "=m" (*fx), "m" (*fx));
}

static inline void fxrstor(struct fxregs_state *fx)
{
	if (IS_ENABLED(CONFIG_X86_32))
		kernel_insn(fxrstor %[fx], "=m" (*fx), [fx] "m" (*fx));
	else
		kernel_insn(fxrstorq %[fx], "=m" (*fx), [fx] "m" (*fx));
}

static inline int fxrstor_safe(struct fxregs_state *fx)
{
	if (IS_ENABLED(CONFIG_X86_32))
		return kernel_insn_err(fxrstor %[fx], "=m" (*fx), [fx] "m" (*fx));
	else
		return kernel_insn_err(fxrstorq %[fx], "=m" (*fx), [fx] "m" (*fx));
}

static inline int fxrstor_from_user_sigframe(struct fxregs_state __user *fx)
{
	if (IS_ENABLED(CONFIG_X86_32))
		return user_insn(fxrstor %[fx], "=m" (*fx), [fx] "m" (*fx));
	else
		return user_insn(fxrstorq %[fx], "=m" (*fx), [fx] "m" (*fx));
}

static inline void frstor(struct fregs_state *fx)
{
	kernel_insn(frstor %[fx], "=m" (*fx), [fx] "m" (*fx));
}

static inline int frstor_safe(struct fregs_state *fx)
{
	return kernel_insn_err(frstor %[fx], "=m" (*fx), [fx] "m" (*fx));
}

static inline int frstor_from_user_sigframe(struct fregs_state __user *fx)
{
	return user_insn(frstor %[fx], "=m" (*fx), [fx] "m" (*fx));
}

static inline void fxsave(struct fxregs_state *fx)
{
	if (IS_ENABLED(CONFIG_X86_32))
		asm volatile( "fxsave %[fx]" : [fx] "=m" (*fx));
	else
		asm volatile("fxsaveq %[fx]" : [fx] "=m" (*fx));
}

/* These macros all use (%edi)/(%rdi) as the single memory argument. */
#define XSAVE		".byte " REX_PREFIX "0x0f,0xae,0x27"
#define XSAVEOPT	".byte " REX_PREFIX "0x0f,0xae,0x37"
#define XSAVES		".byte " REX_PREFIX "0x0f,0xc7,0x2f"
#define XRSTOR		".byte " REX_PREFIX "0x0f,0xae,0x2f"
#define XRSTORS		".byte " REX_PREFIX "0x0f,0xc7,0x1f"

/*
 * After this @err contains 0 on success or the negated trap number when
 * the operation raises an exception. For faults this results in -EFAULT.
 */
#define XSTATE_OP(op, st, lmask, hmask, err)				\
	asm volatile("1:" op "\n\t"					\
		     "xor %[err], %[err]\n"				\
		     "2:\n\t"						\
		     ".pushsection .fixup,\"ax\"\n\t"			\
		     "3: negl %%eax\n\t"				\
		     "jmp 2b\n\t"					\
		     ".popsection\n\t"					\
		     _ASM_EXTABLE_FAULT(1b, 3b)				\
		     : [err] "=a" (err)					\
		     : "D" (st), "m" (*st), "a" (lmask), "d" (hmask)	\
		     : "memory")

/*
 * If XSAVES is enabled, it replaces XSAVEOPT because it supports a compact
 * format and supervisor states in addition to modified optimization in
 * XSAVEOPT.
 *
 * Otherwise, if XSAVEOPT is enabled, XSAVEOPT replaces XSAVE because XSAVEOPT
 * supports modified optimization which is not supported by XSAVE.
 *
 * We use XSAVE as a fallback.
 *
 * The 661 label is defined in the ALTERNATIVE* macros as the address of the
 * original instruction which gets replaced. We need to use it here as the
 * address of the instruction where we might get an exception at.
 */
#define XSTATE_XSAVE(st, lmask, hmask, err)				\
	asm volatile(ALTERNATIVE_2(XSAVE,				\
				   XSAVEOPT, X86_FEATURE_XSAVEOPT,	\
				   XSAVES,   X86_FEATURE_XSAVES)	\
		     "\n"						\
		     "xor %[err], %[err]\n"				\
		     "3:\n"						\
		     ".pushsection .fixup,\"ax\"\n"			\
		     "4: movl $-2, %[err]\n"				\
		     "jmp 3b\n"						\
		     ".popsection\n"					\
		     _ASM_EXTABLE(661b, 4b)				\
		     : [err] "=r" (err)					\
		     : "D" (st), "m" (*st), "a" (lmask), "d" (hmask)	\
		     : "memory")

/*
 * Use XRSTORS to restore context if it is enabled. XRSTORS supports compact
 * XSAVE area format.
 */
#define XSTATE_XRESTORE(st, lmask, hmask)				\
	asm volatile(ALTERNATIVE(XRSTOR,				\
				 XRSTORS, X86_FEATURE_XSAVES)		\
		     "\n"						\
		     "3:\n"						\
		     _ASM_EXTABLE_HANDLE(661b, 3b, ex_handler_fprestore)\
		     :							\
		     : "D" (st), "m" (*st), "a" (lmask), "d" (hmask)	\
		     : "memory")

/*
 * Save processor xstate to xsave area.
 *
 * Uses either XSAVE or XSAVEOPT or XSAVES depending on the CPU features
 * and command line options. The choice is permanent until the next reboot.
 */
static inline void os_xsave(struct xregs_state *xstate)
{
	u64 mask = xfeatures_mask_all;
	u32 lmask = mask;
	u32 hmask = mask >> 32;
	int err;

	WARN_ON_FPU(!alternatives_patched);

	XSTATE_XSAVE(xstate, lmask, hmask, err);

	/* We should never fault when copying to a kernel buffer: */
	WARN_ON_FPU(err);
}

/*
 * Restore processor xstate from xsave area.
 *
 * Uses XRSTORS when XSAVES is used, XRSTOR otherwise.
 */
static inline void os_xrstor(struct xregs_state *xstate, u64 mask)
{
	u32 lmask = mask;
	u32 hmask = mask >> 32;

	XSTATE_XRESTORE(xstate, lmask, hmask);
}

/*
 * Save xstate to user space xsave area.
 *
 * We don't use modified optimization because xrstor/xrstors might track
 * a different application.
 *
 * We don't use compacted format xsave area for backward compatibility for
 * old applications which don't understand the compacted format of the
 * xsave area.
 *
 * The caller has to zero buf::header before calling this because XSAVE*
 * does not touch the reserved fields in the header.
 */
static inline int xsave_to_user_sigframe(struct xregs_state __user *buf)
{
	/*
	 * Include the features which are not xsaved/rstored by the kernel
	 * internally, e.g. PKRU. That's user space ABI and also required
	 * to allow the signal handler to modify PKRU.
	 */
	u64 mask = xfeatures_mask_uabi();
	u32 lmask = mask;
	u32 hmask = mask >> 32;
	int err;

	stac();
	XSTATE_OP(XSAVE, buf, lmask, hmask, err);
	clac();

	return err;
}

/*
 * Restore xstate from user space xsave area.
 */
static inline int xrstor_from_user_sigframe(struct xregs_state __user *buf, u64 mask)
{
	struct xregs_state *xstate = ((__force struct xregs_state *)buf);
	u32 lmask = mask;
	u32 hmask = mask >> 32;
	int err;

	stac();
	XSTATE_OP(XRSTOR, xstate, lmask, hmask, err);
	clac();

	return err;
}

/*
 * Restore xstate from kernel space xsave area, return an error code instead of
 * an exception.
 */
static inline int os_xrstor_safe(struct xregs_state *xstate, u64 mask)
{
	u32 lmask = mask;
	u32 hmask = mask >> 32;
	int err;

	if (cpu_feature_enabled(X86_FEATURE_XSAVES))
		XSTATE_OP(XRSTORS, xstate, lmask, hmask, err);
	else
		XSTATE_OP(XRSTOR, xstate, lmask, hmask, err);

	return err;
}

extern void restore_fpregs_from_fpstate(union fpregs_state *fpstate, u64 mask);

extern int copy_fpstate_to_sigframe(void __user *buf, void __user *fp, int size);

DECLARE_PER_CPU(struct fpu *, fpu_fpregs_owner_ctx);

#endif /* _ASM_X86_FPU_INTERNAL_H */
