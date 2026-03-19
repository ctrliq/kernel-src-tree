/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * FIPS module-internal AES-GCM library functions.
 *
 * These are provided by fips_module/lib/crypto/aesgcm.c, a private copy of
 * lib/crypto/aesgcm.c compiled inside the FIPS cryptographic boundary.
 */
#ifndef _FIPS_LIB_CRYPTO_AESGCM_LIB_H
#define _FIPS_LIB_CRYPTO_AESGCM_LIB_H

#include <crypto/gcm.h>

int fips_lib_aesgcm_expandkey(struct aesgcm_ctx *ctx, const u8 *key,
			      unsigned int keysize, unsigned int authsize);

void fips_lib_aesgcm_encrypt(const struct aesgcm_ctx *ctx, u8 *dst,
			     const u8 *src, int crypt_len, const u8 *assoc,
			     int assoc_len, const u8 iv[GCM_AES_IV_SIZE],
			     u8 *authtag);

bool __must_check fips_lib_aesgcm_decrypt(const struct aesgcm_ctx *ctx,
					  u8 *dst, const u8 *src, int crypt_len,
					  const u8 *assoc, int assoc_len,
					  const u8 iv[GCM_AES_IV_SIZE],
					  const u8 *authtag);

#endif /* _FIPS_LIB_CRYPTO_AESGCM_LIB_H */
