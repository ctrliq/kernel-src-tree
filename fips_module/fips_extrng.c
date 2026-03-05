// SPDX-License-Identifier: GPL-2.0-only
/*
 * fips_extrng - redirect /dev/random and getrandom(2) to the FIPS DRBG
 *
 * Registers a struct random_extrng hook via random_register_extrng() so
 * that all reads of /dev/random, /dev/urandom, and calls to getrandom(2)
 * are served by the fips_module HMAC-SHA-512 no-prediction-resistance
 * DRBG rather than the kernel's ChaCha20 CRNG.
 *
 * This replaces the hook installed by the vmlinux crypto/rng.c
 * late_initcall (crypto_devrandom_rng) with an implementation that uses
 * an explicitly in-boundary DRBG handle whose entire entropy path stays
 * within fips_module:
 *
 *   getrandom(2) / read(/dev/random) / read(/dev/urandom)
 *     → fips_extrng_read_iter()
 *       → crypto_rng_get_bytes(fips_extrng_drbg)
 *         → drbg_kcapi_random() [fips_module/crypto/drbg.c]
 *           → drbg_generate_long() → drbg_generate() → drbg_seed()
 *             → drbg_get_random_bytes()
 *               → crypto_rng_get_bytes(drbg->jent)
 *                 → fips-jitterentropy_rng
 *                   [fips_module/crypto/jitterentropy*.c]
 *
 * get_random_bytes() (vmlinux ChaCha20 CRNG) is never called.
 *
 * Reseed path (getrandom(GRND_RANDOM)):
 *   The caller sets @reseed=true.  fips_extrng_read_iter() acquires
 *   fips_extrng_reseed_lock, sets CRYPTO_TFM_REQ_NEED_RESEED on the
 *   DRBG tfm, then calls crypto_rng_get_bytes().  drbg_kcapi_random()
 *   sees the flag, passes reseed=true to drbg_generate_long(), which
 *   sets DRBG_SEED_STATE_UNSEEDED under drbg_mutex so the reseed and
 *   the first output block are atomic.  The flag is cleared by
 *   drbg_kcapi_random() after the call.
 *
 * Thread safety:
 *   Ordinary generate calls rely on the DRBG's internal drbg_mutex.
 *   The reseed path additionally holds fips_extrng_reseed_lock to
 *   ensure the flag-set and generate are atomic with respect to other
 *   concurrent reseed callers, matching the approach used by the vmlinux
 *   crypto_reseed_rng_lock in crypto/rng.c.
 */

#include <linux/random.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uio.h>
#include <crypto/rng.h>

/*
 * Use the fips_module's own HMAC-SHA-512 nopr DRBG by driver name to
 * guarantee we resolve to the in-boundary copy.
 */
#define FIPS_EXTRNG_DRBG_DRIVER  "fips-drbg_nopr_hmac_sha512"

/*
 * SP800-90A Table 3: max bits per request = 2^19 bits = 65536 bytes.
 */
#define FIPS_EXTRNG_MAX_REQUEST  (1 << 16)

static struct crypto_rng *fips_extrng_drbg;

/* Serialises the reseed-then-generate path. */
static DEFINE_MUTEX(fips_extrng_reseed_lock);

static ssize_t fips_extrng_read_iter(struct iov_iter *iter, bool reseed)
{
	u8 *tmp;
	ssize_t ret = 0;

	if (!iov_iter_count(iter))
		return 0;

	tmp = kmalloc(FIPS_EXTRNG_MAX_REQUEST, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	if (reseed) {
		mutex_lock(&fips_extrng_reseed_lock);
		crypto_tfm_set_flags(crypto_rng_tfm(fips_extrng_drbg),
				     CRYPTO_TFM_REQ_NEED_RESEED);
	}

	while (iov_iter_count(iter)) {
		size_t chunk = min_t(size_t, iov_iter_count(iter),
				     FIPS_EXTRNG_MAX_REQUEST);
		size_t copied;
		int err;

		err = crypto_rng_get_bytes(fips_extrng_drbg, tmp, chunk);
		if (err) {
			ret = ret ? ret : err;
			break;
		}

		copied = copy_to_iter(tmp, chunk, iter);
		ret += copied;

		if (copied < chunk) {
			ret = ret ? ret : -EFAULT;
			break;
		}

		if (need_resched()) {
			if (signal_pending_current())
				break;
			schedule();
		}
	}

	if (reseed)
		mutex_unlock(&fips_extrng_reseed_lock);

	memzero_explicit(tmp, FIPS_EXTRNG_MAX_REQUEST);
	kfree(tmp);
	return ret;
}

static const struct random_extrng fips_extrng = {
	.extrng_read_iter = fips_extrng_read_iter,
	.owner            = THIS_MODULE,
};

int fips_extrng_init(void)
{
	int ret;

	fips_extrng_drbg = crypto_alloc_rng(FIPS_EXTRNG_DRBG_DRIVER, 0, 0);
	if (IS_ERR(fips_extrng_drbg)) {
		ret = PTR_ERR(fips_extrng_drbg);
		fips_extrng_drbg = NULL;
		pr_err("fips_module: failed to allocate extrng DRBG: %d\n", ret);
		return ret;
	}

	/*
	 * Seed from within the FIPS boundary: NULL/0 causes drbg_instantiate()
	 * to call drbg_seed() which draws from drbg->jent exclusively.
	 */
	ret = crypto_rng_reset(fips_extrng_drbg, NULL, 0);
	if (ret) {
		pr_err("fips_module: extrng DRBG seeding failed: %d\n", ret);
		crypto_free_rng(fips_extrng_drbg);
		fips_extrng_drbg = NULL;
		return ret;
	}

	/*
	 * Replace the vmlinux crypto_devrandom_rng hook so all getrandom(2),
	 * /dev/random and /dev/urandom traffic is served by our DRBG.
	 */
	random_register_extrng(&fips_extrng);

	pr_info("fips_module: getrandom(2) and /dev/random redirected to "
		"FIPS DRBG (HMAC-SHA-512, jent-seeded)\n");
	return 0;
}

void fips_extrng_exit(void)
{
	/*
	 * Remove the hook first.  random_unregister_extrng() calls
	 * synchronize_rcu(), blocking until all in-flight getrandom() /
	 * /dev/random callers that already took the RCU read lock have
	 * returned.  Only then is it safe to free the DRBG handle.
	 */
	random_unregister_extrng();
	crypto_free_rng(fips_extrng_drbg);
	fips_extrng_drbg = NULL;
}
