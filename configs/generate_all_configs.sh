#!/bin/sh

for i in ${NAME}-*.config; do
	NEW=${NAME}-${VERSION}-$(echo $i | sed -e "s/${NAME}//" | cut -d - -f2-)
	mv ${i} ${NEW}
done
