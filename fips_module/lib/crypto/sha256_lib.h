/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * FIPS module-internal SHA-224/256 and HMAC-SHA-224/256 library functions.
 *
 * These are renamed copies of lib/crypto/sha256.c functions, prefixed with
 * fips_lib_ so that intra-module references unambiguously resolve to the
 * in-boundary implementation rather than the kernel's exported symbols.
 */
#ifndef _FIPS_SHA256_LIB_H
#define _FIPS_SHA256_LIB_H

#include <crypto/sha2.h>

/* SHA-224 */
void fips_lib_sha224_init(struct sha224_ctx *ctx);
void fips_lib_sha224_final(struct sha224_ctx *ctx, u8 out[SHA224_DIGEST_SIZE]);
void fips_lib_sha224(const u8 *data, size_t len, u8 out[SHA224_DIGEST_SIZE]);

/* SHA-256 */
void fips_lib_sha256_init(struct sha256_ctx *ctx);
void fips_lib_sha256_final(struct sha256_ctx *ctx, u8 out[SHA256_DIGEST_SIZE]);
void fips_lib_sha256(const u8 *data, size_t len, u8 out[SHA256_DIGEST_SIZE]);
void fips_lib_sha256_finup_2x(const struct sha256_ctx *ctx,
			      const u8 *data1, const u8 *data2, size_t len,
			      u8 out1[SHA256_DIGEST_SIZE],
			      u8 out2[SHA256_DIGEST_SIZE]);
bool fips_lib_sha256_finup_2x_is_optimized(void);

/*
 * Internal update and HMAC-init primitives.  These are exposed so that
 * callers (e.g. the shash wrapper in crypto/sha256.c) can bypass the
 * sha2.h inline wrappers — which call the kernel's __sha256_update /
 * __hmac_sha256_init symbols — and call our in-boundary versions directly.
 */
void fips_lib__sha256_update(struct __sha256_ctx *ctx,
			     const u8 *data, size_t len);
void fips_lib__hmac_sha256_init(struct __hmac_sha256_ctx *ctx,
				const struct __hmac_sha256_key *key);

/* HMAC-SHA-224 */
void fips_lib_hmac_sha224_preparekey(struct hmac_sha224_key *key,
				     const u8 *raw_key, size_t raw_key_len);
void fips_lib_hmac_sha224_init_usingrawkey(struct hmac_sha224_ctx *ctx,
					   const u8 *raw_key,
					   size_t raw_key_len);
void fips_lib_hmac_sha224_final(struct hmac_sha224_ctx *ctx,
				u8 out[SHA224_DIGEST_SIZE]);
void fips_lib_hmac_sha224(const struct hmac_sha224_key *key,
			  const u8 *data, size_t data_len,
			  u8 out[SHA224_DIGEST_SIZE]);
void fips_lib_hmac_sha224_usingrawkey(const u8 *raw_key, size_t raw_key_len,
				      const u8 *data, size_t data_len,
				      u8 out[SHA224_DIGEST_SIZE]);

/* HMAC-SHA-256 */
void fips_lib_hmac_sha256_preparekey(struct hmac_sha256_key *key,
				     const u8 *raw_key, size_t raw_key_len);
void fips_lib_hmac_sha256_init_usingrawkey(struct hmac_sha256_ctx *ctx,
					   const u8 *raw_key,
					   size_t raw_key_len);
void fips_lib_hmac_sha256_final(struct hmac_sha256_ctx *ctx,
				u8 out[SHA256_DIGEST_SIZE]);
void fips_lib_hmac_sha256(const struct hmac_sha256_key *key,
			  const u8 *data, size_t data_len,
			  u8 out[SHA256_DIGEST_SIZE]);
void fips_lib_hmac_sha256_usingrawkey(const u8 *raw_key, size_t raw_key_len,
				      const u8 *data, size_t data_len,
				      u8 out[SHA256_DIGEST_SIZE]);

/* Initialise arch-specific SHA-256 acceleration (call once at module init). */
void fips_lib_sha256_arch_init(void);

#endif /* _FIPS_SHA256_LIB_H */
