#!/usr/bin/env bats
@test "rpminspect" {
    if ! test -x /usr/bin/rpminspect
    then
        skip "The rpminspect package is not installed"
    else
        skip "Skip rpminspect test pending fixes"
    fi

    numsrpms=$(find "$BATS_TEST_DIRNAME"/.. -name "*.rpm" | wc -l)
    if [ "$numsrpms" != "1" ]; then
	skip "Only one SRPM should be in $BATS_TEST_DIRNAME/redhat/rpms/SRPMS."
    fi

    srpm=$(find "$BATS_TEST_DIRNAME"/.. -name "*.rpm")
    run rpminspect $srpm
    [ "$status" = 0 ]
}
