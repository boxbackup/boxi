## Process this file with automake to produce Makefile.in

## Created by Anjuta - will be overwritten
## If you don't want it to overwrite it,
## 	Please disable it in the Anjuta project configuration

INCLUDES = \
	$(WX_CXXFLAGS)\
	 -I../boxbackup/lib/common -I../boxbackup/lib/backupclient -I../boxbackup/lib/server -I../include -I/usr/include/libxml2

boxi_CXXFLAGS =\
	 -DPLATFORM_GCC3\
	-DPLATFORM_$(shell uname -s | tr a-z A-Z) \
	 -Wall\
	 -g

bin_PROGRAMS = boxi

boxi_SOURCES = \
	main.cc\
	ServerFilesPanel.cc\
	ClientConfig.cc\
	Location.cc\
	ClientInfoPanel.cc\
	BackupFilesPanel.cc\
	BackupLocationsPanel.cc\
	Restore.cc\
	cross16.png.c\
	tick16.png.c\
	ServerConnection.cc\
	equal16.png.c\
	hourglass16.png.c\
	notequal16.png.c\
	sametime16.png.c\
	unknown16.png.c\
	oldfile16.png.c\
	exclam16.png.c\
	partial16.png.c\
	folder16.png.c\
	alien16.png.c\
	BackupDaemonPanel.cc\
	ClientConnection.cc\
	cross16a.png.c\
	tick16a.png.c\
	ComparePanel.cc\
	ParamPanel.cc\
	crossgrey16.png.c\
	empty16.png.c\
	plus16.png.c\
	plusgrey16.png.c\
	tickgrey16.png.c

boxi_LDFLAGS = 

boxi_LDADD =  \
	$(WX_LIBS)\
	../boxbackup/release/lib/backupclient/backupclient.a ../boxbackup/release/lib/server/server.a ../boxbackup/release/lib/common/common.a ../boxbackup/release/lib/crypto/crypto.a ../boxbackup/release/lib/compress/compress.a -lcrypto -lssl -lz

