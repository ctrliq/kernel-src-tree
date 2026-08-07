#!/bin/sh

# This script should be run from the kernel-src-tree directory
# It reads the tarfile name from the spec file and creates the tarball

# Determine dist-git directory paths
# Assuming this script is in ciq/SOURCES/
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DISTGIT_ROOT="$(dirname "$SCRIPT_DIR")"
SOURCE_DIR="$DISTGIT_ROOT/SOURCES"
SPEC_FILE="$DISTGIT_ROOT/SPECS/kernel-clk6.18.spec"

# Extract version information from spec file
KERNEL_MAJOR_MINOR=$(grep '^%define kernel_major_minor' "$SPEC_FILE" | awk '{print $3}')
KERNEL_PATCH=$(grep '^%define kernel_patch' "$SPEC_FILE" | awk '{print $3}')
BUILDID=$(grep '^%define buildid' "$SPEC_FILE" | awk '{print $3}')
PKGRELEASE=$(grep '^%define pkgrelease' "$SPEC_FILE" | awk '{print $3}')

# el_version can be passed as first argument, or falls back to the buildroot's
# %{rhel} macro (matching the spec's %{!?el_version: %define el_version %{rhel}}).
if [ -n "$1" ]; then
    EL_VERSION="$1"
else
    EL_VERSION=$(rpm --eval '%{rhel}' 2>/dev/null)
    if [ -z "$EL_VERSION" ] || echo "$EL_VERSION" | grep -q '%{'; then
        echo "Error: Could not determine el_version."
        echo "Usage: $0 [el_version]"
        echo "  e.g. $0 9   or   $0 10"
        exit 1
    fi
fi

if [ -z "$KERNEL_MAJOR_MINOR" ] || [ -z "$KERNEL_PATCH" ] || [ -z "$PKGRELEASE" ] || [ -z "$EL_VERSION" ]; then
    echo "Error: Could not extract kernel version from $SPEC_FILE"
    exit 1
fi

# Resolve macros in pkgrelease
PKGRELEASE=$(echo "$PKGRELEASE" | sed "s/%{?buildid}/${BUILDID}/")

SPEC_VERSION="${KERNEL_MAJOR_MINOR}.${KERNEL_PATCH}"
TARFILE_RELEASE="${SPEC_VERSION}-${PKGRELEASE}.el${EL_VERSION}"

# Get current git tag and extract version
CURRENT_TAG=$(git describe --tags --abbrev=0 2>/dev/null)
if [ -z "$CURRENT_TAG" ]; then
    echo "Error: Could not determine current git tag"
    exit 1
fi

TAG_VERSION=${CURRENT_TAG/ciq_kernel-/}
TAG_VERSION=${TAG_VERSION#v}
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

set -e

ZSTD_CMD=$(which zstd 2>/dev/null) || { echo "Error: zstd not found. Install it with: dnf install zstd"; exit 1; }

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
	TARID=$("$ZSTD_CMD" -d -c -qq "$TARBALL" | git get-tar-commit-id 2>/dev/null)
	if [ "$_GITID" = "$TARID" ]; then
		echo "$(basename "$TARBALL") unchanged..."
		exit 0
	fi
	rm -f "$TARBALL"
fi

echo "Creating $(basename "$TARBALL")..."
trap '[ $? -ne 0 ] && rm -vf "$TARBALL"' EXIT
git archive --prefix="linux-$TARFILE_RELEASE"/ --format=tar "$_GITID" | "$ZSTD_CMD" $ZSTD_OPTIONS $ZSTD_THREADS > "$TARBALL";

echo "Tarball created: $TARBALL"
