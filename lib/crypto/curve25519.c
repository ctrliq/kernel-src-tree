// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * This is an implementation of the Curve25519 ECDH algorithm, using either
 * a 32-bit implementation or a 64-bit implementation with 128-bit integers,
 * depending on what is supported by the target compiler.
 *
 * Information: https://cr.yp.to/ecdh.html
 */

#include <crypto/curve25519.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fips.h>

static int __init curve25519_init(void)
{
#ifdef CONFIG_RHEL_DIFFERENCES
	if (fips_enabled) {
		printk(KERN_WARNING "curve25519 is not allowed in FIPS mode!\n");
		return -EOPNOTSUPP;
	}
#endif

	if (IS_ENABLED(CONFIG_CRYPTO_SELFTESTS) &&
	    WARN_ON(!curve25519_selftest()))
		return -ENODEV;
	return 0;
}

static void __exit curve25519_exit(void)
{
}

module_init(curve25519_init);
module_exit(curve25519_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Curve25519 scalar multiplication");
MODULE_AUTHOR("Jason A. Donenfeld <Jason@zx2c4.com>");
