// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Cryptographic API.
 *
 * RNG operations.
 *
 * Copyright (c) 2008 Neil Horman <nhorman@tuxdriver.com>
 * Copyright (c) 2015 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * Copyright (C) 2025 Ctrl IQ, Inc.
 * Author: Sultan Alsawaf <sultan@ciq.com>
 */

#include <linux/atomic.h>
#include <crypto/internal/rng.h>
#include <linux/err.h>
#include <linux/fips.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/rtmutex.h>
#include <linux/seq_file.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/cryptouser.h>
#include <net/netlink.h>

#include "internal.h"

static ____cacheline_aligned_in_smp DEFINE_RT_MUTEX(crypto_default_rng_lock);
struct crypto_rng *crypto_default_rng;
EXPORT_SYMBOL_GPL(crypto_default_rng);
static unsigned int crypto_default_rng_refcnt;
static bool drbg_registered __ro_after_init;

/*
 * Per-CPU RNG instances are only used by crypto_devrandom_rng. The global RNG,
 * crypto_default_rng, is only used directly by other drivers.
 *
 * Per-CPU instances of the DRBG are efficient because the DRBG itself supports
 * an arbitrary number of instances and can be seeded on a per-CPU basis.
 *
 * Specifically, the DRBG is seeded by the CRNG and the Jitter RNG. The CRNG is
 * globally accessible and is already per-CPU. And while the Jitter RNG _isn't_
 * per-CPU, creating a DRBG instance also creates a Jitter RNG instance;
 * therefore, per-CPU DRBG instances implies per-CPU Jitter RNG instances.
 */
struct cpu_rng_inst {
	local_lock_t lock;
	struct rt_mutex mlock;
	struct crypto_rng *rng;
	void *page;
};

static DEFINE_PER_CPU_ALIGNED(struct cpu_rng_inst, pcpu_default_rng) = {
	.lock = INIT_LOCAL_LOCK(pcpu_default_rng.lock),
	.mlock = __RT_MUTEX_INITIALIZER(pcpu_default_rng.mlock)
};
static DEFINE_PER_CPU_ALIGNED(struct cpu_rng_inst, pcpu_reseed_rng) = {
	/* The reseed instances don't use the local lock */
	.mlock = __RT_MUTEX_INITIALIZER(pcpu_reseed_rng.mlock)
};

