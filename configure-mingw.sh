#!/bin/sh

DEP_PATH=/usr/i686-pc-mingw32

if [ ! -x "configure" ]; then
	if ! NOCONFIGURE=1 \
		ACLOCAL_FLAGS="-I '${DEP_PATH}/share/aclocal'" \
		./autogen.sh
	then
		echo "Error: bootstrap failed, aborting." >&2
		exit 1
	fi
fi

export PKG_CONFIG_PATH="$DEP_PATH/lib/pkgconfig"
LIBZ_PATH="$DEP_PATH/sys-root/mingw/lib"

if ! ./boxbackup/infrastructure/mingw/configure.sh "$@" \
	--with-wx-config="$DEP_PATH/bin/wx-config" \
	--with-cppunit-prefix="$DEP_PATH"
then
	echo "Error: configure failed, aborting." >&2
	exit 1
fi
