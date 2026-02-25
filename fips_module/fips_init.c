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

/*
 * fips_run_selftests - explicitly run the module-private self-tests.
 *
 * The kernel's cryptomgr calls the kernel's own alg_test() asynchronously
 * for each algorithm when it is registered.  That code lives in vmlinux and
 * is outside the FIPS cryptographic boundary defined by this module.
 *
 * This function calls fips_alg_test() instead, which resolves at link time
 * to the module-local alg_test() defined in our bundled testmgr.c copy.
 * Every algorithm registered by the module is tested explicitly before
 * fips_module_init() returns, so:
 *   - the test code is inside the FIPS boundary, and
 *   - a test failure aborts module loading.
 *
 * Template-based algorithms (ecb, cbc, ctr, xts, gcm, hmac, cmac) are
 * tested by passing the algorithm name as both 'driver' and 'alg'; the
 * crypto API instantiates the template using the highest-priority
 * implementation, which is always this module's copy (priority 100000).
 *
 * DRBG variants are also tested here for completeness; the DRBG init code
 * already runs a subset of these tests internally via the module-local
 * alg_test() as a side-effect of module linking.
 */
static int fips_run_selftests(void)
{
	int ret;

/*
 * Helper macro: call fips_alg_test() and abort with an error message on
 * any non-zero return.  alg_test() returns 0 for both "test passed" and
 * "no test registered for this algorithm", so only genuine failures (< 0)
 * are reported.
 */
#define SELFTEST(driver, alg, type, mask)				\
	do {								\
		ret = fips_alg_test((driver), (alg), (type), (mask));	\
		if (ret) {						\
			pr_err("fips_module: self-test FAILED for"	\
			       " '%s' (%s): %d\n",			\
			       (driver), (alg), ret);			\
			return ret;					\
		}							\
	} while (0)

	/* ---------------------------------------------------------------
	 * AES block cipher (raw)
	 * alg_test() wraps raw ciphers in ECB automatically when type bits
	 * indicate CRYPTO_ALG_TYPE_CIPHER.
	 * --------------------------------------------------------------- */
	SELFTEST("fips-aes-generic", "aes",
		 CRYPTO_ALG_TYPE_CIPHER, CRYPTO_ALG_TYPE_MASK);

	/* ---------------------------------------------------------------
	 * GHASH
	 * --------------------------------------------------------------- */
	SELFTEST("fips-ghash-generic", "ghash", 0, 0);

	/* ---------------------------------------------------------------
	 * SHA hash functions
	 * --------------------------------------------------------------- */
	SELFTEST("fips-sha1-lib",          "sha1",     0, 0);
	SELFTEST("fips-sha224-lib",        "sha224",   0, 0);
	SELFTEST("fips-sha256-lib",        "sha256",   0, 0);
	SELFTEST("fips-sha384-lib",        "sha384",   0, 0);
	SELFTEST("fips-sha512-lib",        "sha512",   0, 0);
	SELFTEST("fips-sha3-224-generic",  "sha3-224", 0, 0);
	SELFTEST("fips-sha3-256-generic",  "sha3-256", 0, 0);
	SELFTEST("fips-sha3-384-generic",  "sha3-384", 0, 0);
	SELFTEST("fips-sha3-512-generic",  "sha3-512", 0, 0);

	/* ---------------------------------------------------------------
	 * HMAC (registered directly by the sha*-lib implementations)
	 * --------------------------------------------------------------- */
	SELFTEST("fips-hmac-sha1-lib",    "hmac(sha1)",   0, 0);
	SELFTEST("fips-hmac-sha224-lib",  "hmac(sha224)", 0, 0);
	SELFTEST("fips-hmac-sha256-lib",  "hmac(sha256)", 0, 0);
	SELFTEST("fips-hmac-sha384-lib",  "hmac(sha384)", 0, 0);
	SELFTEST("fips-hmac-sha512-lib",  "hmac(sha512)", 0, 0);

	/* HMAC-SHA3 (template-composed: hmac wraps our fips SHA3 shash) */
	SELFTEST("hmac(sha3-224)", "hmac(sha3-224)", 0, 0);
	SELFTEST("hmac(sha3-256)", "hmac(sha3-256)", 0, 0);
	SELFTEST("hmac(sha3-384)", "hmac(sha3-384)", 0, 0);
	SELFTEST("hmac(sha3-512)", "hmac(sha3-512)", 0, 0);

	/* ---------------------------------------------------------------
	 * AES cipher modes (template-based)
	 * Passing the algorithm name as 'driver' causes crypto_alloc_*()
	 * to select the highest-priority implementation (this module,
	 * priority 100000).
	 * --------------------------------------------------------------- */
	SELFTEST("ecb(aes)",         "ecb(aes)",         0, 0);
	SELFTEST("cbc(aes)",         "cbc(aes)",         0, 0);
	SELFTEST("ctr(aes)",         "ctr(aes)",         0, 0);
	SELFTEST("xts(aes)",         "xts(aes)",         0, 0);
	SELFTEST("gcm(aes)",         "gcm(aes)",         0, 0);
	SELFTEST("rfc4106(gcm(aes))", "rfc4106(gcm(aes))", 0, 0);

	/* CMAC */
	SELFTEST("cmac(aes)", "cmac(aes)", 0, 0);

	/* ---------------------------------------------------------------
	 * authenc: AEAD combining HMAC authentication with AES encryption.
	 * The authenc template composes registered hmac and skcipher
	 * implementations at instantiation time; no fixed driver names.
	 * rfc3686(ctr(aes)) is implemented within ctr.c.
	 * --------------------------------------------------------------- */
	SELFTEST("authenc(hmac(sha1),cbc(aes))",
		 "authenc(hmac(sha1),cbc(aes))", 0, 0);
	SELFTEST("authenc(hmac(sha1),ctr(aes))",
		 "authenc(hmac(sha1),ctr(aes))", 0, 0);
	SELFTEST("authenc(hmac(sha1),rfc3686(ctr(aes)))",
		 "authenc(hmac(sha1),rfc3686(ctr(aes)))", 0, 0);
	SELFTEST("authenc(hmac(sha256),cbc(aes))",
		 "authenc(hmac(sha256),cbc(aes))", 0, 0);
	SELFTEST("authenc(hmac(sha256),ctr(aes))",
		 "authenc(hmac(sha256),ctr(aes))", 0, 0);
	SELFTEST("authenc(hmac(sha256),rfc3686(ctr(aes)))",
		 "authenc(hmac(sha256),rfc3686(ctr(aes)))", 0, 0);
	SELFTEST("authenc(hmac(sha384),ctr(aes))",
		 "authenc(hmac(sha384),ctr(aes))", 0, 0);
	SELFTEST("authenc(hmac(sha384),rfc3686(ctr(aes)))",
		 "authenc(hmac(sha384),rfc3686(ctr(aes)))", 0, 0);
	SELFTEST("authenc(hmac(sha512),cbc(aes))",
		 "authenc(hmac(sha512),cbc(aes))", 0, 0);
	SELFTEST("authenc(hmac(sha512),ctr(aes))",
		 "authenc(hmac(sha512),ctr(aes))", 0, 0);
	SELFTEST("authenc(hmac(sha512),rfc3686(ctr(aes)))",
		 "authenc(hmac(sha512),rfc3686(ctr(aes)))", 0, 0);

	/* ---------------------------------------------------------------
	 * CCM: Counter with CBC-MAC (AEAD).
	 * ccm.c also registers cbcmac (the internal MAC primitive),
	 * ccm_base, ccm, and rfc4309 templates.
	 * --------------------------------------------------------------- */
	SELFTEST("cbcmac(aes)", "cbcmac(aes)", 0, 0);
	SELFTEST("ccm(aes)",         "ccm(aes)",         0, 0);
	SELFTEST("rfc4309(ccm(aes))", "rfc4309(ccm(aes))", 0, 0);

	/* ---------------------------------------------------------------
	 * Asymmetric / key-agreement algorithms
	 * --------------------------------------------------------------- */
	/* RSA */
	SELFTEST("fips-rsa-generic",    "rsa",             0, 0);
	SELFTEST("pkcs1pad(rsa)",       "pkcs1pad(rsa)",   0, 0);
	SELFTEST("pkcs1(rsa,none)",     "pkcs1(rsa,none)", 0, 0);
	SELFTEST("pkcs1(rsa,sha224)",   "pkcs1(rsa,sha224)", 0, 0);
	SELFTEST("pkcs1(rsa,sha256)",   "pkcs1(rsa,sha256)", 0, 0);
	SELFTEST("pkcs1(rsa,sha384)",   "pkcs1(rsa,sha384)", 0, 0);
	SELFTEST("pkcs1(rsa,sha512)",   "pkcs1(rsa,sha512)", 0, 0);

	/* ECDSA */
	SELFTEST("fips-ecdsa-nist-p192-generic", "ecdsa-nist-p192", 0, 0);
	SELFTEST("fips-ecdsa-nist-p256-generic", "ecdsa-nist-p256", 0, 0);
	SELFTEST("fips-ecdsa-nist-p384-generic", "ecdsa-nist-p384", 0, 0);
	SELFTEST("fips-ecdsa-nist-p521-generic", "ecdsa-nist-p521", 0, 0);

	/* ECDSA P1363 encoding wrappers (IEEE P1363 raw r||s format) */
	SELFTEST("p1363(ecdsa-nist-p256)", "p1363(ecdsa-nist-p256)", 0, 0);
	SELFTEST("p1363(ecdsa-nist-p384)", "p1363(ecdsa-nist-p384)", 0, 0);
	SELFTEST("p1363(ecdsa-nist-p521)", "p1363(ecdsa-nist-p521)", 0, 0);

	/* ECDSA X9.62 encoding wrappers (DER/ASN.1 format) */
	SELFTEST("x962(ecdsa-nist-p256)", "x962(ecdsa-nist-p256)", 0, 0);
	SELFTEST("x962(ecdsa-nist-p384)", "x962(ecdsa-nist-p384)", 0, 0);
	SELFTEST("x962(ecdsa-nist-p521)", "x962(ecdsa-nist-p521)", 0, 0);

	/* ECDH */
	SELFTEST("fips-ecdh-nist-p192-generic", "ecdh-nist-p192", 0, 0);
	SELFTEST("fips-ecdh-nist-p256-generic", "ecdh-nist-p256", 0, 0);
	SELFTEST("fips-ecdh-nist-p384-generic", "ecdh-nist-p384", 0, 0);

	/* DH */
	SELFTEST("fips-dh-generic", "dh", 0, 0);

	/* ---------------------------------------------------------------
	 * JitterEntropy RNG
	 * --------------------------------------------------------------- */
	SELFTEST("fips-jitterentropy_rng", "jitterentropy_rng", 0, 0);

	/* ---------------------------------------------------------------
	 * DRBG: all prediction-resistant and non-prediction-resistant
	 * variants for CTR(AES), Hash(SHA), and HMAC(SHA).
	 * --------------------------------------------------------------- */
	SELFTEST("fips-drbg_pr_ctr_aes128",    "drbg_pr_ctr_aes128",   0, 0);
	SELFTEST("fips-drbg_pr_ctr_aes192",    "drbg_pr_ctr_aes192",   0, 0);
	SELFTEST("fips-drbg_pr_ctr_aes256",    "drbg_pr_ctr_aes256",   0, 0);
	SELFTEST("fips-drbg_pr_sha256",        "drbg_pr_sha256",        0, 0);
	SELFTEST("fips-drbg_pr_sha384",        "drbg_pr_sha384",        0, 0);
	SELFTEST("fips-drbg_pr_sha512",        "drbg_pr_sha512",        0, 0);
	SELFTEST("fips-drbg_pr_hmac_sha256",   "drbg_pr_hmac_sha256",   0, 0);
	SELFTEST("fips-drbg_pr_hmac_sha384",   "drbg_pr_hmac_sha384",   0, 0);
	SELFTEST("fips-drbg_pr_hmac_sha512",   "drbg_pr_hmac_sha512",   0, 0);
	SELFTEST("fips-drbg_nopr_ctr_aes128",  "drbg_nopr_ctr_aes128",  0, 0);
	SELFTEST("fips-drbg_nopr_ctr_aes192",  "drbg_nopr_ctr_aes192",  0, 0);
	SELFTEST("fips-drbg_nopr_ctr_aes256",  "drbg_nopr_ctr_aes256",  0, 0);
	SELFTEST("fips-drbg_nopr_sha256",      "drbg_nopr_sha256",       0, 0);
	SELFTEST("fips-drbg_nopr_sha384",      "drbg_nopr_sha384",       0, 0);
	SELFTEST("fips-drbg_nopr_sha512",      "drbg_nopr_sha512",       0, 0);
	SELFTEST("fips-drbg_nopr_hmac_sha256", "drbg_nopr_hmac_sha256",  0, 0);
	SELFTEST("fips-drbg_nopr_hmac_sha384", "drbg_nopr_hmac_sha384",  0, 0);
	SELFTEST("fips-drbg_nopr_hmac_sha512", "drbg_nopr_hmac_sha512",  0, 0);

#if defined(CONFIG_X86_64) || defined(CONFIG_X86)
	/* ---------------------------------------------------------------
	 * x86_64 AES-NI / AVX hardware-accelerated implementations
	 * --------------------------------------------------------------- */
	SELFTEST("fips-aes-aesni", "aes",
		 CRYPTO_ALG_TYPE_CIPHER, CRYPTO_ALG_TYPE_MASK);
	SELFTEST("fips-ecb-aes-aesni", "ecb(aes)", 0, 0);
	SELFTEST("fips-cbc-aes-aesni", "cbc(aes)", 0, 0);
	SELFTEST("fips-ctr-aes-aesni", "ctr(aes)", 0, 0);
	SELFTEST("fips-xts-aes-aesni", "xts(aes)", 0, 0);
	/* GHASH via CLMUL (pclmulqdq) */
	SELFTEST("fips-ghash-pclmulqdqni", "ghash", 0, 0);
#endif /* CONFIG_X86_64 || CONFIG_X86 */

#ifdef CONFIG_ARM64
	/* ---------------------------------------------------------------
	 * ARM64 Crypto Extension hardware-accelerated implementations
	 * --------------------------------------------------------------- */
	SELFTEST("fips-aes-ce", "aes",
		 CRYPTO_ALG_TYPE_CIPHER, CRYPTO_ALG_TYPE_MASK);
	SELFTEST("fips-ghash-neon",  "ghash",    0, 0);
	SELFTEST("fips-gcm-aes-ce",  "gcm(aes)", 0, 0);
	SELFTEST("fips-sha3-224-ce", "sha3-224", 0, 0);
	SELFTEST("fips-sha3-256-ce", "sha3-256", 0, 0);
	SELFTEST("fips-sha3-384-ce", "sha3-384", 0, 0);
	SELFTEST("fips-sha3-512-ce", "sha3-512", 0, 0);
#endif /* CONFIG_ARM64 */

#undef SELFTEST

	return 0;
}

