#!/usr/bin/env bats

@test "Makefile variable declarations" {
	# By design, only the Makefile.variables file should have ?= declarations

	value=$(git grep "?=" Makefile.variables | wc -l)
	if [ $value -eq 0 ]; then
		echo "Test failed: No ?= variables found in Makefile.variables"
		exit 1
	fi

	value=$(git grep "?=" Makefile | grep -v "\"?=" | wc -l)
	if [ $value -gt 0 ]; then
		echo "Test failed: Makefile should not ?= declarations."
		exit 1
	fi
}
