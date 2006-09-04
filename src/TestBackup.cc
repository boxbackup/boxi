/***************************************************************************
 *            TestBackup.cc
 *
 *  Wed May 10 23:10:16 2006
 *  Copyright 2006 Chris Wilson
 *  Email chris-boxisource@qwirx.com
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/time.h> // for utimes()
#include <utime.h> // for utime()

#include <openssl/ssl.h>

#include <wx/button.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/treectrl.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <cppunit/TestSuite.h>
#include <cppunit/extensions/HelperMacros.h>

#define TLS_CLASS_IMPLEMENTATION_CPP

#include "SandBox.h"
#include "Box.h"
#include "BackupClientRestore.h"
#include "BackupStoreDaemon.h"
#include "RaidFileController.h"
#include "BackupStoreAccountDatabase.h"
#include "BackupStoreAccounts.h"
#include "autogen_BackupProtocolServer.h"
#include "BackupStoreConfigVerify.h"
#include "BackupQueries.h"
#include "BackupDaemonConfigVerify.h"
#include "BackupClientCryptoKeys.h"
#include "BackupStoreInfo.h"
#include "StoreStructure.h"
#include "NamedLock.h"
#include "BackupClientMakeExcludeList.h"
#include "BoxTime.h"
#include "BoxTimeToUnix.h"
#include "FileModificationTime.h"
#include "BackupStoreException.h"
#include "BackupStoreConstants.h"

#include "main.h"
#include "BoxiApp.h"
#include "ClientConfig.h"
#include "MainFrame.h"
#include "SetupWizard.h"
#include "SslConfig.h"
#include "TestBackup.h"
#include "TestBackupConfig.h"
#include "FileTree.h"
#include "Restore.h"
#include "ServerConnection.h"

#undef TLS_CLASS_IMPLEMENTATION_CPP

const ConfigurationVerify *TestBackupStoreDaemon::GetConfigVerify() const
{
	return &BackupConfigFileVerify;
}

void TestBackupStoreDaemon::NotifyListenerIsReady()
{
	wxMutexLocker lock(mConditionLock);
	mStateListening = true;
	mStateKnownCondition.Signal();
}

void TestBackupStoreDaemon::SetupInInitialProcess()
{
	const Configuration &config(GetConfiguration());
	
	// Initialise the raid files controller
	RaidFileController &rcontroller = RaidFileController::GetController();
	rcontroller.Initialise(config.GetKeyValue("RaidFileConf").c_str());
	
	// Load the account database
	std::auto_ptr<BackupStoreAccountDatabase> pdb(
		BackupStoreAccountDatabase::Read(
			config.GetKeyValue("AccountDatabase").c_str()));
	mpAccountDatabase = pdb.release();
	
	// Create a accounts object
	mpAccounts = new BackupStoreAccounts(*mpAccountDatabase);
	
	// Ready to go!
}

void TestBackupStoreDaemon::Connection(SocketStreamTLS &rStream)
{
	// Get the common name from the certificate
	std::string clientCommonName(rStream.GetPeerCommonName());
	
	// Log the name
	// ::syslog(LOG_INFO, "Certificate CN: %s\n", clientCommonName.c_str());
	
	// Check it
	int32_t id;
	if(::sscanf(clientCommonName.c_str(), "BACKUP-%x", &id) != 1)
	{
		// Bad! Disconnect immediately
		return;
	}

	// Make ps listings clearer
	// SetProcessTitle("client %08x", id);

	// Create a context, using this ID
	BackupContext context(id, *this);
	
	// See if the client has an account?
	if(mpAccounts && mpAccounts->AccountExists(id))
	{
		std::string root;
		int discSet;
		mpAccounts->GetAccountRoot(id, root, discSet);
		context.SetClientHasAccount(root, discSet);
	}

	// Handle a connection with the backup protocol
	BackupProtocolServer server(rStream);
	server.SetLogToSysLog(false);
	server.SetTimeout(BACKUP_STORE_TIMEOUT);
	server.DoServer(context);
	context.CleanUp();
}

void StoreServerThread::Start()
{
	mDaemon.mConditionLock.Lock();	
	CPPUNIT_ASSERT_EQUAL(wxTHREAD_NO_ERROR, Create());
	CPPUNIT_ASSERT_EQUAL(wxTHREAD_NO_ERROR, Run());
	
	// wait until listening thread is ready to accept connections
	mDaemon.mStateKnownCondition.Wait();
	CPPUNIT_ASSERT(mDaemon.mStateListening);
	CPPUNIT_ASSERT(!mDaemon.mStateDead);
}

void StoreServerThread::Stop()
{
	if (!IsAlive()) return;
	mDaemon.SetTerminateWanted();
	Wait();
}

StoreServerThread::ExitCode StoreServerThread::Entry()
{
	try
	{
		mDaemon.Run();
		return 0;
	}
	catch (BoxException& e)
	{
		printf("Server thread died due to exception: %s\n",
			e.what());
	}
	catch (...)
	{
		printf("Server thread died due to unknown exception\n");
	}
	mDaemon.mStateListening = false;
	mDaemon.mStateDead = true;
	mDaemon.mStateKnownCondition.Signal();
	return (void *)1;
}

void StoreServer::Start()
{
	if (mapThread.get()) 
	{
		throw "already running";
	}
	
	mapThread.reset(new StoreServerThread(mConfigFile)),
	mapThread->Start();
}

void StoreServer::Stop()
{
	if (!mapThread.get()) 
	{
		throw "not running";
	}

	mapThread->Stop();
	mapThread.reset();
}

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestBackup, "WxGuiTest");

CppUnit::Test *TestBackup::suite()
{
	CppUnit::TestSuite *suiteOfTests =
		new CppUnit::TestSuite("TestBackup");
	suiteOfTests->addTest(
		new CppUnit::TestCaller<TestBackup>(
			"TestBackup",
			&TestBackup::RunTest));
	return suiteOfTests;
}

void Unzip(const wxFileName& rZipFile, const wxFileName& rDestDir,
	bool restoreTimes)
{
	CPPUNIT_ASSERT(rZipFile.FileExists());
	wxFileInputStream zipFis(rZipFile.GetFullPath());
	CPPUNIT_ASSERT(zipFis.Ok());
	wxZipInputStream zipInput(zipFis);
	
	for (std::auto_ptr<wxZipEntry> apEntry(zipInput.GetNextEntry());
		apEntry.get() != NULL; apEntry.reset(zipInput.GetNextEntry()))
	{
		wxFileName outName = MakeAbsolutePath(rDestDir, 
			apEntry->GetInternalName());

		if (apEntry->IsDir())
		{
			CPPUNIT_ASSERT(!outName.FileExists());
			if (!outName.DirExists())
			{
				wxMkdir(outName.GetFullPath(), 0700);
				CPPUNIT_ASSERT(outName.DirExists());
			}
		}
		else
		{
			// CPPUNIT_ASSERT(!outName.FileExists());
			CPPUNIT_ASSERT(!outName.DirExists());
		
			wxFileOutputStream outFos(outName.GetFullPath());
			CPPUNIT_ASSERT(outFos.Ok());

			outFos.Write(zipInput);
			/*
			CPPUNIT_ASSERT_EQUAL((int)apEntry->GetSize(),
			       (int)outFos.LastWrite());
			*/

			CPPUNIT_ASSERT(outName.FileExists());
			
			wxFile outFile(outName.GetFullPath());
			CPPUNIT_ASSERT_EQUAL(apEntry->GetSize(), 
				outFile.Length());
		}
		
		if (restoreTimes)
		{
			wxDateTime time = apEntry->GetDateTime();
			struct timeval tvs[2];
			tvs[0].tv_sec  = time.GetTicks();
			tvs[0].tv_usec = 0;
			tvs[1].tv_sec  = time.GetTicks();
			tvs[1].tv_usec = 0;
			wxCharBuffer buf = outName.GetFullPath().mb_str(wxConvLibc);
			CPPUNIT_ASSERT(::utimes(buf.data(), tvs) == 0);
		}
	}
}

