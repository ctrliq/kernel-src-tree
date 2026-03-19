/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * FIPS module-internal SHA-384/512 and HMAC-SHA-384/512 library functions.
 *
 * These are provided by fips_module/lib/crypto/sha512.c, a private copy of
 * lib/crypto/sha512.c compiled inside the FIPS cryptographic boundary.
 */
#ifndef _FIPS_LIB_CRYPTO_SHA512_LIB_H
#define _FIPS_LIB_CRYPTO_SHA512_LIB_H

#include <crypto/sha2.h>

/* SHA-384 */
void fips_lib_sha384_init(struct sha384_ctx *ctx);
void fips_lib_sha384_final(struct sha384_ctx *ctx, u8 out[SHA384_DIGEST_SIZE]);
void fips_lib_sha384(const u8 *data, size_t len, u8 out[SHA384_DIGEST_SIZE]);

/* SHA-512 */
void fips_lib_sha512_init(struct sha512_ctx *ctx);
void fips_lib_sha512_final(struct sha512_ctx *ctx, u8 out[SHA512_DIGEST_SIZE]);
void fips_lib_sha512(const u8 *data, size_t len, u8 out[SHA512_DIGEST_SIZE]);

/*
 * Internal update and HMAC-init primitives exposed so that callers can
 * bypass the sha2.h inline wrappers (which call the kernel's exported
 * __sha512_update / __hmac_sha512_init symbols) and use our in-boundary
 * versions directly.
 */
void fips_lib__sha512_update(struct __sha512_ctx *ctx,
			     const u8 *data, size_t len);
void fips_lib__hmac_sha512_init(struct __hmac_sha512_ctx *ctx,
				const struct __hmac_sha512_key *key);

/* Inline update wrappers — mirror the inlines in include/crypto/sha2.h */
static inline void fips_lib_sha384_update(struct sha384_ctx *ctx,
					  const u8 *data, size_t len)
{
	fips_lib__sha512_update(&ctx->ctx, data, len);
}

static inline void fips_lib_sha512_update(struct sha512_ctx *ctx,
					  const u8 *data, size_t len)
{
	fips_lib__sha512_update(&ctx->ctx, data, len);
}

/* HMAC-SHA-384 */
void fips_lib_hmac_sha384_preparekey(struct hmac_sha384_key *key,
				     const u8 *raw_key, size_t raw_key_len);
void fips_lib_hmac_sha384_init_usingrawkey(struct hmac_sha384_ctx *ctx,
					   const u8 *raw_key,
					   size_t raw_key_len);
void fips_lib_hmac_sha384_final(struct hmac_sha384_ctx *ctx,
				u8 out[SHA384_DIGEST_SIZE]);
void fips_lib_hmac_sha384(const struct hmac_sha384_key *key,
			  const u8 *data, size_t data_len,
			  u8 out[SHA384_DIGEST_SIZE]);
void fips_lib_hmac_sha384_usingrawkey(const u8 *raw_key, size_t raw_key_len,
				      const u8 *data, size_t data_len,
				      u8 out[SHA384_DIGEST_SIZE]);

static inline void fips_lib_hmac_sha384_init(struct hmac_sha384_ctx *ctx,
					     const struct hmac_sha384_key *key)
{
	fips_lib__hmac_sha512_init(&ctx->ctx, &key->key);
}

static inline void fips_lib_hmac_sha384_update(struct hmac_sha384_ctx *ctx,
					       const u8 *data, size_t data_len)
{
	fips_lib__sha512_update(&ctx->ctx.sha_ctx, data, data_len);
}

/* HMAC-SHA-512 */
void fips_lib_hmac_sha512_preparekey(struct hmac_sha512_key *key,
				     const u8 *raw_key, size_t raw_key_len);
void fips_lib_hmac_sha512_init_usingrawkey(struct hmac_sha512_ctx *ctx,
					   const u8 *raw_key,
					   size_t raw_key_len);
void fips_lib_hmac_sha512_final(struct hmac_sha512_ctx *ctx,
				u8 out[SHA512_DIGEST_SIZE]);
void fips_lib_hmac_sha512(const struct hmac_sha512_key *key,
			  const u8 *data, size_t data_len,
			  u8 out[SHA512_DIGEST_SIZE]);
void fips_lib_hmac_sha512_usingrawkey(const u8 *raw_key, size_t raw_key_len,
				      const u8 *data, size_t data_len,
				      u8 out[SHA512_DIGEST_SIZE]);

static inline void fips_lib_hmac_sha512_init(struct hmac_sha512_ctx *ctx,
					     const struct hmac_sha512_key *key)
{
	fips_lib__hmac_sha512_init(&ctx->ctx, &key->key);
}

static inline void fips_lib_hmac_sha512_update(struct hmac_sha512_ctx *ctx,
					       const u8 *data, size_t data_len)
{
	fips_lib__sha512_update(&ctx->ctx.sha_ctx, data, data_len);
}

#endif /* _FIPS_LIB_CRYPTO_SHA512_LIB_H */
