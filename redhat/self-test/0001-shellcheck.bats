#!/usr/bin/env bats
# Purpose: This test runs shellcheck on all .sh files in the redhat directory.

@test "shellcheck" {
    if ! test -x /usr/bin/shellcheck
    then
        skip "The ShellCheck package is not installed"
    fi
    shellcheck $(find $BATS_TEST_DIRNAME/.. -name "*.sh" -not -path "$BATS_TEST_DIRNAME/../rpm/*")
}
