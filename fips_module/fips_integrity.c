// SPDX-License-Identifier: GPL-2.0-only
/*
 * FIPS Cryptographic Module - Self-Integrity Check
 *
 * Reads the module's own .ko file and verifies its HMAC-SHA-256 against
 * the value embedded in the .fips_hmac ELF section at build time.
 *
 * This satisfies the FIPS 140-3 requirement that a cryptographic module
 * must verify its own integrity as part of its power-on self-tests, before
 * any approved security functions are made available.
 *
 * Build-time workflow:
 *   The post-build script scripts/fips_hmac_patch.py zeroes the .fips_hmac
 *   section, computes HMAC-SHA-256 of the entire .ko with a fixed key, and
 *   patches the result back into that section.  This must be run after every
 *   build and before the module is installed.  If the module is stripped
 *   before installation, stripping must occur before the HMAC is patched.
 *
 * Runtime workflow:
 *   fips_self_integrity_check() reads the .ko file named by the "path="
 *   module parameter, locates the .fips_hmac ELF section by name, saves and
 *   zeroes those bytes (reproducing the pre-patch image), recomputes
 *   HMAC-SHA-256 with the same key, and compares against the saved value.
 *   Any mismatch (or failure to locate the file / section) is fatal.
 */

#include <linux/elf.h>
#include <linux/kernel_read_file.h>
#include <linux/module.h>
#include <linux/overflow.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include "lib/sha256_lib.h"

/* Module parameter: absolute path to this module's .ko file */
static char fips_module_path[256];
module_param_string(path, fips_module_path, sizeof(fips_module_path), 0400);
MODULE_PARM_DESC(path, "Absolute path to fips_module.ko (required for integrity check)");

/*
 * Expected HMAC-SHA-256 of the module binary.  Zeroed in source; the
 * post-build script scripts/fips_hmac_patch.py computes the HMAC of the
 * .ko (with this field zeroed) using fips_integrity_key below, then patches
 * the result back into this section before the module is installed.
 *
 * Placing the value in its own ELF section (".fips_hmac") lets both the
 * runtime code and the build script locate it by section name, with no
 * dependency on symbol table entries.
 */
static const u8 __fips_module_hmac[SHA256_DIGEST_SIZE]
	__used __section(".fips_hmac") = { 0 };

/*
 * Fixed HMAC key embedded in the cryptographic boundary.
 *
 * The key is not secret — its purpose is integrity detection (corruption
 * or tampering), not confidentiality.  Embedding it in source means any
 * modification to the key itself also alters the file being measured, so
 * the check is self-referential with respect to this value.
 *
 * This constant must match FIPS_KEY in scripts/fips_hmac_patch.py exactly.
 */
static const u8 fips_integrity_key[] = "fips_module integrity v1";
#define FIPS_INTEGRITY_KEY_LEN (sizeof(fips_integrity_key) - 1)

/*
 * fips_find_hmac_section - find the ".fips_hmac" section in an ELF64 image.
 *
 * Parses the section header table in the buffer [@buf, @size) and writes
 * the file offset of the ".fips_hmac" section to @offset_out.
 *
 * Performs thorough bounds checks on all section header fields to handle
 * a truncated or malformed .ko gracefully.
 *
 * Returns 0 on success, -EINVAL for a malformed ELF, -ENODATA if the
 * section is absent.
 */
