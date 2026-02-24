// SPDX-License-Identifier: GPL-2.0-only
/*
 * FIPS Cryptographic Module
 *
 * Loadable kernel module providing FIPS-validated implementations of
 * AES, SHA, HMAC, CMAC, GMAC, KDF, DRBG, JitterEntropy, RSA, ECDSA,
 * ECDH, and DH cryptographic algorithms.
 *
 * All algorithm implementations are private copies of the corresponding
 * Linux kernel crypto implementations, modified to coexist within this
 * single module boundary.  They are registered with higher priority than
 * the kernel built-ins so that callers receive the FIPS-boundary
 * implementations when this module is loaded.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/crypto.h>

/*
 * Forward declarations for all algorithm init/exit functions.
 * Defined in the individual algorithm source files within this module.
 */

/* AES block cipher (crypto/aes_generic.c) */
int  fips_aes_generic_init(void);
void fips_aes_generic_exit(void);

/* AES modes (crypto/cbc.c, ctr.c, ecb.c, gcm.c, xts.c) */
int  fips_cbc_init(void);
void fips_cbc_exit(void);
int  fips_ctr_init(void);
void fips_ctr_exit(void);
int  fips_ecb_init(void);
void fips_ecb_exit(void);
int  fips_gcm_init(void);
void fips_gcm_exit(void);
int  fips_xts_init(void);
void fips_xts_exit(void);

/* GHASH (crypto/ghash-generic.c) */
int  fips_ghash_init(void);
void fips_ghash_exit(void);

/* SHA hash functions */
int  fips_sha1_init(void);
void fips_sha1_exit(void);
int  fips_sha256_init(void);
void fips_sha256_exit(void);
int  fips_sha512_init(void);
void fips_sha512_exit(void);
int  fips_sha3_init(void);
void fips_sha3_exit(void);

/* Message authentication codes */
int  fips_hmac_init(void);
void fips_hmac_exit(void);
int  fips_cmac_init(void);
void fips_cmac_exit(void);

/* Key derivation (crypto/kdf_sp800108.c) */
int  fips_kdf_sp800108_init(void);
void fips_kdf_sp800108_exit(void);

/* DRBG + JitterEntropy (crypto/drbg.c, jitterentropy-kcapi.c) */
int  fips_drbg_init(void);
void fips_drbg_exit(void);
int  fips_jent_init(void);
void fips_jent_exit(void);

/* RSA (crypto/rsa.c) */
int  fips_rsa_init(void);
void fips_rsa_exit(void);

/* ECDSA (crypto/ecdsa.c) */
int  fips_ecdsa_init(void);
void fips_ecdsa_exit(void);

/* ECDH (crypto/ecdh.c) */
int  fips_ecdh_init(void);
void fips_ecdh_exit(void);

/* DH (crypto/dh.c) */
int  fips_dh_init(void);
void fips_dh_exit(void);

/* x86_64 hardware-accelerated AES-NI / GHASH */
#if defined(CONFIG_X86_64) || defined(CONFIG_X86)
int  fips_aesni_init(void);
void fips_aesni_exit(void);
int  fips_ghash_clmul_init(void);
void fips_ghash_clmul_exit(void);
#endif

/* ARM64 hardware-accelerated AES-CE / GHASH-CE / SHA3-CE */
#ifdef CONFIG_ARM64
int  fips_aes_ce_init(void);
void fips_aes_ce_exit(void);
int  fips_ghash_ce_init(void);
void fips_ghash_ce_exit(void);
int  fips_sha3_ce_init(void);
void fips_sha3_ce_exit(void);
#endif

