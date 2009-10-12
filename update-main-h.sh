#!/bin/bash
echo "Updating version in include/version.h"

if [ -d .svn ]; then
	svn_revision_advanced="SVN r`svnversion -n`"
	svn_revision_number="`svnversion -n | cut -d: -f1 | cut -dM -f1 | cut -dS -f1`"
	svn_revision=" SVN r$svn_revision_number"
else
	svn_revision_advanced="Not SVN"
	svn_revision_number="0"
	svn_revision=""
fi

sed -i \
	-e "s/\(BOXI_VERSION \"[0-9.]*\).*\(\"\)/\1$svn_revision\2/g" \
	-e "s/\(BOXI_VERSION_ADVANCED\) .*/\1 \"$svn_revision_advanced\"/g" \
	-e "s/\(BOXI_VERSION_COMMAS \d+,\d+,\d+\),.*/\1,$svn_revision_number\"/g" \
	include/version.h

