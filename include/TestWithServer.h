/***************************************************************************
 *            TestWithServer.h
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
 
#ifndef _TESTWITHSERVER_H
#define _TESTWITHSERVER_H

#include "TestFrame.h"
#include "ServerConnection.h"

class StoreServer;

class TestWithServer : public TestWithConfig
{
	public:
	TestWithServer()
	: TestWithConfig        (),
	  mpTestDataLocation    (NULL),
	  mpMainFrame           (NULL),
	  mpBackupPanel         (NULL),
	  mpBackupProgressPanel (NULL),
	  mpBackupErrorList     (NULL),
	  mpBackupStartButton   (NULL),
	  mpBackupProgressCloseButton (NULL)
	{ }
	virtual ~TestWithServer()
	{ }

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

	std::auto_ptr<StoreServer> mapServer;
	std::auto_ptr<ServerConnection> mapConn;
	TLSContext mTlsContext;
	
	void CompareFiles(const wxFileName& first, const wxFileName& second);
	void CompareDirs(wxFileName dir1, wxFileName dir2, 
		size_t diffsExpected);
	void CompareDirsInternal(wxFileName dir1, wxFileName dir2, 
		wxArrayString& rDiffs);
		
	void SetupDefaultLocation();
	void RemoveDefaultLocation();
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
