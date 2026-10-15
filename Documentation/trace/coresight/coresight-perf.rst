.. SPDX-License-Identifier: GPL-2.0

================
CoreSight - Perf
================

    :Author:   Carsten Haitzler <carsten.haitzler@arm.com>
    :Date:     June 29th, 2022

Perf is able to locally access CoreSight trace data and store it to the
output perf data files. This data can then be later decoded to give the
instructions that were traced for debugging or profiling purposes. You
can log such data with a perf record command like::

   perf record -e cs_etm//u testbinary

This would run some test binary (testbinary) until it exits and record
a perf.data trace file. That file would have AUX sections if CoreSight
is working correctly. You can dump the content of this file as
readable text with a command like::

   perf report --stdio --dump -i perf.data

You should find some sections of this file have AUX data blocks like::

   0x1e78 [0x30]: PERF_RECORD_AUXTRACE size: 0x11dd0  offset: 0  ref: 0x1b614fc1061b0ad1  idx: 0  tid: 531230  cpu: -1

   . ... CoreSight ETM Trace data: size 73168 bytes
           Idx:0; ID:10;   I_ASYNC : Alignment Synchronisation.
             Idx:12; ID:10;  I_TRACE_INFO : Trace Info.; INFO=0x0 { CC.0 }
             Idx:17; ID:10;  I_ADDR_L_64IS0 : Address, Long, 64 bit, IS0.; Addr=0x0000000000000000;
             Idx:26; ID:10;  I_TRACE_ON : Trace On.
             Idx:27; ID:10;  I_ADDR_CTXT_L_64IS0 : Address & Context, Long, 64 bit, IS0.; Addr=0x0000FFFFB6069140; Ctxt: AArch64,EL0, NS;
             Idx:38; ID:10;  I_ATOM_F6 : Atom format 6.; EEEEEEEEEEEEEEEEEEEEEEEE
             Idx:39; ID:10;  I_ATOM_F6 : Atom format 6.; EEEEEEEEEEEEEEEEEEEEEEEE
             Idx:40; ID:10;  I_ATOM_F6 : Atom format 6.; EEEEEEEEEEEEEEEEEEEEEEEE
             Idx:41; ID:10;  I_ATOM_F6 : Atom format 6.; EEEEEEEEEEEN
             ...

If you see these above, then your system is tracing CoreSight data
correctly.

To compile perf with CoreSight support in the tools/perf directory do::

    make CORESIGHT=1

This requires OpenCSD to build. You may install distribution packages
for the support such as libopencsd and libopencsd-dev or download it
and build yourself. Upstream OpenCSD is located at:

  https://github.com/Linaro/OpenCSD

For complete information on building perf with CoreSight support and
more extensive usage look at:

  https://github.com/Linaro/OpenCSD/blob/master/HOWTO.md


Kernel CoreSight Support
------------------------

You will also want CoreSight support enabled in your kernel config.
Ensure it is enabled with::

   CONFIG_CORESIGHT=y

There are various other CoreSight options you probably also want
enabled like::

   CONFIG_CORESIGHT_LINKS_AND_SINKS=y
   CONFIG_CORESIGHT_LINK_AND_SINK_TMC=y
   CONFIG_CORESIGHT_CATU=y
   CONFIG_CORESIGHT_SINK_TPIU=y
   CONFIG_CORESIGHT_SINK_ETBV10=y
   CONFIG_CORESIGHT_SOURCE_ETM4X=y
   CONFIG_CORESIGHT_CTI=y
   CONFIG_CORESIGHT_CTI_INTEGRATION_REGS=y

Please refer to the kernel configuration help for more information.

Perf test - Verify kernel and userspace perf CoreSight work
-----------------------------------------------------------

There are a set of Perf tests for CoreSight which can be run with::

  sudo perf test coresight
