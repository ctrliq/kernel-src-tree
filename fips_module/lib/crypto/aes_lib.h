/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * FIPS module-internal AES library functions.
 *
 * These are provided by fips_module/lib/crypto/aes.c, a private copy of
 * lib/crypto/aes.c compiled inside the FIPS cryptographic boundary.
 */
#ifndef _FIPS_LIB_CRYPTO_AES_LIB_H
#define _FIPS_LIB_CRYPTO_AES_LIB_H

#include <crypto/aes.h>

int fips_lib_aes_expandkey(struct crypto_aes_ctx *ctx, const u8 *in_key,
			   unsigned int key_len);

void fips_lib_aes_encrypt(const struct crypto_aes_ctx *ctx, u8 *out,
			  const u8 *in);

void fips_lib_aes_decrypt(const struct crypto_aes_ctx *ctx, u8 *out,
			  const u8 *in);

int fips_aes_bootstrap_selftest(void);

#endif /* _FIPS_LIB_CRYPTO_AES_LIB_H */
