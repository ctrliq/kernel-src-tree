About
=====

This collection tests the ``ptrace()`` system call, specifically the
ability of a tracer to set breakpoints (on functions) and watchpoints
(on variables) in the tracee and whether they are correctly triggered
when tracee hits them.

The ``breakpoint_test`` test from the collection is chronologically the
very first kernel selftest.

Tests suite compilation and running
===================================

The suite generally contains two tests:

#. ``breakpoint_test`` (on x86\* platforms) or ``breakpoint_test_arm64``
   (on the arm64 platforms),
#. ``step_after_suspend_test``.

For other platforms than x86\* and arm64 the suite contains only
``step_after_suspend_test``.

``breakpoint_test``
-------------------

When compiling the ``breakpoints`` collection on *x86_64* beware not to
set the ``ARCH`` variable explicitly, or the ``breakpoint_test`` test
won't be produced. So, if your ``uname -m`` reports ``x86_64`` then this
is ok:

.. code:: shell

   make -C tools/testing/selftests TARGETS=breakpoints

and this is **not** ok:

.. code:: shell

   make ARCH=x86_64 -C tools/testing/selftests TARGETS=breakpoints

It's obviously a bug in the
``tools/testing/selftests/breakpoints/Makefile`` file, precisely in the
way the ``ARCH`` variable is normalized with ``sed``, with this
normalization being bound to setting default ``ARCH`` value instead of
being general,

.. code:: makefile

   ARCH ?= $(shell echo $(uname_M) | sed -e s/i.86/x86/ -e s/x86_64/x86/)

yet the conditional compilation of ``breakpoint_test`` assuming this
normalization in all cases.

If compiled successfully the test is expected to pass on all versions
with a message similar to

::

   TAP version 13
   1..1
   # timeout set to 45
   # selftests: breakpoints: breakpoint_test
   # TAP version 13
   # 1..110
   # ok 1 Test breakpoint 0 with local: 0 global: 1
   # ok 2 Test breakpoint 1 with local: 0 global: 1
   # ok 3 Test breakpoint 2 with local: 0 global: 1
   # ok 4 Test breakpoint 3 with local: 0 global: 1
   # ok 5 Test breakpoint 0 with local: 1 global: 0
   # ok 6 Test breakpoint 1 with local: 1 global: 0
   …
   # ok 104 Test read watchpoint 3 with len: 8 local: 1 global: 0
   # ok 105 Test read watchpoint 0 with len: 8 local: 1 global: 1
   # ok 106 Test read watchpoint 1 with len: 8 local: 1 global: 1
   # ok 107 Test read watchpoint 2 with len: 8 local: 1 global: 1
   # ok 108 Test read watchpoint 3 with len: 8 local: 1 global: 1
   # ok 109 Test icebp
   # ok 110 Test int 3 trap
   # # Totals: pass:110 fail:0 xfail:0 xpass:0 skip:0 error:0
   ok 1 selftests: breakpoints: breakpoint_test

``breakpoint_test_arm64``
-------------------------

On versions ``ciqlts8_6``, ``ciqlts8_8`` the test contains a bug which
prevents it from being compiled successfully for ``arm64``:

