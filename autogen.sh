#!/bin/bash

PACKAGE=xdg-launch
VERSION=
DATE="`date -uI`"

if [ -z "$VERSION" ]; then
	VERSION='1.1'
	if [ -x "`which git 2>/dev/null`" -a -d .git ]; then
		VERSION=$(git describe --tags|sed 's,[-_],.,g;s,\.g.*$,,')
		DATE=$(git show -s --format=%ci HEAD^{commit}|awk '{print$1}')
	fi
fi

GTVERSION=`gettext --version|head -1|awk '{print$NF}'|sed -r 's,(^[^\.]*\.[^\.]*\.[^\.]*)\..*$,\1,'`

sed -i.bak configure.ac -r \
	-e "s:AC_INIT\([[]$PACKAGE[]],[[][^]]*[]]:AC_INIT([$PACKAGE],[$VERSION]:
	    s:AC_REVISION\([[][^]]*[]]\):AC_REVISION([$VERSION]):
	    s:^DATE=.*$:DATE='$DATE':
	    s:^AM_GNU_GETTEXT_VERSION.*:AM_GNU_GETTEXT_VERSION([$GTVERSION]):"

mkdir m4 2>/dev/null

autoreconf -iv
