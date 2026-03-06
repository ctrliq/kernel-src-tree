// SPDX-License-Identifier: GPL-2.0-only
/*
 * fips_algtest - enforce fips_module's FIPS algorithm approval list
 *
 * Registers a CRYPTO_MSG_ALG_REGISTER notifier with priority higher than
 * algboss (which uses priority 0) so that every algorithm registration is
 * screened against fips_module's own alg_test_descs[] table before the
 * vmlinux cryptomgr gets a chance to schedule a test thread.
 *
 * Two cases on CRYPTO_MSG_ALG_REGISTER:
 *
 * 1. Algorithm NOT in fips_module's approved list (fips_alg_is_allowed()==0):
 *    crypto_alg_tested() is called synchronously with -ECANCELED, which
 *    marks the algorithm with CRYPTO_ALG_FIPS_INTERNAL, signals the larval's
 *    completion, and makes the algorithm unavailable to external callers.
 *    NOTIFY_STOP is returned so algboss never sees the event.
 *    This path is safe to run synchronously because fips_alg_is_allowed()
 *    returning 0 guarantees fips_alg_test() exits before any crypto_alloc_*()
 *    call, so there is no risk of waiting on an unresolved larval.
 *
 * 2. Algorithm IS in fips_module's approved list (fips_alg_is_allowed()==1):
 *    A kthread is spawned (mirroring algboss's cryptomgr_schedule_test())
 *    that calls fips_alg_test() then crypto_alg_tested() with the result.
 *    NOTIFY_STOP is returned immediately so algboss never schedules its own
 *    test thread, ensuring the vmlinux alg_test() is never used.
 *    The kthread is necessary here because fips_alg_test() runs full test
 *    vectors which call crypto_alloc_*() internally; running this on the
 *    registration call stack while the algorithm's larval is still pending
 *    can deadlock on template-composed algorithm lookups.
 *
 * Fail-safe policy: any algorithm not explicitly listed as fips_allowed=1
 * in fips_module's alg_test_descs[] is blocked.  Unknown algorithms are
 * denied, not permitted.
 *
 * Locking note:
 *   CRYPTO_MSG_ALG_REGISTER is fired from crypto_schedule_test() after
 *   crypto_alg_sem has been released, so calling crypto_alg_tested() from
 *   within this notifier (case 1) is safe — no lock ordering violation.
 */

#include <linux/crypto.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <crypto/algapi.h>

/*
 * crypto_alg_tested() is EXPORT_SYMBOL_GPL but is not declared in any
 * public header (it lives in crypto/internal.h).  Forward-declare it here.
 */
void crypto_alg_tested(const char *name, int err);

struct fips_test_param {
	char driver[CRYPTO_MAX_ALG_NAME];
	char alg[CRYPTO_MAX_ALG_NAME];
	u32  type;
};

/*
 * Kthread body for approved algorithms: runs fips_alg_test() with
 * fips_module's own test vectors and reports the result to the crypto API
 * via crypto_alg_tested().  Mirrors algboss's cryptomgr_test() kthread.
 */
static int fips_algtest_thread(void *data)
{
	struct fips_test_param *param = data;
	int err;

	err = fips_alg_test(param->driver, param->alg,
			    param->type, CRYPTO_ALG_TESTED);
	crypto_alg_tested(param->driver, err);

	kfree(param);
	module_put_and_kthread_exit(0);
}

static int fips_algtest_notify(struct notifier_block *this,
			       unsigned long msg, void *data)
{
	struct crypto_alg *alg = data;
	struct fips_test_param *param;
	struct task_struct *thread;

	if (msg != CRYPTO_MSG_ALG_REGISTER)
		return NOTIFY_DONE;

	if (!fips_alg_is_allowed(alg->cra_name, alg->cra_driver_name)) {
		/*
		 * Not in fips_module's approved list.  Reject immediately
		 * without running any test vectors — safe to do synchronously
		 * since the rejection path does no crypto_alloc_*() calls.
		 */
		pr_info("fips_module: blocking non-approved algorithm %s (%s)\n",
			alg->cra_name, alg->cra_driver_name);
		crypto_alg_tested(alg->cra_driver_name, -ECANCELED);
		return NOTIFY_STOP;
	}

	/*
	 * Approved algorithm: spawn a kthread to run fips_alg_test() and
	 * report the result via crypto_alg_tested().  Return NOTIFY_STOP
	 * so algboss never schedules its own test thread — the vmlinux
	 * alg_test() is therefore never called for this algorithm.
	 */
	if (!try_module_get(THIS_MODULE))
		return NOTIFY_OK;

	param = kzalloc(sizeof(*param), GFP_KERNEL);
	if (!param)
		goto err_put_module;

	strscpy(param->driver, alg->cra_driver_name, sizeof(param->driver));
	strscpy(param->alg,    alg->cra_name,        sizeof(param->alg));
	param->type = alg->cra_flags;

	thread = kthread_run(fips_algtest_thread, param, "fips_algtest");
	if (IS_ERR(thread))
		goto err_free_param;

	return NOTIFY_STOP;

err_free_param:
	kfree(param);
err_put_module:
	module_put(THIS_MODULE);
	/*
	 * kthread_run or kzalloc failed — fall back to NOTIFY_OK so algboss
	 * can still schedule a test, rather than leaving the larval permanently
	 * unresolved.
	 */
	return NOTIFY_OK;
}

static struct notifier_block fips_algtest_notifier = {
	.notifier_call	= fips_algtest_notify,
	.priority	= 100,	/* higher than algboss (0) */
};

int fips_algtest_init(void)
{
	int ret;

	ret = crypto_register_notifier(&fips_algtest_notifier);
	if (ret)
		pr_err("fips_module: failed to register algtest notifier: %d\n",
		       ret);
	else
		pr_info("fips_module: FIPS algorithm enforcement notifier registered\n");
	return ret;
}

void fips_algtest_exit(void)
{
	crypto_unregister_notifier(&fips_algtest_notifier);
}
