#!/usr/bin/env bats
# Purpose: This test creates a set of Makefile variables, and a set of
# variables that are used in the specfile.  This data is diff'd against a
# "known good" set of data and if there is a difference an error is reported.

load test-lib.bash

@test "self-test-data check" {
	mkdir -p $BATS_TMPDIR/data
	RHDISTDATADIR=$BATS_TMPDIR/data make dist-self-test-data

	redhat=$(make dist-dump-variables | grep "REDHAT=" | cut -d"=" -f2 | xargs)

	echo "Diffing directories ${redhat}/self-test/data and $BATS_TMPDIR/data"
	run diff -urNp -x create-data.sh ${redhat}/self-test/data $BATS_TMPDIR/data
	[ -d $BATS_TMPDIR ] && rm -rf $BATS_TMPDIR/data
	check_status
}
