/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2010-2015, 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (C) 2015 Linaro Ltd.
 */
#ifndef __QCOM_SCM_H
#define __QCOM_SCM_H

#include <linux/err.h>
#include <linux/types.h>
#include <linux/cpumask.h>

#define MAX_QCOM_SCM_ARGS 10
#define MAX_QCOM_SCM_RETS 3

#define QCOM_SCM_VERSION(major, minor)	(((major) << 16) | ((minor) & 0xFF))
#define QCOM_SCM_CPU_PWR_DOWN_L2_ON	0x0
#define QCOM_SCM_CPU_PWR_DOWN_L2_OFF	0x1
#define QCOM_SCM_HDCP_MAX_REQ_CNT	5

#define QCOM_SCM_SVC_BOOT		0x01
#define QCOM_SCM_BOOT_SET_ADDR		0x01
#define QCOM_SCM_BOOT_TERMINATE_PC	0x02
#define QCOM_SCM_BOOT_SET_DLOAD_MODE	0x10
#define QCOM_SCM_BOOT_SET_ADDR_MC	0x11
#define QCOM_SCM_BOOT_SET_REMOTE_STATE	0x0a
#define QCOM_SCM_FLUSH_FLAG_MASK	0x3
#define QCOM_SCM_BOOT_MAX_CPUS		4
#define QCOM_SCM_BOOT_MC_FLAG_AARCH64	BIT(0)
#define QCOM_SCM_BOOT_MC_FLAG_COLDBOOT	BIT(1)
#define QCOM_SCM_BOOT_MC_FLAG_WARMBOOT	BIT(2)

#define QCOM_SCM_SVC_PIL		0x02
#define QCOM_SCM_PIL_PAS_INIT_IMAGE	0x01
#define QCOM_SCM_PIL_PAS_MEM_SETUP	0x02
#define QCOM_SCM_PIL_PAS_AUTH_AND_RESET	0x05
#define QCOM_SCM_PIL_PAS_SHUTDOWN	0x06
#define QCOM_SCM_PIL_PAS_IS_SUPPORTED	0x07
#define QCOM_SCM_PIL_PAS_MSS_RESET	0x0a

#define QCOM_SCM_SVC_IO			0x05
#define QCOM_SCM_IO_READ		0x01
#define QCOM_SCM_IO_WRITE		0x02

#define QCOM_SCM_SVC_INFO		0x06
#define QCOM_SCM_INFO_IS_CALL_AVAIL	0x01

#define QCOM_SCM_SVC_MP				0x0c
#define QCOM_SCM_MP_RESTORE_SEC_CFG		0x02
#define QCOM_SCM_MP_IOMMU_SECURE_PTBL_SIZE	0x03
#define QCOM_SCM_MP_IOMMU_SECURE_PTBL_INIT	0x04
#define QCOM_SCM_MP_IOMMU_SET_CP_POOL_SIZE	0x05
#define QCOM_SCM_MP_VIDEO_VAR			0x08
#define QCOM_SCM_MP_ASSIGN			0x16

#define QCOM_SCM_SVC_OCMEM		0x0f
#define QCOM_SCM_OCMEM_LOCK_CMD		0x01
#define QCOM_SCM_OCMEM_UNLOCK_CMD	0x02

#define QCOM_SCM_SVC_ES			0x10    /* Enterprise Security */
#define QCOM_SCM_ES_INVALIDATE_ICE_KEY	0x03
#define QCOM_SCM_ES_CONFIG_SET_ICE_KEY	0x04

#define QCOM_SCM_SVC_HDCP		0x11
#define QCOM_SCM_HDCP_INVOKE		0x01

#define QCOM_SCM_SVC_LMH			0x13
#define QCOM_SCM_LMH_LIMIT_PROFILE_CHANGE	0x01
#define QCOM_SCM_LMH_LIMIT_DCVSH		0x10

#define QCOM_SCM_SVC_SMMU_PROGRAM		0x15
#define QCOM_SCM_SMMU_PT_FORMAT			0x01
#define QCOM_SCM_SMMU_CONFIG_ERRATA1		0x03
#define QCOM_SCM_SMMU_CONFIG_ERRATA1_CLIENT_ALL	0x02

#define QCOM_SCM_SVC_WAITQ			0x24
#define QCOM_SCM_WAITQ_RESUME			0x02
#define QCOM_SCM_WAITQ_GET_WQ_CTX		0x03

/**
 * struct qcom_scm_desc
 * @arginfo:    Metadata describing the arguments in args[]
 * @args:       The array of arguments for the secure syscall
 */
struct qcom_scm_desc {
	u32 svc;
	u32 cmd;
	u32 arginfo;
	u64 args[MAX_QCOM_SCM_ARGS];
	u32 owner;
};

/**
 * struct qcom_scm_res
 * @result:     The values returned by the secure syscall
 */
struct qcom_scm_res {
	u64 result[MAX_QCOM_SCM_RETS];
};

struct qcom_scm_hdcp_req {
	u32 addr;
	u32 val;
};

struct qcom_scm_vmperm {
	int vmid;
	int perm;
};

enum qcom_scm_ocmem_client {
	QCOM_SCM_OCMEM_UNUSED_ID = 0x0,
	QCOM_SCM_OCMEM_GRAPHICS_ID,
	QCOM_SCM_OCMEM_VIDEO_ID,
	QCOM_SCM_OCMEM_LP_AUDIO_ID,
	QCOM_SCM_OCMEM_SENSORS_ID,
	QCOM_SCM_OCMEM_OTHER_OS_ID,
	QCOM_SCM_OCMEM_DEBUG_ID,
};