static int __init fips_module_init(void)
{
	int ret;

	/*
	 * Initialize algorithms in dependency order:
	 * 1. Base cipher (AES)
	 * 2. Hash primitives (GHASH, SHA)
	 * 3. Hardware accelerated variants
	 * 4. Cipher modes (ECB, CBC, CTR, XTS, GCM)
	 * 5. MAC algorithms (HMAC, CMAC) and AEAD templates (authenc)
	 * 6. KDF
	 * 7. Asymmetric (RSA, ECC, ECDSA, ECDH, DH)
	 * 8. DRBG and entropy (JitterEntropy, DRBG)
	 *
	 * After all algorithms are registered, fips_run_selftests() is called
	 * to explicitly exercise the module-private copy of alg_test() and its
	 * test vectors.  A self-test failure aborts module loading.
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

	ret = fips_authenc_init();
	if (ret) {
		pr_err("fips_module: authenc init failed: %d\n", ret);
		goto err_authenc;
	}

	ret = fips_ccm_init();
	if (ret) {
		pr_err("fips_module: ccm init failed: %d\n", ret);
		goto err_ccm;
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

	/*
	 * Run the module-private self-tests for every registered algorithm.
	 * fips_run_selftests() calls fips_alg_test() which resolves to the
	 * local copy of alg_test() in this module's bundled testmgr.c, not
	 * the kernel's built-in version triggered by the cryptomgr.
	 */
	ret = fips_run_selftests();
	if (ret) {
		pr_err("fips_module: self-tests FAILED: %d\n", ret);
		goto err_selftests;
	}

	pr_info("fips_module: all FIPS algorithms registered and "
		"self-tests passed\n");
	return 0;

	/* Unwind in reverse order on error */
err_selftests:
	fips_drbg_exit();
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
	fips_ccm_exit();
err_ccm:
	fips_authenc_exit();
err_authenc:
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
	fips_ccm_exit();
	fips_authenc_exit();
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
