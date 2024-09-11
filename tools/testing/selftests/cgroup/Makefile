# SPDX-License-Identifier: GPL-2.0
CFLAGS += -Wall

all:

TEST_FILES     := with_stress.sh
TEST_PROGS     := test_stress.sh
TEST_GEN_PROGS = test_memcontrol
TEST_GEN_PROGS += test_kmem

include ../lib.mk

$(OUTPUT)/test_memcontrol: cgroup_util.c
$(OUTPUT)/test_kmem: cgroup_util.c