int BlockSizeOfDiscSet(int DiscSet)
{
	// Get controller, check disc set number
	RaidFileController &controller(RaidFileController::GetController());
	if(DiscSet < 0 || DiscSet >= controller.GetNumDiscSets())
	{
		printf("Disc set %d does not exist\n", DiscSet);
		exit(1);
	}
	
	// Return block size
	return controller.GetDiscSet(DiscSet).GetBlockSize();
}

// copied from bbstoreaccounts.o
static int64_t SizeStringToBlocks(const char *string, int DiscSet)
{
	// Find block size
	int blockSize = BlockSizeOfDiscSet(DiscSet);
	
	// Get number
	char *endptr = (char*)string;
	int64_t number = strtol(string, &endptr, 0);
	if(endptr == string || number == LONG_MIN || number == LONG_MAX)
	{
		printf("%s is an invalid number\n", string);
		exit(1);
	}
	
	// Check units
	switch(*endptr)
	{
	case 'M':
	case 'm':
		// Units: Mb
		return (number * 1024*1024) / blockSize;
		break;
		
	case 'G':
	case 'g':
		// Units: Gb
		return (number * 1024*1024*1024) / blockSize;
		break;
		
	case 'B':
	case 'b':
		// Units: Blocks
		// Easy! Just return the number specified.
		return number;
		break;
	
	default:
		printf("%s has an invalid units specifier\nUse B for blocks, M for Mb, G for Gb, eg 2Gb\n", string);
		exit(1);
		break;		
	}
}

bool GetWriteLockOnAccount(NamedLock &rLock, const std::string rRootDir, int DiscSetNum)
{
	std::string writeLockFilename;
	StoreStructure::MakeWriteLockFilename(rRootDir, DiscSetNum, writeLockFilename);

	bool gotLock = false;
	int triesLeft = 8;
	do
	{
		gotLock = rLock.TryAndGetLock(writeLockFilename.c_str(), 0600 /* restrictive file permissions */);
		
		if(!gotLock)
		{
			--triesLeft;
			::sleep(1);
		}
	} while(!gotLock && triesLeft > 0);

	CPPUNIT_ASSERT(gotLock);

	return gotLock;
}

// copied from bbstoreaccounts.o
static void SetLimit(const Configuration &rConfig, int32_t ID, 
	const char *SoftLimitStr, const char *HardLimitStr)
{
	// Load in the account database 
	std::auto_ptr<BackupStoreAccountDatabase> db(
		BackupStoreAccountDatabase::Read(
			rConfig.GetKeyValue("AccountDatabase").c_str()));
	
	// Already exists?
	CPPUNIT_ASSERT(db->EntryExists(ID));
	
	// Load it in
	BackupStoreAccounts acc(*db);
	std::string rootDir;
	int discSet;
	acc.GetAccountRoot(ID, rootDir, discSet);
	
	// Attempt to lock
	NamedLock writeLock;
	CPPUNIT_ASSERT(GetWriteLockOnAccount(writeLock, rootDir, discSet));

	// Load the info
	std::auto_ptr<BackupStoreInfo> info(BackupStoreInfo::Load(
		ID, rootDir, discSet, false /* Read/Write */));

	// Change the limits
	int64_t softlimit = SizeStringToBlocks(SoftLimitStr, discSet);
	int64_t hardlimit = SizeStringToBlocks(HardLimitStr, discSet);
	// CheckSoftHardLimits(softlimit, hardlimit);
	info->ChangeLimits(softlimit, hardlimit);
	
	// Save
	info->Save();
}

void CompareBackup(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, BackupQueries::CompareParams& rParams,
	const wxString& rRemoteDir, const wxFileName& rLocalPath)
{
	// Try and work out the time before which all files should be on the server
	{
		std::string syncTimeFilename =
			rClientConfig.GetKeyValue("DataDirectory") + 
			DIRECTORY_SEPARATOR + "last_sync_start";

		// Stat it to get file time
		struct stat st;
		if(::stat(syncTimeFilename.c_str(), &st) == 0)
		{
			// Files modified after this time shouldn't be on the 
			// server, so report errors slightly differently
			rParams.mLatestFileUploadTime = FileModificationTime(st)
				- SecondsToBoxTime(rClientConfig.GetKeyValueInt(
					"MinimumFileAge"));
		}
	}

	// Connect to server
	SocketStreamTLS socket;
	socket.Open(rTlsContext, Socket::TypeINET, 
		rClientConfig.GetKeyValue("StoreHostname").c_str(), 
		BOX_PORT_BBSTORED);
	
	// Make a protocol, and handshake
	BackupProtocolClient connection(socket);
	connection.Handshake();
	
	// Check the version of the server
	{
		std::auto_ptr<BackupProtocolClientVersion> serverVersion
		(
			connection.QueryVersion(BACKUP_STORE_SERVER_VERSION)
		);
		CPPUNIT_ASSERT_EQUAL(BACKUP_STORE_SERVER_VERSION,
			serverVersion->GetVersion());
	}

	// Login -- if this fails, the Protocol will exception
	connection.QueryLogin
	(
		rClientConfig.GetKeyValueInt("AccountNumber"),
		BackupProtocolClientLogin::Flags_ReadOnly
	);

	// Set up a context for our work
	BackupQueries query(connection, rClientConfig);

	wxCharBuffer remote = rRemoteDir.mb_str(wxConvLibc);
	wxCharBuffer local  = rLocalPath.GetFullPath().mb_str(wxConvLibc);
	query.Compare(remote.data(), local.data(), rParams);

	connection.QueryFinished();
}

void CompareExpectNoDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const wxString& rRemoteDir, 
	const wxFileName& rLocalPath)
{
	BackupQueries::CompareParams params;
	params.mQuiet = false;
	CompareBackup(rClientConfig, rTlsContext, params, rRemoteDir, rLocalPath);

	CPPUNIT_ASSERT_EQUAL(0, params.mDifferences);
	CPPUNIT_ASSERT_EQUAL(0, 
		params.mDifferencesExplainedByModTime);
	CPPUNIT_ASSERT_EQUAL(0, params.mExcludedDirs);
	CPPUNIT_ASSERT_EQUAL(0, params.mExcludedFiles);
}

void CompareExpectDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const wxString& rRemoteDir, 
	const wxFileName& rLocalPath, int numDiffs, int numDiffsModTime)
{
	BackupQueries::CompareParams params;
	params.mQuiet = true;
	CompareBackup(rClientConfig, rTlsContext, params, rRemoteDir, rLocalPath);

	CPPUNIT_ASSERT_EQUAL(numDiffs, params.mDifferences);
	CPPUNIT_ASSERT_EQUAL(numDiffsModTime, 
		params.mDifferencesExplainedByModTime);
	CPPUNIT_ASSERT_EQUAL(0, params.mExcludedDirs);
	CPPUNIT_ASSERT_EQUAL(0, params.mExcludedFiles);
}

static std::auto_ptr<BackupStoreInfo> GetAccountInfo(const Configuration& rConfig)
{
	// Load in the account database 
	std::auto_ptr<BackupStoreAccountDatabase> db(BackupStoreAccountDatabase::Read(rConfig.GetKeyValue("AccountDatabase").c_str()));

	// Account exists?
	CPPUNIT_ASSERT(db->EntryExists(2));

	// Load it in
	BackupStoreAccounts acc(*db);
	std::string rootDir;
	int discSet;
	acc.GetAccountRoot(2, rootDir, discSet);
	std::auto_ptr<BackupStoreInfo> info(BackupStoreInfo::Load(
		2, rootDir, discSet, true /* ReadOnly */));
	return info;
}

void CompareLocation(const Configuration& rConfig, 
	TLSContext& rTlsContext,
	const std::string& rLocationName, 
	BackupQueries::CompareParams& rParams)
{
	// Find the location's sub configuration
	const Configuration &locations(rConfig.GetSubConfiguration("BackupLocations"));
	CPPUNIT_ASSERT(locations.SubConfigurationExists(rLocationName.c_str()));
	const Configuration &loc(locations.GetSubConfiguration(rLocationName.c_str()));
	
	// Generate the exclude lists
	if(!rParams.mIgnoreExcludes)
	{
		rParams.mpExcludeFiles = BackupClientMakeExcludeList_Files(loc);
		rParams.mpExcludeDirs = BackupClientMakeExcludeList_Dirs(loc);
	}
			
	// Then get it compared
	std::string localPathString = std::string("/") + rLocationName;
	wxString remotePath(localPathString.c_str(), wxConvLibc);
	wxString localPath(loc.GetKeyValue("Path").c_str(), wxConvLibc);
	CompareBackup(rConfig, rTlsContext, rParams, 
		remotePath, localPath);
	
	// Delete exclude lists
	rParams.DeleteExcludeLists();
}

void CompareLocationExpectNoDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const std::string& rLocationName,
	int excludedDirs, int excludedFiles)
{
	BackupQueries::CompareParams params;
	params.mQuiet = false;
	CompareLocation(rClientConfig, rTlsContext, rLocationName, params);

	CPPUNIT_ASSERT_EQUAL(0, params.mDifferences);
	CPPUNIT_ASSERT_EQUAL(0, 
		params.mDifferencesExplainedByModTime);
	CPPUNIT_ASSERT_EQUAL(excludedDirs,  params.mExcludedDirs);
	CPPUNIT_ASSERT_EQUAL(excludedFiles, params.mExcludedFiles);
}

void CompareLocationExpectDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const std::string& rLocationName, 
	int numDiffs, int numDiffsModTime, int excludedDirs, int excludedFiles)
{
	BackupQueries::CompareParams params;
	params.mQuiet = true;
	CompareLocation(rClientConfig, rTlsContext, rLocationName, params);

	CPPUNIT_ASSERT_EQUAL(numDiffs, params.mDifferences);
	CPPUNIT_ASSERT_EQUAL(numDiffsModTime, 
		params.mDifferencesExplainedByModTime);
	CPPUNIT_ASSERT_EQUAL(excludedDirs,  params.mExcludedDirs);
	CPPUNIT_ASSERT_EQUAL(excludedFiles, params.mExcludedFiles);
}

int64_t SearchDir(BackupStoreDirectory& rDir, const std::string& rChildName)
{
	BackupStoreDirectory::Iterator i(rDir);
	BackupStoreFilenameClear child(rChildName.c_str());
	BackupStoreDirectory::Entry *en = i.FindMatchingClearName(child);
	if (en == 0) return 0;
	int64_t id = en->GetObjectID();
	CPPUNIT_ASSERT(id > 0);
	CPPUNIT_ASSERT(id != BackupProtocolClientListDirectory::RootDirectory);
	return id;
}

#ifdef WIN32
void SetFileTime(const char* filename, FILETIME creationTime, 
	FILETIME lastModTime, FILETIME lastAccessTime)
{
	HANDLE handle = openfile(filename, O_RDWR, 0);
	CPPUNIT_ASSERT(handle != INVALID_HANDLE_VALUE);
	CPPUNIT_ASSERT(SetFileTime(handle, &creationTime, &lastAccessTime,
		&lastModTime));
	CPPUNIT_ASSERT(CloseHandle(handle));
}
#else
void SetFileTime(const char* filename, time_t creationTime /* not used */, 
	time_t lastModTime, time_t lastAccessTime)
{
	struct utimbuf ut;
	ut.actime  = lastAccessTime;
	ut.modtime = lastModTime;
	CPPUNIT_ASSERT(utime(filename, &ut) == 0);
}
#endif

