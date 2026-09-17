/* SPDX-License-Identifier: GPL-2.0-or-later WITH Linux-syscall-note */
/*
 * AMD Platform Management Framework (PMF) UAPI Header
 *
 * Copyright (c) 2026, Advanced Micro Devices, Inc.
 * All Rights Reserved.
 *
 * This file defines the user-space API for interacting with the AMD PMF
 * driver. It provides ioctl interfaces to query platform-specific metrics
 * such as power source, slider position, platform type, laptop placement,
 * and various BIOS input/output parameters.
 */

#ifndef _UAPI_LINUX_AMD_PMF_H
#define _UAPI_LINUX_AMD_PMF_H

#include <linux/bits.h>
#include <linux/ioctl.h>
#include <linux/types.h>

/**
 * AMD_PMF_IOC_MAGIC - Magic number for AMD PMF ioctl commands
 *
 * This magic number uniquely identifies AMD PMF ioctl operations.
 */
#define AMD_PMF_IOC_MAGIC	'p'

/**
 * IOCTL_AMD_PMF_POPULATE_DATA - ioctl command to retrieve PMF metrics data
 *
 * This ioctl command is used to populate the amd_pmf_info structure
 * with the requested PMF metrics information.
 */
#define IOCTL_AMD_PMF_POPULATE_DATA		_IOWR(AMD_PMF_IOC_MAGIC, 0x00, __u64)

#define AMD_PMF_BIOS_PARAMS_MAX		10

/* AMD PMF feature flags - bitmask indicating supported features */
#define AMD_PMF_FEAT_AUTO_MODE			BIT(0)
#define AMD_PMF_FEAT_STATIC_POWER_SLIDER	BIT(1)
#define AMD_PMF_FEAT_POLICY_BUILDER		BIT(2)
#define AMD_PMF_FEAT_DYNAMIC_POWER_SLIDER_AC	BIT(3)
#define AMD_PMF_FEAT_DYNAMIC_POWER_SLIDER_DC	BIT(4)

struct amd_pmf_info {
	__u64 size;

	/* Feature info */
	__u32 features_supported;

	/* Power and state info */
	__u32 platform_type;
	__u32 power_source;
	__u32 laptop_placement;
	__u32 lid_state;
	__u32 user_presence;
	__u32 slider_position;

	/* Thermal and power metrics */
	__s32 skin_temp;
	__u32 gfx_busy;
	__s32 ambient_light;
	__u32 avg_c0_residency;
	__u32 max_c0_residency;
	__u32 socket_power;

	/* BIOS parameters */
	__u32 bios_input[AMD_PMF_BIOS_PARAMS_MAX];
	__u32 bios_output[AMD_PMF_BIOS_PARAMS_MAX];
};

#endif /* _UAPI_LINUX_AMD_PMF_H */
