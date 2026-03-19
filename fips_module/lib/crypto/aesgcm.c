// SPDX-License-Identifier: GPL-2.0
/*
 * fips_module private copy of lib/crypto/aesgcm.c
 *
 * Provides fips_lib_aesgcm_expandkey(), fips_lib_aesgcm_encrypt(), and
 * fips_lib_aesgcm_decrypt() — module-private implementations of AES-GCM
 * library functions for use within the FIPS cryptographic boundary.
 * These functions are not exported to vmlinux; all callers within fips_module
 * must include "lib/crypto/aesgcm_lib.h" and call the fips_lib_* variants
 * directly.
 *
 * Internal AES block operations are redirected to fips_lib_aes_expandkey()
 * and fips_lib_aes_encrypt() from lib/crypto/aes_lib.h rather than the
 * vmlinux aes_expandkey()/aes_encrypt() symbols.
 *
 * Minimal library implementation of GCM
 *
 * Copyright 2022 Google LLC
 */

#include <asm/irqflags.h>
#include <crypto/algapi.h>
#include <crypto/gcm.h>
#include <crypto/ghash.h>
#include "aes_lib.h"
#include "aesgcm_lib.h"

static void aesgcm_encrypt_block(const struct crypto_aes_ctx *ctx, void *dst,
				 const void *src)
{
	unsigned long flags;

	/*
	 * In AES-GCM, both the GHASH key derivation and the CTR mode
	 * encryption operate on known plaintext, making them susceptible to
	 * timing attacks on the encryption key. The AES library already
	 * mitigates this risk to some extent by pulling the entire S-box into
	 * the caches before doing any substitutions, but this strategy is more
	 * effective when running with interrupts disabled.
	 */
	local_irq_save(flags);
	fips_lib_aes_encrypt(ctx, dst, src);
	local_irq_restore(flags);
}

/**
 * fips_lib_aesgcm_expandkey - Expands the AES and GHASH keys for the AES-GCM
 *			       key schedule
 *
 * @ctx:	The data structure that will hold the AES-GCM key schedule
 * @key:	The AES encryption input key
 * @keysize:	The length in bytes of the input key
 * @authsize:	The size in bytes of the GCM authentication tag
 *
 * Returns: 0 on success, or -EINVAL if @keysize or @authsize contain values
 * that are not permitted by the GCM specification.
 */
int fips_lib_aesgcm_expandkey(struct aesgcm_ctx *ctx, const u8 *key,
			      unsigned int keysize, unsigned int authsize)
{
	u8 kin[AES_BLOCK_SIZE] = {};
	int ret;

	ret = crypto_gcm_check_authsize(authsize) ?:
	      fips_lib_aes_expandkey(&ctx->aes_ctx, key, keysize);
	if (ret)
		return ret;

	ctx->authsize = authsize;
	aesgcm_encrypt_block(&ctx->aes_ctx, &ctx->ghash_key, kin);

	return 0;
}

static void aesgcm_ghash(be128 *ghash, const be128 *key, const void *src,
			 int len)
{
	while (len > 0) {
		crypto_xor((u8 *)ghash, src, min(len, GHASH_BLOCK_SIZE));
		gf128mul_lle(ghash, key);

		src += GHASH_BLOCK_SIZE;
		len -= GHASH_BLOCK_SIZE;
	}
}

static void aesgcm_mac(const struct aesgcm_ctx *ctx, const u8 *src, int src_len,
		       const u8 *assoc, int assoc_len, __be32 *ctr, u8 *authtag)
{
	be128 tail = { cpu_to_be64(assoc_len * 8), cpu_to_be64(src_len * 8) };
	u8 buf[AES_BLOCK_SIZE];
	be128 ghash = {};

	aesgcm_ghash(&ghash, &ctx->ghash_key, assoc, assoc_len);
	aesgcm_ghash(&ghash, &ctx->ghash_key, src, src_len);
	aesgcm_ghash(&ghash, &ctx->ghash_key, &tail, sizeof(tail));

	ctr[3] = cpu_to_be32(1);
	aesgcm_encrypt_block(&ctx->aes_ctx, buf, ctr);
	crypto_xor_cpy(authtag, buf, (u8 *)&ghash, ctx->authsize);

	memzero_explicit(&ghash, sizeof(ghash));
	memzero_explicit(buf, sizeof(buf));
}

