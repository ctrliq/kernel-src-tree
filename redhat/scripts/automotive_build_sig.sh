#!/bin/bash

# clones and updates an automotive sig repo
# $1: branch to be used
# $2: local pristine clone of auto-sig
# $3: alternate tmp directory (if you have faster storage)
# $4: kernel source tarball
# $5: package name
# shellcheck disable=SC2164

autosig_branch=$1;
autosig_cache=$2;
autosig_tmp=$3;
autosig_tarball=$4;
package_name=$5;

redhat=$(dirname "$0")/..;
topdir="$redhat"/..;

function die
{
	echo "Error: $1" >&2;
	exit 1;
}

function upload_sources()
{
	# Uploading the tarball only using CentOS tools
	echo "Cloning the centos common repository";
	git clone https://git.centos.org/centos-git-common.git centos-git-common >/dev/null || die "Unable to clone centos tools";
	./centos-git-common/lookaside_upload -f $autosig_tarball -n $package_name -b $autosig_branch;
}

function update_patches()
{
	# Cloning the source reference repo
	echo "Cloning $package_name source rpm repository";
	git clone -b $autosig_branch ssh://git@git.centos.org/rpms/$package_name $package_name >/dev/null || die "Unable to clone using local cache";

	# Copy all the new sources (including SPEC) except the big tarball, but use it as a reference
	find "$redhat"/rpm/SOURCES/ \( ! -name "*${autosig_tarball#*.}" \) -type f | xargs cp -t "${package_name}/SOURCES/";
	mv "${package_name}/SOURCES/kernel.spec" "${package_name}/SPECS/";
	tarball_sha=($(sha1sum $autosig_tarball));
	tarball_name=$(basename -- "$autosig_tarball");

	# Update tarball metada
	echo "$tarball_sha SOURCES/$tarball_name" > "${package_name}/.kernel-auto.metadata";
	pushd "$package_name" &> /dev/null;

	# Check for changes and commit/push them
	source_changes=$(git status --porcelain | cut -b4-);
	echo $source_changes
	if [ -z "$source_changes" ];
	then
		die "Nothing has changed.";
	fi
	git commit -a -s -m"$(echo -e "[redhat] Update sources for $tarball_name\n\nChanges included into this commit:\n$source_changes")" || die "Unable to commit the changes";
	git push origin $autosig_branch || die "Unable to push the changes"
	kernel_auto_rev=$(git log -1 --pretty=format:%H);
	popd &> /dev/null;
	sed -i -e "s/^AUTOGITCOMMIT:=[^ ]*/AUTOGITCOMMIT:=$kernel_auto_rev/" $redhat/Makefile.auto;
}

# Sanity check of global git variables set
git config --list --global | grep -q "user\.name" && git config --list --global | grep -q "user\.email" || die "Configure your username and email into global git settings";

# Create a stagging repository
date=$(date +"%Y-%m-%d");
tmp="$(mktemp -d --tmpdir="$autosig_tmp" AUTOSIG."$date".XXXXXXXX)";

cd "$tmp" || die "Unable to create temporary directory";

# upload the sources into centos repos
upload_sources || die "Unable to upload the sources"

# update the centos sources into package repo
update_patches || die "Unable to copy the patches";

# all done
echo "IMPORTANT: Remember to commit your changes here...";
