# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Copyright (C) ST-Ericsson SA 2010
# * Author: shujuan.chen@stericsson.com for ST-Ericsson.
 */

ifdef CONFIG_CRYPTO_DEV_UX500_DEBUG
CFLAGS_cryp_core.o := -DDEBUG
CFLAGS_cryp.o := -DDEBUG
CFLAGS_cryp_irq.o := -DDEBUG
endif

obj-$(CONFIG_CRYPTO_DEV_UX500_CRYP) += ux500_cryp.o
ux500_cryp-objs :=  cryp.o cryp_irq.o cryp_core.o
