/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_MPSPEC_H
#define _ASM_X86_MPSPEC_H

#include <linux/types.h>

#include <asm/mpspec_def.h>
#include <asm/x86_init.h>
#include <asm/apicdef.h>

extern int pic_mode;

#ifdef CONFIG_X86_32

/*
 * Summit or generic (i.e. installer) kernels need lots of bus entries.
 * Maximum 256 PCI busses, plus 1 ISA bus in each of 4 cabinets.
 */
#if CONFIG_BASE_SMALL == 0
# define MAX_MP_BUSSES		260
#else
# define MAX_MP_BUSSES		32
#endif

#define MAX_IRQ_SOURCES		256

#else /* CONFIG_X86_64: */

#define MAX_MP_BUSSES		256
/* Each PCI slot may be a combo card with its own bus.  4 IRQ pins per slot. */
#define MAX_IRQ_SOURCES		(MAX_MP_BUSSES * 4)

#endif /* CONFIG_X86_64 */

#ifdef CONFIG_EISA
extern int mp_bus_id_to_type[MAX_MP_BUSSES];
#endif

extern DECLARE_BITMAP(mp_bus_not_pci, MAX_MP_BUSSES);

extern u32 boot_cpu_physical_apicid;
extern u8 boot_cpu_apic_version;

#ifdef CONFIG_X86_LOCAL_APIC
extern int smp_found_config;
#else
# define smp_found_config 0
#endif

static inline void get_smp_config(void)
{
	x86_init.mpparse.get_smp_config(0);
}

static inline void early_get_smp_config(void)
{
	x86_init.mpparse.get_smp_config(1);
}

static inline void find_smp_config(void)
{
	x86_init.mpparse.find_smp_config();
}

#ifdef CONFIG_X86_MPPARSE
extern void e820__memblock_alloc_reserved_mpc_new(void);
extern int enable_update_mptable;
extern void default_find_smp_config(void);
extern void default_get_smp_config(unsigned int early);
#else
static inline void e820__memblock_alloc_reserved_mpc_new(void) { }
#define enable_update_mptable 0
#define default_find_smp_config x86_init_noop
#define default_get_smp_config x86_init_uint_noop
#endif

int generic_processor_info(int apicid);

extern DECLARE_BITMAP(phys_cpu_present_map, MAX_LOCAL_APIC);

static inline void reset_phys_cpu_present_map(u32 apicid)
{
	bitmap_zero(phys_cpu_present_map, MAX_LOCAL_APIC);
	set_bit(apicid, phys_cpu_present_map);
}

static inline void copy_phys_cpu_present_map(unsigned long *dst)
{
	bitmap_copy(dst, phys_cpu_present_map, MAX_LOCAL_APIC);
}

#endif /* _ASM_X86_MPSPEC_H */
