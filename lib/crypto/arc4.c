// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Cryptographic API
 *
 * ARC4 Cipher Algorithm
 *
 * Jon Oberheide <jon@oberheide.org>
 */

#include <crypto/arc4.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/fips.h>

int arc4_setkey(struct arc4_ctx *ctx, const u8 *in_key, unsigned int key_len)
{
	int i, j = 0, k = 0;

#ifdef CONFIG_RHEL_DIFFERENCES
	if (fips_enabled) {
		WARN_ONCE(1, "ARC4 is not allowed in FIPS mode\n");
		return -EOPNOTSUPP;
	}
#endif

	ctx->x = 1;
	ctx->y = 0;

	for (i = 0; i < 256; i++)
		ctx->S[i] = i;

	for (i = 0; i < 256; i++) {
		u32 a = ctx->S[i];

		j = (j + in_key[k] + a) & 0xff;
		ctx->S[i] = ctx->S[j];
		ctx->S[j] = a;
		if (++k >= key_len)
			k = 0;
	}

	return 0;
}
EXPORT_SYMBOL(arc4_setkey);

void arc4_crypt(struct arc4_ctx *ctx, u8 *out, const u8 *in, unsigned int len)
{
	u32 *const S = ctx->S;
	u32 x, y, a, b;
	u32 ty, ta, tb;

	if (len == 0)
		return;

#ifdef CONFIG_RHEL_DIFFERENCES
	BUG_ON(fips_enabled);
#endif

	x = ctx->x;
	y = ctx->y;

	a = S[x];
	y = (y + a) & 0xff;
	b = S[y];

	do {
		S[y] = a;
		a = (a + b) & 0xff;
		S[x] = b;
		x = (x + 1) & 0xff;
		ta = S[x];
		ty = (y + ta) & 0xff;
		tb = S[ty];
		*out++ = *in++ ^ S[a];
		if (--len == 0)
			break;
		y = ty;
		a = ta;
		b = tb;
	} while (true);

	ctx->x = x;
	ctx->y = y;
}
EXPORT_SYMBOL(arc4_crypt);

MODULE_DESCRIPTION("ARC4 Cipher Algorithm");
MODULE_LICENSE("GPL");

#ifdef CONFIG_RHEL_DIFFERENCES
MODULE_ALIAS("cifs_arc4");
#endif
