#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""
fips_hmac_patch.py -- embed HMAC-SHA-256 integrity value in fips_module.ko

Usage:
    python3 fips_hmac_patch.py <path-to-fips_module.ko>

This script computes the FIPS module self-integrity HMAC and patches it into
the .ko binary.  It must be run after every build and before the module is
installed.  The runtime check in fips_integrity.c reproduces this computation
to verify the module has not been corrupted or tampered with.

Algorithm:
  1. Parse the ELF64 section header table to find the .fips_hmac section.
  2. Zero the 32 bytes at the section's file offset (they hold the previous
     HMAC or zeros on a fresh build).
  3. Compute HMAC-SHA-256 of the entire .ko image (with those bytes zeroed),
     using the fixed key below.
  4. Patch the computed HMAC back into the .fips_hmac section.
  5. Write the modified binary back to the same file.

The key must match fips_integrity_key[] in fips_integrity.c exactly.

NOTE: If the module is stripped before installation, stripping must happen
before running this script.  Stripping after patching invalidates the HMAC.
"""

import sys
import struct
import hmac
import hashlib

# Must match fips_integrity_key[] in fips_integrity.c
FIPS_KEY = b'fips_module integrity v1'

FIPS_SECTION = b'.fips_hmac'
HMAC_SIZE = 32          # SHA256_DIGEST_SIZE
ELFMAG = b'\x7fELF'
ELFCLASS64 = 2
EI_CLASS = 4


def find_hmac_section(data: bytearray) -> tuple:
    """
    Parse the ELF64 section header table and return (file_offset, sh_size)
    of the .fips_hmac section.

    Raises ValueError on a malformed ELF or if the section is absent.
    """
    if len(data) < 64:
        raise ValueError("File too small to be a valid ELF")
    if data[:4] != ELFMAG:
        raise ValueError("Not a valid ELF file (bad magic)")
    if data[EI_CLASS] != ELFCLASS64:
        raise ValueError(
            f"Expected ELF64 (class {ELFCLASS64}), got class {data[EI_CLASS]}")

    # ELF64 header fields we need:
    #   e_shoff     @ offset 40  (uint64)
    #   e_shentsize @ offset 58  (uint16)
    #   e_shnum     @ offset 60  (uint16)
    #   e_shstrndx  @ offset 62  (uint16)
    (e_shoff,) = struct.unpack_from('<Q', data, 40)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', data, 58)

    if e_shoff == 0 or e_shnum == 0:
        raise ValueError("ELF has no section headers")
    if e_shstrndx == 0 or e_shstrndx >= e_shnum:
        raise ValueError(
            f"Invalid e_shstrndx={e_shstrndx} (e_shnum={e_shnum})")

    # Bounds-check the section header table
    shdr_table_end = e_shoff + e_shnum * e_shentsize
    if shdr_table_end > len(data):
        raise ValueError("Section header table extends past end of file")

    def read_shdr(i):
        """Return (sh_name, sh_offset, sh_size) for section i."""
        base = e_shoff + i * e_shentsize
        (sh_name,) = struct.unpack_from('<I', data, base)
        # sh_offset is at base+24, sh_size at base+32 (both uint64)
        sh_offset, sh_size = struct.unpack_from('<QQ', data, base + 24)
        return sh_name, sh_offset, sh_size

    # Read the section name string table
    _, shstr_offset, shstr_size = read_shdr(e_shstrndx)
    if shstr_offset + shstr_size > len(data):
        raise ValueError("Section name string table extends past end of file")
    shstrtab = data[shstr_offset: shstr_offset + shstr_size]

    for i in range(e_shnum):
        sh_name, sh_offset, sh_size = read_shdr(i)
        if sh_name >= len(shstrtab):
            continue
        # Extract the nul-terminated name
        nul = shstrtab.find(b'\x00', sh_name)
        if nul < 0:
            continue
        name = shstrtab[sh_name:nul]
        if name != FIPS_SECTION:
            continue

        # Found it — validate
        if sh_size < HMAC_SIZE:
            raise ValueError(
                f".fips_hmac section is {sh_size} bytes, need >= {HMAC_SIZE}")
        if sh_offset + sh_size > len(data):
            raise ValueError(
                ".fips_hmac section extends past end of file")

        return sh_offset, sh_size

    raise ValueError(f"Section {FIPS_SECTION.decode()!r} not found in ELF")


def compute_and_patch(ko_path: str) -> None:
    with open(ko_path, 'rb') as f:
        data = bytearray(f.read())

    offset, _ = find_hmac_section(data)

    # Record what was there (all zeros on a fresh build, old HMAC on rebuild)
    old_hmac = bytes(data[offset: offset + HMAC_SIZE])

    # Zero the placeholder — this is what the runtime check does before
    # recomputing, so we must use the same zeroed image here.
    data[offset: offset + HMAC_SIZE] = b'\x00' * HMAC_SIZE

    # Compute HMAC-SHA-256 of the zeroed image
    h = hmac.new(FIPS_KEY, bytes(data), hashlib.sha256)
    computed = h.digest()

    # Patch the HMAC back in
    data[offset: offset + HMAC_SIZE] = computed

    with open(ko_path, 'wb') as f:
        f.write(data)

    print(f"fips_hmac_patch: patched {ko_path}")
    if old_hmac != b'\x00' * HMAC_SIZE:
        print(f"  previous: {old_hmac.hex()}")
    print(f"  embedded: {computed.hex()}")


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} fips_module.ko", file=sys.stderr)
        sys.exit(1)

    try:
        compute_and_patch(sys.argv[1])
    except (ValueError, OSError) as e:
        print(f"fips_hmac_patch: ERROR: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
