// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * AMD Platform Management Framework Util Layer
 *
 * Copyright (c) 2026, Advanced Micro Devices, Inc.
 * All Rights Reserved.
 *
 * Authors: Shyam Sundar S K <Shyam-sundar.S-k@amd.com>
 *	    Sanket Goswami <Sanket.Goswami@amd.com>
 */

#include <linux/amd-pmf.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include "pmf.h"

static struct amd_pmf_dev *pmf_dev_handle;
static DEFINE_MUTEX(pmf_util_lock);

static long amd_pmf_set_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return -ENOTTY;
}

static int amd_pmf_open(struct inode *inode, struct file *filp)
{
	guard(mutex)(&pmf_util_lock);
	if (!pmf_dev_handle)
		return -ENODEV;

	filp->private_data = pmf_dev_handle;
	return 0;
}

static const struct file_operations pmf_if_ops = {
	.owner          = THIS_MODULE,
	.open           = amd_pmf_open,
	.unlocked_ioctl = amd_pmf_set_ioctl,
};

static struct miscdevice amd_pmf_util_if = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "amdpmf_interface",
	.fops		= &pmf_if_ops,
};

int amd_pmf_cdev_register(struct amd_pmf_dev *dev)
{
	int ret;

	guard(mutex)(&pmf_util_lock);
	pmf_dev_handle = dev;
	ret = misc_register(&amd_pmf_util_if);
	if (ret)
		pmf_dev_handle = NULL;

	return ret;
}

void amd_pmf_cdev_unregister(void)
{
	guard(mutex)(&pmf_util_lock);
	if (pmf_dev_handle)
		misc_deregister(&amd_pmf_util_if);
	pmf_dev_handle = NULL;
}
