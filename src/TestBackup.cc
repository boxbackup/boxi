/***************************************************************************
 *            TestBackup.cc
 *
 *  Wed May 10 23:10:16 2006
 *  Copyright 2006-2007 Chris Wilson
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

void TestBackupStoreDaemon::Setup() 
{ 
	SetupInInitialProcess(); 
}

bool TestBackupStoreDaemon::Load(const std::string& file) 
{ 
	return LoadConfigurationFile(file); 
}

void TestBackupStoreDaemon::Run()
{ 
	ServerTLS<BOX_PORT_BBSTORED, 1, false>::Run(); 
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
	wxLogNull nolog;
	wxThreadError err = Delete();
	if (err != wxTHREAD_NO_ERROR)
	{
		wxString msg;
		msg.Printf(_("Failed to wait for server thread: error %d"),
			err);
		wxGetApp().ShowMessageBox(BM_TEST_WAIT_FOR_THREAD_FAILED,
			msg, _("Boxi Error"), wxOK | wxICON_ERROR, NULL);
	}
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
	
	std::vector<wxZipEntry*> entries;
	
	for (wxZipEntry* pEntry = zipInput.GetNextEntry();
		pEntry != NULL; pEntry = zipInput.GetNextEntry())
	{
		wxFileName outName = MakeAbsolutePath(rDestDir, 
			pEntry->GetInternalName());

		if (pEntry->IsDir())
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
	
			wxString outNameString(outName.GetFullPath());
			wxFFileOutputStream outFos(outNameString);
			CPPUNIT_ASSERT(outFos.Ok());

			outFos.Write(zipInput);
			outFos.Close();
			/*
			CPPUNIT_ASSERT_EQUAL((int)apEntry->GetSize(),
			       (int)outFos.LastWrite());
			*/

			CPPUNIT_ASSERT(outName.FileExists());
			
			wxFile outFile(outName.GetFullPath());
			wxCharBuffer buf = outName.GetFullPath().mb_str();
			CPPUNIT_ASSERT_EQUAL_MESSAGE(buf.data(), 
				pEntry->GetSize(), outFile.Length());
		}
		
		entries.push_back(pEntry);
	}
	
	for (std::vector<wxZipEntry*>::reverse_iterator ipEntry = entries.rbegin();
		ipEntry != entries.rend(); ipEntry++)
	{	
		wxZipEntry* pEntry = *ipEntry;
		
		if (restoreTimes)
		{
			wxDateTime time = pEntry->GetDateTime();
			struct timeval tvs[2];
			tvs[0].tv_sec  = time.GetTicks();
			tvs[0].tv_usec = 0;
			tvs[1].tv_sec  = time.GetTicks();
			tvs[1].tv_usec = 0;

			wxFileName outName = MakeAbsolutePath(rDestDir, 
				pEntry->GetInternalName());
			wxCharBuffer buf = outName.GetFullPath().mb_str();
			CPPUNIT_ASSERT(::utimes(buf.data(), tvs) == 0);
		}
		
		delete pEntry;
	}
}

std::auto_ptr<wxZipEntry> FindZipEntry(const wxFileName& rZipFile, 
	const wxString& rFileName)
{
	CPPUNIT_ASSERT(rZipFile.FileExists());
	wxFileInputStream zipFis(rZipFile.GetFullPath());
	CPPUNIT_ASSERT(zipFis.Ok());
	wxZipInputStream zipInput(zipFis);
	
	std::auto_ptr<wxZipEntry> apEntry;
	
	for (apEntry.reset(zipInput.GetNextEntry());
		apEntry.get() != NULL; 
		apEntry.reset(zipInput.GetNextEntry()))
	{
		if (apEntry->GetInternalName().IsSameAs(rFileName))
		{
			break;
		}
	}
	
	return apEntry;
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

void TestBackup::TestConfigChecks()
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

	mapServer->Stop();
	CHECK_BACKUP_ERROR(BM_BACKUP_FAILED_CONNECT_FAILED);
	mapServer->Start();

	/*
	TODO: write tests for:
	BM_BACKUP_FAILED_INTERRUPTED,
	BM_BACKUP_FAILED_UNKNOWN_ERROR,
	*/
}

