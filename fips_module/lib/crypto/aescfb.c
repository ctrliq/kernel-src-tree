// SPDX-License-Identifier: GPL-2.0
/*
 * fips_module private copy of lib/crypto/aescfb.c
 *
 * Provides fips_lib_aescfb_encrypt() and fips_lib_aescfb_decrypt() —
 * module-private implementations of AES-CFB library functions for use
 * within the FIPS cryptographic boundary.  These functions are not exported
 * to vmlinux; all callers within fips_module must include
 * "lib/crypto/aescfb_lib.h" and call the fips_lib_* variants directly.
 *
 * Internal AES block encryption is redirected to fips_lib_aes_encrypt()
 * from lib/crypto/aes_lib.h rather than the vmlinux aes_encrypt() symbol.
 *
 * Minimal library implementation of AES in CFB mode
 *
 * Copyright 2023 Google LLC
 */

#include <asm/irqflags.h>
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include "aes_lib.h"
#include "aescfb_lib.h"

static void aescfb_encrypt_block(const struct crypto_aes_ctx *ctx, void *dst,
				 const void *src)
{
	unsigned long flags;

	/*
	 * In AES-CFB, the AES encryption operates on known 'plaintext' (the IV
	 * and ciphertext), making it susceptible to timing attacks on the
	 * encryption key. The AES library already mitigates this risk to some
	 * extent by pulling the entire S-box into the caches before doing any
	 * substitutions, but this strategy is more effective when running with
	 * interrupts disabled.
	 */
	local_irq_save(flags);
	fips_lib_aes_encrypt(ctx, dst, src);
	local_irq_restore(flags);
}

/**
 * fips_lib_aescfb_encrypt - Perform AES-CFB encryption on a block of data
 *
 * @ctx:	The AES-CFB key schedule
 * @dst:	Pointer to the ciphertext output buffer
 * @src:	Pointer the plaintext (may equal @dst for encryption in place)
 * @len:	The size in bytes of the plaintext and ciphertext.
 * @iv:		The initialization vector (IV) to use for this block of data
 */
void fips_lib_aescfb_encrypt(const struct crypto_aes_ctx *ctx, u8 *dst,
			     const u8 *src, int len,
			     const u8 iv[AES_BLOCK_SIZE])
{
	u8 ks[AES_BLOCK_SIZE];
	const u8 *v = iv;

	while (len > 0) {
		aescfb_encrypt_block(ctx, ks, v);
		crypto_xor_cpy(dst, src, ks, min(len, AES_BLOCK_SIZE));
		v = dst;

		dst += AES_BLOCK_SIZE;
		src += AES_BLOCK_SIZE;
		len -= AES_BLOCK_SIZE;
	}

	memzero_explicit(ks, sizeof(ks));
}

/**
 * fips_lib_aescfb_decrypt - Perform AES-CFB decryption on a block of data
 *
 * @ctx:	The AES-CFB key schedule
 * @dst:	Pointer to the plaintext output buffer
 * @src:	Pointer the ciphertext (may equal @dst for decryption in place)
 * @len:	The size in bytes of the plaintext and ciphertext.
 * @iv:		The initialization vector (IV) to use for this block of data
 */
void fips_lib_aescfb_decrypt(const struct crypto_aes_ctx *ctx, u8 *dst,
			     const u8 *src, int len,
			     const u8 iv[AES_BLOCK_SIZE])
{
	u8 ks[2][AES_BLOCK_SIZE];

	aescfb_encrypt_block(ctx, ks[0], iv);

	for (int i = 0; len > 0; i ^= 1) {
		if (len > AES_BLOCK_SIZE)
			/*
			 * Generate the keystream for the next block before
			 * performing the XOR, as that may update in place and
			 * overwrite the ciphertext.
			 */
			aescfb_encrypt_block(ctx, ks[!i], src);

		crypto_xor_cpy(dst, src, ks[i], min(len, AES_BLOCK_SIZE));

		dst += AES_BLOCK_SIZE;
		src += AES_BLOCK_SIZE;
		len -= AES_BLOCK_SIZE;
	}

	memzero_explicit(ks, sizeof(ks));
}
