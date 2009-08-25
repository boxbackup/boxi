#!/bin/bash
echo "Updating version in include/main.h"

if [ -d .svn ]; then
    svn_revision=" SVN R`svnversion -n | cut -d: -f1 | cut -dM -f1 | cut -dS -f1`"
else
    svn_revision=""
fi

sed -i -e "s/BOXI_VERSION\ \"0.1.1\"/BOXI_VERSION \"0.1.1$svn_revision\"/g" include/main.h

