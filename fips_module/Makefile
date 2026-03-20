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
	fips_integrity.o				\
	fips_extrng.o					\
	fips_algtest.o					\
							\
	crypto/aes_generic.o				\
	crypto/cbc.o					\
	crypto/ctr.o					\
	crypto/ecb.o					\
	crypto/gcm.o					\
	crypto/ghash-generic.o				\
	crypto/xts.o					\
							\
	lib/crypto/aes.o				\
	lib/crypto/aescfb.o				\
	lib/crypto/aesgcm.o				\
	lib/crypto/sha1.o				\
	lib/crypto/sha256.o				\
	lib/crypto/sha512.o				\
							\
	crypto/sha1.o					\
	crypto/sha256.o					\
	crypto/sha512.o					\
	crypto/sha3_generic.o				\
							\
	crypto/aead.o					\
	crypto/authenc.o				\
	crypto/ccm.o					\
	crypto/essiv.o					\
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
	lib/crypto/x86/sha1-ssse3-and-avx.o		\
	lib/crypto/x86/sha1-avx2-asm.o			\
	lib/crypto/x86/sha1-ni-asm.o			\
	lib/crypto/x86/sha256-ssse3-asm.o		\
	lib/crypto/x86/sha256-avx-asm.o			\
	lib/crypto/x86/sha256-avx2-asm.o		\
	lib/crypto/x86/sha256-ni-asm.o			\
	lib/crypto/x86/sha512-ssse3-asm.o		\
	lib/crypto/x86/sha512-avx-asm.o			\
	lib/crypto/x86/sha512-avx2-asm.o		\
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
	lib/crypto/arm64/sha1-ce-core.o			\
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

# AES implementations benefit from disabling code-hoisting optimisation which
# can create timing side-channels.
CFLAGS_crypto/aes_generic.o := $(call cc-option,-fno-code-hoisting)
CFLAGS_lib/crypto/aes.o     := $(call cc-option,-fno-code-hoisting)

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
#
# After the kbuild step links fips_module.ko, the post-build script
# scripts/fips_hmac_patch.py must be run to embed the expected HMAC-SHA-256
# of the module image into its .fips_hmac ELF section.  The 'all' target
# does this automatically when building from within fips_module/.
#
# When building from the kernel root with 'make M=fips_module', kbuild
# does not invoke this Makefile's 'all' target.  Run the patch step
# manually afterwards:
#   python3 fips_module/scripts/fips_hmac_patch.py fips_module/fips_module.ko
# or use the 'hmac_patch' target:
#   make -C fips_module hmac_patch
#
# NOTE: If the module is stripped before installation, stripping must
# happen BEFORE the HMAC patch step, since stripping modifies the file.

PYTHON3 ?= python3
FIPS_HMAC_PATCH := $(PWD)/scripts/fips_hmac_patch.py

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(PYTHON3) $(FIPS_HMAC_PATCH) $(PWD)/fips_module.ko

hmac_patch:
	$(PYTHON3) $(FIPS_HMAC_PATCH) $(PWD)/fips_module.ko

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