::

   gcc     breakpoint_test_arm64.c /mnt/build_files/kernel-src-tree-kselftests-ciqlts8_8/tools/testing/selftests/kselftest_harness.h /mnt/build_files/kernel-src-tree-kselftests-ciqlts8_8/tools/testing/selftests/kselftest.h  -o /mnt/build_files/kernel-src-tree-kselftests-ciqlts8_8/tools/testing/selftests/breakpoints/breakpoint_test_arm64
   breakpoint_test_arm64.c: In function ‘main’:
   breakpoint_test_arm64.c:226:14: warning: implicit declaration of function ‘run_test’; did you mean ‘arun_test’? [-Wimplicit-function-declaration]
        result = run_test(size, MIN(size, 8), wr, wp);
                 ^~~~~~~~
                 arun_test
   /tmp/cc5dMu7r.o: In function `main':
   breakpoint_test_arm64.c:(.text+0xb08): undefined reference to `run_test'
   breakpoint_test_arm64.c:(.text+0xc10): undefined reference to `run_test'
   collect2: error: ld returned 1 exit status

This bug was fixed in the mainline with the
``5b06eeae52c02dd0d9bc8488275a1207d410870b`` commit. It's in the process
of backporting to ``ciqlts8_6``, ``ciqlts8_8`` at the time of this
writing.

Additionally, when compiling the test on base cloud images for
``ciqlts8_6`` and ``ciqlts8_8``\  [1]_, and using Mountain-provided
repositories, the following error may be encountered:

::

   breakpoint_test_arm64.c: In function ‘run_test’:
   breakpoint_test_arm64.c:188:25: error: ‘TRAP_HWBKPT’ undeclared (first use in this function); did you mean ‘TRAP_BRKPT’?
     if (siginfo.si_code != TRAP_HWBKPT) {
                            ^~~~~~~~~~~
                            TRAP_BRKPT
   breakpoint_test_arm64.c:188:25: note: each undeclared identifier is reported only once for each function it appears in

The ``TRAP_BRKPT`` constant is searched for in the
``/usr/include/bits/siginfo-consts.h`` file, which in versions ≥ 9.2
contains

.. code:: c

   /* `si_code' values for SIGTRAP signal.  */
   enum
   {
     TRAP_BRKPT = 1,       /* Process breakpoint.  */
   #  define TRAP_BRKPT    TRAP_BRKPT
     TRAP_TRACE,           /* Process trace trap.  */
   #  define TRAP_TRACE    TRAP_TRACE
     TRAP_BRANCH,          /* Process taken branch trap.  */
   #  define TRAP_BRANCH   TRAP_BRANCH
     TRAP_HWBKPT,          /* Hardware breakpoint/watchpoint.  */
   #  define TRAP_HWBKPT   TRAP_HWBKPT
     TRAP_UNK          /* Undiagnosed trap.  */
   #  define TRAP_UNK  TRAP_UNK
   };
   # endif

while in versions < 9.2 contains

.. code:: c

   /* `si_code' values for SIGTRAP signal.  */
   enum
   {
     TRAP_BRKPT = 1,       /* Process breakpoint.  */
   #  define TRAP_BRKPT    TRAP_BRKPT
     TRAP_TRACE            /* Process trace trap.  */
   #  define TRAP_TRACE    TRAP_TRACE
   };
   # endif

If the ``TRAP_HWBKPT`` constant can't be included in
``/usr/include/bits/siginfo-consts.h`` in some civilised manner then
simply expanding the set of known ``TRAP_*`` codes in the system to
match those on ≥ 9.2 should work:

.. code:: shell

   sudo sed -e '144 d' -e 's/#  define TRAP_TRACE\tTRAP_TRACE/  TRAP_TRACE,\n#  define TRAP_TRACE\tTRAP_TRACE\n  TRAP_BRANCH,\n#  define TRAP_BRANCH\tTRAP_BRANCH\n  TRAP_HWBKPT,\n#  define TRAP_HWBKPT\tTRAP_HWBKPT\n  TRAP_UNK\n#  define TRAP_UNK\tTRAP_UNK/g' -i /usr/include/bits/siginfo-consts.h

No compilation problems for ``breakpoint_test_arm64`` were encountered
on versions ``ciqlts9_2`` and ``ciqlts9_4``.

If the test compiled successfully it's expected to pass with the message
similar to the following

