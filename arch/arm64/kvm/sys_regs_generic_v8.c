/*
 * Copyright (C) 2012,2013 - ARM Ltd
 * Author: Marc Zyngier <marc.zyngier@arm.com>
 *
 * Based on arch/arm/kvm/coproc_a15.c:
 * Copyright (C) 2012 - Virtual Open Systems and Columbia University
 * Authors: Rusty Russell <rusty@rustcorp.au>
 *          Christoffer Dall <c.dall@virtualopensystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/kvm_host.h>
#include <asm/cputype.h>
#include <asm/kvm_arm.h>
#include <asm/kvm_asm.h>
#include <asm/kvm_emulate.h>
#include <asm/kvm_coproc.h>
#include <asm/sysreg.h>
#include <linux/init.h>

#include "sys_regs.h"

/*
 * Implementation specific sys-reg registers.
 * Important: Must be sorted ascending by Op0, Op1, CRn, CRm, Op2
 */
static const struct sys_reg_desc genericv8_sys_regs[] = {
};

static const struct sys_reg_desc genericv8_cp15_regs[] = {
};

struct kvm_sys_reg_target_table genericv8_target_table = {
	.table64 = {
		.table = genericv8_sys_regs,
		.num = ARRAY_SIZE(genericv8_sys_regs),
	},
	.table32 = {
		.table = genericv8_cp15_regs,
		.num = ARRAY_SIZE(genericv8_cp15_regs),
	},
};

static int __init sys_reg_genericv8_init(void)
{
	unsigned int i;

	for (i = 1; i < ARRAY_SIZE(genericv8_sys_regs); i++)
		BUG_ON(cmp_sys_reg(&genericv8_sys_regs[i-1],
			       &genericv8_sys_regs[i]) >= 0);

	kvm_check_target_sys_reg_table(&genericv8_target_table);

	return 0;
}
late_initcall(sys_reg_genericv8_init);
