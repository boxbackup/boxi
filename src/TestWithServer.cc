/***************************************************************************
 *            TestWithServer.cc
 *
 *  Sun Sep 24 13:49:55 2006
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
#include <wx/datectrl.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/spinctrl.h>
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
#include "TestWithServer.h"
#include "TestBackupConfig.h"
#include "FileTree.h"
#include "Restore.h"
#include "ServerConnection.h"
#include "RestorePanel.h"
#include "RestoreProgressPanel.h"
#include "RestoreFilesPanel.h"

#undef TLS_CLASS_IMPLEMENTATION_CPP

void TestWithServer::setUp()
{
	// create a working directory
	mBaseDir.AssignTempFileName(wxT("boxi-test-tempdir-"));
	CPPUNIT_ASSERT(mBaseDir.FileExists());
	CPPUNIT_ASSERT(wxRemoveFile(mBaseDir.GetFullPath()));
	CPPUNIT_ASSERT(wxMkdir     (mBaseDir.GetFullPath(), 0700));
	CPPUNIT_ASSERT(mBaseDir.DirExists());
	
	// create a directory to hold config files for the tests
	mConfDir = wxFileName(mBaseDir.GetFullPath(), _("conf"));
	CPPUNIT_ASSERT(wxMkdir(mConfDir.GetFullPath(), 0700));
	CPPUNIT_ASSERT(mConfDir.DirExists());
	
	// create a directory to hold bbstored's store temporarily
	mStoreDir = wxFileName(mBaseDir.GetFullPath(), _("store"));
	CPPUNIT_ASSERT(!mStoreDir.DirExists());
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
	
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\tListenAddresses = inet:127.0.0.1\n")));
		
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
	
	isOk = mapConn->InitTlsContext(mTlsContext, msg);
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
	
	mpBackupPanel = wxDynamicCast
	(
		mpMainFrame->FindWindow(ID_Backup_Panel), wxPanel
	);
	CPPUNIT_ASSERT(mpBackupPanel);
	
	mpBackupProgressPanel = wxDynamicCast
	(
		mpMainFrame->FindWindow(ID_Backup_Progress_Panel), wxPanel
	);
	CPPUNIT_ASSERT(mpBackupProgressPanel);
	CPPUNIT_ASSERT(!mpBackupProgressPanel->IsShown());

	mpBackupErrorList = wxDynamicCast
	(
		mpBackupProgressPanel->FindWindow(ID_BackupProgress_ErrorList), 
		wxListBox
	);
	CPPUNIT_ASSERT(mpBackupErrorList);
	CPPUNIT_ASSERT_EQUAL(0, mpBackupErrorList->GetCount());
	
	mpBackupStartButton = wxDynamicCast
	(
		mpBackupPanel->FindWindow(ID_Function_Start_Button), wxButton
	);
	CPPUNIT_ASSERT(mpBackupStartButton);

	mpBackupProgressCloseButton = wxDynamicCast
	(
		mpBackupProgressPanel->FindWindow(wxID_CANCEL), wxButton
	);
	CPPUNIT_ASSERT(mpBackupProgressCloseButton);

	// makes tests faster
	mpConfig->MinimumFileAge.Set(0);
}

void TestWithServer::CompareFiles(const wxFileName& first, 
	const wxFileName& second)
{
	wxFile a(first.GetFullPath());
	wxFile b(second.GetFullPath());
	CPPUNIT_ASSERT(a.IsOpened());
	CPPUNIT_ASSERT(b.IsOpened());
	CPPUNIT_ASSERT_EQUAL(a.Length(), b.Length());
	char buffer1[4096], buffer2[sizeof(buffer1)];
	for (int pos = 0; pos < a.Length(); pos += sizeof(buffer1))
	{
		size_t toread = sizeof(buffer1);
		if (toread > a.Length() - pos)
		{
			toread = a.Length() - pos;
		}
		CPPUNIT_ASSERT_EQUAL((ssize_t)toread, a.Read(buffer1, toread));
		CPPUNIT_ASSERT_EQUAL((ssize_t)toread, b.Read(buffer2, toread));
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

void TestWithServer::CompareDirs(wxFileName file1, wxFileName file2, 
	size_t diffsExpected)
{
	wxString msg;
	msg.Printf(_("%s must exist"), file1.GetFullPath().c_str());
	wxCharBuffer buf = msg.mb_str();
	CPPUNIT_ASSERT_MESSAGE(buf.data(), file1.FileExists() || file1.DirExists());

	msg.Printf(_("%s must exist"), file2.GetFullPath().c_str());
	buf = msg.mb_str();
	CPPUNIT_ASSERT_MESSAGE(buf.data(), file2.FileExists() || file2.DirExists());
	
	wxArrayString diffs;
	CompareDirsInternal(file1, file2, diffs);
	
	if (diffs.Count() == diffsExpected)
	{
		return;
	}
	
	msg = _("Differences are: ");
	
	for (size_t i = 0; i < diffs.Count(); i++)
	{
		if (i > 0)
		{
			msg.Append(_(", "));
		}
		
		msg.Append(diffs.Item(i));
	}
	
	buf = msg.mb_str();
	CPPUNIT_ASSERT_EQUAL_MESSAGE(buf.data(), (size_t)diffsExpected, 
		diffs.Count());
}

void TestWithServer::CompareDirsInternal(wxFileName file1, wxFileName file2, 
	wxArrayString& rDiffs)
{
	CPPUNIT_ASSERT(file1.FileExists() || file1.DirExists());
	CPPUNIT_ASSERT(file2.FileExists() || file2.DirExists());
	
	if (file1.FileExists() && ! file2.FileExists())
	{
		wxString msg;
		msg.Printf(_("%s is not a file"), 
			file2.GetFullPath().c_str());
		rDiffs.Add(msg);
		return;
	}

	if (file1.DirExists() && ! file2.DirExists())
	{
		wxString msg;
		msg.Printf(_("%s is not a directory"), 
			file2.GetFullPath().c_str());
		rDiffs.Add(msg);
		return;
	}
	
	if (file1.FileExists())
	{
		wxFile f1(file1.GetFullPath());
		wxFile f2(file2.GetFullPath());
		
		if (f1.Length() != f2.Length())
		{
			wxString msg;
			msg.Printf(_("%s has wrong length (expected %d, "
				"found %d)"), file2.GetFullPath().c_str(),
				f1.Length(), f2.Length());
			rDiffs.Add(msg);
			return;
		}
		
		char b1[4096], b2[sizeof(b1)];
		
		for (size_t offset = 0; offset < f1.Length(); 
			offset += sizeof(b1))
		{
			ssize_t toread = sizeof(b1);
			if (toread > f1.Length() - offset)
			{
				toread = f1.Length() - offset;
			}
			
			CPPUNIT_ASSERT(f1.Read(b1, toread) == toread);
			CPPUNIT_ASSERT(f2.Read(b2, toread) == toread);
			
			for (ssize_t i = 0; i < toread; i++)
			{
				if (b1[i] != b2[i])
				{
					wxString msg;
					msg.Printf(_("%s has wrong data at byte %d "
						"(expected %d, found %d)"), 
						file2.GetFullPath().c_str(),
						offset + i, (int)b1[i], (int)b2[i]);					
					rDiffs.Add(msg);
					return;
				}
			}
		}
	}

	if (file1.DirExists())
	{
		wxArrayString expectedEntries;
		wxDir expected(file1.GetFullPath());
		wxString file;
		
		for (bool doContinue = expected.GetFirst(&file);
			doContinue; doContinue = expected.GetNext(&file))
		{
			expectedEntries.Add(file);
		}
		
		wxArrayString actualEntries;
		wxDir actual(file2.GetFullPath());
		
		for (bool doContinue = actual.GetFirst(&file);
			doContinue; doContinue = actual.GetNext(&file))
		{
			actualEntries.Add(file);
		}
		
		for (size_t i = 0; i < actualEntries.Count(); i++)
		{
			wxString file = actualEntries.Item(i);
			
			wxFileName f1full(file1.GetFullPath(), file);
			wxFileName f2full(file2.GetFullPath(), file);
			
			if (expectedEntries.Index(file) == wxNOT_FOUND)
			{
				wxString msg;
				msg.Printf(_("%s was not expected"), 
					f2full.GetFullPath().c_str());
				rDiffs.Add(msg);
			}
			else
			{
				CompareDirsInternal(f1full, f2full, rDiffs);		
				expectedEntries.Remove(file);
			}
		}
		
		for (size_t i = 0; i < expectedEntries.Count(); i++)
		{
			wxFileName f2full(file2.GetFullPath(), 
				expectedEntries.Item(i));
			wxString msg;
			msg.Printf(_("%s was not found"), 
				f2full.GetFullPath().c_str());
			rDiffs.Add(msg);
		}
	}
}

void TestWithServer::tearDown()
{
	if (mpMainFrame)
	{
		mpMainFrame->GetConnection()->Disconnect();
	}
	
	if (mapServer.get())
	{
		mapServer->Stop();
	}

	// clean up
	if (mpTestDataLocation)
	{
		CPPUNIT_ASSERT_EQUAL((size_t)1, mpConfig->GetLocations().size());
		mpConfig->RemoveLocation(*mpTestDataLocation);
		CPPUNIT_ASSERT_EQUAL((size_t)0, mpConfig->GetLocations().size());
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
