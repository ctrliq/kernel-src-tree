#!/bin/bash

LAST_MARKER=$(cat "${REDHAT}"/marker)
clogf="$SOURCES/changelog"
# hide [redhat] entries from changelog
HIDE_REDHAT=1;
# hide entries for unsupported arches
HIDE_UNSUPPORTED_ARCH=1;
# override LC_TIME to avoid date conflicts when building the srpm
LC_TIME=

GIT_FORMAT="--format=- %s (%an)%n%N%n^^^NOTES-END^^^%n%b"
GIT_NOTES="--notes=refs/notes/${RHEL_MAJOR}.${RHEL_MINOR}*"

lasttag=$(git rev-list --first-parent --grep="^\[redhat\] kernel-${SPECKVERSION}.${SPECKPATCHLEVEL}" --max-count=1 HEAD)
# if we didn't find the proper tag, assume this is the first release
if [[ -z $lasttag ]]; then
    if [[ -z ${MARKER//[0-9a-f]/} ]]; then
        # if we're doing an untagged release, just use the marker
        echo "Using $MARKER"
        lasttag=$MARKER
    else
	lasttag=$(git describe --match="$MARKER" --abbrev=0)
    fi
fi
echo "Gathering new log entries since $lasttag"
# master is expected to track mainline.

cname="$(git var GIT_COMMITTER_IDENT |sed 's/>.*/>/')"
cdate="$(LC_ALL=C date +"%a %b %d %Y")"
cversion="[$BASEVERSION]";
echo "* $cdate $cname $cversion" > "$clogf"

git log --topo-order --no-merges -z "$GIT_NOTES" "$GIT_FORMAT" \
	^"${UPSTREAM}" "$lasttag".. -- ':!/redhat/rhdocs' | "${0%/*}"/genlog.py >> "$clogf"

if [ "$HIDE_REDHAT" = "1" ]; then
	grep -v -e "^- \[redhat\]" "$clogf" |
		sed -e 's!\[Fedora\]!!g' > "$clogf.stripped"
	cp "$clogf.stripped" "$clogf"
fi

if [ "$HIDE_UNSUPPORTED_ARCH" = "1" ]; then
	grep -E -v "^- \[(alpha|arc|arm|avr32|blackfin|c6x|cris|frv|h8300|hexagon|ia64|m32r|m68k|metag|microblaze|mips|mn10300|openrisc|parisc|score|sh|sparc|tile|um|unicore32|xtensa)\]" "$clogf" > "$clogf.stripped"
	cp "$clogf.stripped" "$clogf"
fi

# If the markers aren't the same then this a rebase.
# This means we need to zap entries that are already present in the changelog.
if [ "$MARKER" != "$LAST_MARKER" ]; then
	# awk trick to get all unique lines
	awk '!seen[$0]++' "$SOURCES/$SPECCHANGELOG" "$clogf" > "$clogf.unique"
	# sed trick to get the end of the changelog minus the line
	sed -e '1,/# END OF CHANGELOG/ d' "$clogf.unique" > "$clogf.tmp"
	# Add an explicit entry to indicate a rebase.
	echo "" > "$clogf"
	echo -e "- $MARKER rebase" | cat "$clogf.tmp" - >> "$clogf"
	rm "$clogf.tmp" "$clogf.unique"
fi

# HACK temporary hack until single tree workflow
# Don't reprint all the ark-patches again.
if [ -n "$(git log --oneline --first-parent --grep="Merge ark patches" "$lasttag"..)" ]; then
	# Throw away the clogf and just print the summary merge
	echo "" > "$clogf"
	echo "- Merge ark-patches" >> "$clogf"
fi

# during rh-dist-git genspec runs again and generates empty changelog
# create empty file to avoid adding extra header to changelog
LENGTH=$(grep -c "^-" "$clogf" | awk '{print $1}')
if [ "$LENGTH" = 0 ]; then
	rm -f "$clogf"
	touch "$clogf"
fi

cat "$clogf" "$SOURCES/$SPECCHANGELOG" > "$clogf.full"
mv -f "$clogf.full" "$SOURCES/$SPECCHANGELOG"

# genlog.py generates Resolves lines as well, strip these from RPM changelog
grep -v -e "^Resolves: " "$SOURCES/$SPECCHANGELOG" > "$clogf".stripped

test -f "$SOURCES/$SPECFILE" &&
	sed -i -e "
	/%%SPECCHANGELOG%%/r $clogf.stripped
	/%%SPECCHANGELOG%%/d" "$SOURCES/$SPECFILE"

echo "MARKER is $MARKER"

rm -f "$clogf"{,.stripped};
