/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __PKWM_TRUSTED_KEY_H
#define __PKWM_TRUSTED_KEY_H

#include <keys/trusted-type.h>
#include <linux/bitops.h>
#include <linux/printk.h>

extern struct trusted_key_ops pkwm_trusted_key_ops;

struct trusted_pkwm_options {
	u16 wrap_flags;
};

#endif
