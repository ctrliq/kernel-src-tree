About
=====

LKDTM (Linux Kernel Dump Test Module) is used for crashing the kernel on
demand and checking whether the kernel handled this properly.

Related files
=============

``Documentation/fault-injection/provoke-crashes.rst``
-----------------------------------------------------

Describes the ``lkdtm`` module allowing for provoked kernel crashes.

The collection's test names correspond to the ``cpoint_type`` ("crash
point type") values mentioned in that document.

``tools/testing/selftests/lkdtm/tests.txt``
-------------------------------------------

List of the actual tests contained in the collection, with some
meta-data attached. The mapping:

::

   #PANIC
   BUG kernel BUG at
   WARNING WARNING:
   WARNING_MESSAGE message trigger
   EXCEPTION
   #LOOP Hangs the system
   #EXHAUST_STACK Corrupts memory on failure
   #CORRUPT_STACK Crashes entire system on success
   #CORRUPT_STACK_STRONG Crashes entire system on success
   …

->

::

   lkdtm:PANIC.sh
   lkdtm:BUG.sh
   lkdtm:WARNING.sh
   lkdtm:WARNING_MESSAGE.sh
   lkdtm:EXCEPTION.sh
   lkdtm:LOOP.sh
   lkdtm:EXHAUST_STACK.sh
   lkdtm:CORRUPT_STACK.sh
   lkdtm:CORRUPT_STACK_STRONG.sh
   …

#. The lines starting with ``#`` aren't really comments, in a sense that
   whatever follows the ``#`` isn't ignored - it does in fact specify a
   test, but it will be reported as skipped when executed. The
   "dangerous" tests are commented out by default.

#. When a line contains multiple words only the first one is used as the
   test's name.

   ::

      BUDDY_INIT_ON_ALLOC Memory appears initialized

   ->

   ::

      lkdtm:BUDDY_INIT_ON_ALLOC.sh

   The rest is used as a short *note* about what is to be expected from
   this test, displayed upon the test execution. It's also what's being
   searched for in dmesg after provoking the failure, as a test's
   success criterion.

``tools/testing/selftests/lkdtm/run.sh``
----------------------------------------

The template for each test in the collection. Although the test names
like ``lkdtm:WARNING_MESSAGE.sh`` suggest there is a shell script for
each of them there is in fact only one - the ``run.sh`` - which is
copied multiple times inside the ``tools/testing/selftests/lkdtm/``
directory with different names corresponding to different test names
when the ``lkdtm`` collection is build. Each script then, when run,
introspects its own name to know which test it should conduct and then
just writes the test's name into the
``/sys/kernel/debug/provoke-crash/DIRECT``, like

.. code:: shell

   echo ‹test› > /sys/kernel/debug/provoke-crash/DIRECT

After that the newly shown dmesg lines are searched for the test's note,
or for "call trace:" if it doesn't have any. If the text was found the
test reports success, otherwise failure.

For example, the ``lkdtm:WARNING_MESSAGE.sh`` test is realized by
``tools/testing/selftests/lkdtm/WARNING_MESSAGE.sh``, copied from
``tools/testing/selftests/lkdtm/run.sh``, which writes

.. code:: shell

   echo WARNING_MESSAGE > /sys/kernel/debug/provoke-crash/DIRECT

Then the dmesg is searched for "message trigger", as that is the
``WARNING_MESSAGE``'s note defined in the
`tools/testing/selftests/lkdtm/tests.txt`_

``tools/testing/selftests/lkdtm/stack-entropy.sh``
--------------------------------------------------

Corresponds to the ``lkdtm:stack-entropy.sh`` test, the only one not
realized by the ``run.sh`` script.

Requirements and applicability
==============================

The ``lkdtm`` collection is not available in pre-``9.2`` versions. The
tests don't have any compilation requirements, but to run them the
``lkdtm`` module has to be available in the tested kernel, provided by
the ``CONFIG_LKDTM`` option, which is disabled in all default
configurations of LTS versions ``9.2``, ``9.4`` (other not checked)

.. code:: shell

   grep CONFIG_LKDTM kernel-src-tree-{ciqlts9_2,ciqlts9_4}/configs/kernel*.config

::

   kernel-src-tree-ciqlts9_2/configs/kernel-aarch64-64k-debug-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_2/configs/kernel-aarch64-64k-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_2/configs/kernel-aarch64-debug-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_2/configs/kernel-aarch64-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_2/configs/kernel-ppc64le-debug-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_2/configs/kernel-ppc64le-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_2/configs/kernel-s390x-debug-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_2/configs/kernel-s390x-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_2/configs/kernel-s390x-zfcpdump-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_2/configs/kernel-x86_64-debug-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_2/configs/kernel-x86_64-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-aarch64-64k-debug-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-aarch64-64k-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-aarch64-debug-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-aarch64-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-aarch64-rt-debug-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-aarch64-rt-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-ppc64le-debug-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-ppc64le-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-s390x-debug-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-s390x-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-s390x-zfcpdump-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-x86_64-debug-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-x86_64-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-x86_64-rt-debug-rhel.config:# CONFIG_LKDTM is not set
   kernel-src-tree-ciqlts9_4/configs/kernel-x86_64-rt-rhel.config:# CONFIG_LKDTM is not set

This does not mean that these tests don't apply to Rocky Linux in
general (disabling ``CONFIG_LKDTM`` does not eliminate potentially
malfunctioning code branches, only the way to expose them), but it
certainly doesn't make sense to run them on the default builds.

Note that for full functionality also other options may need to be
enabled - see the ``tools/testing/selftests/lkdtm/config`` file.