static int __init fips_module_init(void)
{
	int ret;

	/*
	 * Initialize algorithms in dependency order:
	 * 1. Base cipher (AES)
	 * 2. Hash primitives (GHASH, SHA)
	 * 3. Hardware accelerated variants
	 * 4. Cipher modes (ECB, CBC, CTR, XTS, GCM)
	 * 5. MAC algorithms (HMAC, CMAC)
	 * 6. KDF
	 * 7. Asymmetric (RSA, ECC, ECDSA, ECDH, DH)
	 * 8. DRBG and entropy (JitterEntropy, DRBG)
	 *
	 * The kernel's crypto test framework (testmgr.c / alg_test()) runs
	 * self-tests automatically for each algorithm as it is registered.
	 * When fips_enabled is set, test failures prevent the algorithm
	 * from being used.
	 */

	ret = fips_aes_generic_init();
	if (ret) {
		pr_err("fips_module: aes_generic init failed: %d\n", ret);
		goto err_aes_generic;
	}

	ret = fips_ghash_init();
	if (ret) {
		pr_err("fips_module: ghash init failed: %d\n", ret);
		goto err_ghash;
	}

	ret = fips_sha1_init();
	if (ret) {
		pr_err("fips_module: sha1 init failed: %d\n", ret);
		goto err_sha1;
	}

	ret = fips_sha256_init();
	if (ret) {
		pr_err("fips_module: sha256 init failed: %d\n", ret);
		goto err_sha256;
	}

	ret = fips_sha512_init();
	if (ret) {
		pr_err("fips_module: sha512 init failed: %d\n", ret);
		goto err_sha512;
	}

	ret = fips_sha3_init();
	if (ret) {
		pr_err("fips_module: sha3 init failed: %d\n", ret);
		goto err_sha3;
	}

#if defined(CONFIG_X86_64) || defined(CONFIG_X86)
	ret = fips_aesni_init();
	if (ret) {
		pr_err("fips_module: aesni init failed: %d\n", ret);
		goto err_aesni;
	}

	ret = fips_ghash_clmul_init();
	if (ret) {
		pr_err("fips_module: ghash_clmul init failed: %d\n", ret);
		goto err_ghash_clmul;
	}
#endif

#ifdef CONFIG_ARM64
	ret = fips_aes_ce_init();
	if (ret) {
		pr_err("fips_module: aes_ce init failed: %d\n", ret);
		goto err_aes_ce;
	}

	ret = fips_ghash_ce_init();
	if (ret) {
		pr_err("fips_module: ghash_ce init failed: %d\n", ret);
		goto err_ghash_ce;
	}

	ret = fips_sha3_ce_init();
	if (ret) {
		pr_err("fips_module: sha3_ce init failed: %d\n", ret);
		goto err_sha3_ce;
	}
#endif

	ret = fips_ecb_init();
	if (ret) {
		pr_err("fips_module: ecb init failed: %d\n", ret);
		goto err_ecb;
	}

	ret = fips_cbc_init();
	if (ret) {
		pr_err("fips_module: cbc init failed: %d\n", ret);
		goto err_cbc;
	}

	ret = fips_ctr_init();
	if (ret) {
		pr_err("fips_module: ctr init failed: %d\n", ret);
		goto err_ctr;
	}

	ret = fips_xts_init();
	if (ret) {
		pr_err("fips_module: xts init failed: %d\n", ret);
		goto err_xts;
	}

	ret = fips_gcm_init();
	if (ret) {
		pr_err("fips_module: gcm init failed: %d\n", ret);
		goto err_gcm;
	}

	ret = fips_hmac_init();
	if (ret) {
		pr_err("fips_module: hmac init failed: %d\n", ret);
		goto err_hmac;
	}

	ret = fips_cmac_init();
	if (ret) {
		pr_err("fips_module: cmac init failed: %d\n", ret);
		goto err_cmac;
	}

	ret = fips_kdf_sp800108_init();
	if (ret) {
		pr_err("fips_module: kdf_sp800108 init failed: %d\n", ret);
		goto err_kdf;
	}

	ret = fips_rsa_init();
	if (ret) {
		pr_err("fips_module: rsa init failed: %d\n", ret);
		goto err_rsa;
	}

	ret = fips_ecdsa_init();
	if (ret) {
		pr_err("fips_module: ecdsa init failed: %d\n", ret);
		goto err_ecdsa;
	}

	ret = fips_ecdh_init();
	if (ret) {
		pr_err("fips_module: ecdh init failed: %d\n", ret);
		goto err_ecdh;
	}

	ret = fips_dh_init();
	if (ret) {
		pr_err("fips_module: dh init failed: %d\n", ret);
		goto err_dh;
	}

	ret = fips_jent_init();
	if (ret) {
		pr_err("fips_module: jitterentropy init failed: %d\n", ret);
		goto err_jent;
	}

	ret = fips_drbg_init();
	if (ret) {
		pr_err("fips_module: drbg init failed: %d\n", ret);
		goto err_drbg;
	}

	pr_info("fips_module: all FIPS algorithms registered\n");
	return 0;

	/* Unwind in reverse order on error */
err_drbg:
	fips_jent_exit();
err_jent:
	fips_dh_exit();
err_dh:
	fips_ecdh_exit();
err_ecdh:
	fips_ecdsa_exit();
err_ecdsa:
	fips_rsa_exit();
err_rsa:
	fips_kdf_sp800108_exit();
err_kdf:
	fips_cmac_exit();
err_cmac:
	fips_hmac_exit();
err_hmac:
	fips_gcm_exit();
err_gcm:
	fips_xts_exit();
err_xts:
	fips_ctr_exit();
err_ctr:
	fips_cbc_exit();
err_cbc:
	fips_ecb_exit();
err_ecb:
#ifdef CONFIG_ARM64
	fips_sha3_ce_exit();
err_sha3_ce:
	fips_ghash_ce_exit();
err_ghash_ce:
	fips_aes_ce_exit();
err_aes_ce:
#endif
#if defined(CONFIG_X86_64) || defined(CONFIG_X86)
	fips_ghash_clmul_exit();
err_ghash_clmul:
	fips_aesni_exit();
err_aesni:
#endif
	fips_sha3_exit();
err_sha3:
	fips_sha512_exit();
err_sha512:
	fips_sha256_exit();
err_sha256:
	fips_sha1_exit();
err_sha1:
	fips_ghash_exit();
err_ghash:
	fips_aes_generic_exit();
err_aes_generic:
	return ret;
}

static void __exit fips_module_exit(void)
{
	/* Unregister in reverse initialization order */
	fips_drbg_exit();
	fips_jent_exit();
	fips_dh_exit();
	fips_ecdh_exit();
	fips_ecdsa_exit();
	fips_rsa_exit();
	fips_kdf_sp800108_exit();
	fips_cmac_exit();
	fips_hmac_exit();
	fips_gcm_exit();
	fips_xts_exit();
	fips_ctr_exit();
	fips_cbc_exit();
	fips_ecb_exit();
#ifdef CONFIG_ARM64
	fips_sha3_ce_exit();
	fips_ghash_ce_exit();
	fips_aes_ce_exit();
#endif
#if defined(CONFIG_X86_64) || defined(CONFIG_X86)
	fips_ghash_clmul_exit();
	fips_aesni_exit();
#endif
	fips_sha3_exit();
	fips_sha512_exit();
	fips_sha256_exit();
	fips_sha1_exit();
	fips_ghash_exit();
	fips_aes_generic_exit();
}

module_init(fips_module_init);
module_exit(fips_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FIPS Cryptographic Module");
MODULE_AUTHOR("CIQ");
MODULE_VERSION("1.0");
MODULE_IMPORT_NS("CRYPTO_INTERNAL");