void TestBackup::RunTest()
{
	// create a working directory
	wxFileName tempDir;
	tempDir.AssignTempFileName(wxT("boxi-tempdir-"));
	CPPUNIT_ASSERT(wxRemoveFile(tempDir.GetFullPath()));
	CPPUNIT_ASSERT(wxMkdir     (tempDir.GetFullPath(), 0700));
	CPPUNIT_ASSERT(tempDir.DirExists());
	
	// create a directory to hold config files for the tests
	wxFileName confDir(tempDir.GetFullPath(), _("conf"));
	CPPUNIT_ASSERT(wxMkdir(confDir.GetFullPath(), 0700));
	CPPUNIT_ASSERT(confDir.DirExists());
	
	// create a directory to hold bbstored's store temporarily
	wxFileName storeDir(tempDir.GetFullPath(), _("store"));
	CPPUNIT_ASSERT(wxMkdir(storeDir.GetFullPath(), 0700));
	CPPUNIT_ASSERT(storeDir.DirExists());

	// create raidfile.conf for the store daemon
	wxFileName raidConf(confDir.GetFullPath(), _("raidfile.conf"));
	CPPUNIT_ASSERT(!raidConf.FileExists());
	
	{
		wxFileName storeDir0(storeDir.GetFullPath(), _("0_0"));
		CPPUNIT_ASSERT(wxMkdir(storeDir0.GetFullPath(), 0700));
		CPPUNIT_ASSERT(storeDir0.DirExists());
	
		wxFileName storeDir1(storeDir.GetFullPath(), _("0_1"));
		CPPUNIT_ASSERT(wxMkdir(storeDir1.GetFullPath(), 0700));
		CPPUNIT_ASSERT(storeDir1.DirExists());
	
		wxFileName storeDir2(storeDir.GetFullPath(), _("0_2"));
		CPPUNIT_ASSERT(wxMkdir(storeDir2.GetFullPath(), 0700));
		CPPUNIT_ASSERT(storeDir2.DirExists());
	
		wxFile raidConfFile;
		CPPUNIT_ASSERT(raidConfFile.Create(raidConf.GetFullPath()));
		
		CPPUNIT_ASSERT(raidConfFile.Write(_("disc0\n")));
		CPPUNIT_ASSERT(raidConfFile.Write(_("{\n")));
		CPPUNIT_ASSERT(raidConfFile.Write(_("\tSetNumber = 0\n")));
		CPPUNIT_ASSERT(raidConfFile.Write(_("\tBlockSize = 2048\n")));

		CPPUNIT_ASSERT(raidConfFile.Write(_("\tDir0 = ")));
		CPPUNIT_ASSERT(raidConfFile.Write(storeDir0.GetFullPath()));
		CPPUNIT_ASSERT(raidConfFile.Write(_("\n")));

		CPPUNIT_ASSERT(raidConfFile.Write(_("\tDir1 = ")));
		CPPUNIT_ASSERT(raidConfFile.Write(storeDir1.GetFullPath()));
		CPPUNIT_ASSERT(raidConfFile.Write(_("\n")));
		
		CPPUNIT_ASSERT(raidConfFile.Write(_("\tDir2 = ")));
		CPPUNIT_ASSERT(raidConfFile.Write(storeDir2.GetFullPath()));
		CPPUNIT_ASSERT(raidConfFile.Write(_("\n")));

		CPPUNIT_ASSERT(raidConfFile.Write(_("}\n")));
		raidConfFile.Close();
	}
	
	// create accounts.txt for the store daemon
	wxFileName accountsDb(confDir.GetFullPath(), _("accounts.txt"));
	CPPUNIT_ASSERT(!accountsDb.FileExists());
	
	{
		wxFile accountsDbFile;
		CPPUNIT_ASSERT(accountsDbFile.Create(accountsDb.GetFullPath()));
	}
	
	// create bbstored.conf for the store daemon
	wxFileName storeConfigFileName(confDir.GetFullPath(), _("bbstored.conf"));
	
	{
		wxFile storeConfigFile;
		CPPUNIT_ASSERT(storeConfigFile.Create(storeConfigFileName.GetFullPath()));
		
		CPPUNIT_ASSERT(raidConf.FileExists());
		CPPUNIT_ASSERT(storeConfigFile.Write(_("RaidFileConf = ")));
		CPPUNIT_ASSERT(storeConfigFile.Write(raidConf.GetFullPath()));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\n")));
		
		wxFileName accountsDb(confDir.GetFullPath(), _("accounts.txt"));
		CPPUNIT_ASSERT(accountsDb.FileExists());
		CPPUNIT_ASSERT(storeConfigFile.Write(_("AccountDatabase = ")));
		CPPUNIT_ASSERT(storeConfigFile.Write(accountsDb.GetFullPath()));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\n")));
		
		CPPUNIT_ASSERT(storeConfigFile.Write(_("TimeBetweenHousekeeping = 900\n")));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("Server\n")));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("{\n")));
	
		wxFileName pidFile(confDir.GetFullPath(), _("bbstored.pid"));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\tPidFile = ")));
		CPPUNIT_ASSERT(storeConfigFile.Write(pidFile.GetFullPath()));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\n")));
	
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\tListenAddresses = inet:0.0.0.0\n")));
		
		wxFileName serverCert(MakeAbsolutePath(
			_("../test/config/bbstored/server-cert.pem")));
		CPPUNIT_ASSERT(serverCert.FileExists());
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\tCertificateFile = ")));
		CPPUNIT_ASSERT(storeConfigFile.Write(serverCert.GetFullPath()));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\n")));
	
		wxFileName serverKey(MakeAbsolutePath(
			_("../test/config/bbstored/server-key.pem")));
		CPPUNIT_ASSERT(serverKey.FileExists());
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\tPrivateKeyFile = ")));
		CPPUNIT_ASSERT(storeConfigFile.Write(serverKey.GetFullPath()));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\n")));
	
		wxFileName clientCA(MakeAbsolutePath(
			_("../test/config/bbstored/client-ca.pem")));
		CPPUNIT_ASSERT(clientCA.FileExists());
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\tTrustedCAsFile = ")));
		CPPUNIT_ASSERT(storeConfigFile.Write(clientCA.GetFullPath()));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\n")));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("}\n")));
	
		storeConfigFile.Close();
	}

	{
		wxString command = _("../boxbackup/release/bin/bbstoreaccounts"
			"/bbstoreaccounts -c ");
		command.Append(storeConfigFileName.GetFullPath());
		command.Append(_(" create 2 0 100M 200M"));
		
		long result = wxExecute(command, wxEXEC_SYNC);
		if (result != 0)
		{
			wxCharBuffer buf = command.mb_str(wxConvLibc);
			printf("Failed to execute command '%s': error %d\n",
				buf.data(), (int)result);
			CPPUNIT_ASSERT_EQUAL(0, (int)result);
		}
	}

	// create a directory to hold the data that we will be backing up	
	wxFileName testDataDir(tempDir.GetFullPath(), _("testdata"));
	CPPUNIT_ASSERT(testDataDir.Mkdir(0700));
	
	MainFrame* pMainFrame = GetMainFrame();
	CPPUNIT_ASSERT(pMainFrame);
	
	mpConfig = pMainFrame->GetConfig();
	CPPUNIT_ASSERT(mpConfig);
	
	StoreServer server(storeConfigFileName.GetFullPath());
	server.Start();

	pMainFrame->DoFileOpen(MakeAbsolutePath(
		_("../test/config/bbackupd.conf")).GetFullPath());
	wxString msg;
	bool isOk = mpConfig->Check(msg);

	if (!isOk)
	{
		wxCharBuffer buf = msg.mb_str(wxConvLibc);
		CPPUNIT_ASSERT_MESSAGE(buf.data(), isOk);
	}
	
	ServerConnection conn(mpConfig);
	TLSContext tls;
	
	isOk = conn.InitTlsContext(tls, msg);
	if (!isOk)
	{
		wxCharBuffer buf = msg.mb_str(wxConvLibc);
		CPPUNIT_ASSERT_MESSAGE(buf.data(), isOk);
	}
	
	BackupStoreDirectory dir;

	CPPUNIT_ASSERT(conn.ListDirectory(
		BackupProtocolClientListDirectory::RootDirectory,
		BackupProtocolClientListDirectory::Flags_EXCLUDE_NOTHING,
		dir));
	
	conn.Disconnect();
	
	BackupStoreDirectory::Iterator i(dir);
	BackupStoreDirectory::Entry *en;
	bool hasEntries = false;
	
	while((en = i.Next()) != 0)
	{
		BackupStoreFilenameClear clear(en->GetName());
		printf("Unexpected dir entry: %s\n", 
			clear.GetClearFilename().c_str());
		hasEntries = true;
	}

	CPPUNIT_ASSERT(!hasEntries);
	
	wxPanel* pBackupPanel = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_Backup_Panel), wxPanel
	);
	CPPUNIT_ASSERT(pBackupPanel);

	CPPUNIT_ASSERT(!pBackupPanel->IsShown());	
	ClickButtonWaitEvent(ID_Main_Frame, ID_General_Backup_Button);
	CPPUNIT_ASSERT(pBackupPanel->IsShown());
	
	// makes tests faster
	mpConfig->MinimumFileAge.Set(0);
	
	Location* pTestDataLocation = NULL;
	
	{
		wxFileName spaceTestZipFile(_("../test/data/spacetest1.zip"));
		CPPUNIT_ASSERT(spaceTestZipFile.FileExists());
		Unzip(spaceTestZipFile, testDataDir);

		CPPUNIT_ASSERT_EQUAL((size_t)0, mpConfig->GetLocations().size());
		
		Location testDirLoc(_("testdata"), testDataDir.GetFullPath(),
			mpConfig);
		mpConfig->AddLocation(testDirLoc);
		CPPUNIT_ASSERT_EQUAL((size_t)1, 
			mpConfig->GetLocations().size());
		pTestDataLocation = mpConfig->GetLocation(testDirLoc);
		CPPUNIT_ASSERT(pTestDataLocation);

		MyExcludeList& rExcludes = pTestDataLocation->GetExcludeList();

		#define ADD_ENTRY(type, path) \
		rExcludes.AddEntry(MyExcludeEntry(theExcludeTypes[type], path))
		
		ADD_ENTRY(ETI_EXCLUDE_FILE, MakeAbsolutePath(testDataDir,_("excluded_1")).GetFullPath());
		ADD_ENTRY(ETI_EXCLUDE_FILE, MakeAbsolutePath(testDataDir,_("excluded_2")).GetFullPath());
		ADD_ENTRY(ETI_EXCLUDE_FILES_REGEX, _("\\.excludethis$"));
		ADD_ENTRY(ETI_EXCLUDE_FILES_REGEX, _("EXCLUDE"));
		ADD_ENTRY(ETI_ALWAYS_INCLUDE_FILE, MakeAbsolutePath(testDataDir,_("dont.excludethis")).GetFullPath());
		ADD_ENTRY(ETI_EXCLUDE_DIR, MakeAbsolutePath(testDataDir,_("exclude_dir")).GetFullPath());
		ADD_ENTRY(ETI_EXCLUDE_DIR, MakeAbsolutePath(testDataDir,_("exclude_dir_2")).GetFullPath());
		ADD_ENTRY(ETI_EXCLUDE_DIRS_REGEX, _("not_this_dir"));
		ADD_ENTRY(ETI_ALWAYS_INCLUDE_DIRS_REGEX, _("ALWAYSINCLUDE"));
		
		#undef ADD_ENTRY

		CPPUNIT_ASSERT_EQUAL((size_t)1, mpConfig->GetLocations().size());
	}

	// helps with debugging
	wxFileName configFile(confDir.GetFullPath(), _("bbackupd.conf"));
	mpConfig->Save(configFile.GetFullPath());
	CPPUNIT_ASSERT(configFile.FileExists());

	wxPanel* pBackupProgressPanel = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_Backup_Progress_Panel), wxPanel
	);
	CPPUNIT_ASSERT(pBackupProgressPanel);
	CPPUNIT_ASSERT(!pBackupProgressPanel->IsShown());

	wxListBox* pBackupErrorList = wxDynamicCast
	(
		pBackupProgressPanel->FindWindow(ID_BackupProgress_ErrorList), 
		wxListBox
	);
	CPPUNIT_ASSERT(pBackupErrorList);
	CPPUNIT_ASSERT_EQUAL(0, pBackupErrorList->GetCount());
			
	wxButton* pBackupStartButton = wxDynamicCast
	(
		pBackupPanel->FindWindow(ID_Function_Start_Button), wxButton
	);
	CPPUNIT_ASSERT(pBackupStartButton);

	wxButton* pBackupProgressCloseButton = wxDynamicCast
	(
		pBackupProgressPanel->FindWindow(wxID_CANCEL), wxButton
	);
	CPPUNIT_ASSERT(pBackupProgressCloseButton);

	// TODO: test BM_BACKUP_FAILED_CANNOT_INIT_ENCRYPTION
	
	#define CHECK_BACKUP() \
	ClickButtonWaitEvent(pBackupStartButton); \
	CPPUNIT_ASSERT(pBackupProgressPanel->IsShown()); \
	CPPUNIT_ASSERT(pMainFrame->IsTopPanel(pBackupProgressPanel)); \
	ClickButtonWaitEvent(pBackupProgressCloseButton); \
	CPPUNIT_ASSERT(!pBackupProgressPanel->IsShown());

	#define CHECK_BACKUP_ERROR(message) \
	MessageBoxSetResponse(message, wxOK); \
	CHECK_BACKUP(); \
	MessageBoxCheckFired(); \
	CPPUNIT_ASSERT(pBackupErrorList->GetCount() > 0);

	#define CHECK_BACKUP_OK(message) \
	CHECK_BACKUP(); \
	if (pBackupErrorList->GetCount() > 0) \
	{ \
		wxCharBuffer buf = pBackupErrorList->GetString(0).mb_str(wxConvLibc); \
		CPPUNIT_ASSERT_MESSAGE(buf.data(), pBackupErrorList->GetCount() > 0); \
	}

	#define CHECK_COMPARE_OK() \
	CompareExpectNoDifferences(rClientConfig, tlsContext, _("testdata"), \
		testDataDir);

	#define CHECK_COMPARE_FAILS(diffs, modified) \
	CompareExpectDifferences(rClientConfig, tlsContext, _("testdata"), \
		testDataDir, diffs, modified);

	#define CHECK_COMPARE_LOC_OK(exdirs, exfiles) \
	CompareLocationExpectNoDifferences(rClientConfig, tlsContext, \
		"testdata", exdirs, exfiles);

	#define CHECK_COMPARE_LOC_FAILS(diffs, modified, exdirs, exfiles) \
	CompareLocationExpectDifferences(rClientConfig, tlsContext, "testdata", \
		diffs, modified, exdirs, exfiles);

	{
		#define CHECK_PROPERTY(property, message) \
		CHECK_BACKUP_ERROR(message); \
		mpConfig->property.Revert()

		#define CHECK_UNSET_PROPERTY(property, message) \
		mpConfig->property.Clear(); \
		CHECK_PROPERTY(property, message)
		
		CHECK_UNSET_PROPERTY(StoreHostname,  BM_BACKUP_FAILED_NO_STORE_HOSTNAME);
		CHECK_UNSET_PROPERTY(KeysFile,       BM_BACKUP_FAILED_NO_KEYS_FILE);
		CHECK_UNSET_PROPERTY(AccountNumber,  BM_BACKUP_FAILED_NO_ACCOUNT_NUMBER);
		CHECK_UNSET_PROPERTY(MinimumFileAge, BM_BACKUP_FAILED_NO_MINIMUM_FILE_AGE);
		CHECK_UNSET_PROPERTY(MaxUploadWait,  BM_BACKUP_FAILED_NO_MAXIMUM_UPLOAD_WAIT);
		CHECK_UNSET_PROPERTY(FileTrackingSizeThreshold, BM_BACKUP_FAILED_NO_TRACKING_THRESHOLD);
		CHECK_UNSET_PROPERTY(DiffingUploadSizeThreshold, BM_BACKUP_FAILED_NO_DIFFING_THRESHOLD);
		
		// TODO: test BM_BACKUP_FAILED_INVALID_SYNC_PERIOD
		/*
		mpConfig->MinimumFileAge.Set(GetCurrentBoxTime() + 1000000);
		CHECK_PROPERTY(MinimumFileAge, BM_BACKUP_FAILED_INVALID_SYNC_PERIOD);
		*/

		#undef CHECK_UNSET_PROPERTY
		#undef CHECK_PROPERTY

		server.Stop();
		CHECK_BACKUP_ERROR(BM_BACKUP_FAILED_CONNECT_FAILED);
		server.Start();

		/*
		TODO: write tests for:
		BM_BACKUP_FAILED_INTERRUPTED,
		BM_BACKUP_FAILED_UNKNOWN_ERROR,
		*/
	}

	mpConfig->Save(configFile.GetFullPath());
	CPPUNIT_ASSERT(configFile.FileExists());
	
	std::auto_ptr<Configuration> apClientConfig;
	{
		std::string errs;
		wxCharBuffer buf = configFile.GetFullPath().mb_str(wxConvLibc);
		apClientConfig = Configuration::LoadAndVerify(buf.data(), 
			&BackupDaemonConfigVerify, errs);
		CPPUNIT_ASSERT(apClientConfig.get());
		CPPUNIT_ASSERT(errs.empty());
	}
	const Configuration &rClientConfig(*apClientConfig);

	// Setup and connect
	// 1. TLS context
	SSLLib::Initialise();
	// Read in the certificates creating a TLS context
	TLSContext tlsContext;
	std::string certFile(rClientConfig.GetKeyValue("CertificateFile"));
	std::string keyFile(rClientConfig.GetKeyValue("PrivateKeyFile"));
	std::string caFile(rClientConfig.GetKeyValue("TrustedCAsFile"));
	tlsContext.Initialise(false /* as client */, 
		certFile.c_str(), keyFile.c_str(), caFile.c_str());
	
	// Initialise keys
	BackupClientCryptoKeys_Setup(rClientConfig.GetKeyValue("KeysFile").c_str());

	// before the first backup, there should be differences
	CHECK_COMPARE_LOC_FAILS(1, 0, 0, 0);
	
	CHECK_BACKUP_OK();

	// and afterwards, there should be no differences any more
	CHECK_COMPARE_LOC_OK(0, 0);

	// check that attributes are backed up and compared correctly - later
	/*
	{
		wxFileName testFileName = MakeAbsolutePath(testDataDir, 
			_("spacetest/f1"));
		CPPUNIT_ASSERT(testFileName.FileExists());
		wxCharBuffer namebuf = testFileName.GetFullPath().mb_str(wxConvLibc);
		std::string cmd = "echo foo > ";
		cmd += namebuf.data();
		CPPUNIT_ASSERT(system(cmd.c_str()) == 0);
		
		CHECK_BACKUP_OK();
		
		// make one of the files read-only, expect a compare failure
		#ifdef WIN32
		wxString cmd = _("attrib +r ");
		cmd.Append(testFileName);
		wxCharBuffer cmdbuf = cmd.mb_str(wxConvLibc);
		int compareReturnValue = ::system(cmdbuf.data());
		CPPUNIT_ASSERT_EQUAL(0, compareReturnValue);
		#else
		CPPUNIT_ASSERT_EQUAL(0, chmod(namebuf.data(), 0000));
		#endif
		
		CHECK_COMPARE_LOC_FAILS(1, 0, 0, 0);
		CPPUNIT_ASSERT_EQUAL(0, chmod(namebuf.data(), 0644));
		CHECK_COMPARE_LOC_OK(0, 0);
	}
	*/
	
	// ensure that the mod time of newly created files 
	// is later than the last backup.
	// sleep(1);

	// Set limit to something very small
	{
		// About 28 blocks will be used at this point (14 files in RAID)
		CPPUNIT_ASSERT_EQUAL((int64_t)28, 
			GetAccountInfo(server.GetConfiguration())->GetBlocksUsed());

		// Backup will fail if the size used is greater than 
		// soft limit + 1/3 of (hard - soft). 
		// Set small values for limits accordingly.
		SetLimit(server.GetConfiguration(), 2, "10B", "40B");

		// Unpack some more files
		wxFileName spaceTestZipFile(_("../test/data/spacetest2.zip"));
		CPPUNIT_ASSERT(spaceTestZipFile.FileExists());
		Unzip(spaceTestZipFile, testDataDir);

		// Delete a file and a directory
		CPPUNIT_ASSERT(wxRemoveFile(MakeAbsolutePath(testDataDir,
			_("spacetest/d1/f3")).GetFullPath()));
		DeleteRecursive(MakeAbsolutePath(testDataDir, 
			_("spacetest/d3/d4")));
	}
	
	// check the number of differences before backup
	// 1 file and 1 dir added (dir d8 contains one file, f7, not counted)
	// and 1 file and 1 dir removed (mod times not checked)
	CHECK_COMPARE_LOC_FAILS(4, 2, 0, 0);
	
	// fixme: should return an error
	mpConfig->ExtendedLogging.Set(true);
	CHECK_BACKUP_ERROR(BM_BACKUP_FAILED_STORE_FULL);
	
	// backup should not complete, so there should still be differences
	// the deleted file and directory should have been deleted on the store,
	// but the locally changed files should not have been uploaded
	CPPUNIT_ASSERT_EQUAL((int64_t)28, 
			GetAccountInfo(server.GetConfiguration())->GetBlocksUsed());
	CHECK_COMPARE_LOC_FAILS(2, 0, 0, 0);
	
	// set the limits back
	SetLimit(server.GetConfiguration(), 2, "1000B", "2000B");
	
	// unpack some more test files
	{
		wxFileName baseFilesZipFile(MakeAbsolutePath(
			_("../test/data/test_base.zip")).GetFullPath());
		CPPUNIT_ASSERT(baseFilesZipFile.FileExists());
		Unzip(baseFilesZipFile, testDataDir);
	}
	
	// run a backup
	CHECK_BACKUP_OK();
	
	// check that it worked
	CHECK_COMPARE_LOC_OK(0, 0);
	
	// Delete a file
	CPPUNIT_ASSERT(wxRemoveFile(MakeAbsolutePath(testDataDir, 
		_("x1/dsfdsfs98.fd")).GetFullPath()));
	
	#ifndef WIN32
	{
		// New symlink
		wxCharBuffer buf = wxFileName(testDataDir.GetFullPath(),
			_("symlink-to-dir")).GetFullPath().mb_str(wxConvLibc);
		CPPUNIT_ASSERT(::symlink("does-not-exist", buf.data()) == 0);
	}
	#endif		

	// Update a file (will be uploaded as a diff)
	{
		// Check that the file is over the diffing threshold in the 
		// bbackupd.conf file
		wxFileName bigFileName(MakeAbsolutePath(testDataDir, _("f45.df")));
		wxFile bigFile(bigFileName.GetFullPath(), wxFile::read_write);
		CPPUNIT_ASSERT(bigFile.Length() > 1024);
		
		// Add a bit to the end
		CPPUNIT_ASSERT(bigFile.IsOpened());
		CPPUNIT_ASSERT(bigFile.SeekEnd() != wxInvalidOffset);
		CPPUNIT_ASSERT(bigFile.Write(_("EXTRA STUFF")));
		CPPUNIT_ASSERT(bigFile.Length() > 1024);
	}

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);

	// Bad case: delete a file/symlink, replace it with a directory
	#ifndef WIN32
	{
		// Delete symlink
		wxCharBuffer buf = wxFileName(testDataDir.GetFullPath(),
			_("symlink-to-dir")).GetFullPath().mb_str(wxConvLibc);
		CPPUNIT_ASSERT(::unlink(buf.data()) == 0);
	}
	#endif
	
	CPPUNIT_ASSERT(wxMkdir(MakeAbsolutePath(testDataDir, 
		_("symlink-to-dir")).GetFullPath(), 0755));
	wxFileName dirTestDirName(MakeAbsolutePath(testDataDir, 
		_("x1/dir-to-file")));
	CPPUNIT_ASSERT(wxMkdir(dirTestDirName.GetFullPath(), 0755));
	// NOTE: create a file within the directory to avoid deletion by the 
	// housekeeping process later
	wxFileName placeHolderName(dirTestDirName.GetFullPath(), _("contents"));
	
	#ifdef WIN32
	{
		wxFile f;
		CPPUNIT_ASSERT(f.Create(placeHolderName.GetFullPath()));
	}
	#else // !WIN32
	{
		wxCharBuffer buf = placeHolderName.GetFullPath().mb_str(wxConvLibc);
		CPPUNIT_ASSERT(::symlink("does-not-exist", buf.data()) == 0);
	}
	#endif

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);

	// And the inverse, replace a directory with a file/symlink
	CPPUNIT_ASSERT(wxRemoveFile(placeHolderName.GetFullPath()));
	CPPUNIT_ASSERT(wxRmdir(dirTestDirName.GetFullPath()));
	
	#ifdef WIN32
	{
		wxFile f;
		CPPUNIT_ASSERT(f.Create(dirTestDirName.GetFullPath()));
	}
	#else // !WIN32
	{
		wxCharBuffer buf = dirTestDirName.GetFullPath().mb_str(wxConvLibc);
		CPPUNIT_ASSERT(::symlink("does-not-exist", buf.data()) == 0);
	}
	#endif
	
	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);

	// And then, put it back to how it was before.
	CPPUNIT_ASSERT(wxRemoveFile(dirTestDirName.GetFullPath()));
	CPPUNIT_ASSERT(wxMkdir(dirTestDirName.GetFullPath(), 0755));
	wxFileName placeHolderName2(dirTestDirName.GetFullPath(), _("contents2"));
	#ifdef WIN32
	{
		wxFile f;
		CPPUNIT_ASSERT(f.Create(placeHolderName2.GetFullPath()));
	}
	#else // !WIN32
	{
		wxCharBuffer buf = placeHolderName2.GetFullPath().mb_str(wxConvLibc);
		CPPUNIT_ASSERT(::symlink("does-not-exist", buf.data()) == 0);
	}
	#endif

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);
	
	// And finally, put it back to how it was before it was put back to 
	// how it was before. This gets lots of nasty things in the store with 
	// directories over other old directories.
	CPPUNIT_ASSERT(wxRemoveFile(placeHolderName2.GetFullPath()));
	CPPUNIT_ASSERT(wxRmdir(dirTestDirName.GetFullPath()));
	
	#ifdef WIN32
	{
		wxFile f;
		CPPUNIT_ASSERT(f.Create(dirTestDirName.GetFullPath()));
	}
	#else // !WIN32
	{
		wxCharBuffer buf = dirTestDirName.GetFullPath().mb_str(wxConvLibc);
		CPPUNIT_ASSERT(::symlink("does-not-exist", buf.data()) == 0);
	}
	#endif

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);

	// case which went wrong: rename a tracked file over a deleted file
	#ifdef WIN32
	CPPUNIT_ASSERT(wxRemoveFile(MakeAbsolutePath(testDataDir, 
		_("x1/dsfdsfs98.fd"))));
	#endif
	
	CPPUNIT_ASSERT(wxRenameFile(
		MakeAbsolutePath(testDataDir, _("df9834.dsf")).GetFullPath(),
		MakeAbsolutePath(testDataDir, _("x1/dsfdsfs98.fd")).GetFullPath()));

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);

	// Move that file back
	CPPUNIT_ASSERT(wxRenameFile(
		MakeAbsolutePath(testDataDir, _("x1/dsfdsfs98.fd")).GetFullPath(),
		MakeAbsolutePath(testDataDir, _("df9834.dsf")).GetFullPath()));

	// Add some more files. Because we restore the original
	// time stamps, these files will look very old to the daemon.
	// Lucky it'll upload them then!
	wxFileName test2ZipFile(MakeAbsolutePath(_("../test/data/test2.zip")));
	Unzip(test2ZipFile, testDataDir, true);

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);

	// Then modify an existing file
	{
		wxFileName fileToUpdate(MakeAbsolutePath(testDataDir,
			_("sub23/rand.h")));
		wxFile f(fileToUpdate.GetFullPath(), wxFile::read_write);
		CPPUNIT_ASSERT(f.IsOpened());
		CPPUNIT_ASSERT(f.SeekEnd() != wxInvalidOffset);
		CPPUNIT_ASSERT(f.Write(_("MODIFIED!\n")));
		f.Close();
		
		// and then move the time backwards!
		struct timeval times[2];
		BoxTimeToTimeval(SecondsToBoxTime((time_t)(365*24*60*60)), times[1]);
		times[0] = times[1];
		wxCharBuffer buf = fileToUpdate.GetFullPath().mb_str(wxConvLibc);
		CPPUNIT_ASSERT(::utimes(buf.data(), times) == 0);
	}

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);
	
	{
		// Add some files and directories which are marked as excluded
		wxFileName zipExcludeTest(MakeAbsolutePath(_("../test/data/testexclude.zip")));
		Unzip(zipExcludeTest, testDataDir, true);
	}
	
	// backup should still work
	CHECK_BACKUP_OK();
	
	// compare location (with exclude lists) should pass
	CHECK_COMPARE_LOC_OK(3, 4);
	
	// compare directly (without exclude lists) should fail
	CHECK_COMPARE_FAILS(7, 0);

	// check that the excluded files did not make it
	// into the store, and the included files did
	{
		SocketStreamTLS socket;
		socket.Open(tlsContext, Socket::TypeINET, 
			rClientConfig.GetKeyValue("StoreHostname").c_str(), 
			BOX_PORT_BBSTORED);
		BackupProtocolClient connection(socket);
		connection.Handshake();
		std::auto_ptr<BackupProtocolClientVersion> 
			serverVersion(connection.QueryVersion(
				BACKUP_STORE_SERVER_VERSION));
		if(serverVersion->GetVersion() != 
			BACKUP_STORE_SERVER_VERSION)
		{
			THROW_EXCEPTION(BackupStoreException, 
				WrongServerVersion);
		}
		connection.QueryLogin(
			rClientConfig.GetKeyValueInt("AccountNumber"),
			BackupProtocolClientLogin::Flags_ReadOnly);
		
		int64_t rootDirId = BackupProtocolClientListDirectory
			::RootDirectory;
		std::auto_ptr<BackupProtocolClientSuccess> dirreply(
			connection.QueryListDirectory(
				rootDirId, false, 0, false));
		std::auto_ptr<IOStream> dirstream(
			connection.ReceiveStream());
		BackupStoreDirectory dir;
		dir.ReadFromStream(*dirstream, connection.GetTimeout());

		int64_t testDirId = SearchDir(dir, "testdata");
		CPPUNIT_ASSERT(testDirId != 0);
		dirreply = connection.QueryListDirectory(testDirId, false, 0, false);
		dirstream = connection.ReceiveStream();
		dir.ReadFromStream(*dirstream, connection.GetTimeout());
		
		CPPUNIT_ASSERT(!SearchDir(dir, "excluded_1"));
		CPPUNIT_ASSERT(!SearchDir(dir, "excluded_2"));
		CPPUNIT_ASSERT(!SearchDir(dir, "exclude_dir"));
		CPPUNIT_ASSERT(!SearchDir(dir, "exclude_dir_2"));
		// xx_not_this_dir_22 should not be excluded by
		// ExcludeDirsRegex, because it's a file
		CPPUNIT_ASSERT(SearchDir (dir, "xx_not_this_dir_22"));
		CPPUNIT_ASSERT(!SearchDir(dir, "zEXCLUDEu"));
		CPPUNIT_ASSERT(SearchDir (dir, "dont.excludethis"));
		CPPUNIT_ASSERT(SearchDir (dir, "xx_not_this_dir_ALWAYSINCLUDE"));

		int64_t sub23id = SearchDir(dir, "sub23");
		CPPUNIT_ASSERT(sub23id != 0);
		dirreply = connection.QueryListDirectory(sub23id, false, 0, false);
		dirstream = connection.ReceiveStream();
		dir.ReadFromStream(*dirstream, connection.GetTimeout());
		CPPUNIT_ASSERT(!SearchDir(dir, "xx_not_this_dir_22"));
		CPPUNIT_ASSERT(!SearchDir(dir, "somefile.excludethis"));
		connection.QueryFinished();
	}
	
	#ifndef WIN32
	// These tests only work as non-root users.
	if(::getuid() != 0)
	{
		// Check that read errors are reported neatly
		// Dir and file which can't be read
		wxFileName unreadableDir = MakeAbsolutePath(testDataDir, 
			_("sub23/read-fail-test-dir"));
		CPPUNIT_ASSERT(wxMkdir(unreadableDir.GetFullPath(), 0000));
		
		wxFileName unreadableFile = MakeAbsolutePath(testDataDir,
			_("read-fail-test-file"));
		wxFile f;
		CPPUNIT_ASSERT(f.Create(unreadableFile.GetFullPath(), false, 
			0000));

		CHECK_BACKUP_OK();
		CHECK_COMPARE_LOC_FAILS(2, 0, 3, 4);

		// Check that it was reported correctly
		CPPUNIT_ASSERT_EQUAL(3, pBackupErrorList->GetCount());
		wxString msg;
		
		msg.Printf(_("Failed to send file '%s': Access denied"), 
			unreadableFile.GetFullPath().c_str());
		CPPUNIT_ASSERT_EQUAL(msg, pBackupErrorList->GetString(0));
		
		msg.Printf(_("Failed to list directory '%s': Access denied"), 
			unreadableDir.GetFullPath().c_str());
		CPPUNIT_ASSERT_EQUAL(msg, pBackupErrorList->GetString(1));
		
		CPPUNIT_ASSERT_EQUAL(wxString(_("Backup Finished")), 
			pBackupErrorList->GetString(2));
		
		CPPUNIT_ASSERT(wxRmdir(unreadableDir.GetFullPath()));
		CPPUNIT_ASSERT(wxRemoveFile(unreadableFile.GetFullPath()));
	}
	#endif

	// check that a file that is newly created is not uploaded
	// until it reaches the minimum file age
	{
		mpConfig->MinimumFileAge.Set(5);
		
		wxFile f;
		CPPUNIT_ASSERT(f.Create(MakeAbsolutePath(testDataDir, 
			_("continuous-update")).GetFullPath()));
		
		// too new
		CHECK_BACKUP_OK();
		CHECK_COMPARE_LOC_FAILS(1, 0, 3, 4);
		
		sleep(5);
		CHECK_BACKUP_OK();
		CHECK_COMPARE_LOC_OK(3, 4);
		
		mpConfig->MinimumFileAge.Set(0);
	}

	// Delete a directory
	DeleteRecursive(MakeAbsolutePath(testDataDir, _("x1")));
	
	// Change attributes on an original file.
	{
		wxCharBuffer buf = MakeAbsolutePath(testDataDir, _("df9834.dsf"))
			.GetFullPath().mb_str(wxConvLibc);
		CPPUNIT_ASSERT(::chmod(buf.data(), 0423) == 0);
	}
	
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);

	// check that attributes are backed up and compared correctly
	{
		wxFileName testFileName = MakeAbsolutePath(testDataDir, 
			_("f1.dat"));
		wxCharBuffer buf = testFileName.GetFullPath().mb_str(wxConvLibc);

		#ifndef WIN32		
		// make one of the files unreadable, expect a compare failure
		struct stat st;
		CPPUNIT_ASSERT(stat(buf.data(), &st) == 0);
		CPPUNIT_ASSERT_EQUAL(0, chmod(buf.data(), 0000));
		CHECK_COMPARE_LOC_FAILS(2, 0, 3, 4);
		#endif
		
		// make one of the files read-only, expect a compare failure
		#ifdef WIN32
		wxString cmd = _("attrib +r ");
		cmd.Append(testFileName);
		wxCharBuffer cmdbuf = cmd.mb_str(wxConvLibc);
		int compareReturnValue = ::system(cmdbuf.data());
		CPPUNIT_ASSERT_EQUAL(0, compareReturnValue);
		#else
		CPPUNIT_ASSERT_EQUAL(0, chmod(buf.data(), 0444));
		#endif
		
		CHECK_COMPARE_LOC_FAILS(1, 0, 3, 4);
	
		// set it back, expect no failures
		#ifdef WIN32
		cmd = _("attrib -r ");
		cmd.Append(testFileName);
		cmdbuf = cmd.mb_str(wxConvLibc);
		compareReturnValue = ::system(cmdbuf.data());
		CPPUNIT_ASSERT_EQUAL(0, compareReturnValue);
		#else
		CPPUNIT_ASSERT_EQUAL(0, chmod(buf.data(), st.st_mode));
		#endif
		
		CHECK_COMPARE_LOC_OK(3, 4);

		// change the timestamp on a file, expect a compare failure
		#ifdef WIN32
		HANDLE handle = openfile(buf.data(), O_RDWR, 0);
		CPPUNIT_ASSERT(handle != INVALID_HANDLE_VALUE);
		FILETIME creationTime, lastModTime, lastAccessTime;
		CPPUNIT_ASSERT(GetFileTime(handle, &creationTime, &lastAccessTime, 
			&lastModTime) != 0);
		CPPUNIT_ASSERT(CloseHandle(handle));
		FILETIME dummyTime = lastModTime;
		dummyTime.dwHighDateTime -= 100;
		#else
		time_t creationTime = st.st_ctime, 
			lastModTime = st.st_mtime, 
			lastAccessTime = st.st_atime,
			dummyTime = lastModTime - 10000;
		#endif

		SetFileTime(buf.data(), dummyTime, lastModTime,	lastAccessTime);
		#ifdef WIN32
		// creation time is backed up, so changing it should cause
		// a compare failure
		CHECK_COMPARE_LOC_FAILS(1, 0, 3, 4);
		#else
		// inode change time is backed up, but not restored or compared
		CHECK_COMPARE_LOC_OK(3, 4);
		#endif		

		// last access time is not backed up, so it cannot be compared
		SetFileTime(buf.data(), creationTime, lastModTime, dummyTime);
		CHECK_COMPARE_LOC_OK(3, 4);
		
		// last modified time is backed up, so changing it should cause
		// a compare failure
		SetFileTime(buf.data(), creationTime, dummyTime, lastAccessTime);
		CHECK_COMPARE_LOC_FAILS(1, 0, 3, 4);

		// set back to original values, check that compare succeeds
		SetFileTime(buf.data(), creationTime, lastModTime, lastAccessTime);
		CHECK_COMPARE_LOC_OK(3, 4);
	}

	{
		// Add some more files and modify others
		// Don't use the timestamp flag this time, so they have 
		// a recent modification time.
		wxFileName zipTest3(MakeAbsolutePath(_("../test/data/test3.zip")));
		Unzip(zipTest3, testDataDir, false);
	}

	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);

	{
		// Rename directory
		wxFileName from = MakeAbsolutePath(testDataDir,
			_("sub23/dhsfdss"));
		wxFileName to = MakeAbsolutePath(testDataDir,
			_("renamed-dir"));
		CPPUNIT_ASSERT(wxRenameFile(from.GetFullPath(), 
			to.GetFullPath()));
		
		CHECK_BACKUP_OK();
		CHECK_COMPARE_LOC_OK(3, 4);
		
		// Rename some files -- one under the threshold, others above
		/*
		from = MakeAbsolutePath(testDataDir,
			_("continousupdate"));
		to = MakeAbsolutePath(testDataDir,
			_("continousupdate-ren"));
		CPPUNIT_ASSERT(wxRenameFile(from.GetFullPath(), 
			to.GetFullPath()));
		*/

		from = MakeAbsolutePath(testDataDir,
			_("df324"));
		to = MakeAbsolutePath(testDataDir,
			_("df324-ren"));
		CPPUNIT_ASSERT(wxRenameFile(from.GetFullPath(), 
			to.GetFullPath()));

		from = MakeAbsolutePath(testDataDir,
			_("sub23/find2perl"));
		to = MakeAbsolutePath(testDataDir,
			_("find2perl-ren"));
		CPPUNIT_ASSERT(wxRenameFile(from.GetFullPath(), 
			to.GetFullPath()));

		CHECK_BACKUP_OK();
		CHECK_COMPARE_LOC_OK(3, 4);

		// Check that modifying files with madly in the future 
		// timestamps still get added

		{
			// Create a new file
			wxFileName fn = MakeAbsolutePath(testDataDir, 
				_("sub23/in-the-future"));
			wxFile f;
			CPPUNIT_ASSERT(f.Create(fn.GetFullPath()));
			CPPUNIT_ASSERT(f.Write(_("Back to the future!\n")));
			f.Close();

			// and then move the time forwards!
			struct timeval times[2];
			BoxTimeToTimeval(GetCurrentBoxTime() + 
				SecondsToBoxTime((time_t)(365*24*60*60)), 
				times[1]);
			times[0] = times[1];
			wxCharBuffer buf = fn.GetFullPath().mb_str(wxConvLibc);
			CPPUNIT_ASSERT(::utimes(buf.data(), times) == 0);
		}

		CHECK_BACKUP_OK();
		CHECK_COMPARE_LOC_OK(3, 4);
	}
	
	// try a restore
	/*
	{
		wxPanel* pRestorePanel = wxDynamicCast
		(
			pMainFrame->FindWindow(ID_Restore_Panel), wxPanel
		);
		CPPUNIT_ASSERT(pRestorePanel);
	
		CPPUNIT_ASSERT(!pRestorePanel->IsShown());	
		ClickButtonWaitEvent(ID_Main_Frame, ID_General_Restore_Button);
		CPPUNIT_ASSERT(pRestorePanel->IsShown());
		
		wxListBox* pLocsList = wxDynamicCast
		(
			pRestorePanel->FindWindow(ID_Function_Source_List), wxListBox
		);
		CPPUNIT_ASSERT(pLocsList);
		CPPUNIT_ASSERT_EQUAL(0, pLocsList->GetCount());
		
		wxPanel* pRestoreFilesPanel = wxDynamicCast
		(
			pMainFrame->FindWindow(ID_Restore_Files_Panel), wxPanel
		);
		CPPUNIT_ASSERT(pRestoreFilesPanel);

		CPPUNIT_ASSERT(!pRestoreFilesPanel->IsShown());	
		ClickButtonWaitEvent(ID_Restore_Panel, ID_Function_Source_Button);
		CPPUNIT_ASSERT(pRestoreFilesPanel->IsShown());
		
		wxTreeCtrl* pRestoreTree = wxDynamicCast
		(
			pRestoreFilesPanel->FindWindow(ID_Server_File_Tree), 
			wxTreeCtrl
		);
		CPPUNIT_ASSERT(pRestoreTree);
		
		wxTreeItemId rootId = pRestoreTree->GetRoot();
		CPPUNIT_ASSERT(rootId.IsOk());
		CPPUNIT_ASSERT_EQUAL(wxString(_("Server root")),
			pRestoreTree->GetItemText(rootId));
		pRestoreTree->Expand(rootId);
		CPPUNIT_ASSERT_EQUAL(1, pRestoreTree->GetChildrenCount(rootId), 
			false);
			
		wxTreeItemIdValue cookie;
		wxTreeItemId loc = pRestoreTree->GetFirstChild(rootId, cookie);
		CPPUNIT_ASSERT(loc.IsOk());
		CPPUNIT_ASSERT_EQUAL(wxString(_("testdata")),
			pRestoreTree->GetItemText(loc));
		pRestoreTree->Expand(loc);
		
		
	}
	*/
	
	// try a restore
	{
		BackupStoreDirectory dir;

		CPPUNIT_ASSERT(conn.ListDirectory(
			BackupProtocolClientListDirectory::RootDirectory,
			BackupProtocolClientListDirectory::Flags_EXCLUDE_NOTHING,
			dir));
		int64_t testId = SearchDir(dir, "testdata");
		CPPUNIT_ASSERT(testId);

		wxFileName restoreDir(tempDir.GetFullPath(), _("restore"));
		CPPUNIT_ASSERT(!restoreDir.DirExists());
		// CPPUNIT_ASSERT(wxMkdir(restoreDir.GetFullPath()));
		
		wxCharBuffer buf = restoreDir.GetFullPath().mb_str(wxConvLibc);
		CPPUNIT_ASSERT_EQUAL((int)Restore_Complete, conn.Restore(
			testId, buf.data(), NULL, NULL, false, false, false));

		conn.Disconnect();
		
		CompareExpectNoDifferences(rClientConfig, tlsContext, 
			_("testdata"), restoreDir);
			
		DeleteRecursive(restoreDir);
	}
	
	// clean up
	CPPUNIT_ASSERT_EQUAL((size_t)1, mpConfig->GetLocations().size());
	mpConfig->RemoveLocation(*pTestDataLocation);
	CPPUNIT_ASSERT_EQUAL((size_t)0, mpConfig->GetLocations().size());
	DeleteRecursive(testDataDir);
	CPPUNIT_ASSERT(wxRemoveFile(storeConfigFileName.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(accountsDb.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(raidConf.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(configFile.GetFullPath()));		
	CPPUNIT_ASSERT(confDir.Rmdir());
	DeleteRecursive(storeDir);
	CPPUNIT_ASSERT(tempDir.Rmdir());
}
