About
=====

This directory is meant to collect and organize all data, knowledge,
experience and insight related to kernel selftests of Rocky Linux.

Paths
=====

Below a few relevant files and directories from the Linux kernel's
source tree are discussed, at least those which are relevant to a *tests
user*, that is someone who compiles and runs them.

``Documentation/dev-tools/kselftest.rst``
-----------------------------------------

The official kernel documentation of kselftests. If you are new to the
topic then read it first to get the overall idea of how to work with
kselftests before continuing with this document, as it's meant to
complement and expand on ``kselftest.rst`` in Rocky's context, not to
replace it.

``tools/testing/selftests/``
----------------------------

The Linux kernel project's subdirectory in which all kselftests are
contained. The structure will be explained soon, but first some
terminology:

Test
   A single executable program, may be a script or an ELF binary,
   testing a specific functionality. For example the test *ip_defrag.sh*
   focuses on testing the IP defragmentation functionality from the
   kernel's networking stack.
Collection
   A set of tests related to the same specific module or subsystem. For
   example, the *net* collection contains all tests related to
   networking and contains, unsurprisingly, the *ip_defrag.sh* test
   mentioned above.

Every *test* belongs to some *collection*. The *fully qualified name* of
a test is prefixed by a collection it belongs to, and assumes the form
*‹collection›:‹test›*, for example ``net:ip_defrag.sh``. One can find
collections rich in tests, like ``bpf`` with around 45 of them, as well
as containing only a single one, like ``zram`` with ``zram: zram.sh``,
and everything in between.

The *tests* map directly to the files with the same name in the
``tools/testing/selftests/`` subdirectory. For example the
``net:ip_defrag.sh`` is realized by the
``tools/testing/selftests/net/ip_defrag.sh`` script. Every test maps to
an executable file in ``tools/testing/selftests/``, but not every
executable file in ``tools/testing/selftests/`` maps to a test (eg.
``tools/testing/selftests/rcutorture/bin/parse-build.sh``)

The *collections* map directly to the subdirectories in
``tools/testing/selftests/``. For example the ``net`` collection refers
to the ``tools/testing/selftests/net/`` subdir. All collections map to
subdirectories of ``tools/testing/selftests/``, but not all
subdirectories of ``tools/testing/selftests/`` map to a collection (eg.
``arm64/signal/testcases``).

While the subdirectories of ``tools/testing/selftests/`` can be composed
hierarchically, the *collections* are not. In case a collection's
directory is nested on a level deeper than 1 in
``tools/testing/selftests/`` its name is simply the relative path. For
example the ``tools/testing/selftests/net/forwarding`` defines the
``net/forwarding`` collection. Because there is no hierarchy among
collections the ``net`` collection is separate from ``net/forwarding``
and technically entirely different, even if logically related. As a
result the tests contained by ``net/forwarding`` are **not** contained
by ``net``. This may be a bit confusing, as the ``net`` collection now
doesn't contain "all network-related tests" but rather "all
network-related tests which aren't already contained by some more
specific collections".

To get the explicit list of all fully qualified test names provided by
the releaes relate to `Printing the list of all available tests`_.

``tools/testing/selftests/Makefile``
------------------------------------

A central file defining the building, running and installation of the
selftests. The ``TARGETS`` defined there are equivalent to
*collections*. Using this file to run selftests (see
`Documentation/dev-tools/kselftest.rst`_) allows to run them on the
collecion-level granularity, but no further. For example the call

.. code:: shell

   make -C tools/testing/selftests TARGETS="seccomp cgroup" run_tests

from the kernel's directory root will run all of the tests

-  ``seccomp:seccomp_bpf``
-  ``seccomp:seccomp_benchmark``
-  ``cgroup:test_memcontrol``
-  ``cgroup:test_kmem``
-  ``cgroup:test_core``
-  ``cgroup:test_freezer``
-  ``cgroup:test_kill``
-  ``cgroup:test_stress.sh``
-  ``cgroup:test_cpuset_prs.sh``

To run only a specific test from a collection containing more of them
(eg. ``cgroup:test_freezer``) a different method must be used - see
`Running a specific set of tests`_.

