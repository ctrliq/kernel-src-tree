.. _rh_waived_items:

====================
Red Hat Waived Items
====================

Waived Items is a mechanism offered by Red Hat which allows customers to "waive"
and utilize features that are not enabled by default as these are considered as
unmaintained, insecure, rudimentary, or deprecated, but are shipped with the
RHEL kernel for customer's convinience only.
Waived Items can range from features that can be enabled on demand to specific
security mitigations that can be disabled on demand.

To explicitly "waive" any of these items, RHEL offers the ``rh_waived``
kernel boot parameter. To allow set of waived items, append
``rh_waived=<item name>,...,<item name>`` to the kernel
cmdline.
Appending ``rh_waived=features`` will waive all features listed below,
and appending ``rh_waived=cves`` will waive all security mitigations
listed below.

The waived items listed in the next session follow the pattern below:

- item name
        item description

List of Red Hat Waived Items
============================

