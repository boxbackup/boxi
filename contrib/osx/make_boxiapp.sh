#!/bin/sh
#
# Note: please run this script from the main Boxi source directory as:
# contrib/osx/make_boxiapp.sh

echo Remove all references to wxchart from the current source tree
for i in $(find . -name Makefile.am -type f); do sed 's/wxchart//g' $i > $i-tmp; mv $i-tmp $i; done

echo Run autogen.sh which will fail at some point because of bad Makefile in wxchart
./autogen.sh

echo Create the Makefile that failed autogen.sh before and configure with lcrypto
automake Makefile && LIBS="-lcrypto" ./configure

echo create the image headers from PNGs and make the project
./make-image-headers.pl && make

echo create the Boxi.app package and copy it into the root src dir
cd src
make Boxi.app
cp -R Boxi.app ..
cd ..

echo DONE. You will find Boxi.app in the Boxi root directory where you started this script from.
echo In case you want to re-build Boxi.app, go to the src directory and run 
echo 
echo make Boxi.app
echo
echo Have fun.