``tools/testing/selftests/lib.mk``
----------------------------------

This file is included by all the recursive ``Makefile`` files in the
``tools/testing/selftests`` subtree. Included by the main
`tools/testing/selftests/Makefile`_ as well. If something seems like it
should be in the central Makefile but isn't then it's probably here.

The following variables are populated by all the Makefiles scattered
around the ``tools/testing/selftests`` subtree and collectively list all
the executable tests:

TEST_GEN_PROGS
   Names of generated test programs. Examples:

   -  ``tools/testing/selftests/arm64/bti/Makefile``

      .. code:: makefile

         TEST_GEN_PROGS := btitest nobtitest

   -  ``tools/testing/selftests/proc/Makefile``

      .. code:: makefile

         TEST_GEN_PROGS :=
         TEST_GEN_PROGS += fd-001-lookup
         TEST_GEN_PROGS += fd-002-posix-eq
         TEST_GEN_PROGS += fd-003-kthread
         TEST_GEN_PROGS += proc-loadavg-001
         TEST_GEN_PROGS += proc-pid-vm
         TEST_GEN_PROGS += proc-self-map-files-001
         TEST_GEN_PROGS += proc-self-map-files-002
         TEST_GEN_PROGS += proc-self-syscall
         TEST_GEN_PROGS += proc-self-wchan
         TEST_GEN_PROGS += proc-subset-pid
         TEST_GEN_PROGS += proc-uptime-001
         TEST_GEN_PROGS += proc-uptime-002
         TEST_GEN_PROGS += read
         TEST_GEN_PROGS += self
         TEST_GEN_PROGS += setns-dcache
         TEST_GEN_PROGS += setns-sysvipc
         TEST_GEN_PROGS += thread-self
         TEST_GEN_PROGS += proc-multiple-procfs
         TEST_GEN_PROGS += proc-fsconfig-hidepid

TEST_PROGS
   ::

      # TEST_PROGS are for test shell scripts.

   Names for the test programs that don't need generating. Examples:

   -  ``tools/testing/selftests/net/forwarding/Makefile``

      .. code:: makefile

         TEST_PROGS = bridge_igmp.sh \
                 bridge_locked_port.sh \
                 bridge_mld.sh \
                 bridge_port_isolation.sh \
                 …
                 vxlan_symmetric_ipv6.sh \
                 vxlan_symmetric.sh

   -  ``tools/testing/selftests/sysctl/Makefile``

      .. code:: makefile

         TEST_PROGS := sysctl.sh

TEST_CUSTOM_PROGS
   ::

      # TEST_CUSTOM_PROGS should be used by tests that require
      # custom build rule and prevent common build rule use.

   Rarely used. Examples:

   -  ``tools/testing/selftests/bpf/Makefile``

      .. code:: makefile

         TEST_CUSTOM_PROGS = $(OUTPUT)/urandom_read

   -  ``tools/testing/selftests/sync/Makefile``

      .. code:: makefile

         TEST_CUSTOM_PROGS := $(OUTPUT)/sync_test

``tools/testing/selftests/run_kselftest.sh``
--------------------------------------------

This script provides the most complete and convenient interface to run
the selftests.

::

   Usage: tools/testing/selftests/run_kselftest.sh [OPTIONS]
     -s | --summary                Print summary with detailed log in output.log
     -t | --test COLLECTION:TEST   Run TEST from COLLECTION
     -c | --collection COLLECTION  Run all tests from COLLECTION
     -l | --list                   List the available collection:test entries
     -d | --dry-run                Don't actually run any tests
     -h | --help                   Show this usage info

Unfortunately it was meant to be used on the *installed* version of
selftests and cannot be used directly from the source as it's done with
``make … run_tests``, at least not without preparation: it requires the
`tools/testing/selftests/kselftest-list.txt`_ file, which must be
created first (see `Printing the list of all available tests`_ and
`Running a specific set of tests`_). Once it's in place this script is
the preferred method to conduct testing.

