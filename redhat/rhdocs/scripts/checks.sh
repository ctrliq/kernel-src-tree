#!/bin/bash
set -e

function in_names()
{
	local name
	for name in "${names[@]}"; do
		[[ $1 != "$name" ]] || return 0
	done
	return 1
}

python3 scripts/owners-tool.py verify info/owners.yaml templates/owners-schema.yaml

if [[ $GITLAB_CI ]]; then
	git fetch origin main
fi
base=$(git merge-base origin/main HEAD) || {
	echo "ERROR: cannot determine merge base for HEAD."
	echo "This is a BUG in the check script. Please report this."
	exit 1
}
IFS=$'\n' names=($(git diff --name-only $base))
if in_names info/owners.yaml; then
	for name in "${names[@]}"; do
		[[ $name != info/owners.yaml && $name != info/RHMAINTAINERS &&
		   $name != info/CODEOWNERS ]] || continue
		echo "======================================================="
		echo "These changes include owners.yaml modifications together with other"
		echo "changes.  As a safeguard against mistakes, this is not allowed."
		echo
		echo "If you need to do infrastructure changes please submit two separate"
		echo "merge requests, one for owners.yaml and one for the rest."
		echo "======================================================="
		exit 1
	done
fi
if in_names info/owners.yaml && \
	test "$(git config --get owners.warning)" != "false"; then
	echo "======================================================="
	echo "These changes include owners.yaml modifications.  Please"
	echo "review these Merge Request Approval Rules.  The Merge Request"
	echo "author must add the appropriate engineers as reviewers on"
	echo "the submitted documentation project Merge Request."
	echo " "
	echo "* Included and excluded file changes can be merged if the"
	echo "MR author is a subsystem maintainer. If the author is not a"
	echo "subsystem maintainer, then the subsystem maintainer must"
	echo "provide an approve."
	echo " "
	echo "* Any MR adding an engineer in a role must be authored by"
	echo "or approved by the added engineer. An additional approve from a"
	echo "subsystem maintainer is required, unless the maintainer is the"
	echo "author of the MR."
	echo " "
	echo "* Any MR removing an engineer in a role must be authored by"
	echo "or approved by the removed engineer, except in the case when"
	echo "the removed engineer is no longer with Red Hat. While removals"
	echo "from roles do not require the approve of the maintainer, MR"
	echo "authors are encouraged to add the maintainer for an approve."
	echo " "
	echo "* Any MR adding or modifying a devel-sst field requires the"
	echo "approval from the subsystem maintainer."
	echo " "
	echo "This warning can be disabled by executing:"
	echo "        git config --add owners.warning false"
	echo "======================================================="
fi
if in_names templates/owners-schema.yaml && \
	test "$(git config --get ownersschema.warning)" != "false"; then
	echo "======================================================="
	echo "These changes include owners-schema modifications.  If you are"
	echo "changing SST names, you must ensure the SST names themselves, and"
	echo "the SST name changes in the file are approved by RHEL management."
	echo "If you are changing the format of owners.yaml, you must ensure"
	echo "the changes have been approved by the KWF (Kernel Workflow) group."
	echo "Changes that have failed to meet this will be removed by"
	echo "reverting commits."
	echo " "
	echo "This warning can be disabled by executing:"
	echo "        git config --add ownersschema.warning false"
	echo "======================================================="
fi
exit 0
