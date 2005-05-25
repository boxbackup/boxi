#!/bin/sh

CVSROOT=:ext:gcc@cvs.sourceforge.net:/cvsroot/boxi

abort() {
	echo "$*"
	exit 1
}

if [ -z "$2" -o -n "$3" ]; then
	echo "Usage: $0 <version> <tag>"
	exit 2
fi

[ -d "boxi-$1" ] && abort "boxi-$1 already exists"

cvs -q -d $CVSROOT co -r $2 -d boxi-$1 boxi \
|| abort "Failed to check out boxi $2 from repository"

cd boxi-$1 || abort "version directory missing"

tar xzvf boxbackup-0.09.tgz \
|| abort "Failed to extract BoxBackup 0.09 pristine sources"

diff -Nru --exclude=CVS boxbackup-0.09 boxbackup > boxbackup.patch \
&& abort "Failed to diff between BoxBackup 0.09 and CVS version"

perl -i -pe '
	s|# PATCH IN RELEASE|patch -p1 < ../boxbackup.patch|;
	s|# extract boxbackup in release build|rm -rf boxbackup\ntar xzvf boxbackup-0.09.tgz\nmv boxbackup-0.09 boxbackup|;
' configure.in || abort "Failed to patch configure.in"

rm -rf boxbackup-0.09 boxbackup

./make-image-headers.pl || abort "compiling image files failed"
NOCONFIGURE=1 ./autogen.sh || abort "autogen.sh failed"
cd docs
pod2html README.pod -t "Boxi README" --noindex > README.html
pod2text README.pod                            > README.txt
rm -f pod2htm*.tmp
cp README.html ../..
cd ..

rm -rf autom4te.cache acinclude.m4
mkdir -p test/clientdata

cd ..
tar czvf boxi-$1.tar.gz boxi-$1 --exclude CVS \
|| abort "creating tarball failed"
rm -rf boxi-$1 || abort "deleting old directory boxi-$1 failed"
