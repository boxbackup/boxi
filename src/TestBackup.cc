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
#include "BackupContext.h"
#include "autogen_BackupProtocolServer.h"
#include "BackupStoreConfigVerify.h"
#include "BackupQueries.h"
#include "BackupDaemonConfigVerify.h"
#include "BackupClientCryptoKeys.h"
#include "BackupStoreConstants.h"
#include "BackupStoreInfo.h"
#include "StoreStructure.h"
#include "NamedLock.h"
#include "BackupClientMakeExcludeList.h"

#include "main.h"
#include "BoxiApp.h"
#include "ClientConfig.h"
#include "MainFrame.h"
#include "SetupWizard.h"
#include "SslConfig.h"
#include "TestBackup.h"
#include "TestBackupConfig.h"
#include "FileTree.h"

#include "ServerConnection.h"

#undef TLS_CLASS_IMPLEMENTATION_CPP

class TestBackupStoreDaemon
: public ServerTLS<BOX_PORT_BBSTORED, 1, false>,
  public HousekeepingInterface
{
	public:
	TestBackupStoreDaemon::TestBackupStoreDaemon()
	: mStateKnownCondition(mConditionLock),
	  mStateListening(false), mStateDead(false)
	{ }
	
	virtual void Run()   { ServerTLS<BOX_PORT_BBSTORED, 1, false>::Run(); }
	virtual void Setup() { SetupInInitialProcess(); }
	virtual bool Load(const std::string& file) 
	{ 
		return LoadConfigurationFile(file); 
	}
	
	wxMutex     mConditionLock;
	wxCondition mStateKnownCondition;
	bool        mStateListening;
	bool        mStateDead;

	virtual void SendMessageToHousekeepingProcess(const void *Msg, int MsgLen)
	{ }

	protected:
	virtual void NotifyListenerIsReady();
	virtual void Connection(SocketStreamTLS &rStream);
	virtual const ConfigurationVerify *GetConfigVerify() const
	{
		return &BackupConfigFileVerify;
	}

	private:
	void SetupInInitialProcess();
	BackupStoreAccountDatabase *mpAccountDatabase;
	BackupStoreAccounts *mpAccounts;
};

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

class StoreServerThread : public wxThread
{
	TestBackupStoreDaemon mDaemon;

	public:
	StoreServerThread(const wxString& rFilename)
	: wxThread(wxTHREAD_JOINABLE)
	{
		wxCharBuffer buf = rFilename.mb_str(wxConvLibc);
		mDaemon.Load(buf.data());
		mDaemon.Setup();
	}

	~StoreServerThread()
	{
		Stop();
	}

	void Start();
	void Stop();
	
	const Configuration& GetConfiguration()
	{
		return mDaemon.GetConfiguration();
	}

	private:
	virtual ExitCode Entry();
};

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

class StoreServer
{
	private:
	std::auto_ptr<StoreServerThread> mapThread;
	wxString mConfigFile;

	public:
	StoreServer(const wxString& rFilename)
	: mConfigFile(rFilename)
	{ }

	~StoreServer()
	{
		if (mapThread.get())
		{
			Stop();
		}
	}

	void Start();
	void Stop();

	const Configuration& GetConfiguration()
	{
		if (!mapThread.get()) 
		{
			throw "not running";
		}
		
		return mapThread->GetConfiguration();
	}
};

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

static void Unzip(const wxFileName& rZipFile, const wxFileName& rDestDir,
	bool restoreTimes = false)
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
			CPPUNIT_ASSERT(!outName.FileExists());
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

