/*
 * IOMMU API for ARM architected SMMU implementations.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) 2013 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#ifndef _ARM_SMMU_H
#define _ARM_SMMU_H

/* Configuration registers */
#define ARM_SMMU_GR0_sCR0		0x0
#define sCR0_CLIENTPD			(1 << 0)
#define sCR0_GFRE			(1 << 1)
#define sCR0_GFIE			(1 << 2)
#define sCR0_EXIDENABLE			(1 << 3)
#define sCR0_GCFGFRE			(1 << 4)
#define sCR0_GCFGFIE			(1 << 5)
#define sCR0_USFCFG			(1 << 10)
#define sCR0_VMIDPNE			(1 << 11)
#define sCR0_PTM			(1 << 12)
#define sCR0_FB				(1 << 13)
#define sCR0_VMID16EN			(1 << 31)
#define sCR0_BSU_SHIFT			14
#define sCR0_BSU_MASK			0x3

/* Auxiliary Configuration register */
#define ARM_SMMU_GR0_sACR		0x10

/* Identification registers */
#define ARM_SMMU_GR0_ID0		0x20
#define ARM_SMMU_GR0_ID1		0x24
#define ARM_SMMU_GR0_ID2		0x28
#define ARM_SMMU_GR0_ID3		0x2c
#define ARM_SMMU_GR0_ID4		0x30
#define ARM_SMMU_GR0_ID5		0x34
#define ARM_SMMU_GR0_ID6		0x38
#define ARM_SMMU_GR0_ID7		0x3c
#define ARM_SMMU_GR0_sGFSR		0x48
#define ARM_SMMU_GR0_sGFSYNR0		0x50
#define ARM_SMMU_GR0_sGFSYNR1		0x54
#define ARM_SMMU_GR0_sGFSYNR2		0x58

#define ID0_S1TS			(1 << 30)
#define ID0_S2TS			(1 << 29)
#define ID0_NTS				(1 << 28)
#define ID0_SMS				(1 << 27)
#define ID0_ATOSNS			(1 << 26)
#define ID0_PTFS_NO_AARCH32		(1 << 25)
#define ID0_PTFS_NO_AARCH32S		(1 << 24)
#define ID0_CTTW			(1 << 14)
#define ID0_NUMIRPT_SHIFT		16
#define ID0_NUMIRPT_MASK		0xff
#define ID0_NUMSIDB_SHIFT		9
#define ID0_NUMSIDB_MASK		0xf
#define ID0_EXIDS			(1 << 8)
#define ID0_NUMSMRG_SHIFT		0
#define ID0_NUMSMRG_MASK		0xff

#define ID1_PAGESIZE			(1 << 31)
#define ID1_NUMPAGENDXB_SHIFT		28
#define ID1_NUMPAGENDXB_MASK		7
#define ID1_NUMS2CB_SHIFT		16
#define ID1_NUMS2CB_MASK		0xff
#define ID1_NUMCB_SHIFT			0
#define ID1_NUMCB_MASK			0xff

#define ID2_OAS_SHIFT			4
#define ID2_OAS_MASK			0xf
#define ID2_IAS_SHIFT			0
#define ID2_IAS_MASK			0xf
#define ID2_UBS_SHIFT			8
#define ID2_UBS_MASK			0xf
#define ID2_PTFS_4K			(1 << 12)
#define ID2_PTFS_16K			(1 << 13)
#define ID2_PTFS_64K			(1 << 14)
#define ID2_VMID16			(1 << 15)

#define ID7_MAJOR_SHIFT			4
#define ID7_MAJOR_MASK			0xf

/* Global TLB invalidation */
#define ARM_SMMU_GR0_TLBIVMID		0x64
#define ARM_SMMU_GR0_TLBIALLNSNH	0x68
#define ARM_SMMU_GR0_TLBIALLH		0x6c
#define ARM_SMMU_GR0_sTLBGSYNC		0x70
#define ARM_SMMU_GR0_sTLBGSTATUS	0x74
#define sTLBGSTATUS_GSACTIVE		(1 << 0)

/* Stream mapping registers */
#define ARM_SMMU_GR0_SMR(n)		(0x800 + ((n) << 2))
#define SMR_VALID			(1 << 31)
#define SMR_MASK_SHIFT			16
#define SMR_ID_SHIFT			0

#define ARM_SMMU_GR0_S2CR(n)		(0xc00 + ((n) << 2))
#define S2CR_CBNDX_SHIFT		0
#define S2CR_CBNDX_MASK			0xff
#define S2CR_EXIDVALID			(1 << 10)
#define S2CR_TYPE_SHIFT			16
#define S2CR_TYPE_MASK			0x3
enum arm_smmu_s2cr_type {
	S2CR_TYPE_TRANS,
	S2CR_TYPE_BYPASS,
	S2CR_TYPE_FAULT,
};

