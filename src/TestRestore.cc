/***************************************************************************
 *            TestRestore.cc
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
#include "RaidFileController.h"
#include "BackupDaemonConfigVerify.h"
#include "BackupClientCryptoKeys.h"
#include "BackupStoreConstants.h"
#include "BackupStoreException.h"

#include "main.h"
#include "BoxiApp.h"
#include "ClientConfig.h"
#include "MainFrame.h"
#include "TestBackup.h"
#include "TestRestore.h"
#include "TestBackupConfig.h"
#include "FileTree.h"
#include "Restore.h"
#include "ServerConnection.h"
#include "RestorePanel.h"
#include "RestoreProgressPanel.h"
#include "RestoreFilesPanel.h"

#undef TLS_CLASS_IMPLEMENTATION_CPP

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestRestore, "WxGuiTest");

CppUnit::Test *TestRestore::suite()
{
	CppUnit::TestSuite *suiteOfTests =
		new CppUnit::TestSuite("TestRestore");
	suiteOfTests->addTest(
		new CppUnit::TestCaller<TestRestore>(
			"TestRestore",
			&TestRestore::RunTest));
	return suiteOfTests;
}

void TestRestore::setUp()
{
	// create a working directory
	mBaseDir.AssignTempFileName(wxT("boxi-testrestore-tempdir-"));
	CPPUNIT_ASSERT(wxRemoveFile(mBaseDir.GetFullPath()));
	CPPUNIT_ASSERT(wxMkdir     (mBaseDir.GetFullPath(), 0700));
	CPPUNIT_ASSERT(mBaseDir.DirExists());
	
	// create a directory to hold config files for the tests
	mConfDir = wxFileName(mBaseDir.GetFullPath(), _("conf"));
	CPPUNIT_ASSERT(wxMkdir(mConfDir.GetFullPath(), 0700));
	CPPUNIT_ASSERT(mConfDir.DirExists());
	
	// create a directory to hold bbstored's store temporarily
	mStoreDir = wxFileName(mBaseDir.GetFullPath(), _("store"));
	CPPUNIT_ASSERT(wxMkdir(mStoreDir.GetFullPath(), 0700));
	CPPUNIT_ASSERT(mStoreDir.DirExists());

	// create raidfile.conf for the store daemon
	mRaidConfigFile = wxFileName(mConfDir.GetFullPath(), _("raidfile.conf"));
	CPPUNIT_ASSERT(!mRaidConfigFile.FileExists());
	
	{
		wxFileName mStoreDir0(mStoreDir.GetFullPath(), _("0_0"));
		CPPUNIT_ASSERT(wxMkdir(mStoreDir0.GetFullPath(), 0700));
		CPPUNIT_ASSERT(mStoreDir0.DirExists());
	
		wxFileName mStoreDir1(mStoreDir.GetFullPath(), _("0_1"));
		CPPUNIT_ASSERT(wxMkdir(mStoreDir1.GetFullPath(), 0700));
		CPPUNIT_ASSERT(mStoreDir1.DirExists());
	
		wxFileName mStoreDir2(mStoreDir.GetFullPath(), _("0_2"));
		CPPUNIT_ASSERT(wxMkdir(mStoreDir2.GetFullPath(), 0700));
		CPPUNIT_ASSERT(mStoreDir2.DirExists());
	
		wxFile mRaidConfigFileFile;
		CPPUNIT_ASSERT(mRaidConfigFileFile.Create(mRaidConfigFile.GetFullPath()));
		
		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(_("disc0\n")));
		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(_("{\n")));
		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(_("\tSetNumber = 0\n")));
		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(_("\tBlockSize = 2048\n")));

		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(_("\tDir0 = ")));
		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(mStoreDir0.GetFullPath()));
		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(_("\n")));

		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(_("\tDir1 = ")));
		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(mStoreDir1.GetFullPath()));
		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(_("\n")));
		
		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(_("\tDir2 = ")));
		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(mStoreDir2.GetFullPath()));
		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(_("\n")));

		CPPUNIT_ASSERT(mRaidConfigFileFile.Write(_("}\n")));
		mRaidConfigFileFile.Close();
	}
	
	// create accounts.txt for the store daemon
	mAccountsFile = wxFileName(mConfDir.GetFullPath(), _("accounts.txt"));
	CPPUNIT_ASSERT(!mAccountsFile.FileExists());
	
	{
		wxFile mAccountsFileFile;
		CPPUNIT_ASSERT(mAccountsFileFile.Create(mAccountsFile.GetFullPath()));
	}
	
	// create bbstored.conf for the store daemon
	mStoreConfigFileName = wxFileName(mConfDir.GetFullPath(), _("bbstored.conf"));
	
	{
		wxFile storeConfigFile;
		CPPUNIT_ASSERT(storeConfigFile.Create(mStoreConfigFileName.GetFullPath()));
		
		CPPUNIT_ASSERT(mRaidConfigFile.FileExists());
		CPPUNIT_ASSERT(storeConfigFile.Write(_("RaidFileConf = ")));
		CPPUNIT_ASSERT(storeConfigFile.Write(mRaidConfigFile.GetFullPath()));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\n")));
		
		wxFileName mAccountsFile(mConfDir.GetFullPath(), _("accounts.txt"));
		CPPUNIT_ASSERT(mAccountsFile.FileExists());
		CPPUNIT_ASSERT(storeConfigFile.Write(_("AccountDatabase = ")));
		CPPUNIT_ASSERT(storeConfigFile.Write(mAccountsFile.GetFullPath()));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\n")));
		
		CPPUNIT_ASSERT(storeConfigFile.Write(_("TimeBetweenHousekeeping = 900\n")));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("Server\n")));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("{\n")));
	
		wxFileName pidFile(mConfDir.GetFullPath(), _("bbstored.pid"));
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

	CPPUNIT_ASSERT(mStoreConfigFileName.FileExists());

	{
		wxString command = _("../boxbackup/release/bin/bbstoreaccounts"
			"/bbstoreaccounts -c ");
		command.Append(mStoreConfigFileName.GetFullPath());
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
	mTestDataDir = wxFileName(mBaseDir.GetFullPath(), _("testdata"));
	CPPUNIT_ASSERT(mTestDataDir.Mkdir(0700));
	
	mpMainFrame = GetMainFrame();
	CPPUNIT_ASSERT(mpMainFrame);
	
	mpConfig = mpMainFrame->GetConfig();
	CPPUNIT_ASSERT(mpConfig);
	
	mapServer.reset(new StoreServer(mStoreConfigFileName.GetFullPath()));
	mapServer->Start();

	mpMainFrame->DoFileOpen(MakeAbsolutePath(
		_("../test/config/bbackupd.conf")).GetFullPath());
	wxString msg;
	bool isOk = mpConfig->Check(msg);

	if (!isOk)
	{
		wxCharBuffer buf = msg.mb_str(wxConvLibc);
		CPPUNIT_ASSERT_MESSAGE(buf.data(), isOk);
	}
	
	mapConn.reset(new ServerConnection(mpConfig));
	TLSContext tls;
	
	isOk = mapConn->InitTlsContext(tls, msg);
	if (!isOk)
	{
		wxCharBuffer buf = msg.mb_str(wxConvLibc);
		CPPUNIT_ASSERT_MESSAGE(buf.data(), isOk);
	}
	
	BackupStoreDirectory dir;

	CPPUNIT_ASSERT(mapConn->ListDirectory(
		BackupProtocolClientListDirectory::RootDirectory,
		BackupProtocolClientListDirectory::Flags_EXCLUDE_NOTHING,
		dir));
	
	mapConn->Disconnect();
	
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

	CPPUNIT_ASSERT_EQUAL((size_t)0, mpConfig->GetLocations().size());
		
	mpTestDataLocation = NULL;	
	Location testDirLoc(_("testdata"), mTestDataDir.GetFullPath(),
		mpConfig);
	mpConfig->AddLocation(testDirLoc);
	CPPUNIT_ASSERT_EQUAL((size_t)1, 
		mpConfig->GetLocations().size());
	mpTestDataLocation = mpConfig->GetLocation(testDirLoc);
	CPPUNIT_ASSERT(mpTestDataLocation);

	MyExcludeList& rExcludes = mpTestDataLocation->GetExcludeList();

	#define ADD_ENTRY(type, path) \
	rExcludes.AddEntry(MyExcludeEntry(theExcludeTypes[type], path))
	
	ADD_ENTRY(ETI_EXCLUDE_FILE, MakeAbsolutePath(mTestDataDir,_("excluded_1")).GetFullPath());
	ADD_ENTRY(ETI_EXCLUDE_FILE, MakeAbsolutePath(mTestDataDir,_("excluded_2")).GetFullPath());
	ADD_ENTRY(ETI_EXCLUDE_FILES_REGEX, _("\\.excludethis$"));
	ADD_ENTRY(ETI_EXCLUDE_FILES_REGEX, _("EXCLUDE"));
	ADD_ENTRY(ETI_ALWAYS_INCLUDE_FILE, MakeAbsolutePath(mTestDataDir,_("dont.excludethis")).GetFullPath());
	ADD_ENTRY(ETI_EXCLUDE_DIR, MakeAbsolutePath(mTestDataDir,_("exclude_dir")).GetFullPath());
	ADD_ENTRY(ETI_EXCLUDE_DIR, MakeAbsolutePath(mTestDataDir,_("exclude_dir_2")).GetFullPath());
	ADD_ENTRY(ETI_EXCLUDE_DIRS_REGEX, _("not_this_dir"));
	ADD_ENTRY(ETI_ALWAYS_INCLUDE_DIRS_REGEX, _("ALWAYSINCLUDE"));
	
	#undef ADD_ENTRY

	CPPUNIT_ASSERT_EQUAL((size_t)1, mpConfig->GetLocations().size());

	// helps with debugging
	mClientConfigFile = wxFileName(mConfDir.GetFullPath(), _("bbackupd.conf"));
	mpConfig->Save(mClientConfigFile.GetFullPath());
	CPPUNIT_ASSERT(mClientConfigFile.FileExists());

	{
		std::string errs;
		wxCharBuffer buf = mClientConfigFile.GetFullPath().mb_str(wxConvLibc);
		mapClientConfig = Configuration::LoadAndVerify(buf.data(), 
			&BackupDaemonConfigVerify, errs);
		CPPUNIT_ASSERT(mapClientConfig.get());
		CPPUNIT_ASSERT(errs.empty());
	}
	
	SSLLib::Initialise();
	// Read in the certificates creating a TLS context
	std::string certFile(mapClientConfig->GetKeyValue("CertificateFile"));
	std::string keyFile (mapClientConfig->GetKeyValue("PrivateKeyFile"));
	std::string caFile  (mapClientConfig->GetKeyValue("TrustedCAsFile"));
	mTlsContext.Initialise(false /* as client */, 
		certFile.c_str(), keyFile.c_str(), caFile.c_str());
	
	// Initialise keys
	BackupClientCryptoKeys_Setup(mapClientConfig->GetKeyValue("KeysFile").c_str());
}

