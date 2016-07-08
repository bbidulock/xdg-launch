#!/bin/bash

# A little script to automagically generate a GNU NEWS file from git.

if [ -z "$PACKAGE" -a -f configure.template ]; then
	PACKAGE=$(grep AC_INIT configure.template|sed -r 's,AC_INIT[(][[],,;s,[]].*,,')
fi

output=()
o=

for t in $(git tag); do
	date=$(git show -s --format=%ci "$t"|awk '{print$1}')
	title="Release ${PACKAGE}-$t released $date"
	under=$(echo "$title"|sed 's,.,-,g')
	cmd="git shortlog -e -n -w80,6,8 ${o}${t}"
	detail=$(eval "$cmd")
	log="\n$title\n$under\n\n$cmd\n\n$detail\n\n"
	output=("$log" "${output[@]}")
	o="${t}..."
done

head=$(git show -s --format=%H HEAD)
last=$(git show -s --format=%H "$t")

if [[ $head != $last ]]; then
	VERSION=$(git describe --tags|sed 's,[-_],.,g;s,\.g.*$,,')
	DATE=$(git log --date=iso|grep -m 1 '^Date:'|awk '{print$2}')
	title="Release $PACKAGE-$VERSION released $DATE"
	under=$(echo "$title"|sed 's,.,-,g')
	cmd="git shortlog -e -n -w80,6,8 ${t}...HEAD"
	detail=$(eval "$cmd")
	log="\n$title\n$under\n\n$cmd\n\n$detail\n\n"
	output=("$log" "${output[@]}")
fi

echo -e "${output[*]}" |sed 's,[[:space:]]*$,,'