int crypto_rng_reset(struct crypto_rng *tfm, const u8 *seed, unsigned int slen)
{
	struct crypto_alg *alg = tfm->base.__crt_alg;
	u8 *buf = NULL;
	int err;

	if (!seed && slen) {
		buf = kmalloc(slen, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		err = get_random_bytes_wait(buf, slen);
		if (err)
			goto out;
		seed = buf;
	}

	crypto_stats_get(alg);
	err = crypto_rng_alg(tfm)->seed(tfm, seed, slen);
	crypto_stats_rng_seed(alg, err);
out:
	kfree_sensitive(buf);
	return err;
}
EXPORT_SYMBOL_GPL(crypto_rng_reset);

static int crypto_rng_init_tfm(struct crypto_tfm *tfm)
{
	return 0;
}

static unsigned int seedsize(struct crypto_alg *alg)
{
	struct rng_alg *ralg = container_of(alg, struct rng_alg, base);

	return ralg->seedsize;
}

#ifdef CONFIG_NET
static int crypto_rng_report(struct sk_buff *skb, struct crypto_alg *alg)
{
	struct crypto_report_rng rrng;

	memset(&rrng, 0, sizeof(rrng));

	strscpy(rrng.type, "rng", sizeof(rrng.type));

	rrng.seedsize = seedsize(alg);

	return nla_put(skb, CRYPTOCFGA_REPORT_RNG, sizeof(rrng), &rrng);
}
#else
static int crypto_rng_report(struct sk_buff *skb, struct crypto_alg *alg)
{
	return -ENOSYS;
}
#endif

static void crypto_rng_show(struct seq_file *m, struct crypto_alg *alg)
	__maybe_unused;
static void crypto_rng_show(struct seq_file *m, struct crypto_alg *alg)
{
	seq_printf(m, "type         : rng\n");
	seq_printf(m, "seedsize     : %u\n", seedsize(alg));
}

static const struct crypto_type crypto_rng_type = {
	.extsize = crypto_alg_extsize,
	.init_tfm = crypto_rng_init_tfm,
#ifdef CONFIG_PROC_FS
	.show = crypto_rng_show,
#endif
	.report = crypto_rng_report,
	.maskclear = ~CRYPTO_ALG_TYPE_MASK,
	.maskset = CRYPTO_ALG_TYPE_MASK,
	.type = CRYPTO_ALG_TYPE_RNG,
	.tfmsize = offsetof(struct crypto_rng, base),
};

struct crypto_rng *crypto_alloc_rng(const char *alg_name, u32 type, u32 mask)
{
	return crypto_alloc_tfm(alg_name, &crypto_rng_type, type, mask);
}
EXPORT_SYMBOL_GPL(crypto_alloc_rng);

static int crypto_get_rng(struct crypto_rng **rngp)
{
	struct crypto_rng *rng;
	int err;

	if (!*rngp) {
		rng = crypto_alloc_rng("stdrng", 0, 0);
		err = PTR_ERR(rng);
		if (IS_ERR(rng))
			return err;

		err = crypto_rng_reset(rng, NULL, crypto_rng_seedsize(rng));
		if (err) {
			crypto_free_rng(rng);
			return err;
		}

		*rngp = rng;
	}

	return 0;
}

int crypto_get_default_rng(void)
{
	int err;

	rt_mutex_lock(&crypto_default_rng_lock);
	err = crypto_get_rng(&crypto_default_rng);
	if (!err)
		crypto_default_rng_refcnt++;
	rt_mutex_unlock(&crypto_default_rng_lock);

	return err;
}
EXPORT_SYMBOL_GPL(crypto_get_default_rng);

void crypto_put_default_rng(void)
{
	rt_mutex_lock(&crypto_default_rng_lock);
	crypto_default_rng_refcnt--;
	rt_mutex_unlock(&crypto_default_rng_lock);
}
EXPORT_SYMBOL_GPL(crypto_put_default_rng);

#if defined(CONFIG_CRYPTO_RNG) || defined(CONFIG_CRYPTO_RNG_MODULE)
int crypto_del_default_rng(void)
{
	bool busy;

	rt_mutex_lock(&crypto_default_rng_lock);
	if (!(busy = crypto_default_rng_refcnt)) {
		crypto_free_rng(crypto_default_rng);
		crypto_default_rng = NULL;
	}
	rt_mutex_unlock(&crypto_default_rng_lock);

	return busy ? -EBUSY : 0;
}
EXPORT_SYMBOL_GPL(crypto_del_default_rng);
#endif

int crypto_register_rng(struct rng_alg *alg)
{
	struct crypto_alg *base = &alg->base;

	if (alg->seedsize > PAGE_SIZE / 8)
		return -EINVAL;

	/*
	 * In FIPS mode, the DRBG must take precedence over all other "stdrng"
	 * algorithms. Therefore, forbid registration of a non-DRBG stdrng in
	 * FIPS mode. All of the DRBG's driver names are prefixed with "drbg_".
	 * This also stops new stdrng instances from getting registered after it
	 * is known that the DRBG is registered, so a new module can't come in
	 * and pretend to be the DRBG. And when CONFIG_CRYPTO_FIPS is enabled,
	 * the DRBG is built into the kernel directly; it can't be a module.
	 */
	if (fips_enabled && !strcmp(base->cra_name, "stdrng") &&
	    (drbg_registered || strncmp(base->cra_driver_name, "drbg_", 5)))
		return -EINVAL;

	base->cra_type = &crypto_rng_type;
	base->cra_flags &= ~CRYPTO_ALG_TYPE_MASK;
	base->cra_flags |= CRYPTO_ALG_TYPE_RNG;

	return crypto_register_alg(base);
}
EXPORT_SYMBOL_GPL(crypto_register_rng);

void crypto_unregister_rng(struct rng_alg *alg)
{
	crypto_unregister_alg(&alg->base);
}
EXPORT_SYMBOL_GPL(crypto_unregister_rng);

int crypto_register_rngs(struct rng_alg *algs, int count)
{
	int i, ret;

	for (i = 0; i < count; i++) {
		ret = crypto_register_rng(algs + i);
		if (ret)
			goto err;
	}

	/*
	 * Track when the DRBG is registered in FIPS mode. The DRBG calls
	 * crypto_register_rngs() to register its stdrng instances, and since
	 * crypto_register_rng() only allows stdrng instances from the DRBG in
	 * FIPS mode, a successful stdrng registration means it was the DRBG.
	 * Just check the first alg in the array to see if it's called "stdrng",
	 * since all of the DRBG's algorithms are named "stdrng". Once
	 * drbg_registered is set to true, this if-statement is always false.
	 */
	if (fips_enabled && !strcmp(algs->base.cra_name, "stdrng"))
		drbg_registered = true;

	return 0;

err:
	for (--i; i >= 0; --i)
		crypto_unregister_rng(algs + i);

	return ret;
}
EXPORT_SYMBOL_GPL(crypto_register_rngs);

void crypto_unregister_rngs(struct rng_alg *algs, int count)
{
	int i;

	for (i = count - 1; i >= 0; --i)
		crypto_unregister_rng(algs + i);
}
EXPORT_SYMBOL_GPL(crypto_unregister_rngs);

/*
 * On non-PREEMPT_RT kernels, local locks disable preemption. When there's no
 * rng allocated, one must be allocated by calling crypto_get_rng(), which can
 * sleep. Therefore, crypto_get_rng() cannot be called under local_lock(), so if
 * our CPU's RNG instance doesn't have an rng allocated, we drop the local lock
 * and take a mutex lock instead. After the local lock is dropped, the current
 * task can be freely migrated to another CPU, which means that calling
 * local_lock() again might not result in the same instance getting locked as
 * before. That's why this function exists: to loop on calling local_lock() and
 * allocating an rng as needed with crypto_get_rng() until the current CPU's
 * instance is found to have an rng allocated. If crypto_get_rng() ever fails,
 * this function returns an error even if there are instances for other CPUs
 * which _do_ have an rng allocated.
 */
static __always_inline struct cpu_rng_inst *
lock_default_rng(struct crypto_rng **rng) __acquires(&cri->lock)
{
	struct cpu_rng_inst __percpu *pcri = &pcpu_default_rng;
	struct cpu_rng_inst *cri;
	int ret;

	while (1) {
		local_lock(&pcri->lock);
		cri = this_cpu_ptr(pcri);
		/*
		 * cri->rng can only transition from NULL to non-NULL. This may
		 * occur on a different CPU, thus cri->rng must be read
		 * atomically to prevent data races; this elides mlock by
		 * pairing with the WRITE_ONCE() in the slow path below.
		 *
		 * And if cri->rng is non-NULL, then it is good to go. To avoid
		 * data races due to load speculation on torn cri->rng loads
		 * _after_ the NULL check, one of the following is required:
		 * 	1. smp_acquire__after_ctrl_dep() in the if-statement
		 * 	2. All cri->rng reads are performed with READ_ONCE()
		 * 	3. cri->rng is never read again outside this function
		 *
		 * Option #3 yields the best performance, so this function
		 * provides the rng pointer as an output for the caller to use.
		 */
		*rng = READ_ONCE(cri->rng);
		if (likely(*rng))
			return cri;

		/*
		 * Slow path: there's no rng currently allocated to this instance.
		 * Release the local lock and acquire this instance's mlock to
		 * perform the allocation.
		 *
		 * Note that this task may be migrated to a different CPU now!
		 */
		local_unlock(&cri->lock);
		rt_mutex_lock(&cri->mlock);
		if (!cri->rng) {
			struct crypto_rng *new_rng = NULL;

			ret = crypto_get_rng(&new_rng);
			if (ret) {
				rt_mutex_unlock(&cri->mlock);
				break;
			}

			/*
			 * Pairs with READ_ONCE() above, because we might not be
			 * on the same CPU anymore as when we first got `cri`.
			 */
			WRITE_ONCE(cri->rng, new_rng);
		}
		rt_mutex_unlock(&cri->mlock);
	}

	/*
	 * Even if this task got migrated to another CPU that _does_ have an rng
	 * allocated, just bail out if crypto_get_rng() ever fails in order to
	 * avoid looping forever.
	 */
	return ERR_PTR(ret);
}

static __always_inline struct cpu_rng_inst *
lock_reseed_rng(struct crypto_rng **rng) __acquires(&cri->mlock)
{
	struct cpu_rng_inst __percpu *pcri = &pcpu_reseed_rng;
	struct cpu_rng_inst *cri;
	int ret;

	/*
	 * Use whichever CPU this task is currently running on, knowing full
	 * well that the task can freely migrate to other CPUs. The reseed RNG
	 * requires holding a lock across the entire devrandom read, so that
	 * another task cannot extract entropy from the same seed. In other
	 * words, when reseeding is requested, reseeding must be done every time
	 * every time mlock is acquired.
	 */
	cri = raw_cpu_ptr(pcri);
	rt_mutex_lock(&cri->mlock);
	if (likely(cri->rng)) {
		/*
		 * Since this rng instance wasn't just allocated, it needs to be
		 * explicitly reseeded. New rng instances are seeded on creation
		 * in crypto_get_rng() and thus don't need explicit reseeding.
		 */
		crypto_tfm_set_flags(crypto_rng_tfm(cri->rng),
				     CRYPTO_TFM_REQ_NEED_RESEED);
	} else {
		ret = crypto_get_rng(&cri->rng);
		if (ret) {
			rt_mutex_unlock(&cri->mlock);
			return ERR_PTR(ret);
		}
	}

	*rng = cri->rng;
	return cri;
}

#define lock_local_rng(rng, reseed) \
	({ (reseed) ? lock_reseed_rng(rng) : lock_default_rng(rng); })

#define unlock_local_rng(cri, reseed) \
do {						\
	if (reseed)				\
		rt_mutex_unlock(&(cri)->mlock);	\
	else					\
		local_unlock(&(cri)->lock);	\
} while (0)

static __always_inline void
clear_rng_page(struct cpu_rng_inst *cri, size_t count)
{
	/* For zeroing a whole page, clear_page() is faster than memset() */
	count < PAGE_SIZE ? memset(cri->page, 0, count) : clear_page(cri->page);
}

static ssize_t crypto_devrandom_read_iter(struct iov_iter *iter, bool reseed)
{
	/* lock_local_rng() puts us in atomic context for !reseed on non-RT */
	const bool atomic = !reseed && !IS_ENABLED(CONFIG_PREEMPT_RT);
	const bool user_no_reseed = !reseed && user_backed_iter(iter);
	size_t ulen, page_dirty_len = 0;
	struct cpu_rng_inst *cri;
	struct crypto_rng *rng;
	void __user *uaddr;
	struct page *upage;
	ssize_t ret = 0;

	if (unlikely(!iov_iter_count(iter)))
		return 0;

	/* Set up the starting user destination address and length */
	if (user_no_reseed) {
		if (iter_is_ubuf(iter)) {
			uaddr = iter->ubuf + iter->iov_offset;
			ulen = iov_iter_count(iter);
		} else if (iter_is_iovec(iter)) {
			uaddr = iter_iov_addr(iter);
			ulen = iter_iov_len(iter);
		} else {
			/*
			 * ITER_UBUF and ITER_IOVEC are the only user-backed
			 * iters. Bug out if a new user-backed iter appears.
			 */
			BUG();
		}
	}

restart:
	/*
	 * Pin the user page backing the current user destination address,
	 * potentially prefaulting to allocate a page for the destination. By
	 * prefaulting without the RNG lock held, the DRBG won't be blocked by
	 * time spent on page faults for this task, and thus the DRBG can still
	 * be used by other tasks.
	 */
	if (user_no_reseed && pin_user_pages_fast((unsigned long)uaddr, 1,
						  FOLL_WRITE, &upage) != 1)
		goto exit;

	cri = lock_local_rng(&rng, reseed);
	if (IS_ERR(cri)) {
		if (!ret)
			ret = PTR_ERR(cri);
		goto unpin_upage;
	}

	while (1) {
		size_t copied, i = min(iov_iter_count(iter), PAGE_SIZE);
		bool resched_without_lock = false;
		int err;

		/*
		 * Generate up to one page at a time, and align to a page
		 * boundary so we only need to pin one user page at a time.
		 */
		if (user_no_reseed)
			i = min3(i, PAGE_SIZE - offset_in_page(uaddr), ulen);

		/*
		 * On non-PREEMPT_RT kernels, local locks disable preemption.
		 * The DRBG's generate() function has a mutex lock, which could
		 * mean that we'll schedule while atomic if the mutex lock
		 * sleeps. However, that will never happen if we ensure that
		 * there's never any contention on the DRBG's mutex lock while
		 * we're atomic! Our local lock ensures calls to the DRBG are
		 * always serialized, so there's no contention from here. And
		 * the DRBG only uses its mutex lock from one other path, when
		 * an instance of the DRBG is freshly allocated, which we only
		 * do from crypto_get_rng(). So the DRBG's mutex lock is
		 * guaranteed to not have contention when we call generate() and
		 * thus it'll never sleep here. And of course, nothing else in
		 * generate() ever sleeps.
		 */
		err = crypto_rng_get_bytes(rng, cri->page, i);
		if (err) {
			if (!ret)
				ret = err;
			break;
		}

		/*
		 * Record the number of bytes used in cri->page and either copy
		 * directly to the user address without faulting, or copy to the
		 * iter which is always backed by kernel memory when !reseed &&
		 * !user_backed_iter(). When reseed == true, the iter may be
		 * backed by user memory, but we copy to it with the possibility
		 * of page faults anyway because we need to hold the lock across
		 * the entire call; this is why a mutex is used instead of a
		 * local lock for the reseed RNG, to permit sleeping without
		 * yielding the DRBG instance.
		 */
		page_dirty_len = max(i, page_dirty_len);
		if (user_no_reseed) {
			err = copy_to_user_nofault(uaddr, cri->page, i);
			if (err >= 0) {
				iov_iter_advance(iter, i - err);
				ret += i - err;
			}
			if (err)
				break;
		} else {
			/*
			 * We know that copying from cri->page is safe, so use
			 * _copy_to_iter() directly to skip check_copy_size().
			 */
			copied = _copy_to_iter(cri->page, i, iter);
			ret += copied;
			if (copied != i)
				break;
		}

		/*
		 * Quit when either the requested number of bytes have been
		 * generated or there is a pending signal.
		 */
		if (!iov_iter_count(iter) || signal_pending(current))
			break;

		/* Compute the next user destination address and length */
		if (user_no_reseed) {
			ulen -= i;
			if (likely(ulen)) {
				uaddr += i;
			} else {
				/*
				 * This path is only reachable by ITER_IOVEC
				 * because ulen is initialized to the request
				 * size for ITER_UBUF, and therefore ITER_UBUF
				 * will always quit at the iov_iter_count()
				 * check above before ulen can become zero.
				 *
				 * iter->iov_offset is guaranteed to be zero
				 * here, so iter_iov_{addr|len}() isn't needed.
				 */
				uaddr = iter_iov(iter)->iov_base;
				ulen = iter_iov(iter)->iov_len;
			}

			unpin_user_page(upage);
		}

		/*
		 * Reschedule right now if needed and we're not atomic. If we're
		 * atomic, then we must first drop the lock to reschedule.
		 */
		if (need_resched()) {
			if (atomic)
				resched_without_lock = true;
			else
				cond_resched();
		}

		/*
		 * Optimistically try to pin the next user page without
		 * faulting, so we don't need to clear cri->page and drop the
		 * lock on every iteration. If this fails, we fall back to
		 * pinning with the option to prefault.
		 */
		if (user_no_reseed && !resched_without_lock &&
		    pin_user_pages_fast_only((unsigned long)uaddr, 1,
					     FOLL_WRITE, &upage) == 1)
			continue;

		/*
		 * Restart if either rescheduling is needed (and requires
		 * dropping the lock since we're atomic) or the optimistic page
		 * pinning attempt failed.
		 *
		 * This always implies `reseed == false`, so unlock_local_rng()
		 * can just be passed `false` for reseed to eliminate a branch.
		 */
		if (resched_without_lock || user_no_reseed) {
			/*
			 * Clear the buffer of our latest random bytes before
			 * unlocking and potentially migrating CPUs, in which
			 * case we wouldn't have the same `cri` anymore.
			 */
			clear_rng_page(cri, page_dirty_len);
			unlock_local_rng(cri, false);
			page_dirty_len = 0;
			if (resched_without_lock)
				cond_resched();
			goto restart;
		}
	}

	if (page_dirty_len)
		clear_rng_page(cri, page_dirty_len);
	unlock_local_rng(cri, reseed);
unpin_upage:
	if (user_no_reseed)
		unpin_user_page(upage);
exit:
	return ret ? ret : -EFAULT;
}

static const struct random_extrng crypto_devrandom_rng = {
	.extrng_read_iter = crypto_devrandom_read_iter
};

static void __init alloc_pcpu_inst(struct cpu_rng_inst __percpu *pcri)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct cpu_rng_inst *cri = per_cpu_ptr(pcri, cpu);

		cri->page = (void *)__get_free_page(GFP_KERNEL | __GFP_NOFAIL);
		local_lock_init(&cri->lock);
	}
}

static int __init crypto_rng_init(void)
{
	if (!fips_enabled)
		return 0;

	/*
	 * Never fail to register the RNG override in FIPS mode because failure
	 * would result in the system quietly booting without the FIPS-mandated
	 * RNG installed. This would be catastrophic for FIPS compliance, hence
	 * the RNG override setup is *not* allowed to fail.
	 */
	alloc_pcpu_inst(&pcpu_default_rng);
	alloc_pcpu_inst(&pcpu_reseed_rng);
	random_register_extrng(&crypto_devrandom_rng);
	return 0;
}

late_initcall(crypto_rng_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Random Number Generator");
