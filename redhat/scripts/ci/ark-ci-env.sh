#! /bin/sh

die()
{
	echo "$1"
	exit 1
}

ci_pre_check()
{
	if test -n "${TO_PUSH}" && test -z "${GITLAB_URL}"; then
                die "Please run 'git remote add gitlab <url>' to enable git-push."
        fi
        git diff-index --quiet HEAD || die "Dirty tree, please clean before merging."
}

# Common variables for all CI scripts
UPSTREAM_REF=${1:-"master"}
BRANCH=${2:-"os-build"}
PROJECT_ID=${PROJECT_ID:-"13604247"}
TO_PUSH=${DIST_PUSH:-""}
GITLAB_URL="$(git remote get-url gitlab 2>/dev/null)"

ci_pre_check

export UPSTREAM_REF
export BRANCH
export PROJECT_ID
export TO_PUSH
export GITLAB_URL
