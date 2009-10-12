## Process this file with automake to produce Makefile.in

## Created by Anjuta - will be overwritten
## If you don't want it to overwrite it,
## 	Please disable it in the Anjuta project configuration

INCLUDES = \
	$(WX_CXXFLAGS)\
	 -I../boxbackup/lib/common -I../boxbackup/lib/backupclient -I../boxbackup/lib/server -I../include -I../boxbackup/bin/bbackupd -I../boxbackup/lib/crypto -I../boxbackup/lib/win32 -I../boxbackup/bin/bbstored -I../boxbackup/lib/backupstore -I../boxbackup/lib/raidfile -I../boxbackup/bin/bbackupquery

AM_CXXFLAGS =\
	 -DBOXI\
	 -Wall\
	 -g

bin_PROGRAMS = boxi

boxi_SOURCES = \
	main.cc\
	ClientConfig.cc\
	Location.cc\
	ClientInfoPanel.cc\
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
	tickgrey16.png.c\
	BackupPanel.cc\
	BackupProgressPanel.cc\
	GeneralPanel.cc\
	partialtick16.png.c\
	Property.cc\
	SetupWizard.cc\
	FunctionPanel.cc\
	RestorePanel.cc\
	RestoreFilesPanel.cc\
	FileTree.cc\
	MainFrame.cc\
	TestFrame.cc\
	WxGuiTestHelper.cpp\
	swWxGuiTestEventSimulationHelper.cpp\
	TestConfig.cc\
	TestWizard.cc\
	TestBackup.cc\
	TestBackupConfig.cc\
	TestRestore.cc\
	RestoreProgressPanel.cc\
	TestWithServer.cc\
	TestCompare.cc\
	TestBackupConfig2.cc \
	CompareFilesPanel.cc \
	CompareProgressPanel.cc \
	ProgressPanel.cc

if WINDOWS
boxi_SOURCES += boxi.rc
endif

boxi_LDFLAGS = 

boxi_LDADD = \
	$(WX_LIBS) \
	../boxbackup/release/bin/bbackupd/BackupClientContext.o \
	../boxbackup/release/bin/bbackupd/BackupClientDeleteList.o \
	../boxbackup/release/bin/bbackupd/BackupClientDirectoryRecord.o \
	../boxbackup/release/bin/bbackupd/BackupClientInodeToIDMap.o \
	../boxbackup/release/bin/bbackupd/autogen_ClientException.o \
	../boxbackup/release/bin/bbackupd/BackupDaemon.o \
	../boxbackup/release/bin/bbackupd/Win32BackupService.o \
	../boxbackup/release/bin/bbackupd/Win32ServiceFunctions.o \
	../boxbackup/release/bin/bbstored/BBStoreDHousekeeping.o \
	../boxbackup/release/bin/bbstored/HousekeepStoreAccount.o \
	../boxbackup/release/bin/bbstored/BackupStoreContext.o \
	../boxbackup/release/bin/bbstored/BackupCommands.o \
	../boxbackup/release/bin/bbstored/autogen_BackupProtocolServer.o \
	../boxbackup/release/bin/bbstored/BackupStoreDaemon.o \
	../boxbackup/release/bin/bbackupquery/BackupQueries.o \
	../boxbackup/release/bin/bbackupquery/autogen_Documentation.o \
	../boxbackup/release/lib/backupstore/backupstore.a \
	../boxbackup/release/lib/raidfile/raidfile.a \
	../boxbackup/release/lib/backupclient/backupclient.a \
	../boxbackup/release/lib/server/server.a \
	../boxbackup/release/lib/crypto/crypto.a \
	../boxbackup/release/lib/compress/compress.a \
	../boxbackup/release/lib/common/common.a \
	../boxbackup/release/lib/win32/win32.a -lz

# override .c.o rule for prettier output.
# adapted from /usr/share/automake-1.9/am/depend2.am

HIDE_ECHO = $(if $V,,@ echo $(1) $(2) &&)
CXXLINK = $(LIBTOOL) $(if $V,,--silent) --tag=CXX --mode=link $(CXXLD) \
	$(AM_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@

.cc.o:
	$(call HIDE_ECHO,"  [CXX]     ",$<) \
	if $(CXXCOMPILE) -MT $@ -MD -MP -MF "$(DEPDIR)/$*.Tpo" -c -o $@ $<; \
	then mv -f "$(DEPDIR)/$*.Tpo" "$(DEPDIR)/$*.Po"; \
	else rm -f "$(DEPDIR)/$*.Tpo"; exit 1; fi

.rc.o:
	$(call HIDE_ECHO,"  [WINDRES] ",$<) \
	$(RC) $(WX_CXXFLAGS) -o $@ $<

boxi$(EXEEXT): $(boxi_OBJECTS) $(boxi_DEPENDENCIES)
	@rm -f boxi$(EXEEXT)
	$(call HIDE_ECHO,"  [LINK]    ",$@) \
	$(CXXLINK) $(boxi_LDFLAGS) $(boxi_OBJECTS) $(boxi_LDADD) $(LIBS)


