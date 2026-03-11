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
#include <linux/fips.h>

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
	SELFTEST("rfc3686(ctr(aes))", "rfc3686(ctr(aes))", 0, 0);
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
	 * cbcmac is an internal primitive (fips_allowed=0); it is tested
	 * implicitly by the ccm(aes) and rfc4309(ccm(aes)) tests below.
	 * --------------------------------------------------------------- */
	SELFTEST("ccm(aes)",         "ccm(aes)",         0, 0);
	SELFTEST("rfc4309(ccm(aes))", "rfc4309(ccm(aes))", 0, 0);

	/* ---------------------------------------------------------------
	 * ESSIV: Encrypted Salt-Sector IV Generation.
	 * Used by dm-crypt and fscrypt for sector-based IV generation.
	 * Both the skcipher variant (essiv(cbc(aes),sha256)) and the AEAD
	 * variant (essiv(authenc(hmac(sha256),cbc(aes)),sha256)) are
	 * fips_allowed=1.  authenc must be registered before essiv.
	 * --------------------------------------------------------------- */
	SELFTEST("essiv(cbc(aes),sha256)",
		 "essiv(cbc(aes),sha256)", 0, 0);
	SELFTEST("essiv(authenc(hmac(sha256),cbc(aes)),sha256)",
		 "essiv(authenc(hmac(sha256),cbc(aes)),sha256)", 0, 0);

	/* ---------------------------------------------------------------
	 * Asymmetric / key-agreement algorithms
	 * --------------------------------------------------------------- */
	/* RSA */
	SELFTEST("fips-rsa-generic",    "rsa",             0, 0);
	SELFTEST("pkcs1pad(rsa)",       "pkcs1pad(rsa)",   0, 0);
	SELFTEST("pkcs1(rsa,sha224)",   "pkcs1(rsa,sha224)", 0, 0);
	SELFTEST("pkcs1(rsa,sha256)",   "pkcs1(rsa,sha256)", 0, 0);
	SELFTEST("pkcs1(rsa,sha384)",   "pkcs1(rsa,sha384)", 0, 0);
	SELFTEST("pkcs1(rsa,sha512)",   "pkcs1(rsa,sha512)", 0, 0);
	SELFTEST("pkcs1(rsa,sha3-256)", "pkcs1(rsa,sha3-256)", 0, 0);
	SELFTEST("pkcs1(rsa,sha3-384)", "pkcs1(rsa,sha3-384)", 0, 0);
	SELFTEST("pkcs1(rsa,sha3-512)", "pkcs1(rsa,sha3-512)", 0, 0);

	/* ECDSA (P-192 is not FIPS-approved; omit its self-test) */
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

	/* ECDH (P-192 is not FIPS-approved; omit its self-test) */
	SELFTEST("fips-ecdh-nist-p256-generic", "ecdh-nist-p256", 0, 0);
	SELFTEST("fips-ecdh-nist-p384-generic", "ecdh-nist-p384", 0, 0);

	/* DH: classical raw dh is not FIPS-approved; FFDHE groups are. */
	/* FFDHE (Finite-Field DH groups per RFC 7919).
	 * The ffdhe* templates are registered by fips_dh_init() as part of
	 * dh.c when CONFIG_CRYPTO_DH_RFC7919_GROUPS=y.  All five groups are
	 * marked fips_allowed=1 in testmgr.h. */
	SELFTEST("ffdhe2048(dh)", "ffdhe2048(dh)", 0, 0);
	SELFTEST("ffdhe3072(dh)", "ffdhe3072(dh)", 0, 0);
	SELFTEST("ffdhe4096(dh)", "ffdhe4096(dh)", 0, 0);
	SELFTEST("ffdhe6144(dh)", "ffdhe6144(dh)", 0, 0);
	SELFTEST("ffdhe8192(dh)", "ffdhe8192(dh)", 0, 0);

	/* ---------------------------------------------------------------
	 * JitterEntropy RNG
	 * --------------------------------------------------------------- */
	SELFTEST("jitterentropy_rng-fips", "jitterentropy_rng", 0, 0);

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
	/* ghash standalone is not FIPS-approved; tested implicitly via gcm(aes) */
#endif /* CONFIG_X86_64 || CONFIG_X86 */

