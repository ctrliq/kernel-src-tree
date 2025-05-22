Requirements and applicability
==============================

The whole testing logic is implemented in the ``test_user_copy.ko``
module, with the "pass" criterion in ``test_user_copy.sh`` script
(realizing the collection's only test ``user:test_user_copy.sh``) being
simply whether the module loaded successfully with ``modprobe`` or not.
To be available it requires ``CONFIG_TEST_USER_COPY`` option, which is
disabled in all Rocky versions LTS 8.6, 8.8, 9.2, 9.4. As such the test
can be omitted when testing default kernel builds.
