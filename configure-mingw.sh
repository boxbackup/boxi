#!/bin/sh

if [ ! -x "configure" ]; then
	if ! NOCONFIGURE=1 \
		ACLOCAL_FLAGS='-I /usr/i686-pc-mingw32/share/aclocal' \
		./autogen.sh
	then
		echo "Error: bootstrap failed, aborting." >&2
		exit 1
	fi
fi

if ! ./configure --target=i686-pc-mingw32 \
	CC="gcc -mno-cygwin" \
	CPP="gcc -mno-cygwin -E" \
	CXX="g++ -mno-cygwin" \
	LD="g++ -mno-cygwin" \
	CFLAGS="-mthreads" \
	CXXFLAGS="-mthreads" \
	LDFLAGS="-mthreads -L/usr/i686-pc-mingw32/lib" \
	LIBS="-lcrypto -lws2_32 -lgdi32" \
	--with-wx-config=/usr/i686-pc-mingw32/bin/wx-config \
	--with-cppunit-prefix=/usr/i686-pc-mingw32
then
	echo "Error: configure failed, aborting." >&2
	exit 1
fi
