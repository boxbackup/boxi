/***************************************************************************
 *            TestBackup.cc
 *
 *  Wed May 10 23:10:16 2006
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

#include <wx/button.h>

#include <cppunit/TestSuite.h>
#include <cppunit/extensions/HelperMacros.h>

#include "main.h"
#include "BoxiApp.h"
#include "ClientConfig.h"
#include "MainFrame.h"
#include "SetupWizard.h"
#include "SslConfig.h"
#include "TestFrame.h"

class TestBackup : public TestWithConfig
{
	public:
	TestBackup() { }
	virtual void RunTest();
	static CppUnit::Test *suite();
};

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
	CPPUNIT_ASSERT(wxGetApp().HaveConfigFile());
	wxString mConfigFileName = wxGetApp().GetConfigFile();
	
	MainFrame* pMainFrame = GetMainFrame();
	CPPUNIT_ASSERT(pMainFrame);
	
	mpConfig = pMainFrame->GetConfig();
	CPPUNIT_ASSERT(mpConfig);
	
	pMainFrame->DoFileOpen(mConfigFileName);
	wxString msg;
	bool isOk = mpConfig->Check(msg);
	if (!isOk)
	{
		wxCharBuffer buf = msg.mb_str(wxConvLibc);
		CPPUNIT_ASSERT_MESSAGE(buf.data(), mpConfig->Check(msg));
	}
	
	wxPanel* pBackupPanel = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_Backup_Panel), wxPanel
	);
	CPPUNIT_ASSERT(pBackupPanel);

	CPPUNIT_ASSERT(!pBackupPanel->IsShown());	
	ClickButtonWaitEvent(ID_Main_Frame, ID_General_Backup_Button);
	CPPUNIT_ASSERT(pBackupPanel->IsShown());
}
