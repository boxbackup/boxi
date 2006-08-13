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

#include <openssl/ssl.h>

#include <wx/button.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/treectrl.h>
// #include <wx/wfstream.h>
// #include <wx/zipstrm.h>

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

#include "main.h"
#include "BoxiApp.h"
#include "ClientConfig.h"
#include "MainFrame.h"
#include "SetupWizard.h"
#include "SslConfig.h"
#include "TestBackup.h"
#include "FileTree.h"

#include "ServerConnection.h"

#undef TLS_CLASS_IMPLEMENTATION_CPP

class TestBackupStoreDaemon
: public ServerTLS<BOX_PORT_BBSTORED, 1, false>,
  public HousekeepingInterface
{
	public:
	TestBackupStoreDaemon::TestBackupStoreDaemon()
	: mIsReadyCondition(mConditionLock)
	{ }
	
	virtual void Run()   { ServerTLS<BOX_PORT_BBSTORED, 1, false>::Run(); }
	virtual void Setup() { SetupInInitialProcess(); }
	virtual bool Load(const std::string& file) 
	{ 
		return LoadConfigurationFile(file); 
	}
	
	wxMutex     mConditionLock;
	wxCondition mIsReadyCondition;

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
	mIsReadyCondition.Signal();
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
	StoreServerThread::StoreServerThread(const wxString& rFilename)
	: wxThread(wxTHREAD_JOINABLE)
	{
		mDaemon.Load(rFilename.mb_str(wxConvLibc).data());
		mDaemon.Setup();
	}

	StoreServerThread::~StoreServerThread()
	{
		mDaemon.SetTerminateWanted();
		Wait();
	}

	wxThreadError Run();
	
	private:
	virtual ExitCode Entry();
};

