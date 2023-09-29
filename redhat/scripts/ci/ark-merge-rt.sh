#!/bin/bash
#
# This script is intended to sync up the RT and automotive branch (derivative
# of RT).  It adds the extra twist of detecting the right upstream rt branch
# to sync with depending on the existance of the next branch.  Sometimes the
# rt-devel branch waits until -rc1/2 to create new branches.
# Finally the code handles the rebases in those cases where newer branches
# are available.
#
# Why the complexity?
# Development branches will need to be periodically rebased unfortunately.
# Using 'git rebase --onto <new head> <old_head>' only works with one common
# branch to rebase from not two.  Meanwhile, the -devel branches are formed
# from two upstream branches, os-build and linux-rt-devel.  The idea is
# to merge the two branches into a single throwaway branch that can be
# recreated from scratch anytime then use that as the base for -devel.

set -e

# source common CI functions and variables
# shellcheck disable=SC1091
. "$(dirname "$0")"/ark-ci-env.sh

#Upstream RT tree git://git.kernel.org/pub/scm/linux/kernel/git/rt/linux-rt-devel.git
UPSTREAM_RT_TREE_URL="git://git.kernel.org/pub/scm/linux/kernel/git/rt/linux-rt-devel.git"
UPSTREAM_RT_TREE_NAME="linux-rt-devel"
DOWNSTREAM_RT_BRANCH="master-rt-devel"
RT_AUTOMATED_BRANCH="os-build-rt-automated"
RT_DEVEL_BRANCH="os-build-rt-devel"
AUTOMOTIVE_DEVEL_BRANCH="os-build-automotive-devel"

# verify git remote rt is setup
if ! git remote get-url "$UPSTREAM_RT_TREE_NAME" 2>/dev/null; then
	die "Please 'git remote add linux-rt-devel $UPSTREAM_RT_TREE_URL'"
fi

# grab the os-build base branches
ark_git_mirror "os-build" "origin" "os-build"
ark_git_mirror "master" "origin" "master"

# make sure tags are available for git-describe to correctly work
git fetch -t origin

# upstream -rt devel branches are aligned with version numbers and are not
# always up to date with master.  Figure out which branch to mirror based on
# version number and existance.  We may have to trigger a rebase.

# what are the current versions of rt-devel and os-build (use 'master' to
# avoid fedora tagging of kernel-X.Y.0.0.....)
# use git tags which are always 'vX.Y-rcZ-aaa-gbbbbb' or 'vX.Y-aaa-gbbbbb'
# where X.Y is the version number that maps to linux-rt's branch of
# 'linux-X.Y.y'
get_upstream_version()
{
	branch=$1
	upstream="master"

	# Thanks to pre-rc1 merging, we have a 2 week window with an
	# incorrect version number.  Detect and correct.
	mergebase="$(git merge-base "$branch" "$upstream")"
	raw_version="$(git describe "$mergebase")"
	version="$(git show "$branch":Makefile | sed -ne '/^VERSION\ =\ /{s///;p;q}')"
	patchlevel="$(git show "$branch":Makefile | sed -ne '/^PATCHLEVEL\ =\ /{s///;p;q}')"
	kver="${version}.${patchlevel}"

	#-rc indicates no tricks necessary, return version
	if echo "${raw_version}" | grep -q -- "-rc"; then
		echo "$kver"
		return
	fi

	#if -gXXX is _not_ there, must be a GA release, use version
	if ! echo "${raw_version}" | grep -q -- "-g"; then
		echo "$kver"
		return
	fi

	#must be a post tag release with -g but not -rcX, IOW an rc0.
	#Add a 1 to the version number
	echo "${version}.$((patchlevel + 1))"
}

# To handle missing branches, precalculate previous kernel versions to fetch
get_prev_version()
{
	version_str=$1

	version="$(echo "$version_str" | cut -c1)"
	patchlevel="$(echo "$version_str" | cut -c3)"

	echo "${version}.$((patchlevel - 1))"
}

OS_BUILD_VER="$(get_upstream_version os-build)"
OS_BUILD_VER_prev="$(get_prev_version "$OS_BUILD_VER")"

# check latest upstream RT branch
if git fetch -q "$UPSTREAM_RT_TREE_NAME" "linux-${OS_BUILD_VER}.y-rt"; then
	UPSTREAM_RT_DEVEL_VER="${OS_BUILD_VER}"
elif git fetch -q "$UPSTREAM_RT_TREE_NAME" "linux-${OS_BUILD_VER_prev}.y-rt"; then
	UPSTREAM_RT_DEVEL_VER="${OS_BUILD_VER_prev}"
else
	die "Neither version ($OS_BUILD_VER, $OS_BUILD_VER_prev) in upstream tree: $UPSTREAM_RT_TREE_NAME"
fi

