Requirements
============

#. The tests require ``jq`` and ``mausezahn`` tools to run (packages
   ``jq`` and ``netsniff-ng`` on RHEL)

#. The file ``tools/testing/selftests/net/forwarding/forwarding.config``
   must be created in kernel's source dir for the tests not to crash.
   The
   ``tools/testing/selftests/net/forwarding/forwarding.config.sample``
   is provided in the repo as a prototype to start with. No need to
   deviate from it though to have a working configuration - just copying
   ``forwarding.config.sample`` to ``forwarding.config`` should be
   enough to have functional ``net/forwarding`` tests collection.

Problems
========

The tests ``sch_ets.sh``, ``sch_red.sh``, ``sch_tbf_ets.sh``,
``sch_tbf_prio.sh``, ``sch_tbf_root.sh`` can hang the machine for more
than 10 minutes. Whether it's expected or dyfunctional was not
established yet. In either case this hinders efficient testing.
