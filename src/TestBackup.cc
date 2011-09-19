/***************************************************************************
 *            TestBackup.cc
 *
 *  Wed May 10 23:10:16 2006
 *  Copyright 2006-2008 Chris Wilson
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

#include "SandBox.h"

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

#include "Box.h"
#include "BackupClientRestore.h"
#include "BackupStoreDaemon.h"
#include "RaidFileController.h"
#include "BackupStoreAccountDatabase.h"
#include "BackupStoreAccounts.h"
#include "autogen_BackupProtocol.h"
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
	BOXI_ASSERT(rZipFile.FileExists());
	wxFileInputStream zipFis(rZipFile.GetFullPath());
	BOXI_ASSERT(zipFis.Ok());
	wxZipInputStream zipInput(zipFis);
	
	std::vector<wxZipEntry*> entries;
	
	for (wxZipEntry* pEntry = zipInput.GetNextEntry();
		pEntry != NULL; pEntry = zipInput.GetNextEntry())
	{
		wxFileName outName = MakeAbsolutePath(rDestDir, 
			pEntry->GetInternalName());

		if (pEntry->IsDir())
		{
			BOXI_ASSERT(!outName.FileExists());
			outName = wxFileName(outName.GetFullPath(), wxT(""));
			if (!outName.DirExists())
			{
				outName.Mkdir(0700);
				BOXI_ASSERT(outName.DirExists());
			}
		}
		else
		{
			wxString outNameString(outName.GetFullPath());
			BOXI_ASSERT(!wxFileName(outNameString, wxT("")).DirExists());
			
			wxFFileOutputStream outFos(outNameString);
			BOXI_ASSERT(outFos.Ok());

			outFos.Write(zipInput);
			outFos.Close();

			BOXI_ASSERT(outName.FileExists());
			
			wxFile outFile(outNameString);
			wxCharBuffer buf = outNameString.mb_str();
			CPPUNIT_ASSERT_EQUAL_MESSAGE(buf.data(), 
				pEntry->GetSize(), outFile.Length());
		}
		
		if (restoreTimes)
		{
			entries.push_back(pEntry);
		}
	}
	
	for (std::vector<wxZipEntry*>::reverse_iterator ipEntry = entries.rbegin();
		ipEntry != entries.rend(); ipEntry++)
	{	
		wxZipEntry* pEntry = *ipEntry;
		
		wxDateTime time = pEntry->GetDateTime();
		struct timeval tvs[2];
		tvs[0].tv_sec  = time.GetTicks();
		tvs[0].tv_usec = 0;
		tvs[1].tv_sec  = time.GetTicks();
		tvs[1].tv_usec = 0;

		wxFileName outName = MakeAbsolutePath(rDestDir, 
			pEntry->GetInternalName());
		wxCharBuffer buf = outName.GetFullPath().mb_str();
		BOXI_ASSERT(::utimes(buf.data(), tvs) == 0);
		
		delete pEntry;
	}
}

std::auto_ptr<wxZipEntry> FindZipEntry(const wxFileName& rZipFile, 
	const wxString& rFileName)
{
	BOXI_ASSERT(rZipFile.FileExists());
	wxFileInputStream zipFis(rZipFile.GetFullPath());
	BOXI_ASSERT(zipFis.Ok());
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

	BOXI_ASSERT(gotLock);

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
	BOXI_ASSERT(db->EntryExists(ID));
	
	// Load it in
	BackupStoreAccounts acc(*db);
	std::string rootDir;
	int discSet;
	acc.GetAccountRoot(ID, rootDir, discSet);
	
	// Attempt to lock
	NamedLock writeLock;
	BOXI_ASSERT(GetWriteLockOnAccount(writeLock, rootDir, discSet));

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

box_time_t GetLatestFileUploadTime(const Configuration& rClientConfig)
{
	// Try and work out the time before which all files should be on the server
	std::string syncTimeFilename =
		rClientConfig.GetKeyValue("DataDirectory") + 
		DIRECTORY_SEPARATOR + "last_sync_start";

	// Stat it to get file time
	EMU_STRUCT_STAT st;
	if(EMU_STAT(syncTimeFilename.c_str(), &st) == 0)
	{
		// Files modified after this time shouldn't be on the 
		// server, so report errors slightly differently
		return FileModificationTime(st)
			- SecondsToBoxTime(rClientConfig.GetKeyValueInt(
				"MinimumFileAge"));
	}

	return 0;
}

void CompareBackup(const Configuration& rClientConfig, TLSContext& rTlsContext,
	BackupQueries::CompareParams& rParams, const wxString& rRemoteDir,
	const wxFileName& rLocalPath)
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
		std::auto_ptr<BackupProtocolVersion> serverVersion
		(
			connection.QueryVersion(BACKUP_STORE_SERVER_VERSION)
		);
		BOXI_ASSERT_EQUAL(BACKUP_STORE_SERVER_VERSION,
			serverVersion->GetVersion());
	}

	// Login -- if this fails, the Protocol will exception
	connection.QueryLogin
	(
		rClientConfig.GetKeyValueInt("AccountNumber"),
		BackupProtocolLogin::Flags_ReadOnly
	);

	// Set up a context for our work
	BackupQueries query(connection, rClientConfig, false /* read-write */);

	wxCharBuffer remote = rRemoteDir.mb_str(wxConvBoxi);
	wxCharBuffer local  = rLocalPath.GetFullPath().mb_str(wxConvBoxi);
	query.Compare(remote.data(), local.data(), rParams);

	connection.QueryFinished();
}

void CompareExpectNoDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const wxString& rRemoteDir, 
	const wxFileName& rLocalPath)
{
	BackupQueries::CompareParams params(false, // QuickCompare
		false, // IgnoreExcludes
		false, // IgnoreAttributes
		GetLatestFileUploadTime(rClientConfig));
	params.mQuietCompare = false;
	CompareBackup(rClientConfig, rTlsContext, params, rRemoteDir,
		rLocalPath);

	BOXI_ASSERT_EQUAL(0, params.mDifferences);
	BOXI_ASSERT_EQUAL(0, params.mDifferencesExplainedByModTime);
	BOXI_ASSERT_EQUAL(0, params.mExcludedDirs);
	BOXI_ASSERT_EQUAL(0, params.mExcludedFiles);
}

void CompareExpectDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const wxString& rRemoteDir, 
	const wxFileName& rLocalPath, int numDiffs, int numDiffsModTime)
{
	BackupQueries::CompareParams params(false, // QuickCompare
		false, // IgnoreExcludes
		false, // IgnoreAttributes
		GetLatestFileUploadTime(rClientConfig));
	params.mQuietCompare = true;
	CompareBackup(rClientConfig, rTlsContext, params, rRemoteDir,
		rLocalPath);

	BOXI_ASSERT_EQUAL(numDiffs, params.mDifferences);
	BOXI_ASSERT_EQUAL(numDiffsModTime, 
		params.mDifferencesExplainedByModTime);
	BOXI_ASSERT_EQUAL(0, params.mExcludedDirs);
	BOXI_ASSERT_EQUAL(0, params.mExcludedFiles);
}

static std::auto_ptr<BackupStoreInfo> GetAccountInfo(const Configuration& rConfig)
{
	// Load in the account database 
	std::auto_ptr<BackupStoreAccountDatabase> db(BackupStoreAccountDatabase::Read(rConfig.GetKeyValue("AccountDatabase").c_str()));

	// Account exists?
	BOXI_ASSERT(db->EntryExists(2));

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
	BOXI_ASSERT(locations.SubConfigurationExists(rLocationName.c_str()));
	const Configuration &loc(locations.GetSubConfiguration(rLocationName.c_str()));
	
	// Generate the exclude lists
	if(!rParams.IgnoreExcludes())
	{
		rParams.LoadExcludeLists(loc);
	}
			
	// Then get it compared
	std::string localPathString = std::string("/") + rLocationName;
	wxString remotePath(localPathString.c_str(), wxConvBoxi);
	wxString localPath(loc.GetKeyValue("Path").c_str(), wxConvBoxi);
	CompareBackup(rConfig, rTlsContext, rParams, 
		remotePath, localPath);
}

void CompareLocationExpectNoDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const std::string& rLocationName,
	int excludedDirs, int excludedFiles)
{
	BackupQueries::CompareParams params(false, // QuickCompare
		false, // IgnoreExcludes
		false, // IgnoreAttributes
		GetLatestFileUploadTime(rClientConfig));
	params.mQuietCompare = false;
	CompareLocation(rClientConfig, rTlsContext, rLocationName, params);

	BOXI_ASSERT_EQUAL(0, params.mDifferences);
	BOXI_ASSERT_EQUAL(0, params.mDifferencesExplainedByModTime);
	BOXI_ASSERT_EQUAL(0, params.mUncheckedFiles);
	BOXI_ASSERT_EQUAL(excludedDirs,  params.mExcludedDirs);
	BOXI_ASSERT_EQUAL(excludedFiles, params.mExcludedFiles);
}

void CompareLocationExpectDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const std::string& rLocationName, 
	int numDiffs, int numDiffsModTime, int numUnchecked, int excludedDirs,
	int excludedFiles)
{
	BackupQueries::CompareParams params(false, // QuickCompare
		false, // IgnoreExcludes
		false, // IgnoreAttributes
		GetLatestFileUploadTime(rClientConfig));
	params.mQuietCompare = true;
	CompareLocation(rClientConfig, rTlsContext, rLocationName, params);

	BOXI_ASSERT_EQUAL(numDiffs, params.mDifferences);
	BOXI_ASSERT_EQUAL(numDiffsModTime, 
		params.mDifferencesExplainedByModTime);
	BOXI_ASSERT_EQUAL(numUnchecked,  params.mUncheckedFiles);
	BOXI_ASSERT_EQUAL(excludedDirs,  params.mExcludedDirs);
	BOXI_ASSERT_EQUAL(excludedFiles, params.mExcludedFiles);
}

int64_t SearchDir(BackupStoreDirectory& rDir, const std::string& rChildName)
{
	BackupStoreDirectory::Iterator i(rDir);
	BackupStoreFilenameClear child(rChildName.c_str());
	BackupStoreDirectory::Entry *en = i.FindMatchingClearName(child);
	if (en == 0) return 0;
	int64_t id = en->GetObjectID();
	BOXI_ASSERT(id > 0);
	BOXI_ASSERT(id != BackupProtocolListDirectory::RootDirectory);
	return id;
}

#ifdef WIN32
void SetFileTime(const char* filename, FILETIME creationTime, 
	FILETIME lastModTime, FILETIME lastAccessTime)
{
	HANDLE handle = openfile(filename, O_RDWR, 0);
	BOXI_ASSERT(handle != INVALID_HANDLE_VALUE);
	BOXI_ASSERT(SetFileTime(handle, &creationTime, &lastAccessTime,
		&lastModTime));
	BOXI_ASSERT(CloseHandle(handle));
}
#else
void SetFileTime(const char* filename, time_t creationTime /* not used */, 
	time_t lastModTime, time_t lastAccessTime)
{
	struct utimbuf ut;
	ut.actime  = lastAccessTime;
	ut.modtime = lastModTime;
	BOXI_ASSERT(utime(filename, &ut) == 0);
}
#endif