# verify the core branches exist or use provided defaults
UPSTREAM_RT_DEVEL_BRANCH="linux-${UPSTREAM_RT_DEVEL_VER}.y-rt"
ark_git_branch "$DOWNSTREAM_RT_BRANCH" "$UPSTREAM_RT_TREE_NAME/$UPSTREAM_RT_DEVEL_BRANCH"
ark_git_branch "$RT_AUTOMATED_BRANCH" "$UPSTREAM_RT_TREE_NAME/$UPSTREAM_RT_DEVEL_BRANCH"
ark_git_branch "$RT_DEVEL_BRANCH" "$UPSTREAM_RT_TREE_NAME/$UPSTREAM_RT_DEVEL_BRANCH"
ark_git_branch "$AUTOMOTIVE_DEVEL_BRANCH" "$UPSTREAM_RT_TREE_NAME/$UPSTREAM_RT_DEVEL_BRANCH"

MASTER_RT_DEVEL_VER="$(get_upstream_version "$DOWNSTREAM_RT_BRANCH")"
RT_AUTOMATED_VER="$(get_upstream_version $RT_AUTOMATED_BRANCH)"
RT_DEVEL_VER="$(get_upstream_version $RT_DEVEL_BRANCH)"
AUTOMOTIVE_DEVEL_VER="$(get_upstream_version $AUTOMOTIVE_DEVEL_BRANCH)"

OS_BUILD_BASE_BRANCH="os-build"
RT_REBASE=""

if test "$UPSTREAM_RT_DEVEL_VER" != "$OS_BUILD_VER"; then
	# no newer upstream branch to rebase onto, continue with an
	# os-build stable tag
	OS_BUILD_BASE_BRANCH="kernel-${MASTER_RT_DEVEL_VER}.0-0"
fi

# sanity check, sometimes broken scripts leave a mess
if test "$MASTER_RT_DEVEL_VER" != "$UPSTREAM_RT_DEVEL_VER" -o \
	"$MASTER_RT_DEVEL_VER" != "$RT_AUTOMATED_VER" -o \
	"$MASTER_RT_DEVEL_VER" != "$RT_DEVEL_VER" -o \
	"$MASTER_RT_DEVEL_VER" != "$AUTOMOTIVE_DEVEL_VER"; then
	# rebase time
	RT_REBASE="yes"
fi

## PREP the upstream branches
# on a rebase, propogate all the git resets
# fetch the determined rt-devel branch
ark_git_mirror "$DOWNSTREAM_RT_BRANCH" "$UPSTREAM_RT_TREE_NAME" "$UPSTREAM_RT_DEVEL_BRANCH" "$RT_REBASE"
# finally merge the two correct branches
ark_git_merge "$OS_BUILD_BASE_BRANCH" "$RT_AUTOMATED_BRANCH" "$RT_REBASE"
ark_git_merge "$DOWNSTREAM_RT_BRANCH" "$RT_AUTOMATED_BRANCH"

## MERGE the upstream branches to the development branches
if test -n "$RT_REBASE"; then
	# handle the rebase
	# rebases usually go from prev version to new version
	# rebuild the prev merge base in case the previous automated one is
	# corrupted.
	prev_branch="$(git rev-parse --abbrev-ref HEAD)"
	temp_branch="_temp_rt_devel_$(date +%F)"
	git branch -D "$temp_branch" 2>/dev/null
	git checkout -b "$temp_branch" "kernel-${OS_BUILD_VER_prev}.0-0"
	git merge "$UPSTREAM_RT_TREE_NAME/linux-${OS_BUILD_VER_prev}.y-rt"
	git checkout "$prev_branch"
	ark_git_rebase "$RT_DEVEL_BRANCH" "$temp_branch" "$RT_AUTOMATED_BRANCH"
	ark_git_rebase "$AUTOMOTIVE_DEVEL_BRANCH" "$temp_branch" "$RT_AUTOMATED_BRANCH"
	git branch -D "$temp_branch"
fi

## Build -rt-devel branch, generate pending-rhel configs
ark_git_merge "$RT_AUTOMATED_BRANCH" "$RT_DEVEL_BRANCH"
# don't care if configs were added or not hence '|| true'
ark_update_configs "$RT_DEVEL_BRANCH" || true
# skip pushing config update MRs, keep them in pending-rhel
ark_push_changes "$RT_DEVEL_BRANCH" "skip"

## Build -automotive-devel branch, generate pending-rhel configs
ark_git_merge "$RT_AUTOMATED_BRANCH" "$AUTOMOTIVE_DEVEL_BRANCH"
# don't care if configs were added or not hence '|| true'
ark_update_configs "$AUTOMOTIVE_DEVEL_BRANCH" || true
# skip pushing config update MRs, keep them in pending-rhel
ark_push_changes "$AUTOMOTIVE_DEVEL_BRANCH" "skip"