void TestBackup::RunTest()
{	
	{
		wxFileName spaceTestZipFile(_("../test/data/spacetest1.zip"));
		CPPUNIT_ASSERT(spaceTestZipFile.FileExists());
		Unzip(spaceTestZipFile, mTestDataDir);
	}

	// TODO: test BM_BACKUP_FAILED_CANNOT_INIT_ENCRYPTION
	
	TestConfigChecks();
	
	{
		std::string errs;
		wxCharBuffer buf = mClientConfigFile.GetFullPath().mb_str();
		mapClientConfig = Configuration::LoadAndVerify(buf.data(), 
			&BackupDaemonConfigVerify, errs);
		CPPUNIT_ASSERT(mapClientConfig.get());
		CPPUNIT_ASSERT(errs.empty());
	}

	// before the first backup, there should be differences
	CHECK_COMPARE_LOC_FAILS(1, 0, 0, 0);

	// reconfigure to exclude all files but one
	CPPUNIT_ASSERT(mpTestDataLocation);
	MyExcludeList& rExcludeList = mpTestDataLocation->GetExcludeList();
	const MyExcludeEntry::List& rEntries = rExcludeList.GetEntries();
	CPPUNIT_ASSERT_EQUAL((size_t)9, rEntries.size());
	for (MyExcludeEntry::ConstIterator i = rEntries.begin();
		rEntries.size() > 0; 
		i = rEntries.begin())
	{
		rExcludeList.RemoveEntry(*i);
	}
	CPPUNIT_ASSERT_EQUAL((size_t)0, rEntries.size());
	rExcludeList.AddEntry
	(
		MyExcludeEntry
		(
			theExcludeTypes[ETI_EXCLUDE_FILES_REGEX], 
			wxString(_(".*"))
		)
	);
	rExcludeList.AddEntry
	(
		MyExcludeEntry
		(
			theExcludeTypes[ETI_EXCLUDE_DIRS_REGEX], 
			wxString(_(".*"))
		)
	);
	rExcludeList.AddEntry
	(
		MyExcludeEntry
		(
			theExcludeTypes[ETI_ALWAYS_INCLUDE_DIR],
			MakeAbsolutePath(mTestDataDir, 
				_("spacetest")).GetFullPath()
		)
	);
	CPPUNIT_ASSERT_EQUAL((size_t)3, rEntries.size());
	mpConfig->Save(mClientConfigFile.GetFullPath());
		
	// before the first backup, there should be differences
	CHECK_COMPARE_LOC_FAILS(1, 0, 0, 0);

	CHECK_BACKUP_OK();

	// and afterwards, there should be no differences any more
	CHECK_COMPARE_LOC_OK(0, 0);
	
	// restore the default settings
	RemoveDefaultLocation();
	SetupDefaultLocation();

	// now there should be differences again
	CHECK_COMPARE_LOC_FAILS(6, 0, 0, 0);

	CHECK_BACKUP_OK();

	// and afterwards, there should be no differences any more
	CHECK_COMPARE_LOC_OK(0, 0);
	
	// Set limit to something very small
	{
		// About 28 blocks will be used at this point (14 files in RAID)
		CPPUNIT_ASSERT_EQUAL((int64_t)28, 
			GetAccountInfo(mapServer->GetConfiguration())->GetBlocksUsed());

		// Backup will fail if the size used is greater than 
		// soft limit + 1/3 of (hard - soft). 
		// Set small values for limits accordingly.
		SetLimit(mapServer->GetConfiguration(), 2, "10B", "40B");

		// Unpack some more files
		wxFileName spaceTestZipFile(_("../test/data/spacetest2.zip"));
		CPPUNIT_ASSERT(spaceTestZipFile.FileExists());
		Unzip(spaceTestZipFile, mTestDataDir);

		// Delete a file and a directory
		CPPUNIT_ASSERT(wxRemoveFile(MakeAbsolutePath(mTestDataDir,
			_("spacetest/d1/f3")).GetFullPath()));
		DeleteRecursive(MakeAbsolutePath(mTestDataDir, 
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
			GetAccountInfo(mapServer->GetConfiguration())->GetBlocksUsed());
	CHECK_COMPARE_LOC_FAILS(2, 0, 0, 0);
	
	// set the limits back
	SetLimit(mapServer->GetConfiguration(), 2, "1000B", "2000B");
	
	// unpack some more test files
	{
		wxFileName baseFilesZipFile(MakeAbsolutePath(
			_("../test/data/test_base.zip")).GetFullPath());
		CPPUNIT_ASSERT(baseFilesZipFile.FileExists());
		Unzip(baseFilesZipFile, mTestDataDir);
	}
	
	// run a backup
	CHECK_BACKUP_OK();
	
	// check that it worked
	CHECK_COMPARE_LOC_OK(0, 0);
	
	// Delete a file
	CPPUNIT_ASSERT(wxRemoveFile(MakeAbsolutePath(mTestDataDir, 
		_("x1/dsfdsfs98.fd")).GetFullPath()));
	
	#ifndef WIN32
	{
		// New symlink
		wxCharBuffer buf = wxFileName(mTestDataDir.GetFullPath(),
			_("symlink-to-dir")).GetFullPath().mb_str(wxConvLibc);
		CPPUNIT_ASSERT(::symlink("does-not-exist", buf.data()) == 0);
	}
	#endif		

	// Update a file (will be uploaded as a diff)
	{
		// Check that the file is over the diffing threshold in the 
		// bbackupd.conf file
		wxFileName bigFileName(MakeAbsolutePath(mTestDataDir, _("f45.df")));
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
		wxCharBuffer buf = wxFileName(mTestDataDir.GetFullPath(),
			_("symlink-to-dir")).GetFullPath().mb_str(wxConvLibc);
		CPPUNIT_ASSERT(::unlink(buf.data()) == 0);
	}
	#endif
	
	CPPUNIT_ASSERT(wxMkdir(MakeAbsolutePath(mTestDataDir, 
		_("symlink-to-dir")).GetFullPath(), 0755));
	wxFileName dirTestDirName(MakeAbsolutePath(mTestDataDir, 
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

	TestRenameTrackedOverDeleted();
	TestVeryOldFiles();
	TestModifyExisting();	
	TestExclusions();
	TestReadErrors();
	TestMinimumFileAge();
	TestBackupOfAttributes();
	TestAddMoreFiles();
	TestRenameDir();	
	TestRestore();
	CleanUp();
}

void TestBackup::TestRenameTrackedOverDeleted()
{
	// case which went wrong: rename a tracked file over a deleted file
	#ifdef WIN32
	CPPUNIT_ASSERT(wxRemoveFile(MakeAbsolutePath(mTestDataDir, 
		_("x1/dsfdsfs98.fd")).GetFullPath()));
	#endif
	
	CPPUNIT_ASSERT(wxRenameFile(
		MakeAbsolutePath(mTestDataDir, _("df9834.dsf")).GetFullPath(),
		MakeAbsolutePath(mTestDataDir, _("x1/dsfdsfs98.fd")).GetFullPath()));

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);

	// Move that file back
	CPPUNIT_ASSERT(wxRenameFile(
		MakeAbsolutePath(mTestDataDir, _("x1/dsfdsfs98.fd")).GetFullPath(),
		MakeAbsolutePath(mTestDataDir, _("df9834.dsf")).GetFullPath()));
}

void TestBackup::TestVeryOldFiles()
{
	// Add some more files. Because we restore the original
	// time stamps, these files will look very old to the daemon.
	// Lucky it'll upload them then!
	wxFileName test2ZipFile(MakeAbsolutePath(_("../test/data/test2.zip")));
	Unzip(test2ZipFile, mTestDataDir, true);

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);
}

// Modify an existing file
void TestBackup::TestModifyExisting()
{
	wxFileName fileToUpdate(MakeAbsolutePath(mTestDataDir,
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

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);
}

// check that the excluded files did not make it
// into the store, and the included files did
void TestBackup::TestExclusions()
{
	// Add some files and directories which are marked as excluded
	wxFileName zipExcludeTest(MakeAbsolutePath(_("../test/data/testexclude.zip")));
	Unzip(zipExcludeTest, mTestDataDir, true);
	
	// backup should still work
	CHECK_BACKUP_OK();
	
	// compare location (with exclude lists) should pass
	CHECK_COMPARE_LOC_OK(3, 4);
	
	// compare directly (without exclude lists) should fail
	CHECK_COMPARE_FAILS(7, 0);

	SocketStreamTLS socket;
	socket.Open(mTlsContext, Socket::TypeINET, 
		mapClientConfig->GetKeyValue("StoreHostname").c_str(), 
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
		mapClientConfig->GetKeyValueInt("AccountNumber"),
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

void TestBackup::TestReadErrors()
{
	#ifndef WIN32
	// These tests only work as non-root users.
	if (::getuid() == 0)
	{
		return;
	}
	
	// Check that read errors are reported neatly
	// Dir and file which can't be read
	wxFileName unreadableDir = MakeAbsolutePath(mTestDataDir, 
		_("sub23/read-fail-test-dir"));
	CPPUNIT_ASSERT(wxMkdir(unreadableDir.GetFullPath(), 0000));
	
	wxFileName unreadableFile = MakeAbsolutePath(mTestDataDir,
		_("read-fail-test-file"));
	wxFile f;
	CPPUNIT_ASSERT(f.Create(unreadableFile.GetFullPath(), false, 
		0000));

	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_FAILS(2, 0, 3, 4);

	// Check that it was reported correctly
	CPPUNIT_ASSERT_EQUAL((unsigned int)3,
		mpBackupErrorList->GetCount());
	wxString msg;
	
	msg.Printf(_("Failed to send file '%s': Access denied"), 
		unreadableFile.GetFullPath().c_str());
	CPPUNIT_ASSERT_EQUAL(msg, mpBackupErrorList->GetString(0));
	
	msg.Printf(_("Failed to list directory '%s': Access denied"), 
		unreadableDir.GetFullPath().c_str());
	CPPUNIT_ASSERT_EQUAL(msg, mpBackupErrorList->GetString(1));
	
	CPPUNIT_ASSERT_EQUAL(wxString(_("Backup Finished")), 
		mpBackupErrorList->GetString(2));
	
	CPPUNIT_ASSERT(wxRmdir(unreadableDir.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(unreadableFile.GetFullPath()));
	#endif
}

// check that a file that is newly created is not uploaded
// until it reaches the minimum file age
void TestBackup::TestMinimumFileAge()
{
	mpConfig->MinimumFileAge.Set(5);
	
	wxFile f;
	CPPUNIT_ASSERT(f.Create(MakeAbsolutePath(mTestDataDir, 
		_("continuous-update")).GetFullPath()));
	
	// too new
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_FAILS(1, 0, 3, 4);
	
	sleep(5);
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);
	
	mpConfig->MinimumFileAge.Set(0);
}

// check that attributes are backed up and compared correctly
void TestBackup::TestBackupOfAttributes()
{
	// Delete a directory
	DeleteRecursive(MakeAbsolutePath(mTestDataDir, _("x1")));
	
	// Change attributes on an original file.
	{
		wxCharBuffer buf = MakeAbsolutePath(mTestDataDir, _("df9834.dsf"))
			.GetFullPath().mb_str(wxConvLibc);
		CPPUNIT_ASSERT(::chmod(buf.data(), 0423) == 0);
	}
	
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);

	wxFileName testFileName = MakeAbsolutePath(mTestDataDir, 
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
	cmd.Append(testFileName.GetFullPath());
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
	cmd.Append(testFileName.GetFullPath());
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

void TestBackup::TestAddMoreFiles()
{
	// Add some more files and modify others
	// Don't use the timestamp flag this time, so they have 
	// a recent modification time.
	wxFileName zipTest3(MakeAbsolutePath(_("../test/data/test3.zip")));
	Unzip(zipTest3, mTestDataDir, false);

	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);
}

void TestBackup::TestRenameDir()
{
	// Rename directory
	wxFileName from = MakeAbsolutePath(mTestDataDir,
		_("sub23/dhsfdss"));
	wxFileName to = MakeAbsolutePath(mTestDataDir,
		_("renamed-dir"));
	CPPUNIT_ASSERT(wxRenameFile(from.GetFullPath(), 
		to.GetFullPath()));
	
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);
	
	// Rename some files -- one under the threshold, others above
	/*
	from = MakeAbsolutePath(mTestDataDir,
		_("continousupdate"));
	to = MakeAbsolutePath(mTestDataDir,
		_("continousupdate-ren"));
	CPPUNIT_ASSERT(wxRenameFile(from.GetFullPath(), 
		to.GetFullPath()));
	*/

	from = MakeAbsolutePath(mTestDataDir,
		_("df324"));
	to = MakeAbsolutePath(mTestDataDir,
		_("df324-ren"));
	CPPUNIT_ASSERT(wxRenameFile(from.GetFullPath(), 
		to.GetFullPath()));

	from = MakeAbsolutePath(mTestDataDir,
		_("sub23/find2perl"));
	to = MakeAbsolutePath(mTestDataDir,
		_("find2perl-ren"));
	CPPUNIT_ASSERT(wxRenameFile(from.GetFullPath(), 
		to.GetFullPath()));

	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);

	// Check that modifying files with madly in the future 
	// timestamps still get added

	// Create a new file
	wxFileName fn = MakeAbsolutePath(mTestDataDir, 
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

	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);
}

// try a restore
void TestBackup::TestRestore()
{
	BackupStoreDirectory dir;

	CPPUNIT_ASSERT(mapConn->ListDirectory(
		BackupProtocolClientListDirectory::RootDirectory,
		BackupProtocolClientListDirectory::Flags_EXCLUDE_NOTHING,
		dir));
	int64_t testId = SearchDir(dir, "testdata");
	CPPUNIT_ASSERT(testId);

	wxFileName remStoreDir(mBaseDir.GetFullPath(), _("restore"));
	CPPUNIT_ASSERT(!remStoreDir.DirExists());
	// CPPUNIT_ASSERT(wxMkdir(remStoreDir.GetFullPath()));
	
	wxCharBuffer buf = remStoreDir.GetFullPath().mb_str(wxConvLibc);
	CPPUNIT_ASSERT_EQUAL((int)Restore_Complete, mapConn->Restore(
		testId, buf.data(), NULL, NULL, false, false, false));

	mapConn->Disconnect();
	
	CompareExpectNoDifferences(*mapClientConfig, mTlsContext, 
		_("testdata"), remStoreDir);
		
	DeleteRecursive(remStoreDir);
}

void TestBackup::CleanUp()
{	
	// clean up
	DeleteRecursive(mTestDataDir);
	CPPUNIT_ASSERT(wxRemoveFile(mStoreConfigFileName.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mAccountsFile.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mRaidConfigFile.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mClientConfigFile.GetFullPath()));		
	CPPUNIT_ASSERT(mConfDir.Rmdir());
	DeleteRecursive(mStoreDir);
	CPPUNIT_ASSERT(mBaseDir.Rmdir());
}