static void CompareBackup(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, BackupQueries::CompareParams& rParams,
	const wxString& rRemoteDir, const wxFileName& rLocalPath)
{
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

static void CompareExpectNoDifferences(const Configuration& rClientConfig, 
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

static void CompareExpectDifferences(const Configuration& rClientConfig, 
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

static void CompareLocation(const Configuration& rConfig, 
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
	CompareBackup(rConfig, rTlsContext, rParams, 
		std::string("/") + rLocation, loc.GetKeyValue("Path"));
	
	// Delete exclude lists
	rParams.DeleteExcludeLists();
}

static void CompareLocationExpectNoDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const std::string& rLocationName)
{
	BackupQueries::CompareParams params;
	params.mQuiet = false;
	CompareLocation(rClientConfig, rTlsContext, params, rLocationName);

	CPPUNIT_ASSERT_EQUAL(0, params.mDifferences);
	CPPUNIT_ASSERT_EQUAL(0, 
		params.mDifferencesExplainedByModTime);
	CPPUNIT_ASSERT_EQUAL(0, params.mExcludedDirs);
	CPPUNIT_ASSERT_EQUAL(0, params.mExcludedFiles);
}

static void CompareLocationExpectDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const std::string& rLocationName, 
	int numDiffs, int numDiffsModTime)
{
	BackupQueries::CompareParams params;
	params.mQuiet = true;
	CompareLocation(rClientConfig, rTlsContext, params, rLocationName);

	CPPUNIT_ASSERT_EQUAL(numDiffs, params.mDifferences);
	CPPUNIT_ASSERT_EQUAL(numDiffsModTime, 
		params.mDifferencesExplainedByModTime);
	CPPUNIT_ASSERT_EQUAL(0, params.mExcludedDirs);
	CPPUNIT_ASSERT_EQUAL(0, params.mExcludedFiles);
}

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
	
	Location* pTestDataLocation = NULL;
	
	{
		wxFileName spaceTestZipFile(_("../test/data/spacetest1.zip"));
		CPPUNIT_ASSERT(spaceTestZipFile.FileExists());
		Unzip(spaceTestZipFile, testDataDir);

		CPPUNIT_ASSERT_EQUAL((size_t)0, mpConfig->GetLocations().size());
		
		{
			Location testDirLoc(_("testdata"), testDataDir.GetFullPath(),
				mpConfig);
			mpConfig->AddLocation(testDirLoc);
			CPPUNIT_ASSERT_EQUAL((size_t)1, 
				mpConfig->GetLocations().size());
			pTestDataLocation = mpConfig->GetLocation(testDirLoc);
			CPPUNIT_ASSERT(pTestDataLocation);
		}

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

	#define CHECK_COMPARE_LOC_OK() \
	CompareLocationExpectNoDifferences(rClientConfig, tlsContext, "testdata");

	#define CHECK_COMPARE_LOC_FAILS(diffs, modified) \
	CompareLocationExpectDifferences(rClientConfig, tlsContext, "testdata", \
		diffs, modified);

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
	
	std::string errs;
	wxCharBuffer buf = configFile.GetFullPath().mb_str(wxConvLibc);
	std::auto_ptr<Configuration> apClientConfig
	(
		Configuration::LoadAndVerify(buf.data(), 
			&BackupDaemonConfigVerify, errs)
	);
	CPPUNIT_ASSERT(apClientConfig.get());
	CPPUNIT_ASSERT(errs.empty());
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
	CHECK_COMPARE_LOC_FAILS(1, 0);
	
	CHECK_BACKUP_OK();

	// and afterwards, there should be no differences any more
	CHECK_COMPARE_LOC_OK();
	
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
	CHECK_COMPARE_LOC_FAILS(4, 2);
	
	// fixme: should return an error
	mpConfig->ExtendedLogging.Set(true);
	CHECK_BACKUP_ERROR(BM_BACKUP_FAILED_STORE_FULL);
	
	// backup should not complete, so there should still be differences
	// the deleted file and directory should have been deleted on the store,
	// but the locally changed files should not have been uploaded
	CPPUNIT_ASSERT_EQUAL((int64_t)28, 
			GetAccountInfo(server.GetConfiguration())->GetBlocksUsed());
	CHECK_COMPARE_LOC_FAILS(2, 2);
	
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
	CHECK_COMPARE_LOC_OK();
	
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
	CHECK_COMPARE_LOC_OK();

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
	CHECK_COMPARE_LOC_OK();

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
	CHECK_COMPARE_LOC_OK();

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
	CHECK_COMPARE_LOC_OK();
	
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
	CHECK_COMPARE_LOC_OK();

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
	CHECK_COMPARE_LOC_OK();

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
	CHECK_COMPARE_LOC_OK();

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
	CHECK_COMPARE_LOC_OK();
	
	// Add some files and directories which are marked as excluded
	wxFileName zipExcludeTest(MakeAbsolutePath(_("../test/data/testexclude.zip")));
	Unzip(zipExcludeTest, testDataDir);
	
	// backup should still work
	CHECK_BACKUP_OK();
	
	// compare location (with exclude lists) should pass
	CHECK_COMPARE_LOC_OK();
	
	// compare directly (without exclude lists) should fail
	CHECK_COMPARE_FAILS(8, 0);

	// clean up
#ifndef WIN32
	{
		// Delete symlink
		wxCharBuffer buf = dirTestDirName.GetFullPath().mb_str(wxConvLibc);
		CPPUNIT_ASSERT(::unlink(buf.data()) == 0);
	}
#endif		
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
