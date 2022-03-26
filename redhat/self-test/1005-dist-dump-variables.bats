#!/usr/bin/env bats

@test "self-test-data check" {
	mkdir -p $BATS_TMPDIR/data
	RHDISTDATADIR=$BATS_TMPDIR/data make dist-self-test-data

	redhat=$(make dist-dump-variables | grep "REDHAT=" | cut -d"=" -f2 | xargs)

	echo "Diffing directories ${redhat}/self-test/data and $BATS_TMPDIR/data"
	diff -urNp -x create-data.sh ${redhat}/self-test/data $BATS_TMPDIR/data
	[ -d $BATS_TMPDIR ] && rm -rf $BATS_TMPDIR/data
}
