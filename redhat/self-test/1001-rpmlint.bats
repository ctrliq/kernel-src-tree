#!/usr/bin/env bats
@test "rpmlint" {
    if ! test -x /usr/bin/rpmlint
    then
        skip "The rpmlint package is not installed"
    else
        skip "Skip rpmlint test pending kernel.spec.template changes"
    fi

    numsrpms=$(find "$BATS_TEST_DIRNAME"/.. -name "*.rpm" | wc -l)
    if [ "$numsrpms" != "1" ]; then
	skip "Only one SRPM should be in $BATS_TEST_DIRNAME/redhat/rpms/SRPMS."
    fi

    srpm=$(find "$BATS_TEST_DIRNAME"/.. -name "*.rpm")
    run rpmlint $srpm
    status=$?
    [ "$status" = 0 ]
}
