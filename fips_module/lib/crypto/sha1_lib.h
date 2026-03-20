/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * FIPS module-internal SHA-1 and HMAC-SHA1 library functions.
 *
 * These are provided by fips_module/lib/crypto/sha1.c, a private copy of
 * lib/crypto/sha1.c compiled inside the FIPS cryptographic boundary.
 */
#ifndef _FIPS_LIB_CRYPTO_SHA1_LIB_H
#define _FIPS_LIB_CRYPTO_SHA1_LIB_H

#include <crypto/sha1.h>

void fips_lib_sha1_transform(__u32 *digest, const char *data, __u32 *W);
void fips_lib_sha1_init_raw(__u32 *buf);

void fips_lib_sha1_init(struct sha1_ctx *ctx);
void fips_lib_sha1_update(struct sha1_ctx *ctx, const u8 *data, size_t len);
void fips_lib_sha1_final(struct sha1_ctx *ctx, u8 out[SHA1_DIGEST_SIZE]);
void fips_lib_sha1(const u8 *data, size_t len, u8 out[SHA1_DIGEST_SIZE]);

void fips_lib_hmac_sha1_preparekey(struct hmac_sha1_key *key,
				   const u8 *raw_key, size_t raw_key_len);
void fips_lib_hmac_sha1_init(struct hmac_sha1_ctx *ctx,
			     const struct hmac_sha1_key *key);
void fips_lib_hmac_sha1_init_usingrawkey(struct hmac_sha1_ctx *ctx,
					 const u8 *raw_key, size_t raw_key_len);

static inline void fips_lib_hmac_sha1_update(struct hmac_sha1_ctx *ctx,
					     const u8 *data, size_t data_len)
{
	fips_lib_sha1_update(&ctx->sha_ctx, data, data_len);
}

void fips_lib_hmac_sha1_final(struct hmac_sha1_ctx *ctx,
			      u8 out[SHA1_DIGEST_SIZE]);
void fips_lib_hmac_sha1(const struct hmac_sha1_key *key,
			const u8 *data, size_t data_len,
			u8 out[SHA1_DIGEST_SIZE]);
void fips_lib_hmac_sha1_usingrawkey(const u8 *raw_key, size_t raw_key_len,
				    const u8 *data, size_t data_len,
				    u8 out[SHA1_DIGEST_SIZE]);

/* Initialise arch-specific SHA-1 acceleration (call once at module init). */
void fips_lib_sha1_arch_init(void);

#endif /* _FIPS_LIB_CRYPTO_SHA1_LIB_H */
