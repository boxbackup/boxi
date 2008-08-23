#!/bin/sh

if [ ! -x "configure" ]; then
	if ! NOCONFIGURE=1 ./autogen.sh; then
		echo "Error: bootstrap failed, aborting." >&2
		exit 1
	fi
fi

export CC="gcc -mno-cygwin"
export CPP="gcc -mno-cygwin -E"
export CXX="g++ -mno-cygwin"
export LD="g++ -mno-cygwin"
export CFLAGS="-mno-cygwin -mthreads"
export CXXFLAGS="-mno-cygwin -mthreads"
export LDFLAGS="-mno-cygwin -mthreads -L/usr/i686-pc-mingw32/lib"
export LIBS="-lcrypto -lws2_32 -lgdi32"

if ! ./configure --target=i686-pc-mingw32; then
	echo "Error: configure failed, aborting." >&2
	exit 1
fi