#define S2CR_PRIVCFG_SHIFT		24
#define S2CR_PRIVCFG_MASK		0x3
enum arm_smmu_s2cr_privcfg {
	S2CR_PRIVCFG_DEFAULT,
	S2CR_PRIVCFG_DIPAN,
	S2CR_PRIVCFG_UNPRIV,
	S2CR_PRIVCFG_PRIV,
};

/* Context bank attribute registers */
#define ARM_SMMU_GR1_CBAR(n)		(0x0 + ((n) << 2))
#define CBAR_IRPTNDX			GENMASK(31, 24)
#define CBAR_TYPE			GENMASK(17, 16)
enum arm_smmu_cbar_type {
	CBAR_TYPE_S2_TRANS,
	CBAR_TYPE_S1_TRANS_S2_BYPASS,
	CBAR_TYPE_S1_TRANS_S2_FAULT,
	CBAR_TYPE_S1_TRANS_S2_TRANS,
};
#define CBAR_S1_MEMATTR			GENMASK(15, 12)
#define CBAR_S1_MEMATTR_WB		0xf
#define CBAR_S1_BPSHCFG			GENMASK(9, 8)
#define CBAR_S1_BPSHCFG_NSH		3
#define CBAR_VMID			GENMASK(7, 0)

#define ARM_SMMU_GR1_CBFRSYNRA(n)	(0x400 + ((n) << 2))

#define ARM_SMMU_GR1_CBA2R(n)		(0x800 + ((n) << 2))
#define CBA2R_VMID16			GENMASK(31, 16)
#define CBA2R_VA64			BIT(0)

#define ARM_SMMU_CB_SCTLR		0x0
#define SCTLR_S1_ASIDPNE		BIT(12)
#define SCTLR_CFCFG			BIT(7)
#define SCTLR_CFIE			BIT(6)
#define SCTLR_CFRE			BIT(5)
#define SCTLR_E				BIT(4)
#define SCTLR_AFE			BIT(2)
#define SCTLR_TRE			BIT(1)
#define SCTLR_M				BIT(0)

#define ARM_SMMU_CB_ACTLR		0x4

#define ARM_SMMU_CB_RESUME		0x8
#define RESUME_TERMINATE		BIT(0)

#define ARM_SMMU_CB_TCR2		0x10
#define TCR2_SEP			GENMASK(17, 15)
#define TCR2_SEP_UPSTREAM		0x7
#define TCR2_AS				BIT(4)

#define ARM_SMMU_CB_TTBR0		0x20
#define ARM_SMMU_CB_TTBR1		0x28
#define TTBRn_ASID			GENMASK_ULL(63, 48)

#define ARM_SMMU_CB_TCR			0x30
#define ARM_SMMU_CB_CONTEXTIDR		0x34
#define ARM_SMMU_CB_S1_MAIR0		0x38
#define ARM_SMMU_CB_S1_MAIR1		0x3c

#define ARM_SMMU_CB_PAR			0x50
#define CB_PAR_F			BIT(0)

#define ARM_SMMU_CB_FSR			0x58
#define FSR_MULTI			BIT(31)
#define FSR_SS				BIT(30)
#define FSR_UUT				BIT(8)
#define FSR_ASF				BIT(7)
#define FSR_TLBLKF			BIT(6)
#define FSR_TLBMCF			BIT(5)
#define FSR_EF				BIT(4)
#define FSR_PF				BIT(3)
#define FSR_AFF				BIT(2)
#define FSR_TF				BIT(1)

#define FSR_IGN				(FSR_AFF | FSR_ASF | \
					 FSR_TLBMCF | FSR_TLBLKF)
#define FSR_FAULT			(FSR_MULTI | FSR_SS | FSR_UUT | \
					 FSR_EF | FSR_PF | FSR_TF | FSR_IGN)

#define ARM_SMMU_CB_FAR			0x60

#define ARM_SMMU_CB_FSYNR0		0x68
#define FSYNR0_WNR			BIT(4)

#define ARM_SMMU_CB_S1_TLBIVA		0x600
#define ARM_SMMU_CB_S1_TLBIASID		0x610
#define ARM_SMMU_CB_S1_TLBIVAL		0x620
#define ARM_SMMU_CB_S2_TLBIIPAS2	0x630
#define ARM_SMMU_CB_S2_TLBIIPAS2L	0x638
#define ARM_SMMU_CB_TLBSYNC		0x7f0
#define ARM_SMMU_CB_TLBSTATUS		0x7f4
#define ARM_SMMU_CB_ATS1PR		0x800

#define ARM_SMMU_CB_ATSR		0x8f0
#define ATSR_ACTIVE			BIT(0)

#endif /* _ARM_SMMU_H */
