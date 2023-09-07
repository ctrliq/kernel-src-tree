#! /bin/sh

die()
{
	echo "$1"
	exit 1
}

ci_pre_check()
{
	if test -n "${TO_PUSH}"; then
		if test -z "${GITLAB_PROJECT_URL}" || test -z "$GITLAB_PROJECT_PUSHURL"; then
	                echo "To enable git-push, please run:"
			echo "git remote add gitlab <url>"
			echo "git remote set-url --push gitlab <pushurl>"
			die "Misconfigured 'gitlab' entry for git"
		fi
        fi
        git diff-index --quiet HEAD || die "Dirty tree, please clean before merging."
}

# Common variables for all CI scripts
UPSTREAM_REF=${1:-"master"}
BRANCH=${2:-"os-build"}
PROJECT_ID=${PROJECT_ID:-"13604247"}
TO_PUSH=${DIST_PUSH:-""}
GITLAB_PROJECT_URL="$(git remote get-url gitlab 2>/dev/null)"
GITLAB_PROJECT_PUSHURL="$(git config --get remote.gitlab.pushurl 2>/dev/null)"

ci_pre_check

export UPSTREAM_REF
export BRANCH
export PROJECT_ID
export TO_PUSH
export GITLAB_PROJECT_URL
export GITLAB_PROJECT_PUSHURL