void CompareFiles(const wxFileName& first, const wxFileName& second)
{
	wxFile a(first.GetFullPath());
	wxFile b(second.GetFullPath());
	CPPUNIT_ASSERT(a.IsOpened());
	CPPUNIT_ASSERT(b.IsOpened());
	CPPUNIT_ASSERT_EQUAL(a.Length(), b.Length());
	char buffer1[4096], buffer2[sizeof(buffer1)];
	for (int pos = 0; pos < a.Length(); pos += sizeof(buffer1))
	{
		int toread = sizeof(buffer1);
		if (toread > a.Length() - pos)
		{
			toread = a.Length() - pos;
		}
		CPPUNIT_ASSERT_EQUAL(toread, a.Read(buffer1, toread));
		CPPUNIT_ASSERT_EQUAL(toread, b.Read(buffer2, toread));
		CPPUNIT_ASSERT_EQUAL(0, memcmp(buffer1, buffer2, toread));
	}
}

static wxTreeItemId GetItemIdFromPath(wxTreeCtrl* pTreeCtrl, wxTreeItemId root, 
	wxString path)
{
	wxTreeItemId none;
	
	wxString pathComponent;
	int indexOfSlash = path.Find(_("/"));
	if (indexOfSlash == -1)
	{
		pathComponent = path;
		path = wxEmptyString;
	}
	else
	{
		CPPUNIT_ASSERT(indexOfSlash > 0);
		pathComponent = path.Mid(0, indexOfSlash);
		path = path.Mid(indexOfSlash + 1);
	}
	CPPUNIT_ASSERT(pathComponent.Length() > 0);
	
	pTreeCtrl->Expand(root);
	
	wxTreeItemIdValue cookie;
	for (wxTreeItemId entry = pTreeCtrl->GetFirstChild(root, cookie);
		entry.IsOk(); entry = pTreeCtrl->GetNextChild(root, cookie))
	{
		if (pTreeCtrl->GetItemText(entry).IsSameAs(pathComponent))
		{
			if (path.Length() == 0)
			{
				// this is it
				return entry;
			}
			else
			{
				// it's below this item
				return GetItemIdFromPath(pTreeCtrl, entry,
					path);
			}
		}
	}
	
	return none;
}