void TestBackup::TestConfigChecks()
{
	#define CHECK_PROPERTY(property) \
	CHECK_BACKUP_ERROR(BM_BACKUP_FAILED_CONFIG_ERROR); \
	mpConfig->property.Revert()

	#define CHECK_UNSET_PROPERTY(property) \
	mpConfig->property.Clear(); \
	CHECK_PROPERTY(property)
	
	CHECK_UNSET_PROPERTY(StoreHostname);
	CHECK_UNSET_PROPERTY(KeysFile);
	CHECK_UNSET_PROPERTY(AccountNumber);
	CHECK_UNSET_PROPERTY(MinimumFileAge);
	CHECK_UNSET_PROPERTY(MaxUploadWait);
	CHECK_UNSET_PROPERTY(FileTrackingSizeThreshold);
	CHECK_UNSET_PROPERTY(DiffingUploadSizeThreshold);
	
	// TODO: test BM_BACKUP_FAILED_INVALID_SYNC_PERIOD
	/*
	mpConfig->MinimumFileAge.Set(GetCurrentBoxTime() + 1000000);
	CHECK_PROPERTY(MinimumFileAge, BM_BACKUP_FAILED_INVALID_SYNC_PERIOD);
	*/

	#undef CHECK_UNSET_PROPERTY
	#undef CHECK_PROPERTY

	StopServer();
	CHECK_BACKUP_ERROR(BM_BACKUP_FAILED_CONNECT_FAILED);
	StartServer();

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
		BOXI_ASSERT(spaceTestZipFile.FileExists());
		Unzip(spaceTestZipFile, mTestDataDir);
	}

	// TODO: test BM_BACKUP_FAILED_CANNOT_INIT_ENCRYPTION
	
	TestConfigChecks();
	
	// before the first backup, there should be differences
	CHECK_COMPARE_LOC_FAILS(1, 0, 0, 0, 0);

	// reconfigure to exclude all files but one, the directory "spacetest"
	BOXI_ASSERT(mpTestDataLocation);
	BoxiExcludeList& rExcludeList = mpTestDataLocation->GetExcludeList();
	const BoxiExcludeEntry::List& rEntries = rExcludeList.GetEntries();
	BOXI_ASSERT_EQUAL((size_t)9, rEntries.size());
	for (BoxiExcludeEntry::ConstIterator i = rEntries.begin();
		rEntries.size() > 0; 
		i = rEntries.begin())
	{
		rExcludeList.RemoveEntry(*i);
	}
	BOXI_ASSERT_EQUAL((size_t)0, rEntries.size());
	rExcludeList.AddEntry
	(
		BoxiExcludeEntry
		(
			theExcludeTypes[ETI_EXCLUDE_FILES_REGEX], 
			wxString(_(".*"))
		)
	);
	rExcludeList.AddEntry
	(
		BoxiExcludeEntry
		(
			theExcludeTypes[ETI_EXCLUDE_DIRS_REGEX], 
			wxString(_(".*"))
		)
	);
	rExcludeList.AddEntry
	(
		BoxiExcludeEntry
		(
			theExcludeTypes[ETI_ALWAYS_INCLUDE_DIR],
			MakeAbsolutePath(mTestDataDir, 
				_("spacetest")).GetFullPath()
		)
	);
	BOXI_ASSERT_EQUAL((size_t)3, rEntries.size());
	mpConfig->Save(mClientConfigFile.GetFullPath());
		
	// before the first backup, there should be differences
	CHECK_COMPARE_LOC_FAILS(1, 0, 0, 0, 0);

	CHECK_BACKUP_OK();

	// and afterwards, there should be no differences any more,
	// but 5 dirs and 2 files inside /testdata/spacetest are excluded
	CHECK_COMPARE_LOC_OK(5, 2);
	
	// restore the default settings
	RemoveDefaultLocation();
	SetupDefaultLocation();

	// now there should be differences again, but no more excluded files
	CHECK_COMPARE_LOC_FAILS(7, 0, 0, 0, 0);

	CHECK_BACKUP_OK();

	// and after another backup, there should be no differences any more
	CHECK_COMPARE_LOC_OK(0, 0);
	
	// Set limit to something very small
	{
		// About 28 blocks will be used at this point (14 files in RAID)
		BOXI_ASSERT_EQUAL((int64_t)28, 
			GetAccountInfo(mapServer->GetConfiguration())->GetBlocksUsed());

		// Backup will fail if the size used is greater than the
		// hard limit.
		SetLimit(mapServer->GetConfiguration(), 2, "10B", "29B");
		
		// ensure that newly modified files have a different timestamp
		wxSleep(1);

		// Unpack some more files
		wxFileName spaceTestZipFile(_("../test/data/spacetest2.zip"));
		CPPUNIT_ASSERT(spaceTestZipFile.FileExists());
		Unzip(spaceTestZipFile, mTestDataDir);

		// Delete a file and a directory
		BOXI_ASSERT(wxRemoveFile(MakeAbsolutePath(mTestDataDir,
			_("spacetest/d1/f3")).GetFullPath()));
		DeleteRecursive(MakeAbsolutePath(mTestDataDir, 
			_("spacetest/d3/d4/")));
	}
	
	// check the number of differences before backup
	// 1 file and 1 dir added (dir d8 contains one file, f7, not counted)
	// and 1 file and 1 dir removed (mod times not checked)
	CHECK_COMPARE_LOC_FAILS(4, 2, 0, 0, 0);
	
	// fixme: should return an error
	mpConfig->ExtendedLogging.Set(true);
	CHECK_BACKUP_ERROR(BM_BACKUP_FAILED_STORE_FULL);
	
	// backup should not complete, so there should still be differences
	// the deleted file and directory should have been deleted on the store,
	// but the locally changed files should not have been uploaded
	BOXI_ASSERT_EQUAL((int64_t)28, 
			GetAccountInfo(mapServer->GetConfiguration())->GetBlocksUsed());
	CHECK_COMPARE_LOC_FAILS(2, 0, 0, 0, 0);

	mpConfig->ExtendedLogging.Set(false);

	// set the limits back
	SetLimit(mapServer->GetConfiguration(), 2, "1000B", "2000B");
	
	// unpack some more test files
	{
		wxFileName baseFilesZipFile(MakeAbsolutePath(
			_("../test/data/test_base.zip")).GetFullPath());
		BOXI_ASSERT(baseFilesZipFile.FileExists());
		Unzip(baseFilesZipFile, mTestDataDir);
	}
	
	// run a backup
	CHECK_BACKUP_OK();
	
	// check that it worked
	CHECK_COMPARE_LOC_OK(0, 0);
	
	// Delete a file
	BOXI_ASSERT(wxRemoveFile(MakeAbsolutePath(mTestDataDir, 
		_("x1/dsfdsfs98.fd")).GetFullPath()));
	
	#ifndef WIN32
	{
		// New symlink
		wxCharBuffer buf = wxFileName(mTestDataDir.GetFullPath(),
			_("symlink-to-dir")).GetFullPath().mb_str(wxConvBoxi);
		BOXI_ASSERT(::symlink("does-not-exist", buf.data()) == 0);
	}
	#endif		

	// Update a file (will be uploaded as a diff)
	{
		// Check that the file is over the diffing threshold in the 
		// bbackupd.conf file
		wxFileName bigFileName(MakeAbsolutePath(mTestDataDir, _("f45.df")));
		wxFile bigFile(bigFileName.GetFullPath(), wxFile::read_write);
		BOXI_ASSERT(bigFile.Length() > 1024);
		
		// Add a bit to the end
		BOXI_ASSERT(bigFile.IsOpened());
		BOXI_ASSERT(bigFile.SeekEnd() != wxInvalidOffset);
		BOXI_ASSERT(bigFile.Write(_("EXTRA STUFF")));
		BOXI_ASSERT(bigFile.Length() > 1024);
	}

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);

	// Bad case: delete a file/symlink, replace it with a directory
	#ifndef WIN32
	{
		// Delete symlink
		wxCharBuffer buf = wxFileName(mTestDataDir.GetFullPath(),
			_("symlink-to-dir")).GetFullPath().mb_str(wxConvBoxi);
		BOXI_ASSERT(::unlink(buf.data()) == 0);
	}
	#endif
	
	BOXI_ASSERT(wxMkdir(MakeAbsolutePath(mTestDataDir, 
		_("symlink-to-dir")).GetFullPath(), 0755));
	wxFileName dirTestDirName(MakeAbsolutePath(mTestDataDir, 
		_("x1/dir-to-file")));
	BOXI_ASSERT(wxMkdir(dirTestDirName.GetFullPath(), 0755));
	// NOTE: create a file within the directory to avoid deletion by the 
	// housekeeping process later
	wxFileName placeHolderName(dirTestDirName.GetFullPath(), _("contents"));
	
	#ifdef WIN32
	{
		wxFile f;
		BOXI_ASSERT(f.Create(placeHolderName.GetFullPath()));
	}
	#else // !WIN32
	{
		wxCharBuffer buf = placeHolderName.GetFullPath().mb_str(wxConvBoxi);
		BOXI_ASSERT(::symlink("does-not-exist", buf.data()) == 0);
	}
	#endif

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);

	// And the inverse, replace a directory with a file/symlink
	BOXI_ASSERT(wxRemoveFile(placeHolderName.GetFullPath()));
	BOXI_ASSERT(wxRmdir(dirTestDirName.GetFullPath()));
	
	#ifdef WIN32
	{
		wxFile f;
		BOXI_ASSERT(f.Create(dirTestDirName.GetFullPath()));
	}
	#else // !WIN32
	{
		wxCharBuffer buf = dirTestDirName.GetFullPath().mb_str(wxConvBoxi);
		BOXI_ASSERT(::symlink("does-not-exist", buf.data()) == 0);
	}
	#endif
	
	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);

	// And then, put it back to how it was before.
	BOXI_ASSERT(wxRemoveFile(dirTestDirName.GetFullPath()));
	BOXI_ASSERT(wxMkdir(dirTestDirName.GetFullPath(), 0755));
	wxFileName placeHolderName2(dirTestDirName.GetFullPath(), _("contents2"));
	#ifdef WIN32
	{
		wxFile f;
		BOXI_ASSERT(f.Create(placeHolderName2.GetFullPath()));
	}
	#else // !WIN32
	{
		wxCharBuffer buf = placeHolderName2.GetFullPath().mb_str(wxConvBoxi);
		BOXI_ASSERT(::symlink("does-not-exist", buf.data()) == 0);
	}
	#endif

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);
	
	// And finally, put it back to how it was before it was put back to 
	// how it was before. This gets lots of nasty things in the store with 
	// directories over other old directories.
	BOXI_ASSERT(wxRemoveFile(placeHolderName2.GetFullPath()));
	BOXI_ASSERT(wxRmdir(dirTestDirName.GetFullPath()));
	
	#ifdef WIN32
	{
		wxFile f;
		BOXI_ASSERT(f.Create(dirTestDirName.GetFullPath()));
	}
	#else // !WIN32
	{
		wxCharBuffer buf = dirTestDirName.GetFullPath().mb_str(wxConvBoxi);
		BOXI_ASSERT(::symlink("does-not-exist", buf.data()) == 0);
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
	wxString source = MakeAbsolutePath(mTestDataDir,
		_("df9834.dsf")).GetFullPath();
	wxString target = MakeAbsolutePath(mTestDataDir, 
		_("x1/dsfdsfs98.fd")).GetFullPath();

	BOXI_ASSERT(wxFileExists(source));
	BOXI_ASSERT(!wxFileExists(target));
	
	BOXI_ASSERT(wxRenameFile(source, target));

	// Run another backup and check that it works
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);

	// Move that file back
	BOXI_ASSERT(wxRenameFile(target, source));
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
	BOXI_ASSERT(f.IsOpened());
	BOXI_ASSERT(f.SeekEnd() != wxInvalidOffset);
	BOXI_ASSERT(f.Write(_("MODIFIED!\n")));
	f.Close();
	
	// and then move the time backwards!
	struct timeval times[2];
	BoxTimeToTimeval(SecondsToBoxTime((time_t)(365*24*60*60)), times[1]);
	times[0] = times[1];
	wxCharBuffer buf = fileToUpdate.GetFullPath().mb_str(wxConvBoxi);
	BOXI_ASSERT(::utimes(buf.data(), times) == 0);

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
		mpConfig->StoreHostname.GetString(), BOX_PORT_BBSTORED);
	BackupProtocolClient connection(socket);
	connection.Handshake();
	std::auto_ptr<BackupProtocolVersion> 
		serverVersion(connection.QueryVersion(
			BACKUP_STORE_SERVER_VERSION));
	if(serverVersion->GetVersion() != 
		BACKUP_STORE_SERVER_VERSION)
	{
		THROW_EXCEPTION(BackupStoreException, 
			WrongServerVersion);
	}
	connection.QueryLogin(
		mpConfig->AccountNumber.GetInt(),
		BackupProtocolLogin::Flags_ReadOnly);
	
	int64_t rootDirId = BackupProtocolListDirectory::RootDirectory;
	std::auto_ptr<BackupProtocolSuccess> dirreply(
		connection.QueryListDirectory(
			rootDirId, false, 0, false));
	std::auto_ptr<IOStream> dirstream(
		connection.ReceiveStream());
	BackupStoreDirectory dir;
	dir.ReadFromStream(*dirstream, connection.GetTimeout());

	int64_t testDirId = SearchDir(dir, "testdata");
	BOXI_ASSERT(testDirId != 0);
	dirreply = connection.QueryListDirectory(testDirId, false, 0, false);
	dirstream = connection.ReceiveStream();
	dir.ReadFromStream(*dirstream, connection.GetTimeout());
	
	BOXI_ASSERT(!SearchDir(dir, "excluded_1"));
	BOXI_ASSERT(!SearchDir(dir, "excluded_2"));
	BOXI_ASSERT(!SearchDir(dir, "exclude_dir"));
	BOXI_ASSERT(!SearchDir(dir, "exclude_dir_2"));
	// xx_not_this_dir_22 should not be excluded by
	// ExcludeDirsRegex, because it's a file
	BOXI_ASSERT(SearchDir (dir, "xx_not_this_dir_22"));
	BOXI_ASSERT(!SearchDir(dir, "zEXCLUDEu"));
	BOXI_ASSERT(SearchDir (dir, "dont.excludethis"));
	BOXI_ASSERT(SearchDir (dir, "xx_not_this_dir_ALWAYSINCLUDE"));

	int64_t sub23id = SearchDir(dir, "sub23");
	BOXI_ASSERT(sub23id != 0);
	dirreply = connection.QueryListDirectory(sub23id, false, 0, false);
	dirstream = connection.ReceiveStream();
	dir.ReadFromStream(*dirstream, connection.GetTimeout());
	BOXI_ASSERT(!SearchDir(dir, "xx_not_this_dir_22"));
	BOXI_ASSERT(!SearchDir(dir, "somefile.excludethis"));
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
	BOXI_ASSERT(wxMkdir(unreadableDir.GetFullPath(), 0000));
	
	wxFileName unreadableFile = MakeAbsolutePath(mTestDataDir,
		_("read-fail-test-file"));
	wxFile f;
	BOXI_ASSERT(f.Create(unreadableFile.GetFullPath(), false, 
		0000));

	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_FAILS(1, 0, 1, 3, 4);

	// Check that it was reported correctly
	BOXI_ASSERT_EQUAL((unsigned int)3,
		mpBackupErrorList->GetCount());
	wxString msg;
	
	msg.Printf(_("Failed to send file '%s': Access denied"), 
		unreadableFile.GetFullPath().c_str());
	BOXI_ASSERT_EQUAL(msg, mpBackupErrorList->GetString(0));
	
	msg.Printf(_("Failed to list directory '%s': Access denied"), 
		unreadableDir.GetFullPath().c_str());
	BOXI_ASSERT_EQUAL(msg, mpBackupErrorList->GetString(1));
	
	BOXI_ASSERT_EQUAL(wxString(_("Backup Finished")), 
		mpBackupErrorList->GetString(2));
	
	BOXI_ASSERT(wxRmdir(unreadableDir.GetFullPath()));
	BOXI_ASSERT(wxRemoveFile(unreadableFile.GetFullPath()));
	#endif
}

