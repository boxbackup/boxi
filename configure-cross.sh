#!/bin/sh

export LIBS="-lcrypto -lws2_32 -lgdi32" 

if [ ! -x "configure" ]; then
	if ! NOCONFIGURE=1 ./autogen.sh; then
		echo "Error: bootstrap failed, aborting." >&2
		exit 1
	fi
fi

if ! ./configure \
	--host=i586-mingw32msvc \
	--with-wx-config=/usr/i586-mingw32msvc/bin/wx-config \
	--with-cppunit-prefix=/usr/i586-mingw32msvc/;
then
	echo "Error: configure failed, aborting." >&2
	exit 1
fi
