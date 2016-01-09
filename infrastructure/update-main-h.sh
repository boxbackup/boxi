#!/bin/bash
echo "Updating version in include/version.h"

if [ -d .git ]; then
	git_revision=`git rev-parse --short HEAD`
	git_revision_number=".$git_revision"
else
	git_revision_number=""
	git_revision="Not Git"
fi

sed -i \
<<<<<<< HEAD
	-e "s/\(BOXI_VERSION \"[0-9.]*\).*\(\"\)/\1$git_revision_number\2/g" \
	-e "s/\(BOXI_VERSION_ADVANCED\) .*/\1 \"$git_revision\"/g" \
	-e "s/\(BOXI_VERSION_COMMAS \d+,\d+,\d+\),.*/\1,$git_revision_number\"/g" \
=======
	-e "s/\(BOXI_VERSION \"[0-9]\+\.[0-9]\+\.[0-9]\+\).*\(\"\)/\1 Git $git_revision_number\2/g" \
	-e "s/\(BOXI_VERSION_ADVANCED\) .*/\1 \"$git_revision\"/g" \
	-e "s/\(BOXI_VERSION_COMMAS [0-9]\+,[0-9]\+,[0-9]\+\),.*/\1$git_revision_number/g" \
>>>>>>> master
	include/version.h
