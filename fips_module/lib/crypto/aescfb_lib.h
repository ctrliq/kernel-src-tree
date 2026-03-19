/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * FIPS module-internal AES-CFB library functions.
 *
 * These are provided by fips_module/lib/crypto/aescfb.c, a private copy of
 * lib/crypto/aescfb.c compiled inside the FIPS cryptographic boundary.
 */
#ifndef _FIPS_LIB_CRYPTO_AESCFB_LIB_H
#define _FIPS_LIB_CRYPTO_AESCFB_LIB_H

#include <crypto/aes.h>

void fips_lib_aescfb_encrypt(const struct crypto_aes_ctx *ctx, u8 *dst,
			     const u8 *src, int len,
			     const u8 iv[AES_BLOCK_SIZE]);

void fips_lib_aescfb_decrypt(const struct crypto_aes_ctx *ctx, u8 *dst,
			     const u8 *src, int len,
			     const u8 iv[AES_BLOCK_SIZE]);

#endif /* _FIPS_LIB_CRYPTO_AESCFB_LIB_H */
