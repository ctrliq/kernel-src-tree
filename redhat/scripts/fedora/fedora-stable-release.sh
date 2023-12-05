#!/bin/bash

# shellcheck disable=all

releasenum=$1

# Releasenum is really just the last digit as we have series based on Fedora Release
# by default we assume 0, but if you need to do a rebuild with a fix before a new release
# you can call the script with a 1 or whatever digit you wish for the releasenum.
if [ -z "$releasenum" ]; then
	releasenum="0"
fi

klist -s
if [ ! $? -eq 0 ]; then
	echo "klist couldn't read the credential cache."
	echo "Do you need to fix your kerberos tokens?"
	exit 1
fi


# Occasionally we have a patch which  needs to be applied only to a specific
# Fedora release for whatever reason.  If this is the case, we can set
# ApplyPatches="1" under that releases case switch and we will apply any
# patches from redhat/patches for that release only.
ApplyPatches="0"

for release in $( cat redhat/release_targets );  do 
	case "$release" in
	40) build=30$releasenum
	    ;;
	39) build=20$releasenum
	    ;;
	38) build=10$releasenum
	    ;;
	esac
	if [[ $ApplyPatches == "1" ]] ; then
		for patch in redhat/patches/* ; do patch -p1 < $patch ; done
	fi
	make IS_FEDORA=1 DIST=".fc$release" BUILDID="" BUILD=$build RHDISTGIT_BRANCH=f$release dist-git;
	sleep 60;
	git checkout .
done