// check that a file that is newly created is not uploaded
// until it reaches the minimum file age
void TestBackup::TestMinimumFileAge()
{
	mpConfig->MinimumFileAge.Set(5);
	
	wxFile f;
	BOXI_ASSERT(f.Create(MakeAbsolutePath(mTestDataDir, 
		_("continuous-update")).GetFullPath()));
	
	// too new
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_FAILS(1, 1, 0, 3, 4);
	
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
			.GetFullPath().mb_str(wxConvBoxi);
		BOXI_ASSERT(::chmod(buf.data(), 0423) == 0);
	}
	
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);

	wxFileName testFileName = MakeAbsolutePath(mTestDataDir, 
		_("f1.dat"));
	wxCharBuffer buf = testFileName.GetFullPath().mb_str(wxConvBoxi);

	#ifndef WIN32		
	// make one of the files unreadable, expect a compare failure
	struct stat st;
	BOXI_ASSERT(stat(buf.data(), &st) == 0);
	BOXI_ASSERT_EQUAL(0, chmod(buf.data(), 0000));
	// different attribs, and not checked because local file unreadable
	CHECK_COMPARE_LOC_FAILS(1, 0, 1, 3, 4);
	#endif
	
	// make one of the files read-only, expect a compare failure
	#ifdef WIN32
	wxString cmd = _("attrib +r ");
	cmd.Append(testFileName.GetFullPath());
	wxCharBuffer cmdbuf = cmd.mb_str(wxConvBoxi);
	int compareReturnValue = ::system(cmdbuf.data());
	BOXI_ASSERT_EQUAL(0, compareReturnValue);
	#else
	BOXI_ASSERT_EQUAL(0, chmod(buf.data(), 0444));
	#endif
	
	CHECK_COMPARE_LOC_FAILS(1, 0, 0, 3, 4);

	// set it back, expect no failures
	#ifdef WIN32
	cmd = _("attrib -r ");
	cmd.Append(testFileName.GetFullPath());
	cmdbuf = cmd.mb_str(wxConvBoxi);
	compareReturnValue = ::system(cmdbuf.data());
	BOXI_ASSERT_EQUAL(0, compareReturnValue);
	#else
	BOXI_ASSERT_EQUAL(0, chmod(buf.data(), st.st_mode));
	#endif
	
	CHECK_COMPARE_LOC_OK(3, 4);

	// change the timestamp on a file, expect a compare failure
	#ifdef WIN32
	HANDLE handle = openfile(buf.data(), O_RDWR, 0);
	BOXI_ASSERT(handle != INVALID_HANDLE_VALUE);
	FILETIME creationTime, lastModTime, lastAccessTime;
	BOXI_ASSERT(GetFileTime(handle, &creationTime, &lastAccessTime, 
		&lastModTime) != 0);
	BOXI_ASSERT(CloseHandle(handle));
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
	CHECK_COMPARE_LOC_FAILS(1, 0, 0, 3, 4);
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
	CHECK_COMPARE_LOC_FAILS(1, 0, 0, 3, 4);

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
	BOXI_ASSERT(wxRenameFile(from.GetFullPath(), 
		to.GetFullPath()));
	
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);
	
	// Rename some files -- one under the threshold, others above
	/*
	from = MakeAbsolutePath(mTestDataDir,
		_("continousupdate"));
	to = MakeAbsolutePath(mTestDataDir,
		_("continousupdate-ren"));
	BOXI_ASSERT(wxRenameFile(from.GetFullPath(), 
		to.GetFullPath()));
	*/

	from = MakeAbsolutePath(mTestDataDir,
		_("df324"));
	to = MakeAbsolutePath(mTestDataDir,
		_("df324-ren"));
	BOXI_ASSERT(wxRenameFile(from.GetFullPath(), 
		to.GetFullPath()));

	from = MakeAbsolutePath(mTestDataDir,
		_("sub23/find2perl"));
	to = MakeAbsolutePath(mTestDataDir,
		_("find2perl-ren"));
	BOXI_ASSERT(wxRenameFile(from.GetFullPath(), 
		to.GetFullPath()));

	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);

	// Check that modifying files with madly in the future 
	// timestamps still get added

	// Create a new file
	wxFileName fn = MakeAbsolutePath(mTestDataDir, 
		_("sub23/in-the-future"));
	wxFile f;
	BOXI_ASSERT(f.Create(fn.GetFullPath()));
	BOXI_ASSERT(f.Write(_("Back to the future!\n")));
	f.Close();

	// and then move the time forwards!
	struct timeval times[2];
	BoxTimeToTimeval(GetCurrentBoxTime() + 
		SecondsToBoxTime((time_t)(365*24*60*60)), 
		times[1]);
	times[0] = times[1];
	wxCharBuffer buf = fn.GetFullPath().mb_str(wxConvBoxi);
	BOXI_ASSERT(::utimes(buf.data(), times) == 0);

	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);
}

