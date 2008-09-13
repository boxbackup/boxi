/***************************************************************************
 *            TestWithServer.h
 *
 *  Sun Sep 24 13:49:55 2006
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
 
#ifndef _TESTWITHSERVER_H
#define _TESTWITHSERVER_H

#include <wx/filename.h>

#include "BackupStoreAccounts.h"
#include "BackupStoreAccountDatabase.h"
#include "RaidFileController.h"

#include "TestFrame.h"
#include "ServerConnection.h"

class TestBackupStoreDaemon
: public ServerTLS<BOX_PORT_BBSTORED, 1, false>,
  public HousekeepingInterface
{
	public:
	TestBackupStoreDaemon()
	: mStateKnownCondition(mConditionLock),
	  mStateListening(false), mStateDead(false)
	{ }
	
	virtual void Run();
	virtual void Setup();
	virtual bool Load(const std::string& file);
	
	wxMutex     mConditionLock;
	wxCondition mStateKnownCondition;
	bool        mStateListening;
	bool        mStateDead;

	virtual void SendMessageToHousekeepingProcess(const void *Msg, int MsgLen)
	{ }

	protected:
	virtual void NotifyListenerIsReady();
	virtual void Connection(SocketStreamTLS &rStream);
	virtual const ConfigurationVerify *GetConfigVerify() const;

	private:
	void SetupInInitialProcess();
	std::auto_ptr<BackupStoreAccountDatabase> mapAccountDatabase;
	std::auto_ptr<BackupStoreAccounts> mapAccounts;
};

class StoreServerThread : public wxThread
{
	TestBackupStoreDaemon mDaemon;

	public:
	StoreServerThread(const wxString& rFilename)
	: wxThread(wxTHREAD_JOINABLE)
	{
		wxCharBuffer buf = rFilename.mb_str(wxConvLibc);
		mDaemon.Load(buf.data());
		mDaemon.SetSingleProcess(true);
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

class TestWithServer : public TestWithConfig
{
	public:
	TestWithServer();
	virtual ~TestWithServer();

	virtual void setUp();
	virtual void tearDown();

	protected:
	wxFileName mBaseDir, mConfDir, mStoreDir, mTestDataDir,
		mStoreConfigFileName, mAccountsFile, mRaidConfigFile,
		mClientConfigFile;
	BoxiLocation* mpTestDataLocation;
	MainFrame* mpMainFrame;
	wxPanel*   mpBackupPanel;
	wxPanel*   mpBackupProgressPanel;
	wxListBox* mpBackupErrorList;
	wxButton*  mpBackupStartButton;
	wxButton*  mpBackupProgressCloseButton;

	std::auto_ptr<ServerConnection> mapConn;
	TLSContext mTlsContext;
	
	void CompareFiles(const wxFileName& first, const wxFileName& second);
	void CompareDirs(wxFileName dir1, wxFileName dir2, 
		size_t diffsExpected);
	void CompareDirsInternal(wxFileName dir1, wxFileName dir2, 
		wxArrayString& rDiffs);
		
	void SetupDefaultLocation();
	void RemoveDefaultLocation();
	
	void StartServer();
	void StopServer();
	
	std::auto_ptr<StoreServer> mapServer;
};

#define CHECK_BACKUP() \
	ClickButtonWaitEvent(mpBackupStartButton); \
	CPPUNIT_ASSERT(mpBackupProgressPanel->IsShown()); \
	CPPUNIT_ASSERT(mpMainFrame->IsTopPanel(mpBackupProgressPanel)); \
	ClickButtonWaitEvent(mpBackupProgressCloseButton); \
	CPPUNIT_ASSERT(!mpBackupProgressPanel->IsShown());

#define CHECK_BACKUP_ERROR(message) \
	MessageBoxSetResponse(message, wxOK); \
	CHECK_BACKUP(); \
	MessageBoxCheckFired(); \
	CPPUNIT_ASSERT(mpBackupErrorList->GetCount() > 0);

#define CHECK_BACKUP_OK(message) \
	CHECK_BACKUP(); \
	if (mpBackupErrorList->GetCount() > 0) \
	{ \
		wxCharBuffer buf = mpBackupErrorList->GetString(0).mb_str(); \
		CPPUNIT_ASSERT_MESSAGE(buf.data(), \
			mpBackupErrorList->GetCount() > 0); \
	}

#define CHECK_COMPARE_OK() \
	CompareExpectNoDifferences(mpConfig->GetBoxConfig(), mTlsContext, \
		_("testdata"), mTestDataDir);

#define CHECK_COMPARE_FAILS(diffs, modified) \
	CompareExpectDifferences(mpConfig->GetBoxConfig(), mTlsContext, \
		_("testdata"), mTestDataDir, diffs, modified);

#define CHECK_COMPARE_LOC_OK(exdirs, exfiles) \
	CompareLocationExpectNoDifferences(mpConfig->GetBoxConfig(), \
		mTlsContext, "testdata", exdirs, exfiles);

#define CHECK_COMPARE_LOC_FAILS(diffs, modified, unchecked, exdirs, exfiles) \
	CompareLocationExpectDifferences(mpConfig->GetBoxConfig(), \
		mTlsContext, "testdata", diffs, modified, unchecked, exdirs, \
		exfiles);

#endif /* _TESTWITHSERVER_H */