static void aesgcm_crypt(const struct aesgcm_ctx *ctx, u8 *dst, const u8 *src,
			 int len, __be32 *ctr)
{
	u8 buf[AES_BLOCK_SIZE];
	unsigned int n = 2;

	while (len > 0) {
		/*
		 * The counter increment below must not result in overflow or
		 * carry into the next 32-bit word, as this could result in
		 * inadvertent IV reuse, which must be avoided at all cost for
		 * stream ciphers such as AES-CTR. Given the range of 'int
		 * len', this cannot happen, so no explicit test is necessary.
		 */
		ctr[3] = cpu_to_be32(n++);
		aesgcm_encrypt_block(&ctx->aes_ctx, buf, ctr);
		crypto_xor_cpy(dst, src, buf, min(len, AES_BLOCK_SIZE));

		dst += AES_BLOCK_SIZE;
		src += AES_BLOCK_SIZE;
		len -= AES_BLOCK_SIZE;
	}
	memzero_explicit(buf, sizeof(buf));
}

/**
 * fips_lib_aesgcm_encrypt - Perform AES-GCM encryption on a block of data
 *
 * @ctx:	The AES-GCM key schedule
 * @dst:	Pointer to the ciphertext output buffer
 * @src:	Pointer the plaintext (may equal @dst for encryption in place)
 * @crypt_len:	The size in bytes of the plaintext and ciphertext.
 * @assoc:	Pointer to the associated data,
 * @assoc_len:	The size in bytes of the associated data
 * @iv:		The initialization vector (IV) to use for this block of data
 *		(must be 12 bytes in size as per the GCM spec recommendation)
 * @authtag:	The address of the buffer in memory where the authentication
 *		tag should be stored. The buffer is assumed to have space for
 *		@ctx->authsize bytes.
 */
void fips_lib_aesgcm_encrypt(const struct aesgcm_ctx *ctx, u8 *dst,
			     const u8 *src, int crypt_len, const u8 *assoc,
			     int assoc_len, const u8 iv[GCM_AES_IV_SIZE],
			     u8 *authtag)
{
	__be32 ctr[4];

	memcpy(ctr, iv, GCM_AES_IV_SIZE);

	aesgcm_crypt(ctx, dst, src, crypt_len, ctr);
	aesgcm_mac(ctx, dst, crypt_len, assoc, assoc_len, ctr, authtag);
}

/**
 * fips_lib_aesgcm_decrypt - Perform AES-GCM decryption on a block of data
 *
 * @ctx:	The AES-GCM key schedule
 * @dst:	Pointer to the plaintext output buffer
 * @src:	Pointer the ciphertext (may equal @dst for decryption in place)
 * @crypt_len:	The size in bytes of the plaintext and ciphertext.
 * @assoc:	Pointer to the associated data,
 * @assoc_len:	The size in bytes of the associated data
 * @iv:		The initialization vector (IV) to use for this block of data
 *		(must be 12 bytes in size as per the GCM spec recommendation)
 * @authtag:	The address of the buffer in memory where the authentication
 *		tag is stored.
 *
 * Returns: true on success, or false if the ciphertext failed authentication.
 * On failure, no plaintext will be returned.
 */
bool __must_check fips_lib_aesgcm_decrypt(const struct aesgcm_ctx *ctx,
					  u8 *dst, const u8 *src, int crypt_len,
					  const u8 *assoc, int assoc_len,
					  const u8 iv[GCM_AES_IV_SIZE],
					  const u8 *authtag)
{
	u8 tagbuf[AES_BLOCK_SIZE];
	__be32 ctr[4];

	memcpy(ctr, iv, GCM_AES_IV_SIZE);

	aesgcm_mac(ctx, src, crypt_len, assoc, assoc_len, ctr, tagbuf);
	if (crypto_memneq(authtag, tagbuf, ctx->authsize)) {
		memzero_explicit(tagbuf, sizeof(tagbuf));
		return false;
	}
	aesgcm_crypt(ctx, dst, src, crypt_len, ctr);
	return true;
}
