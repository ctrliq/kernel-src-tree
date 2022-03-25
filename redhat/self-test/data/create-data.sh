#!/usr/bin/bash

target_dir=$1
[ -z "${target_dir}" ] && echo "Missing target directory." && exit 1

# This script generates 'dist-dump-variables' output for various configurations
# using known ark commit IDs.  It uses this information as well as setting
# different values for DISTRO and DIST.
#
# The ark commit IDs are
#
#    78e36f3b0dae := 5.17.0 merge window (5.16 + additional changes before -rc1)
#    2585cf9dfaad := 5.16-rc5
#    df0cc57e057f := 5.16
#    fce15c45d3fb := 5.16-rc5 + 2 additional commits
#

[ -z "$REDHAT" ] && echo "ERROR: execution only supported through 'make dist-self-test-data'" && exit 1

for DISTRO in fedora rhel centos
do
	for commit in 78e36f3b0dae 2585cf9dfaad df0cc57e057f fce15c45d3fb
	do
		for DIST in .fc25 .el7
		do
			varfilename="${DISTRO}-${commit}${DIST}"

			echo "building $varfilename"

			# CURDIR is a make special target and cannot be easily changed.  Omit
			# CURDIR from the output.
			make RHSELFTESTDATA=1 DIST="${DIST}" DISTRO="${DISTRO}" HEAD=${commit} dist-dump-variables | grep "=" | grep -v CURDIR >& "${target_dir}"/"${varfilename}"

			# When executed from a script, the variables in Makefile.variables are
			# listed as having origin 'environment'.  This is because the script
			# inherits the variables from the 'export' command in the redhat/Makefile.
			# The 'dist-dump-variables' target explicitly omits these variables from
			# its output.  As a workaround, read in the variables and output them to
			# the data file.
			# shellcheck disable=SC2002
			cat Makefile.variables | grep -v "^#" | sed '/^$/d' | tr -d " " | awk -F "?=|:=" '{print $1}' | while read -r VAR; do echo "$VAR=${!VAR}"; done >> "${target_dir}"/"${varfilename}"
		done
	done
done

exit 0