void TestRestore::RunTest()
{
	const Configuration &rClientConfig(*mapClientConfig);

	wxPanel* pBackupPanel = wxDynamicCast
	(
		mpMainFrame->FindWindow(ID_Backup_Panel), wxPanel
	);
	CPPUNIT_ASSERT(pBackupPanel);

	CPPUNIT_ASSERT(!pBackupPanel->IsShown());	
	ClickButtonWaitEvent(ID_Main_Frame, ID_General_Backup_Button);
	CPPUNIT_ASSERT(pBackupPanel->IsShown());
	
	// makes tests faster
	mpConfig->MinimumFileAge.Set(0);
	
	{
		wxFileName spaceTestZipFile(_("../test/data/spacetest1.zip"));
		CPPUNIT_ASSERT(spaceTestZipFile.FileExists());
		Unzip(spaceTestZipFile, mTestDataDir);
	}

	{
		wxFileName baseFilesZipFile(MakeAbsolutePath(
			_("../test/data/test_base.zip")).GetFullPath());
		CPPUNIT_ASSERT(baseFilesZipFile.FileExists());
		Unzip(baseFilesZipFile, mTestDataDir);
	}

	{
		wxFileName test2ZipFile(MakeAbsolutePath(_("../test/data/test2.zip")));
		CPPUNIT_ASSERT(test2ZipFile.FileExists());
		Unzip(test2ZipFile, mTestDataDir, true);
	}

	{
		// Add some files and directories which are marked as excluded
		wxFileName zipExcludeTest(MakeAbsolutePath(_("../test/data/testexclude.zip")));
		CPPUNIT_ASSERT(zipExcludeTest.FileExists());
		Unzip(zipExcludeTest, mTestDataDir, true);
	}
	
	{
		// Add some more files and modify others
		// Don't use the timestamp flag this time, so they have 
		// a recent modification time.
		wxFileName zipTest3(MakeAbsolutePath(_("../test/data/test3.zip")));
		Unzip(zipTest3, mTestDataDir, false);
	}

	wxPanel* pBackupProgressPanel = wxDynamicCast
	(
		mpMainFrame->FindWindow(ID_Backup_Progress_Panel), wxPanel
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

	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);
	
	RestorePanel* pRestorePanel = wxDynamicCast
	(
		mpMainFrame->FindWindow(ID_Restore_Panel), RestorePanel
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
		mpMainFrame->FindWindow(ID_Restore_Files_Panel), wxPanel
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
	
	wxTreeItemId rootId = pRestoreTree->GetRootItem();
	CPPUNIT_ASSERT(rootId.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("/ (server root)")),
		pRestoreTree->GetItemText(rootId));
	pRestoreTree->Expand(rootId);
	CPPUNIT_ASSERT_EQUAL((size_t)1, pRestoreTree->GetChildrenCount(rootId, 
		false));
		
	wxTreeItemIdValue cookie;
	wxTreeItemId loc = pRestoreTree->GetFirstChild(rootId, cookie);
	CPPUNIT_ASSERT(loc.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("testdata")),
		pRestoreTree->GetItemText(loc));
	pRestoreTree->Expand(loc);
	
	FileImageList images;
	
	// select a single file to restore
	for (wxTreeItemId entry = pRestoreTree->GetFirstChild(loc, cookie);
		entry.IsOk(); entry = pRestoreTree->GetNextChild(loc, cookie))
	{
		if (pRestoreTree->GetItemText(entry).IsSameAs(_("df9834.dsf")))
		{
			ActivateTreeItemWaitEvent(pRestoreTree, entry);
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
				pRestoreTree->GetItemImage(entry));
		}
		else
		{
			CPPUNIT_ASSERT_EQUAL(images.GetEmptyImageId(),
				pRestoreTree->GetItemImage(entry));
		}
	}

	CPPUNIT_ASSERT_EQUAL(images.GetPartialImageId(),
		pRestoreTree->GetItemImage(loc));
	CPPUNIT_ASSERT_EQUAL(images.GetPartialImageId(),
		pRestoreTree->GetItemImage(rootId));
	
	CPPUNIT_ASSERT_EQUAL(1, pLocsList->GetCount());
	CPPUNIT_ASSERT_EQUAL(wxString(_("+ /testdata/df9834.dsf")),
		pLocsList->GetString(0));
	
	RestoreProgressPanel* pRestoreProgressPanel = wxDynamicCast
	(
		mpMainFrame->FindWindow(ID_Restore_Progress_Panel), RestoreProgressPanel
	);
	CPPUNIT_ASSERT(pRestoreProgressPanel);

	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown());
	MessageBoxSetResponse(BM_RESTORE_FAILED_INVALID_DESTINATION_PATH, wxOK);
	ClickButtonWaitEvent(ID_Restore_Panel, ID_Function_Start_Button);
	MessageBoxCheckFired();
	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown());

	wxRadioButton* pNewLocRadio = wxDynamicCast
	(
		pRestorePanel->FindWindow(ID_Restore_Panel_New_Location_Radio), 
		wxRadioButton
	);
	CPPUNIT_ASSERT(pNewLocRadio);
	ClickRadioWaitEvent(pNewLocRadio);
	
	wxTextCtrl* pNewLocText = wxDynamicCast
	(
		pRestorePanel->FindWindow(ID_Restore_Panel_New_Location_Text), 
		wxTextCtrl
	);
	CPPUNIT_ASSERT(pNewLocText);
	wxFileName restoreDest(mBaseDir.GetFullPath(), _("restore"));
	SetTextCtrlValue(pNewLocText, restoreDest.GetFullPath());

	wxListBox* pRestoreErrorList = wxDynamicCast
	(
		pRestoreProgressPanel->FindWindow(ID_BackupProgress_ErrorList), 
		wxListBox
	);
	CPPUNIT_ASSERT(pRestoreErrorList);
	CPPUNIT_ASSERT_EQUAL(0, pRestoreErrorList->GetCount());

	#define CHECK_RESTORE_OK(files, bytes) \
	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown()); \
	ClickButtonWaitEvent(ID_Restore_Panel, ID_Function_Start_Button); \
	CPPUNIT_ASSERT(pRestoreProgressPanel->IsShown()); \
	if (pRestoreErrorList->GetCount() != 1) \
	{ \
		wxString msg = pRestoreErrorList->GetString( \
			pRestoreErrorList->GetCount() - 1); \
		wxCharBuffer buf = msg.mb_str(wxConvLibc); \
		CPPUNIT_ASSERT_MESSAGE(buf.data(), \
			1 == pRestoreErrorList->GetCount()); \
	} \
	CPPUNIT_ASSERT_EQUAL(1, pRestoreErrorList->GetCount()); \
	CPPUNIT_ASSERT_EQUAL(wxString(_("Restore Finished")), \
		pRestoreErrorList->GetString(0)); \
	ClickButtonWaitEvent(ID_Restore_Progress_Panel, wxID_CANCEL); \
	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown()); \
	mpMainFrame->GetConnection()->Disconnect(); \
	CPPUNIT_ASSERT_EQUAL(wxString(_(#files)), \
		pRestoreProgressPanel->GetNumFilesTotalString()); \
	CPPUNIT_ASSERT_EQUAL(wxString(_(bytes)), \
		pRestoreProgressPanel->GetNumBytesTotalString()); \
	CPPUNIT_ASSERT_EQUAL(wxString(_(#files)), \
		pRestoreProgressPanel->GetNumFilesDoneString()); \
	CPPUNIT_ASSERT_EQUAL(wxString(_(bytes)), \
		pRestoreProgressPanel->GetNumBytesDoneString()); \
	CPPUNIT_ASSERT_EQUAL(wxString(_("0")), \
		pRestoreProgressPanel->GetNumFilesRemainingString()); \
	CPPUNIT_ASSERT_EQUAL(wxString(_("0 B")), \
		pRestoreProgressPanel->GetNumBytesRemainingString()); \
	CPPUNIT_ASSERT_EQUAL(files, pRestoreProgressPanel->GetProgressPos()); \
	CPPUNIT_ASSERT_EQUAL(files, pRestoreProgressPanel->GetProgressMax());
	
	CHECK_RESTORE_OK(1, "18 kB");
	
	CPPUNIT_ASSERT(restoreDest.DirExists());
	wxFileName testdataRestored = MakeAbsolutePath(restoreDest, 
		_("testdata"));
	CPPUNIT_ASSERT(testdataRestored.DirExists());
	wxFileName df9834_dsfRestored = MakeAbsolutePath(testdataRestored, 
		_("df9834.dsf"));
	CPPUNIT_ASSERT(df9834_dsfRestored.FileExists());
	wxFileName df9834_dsfOriginal = MakeAbsolutePath(mTestDataDir, 
		_("df9834.dsf"));
	CompareFiles(df9834_dsfOriginal, df9834_dsfRestored);
	CPPUNIT_ASSERT(wxRemoveFile(df9834_dsfRestored.GetFullPath()));
	CPPUNIT_ASSERT(wxRmdir(testdataRestored.GetFullPath()));
	
	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown());
	MessageBoxSetResponse(BM_RESTORE_FAILED_OBJECT_ALREADY_EXISTS, wxOK);
	ClickButtonWaitEvent(ID_Restore_Panel, ID_Function_Start_Button);
	MessageBoxCheckFired();
	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown());
	
	CPPUNIT_ASSERT(wxRmdir(restoreDest.GetFullPath()));
	
	// now select a whole directory to restore
	// leave the single file selected as well
	for (wxTreeItemId entry = pRestoreTree->GetFirstChild(loc, cookie);
		entry.IsOk(); entry = pRestoreTree->GetNextChild(loc, cookie))
	{
		if (pRestoreTree->GetItemText(entry).IsSameAs(_("sub23")))
		{
			ActivateTreeItemWaitEvent(pRestoreTree, entry);
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
				pRestoreTree->GetItemImage(entry));
		}
		else if (pRestoreTree->GetItemText(entry).IsSameAs(_("df9834.dsf")))
		{
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
				pRestoreTree->GetItemImage(entry));
		}
		else
		{
			CPPUNIT_ASSERT_EQUAL(images.GetEmptyImageId(),
				pRestoreTree->GetItemImage(entry));
		}
	}
	
	CHECK_RESTORE_OK(13, "124 kB");
	
	CPPUNIT_ASSERT(restoreDest.DirExists());
	wxFileName sub23 = MakeAbsolutePath(restoreDest, _("testdata/sub23"));
	CPPUNIT_ASSERT(sub23.DirExists());
	CPPUNIT_ASSERT(df9834_dsfRestored.FileExists());
	CompareFiles(df9834_dsfOriginal, df9834_dsfRestored);
	CompareExpectNoDifferences(rClientConfig, mTlsContext, _("testdata/sub23"),
		sub23);
	DeleteRecursive(sub23);
	CPPUNIT_ASSERT(wxRemoveFile(df9834_dsfRestored.GetFullPath()));
	CPPUNIT_ASSERT(wxRmdir(testdataRestored.GetFullPath()));
	CPPUNIT_ASSERT(wxRmdir(restoreDest.GetFullPath()));
	
	// check that the restore spec contains what we expect
	{
		RestoreSpec& rRestoreSpec(pRestorePanel->GetRestoreSpec());
		const RestoreSpecEntry::Vector entries = rRestoreSpec.GetEntries();
		CPPUNIT_ASSERT_EQUAL((size_t)2, entries.size());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/df9834.dsf")),
			entries[0].GetNode()->GetFullPath());
		CPPUNIT_ASSERT(entries[0].IsInclude());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23")),
			entries[1].GetNode()->GetFullPath());
		CPPUNIT_ASSERT(entries[1].IsInclude());
		
		rRestoreSpec.Remove(entries[0]);
	}
	
	wxTreeItemId sub23id = GetItemIdFromPath(pRestoreTree, loc, 
		_("sub23"));
	CPPUNIT_ASSERT(sub23id.IsOk());
	CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
		pRestoreTree->GetItemImage(sub23id));
	
	wxTreeItemId dhsfdss = GetItemIdFromPath(pRestoreTree, loc, 
		_("sub23/dhsfdss"));
	CPPUNIT_ASSERT(dhsfdss.IsOk());
	CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(),
		pRestoreTree->GetItemImage(dhsfdss));

	// create a weird configuration, with an item included under
	// another included item (i.e. double included)
	{
		
		ActivateTreeItemWaitEvent(pRestoreTree, sub23id);
		CPPUNIT_ASSERT_EQUAL(images.GetEmptyImageId(),
			pRestoreTree->GetItemImage(sub23id));
		CPPUNIT_ASSERT_EQUAL(images.GetEmptyImageId(),
			pRestoreTree->GetItemImage(dhsfdss));
		
		ActivateTreeItemWaitEvent(pRestoreTree, dhsfdss);
		CPPUNIT_ASSERT_EQUAL(images.GetPartialImageId(),
			pRestoreTree->GetItemImage(sub23id));
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
			pRestoreTree->GetItemImage(dhsfdss));

		ActivateTreeItemWaitEvent(pRestoreTree, sub23id);
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
			pRestoreTree->GetItemImage(sub23id));
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(),
			pRestoreTree->GetItemImage(dhsfdss));
	}

	{
		RestoreSpec& rRestoreSpec(pRestorePanel->GetRestoreSpec());
		const RestoreSpecEntry::Vector entries = rRestoreSpec.GetEntries();
		CPPUNIT_ASSERT_EQUAL((size_t)2, entries.size());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23")),
			entries[0].GetNode()->GetFullPath());
		CPPUNIT_ASSERT(entries[0].IsInclude());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23/dhsfdss")),
			entries[1].GetNode()->GetFullPath());
		CPPUNIT_ASSERT(entries[1].IsInclude());
	}
	
	// check that restore works as expected
	CHECK_RESTORE_OK(12, "106 kB");
	
	CPPUNIT_ASSERT(restoreDest.DirExists());
	CPPUNIT_ASSERT(sub23.DirExists());
	CPPUNIT_ASSERT(!df9834_dsfRestored.FileExists());
	CompareExpectNoDifferences(rClientConfig, mTlsContext, _("testdata/sub23"),
		sub23);
	DeleteRecursive(sub23);
	CPPUNIT_ASSERT(wxRmdir(testdataRestored.GetFullPath()));
	CPPUNIT_ASSERT(wxRmdir(restoreDest.GetFullPath()));

	// create a weirder configuration, with double include and the
	// included item also excluded. The include should be ignored,
	// and the exclude used.
	{
		ActivateTreeItemWaitEvent(pRestoreTree, dhsfdss);
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
			pRestoreTree->GetItemImage(sub23id));
		CPPUNIT_ASSERT_EQUAL(images.GetCrossedImageId(),
			pRestoreTree->GetItemImage(dhsfdss));
	}

	{
		RestoreSpec& rRestoreSpec(pRestorePanel->GetRestoreSpec());
		const RestoreSpecEntry::Vector entries = rRestoreSpec.GetEntries();
		CPPUNIT_ASSERT_EQUAL((size_t)3, entries.size());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23")),
			entries[0].GetNode()->GetFullPath());
		CPPUNIT_ASSERT(entries[0].IsInclude());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23/dhsfdss")),
			entries[1].GetNode()->GetFullPath());
		CPPUNIT_ASSERT(entries[1].IsInclude());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23/dhsfdss")),
			entries[2].GetNode()->GetFullPath());
		CPPUNIT_ASSERT(!(entries[2].IsInclude()));
	}
	
	// check that restore works as expected
	CHECK_RESTORE_OK(8, "76 kB");
	
	CPPUNIT_ASSERT(restoreDest.DirExists());
	CPPUNIT_ASSERT(sub23.DirExists());
	CPPUNIT_ASSERT(!df9834_dsfRestored.FileExists());
	CompareExpectDifferences(rClientConfig, mTlsContext, _("testdata/sub23"),
		sub23, 1, 0);
	DeleteRecursive(sub23);
	CPPUNIT_ASSERT(wxRmdir(testdataRestored.GetFullPath()));
	CPPUNIT_ASSERT(wxRmdir(restoreDest.GetFullPath()));
	
	DeleteRecursive(mTestDataDir);
	CPPUNIT_ASSERT(mStoreConfigFileName.FileExists());
	CPPUNIT_ASSERT(wxRemoveFile(mStoreConfigFileName.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mAccountsFile.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mRaidConfigFile.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mClientConfigFile.GetFullPath()));		
	CPPUNIT_ASSERT(mConfDir.Rmdir());
	DeleteRecursive(mStoreDir);
	CPPUNIT_ASSERT(mBaseDir.Rmdir());
}

void TestRestore::tearDown()
{
	mpMainFrame->GetConnection()->Disconnect();
	
	if (mapServer.get())
	{
		mapServer->Stop();
	}

	// clean up
	if (mpTestDataLocation)
	{
		CPPUNIT_ASSERT_EQUAL((size_t)1, mpConfig->GetLocations().size());
		mpConfig->RemoveLocation(*mpTestDataLocation);
	}
	
	/*	
	DeleteRecursive(mTestDataDir);
	CPPUNIT_ASSERT(mStoreConfigFileName.FileExists());
	CPPUNIT_ASSERT(wxRemoveFile(mStoreConfigFileName.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mAccountsFile.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mRaidConfigFile.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mClientConfigFile.GetFullPath()));		
	CPPUNIT_ASSERT(mConfDir.Rmdir());
	DeleteRecursive(mStoreDir);
	CPPUNIT_ASSERT(mBaseDir.Rmdir());
	*/
}
