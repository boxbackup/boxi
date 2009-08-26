#!/bin/bash
echo "Updating version in include/main.h"

if [ -d .svn ]; then
	svn_revision_advanced="SVN r`svnversion -n`"
	svn_revision=" SVN r`svnversion -n | cut -d: -f1 | cut -dM -f1 | cut -dS -f1`"
else
	svn_revision_advanced="Not SVN"
	svn_revision=""
fi

sed -i \
	-e "s/\(BOXI_VERSION \"[0-9.]*\).*\(\"\)/\1$svn_revision\2/g" \
	-e "s/\(BOXI_VERSION_ADVANCED\) .*/\1 \"$svn_revision_advanced\"/g" \
	include/main.h

