#!/usr/bin/bash

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

[ -z "${RHDISTDATADIR}" ] && echo "ERROR: RHDISTDATADIR undefined." && exit 1

# Store variables used in *this* script before unsetting them below.
destdir="${RHDISTDATADIR}"
specfile="${SOURCES}"/kernel.spec

# unset all redhat/Makefile variables so they do not interfere with make targets below
makefile_vars=$(unset SINGLE_TARBALL; make dist-dump-variables | grep "=" | cut -d"=" -f1)
while read -r VAR; do unset "$VAR"; done < <(echo "$makefile_vars")

for DISTRO in fedora rhel centos
do
	for commit in 78e36f3b0dae 2585cf9dfaad df0cc57e057f fce15c45d3fb
	do
		for DIST in .fc25 .el7
		do
			varfilename="${destdir}/${DISTRO}-${commit}${DIST}"

			echo "building $varfilename"

			# Ignored Makefile variables:
			# CURDIR is a make special target and cannot be easily changed.
			# UPSTREAM is the base merge commit and can change from day-to-day as
			# the tree is changed.
			# RHEL_RELEASE can change build-to-build.
			# SHELL can change depending on user's environment
			# RHGITURL may change depending on the user's method of cloning
			# RHDISTDATADIR will change based on these tests
			# VARS is a list of variables added for the 'dist-dump-variables' target
			# and can be ignored.
			make RHSELFTESTDATA=1 DIST="${DIST}" DISTRO="${DISTRO}" HEAD=${commit} dist-dump-variables | grep "=" |\
				grep -v -w CURDIR |\
				grep -v -w UPSTREAM |\
				grep -v -w RHEL_RELEASE |\
				grep -v -w SHELL |\
				grep -v -w RHGITURL |\
				grep -v -w RHDISTDATADIR |\
				grep -v -w VARS |\
				sort -u >& "${varfilename}"

			echo "building ${varfilename}.spec"
			make RHSELFTESTDATA=1 DIST="${DIST}" DISTRO="${DISTRO}" HEAD=${commit} setup-source
			 grep -Fvx -f "./kernel.spec.template" "$specfile" > "${varfilename}".spec
		done
	done
done

exit 0