// try a restore
void TestBackup::TestRestore()
{
	BackupStoreDirectory dir;

	BOXI_ASSERT(mapConn->ListDirectory(
		BackupProtocolListDirectory::RootDirectory,
		BackupProtocolListDirectory::Flags_EXCLUDE_NOTHING,
		dir));
	int64_t testId = SearchDir(dir, "testdata");
	BOXI_ASSERT(testId);

	wxFileName remStoreDir(mBaseDir);
	remStoreDir.AppendDir(_("restore"));
	BOXI_ASSERT(!remStoreDir.DirExists());
	// BOXI_ASSERT(wxMkdir(remStoreDir.GetFullPath()));
	
	wxCharBuffer buf = remStoreDir.GetPath().mb_str();
	BOXI_ASSERT_EQUAL((int)Restore_Complete, mapConn->Restore(
		testId, buf.data(), NULL, NULL, false, false, false));

	mapConn->Disconnect();
	
	CompareExpectNoDifferences(mpConfig->GetBoxConfig(), mTlsContext, 
		_("testdata"), remStoreDir);
		
	DeleteRecursive(remStoreDir);
}

void TestBackup::CleanUp()
{	
	// clean up
	DeleteRecursive(mTestDataDir);
	BOXI_ASSERT(wxRemoveFile(mStoreConfigFileName.GetFullPath()));
	BOXI_ASSERT(wxRemoveFile(mAccountsFile.GetFullPath()));
	BOXI_ASSERT(wxRemoveFile(mRaidConfigFile.GetFullPath()));
	BOXI_ASSERT(wxRemoveFile(mClientConfigFile.GetFullPath()));		
	BOXI_ASSERT(mConfDir.Rmdir());
	DeleteRecursive(mStoreDir);
	BOXI_ASSERT(mBaseDir.Rmdir());
}
