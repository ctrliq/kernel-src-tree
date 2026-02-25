# SPDX-License-Identifier: GPL-2.0
# Makefile for the FIPS cryptographic module
#
# This module bundles FIPS-validated implementations of specific cryptographic
# algorithms into a single loadable kernel module.  Source files are organized
# to mirror the original kernel source layout (crypto/, arch/x86/crypto/,
# arch/arm64/crypto/, lib/crypto/mpi/).
#
# Build from the kernel source tree root:
#   make M=fips_module
#
# Or from within fips_module/:
#   make
#
# KDIR defaults to the parent directory (the kernel source tree root) when
# building from within fips_module/.

KDIR ?= $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST))))/..)

# Module name
MODULE_NAME := fips_module

# -----------------------------------------------------------------------
# Core object files (architecture-independent)
# -----------------------------------------------------------------------
fips_module-y :=					\
	fips_init.o					\
							\
	crypto/aes_generic.o				\
	crypto/cbc.o					\
	crypto/ctr.o					\
	crypto/ecb.o					\
	crypto/gcm.o					\
	crypto/ghash-generic.o				\
	crypto/xts.o					\
							\
	crypto/sha1.o					\
	crypto/sha256.o					\
	crypto/sha512.o					\
	crypto/sha3_generic.o				\
							\
	crypto/authenc.o				\
	crypto/ccm.o					\
	crypto/hmac.o					\
	crypto/cmac.o					\
							\
	crypto/kdf_sp800108.o				\
							\
	crypto/drbg.o					\
	crypto/jitterentropy.o				\
	crypto/jitterentropy-kcapi.o			\
							\
	crypto/rsa.o					\
	crypto/rsa_helper.o				\
	crypto/rsa-pkcs1pad.o				\
	crypto/rsassa-pkcs1.o				\
	crypto/rsapubkey.asn1.o				\
	crypto/rsaprivkey.asn1.o			\
							\
	crypto/ecdsa.o					\
	crypto/ecdsa-x962.o				\
	crypto/ecdsa-p1363.o				\
	crypto/ecdsasignature.asn1.o			\
	crypto/ecc.o					\
	crypto/ecdh.o					\
	crypto/ecdh_helper.o				\
							\
	crypto/dh.o					\
	crypto/dh_helper.o				\
							\
	crypto/testmgr.o				\
							\
	lib/crypto/mpi/mpi-add.o			\
	lib/crypto/mpi/mpi-bit.o			\
	lib/crypto/mpi/mpi-cmp.o			\
	lib/crypto/mpi/mpicoder.o			\
	lib/crypto/mpi/mpi-div.o			\
	lib/crypto/mpi/mpih-cmp.o			\
	lib/crypto/mpi/mpih-div.o			\
	lib/crypto/mpi/mpih-mul.o			\
	lib/crypto/mpi/mpi-mod.o			\
	lib/crypto/mpi/mpi-mul.o			\
	lib/crypto/mpi/mpi-pow.o			\
	lib/crypto/mpi/mpi-sub-ui.o			\
	lib/crypto/mpi/mpiutil.o			\
	lib/crypto/mpi/generic_mpih-add1.o		\
	lib/crypto/mpi/generic_mpih-lshift.o		\
	lib/crypto/mpi/generic_mpih-mul1.o		\
	lib/crypto/mpi/generic_mpih-mul2.o		\
	lib/crypto/mpi/generic_mpih-mul3.o		\
	lib/crypto/mpi/generic_mpih-rshift.o		\
	lib/crypto/mpi/generic_mpih-sub1.o

# -----------------------------------------------------------------------
# x86_64 hardware-accelerated AES-NI and GHASH
# -----------------------------------------------------------------------
fips_module-$(CONFIG_X86_64) +=				\
	arch/x86/crypto/aesni-intel_asm.o		\
	arch/x86/crypto/aesni-intel_glue.o		\
	arch/x86/crypto/aes-ctr-avx-x86_64.o		\
	arch/x86/crypto/aes-gcm-aesni-x86_64.o		\
	arch/x86/crypto/aes-gcm-avx10-x86_64.o		\
	arch/x86/crypto/aes-xts-avx-x86_64.o		\
	arch/x86/crypto/ghash-clmulni-intel_asm.o	\
	arch/x86/crypto/ghash-clmulni-intel_glue.o

# -----------------------------------------------------------------------
# ARM64 hardware-accelerated AES-CE, GHASH-CE, SHA3-CE
# -----------------------------------------------------------------------
fips_module-$(CONFIG_ARM64) +=				\
	arch/arm64/crypto/aes-ce-core.o			\
	arch/arm64/crypto/aes-ce-glue.o			\
	arch/arm64/crypto/aes-modes.o			\
	arch/arm64/crypto/ghash-ce-core.o		\
	arch/arm64/crypto/ghash-ce-glue.o		\
	arch/arm64/crypto/sha3-ce-core.o		\
	arch/arm64/crypto/sha3-ce-glue.o

obj-m := fips_module.o

# -----------------------------------------------------------------------
# Global compiler flags for all fips_module object files
# -----------------------------------------------------------------------

# Automatically include the fips_alg.h prototype header into every
# translation unit.  This provides forward declarations for the fips_*
# init/exit functions and prevents -Wmissing-prototypes errors when those
# functions are defined in the individual source files.
ccflags-y += -include $(src)/fips_alg.h

# Include the fips_module source root so that relative paths inside the
# copied source files continue to resolve correctly.
ccflags-y += -I$(src)

# -----------------------------------------------------------------------
# Special compiler flags
# -----------------------------------------------------------------------

# jitterentropy MUST be compiled without optimisation — timing jitter is
# the entropy source.  Sanitisers are disabled for the same reason.
CFLAGS_crypto/jitterentropy.o		:= -O0
KASAN_SANITIZE_crypto/jitterentropy.o	:= n
UBSAN_SANITIZE_crypto/jitterentropy.o	:= n

# AES generic benefits from disabling code-hoisting optimisation which
# can create timing side-channels.
CFLAGS_crypto/aes_generic.o := $(call cc-option,-fno-code-hoisting)

# -----------------------------------------------------------------------
# ASN.1 generated sources
# -----------------------------------------------------------------------
# kbuild automatically generates %.asn1.c and %.asn1.h from %.asn1 source
# files when %.asn1.o appears in the object list (see scripts/Makefile.build).
#
# We must also declare header dependencies for any .c file that #includes
# a generated .asn1.h, so that make generates the header before compiling
# that file.

$(obj)/crypto/rsa_helper.o:  $(obj)/crypto/rsapubkey.asn1.h \
			     $(obj)/crypto/rsaprivkey.asn1.h
$(obj)/crypto/ecdsa-x962.o:  $(obj)/crypto/ecdsasignature.asn1.h

# -----------------------------------------------------------------------
# Build targets
# -----------------------------------------------------------------------

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
