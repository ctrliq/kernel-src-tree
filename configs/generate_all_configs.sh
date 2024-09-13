#!/bin/sh

# Adjusts the configuration options to build the variants correctly

test -n "$RHTEST" && exit 0

DEBUGBUILDSENABLED=$1
if [ -z "$DEBUGBUILDSENABLED" ]; then
	exit 1
fi

if [ -z "$FLAVOR" ]; then
	FLAVOR=rhel
fi

if [ "$FLAVOR" = "fedora" ]; then
	SECONDARY=rhel
else
	SECONDARY=fedora
fi

for i in kernel-rt-*-"$FLAVOR".config; do
	NEW=kernel-rt-"$SPECVERSION"-$(echo "$i" | cut -d - -f3- | sed s/-"$FLAVOR"//)
	#echo $NEW
	mv "$i" "$NEW"
done

rm -f kernel-rt-*-"$SECONDARY".config

if [ "$DEBUGBUILDSENABLED" -eq 0 ]; then
	for i in kernel-rt-*debug*.config; do
		base=$(echo "$i" | sed -r s/-?debug//g)
		NEW=kernel-rt-$(echo "$base" | cut -d - -f3-)
		mv "$i" "$NEW"
	done
fi
