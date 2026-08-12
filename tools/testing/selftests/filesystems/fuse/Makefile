# SPDX-License-Identifier: GPL-2.0-or-later

CFLAGS += -Wall -O2 -g $(KHDR_INCLUDES)

TEST_GEN_PROGS := fusectl_test
TEST_GEN_FILES := fuse_mnt

# fuse_uring_test requires liburing; add it only when the library is present.
URING_CFLAGS := $(shell pkg-config liburing --cflags 2>/dev/null)
URING_LDLIBS := $(shell pkg-config liburing --libs 2>/dev/null)
ifeq ($(URING_LDLIBS),)
$(warning liburing not found; fuse_uring_test will be skipped)
else
TEST_GEN_PROGS += fuse_uring_test
endif

# fuse_acl_cache_test requires libfuse3; add it only when the library is present.
ACL_CFLAGS := $(shell pkg-config fuse3 --cflags 2>/dev/null)
ACL_LDLIBS := $(shell pkg-config fuse3 --libs 2>/dev/null)
ifneq ($(ACL_CFLAGS),)
TEST_GEN_PROGS += fuse_acl_cache_test
endif

include ../../lib.mk

VAR_CFLAGS := $(shell pkg-config fuse --cflags 2>/dev/null)
ifeq ($(VAR_CFLAGS),)
# libfuse2-devel not found; try fuse3 which provides v2 compat headers
VAR_CFLAGS := $(shell pkg-config fuse3 --cflags 2>/dev/null)
ifeq ($(VAR_CFLAGS),)
VAR_CFLAGS := -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse
endif
endif

VAR_LDLIBS := $(shell pkg-config fuse --libs 2>/dev/null)
ifeq ($(VAR_LDLIBS),)
VAR_LDLIBS := $(shell pkg-config fuse3 --libs 2>/dev/null)
ifeq ($(VAR_LDLIBS),)
VAR_LDLIBS := -lfuse -pthread
endif
endif

$(OUTPUT)/fuse_mnt: CFLAGS += $(VAR_CFLAGS)
$(OUTPUT)/fuse_mnt: LDLIBS += $(VAR_LDLIBS)

$(OUTPUT)/fuse_acl_cache_test: CFLAGS += $(ACL_CFLAGS)
$(OUTPUT)/fuse_acl_cache_test: LDLIBS += $(ACL_LDLIBS)

$(OUTPUT)/fuse_uring_test: CFLAGS += $(URING_CFLAGS) -Wextra -Wno-unused-parameter -pthread
$(OUTPUT)/fuse_uring_test: LDLIBS += $(URING_LDLIBS) -lpthread