enum qcom_scm_sec_dev_id {
	QCOM_SCM_MDSS_DEV_ID    = 1,
	QCOM_SCM_OCMEM_DEV_ID   = 5,
	QCOM_SCM_PCIE0_DEV_ID   = 11,
	QCOM_SCM_PCIE1_DEV_ID   = 12,
	QCOM_SCM_GFX_DEV_ID     = 18,
	QCOM_SCM_UFS_DEV_ID     = 19,
	QCOM_SCM_ICE_DEV_ID     = 20,
};

enum qcom_scm_ice_cipher {
	QCOM_SCM_ICE_CIPHER_AES_128_XTS = 0,
	QCOM_SCM_ICE_CIPHER_AES_128_CBC = 1,
	QCOM_SCM_ICE_CIPHER_AES_256_XTS = 3,
	QCOM_SCM_ICE_CIPHER_AES_256_CBC = 4,
};

#define QCOM_SCM_VMID_HLOS       0x3
#define QCOM_SCM_VMID_MSS_MSA    0xF
#define QCOM_SCM_VMID_WLAN       0x18
#define QCOM_SCM_VMID_WLAN_CE    0x19
#define QCOM_SCM_PERM_READ       0x4
#define QCOM_SCM_PERM_WRITE      0x2
#define QCOM_SCM_PERM_EXEC       0x1
#define QCOM_SCM_PERM_RW (QCOM_SCM_PERM_READ | QCOM_SCM_PERM_WRITE)
#define QCOM_SCM_PERM_RWX (QCOM_SCM_PERM_RW | QCOM_SCM_PERM_EXEC)

extern int qcom_scm_call(struct device *dev,
			 const struct qcom_scm_desc *desc,
			 struct qcom_scm_res *res);

extern int qcom_scm_call_atomic(struct device *dev,
				const struct qcom_scm_desc *desc,
				struct qcom_scm_res *res);

extern bool qcom_scm_is_available(void);

extern int qcom_scm_set_cold_boot_addr(void *entry);
extern int qcom_scm_set_warm_boot_addr(void *entry);
extern void qcom_scm_cpu_power_down(u32 flags);
extern int qcom_scm_set_remote_state(u32 state, u32 id);

struct qcom_scm_pas_metadata {
	void *ptr;
	dma_addr_t phys;
	ssize_t size;
};

extern int qcom_scm_pas_init_image(u32 peripheral, const void *metadata,
				   size_t size,
				   struct qcom_scm_pas_metadata *ctx);
void qcom_scm_pas_metadata_release(struct qcom_scm_pas_metadata *ctx);
extern int qcom_scm_pas_mem_setup(u32 peripheral, phys_addr_t addr,
				  phys_addr_t size);
extern int qcom_scm_pas_auth_and_reset(u32 peripheral);
extern int qcom_scm_pas_shutdown(u32 peripheral);
extern bool qcom_scm_pas_supported(u32 peripheral);

extern int qcom_scm_io_readl(phys_addr_t addr, unsigned int *val);
extern int qcom_scm_io_writel(phys_addr_t addr, unsigned int val);

extern bool qcom_scm_restore_sec_cfg_available(void);
extern int qcom_scm_restore_sec_cfg(u32 device_id, u32 spare);
extern int qcom_scm_iommu_secure_ptbl_size(u32 spare, size_t *size);
extern int qcom_scm_iommu_secure_ptbl_init(u64 addr, u32 size, u32 spare);
extern int qcom_scm_iommu_set_cp_pool_size(u32 spare, u32 size);
extern int qcom_scm_mem_protect_video_var(u32 cp_start, u32 cp_size,
					  u32 cp_nonpixel_start,
					  u32 cp_nonpixel_size);
extern int qcom_scm_assign_mem(phys_addr_t mem_addr, size_t mem_sz,
			       u64 *src,
			       const struct qcom_scm_vmperm *newvm,
			       unsigned int dest_cnt);

extern bool qcom_scm_ocmem_lock_available(void);
extern int qcom_scm_ocmem_lock(enum qcom_scm_ocmem_client id, u32 offset,
			       u32 size, u32 mode);
extern int qcom_scm_ocmem_unlock(enum qcom_scm_ocmem_client id, u32 offset,
				 u32 size);

extern bool qcom_scm_ice_available(void);
extern int qcom_scm_ice_invalidate_key(u32 index);
extern int qcom_scm_ice_set_key(u32 index, const u8 *key, u32 key_size,
				enum qcom_scm_ice_cipher cipher,
				u32 data_unit_size);

extern bool qcom_scm_hdcp_available(void);
extern int qcom_scm_hdcp_req(struct qcom_scm_hdcp_req *req, u32 req_cnt,
			     u32 *resp);

extern int qcom_scm_iommu_set_pt_format(u32 sec_id, u32 ctx_num, u32 pt_fmt);
extern int qcom_scm_qsmmu500_wait_safe_toggle(bool en);

extern int qcom_scm_lmh_dcvsh(u32 payload_fn, u32 payload_reg, u32 payload_val,
			      u64 limit_node, u32 node_id, u64 version);
extern int qcom_scm_lmh_profile_change(u32 profile_id);
extern bool qcom_scm_lmh_dcvsh_available(void);
#endif