#ifdef CONFIG_ARM64
	/* ---------------------------------------------------------------
	 * ARM64 Crypto Extension hardware-accelerated implementations
	 * --------------------------------------------------------------- */
	SELFTEST("fips-aes-ce", "aes",
		 CRYPTO_ALG_TYPE_CIPHER, CRYPTO_ALG_TYPE_MASK);
	/* ghash standalone is not FIPS-approved; tested implicitly via gcm(aes) */
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
	/*
	 * Refuse to load when the kernel is not in FIPS mode.  This module
	 * exists solely to provide a FIPS-validated cryptographic boundary;
	 * loading it on a non-FIPS system would add overhead with no benefit
	 * and could give a false impression of FIPS compliance.
	 */
	if (!fips_enabled) {
		pr_err("fips_module: refusing to load: kernel FIPS mode is not enabled\n");
		return -EPERM;
	}

	/*
	 * Bootstrap self-test: verify HMAC-SHA-256 correctness using the
	 * in-boundary fips_lib_ functions, before the crypto API is touched.
	 * This satisfies the FIPS 140-3 power-on self-test requirement for the
	 * algorithm that would be used for any module integrity check, and
	 * establishes that the SHA-256 implementation is trustworthy before
	 * it is relied upon by every subsequent operation.
	 */
	if (fips_sha256_bootstrap_selftest())
		panic("fips_module: SHA-256 bootstrap self-test FAILED\n");

	/*
	 * Register the CRYPTO_MSG_ALG_REGISTER notifier FIRST, before any
	 * algorithm is registered, so that every registration event is
	 * intercepted by fips_module's notifier (priority 100) rather than
	 * algboss (priority 0).  This ensures that only fips_alg_test() is
	 * ever called for our algorithms — the vmlinux alg_test() is never
	 * used.
	 *
	 * After the notifier is in place, initialize algorithms in dependency
	 * order:
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
	 * test vectors.  A self-test failure panics the kernel — a FIPS module
	 * must never operate with unverified algorithm implementations.
	 */
	if (fips_algtest_init())
		panic("fips_module: algtest notifier init failed\n");

	/*
	 * Retroactively block non-approved algorithms that were already
	 * registered before fips_module loaded (vmlinux built-ins such as
	 * des3_ede, md5, rc4).  The notifier above covers future registrations;
	 * this sweep closes the gap for pre-existing ones.
	 */
	fips_sweep_preregistered_algs();

	if (fips_aes_generic_init())
		panic("fips_module: aes_generic init failed\n");
	if (fips_ghash_init())
		panic("fips_module: ghash init failed\n");
	if (fips_sha1_init())
		panic("fips_module: sha1 init failed\n");
	if (fips_sha256_init())
		panic("fips_module: sha256 init failed\n");
	if (fips_sha512_init())
		panic("fips_module: sha512 init failed\n");
	if (fips_sha3_init())
		panic("fips_module: sha3 init failed\n");

#if defined(CONFIG_X86_64) || defined(CONFIG_X86)
	if (fips_aesni_init())
		panic("fips_module: aesni init failed\n");
	if (fips_ghash_clmul_init())
		panic("fips_module: ghash_clmul init failed\n");
#endif

#ifdef CONFIG_ARM64
	if (fips_aes_ce_init())
		panic("fips_module: aes_ce init failed\n");
	if (fips_ghash_ce_init())
		panic("fips_module: ghash_ce init failed\n");
	if (fips_sha3_ce_init())
		panic("fips_module: sha3_ce init failed\n");
#endif

	if (fips_ecb_init())
		panic("fips_module: ecb init failed\n");
	if (fips_cbc_init())
		panic("fips_module: cbc init failed\n");
	if (fips_ctr_init())
		panic("fips_module: ctr init failed\n");
	if (fips_xts_init())
		panic("fips_module: xts init failed\n");
	if (fips_gcm_init())
		panic("fips_module: gcm init failed\n");
	if (fips_hmac_init())
		panic("fips_module: hmac init failed\n");
	if (fips_cmac_init())
		panic("fips_module: cmac init failed\n");
	if (fips_authenc_init())
		panic("fips_module: authenc init failed\n");
	if (fips_ccm_init())
		panic("fips_module: ccm init failed\n");
	if (fips_essiv_init())
		panic("fips_module: essiv init failed\n");
	if (fips_kdf_sp800108_init())
		panic("fips_module: kdf_sp800108 init failed\n");
	if (fips_rsa_init())
		panic("fips_module: rsa init failed\n");
	if (fips_ecdsa_init())
		panic("fips_module: ecdsa init failed\n");

	/*
	 * JitterEntropy must be registered before ECDH and DH.  The ECDH
	 * shared-secret implementation (ecc.c) calls
	 * crypto_alloc_rng("jitterentropy_rng", 0, 0) for its
	 * point-multiplication blinding scalar.  If jent is not yet in the
	 * crypto_alg_list when the ECDH test kthread runs, that lookup
	 * returns -ENOENT and the P-384 self-test panics.  Registering jent
	 * first ensures it is at least present as a larval entry — the crypto
	 * API will wait for the larval to be resolved rather than failing.
	 */
	if (fips_jent_init())
		panic("fips_module: jitterentropy init failed\n");
	if (fips_ecdh_init())
		panic("fips_module: ecdh init failed\n");
	if (fips_dh_init())
		panic("fips_module: dh init failed\n");
	if (fips_drbg_init())
		panic("fips_module: drbg init failed\n");

	/*
	 * Run the module-private self-tests for every registered algorithm.
	 * fips_run_selftests() calls fips_alg_test() which resolves to the
	 * local copy of alg_test() in this module's bundled testmgr.c, not
	 * the kernel's built-in version triggered by the cryptomgr.
	 */
	if (fips_run_selftests())
		panic("fips_module: self-tests FAILED\n");

	/*
	 * All algorithms are registered and self-tested.  Now redirect
	 * getrandom(2) and /dev/random to the in-boundary FIPS DRBG.
	 */
	if (fips_extrng_init())
		panic("fips_module: extrng init failed\n");

	pr_info("fips_module: all FIPS algorithms registered and "
		"self-tests passed\n");
	return 0;
}

module_init(fips_module_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FIPS Cryptographic Module");
MODULE_AUTHOR("CIQ");
MODULE_VERSION("1.0");
MODULE_IMPORT_NS("CRYPTO_INTERNAL");