::

   # TAP version 13
   # 1..213
   # # child did not single-step
   # ok 1 Test size = 1 write offset = 0 watchpoint offset = -1
   # ok 2 Test size = 1 write offset = 0 watchpoint offset = 0
   # # child did not single-step
   # ok 3 Test size = 1 write offset = 0 watchpoint offset = 1
   # # child did not single-step
   # ok 4 Test size = 1 write offset = 1 watchpoint offset = 0
   # ok 5 Test size = 1 write offset = 1 watchpoint offset = 1
   …
   # ok 206 Test size = 32 write offset = 32 watchpoint offset = 32
   # # child did not single-step
   # ok 207 Test size = 32 write offset = 32 watchpoint offset = 64
   # ok 208 Test size = 1 write offset = -1 watchpoint offset = -8
   # ok 209 Test size = 2 write offset = -2 watchpoint offset = -8
   # ok 210 Test size = 4 write offset = -4 watchpoint offset = -8
   # ok 211 Test size = 8 write offset = -8 watchpoint offset = -8
   # ok 212 Test size = 16 write offset = -16 watchpoint offset = -8
   # ok 213 Test size = 32 write offset = -32 watchpoint offset = -8
   # # Totals: pass:213 fail:0 xfail:0 xpass:0 skip:0 error:0
   ok 1 selftests: breakpoints: breakpoint_test_arm64

``step_after_suspend_test``
---------------------------

No problems were encountered when compiling this test, on any of the
versions ``ciqlts8_6``, ``ciqlts8_8``, ``ciqlts9_2``, ``ciqlts9_4``,
archs ``x86_64``, ``aarch64``. However, for the test to run the system
must provide the ability to suspend it, as it would be done with

::

   root@…# echo mem > /sys/power/state

Can be checked with this command directly, that's what
``step_after_suspend_test`` is doing internally. Unfortunately on the
qemu-kvm virtual machines this fails with the ``virtio-pci`` device (the
one connecting the machine with the hypervisor) refusing to cooperate:

::

   [root@ciqlts-9-2 pvts]# echo mem > /sys/power/state
   [   24.434434] PM: suspend entry (s2idle)
   [   24.498974] Filesystems sync: 0.062 seconds
   [   24.501574] Freezing user space processes ... (elapsed 0.001 seconds) done.
   [   24.507156] OOM killer disabled.
   [   24.509062] Freezing remaining freezable tasks ... (elapsed 0.001 seconds) done.
   [   24.514480] printk: Suspending console(s) (use no_console_suspend to debug)
   [   24.540895] virtio-fs: suspend/resume not yet supported
   [   24.540906] virtio-pci 0000:03:00.0: PM: pci_pm_suspend(): virtio_pci_freeze+0x0/0x50 returns -95
   [   24.540970] virtio-pci 0000:03:00.0: PM: dpm_run_callback(): pci_pm_suspend+0x0/0x170 returns -95
   [   24.541011] virtio-pci 0000:03:00.0: PM: failed to suspend async: error -95
   [   24.554785] PM: Some devices failed to suspend, or early wake event detected
   [   24.686487] OOM killer enabled.
   [   24.687385] Restarting tasks ... done.
   [   24.689834] PM: suspend exit
   bash: echo: write error: Operation not supported
   [root@ciqlts-9-2 pvts]# [   24.886811] ata3: SATA link down (SStatus 0 SControl 300)
   [   24.889470] ata5: SATA link down (SStatus 0 SControl 300)
   [   24.892095] ata2: SATA link down (SStatus 0 SControl 300)
   [   24.894956] ata4: SATA link down (SStatus 0 SControl 300)
   [   24.897717] ata6: SATA link down (SStatus 0 SControl 300)

When running selftests in qemu-kvm it's therefore best to omit this
test, or else it will keep scaring with failures like

::

   # selftests: breakpoints: step_after_suspend_test
   # TAP version 13
   # Bail out! Failed to enter Suspend state
   # # Totals: pass:0 fail:0 xfail:0 xpass:0 skip:0 error:0
   not ok 1 selftests: breakpoints: step_after_suspend_test # exit=1

(Probably a better behavior would be for the test to ``# SKIP`` in this
case.)

.. [1]
   Referring to the images available at
   https://download.rockylinux.org/vault/rocky/8.6/images/Rocky-8-GenericCloud-8.6.20220702.0.aarch64.qcow2
   for ``ciqlts8_6`` and at
   https://download.rockylinux.org/vault/rocky/8.8/images/aarch64/Rocky-8-GenericCloud-Base-8.8-20230518.0.aarch64.qcow2
   for ``ciqlts8_8``, to be precise.
