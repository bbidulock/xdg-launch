#!/bin/bash

PACKAGE='xdg-tools'
VERSION=

if [ -z "$VERSION" ]; then
	VERSION='0.1'
	if [ -x "`which git 2>/dev/null`" -a -d .git ]; then
		VERSION=$(git describe --tags|sed 's,[-_],.,g;s,\.g.*$,,')
		(
		   echo -e "# created with git log --stat=75 | fmt -sct -w80\n"
		   git log --stat=76 | fmt -sct -w80
		)>ChangeLog
	fi
fi

sed -r -e "s:AC_INIT\([[]$PACKAGE[]],[[][^]]*[]]:AC_INIT([$PACKAGE],[$VERSION]:
	   s:AC_REVISION\([[][^]]*[]]\):AC_REVISION([$VERSION]):" \
	  configure.template >configure.ac

autoreconf -fiv
