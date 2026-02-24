/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * FIPS Cryptographic Module - Algorithm entry-point prototypes
 *
 * This header is automatically included by all translation units in the
 * fips_module via the -include compiler flag.  It provides forward
 * declarations for the per-algorithm init/exit functions so that the
 * compiler does not emit -Wmissing-prototypes warnings when those
 * functions are defined in the individual source files.
 */
#ifndef _FIPS_ALG_H
#define _FIPS_ALG_H


/* AES block cipher */
int  fips_aes_generic_init(void);
void fips_aes_generic_exit(void);

/* AES modes */
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

/* GHASH */
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

/* Key derivation */
int  fips_kdf_sp800108_init(void);
void fips_kdf_sp800108_exit(void);

/* DRBG + JitterEntropy */
int  fips_drbg_init(void);
void fips_drbg_exit(void);
int  fips_jent_init(void);
void fips_jent_exit(void);

/* RSA */
int  fips_rsa_init(void);
void fips_rsa_exit(void);

/* ECDSA */
int  fips_ecdsa_init(void);
void fips_ecdsa_exit(void);

/* ECDH */
int  fips_ecdh_init(void);
void fips_ecdh_exit(void);

/* DH */
int  fips_dh_init(void);
void fips_dh_exit(void);

/* x86_64 hardware-accelerated */
int  fips_aesni_init(void);
void fips_aesni_exit(void);
int  fips_ghash_clmul_init(void);
void fips_ghash_clmul_exit(void);

/* ARM64 hardware-accelerated */
int  fips_aes_ce_init(void);
void fips_aes_ce_exit(void);
int  fips_ghash_ce_init(void);
void fips_ghash_ce_exit(void);
int  fips_sha3_ce_init(void);
void fips_sha3_ce_exit(void);

#endif /* _FIPS_ALG_H */
