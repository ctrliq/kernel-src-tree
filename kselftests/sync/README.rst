About
=====

The ``sync`` collection contains a single test ``sync:sync_test``. At
its core it tests the "Sync File" functionality enabled by
``CONFIG_SYNC_FILE``.

The *sync* mechanism in the context of this test is a solution to a
problem of different devices working asynchronously with shared memory
buffers. One device (the producer) fills the buffer while other devices
(the consumers - possibly many) read it. The buffer should not be read
by any consumer until the producer finished filling it. Conversely, the
producer should not try to re-use a buffer for other tasks before the
last consumer finished reading it. The *sync* framework allows producers
to signal to consumers (and vice versa) when the data is ready to be
read (or the buffer to be written). [1]_

This process is first and mainly used for rendering graphics, with a GPU
or V4L driver in the role of a producer and a DRM (Direct Rendering
Manager) driver in the role of a consumer. [2]_

The *sync* framework was born in Android and was later (2016) ported to
Linux. [3]_ It's now part of the ``driver/dma-buf``, which was
previously using a similar mechanism - dma-buf fences [4]_ - albeit
lower-level and more primitve [5]_. It didn't replace dma-buf fences but
acts as an alternative [6]_

Applicability
=============

Some confusion may arise around which module ``sync:sync_test`` actually
tests: is it ``sw_sync.ko``, provided by ``CONFIG_SW_SYNC`` option
(*disabled* in all Rocky versions) or ``sync_file.ko``, provided by
``CONFIG_SYNC_FILE`` (*enabled* in all Rocky versions)?

The ``sw_sync.ko`` is presented in different (and even the same)
contexts as either

#. a module made specifically for testing ``sync_file.ko`` (like the
   "Sync File Validation Framework" title in ``CONFIG_SW_SYNC``'s
   `documentation <https://www.kernelconfig.io/CONFIG_SW_SYNC?q=CONFIG_SW_SYNC&kernelversion=5.15.183&arch=x86>`__),
   or
#. a module providing a separate functionality deserving of its own
   testing (like the "Useful when there is no hardware primitive backing
   the synchronization" characterization in the very same doc).

Current understanding is that ``sync:sync_test`` tests ``sw_sync.ko``
directly and ``sync_file.ko`` indirectly and while, theoretically,
``sw_sync.ko`` provides functionality usable beyond just testing
``sync_file.ko``, in practice that is its only purpose and using it for
any other is discouraged in the module's own documentation ("WARNING:
improper use of this can result in deadlocking kernel drivers from
userspace. Intended for test and debug only." [7]_). At the same time
there are no tests testing ``sync_file.ko`` by other means than with the
use of ``sw_sync.ko``. The ``sw_sync.ko`` should therefore be treated as
a de facto testing module for ``sync_file.ko``.

The ``sync`` test suite therefore applies to all Rocky versions LTS 8.6,
8.8, 9.2, 9.4 (enabled ``CONFIG_SYNC_FILE``), but requires modified
configuration to actually run the tests (disabled ``CONFIG_SW_SYNC``).
Otherwise it will always ``# SKIP`` and can be omitted.

.. [1]
   https://blog.linuxplumbersconf.org/2014/ocw/system/presentations/2355/original/03%20-%20sync%20%26%20dma-fence.pdf

.. [2]
   https://www.kernelconfig.io/CONFIG_SYNC_FILE?q=CONFIG_SYNC_FILE&kernelversion=5.15.183&arch=x86

.. [3]
   https://lwn.net/Articles/672094/

.. [4]
   https://www.kernel.org/doc/html/v4.10/driver-api/dma-buf.html

.. [5]
   https://blog.linuxplumbersconf.org/2014/ocw/system/presentations/2355/original/03%20-%20sync%20%26%20dma-fence.pdf

.. [6]
   https://www.phoronix.com/news/Linux-4.9-To-De-Stage-SW_SYNC

.. [7]
   https://www.kernelconfig.io/CONFIG_SW_SYNC?q=CONFIG_SW_SYNC&kernelversion=5.15.183&arch=x86
