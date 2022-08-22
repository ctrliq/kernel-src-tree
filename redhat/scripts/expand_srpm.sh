#!/bin/sh

# $1: cloned tree
cloned="$1";

cd "$cloned/$PACKAGE_NAME" || die "\"$cloned\" doesn't seem to have a dist-git clone";

# delete everything in the cloned tree to avoid having stale files
rm -r -- *

git reset HEAD -- sources
git checkout sources
echo "*.xz" > .gitignore
echo "*.bz2" >> .gitignore

# expand the srpm into the tree
rpm2cpio "$SRPM" | cpio -idmv

git add -A