static int fips_find_hmac_section(const void *buf, size_t size,
				  loff_t *offset_out)
{
	const Elf64_Ehdr *ehdr = buf;
	const Elf64_Shdr *shdrs;
	const Elf64_Shdr *shstr_shdr;
	const char *shstrtab;
	u64 shdr_table_end;
	u64 shstr_end;
	unsigned int i;

	if (size < sizeof(*ehdr)) {
		pr_err("fips_module: ELF too small: %zu bytes\n", size);
		return -EINVAL;
	}
	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
		/*
		 * Detect common compression formats so the user gets an
		 * actionable message instead of a cryptic "bad magic" error.
		 * kbuild compresses .ko files in-place when
		 * CONFIG_MODULE_COMPRESS_GZIP or CONFIG_MODULE_COMPRESS_XZ is
		 * set; scripts/fips_hmac_patch.py must be run to decompress
		 * the file and embed the HMAC before loading.
		 */
		if (size >= 6 &&
		    ((u8 *)buf)[0] == 0xfd && ((u8 *)buf)[1] == '7' &&
		    ((u8 *)buf)[2] == 'z'  && ((u8 *)buf)[3] == 'X' &&
		    ((u8 *)buf)[4] == 'Z'  && ((u8 *)buf)[5] == 0x00)
			pr_err("fips_module: '%s' is xz-compressed; run "
			       "scripts/fips_hmac_patch.py to decompress "
			       "and patch before loading\n",
			       fips_module_path);
		else if (size >= 2 &&
			 ((u8 *)buf)[0] == 0x1f && ((u8 *)buf)[1] == 0x8b)
			pr_err("fips_module: '%s' is gzip-compressed; run "
			       "scripts/fips_hmac_patch.py to decompress "
			       "and patch before loading\n",
			       fips_module_path);
		else
			pr_err("fips_module: '%s' is not an ELF file "
			       "(bad magic %02x %02x %02x %02x)\n",
			       fips_module_path,
			       ((u8 *)buf)[0], ((u8 *)buf)[1],
			       size > 2 ? ((u8 *)buf)[2] : 0,
			       size > 3 ? ((u8 *)buf)[3] : 0);
		return -EINVAL;
	}
	if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
		pr_err("fips_module: not ELF64 (class=%u)\n",
		       ehdr->e_ident[EI_CLASS]);
		return -EINVAL;
	}
	if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0) {
		pr_err("fips_module: no section headers (shoff=%llu shnum=%u)\n",
		       (unsigned long long)ehdr->e_shoff, ehdr->e_shnum);
		return -EINVAL;
	}
	/* e_shstrndx == SHN_UNDEF (0) means no string table */
	if (ehdr->e_shstrndx == SHN_UNDEF || ehdr->e_shstrndx >= ehdr->e_shnum) {
		pr_err("fips_module: bad e_shstrndx=%u (e_shnum=%u)\n",
		       ehdr->e_shstrndx, ehdr->e_shnum);
		return -EINVAL;
	}
	/*
	 * e_shentsize must match the struct we use for indexing.  If an
	 * adversarially crafted ELF claimed a different entry size, shdrs[i]
	 * pointer arithmetic would land at wrong offsets.
	 */
	if (ehdr->e_shentsize != sizeof(Elf64_Shdr)) {
		pr_err("fips_module: e_shentsize=%u != sizeof(Elf64_Shdr)=%zu\n",
		       ehdr->e_shentsize, sizeof(Elf64_Shdr));
		return -EINVAL;
	}

	/*
	 * Bounds-check the section header table.
	 *
	 * e_shnum is Elf64_Half (u16, max 65535); sizeof(Elf64_Shdr) is 64;
	 * their product fits in u64 without overflow.  The addition with
	 * e_shoff (u64) is guarded by check_add_overflow() to prevent wrap
	 * when e_shoff is near UINT64_MAX.
	 */
	if (check_add_overflow(ehdr->e_shoff,
			       (u64)ehdr->e_shnum * sizeof(Elf64_Shdr),
			       &shdr_table_end) ||
	    shdr_table_end > size) {
		pr_err("fips_module: section header table out of bounds "
		       "(shoff=%llu shnum=%u shentsize=%u filesize=%zu)\n",
		       (unsigned long long)ehdr->e_shoff,
		       ehdr->e_shnum, ehdr->e_shentsize, size);
		return -EINVAL;
	}

	shdrs      = buf + ehdr->e_shoff;
	shstr_shdr = &shdrs[ehdr->e_shstrndx];

	/*
	 * Bounds-check the section name string table.
	 * sh_offset and sh_size are both u64; guard against wrap.
	 */
	if (shstr_shdr->sh_size == 0 ||
	    check_add_overflow(shstr_shdr->sh_offset, shstr_shdr->sh_size,
			       &shstr_end) ||
	    shstr_end > size) {
		pr_err("fips_module: shstrtab out of bounds "
		       "(shstr_off=%llu shstr_size=%llu filesize=%zu)\n",
		       (unsigned long long)shstr_shdr->sh_offset,
		       (unsigned long long)shstr_shdr->sh_size, size);
		return -EINVAL;
	}

	shstrtab = buf + shstr_shdr->sh_offset;

	for (i = 0; i < ehdr->e_shnum; i++) {
		u32 name_off = shdrs[i].sh_name;
		u64 sh_off   = shdrs[i].sh_offset;
		u64 sh_size  = shdrs[i].sh_size;
		u64 sh_end;
		size_t remaining;

		if (name_off >= shstr_shdr->sh_size)
			continue;

		/* Verify name is nul-terminated within the string table */
		remaining = shstr_shdr->sh_size - name_off;
		if (!memchr(shstrtab + name_off, '\0', remaining))
			continue;

		if (strcmp(shstrtab + name_off, ".fips_hmac") != 0)
			continue;

		/*
		 * Found it.  Validate size and file extent.
		 * sh_off + sh_size are both u64; guard against wrap.
		 */
		if (sh_size < SHA256_DIGEST_SIZE)
			return -EINVAL;
		if (check_add_overflow(sh_off, sh_size, &sh_end) ||
		    sh_end > size)
			return -EINVAL;

		*offset_out = (loff_t)sh_off;
		return 0;
	}

	return -ENODATA;
}

