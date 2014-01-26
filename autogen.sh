#!/bin/bash

PACKAGE='xdg-launch'
VERSION=
DATE="`date -uI`"

if [ -z "$VERSION" ]; then
	VERSION='0.1'
	if [ -x "`which git 2>/dev/null`" -a -d .git ]; then
		DATE=$(git log --date=iso|grep '^Date:'|head -1|awk '{print$2}')
		VERSION=$(git describe --tags|sed 's,[-_],.,g;s,\.g.*$,,')
		(
		   echo -e "# created with git log --stat=75 | fmt -sct -w80\n"
		   git log --stat=76 | fmt -sct -w80
		)>ChangeLog
	fi
fi

sed -r -e "s:AC_INIT\([[]$PACKAGE[]],[[][^]]*[]]:AC_INIT([$PACKAGE],[$VERSION]:
	   s:AC_REVISION\([[][^]]*[]]\):AC_REVISION([$VERSION]):
	   s:^DATE=.*$:DATE='$DATE':" \
	  configure.template >configure.ac

subst="s:@PACKAGE@:$PACKAGE:g
       s:@VERSION@:$VERSION:g
       s:@DATE@:$DATE:g"

sed -r -e "$subst" README.in >README
sed -r -e "$subst" NEWS.in >NEWS

autoreconf -fiv
