#!/bin/bash
#
# Generate a release tag and, if based on a tagged upstream release, a set
# of release branches. This script will rebase the ark-patches branch, update
# os-build with the latest configurations, and apply any merge requests labeled
# with "Include in Releases".

set -e

# source common CI functions and variables
# shellcheck source=./redhat/scripts/ci/ark-ci-env.sh
. "$(dirname "$0")"/ark-ci-env.sh

git checkout "${BRANCH}"
touch localversion
make dist-release

# prep ark-latest branch
git checkout --detach "${BRANCH}" && git describe

MR_PATCHES=$(gitlab project-merge-request list --project-id="$PROJECT_ID" \
	--labels="Include in Releases" --state=opened | grep -v "^$" | sort | \
	awk '{ print "https://gitlab.com/cki-project/kernel-ark/-/merge_requests/" $2 ".patch" }')
for patch_url in $MR_PATCHES; do
	curl -sL "$patch_url" | git am
done

make dist-release
make dist-release-tag
RELEASE=$(git describe)
git checkout ark-latest
git reset --hard "$RELEASE"

# Update ark-infra branch
git checkout ark-infra

# Using ark-latest because it has latest fixes
rm -rf makefile Makefile.rhelver redhat/
git archive --format=tar ark-latest makefile Makefile.rhelver redhat/ | tar -x

# Manually add hook instead of cherry-pick
# Add to middle to avoid git merge conflicts
# NOTE: commented out but left for future info to rebuild from scratch
# sed -i '/# We are using a recursive / i include Makefile.rhelver\n' Makefile

git add makefile Makefile.rhelver Makefile redhat
# Future rebuid note, .gitkeep files are gitignored and need force adding
# git add -f redhat/kabi/kabi-module/kabi*

git commit -m "bulk merge ark-infra as of $(date)"

printf "All done!

To push all the release artifacts, run:

git push os-build
for branch in \$(git branch | grep configs/\"\$(date +%%F)\"); do
\tgit push -o merge_request.create -o merge_request.target=os-build\
 -o merge_request.remove_source_branch upstream \"\$branch\"
done
git push upstream %s%s
git push -f upstream ark-latest\n" "$RELEASE" "$RELEASE_BRANCHES"