wxThreadError StoreServerThread::Run()
{
	mDaemon.mConditionLock.Lock();
	
	wxThreadError result = wxThread::Run();
	
	if (result != wxTHREAD_NO_ERROR)
	{
		return result;
	}
	
	// wait until listening thread is ready to accept connections
	mDaemon.mIsReadyCondition.Wait();
	
	return result;
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
	return (void *)1;
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
	
	// unzip the certificates
	/*
	wxFileName certZipFile(_("../test/config/testcerts.zip"));
	CPPUNIT_ASSERT(certZipFile.FileExists());
	wxFileInputStream zipFis(certZipFile.GetFullPath());
	CPPUNIT_ASSERT(zipFis.Ok());
	wxZipInputStream zipInput(zipFis);
	
	for (std::auto_ptr<wxZipEntry> apEntry(zipInput.GetNextEntry());
		apEntry.get() != NULL; apEntry.reset(zipInput.GetNextEntry()))
	{
		wxFileName outName(confDir.GetFullPath(), 
			apEntry->GetInternalName());
		
		CPPUNIT_ASSERT(!outName.FileExists());
		CPPUNIT_ASSERT(!outName.DirExists());
		
		if (apEntry->IsDir())
		{
			wxMkdir(outName.GetFullPath(), 0700);
			CPPUNIT_ASSERT(outName.DirExists());
		}
		else
		{
			wxFileOutputStream outFos(outName.GetFullPath());
			CPPUNIT_ASSERT(outFos.Ok());
			outFos.Write(zipFis);
			CPPUNIT_ASSERT_EQUAL((int)apEntry->GetSize(), 
				(int)outFos.LastWrite());
		}
	}
	*/

	// create a directory to hold bbstored's store temporarily
	wxFileName storeDir(tempDir.GetFullPath(), _("store"));
	CPPUNIT_ASSERT(wxMkdir(storeDir.GetFullPath(), 0700));
	CPPUNIT_ASSERT(storeDir.DirExists());
	/*
	{
		wxFileName backupDir(storeDir.GetFullPath(), _("backup"));
		CPPUNIT_ASSERT(wxMkdir(backupDir.GetFullPath(), 0700));
		wxFileName accountDir(backupDir.GetFullPath(), _("00000002"));
		CPPUNIT_ASSERT(wxMkdir(accountDir.GetFullPath(), 0700));
	}
	*/

	// create raidfile.conf for the store daemon
	wxFileName raidConf(confDir.GetFullPath(), _("raidfile.conf"));
	CPPUNIT_ASSERT(!raidConf.FileExists());
	
	{
		wxFile raidConfFile;
		CPPUNIT_ASSERT(raidConfFile.Create(raidConf.GetFullPath()));
		
		CPPUNIT_ASSERT(raidConfFile.Write(_("disc0\n")));
		CPPUNIT_ASSERT(raidConfFile.Write(_("{\n")));
		CPPUNIT_ASSERT(raidConfFile.Write(_("\tSetNumber = 0\n")));
		CPPUNIT_ASSERT(raidConfFile.Write(_("\tBlockSize = 4096\n")));

		CPPUNIT_ASSERT(raidConfFile.Write(_("\tDir0 = ")));
		CPPUNIT_ASSERT(raidConfFile.Write(storeDir.GetFullPath()));
		CPPUNIT_ASSERT(raidConfFile.Write(_("\n")));

		CPPUNIT_ASSERT(raidConfFile.Write(_("\tDir1 = ")));
		CPPUNIT_ASSERT(raidConfFile.Write(storeDir.GetFullPath()));
		CPPUNIT_ASSERT(raidConfFile.Write(_("\n")));
		
		CPPUNIT_ASSERT(raidConfFile.Write(_("\tDir2 = ")));
		CPPUNIT_ASSERT(raidConfFile.Write(storeDir.GetFullPath()));
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
		
		wxFileName serverCert(_("../test/config/bbstored/server-cert.pem"));
		CPPUNIT_ASSERT(serverCert.FileExists());
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\tCertificateFile = ")));
		CPPUNIT_ASSERT(storeConfigFile.Write(serverCert.GetFullPath()));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\n")));
	
		wxFileName serverKey(_("../test/config/bbstored/server-key.pem"));
		CPPUNIT_ASSERT(serverKey.FileExists());
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\tPrivateKeyFile = ")));
		CPPUNIT_ASSERT(storeConfigFile.Write(serverKey.GetFullPath()));
		CPPUNIT_ASSERT(storeConfigFile.Write(_("\n")));
	
		wxFileName clientCA(_("../test/config/bbstored/client-ca.pem"));
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
			printf("Failed to execute command '%s': error %d\n",
				command.mb_str(wxConvLibc).data(),
				(int)result);
			CPPUNIT_ASSERT_EQUAL(0, (int)result);
		}
	}
	
	MainFrame* pMainFrame = GetMainFrame();
	CPPUNIT_ASSERT(pMainFrame);
	
	mpConfig = pMainFrame->GetConfig();
	CPPUNIT_ASSERT(mpConfig);
	
	StoreServerThread server(storeConfigFileName.GetFullPath());
	CPPUNIT_ASSERT_EQUAL(wxTHREAD_NO_ERROR, server.Create());
	CPPUNIT_ASSERT_EQUAL(wxTHREAD_NO_ERROR, server.Run());

	pMainFrame->DoFileOpen(_("../test/config/bbackupd.conf"));
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

	wxPanel* pBackupFilesPanel = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_Backup_Files_Panel), wxPanel
	);
	CPPUNIT_ASSERT(pBackupFilesPanel);
	
	CPPUNIT_ASSERT(!pBackupFilesPanel->IsShown());	
	ClickButtonWaitEvent(ID_Main_Frame, ID_Function_Source_Button);
	CPPUNIT_ASSERT(pBackupFilesPanel->IsShown());

	wxTreeCtrl* pTree = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_Backup_Locations_Tree), wxTreeCtrl
	);
	CPPUNIT_ASSERT(pTree);

	wxTreeItemId rootId = pTree->GetRootItem();
	wxString rootLabel  = pTree->GetItemText(rootId);
	CPPUNIT_ASSERT_EQUAL((std::string)"/ (local root)", 
		(std::string)rootLabel.mb_str(wxConvLibc).data());
	
	FileImageList images;
	CPPUNIT_ASSERT_EQUAL(images.GetEmptyImageId(), 
		pTree->GetItemImage(rootId));
		
	wxListBox* pLocationsListBox = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_Backup_LocationsList), wxListBox
	);
	CPPUNIT_ASSERT(pLocationsListBox);
	CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox->GetCount());
	
	wxChoice* pExcludeLocsListBox = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_BackupLoc_ExcludeLocList), wxChoice
	);
	CPPUNIT_ASSERT(pExcludeLocsListBox);
	CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetCount());
	
	ActivateTreeItemWaitEvent(pTree, rootId);
	CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT(pLocationsListBox  ->GetString(0).IsSameAs(_("/ -> root")));
	CPPUNIT_ASSERT(pExcludeLocsListBox->GetString(0).IsSameAs(_("/ -> root")));

	MessageBoxSetResponse(BM_BACKUP_FILES_DELETE_LOCATION_QUESTION, wxNO);
	ActivateTreeItemWaitEvent(pTree, rootId);
	MessageBoxCheckFired();
	CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetCount());

	MessageBoxSetResponse(BM_BACKUP_FILES_DELETE_LOCATION_QUESTION, wxYES);
	ActivateTreeItemWaitEvent(pTree, rootId);
	MessageBoxCheckFired();
	CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetCount());
	
	{
		wxFileName testDataDir(tempDir.GetFullPath(), _("testdata"));
		CPPUNIT_ASSERT(testDataDir.Mkdir(0700));
		wxFileName testDepth1Dir(testDataDir.GetFullPath(), _("depth1"));
		CPPUNIT_ASSERT(testDepth1Dir.Mkdir(0700));
		wxFileName testDepth2Dir(testDepth1Dir.GetFullPath(), _("depth2"));
		CPPUNIT_ASSERT(testDepth2Dir.Mkdir(0700));
		wxFileName testDepth3Dir(testDepth2Dir.GetFullPath(), _("depth3"));
		CPPUNIT_ASSERT(testDepth3Dir.Mkdir(0700));
		wxFileName testDepth4Dir(testDepth3Dir.GetFullPath(), _("depth4"));
		CPPUNIT_ASSERT(testDepth4Dir.Mkdir(0700));
		
		wxTreeItemId testDataDirItem = rootId;
		
		wxArrayString testDataDirPathDirs = testDataDir.GetDirs();
		for (size_t i = 0; i < testDataDirPathDirs.Count(); i++)
		{
			wxString dirName = testDataDirPathDirs.Item(i);
			
			if (!pTree->IsExpanded(testDataDirItem))
			{
				pTree->Expand(testDataDirItem);
			}
			
			bool found = false;
			wxTreeItemIdValue cookie;
			wxTreeItemId child;
			
			for (child = pTree->GetFirstChild(testDataDirItem, cookie);
				child.IsOk(); child = pTree->GetNextChild(testDataDirItem, cookie))
			{
				if (pTree->GetItemText(child).IsSameAs(dirName))
				{
					testDataDirItem = child;
					found = true;
					break;
				}
			}
			
			CPPUNIT_ASSERT_MESSAGE(dirName.mb_str(wxConvLibc).data(), found);
		}
		
		wxTreeItemIdValue cookie;
		
		wxTreeItemId depth1 = pTree->GetFirstChild(testDataDirItem, cookie);
		CPPUNIT_ASSERT(depth1.IsOk());
		
		wxTreeItemId depth2 = pTree->GetFirstChild(depth1, cookie);
		CPPUNIT_ASSERT(depth2.IsOk());
		
		wxTreeItemId depth3 = pTree->GetFirstChild(depth2, cookie);
		CPPUNIT_ASSERT(depth3.IsOk());
		
		wxTreeItemId depth4 = pTree->GetFirstChild(depth3, cookie);		
		CPPUNIT_ASSERT(depth4.IsOk());
		
		ActivateTreeItemWaitEvent(pTree, testDataDirItem);
		CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox  ->GetCount());
		CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetCount());
		wxString expectedTitle = testDataDir.GetFullPath();
		expectedTitle.Append(_(" -> testdata"));
		CPPUNIT_ASSERT(pLocationsListBox  ->GetString(0).IsSameAs(expectedTitle));
		CPPUNIT_ASSERT(pExcludeLocsListBox->GetString(0).IsSameAs(expectedTitle));

		CPPUNIT_ASSERT(testDepth4Dir.Rmdir());
		CPPUNIT_ASSERT(testDepth3Dir.Rmdir());
		CPPUNIT_ASSERT(testDepth2Dir.Rmdir());
		CPPUNIT_ASSERT(testDepth1Dir.Rmdir());
		CPPUNIT_ASSERT(testDataDir.Rmdir());
	}
	
	// clean up
	CPPUNIT_ASSERT(wxRemoveFile(storeConfigFileName.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(accountsDb.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(raidConf.GetFullPath()));
	CPPUNIT_ASSERT(confDir.Rmdir());
	wxFileName backupDir (storeDir.GetFullPath(),  _("backup"));
	wxFileName accountDir(backupDir.GetFullPath(), _("00000002"));
	CPPUNIT_ASSERT(wxRemoveFile(wxFileName(accountDir.GetFullPath(), 
		_("info.rfw")).GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(wxFileName(accountDir.GetFullPath(), 
		_("o01.rfw")).GetFullPath()));
	CPPUNIT_ASSERT(accountDir.Rmdir());
	CPPUNIT_ASSERT(backupDir.Rmdir());
	CPPUNIT_ASSERT(storeDir.Rmdir());
	CPPUNIT_ASSERT(tempDir.Rmdir());
}
