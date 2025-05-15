About
=====

The ``efivarfs`` is a file system allowing for displaying, modifying,
creating and removing the UEFI (Unified Extensible Firmware Interface)
variables.

https://wiki.archlinux.org/title/Unified_Extensible_Firmware_Interface

::

   UEFI defines variables through which an operating system can interact with the
   firmware. UEFI boot variables are used by the boot loader and used by the
   operating system only for early system start-up. UEFI runtime variables allow an
   operating system to manage certain settings of the firmware like the UEFI boot
   manager or managing the keys for UEFI Secure Boot protocol etc.

The collection defines a single test: ``efivarfs:efivarfs.sh``.

Requirements and applicability
==============================

The test requires ``CONFIG_EFIVAR_FS`` option to be enabled in the
Kernel. It's ``y`` in all Rocky versions:

.. code:: shell

   grep CONFIG_EFIVAR_FS kernel-src-tree-{ciqlts8_6,ciqlts8_8,ciqlts9_2,ciqlts9_4}/configs/*

::

   kernel-src-tree-ciqlts8_6/configs/kernel-aarch64-debug.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts8_6/configs/kernel-aarch64.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts8_6/configs/kernel-x86_64-debug.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts8_6/configs/kernel-x86_64.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts8_8/configs/kernel-aarch64-debug.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts8_8/configs/kernel-aarch64.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts8_8/configs/kernel-x86_64-debug.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts8_8/configs/kernel-x86_64.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_2/configs/kernel-aarch64-64k-debug-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_2/configs/kernel-aarch64-64k-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_2/configs/kernel-aarch64-debug-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_2/configs/kernel-aarch64-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_2/configs/kernel-x86_64-debug-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_2/configs/kernel-x86_64-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_4/configs/kernel-aarch64-64k-debug-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_4/configs/kernel-aarch64-64k-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_4/configs/kernel-aarch64-debug-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_4/configs/kernel-aarch64-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_4/configs/kernel-aarch64-rt-debug-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_4/configs/kernel-aarch64-rt-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_4/configs/kernel-x86_64-debug-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_4/configs/kernel-x86_64-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_4/configs/kernel-x86_64-rt-debug-rhel.config:CONFIG_EFIVAR_FS=y
   kernel-src-tree-ciqlts9_4/configs/kernel-x86_64-rt-rhel.config:CONFIG_EFIVAR_FS=y

However, because the base cloud images for the ``x86_64`` architecture
all use BIOS by default, the ``efivarfs`` can't really be tested on
these systems without some serious booting reconfiguration. The
``/sys/firmware/efi/efivars`` path where the ``efivarfs`` is typically
mounted (and where ``efivarfs:efivarfs.sh`` test *expects* it to be
mounted) is missing, and the test is skipped:

::

   # selftests: efivarfs: efivarfs.sh
   # skip all tests: efivarfs is not mounted on /sys/firmware/efi/efivars
   ok 1 selftests: efivarfs: efivarfs.sh # SKIP

In contrast, the cloud base images for ``aarch64`` use UEFI and
``efivarfs`` is mounted at ``/sys/firmware/efi/efivars`` on startup by
default.

.. code:: shell

   mount

::

   …
   efivarfs on /sys/firmware/efi/efivars type efivarfs (rw,nosuid,nodev,noexec,relatime)
   …

.. code:: shell

   ls -l /sys/firmware/efi/efivars

::

   -rw-r--r--. 1 root root   66 May 15 21:40 Boot0000-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root   85 May 15 21:40 Boot0001-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root   92 May 15 21:40 Boot0002-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root   92 May 15 21:40 Boot0003-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root  132 May 15 21:40 Boot0004-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root    6 May 15 21:40 BootCurrent-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root    8 May 15 21:40 BootOptionSupport-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root   14 May 15 21:40 BootOrder-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root    8 May 15 21:40 certdb-d9bee56e-75dc-49d9-b4d7-b534210f637a
   -rw-r--r--. 1 root root    8 May 15 21:40 certdbv-d9bee56e-75dc-49d9-b4d7-b534210f637a
   -rw-r--r--. 1 root root   82 May 15 21:40 ConIn-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root  571 May 15 21:40 ConInDev-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root   67 May 15 21:40 ConOut-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root  571 May 15 21:40 ConOutDev-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root   67 May 15 21:40 ErrOut-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root  571 May 15 21:40 ErrOutDev-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root   18 May 15 21:40 Key0000-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root   18 May 15 21:40 Key0001-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root    8 May 15 21:40 Lang-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root   17 May 15 21:40 LangCodes-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root 1729 May 15 21:40 MokListRT-605dab50-e046-4300-abb6-3dd810dd8b23
   -rw-r--r--. 1 root root    5 May 15 21:40 MokListTrustedRT-605dab50-e046-4300-abb6-3dd810dd8b23
   -rw-r--r--. 1 root root   80 May 15 21:40 MokListXRT-605dab50-e046-4300-abb6-3dd810dd8b23
   -rw-r--r--. 1 root root    8 May 15 21:40 MTC-eb704011-1402-11d3-8e77-00a0c969723b
   -rw-r--r--. 1 root root   12 May 15 21:40 OsIndicationsSupported-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root    7 May 15 21:40 PlatformLang-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root   22 May 15 21:40 PlatformLangCodes-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root  114 May 15 21:40 PlatformRecovery0000-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root   22 May 15 21:40 SbatLevelRT-605dab50-e046-4300-abb6-3dd810dd8b23
   -rw-r--r--. 1 root root    5 May 15 21:40 SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root    5 May 15 21:40 SetupMode-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root   68 May 15 21:40 SignatureSupport-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root    6 May 15 21:40 Timeout-8be4df61-93ca-11d2-aa0d-00e098032b8c
   -rw-r--r--. 1 root root    5 May 15 21:40 VarErrorFlag-04b37fe8-f6ae-480b-bdd5-37d98c5e89aa
   -rw-r--r--. 1 root root    5 May 15 21:40 VendorKeys-8be4df61-93ca-11d2-aa0d-00e098032b8c

The ``efivarfs:efivarfs.sh`` test should run fine, with a message
similar to this:

::

   # selftests: efivarfs: efivarfs.sh
   # --------------------
   # running test_create
   # --------------------
   # ./efivarfs.sh: line 52: /sys/firmware/efi/efivars/test_create-210be57c-9849-4fc7-a635-e6382d1aec27: Operation not permitted
   #   [PASS]
   # --------------------
   # running test_create_empty
   # --------------------
   #   [PASS]
   # --------------------
   # running test_create_read
   # --------------------
   #   [PASS]
   # --------------------
   # running test_delete
   # --------------------
   #   [PASS]
   # --------------------
   # running test_zero_size_delete
   # --------------------
   #   [PASS]
   # --------------------
   # running test_open_unlink
   # --------------------
   #   [PASS]
   # --------------------
   # running test_valid_filenames
   # --------------------
   #   [PASS]
   # --------------------
   # running test_invalid_filenames
   # --------------------
   #   [PASS]
   ok 1 selftests: efivarfs: efivarfs.sh

No variability of the test's results was observed across the history of
around 80 runs in total, on all versions {``ciqlts8_6``, ``ciqlts8_8``,
``ciqlts9_2``, ``ciqlts9_4``}; the test is stable.

Related files
=============

``Documentation/filesystems/efivarfs.rst``
------------------------------------------

Official, short documentation of the ``efivarfs`` filesystem.

``tools/testing/selftests/efivarfs/efivarfs.sh``
------------------------------------------------

The script realizing the collection's only test. Each of the subtests
(eg. ``test_open_unlink``) have its corresponding bash function with the
same name defined there. The ``test_create_read`` subtest requires the
compiled ``create-read`` binary and ``test_open_unlink`` requires
``open-unlink``. The rest is realized using standard shell file
operations.