From the technical viewpoint the script serves as a wrapper of
`tools/testing/selftests/kselftest/runner.sh`_, transforming the
specification of to-be-run tests from all the ``--collection`` and
``--test`` arguments into a uniform list of singular tests, handled
further by the procedures from the sourced
`tools/testing/selftests/kselftest/runner.sh`_ script. For example the
``--test net:reuseport_bpf --collection memfd --test net:reuseport_bpf_numa``
arguments will be converted to a list of

-  ``memfd:memfd_test``,
-  ``memfd:run_fuse_test.sh``,
-  ``memfd:run_hugetlbfs_test.sh``,
-  ``net:reuseport_bpf``,
-  ``net:reuseport_bpf_numa``,

and passed to ``run_many`` function, in that order.

``tools/testing/selftests/kselftest/runner.sh``
-----------------------------------------------

The script serves as an intermediary to all the executables in
``tools/testing/selftests`` doing the actual testing. It's used by both
``run_kselftest.sh`` and the ``make … run_tests`` method of running the
selftests. Functions:

-  Takes care of timing out the tests using the ``timeout`` program. If
   not specified the default timeout for all tests is 45 seconds.
   (Related: `Changing the test's timeout`_).

-  Applies settings specific to the collection the test being run
   belongs to. It does that by looking for the ``settings`` file in the
   collection's directory. If it's found the variable assignments found
   there are evaluated, except the variable names are prefixed with
   "kselftests\_" beforehand. For example the
   ``tools/testing/selftests/bpf/settings`` file

   .. code:: shell

      timeout=0
      rhskip="test_bpftool_build.sh test_lwt_seg6local.sh test_doc_build.sh"

   will define the variables

   +--------------------+------------------------------------------------+
   | Variable           | Value                                          |
   +====================+================================================+
   | kselftests_timeout | 0                                              |
   +--------------------+------------------------------------------------+
   | kselftests_rhskip  | "test_bpftool_build.sh test_lwt_seg6local.sh   |
   |                    | test_doc_build.sh"                             |
   +--------------------+------------------------------------------------+

   These variables are used in the ``runner.sh`` script itself. No other
   settings than ``timeout`` and ``rhskip`` were found.

-  Embeds the output of individual testing programs into a TAP 13 format
   ("Test Everything Protocol, ver 13"), putting all stdout in comments
   (lines starting with "#") and printing summaries of the test's
   status, conforming to TAP 13. Four end states are distinguished:

   #. passed test

      ::

         ok 31 selftests: net/forwarding: ipip_flat_gre_key.sh

   #. skipped test (classifiad as passed)

      ::

         ok 54 selftests: net: gre_gso.sh # SKIP

   #. failed test

      ::

         not ok 58 selftests: net: rps_default_mask.sh # exit=1

   #. timed out test (classified as failed)

      ::

         not ok 29 selftests: net/forwarding: ip6gre_inner_v4_multipath.sh # TIMEOUT 45 seconds

``tools/testing/selftests/kselftest-list.txt``
----------------------------------------------

A list of fully qualified test names. Example:

::

   bpf:test_verifier
   bpf:test_tag
   bpf:test_maps
   …
   bpf:test_doc_build.sh
   bpf:test_xsk.sh
   livepatch:test-livepatch.sh
   livepatch:test-callbacks.sh
   livepatch:test-shadow-vars.sh
   livepatch:test-state.sh
   livepatch:test-ftrace.sh
   net:reuseport_bpf
   net:reuseport_bpf_cpu
   net:reuseport_bpf_numa
   net:reuseport_dualstack
   …

This list is what `tools/testing/selftests/run_kselftest.sh`_ script
considers to be "available tests". The file doesn't exist in the
repository and must be created if ``run_kselftest.sh`` is to be used.
See `Printing the list of all available tests`_.

Use cases
=========

Printing the list of all available tests
----------------------------------------

.. code:: shell

   for col in $(make --print-data-base -C tools/testing/selftests SKIP_TARGETS= --dry-run clean \
                    | grep '^TARGETS :\?=' \
                    | sed -e 's/.*:\?=//g'); do
       make --silent COLLECTION=${col} -C tools/testing/selftests/${col} emit_tests
   done 2> /dev/null

Explanation:

