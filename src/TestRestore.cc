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
#include "RestoreProgressPanel.h"

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
	
	wxPanel* pRestorePanel = wxDynamicCast
	(
		mpMainFrame->FindWindow(ID_Restore_Panel), wxPanel
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
	MessageBoxCheckFired(); \
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

	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown());
	ClickButtonWaitEvent(ID_Restore_Panel, ID_Function_Start_Button);
	CPPUNIT_ASSERT(pRestoreProgressPanel->IsShown());
	if (pRestoreErrorList->GetCount() != 1)
	{
		wxString msg = pRestoreErrorList->GetString(
			pRestoreErrorList->GetCount() - 1);
		wxCharBuffer buf = msg.mb_str(wxConvLibc);
		CPPUNIT_ASSERT_MESSAGE(buf.data(), 
			1 == pRestoreErrorList->GetCount());
	}
	CPPUNIT_ASSERT_EQUAL(1, pRestoreErrorList->GetCount());
	CPPUNIT_ASSERT_EQUAL(wxString(_("Restore Finished")),
		pRestoreErrorList->GetString(0));
	ClickButtonWaitEvent(ID_Restore_Progress_Panel, wxID_CANCEL);
	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown());

	CPPUNIT_ASSERT_EQUAL(wxString(_("1")), 
		pRestoreProgressPanel->GetNumFilesTotalString());
	CPPUNIT_ASSERT_EQUAL(wxString(_("18 kB")), 
		pRestoreProgressPanel->GetNumBytesTotalString());
	CPPUNIT_ASSERT_EQUAL(wxString(_("1")), 
		pRestoreProgressPanel->GetNumFilesDoneString());
	CPPUNIT_ASSERT_EQUAL(wxString(_("18 kB")), 
		pRestoreProgressPanel->GetNumBytesDoneString());
	CPPUNIT_ASSERT_EQUAL(wxString(_("0")), 
		pRestoreProgressPanel->GetNumFilesRemainingString());
	CPPUNIT_ASSERT_EQUAL(wxString(_("0 B")), 
		pRestoreProgressPanel->GetNumBytesRemainingString());
	CPPUNIT_ASSERT_EQUAL(1, pRestoreProgressPanel->GetProgressPos());
	CPPUNIT_ASSERT_EQUAL(1, pRestoreProgressPanel->GetProgressMax());
	
	CPPUNIT_ASSERT(restoreDest.FileExists());
	CPPUNIT_ASSERT(wxRemoveFile(restoreDest.GetFullPath()));
	
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
