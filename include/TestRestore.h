/***************************************************************************
 *            TestRestore.h
 *
 *  Sat Aug 12 14:01:01 2006
 *  Copyright  2006  Chris Wilson
 *  chris-boxisource@qwirx.com
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
 
#ifndef _TESTRESTORE_H
#define _TESTRESTORE_H

#include "TestFrame.h"
#include "ServerConnection.h"

class TestRestore : public TestWithConfig
{
	public:
	TestRestore() { }
	virtual void RunTest();
	static CppUnit::Test *suite();
	virtual void setUp();
	virtual void tearDown();
	
	private:
	wxFileName mBaseDir, mConfDir, mStoreDir, mTestDataDir,
		mStoreConfigFileName, mAccountsFile, mRaidConfigFile,
		mClientConfigFile;
	Location* mpTestDataLocation;
	MainFrame* mpMainFrame;
	std::auto_ptr<StoreServer> mapServer;
	std::auto_ptr<ServerConnection> mapConn;
	std::auto_ptr<Configuration> mapClientConfig;
	TLSContext mTlsContext;
};

#define CHECK_BACKUP() \
	ClickButtonWaitEvent(ID_Backup_Panel, ID_Function_Start_Button);

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
	CompareExpectNoDifferences(rClientConfig, mTlsContext, _("testdata"), \
		mTestDataDir);

#define CHECK_COMPARE_FAILS(diffs, modified) \
	CompareExpectDifferences(rClientConfig, mTlsContext, _("testdata"), \
		mTestDataDir, diffs, modified);

#define CHECK_COMPARE_LOC_OK(exdirs, exfiles) \
	CompareLocationExpectNoDifferences(rClientConfig, mTlsContext, \
		"testdata", exdirs, exfiles);

#define CHECK_COMPARE_LOC_FAILS(diffs, modified, exdirs, exfiles) \
	CompareLocationExpectDifferences(rClientConfig, mTlsContext, "testdata", \
		diffs, modified, exdirs, exfiles);

#endif /* _TESTRESTORE_H */