/*
 * fips_self_integrity_check - verify the module's HMAC-SHA-256 integrity.
 *
 * Reads the .ko file named by the "path=" module parameter, locates the
 * .fips_hmac ELF section, saves and zeroes the embedded HMAC placeholder,
 * recomputes HMAC-SHA-256 over the resulting image, and compares against
 * the saved value.
 *
 * Must be called after fips_sha256_bootstrap_selftest() has confirmed
 * that the HMAC-SHA-256 implementation is correct, and before any
 * cryptographic algorithms are registered.
 *
 * Returns 0 on success, negative errno on any failure.
 */
int fips_self_integrity_check(void)
{
	void *buf = NULL;
	size_t file_size = 0;
	loff_t hmac_offset;
	u8 stored_hmac[SHA256_DIGEST_SIZE];
	u8 computed_hmac[SHA256_DIGEST_SIZE];
	ssize_t nread;
	int ret;

	if (!fips_module_path[0]) {
		pr_err("fips_module: path= module parameter is required "
		       "for the integrity check\n");
		return -EINVAL;
	}

	/*
	 * Read the entire .ko image into a vmalloc buffer.
	 * Passing buf=NULL causes the function to allocate the buffer;
	 * buf_size is ignored in that case.  Free with vfree() on success.
	 */
	nread = kernel_read_file_from_path(fips_module_path, 0, &buf, 0,
					   &file_size, READING_UNKNOWN);
	if (nread < 0) {
		pr_err("fips_module: failed to read '%s': %ld\n",
		       fips_module_path, nread);
		return (int)nread;
	}

	/* Locate the .fips_hmac section within the file image */
	ret = fips_find_hmac_section(buf, file_size, &hmac_offset);
	if (ret) {
		pr_err("fips_module: .fips_hmac section not found "
		       "in '%s': %d\n", fips_module_path, ret);
		goto out;
	}

	/*
	 * Save the embedded expected HMAC, then zero the placeholder so
	 * we compute over the same image the build script measured — the
	 * script also zeroed this field before computing the reference HMAC.
	 */
	memcpy(stored_hmac, buf + hmac_offset, SHA256_DIGEST_SIZE);
	memset(buf + hmac_offset, 0, SHA256_DIGEST_SIZE);

	/* Compute HMAC-SHA-256 of the zeroed file image */
	fips_lib_hmac_sha256_usingrawkey(fips_integrity_key,
					 FIPS_INTEGRITY_KEY_LEN,
					 buf, file_size,
					 computed_hmac);

	if (memcmp(computed_hmac, stored_hmac, SHA256_DIGEST_SIZE) != 0) {
		pr_err("fips_module: integrity check FAILED for '%s'\n",
		       fips_module_path);
		ret = -EBADMSG;
	} else {
		pr_info("fips_module: integrity check passed\n");
		ret = 0;
	}

	memzero_explicit(computed_hmac, sizeof(computed_hmac));
	memzero_explicit(stored_hmac, sizeof(stored_hmac));
out:
	vfree(buf);
	return ret;
}
