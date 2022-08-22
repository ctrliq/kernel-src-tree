#!/bin/bash

# Clones a RHEL dist-git tree using a local reference if existent

function die
{
	echo "Error: $1" >&2;
	exit 1;
}

date=$(date +"%Y-%m-%d")
tmp="$(mktemp -d --tmpdir="$RHDISTGIT_TMP" RHEL"$RHEL_MAJOR"."$date".XXXXXXXX)";
cd "$tmp" || die "Unable to create temporary directory";

if [[ -n $RHDISTGIT && -n $RHDISTGIT_CACHE ]]; then
	git clone --reference "$RHDISTGIT_CACHE" "$RHDISTGIT" "$PACKAGE_NAME" >/dev/null || die "Unable to clone using local cache";
	# if there're tarballs present that are listed in the "sources" file,
	# copy them or it'll be downloaded again
	if [ -e "$RHDISTGIT_CACHE/sources" ]; then
		while IFS= read -r i; do
			if [ -f "$RHDISTGIT_CACHE/$i" ]; then
				cp "$RHDISTGIT_CACHE/$i" "$tmp/kernel/";
			fi
		done < "$RHDISTGIT_CACHE"/sources
	fi
else
	echo "No local repo, cloning using $RHPKG_BIN" >&2;
	$RHPKG_BIN clone "$PACKAGE_NAME" >/dev/null || die "Unable to clone using $RHPKG_BIN";
fi

echo "$tmp";

