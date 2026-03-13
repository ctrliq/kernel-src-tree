#!/bin/sh

# This script should be run from the kernel-src-tree directory
# It reads the tarfile name from the spec file and creates the tarball

# Determine dist-git directory paths
# Assuming this script is in ciq/SOURCES/
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DISTGIT_ROOT="$(dirname "$SCRIPT_DIR")"
SOURCE_DIR="$DISTGIT_ROOT/SOURCES"
SPEC_FILE="$DISTGIT_ROOT/SPECS/kernel.spec"

# Extract version information from spec file
TARFILE_RELEASE=$(grep '^%define tarfile_release' "$SPEC_FILE" | awk '{print $3}')
SPEC_VERSION=$(grep '^%define specversion' "$SPEC_FILE" | awk '{print $3}')

if [ -z "$TARFILE_RELEASE" ]; then
    echo "Error: Could not extract tarfile_release from $SPEC_FILE"
    exit 1
fi

if [ -z "$SPEC_VERSION" ]; then
    echo "Error: Could not extract specversion from $SPEC_FILE"
    exit 1
fi

# Get current git tag and extract version
CURRENT_TAG=$(git describe --tags --abbrev=0 2>/dev/null)
if [ -z "$CURRENT_TAG" ]; then
    echo "Error: Could not determine current git tag"
    exit 1
fi

TAG_VERSION=${CURRENT_TAG/ciq_kernel-/}
GIT_VERSION=${TAG_VERSION%%-*}

# Verify that git version matches spec version
if [ "$GIT_VERSION" != "$SPEC_VERSION" ]; then
    echo "Error: Version mismatch!"
    echo "  Git version:  $GIT_VERSION (from tag: $CURRENT_TAG)"
    echo "  Spec version: $SPEC_VERSION"
    echo ""
    echo "Please either:"
    echo "  1. Run update_spec.sh to update the spec to match current git checkout, or"
    echo "  2. Check out the correct git tag that matches the spec version"
    exit 1
fi

TARBALL="$SOURCE_DIR/linux-$TARFILE_RELEASE.tar.zst"
ZSTD_THREADS="--threads=4"
ARCH=$(arch)
ZSTD_OPTIONS="-19"

if [ "$ARCH" != "x86_64" ]
then
        ZSTD_OPTIONS="-19 --long"
fi

# convert from shortened git sha to standard 40 digit git sha
_GITID="$(git rev-parse HEAD)"

if [ -f "$TARBALL" ]; then
	TARID=$(zstdcat -qq "$TARBALL" | git get-tar-commit-id 2>/dev/null)
	if [ "$_GITID" = "$TARID" ]; then
		echo "$(basename "$TARBALL") unchanged..."
		exit 0
	fi
	rm -f "$TARBALL"
fi

echo "Creating $(basename "$TARBALL")..."
trap 'rm -vf "$TARBALL"' INT
git archive --prefix="linux-$TARFILE_RELEASE"/ --format=tar "$_GITID" | zstd $ZSTD_OPTIONS $ZSTD_THREADS > "$TARBALL";

echo "Tarball created: $TARBALL"