``make --print-data-base -C tools/testing/selftests --dry-run clean``
   Provides the value of ``TARGETS`` variable

   ``--print-data-base``
      Prints the value of ``TARGETS`` variable (among many other
      information)
   ``SKIP_TARGETS=``
      Demands explicitly to **not** remove any positions from the
      ``TARGETS`` variable (yes, the Makefile may decide for the user to
      skip some targets, eg. ``bpf`` on ``ciqlts9_2``)
   ``clean``
      Any valid target will do, ``clean`` just takes least time.
   ``--dry-run``
      Makes sure nothing is actually built. We're only interested in the
      data base dump.

``| grep '^TARGETS :\?=' | sed -e 's/.*:\?=//g'``
   Extract the value of the ``TARGETS`` variable
``make --silent --no-print-directory COLLECTION=${col} -C tools/testing/selftests/${col} emit_tests``
   Prints the full names of tests provided by each collection

   ``--silent``, ``--no-print-directory``
      Silences ``make`` and let's only the ``emit_tests`` target speak.
   ``COLLECTION=${col}``
      Makes each line print in full form like ``<collection>:<test>``
      instead of just ``:<test>``.

``2> /dev/null``
   Avoids having error messages from foreign architectures test suites,
   like

   ::

      make: *** No rule to make target 'emit_tests'.  Stop.

   for ``sparc64`` on ``x86_64``, for example.

Note that the resulting list may contain tests which cannot be run
because they weren't compiled. The list is meant to show all the tests
that *may* be run, provided all the prerequisites are met and the tests
were actually built..

Running a specific set of tests
-------------------------------

To run a single test (instead of the whole collection) first write to
the `tools/testing/selftests/kselftest-list.txt`_ file the test name
you wish to run. Then run the
`tools/testing/selftests/run_kselftest.sh`_ script using ``--test``
argument with the same value. Example:

.. code:: shell

   echo fpu:test_fpu > tools/testing/selftests/kselftest-list.txt
   tools/testing/selftests/run_kselftest.sh --test fpu:test_fpu

Alternatively just write the output of the previous snippet dumping the
list of all tests available to the
``tools/testing/selftests/kselftest-list.txt`` file, then pick from them
freely using ``--test`` and ``--collection`` parameters of the
``run_kselftest.sh`` script.

Changing the test's timeout
---------------------------

Example for ``net/forwarding:bridge_mld.sh``:

Run the ``net/forwarding:bridge_mld.sh`` test from the kernel source
tree root

.. code:: shell

   (
       test=net/forwarding:bridge_mld.sh
       echo "${test}" > tools/testing/selftests/kselftest-list.txt
       tools/testing/selftests/run_kselftest.sh --test "${test}"
   )

::

   TAP version 13
   1..1
   # selftests: net/forwarding: bridge_mld.sh
   # TEST: MLDv2 report ff02::cc is_include                              [ OK ]
   # TEST: MLDv2 report ff02::cc include -> allow                        [ OK ]
   # TEST: MLDv2 report ff02::cc include -> is_include                   [ OK ]
   # TEST: MLDv2 report ff02::cc include -> is_exclude                   [ OK ]
   # TEST: MLDv2 report ff02::cc include -> to_exclude                   [ OK ]
   #
   not ok 1 selftests: net/forwarding: bridge_mld.sh # TIMEOUT 45 seconds

The default timeout of 45 seconds was used.

Check if any settings exist already, not to mess something.

.. code:: shell

   cat net/forwarding/settings

::

   cat: net/forwarding/settings: No such file or directory

No settings for the ``net/forwarding`` collection are used, can be
created from scratch and ``run_kselftest.sh`` will pick it up.

Set the timeout to 15 seconds, just for the presentation purpose.

.. code:: shell

   echo "timeout=15" > tools/testing/selftests/net/forwarding/settings

Run the test again

.. code:: shell

   tools/testing/selftests/run_kselftest.sh --test net/forwarding:bridge_mld.sh

::

   TAP version 13
   1..1
   # selftests: net/forwarding: bridge_mld.sh
   #
   not ok 1 selftests: net/forwarding: bridge_mld.sh # TIMEOUT 15 seconds

The timeout was successfully changed to 15 seconds.
