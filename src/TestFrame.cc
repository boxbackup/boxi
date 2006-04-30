/***************************************************************************
 *            TestFrame.cc
 *
 *  Sun Jan 22 22:37:04 2006
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
 *
 *  Includes (small) portions of OpenSSL by Eric Young (eay@cryptsoft.com) and
 *  Tim Hudson (tjh@cryptsoft.com), under their source redistribution license.
 */

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/thread.h>

#include <cppunit/SourceLine.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestSuite.h>
#include <cppunit/extensions/HelperMacros.h>

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "MainFrame.h"
#include "SetupWizard.h"
#include "SslConfig.h"
#include "TestFrame.h"
#include "WxGuiTestHelper.h"

DECLARE_EVENT_TYPE(CREATE_WINDOW_COMMAND, -1)
DECLARE_EVENT_TYPE(TEST_FINISHED_EVENT,   -1)

DEFINE_EVENT_TYPE (CREATE_WINDOW_COMMAND)
DEFINE_EVENT_TYPE (TEST_FINISHED_EVENT)

bool TestFrame::mTestsInProgress = FALSE;

void MessageBoxCheckFired();

class SafeWindowWrapper
{
	private:
	wxWindow* mpWindow;
	
	public:
	SafeWindowWrapper(wxWindow* pWindow) : mpWindow(pWindow) { }
	wxWindowID GetId() { return mpWindow->GetId(); }
	void AddPendingEvent(wxEvent& rEvent) { mpWindow->AddPendingEvent(rEvent); }
};

class TestCase : private wxThread
{
	private:
	TestFrame* mpTestFrame;
	
	void* Entry();
	virtual void RunTest() = 0;

	wxMutex        mMutex;
	wxCondition    mEventHandlerFinished;

	wxCommandEvent GetButtonClickEvent(wxWindow* pButton);
	
	protected:
	MainFrame*  OpenMainFrame();
	wxTextCtrl* GetTextCtrl  (wxWindow* pWindow, wxWindowID id);
	TestFrame*  GetTestFrame () { return mpTestFrame; }
	bool WaitForMain();
	void CloseWindowWaitClosed(wxWindow* pWindow);
	void ClickButtonWaitIdle  (wxWindow* pButton);
	void ClickButtonWaitEvent (wxWindow* pButton);
	void ClickRadioWaitEvent   (wxWindow* pButton);
	wxWindow* FindWindow(wxWindowID id) { return wxWindow::FindWindowById(id); }
	
	public:
	TestCase(TestFrame* pTestFrame) 
	: wxThread(wxTHREAD_DETACHED), 
	  mpTestFrame(pTestFrame),
	  mEventHandlerFinished(mMutex)	
	{ }
	virtual ~TestCase() { }

	void Start()
	{
		assert(Create() == wxTHREAD_NO_ERROR);
		assert(Run()    == wxTHREAD_NO_ERROR);
	}
};

void* TestCase::Entry()
{
	RunTest();
	
	wxCommandEvent fini(TEST_FINISHED_EVENT, mpTestFrame->GetId());
	mpTestFrame->AddPendingEvent(fini);
	
	return 0;
}

bool TestCase::WaitForMain()
{
	if (TestDestroy()) return FALSE;
	mpTestFrame->WaitForIdle();
	return TRUE;
}

MainFrame* TestCase::OpenMainFrame()
{
	wxCommandEvent open(CREATE_WINDOW_COMMAND, mpTestFrame->GetId());
	mpTestFrame->AddPendingEvent(open);	
	assert(WaitForMain());
	
	MainFrame* pMainFrame = mpTestFrame->GetMainFrame();
	assert(pMainFrame);
	
	return pMainFrame;
}

void TestCase::CloseWindowWaitClosed(wxWindow* pWindow)
{
	assert(pWindow);
	
	wxTopLevelWindow* pTopLevelWindow = 
		wxDynamicCast(pWindow, wxTopLevelWindow);
	assert(pTopLevelWindow);
	
	wxWindowID id = pTopLevelWindow->GetId();
	wxCloseEvent close(wxEVT_CLOSE_WINDOW, id);
	close.SetEventObject(pTopLevelWindow);
	
	pTopLevelWindow->AddPendingEvent(close);
	assert(WaitForMain());
	
	while ( (pWindow = FindWindow(id)) )
	{
		assert(WaitForMain());
	}
	assert(!pWindow);	
}

wxCommandEvent TestCase::GetButtonClickEvent(wxWindow* pWindow)
{
	assert(pWindow);
	
	wxButton* pButton = wxDynamicCast(pWindow, wxButton);
	assert(pButton);

	wxCommandEvent click(wxEVT_COMMAND_BUTTON_CLICKED, pButton->GetId());
	click.SetEventObject(pButton);
	
	return click;
}

void TestCase::ClickButtonWaitIdle(wxWindow* pWindow)
{
	wxCommandEvent click = GetButtonClickEvent(pWindow);
	pWindow->AddPendingEvent(click);
	assert(WaitForMain());
}

void TestCase::ClickButtonWaitEvent(wxWindow* pWindow)
{
	wxCommandEvent click = GetButtonClickEvent(pWindow);
	mpTestFrame->WaitForEvent(click);
}

void TestCase::ClickRadioWaitEvent(wxWindow* pWindow)
{
	assert(pWindow);
	
	wxRadioButton* pButton = wxDynamicCast(pWindow, wxRadioButton);
	assert(pButton);

	wxCommandEvent click(wxEVT_COMMAND_RADIOBUTTON_SELECTED, 
		pButton->GetId());
	click.SetEventObject(pButton);
	click.SetInt(1);
	
	mpTestFrame->WaitForEvent(click);
}

wxTextCtrl* TestCase::GetTextCtrl(wxWindow* pWindow, wxWindowID id)
{
	assert(pWindow);
	assert(id);
	assert(id != wxID_ANY);
	
	wxWindow* pTextCtrlWindow = pWindow->FindWindow(id);
	assert(pTextCtrlWindow);
	
	wxTextCtrl *pTextCtrl = wxDynamicCast(pTextCtrlWindow, wxTextCtrl);
	assert(pTextCtrl);
	
	return pTextCtrl;
}

class TestCanOpenAndCloseMainWindow : public TestCase
{
	virtual void RunTest()
	{
		MainFrame* pMainFrame = OpenMainFrame();
		CloseWindowWaitClosed(pMainFrame);
	}
	public:
	TestCanOpenAndCloseMainWindow(TestFrame* pTestFrame) 
	: TestCase(pTestFrame) { }
};

class TestCanOpenAndCloseSetupWizard : public TestCase
{
	virtual void RunTest()
	{
		MainFrame* pMainFrame = OpenMainFrame();
		ClickButtonWaitIdle(pMainFrame->FindWindow(
			ID_General_Setup_Wizard_Button));
		CloseWindowWaitClosed(FindWindow(ID_Setup_Wizard_Frame));
		CloseWindowWaitClosed(pMainFrame);
	}
	public:
	TestCanOpenAndCloseSetupWizard(TestFrame* pTestFrame) 
	: TestCase(pTestFrame) { }
};

/*
class TestCanRunThroughSetupWizardCreatingEverything : public TestCase
{
	public:
	TestCanRunThroughSetupWizardCreatingEverything(TestFrame* pTestFrame) 
	: TestCase(pTestFrame) { }

	protected:
	virtual void RunTest();	
};
*/

BEGIN_EVENT_TABLE(TestFrame, wxFrame)
	EVT_IDLE (TestFrame::OnIdle)
	EVT_COMMAND(wxID_ANY, CREATE_WINDOW_COMMAND, 
		TestFrame::OnCreateWindowCommand)
	EVT_COMMAND(wxID_ANY, TEST_FINISHED_EVENT, 
		TestFrame::OnTestFinishedEvent)
	EVT_BUTTON     (wxID_ANY, TestFrame::MainDoClickButton)
	EVT_RADIOBUTTON(wxID_ANY, TestFrame::MainDoClickRadio)
END_EVENT_TABLE()

TestFrame::TestFrame(const wxString PathToExecutable)
:	wxFrame(NULL, wxID_ANY, wxT("Boxi (test mode)")),
	mPathToExecutable(PathToExecutable),
	mTestThreadIsWaiting(false),
	mMainThreadIsIdle(mMutex),
	mEventHandlerFinished(mMutex),
	mpMainFrame(NULL)
{
	mTests.push_back(new TestCanOpenAndCloseMainWindow(this));
	mTests.push_back(new TestCanOpenAndCloseSetupWizard(this));
	// mTests.push_back(new TestCanRunThroughSetupWizardCreatingEverything(this));
}

void TestFrame::OnIdle(wxIdleEvent& rEvent)
{
	wxMutexLocker lock(mMutex);
	
	if (mTestThreadIsWaiting)
	{
		mMainThreadIsIdle.Signal();
		return;
	}
	
	if (mTestsInProgress)
	{
		return;
	}
	
	mTestsInProgress = true;
	mipCurrentTest = mTests.begin();
	(*mipCurrentTest)->Start();
}

void TestFrame::WaitForIdle()
{
	wxMutexLocker lock(mMutex);
	
	assert(!mTestThreadIsWaiting);
	mTestThreadIsWaiting = TRUE;

	// Disabled temporarily - may cause lockups with 
	// wxProgressDialog calling wxYield()?
	// ::wxWakeUpIdle();

	assert(mMainThreadIsIdle.Wait() == wxCOND_NO_ERROR);
	
	assert(mTestThreadIsWaiting);
	mTestThreadIsWaiting = FALSE;
}

class CreateWindowCommandData : wxClientData
{
	private:
	wxString mFileName;
	bool mHaveFileName;
	
	CreateWindowCommandData(wxChar* pFileNameChars)
	{
		if (pFileNameChars)
		{
			mFileName = pFileNameChars;
			mHaveFileName = TRUE;
		}
		else
		{
			mHaveFileName = FALSE;
		}
	}
};

void TestFrame::OnCreateWindowCommand(wxCommandEvent& rEvent)
{
	wxWindow* pWindow = wxWindow::FindWindowById(ID_Main_Frame);
	assert(!pWindow);
	
	mpMainFrame = new MainFrame 
	(
		NULL, mPathToExecutable,
		wxPoint(50, 50), wxSize(600, 500)
	);
	mpMainFrame->Show(TRUE);
}

void TestFrame::OnTestFinishedEvent(wxCommandEvent& rEvent)
{
	mipCurrentTest++;
	
	if (mipCurrentTest != mTests.end())
	{
		(*mipCurrentTest)->Start();
	}
	else
	{
		Destroy();
	}
}

void TestFrame::WaitForEvent(wxCommandEvent& rEvent)
{
	mMutex.Lock();
	AddPendingEvent(rEvent);
	assert(mEventHandlerFinished.Wait() == wxCOND_NO_ERROR);
	mMutex.Unlock();
}

void TestFrame::MainDoClickButton(wxCommandEvent& rEvent)
{
	wxObject* pObject = rEvent.GetEventObject();
	assert(pObject);
	
	wxButton* pButton = wxDynamicCast(pObject, wxButton);
	assert(pButton);

	pButton->ProcessEvent(rEvent);	
	wxMutexLocker lock(mMutex);
	mEventHandlerFinished.Signal();
}
	

void TestFrame::MainDoClickRadio(wxCommandEvent& rEvent)
{
	wxObject* pObject = rEvent.GetEventObject();
	assert(pObject);
	
	wxRadioButton* pButton = wxDynamicCast(pObject, wxRadioButton);
	assert(pButton);
	
	pButton->SetValue(TRUE);
	// pButton->ProcessEvent(rEvent);
	
	wxMutexLocker lock(mMutex);
	mEventHandlerFinished.Signal();
}

/*
void TestThread::Entry2()
{
	printf("waiting...\n");
	
	if (!WaitForMain()) return;
	
	printf("started.\n");
	
	wxCommandEvent wizard(wxEVT_COMMAND_BUTTON_CLICKED, 
		ID_General_Setup_Wizard_Button);
	wxWindow::FindWindowById(ID_General_Setup_Wizard_Button)->
		AddPendingEvent(wizard);
	if (!WaitForMain()) return;
	
	CloseWindow(ID_Setup_Wizard_Frame);
	if (!WaitForMain()) return;
		
	CloseWindow(mpMainFrame->GetId());
	if (!WaitForMain()) return;
	
	printf("terminating...\n");
	while (WaitForMain()) { }

	printf("terminated.\n");
	return;
}
*/

static message_t expMessageId   = BM_UNKNOWN;
static int       expMessageResp = -1;

#define CPPUNIT_ASSERT_AT(condition, line) \
	CppUnit::Asserter::failIf(!(condition), \
		CppUnit::Message("assertion failed", "Expression: " #condition), \
		line);

#define CPPUNIT_ASSERT_MESSAGE_AT(message, condition, line) \
	CppUnit::Asserter::failIf(!condition, \
		CppUnit::Message(message, "Expression: " #condition), \
		(line));

#define CPPUNIT_ASSERT_EQUAL_AT(expected, actual, line)          \
  ( CPPUNIT_NS::assertEquals( (expected),              \
                              (actual),                \
                              (line),    \
                              "" ) )


int MessageBoxHarness
(
	message_t messageId,
	const wxString& message,
	const wxString& caption,
	long style,
	wxWindow *parent
)
{
	if (TestFrame::IsTestsInProgress())
	{
		assert(expMessageId != BM_UNKNOWN);
	}
	
	if (expMessageId == 0)
	{
		// fall through
		return wxMessageBox(message, caption, style, parent);
	}
	
	assert(messageId == expMessageId);

	int response = expMessageResp;
	
	expMessageId   = BM_UNKNOWN;
	expMessageResp = -1;
	
	return response;
}

#define MessageBoxSetResponse(messageId, response) \
	MessageBoxSetResponseImpl(messageId, response, CPPUNIT_SOURCELINE())

void MessageBoxSetResponseImpl(message_t messageId, int response, 
	const CppUnit::SourceLine& rLine)
{
	CPPUNIT_ASSERT_MESSAGE_AT("There is already an expected message ID",
		expMessageId == 0, rLine);
	expMessageId   = messageId;
	expMessageResp = response;
}

#define MessageBoxCheckFired() CPPUNIT_ASSERT(expMessageId == BM_UNKNOWN)

class GuiStarter : public TestSetUpDecorator
{
	public:
	GuiStarter(CppUnit::Test *pTest) 
	: TestSetUpDecorator (pTest), mpResult(NULL) { }
	
	virtual void run(CppUnit::TestResult *pResult)
	{
		// Store result for latter call (direct or indirect) from
		// WxGuiTestApp::OnRun() allowing us to make up for call of
		// TestDecorator::run(result):
		mpResult = pResult;
		
		this->setUp ();
		// Do NOT call TestDecorator::run(), because flow of control
		// must be diverted to the wxApp instance, by the setUp() 
		// method, which returns when the test(s) are finished
		// TestDecorator::run (result);
		this->tearDown ();
	}
	
	virtual void RunAsDecorator()
	{
		wxASSERT(mpResult != NULL);
		TestDecorator::run(mpResult);
	}
	
	protected:
	virtual void setUp()
	{
		// at this point, we don't have a global wxApp instance,
		// but we need one for wx tests.
		BoxiApp *pBoxiApp = new BoxiApp();
		
		// This is not really necessary, as done automatically by 
		// code from the macro expansion; but it improves understanding:
		wxApp::SetInstance(pBoxiApp);

		// Store this instance for running tests from WxGuiTestApp::OnRun():
		pBoxiApp->SetTestRunner(this);

		WxGuiTestHelper::SetShowModalDialogsNonModalFlag(true);
		
		#if defined (WIN32)
			#if !defined (__BUILTIN__)
				::wxEntry (GetModuleHandle (NULL), NULL, NULL, SW_SHOWNORMAL);
			#else
				::wxEntry (GetModuleHandle (NULL), NULL, NULL, SW_SHOW);
			#endif
		#else
			::wxEntry (g_argc, g_argv);
		#endif
		
		WxGuiTestHelper::SetShowModalDialogsNonModalFlag(false);
	}	
	virtual void tearDown() { }

	private:
	// Store result sent to run() for latter call from WxGuiTestApp::OnRun():
	CppUnit::TestResult *mpResult;
};

class GuiTestSuite : public CppUnit::TestFixture
{
	public:
	GuiTestSuite() { }
	static CppUnit::Test *suite()
	{
		CPPUNIT_NS::TestSuite *suiteOfTests = new CPPUNIT_NS::TestSuite ();
	
		// Add all tests of specially named registry WxGuiTest
		CPPUNIT_NS::Test *wxGuiTestSuite = CppUnit::TestFactoryRegistry
			::getRegistry("WxGuiTest").makeTest ();
		suiteOfTests->addTest(wxGuiTestSuite);
	
		GuiStarter *starter = new GuiStarter(suiteOfTests);
		return starter;
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(GuiTestSuite);

class GuiTestBase : public CppUnit::TestCase
{
	public: 
	GuiTestBase() : CppUnit::TestCase("GuiTestBase") {}
	virtual void RunTest() = 0;
		
	protected:
	void ClickButtonWaitIdle  (wxWindow* pWindow);
	void ClickButtonWaitEvent (wxWindow* pButton);
	void ClickRadioWaitEvent  (wxWindow* pButton);
	void CloseWindow          (wxWindow* pWindow);
	void CloseWindowWaitClosed(wxWindow* pWindow);
	MainFrame* GetMainFrame();
	wxWindow*  FindWindow(wxWindowID id) 
	{ 
		return wxWindow::FindWindowById(id); 
	}
	wxTextCtrl* GetTextCtrl  (wxWindow* pWindow, wxWindowID id);
	
	private:
	wxCommandEvent GetButtonClickEvent(wxWindow* pWindow);

	public:	
	void tearDown()
	{
		expMessageId   = BM_UNKNOWN;
		expMessageResp = -1;
		
		for (size_t i = 0; i < wxTopLevelWindows.GetCount(); i++)
		{
			wxWindow* pOpenWindow = 
				wxTopLevelWindows.Item(i)->GetData();
			CloseWindow(pOpenWindow);
		}
		
		while (wxTopLevelWindows.GetCount() > 0)
		{
			CPPUNIT_ASSERT_EQUAL(0, WxGuiTestHelper::FlushEventQueue());
		}
	}	
};

wxCommandEvent GuiTestBase::GetButtonClickEvent(wxWindow* pWindow)
{
	assert(pWindow);
	
	wxButton* pButton = wxDynamicCast(pWindow, wxButton);
	assert(pButton);

	wxCommandEvent click(wxEVT_COMMAND_BUTTON_CLICKED, pButton->GetId());
	click.SetEventObject(pButton);
	
	return click;
}

void GuiTestBase::ClickButtonWaitIdle(wxWindow* pWindow)
{
	wxCommandEvent click = GetButtonClickEvent(pWindow);
	pWindow->AddPendingEvent(click);
	assert(WxGuiTestHelper::FlushEventQueue() == 0);
}

void GuiTestBase::ClickButtonWaitEvent(wxWindow* pWindow)
{
	wxCommandEvent click = GetButtonClickEvent(pWindow);
	pWindow->ProcessEvent(click);
}

void GuiTestBase::ClickRadioWaitEvent(wxWindow* pWindow)
{
	CPPUNIT_ASSERT(pWindow);
	
	wxRadioButton* pButton = wxDynamicCast(pWindow, wxRadioButton);
	assert(pButton);
	
	pButton->SetValue(true);

	wxCommandEvent click(wxEVT_COMMAND_RADIOBUTTON_SELECTED, 
		pButton->GetId());
	click.SetEventObject(pButton);
	click.SetInt(1);

	pWindow->ProcessEvent(click);
}

void GuiTestBase::CloseWindow(wxWindow* pWindow)
{
	CPPUNIT_ASSERT(pWindow);
	
	wxTopLevelWindow* pTopLevelWindow = 
		wxDynamicCast(pWindow, wxTopLevelWindow);
	CPPUNIT_ASSERT(pTopLevelWindow);
	
	wxWindowID id = pTopLevelWindow->GetId();
	wxCloseEvent close(wxEVT_CLOSE_WINDOW, id);
	close.SetEventObject(pTopLevelWindow);
	
	pTopLevelWindow->AddPendingEvent(close);
}

void GuiTestBase::CloseWindowWaitClosed(wxWindow* pWindow)
{
	wxWindowID id = pWindow->GetId();
	CloseWindow(pWindow);
	
	while ( (pWindow = wxWindow::FindWindowById(id)) )
	{
		assert(WxGuiTestHelper::FlushEventQueue() == 0);
	}
	
	CPPUNIT_ASSERT(!pWindow);	
}

MainFrame* GuiTestBase::GetMainFrame()
{
	MainFrame* pFrame = new MainFrame 
	(
		NULL,
		wxString(g_argv[0], wxConvLibc),
		wxPoint(50, 50), wxSize(600, 500)
	);
	pFrame->Show(TRUE);

	MainFrame* pFrame2 = (MainFrame*)wxWindow::FindWindowById(ID_Main_Frame);
	CPPUNIT_ASSERT_EQUAL(pFrame, pFrame2);
	
	return pFrame;
}

wxTextCtrl* GuiTestBase::GetTextCtrl(wxWindow* pWindow, wxWindowID id)
{
	assert(pWindow);
	assert(id);
	assert(id != wxID_ANY);
	
	wxWindow* pTextCtrlWindow = pWindow->FindWindow(id);
	assert(pTextCtrlWindow);
	
	wxTextCtrl *pTextCtrl = wxDynamicCast(pTextCtrlWindow, wxTextCtrl);
	assert(pTextCtrl);
	
	return pTextCtrl;
}

#define TEST_CASE(TestCaseName) \
class TestCaseName : public GuiTestBase \
{ \
	public: \
	TestCaseName() { } \
	virtual void RunTest(); \
	static CppUnit::Test *suite(); \
}; \
 \
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestCaseName, "WxGuiTest"); \
 \
CppUnit::Test *TestCaseName::suite() \
{ \
	CppUnit::TestSuite *suiteOfTests = \
		new CppUnit::TestSuite(#TestCaseName); \
	suiteOfTests->addTest( \
		new CppUnit::TestCaller<TestCaseName>(#TestCaseName, \
			&TestCaseName::RunTest)); \
	return suiteOfTests; \
}

TEST_CASE(TestOpenWizard)

void TestOpenWizard::RunTest() 
{
	wxWindow* pMainFrame = GetMainFrame();
	CPPUNIT_ASSERT(pMainFrame);

	wxWindow* pWizardButton = 
		pMainFrame->FindWindow(ID_General_Setup_Wizard_Button);
	CPPUNIT_ASSERT(pWizardButton);
	ClickButtonWaitIdle(pWizardButton);
	
	wxWindow* pWizardFrame = FindWindow(ID_Setup_Wizard_Frame);
	CPPUNIT_ASSERT(pWizardFrame);
	
	CloseWindowWaitClosed(pWizardFrame);
	CloseWindowWaitClosed(pMainFrame);
}

class TestCanRunThroughSetupWizardCreatingEverything : public GuiTestBase
{
	public:
	TestCanRunThroughSetupWizardCreatingEverything() { }
	virtual void RunTest();
	static CppUnit::Test *suite();
	
	private:
	wxButton*    mpForwardButton;
	SetupWizard* mpWizard;

	void CheckForwardErrorImpl(message_t messageId, 
		const wxString& rMessageName, const CppUnit::SourceLine& rLine);
	X509* LoadCertificate(const wxFileName& rCertFile);
	void SignRequestWithKey
	(
		const wxFileName& rRequestFile, 
		const wxFileName& rKeyFile, 
		const wxFileName& rIssuerCertFile, 
		const wxFileName& rCertFile
	);
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestCanRunThroughSetupWizardCreatingEverything, "WxGuiTest");

CppUnit::Test *TestCanRunThroughSetupWizardCreatingEverything::suite()
{
	CppUnit::TestSuite *suiteOfTests =
		new CppUnit::TestSuite("TestCanRunThroughSetupWizardCreatingEverything");
	suiteOfTests->addTest(
		new CppUnit::TestCaller<TestCanRunThroughSetupWizardCreatingEverything>(
			"TestCanRunThroughSetupWizardCreatingEverything",
			&TestCanRunThroughSetupWizardCreatingEverything::RunTest));
	return suiteOfTests;
}

static int numCheckedTest = 0;

static wxString MySslProgressCallbackMessage(int n)
{
	wxString msg;
	msg.Printf(wxT("Generating a fake server private key (2048 bits).\n"
			"This may take a minute.\n\n"
			"(%d potential primes checked)"), n);
	return msg;
}

static void MySslProgressCallbackTest(int p, int n, void* cbinfo)
{
	wxProgressDialog* pProgress = (wxProgressDialog *)cbinfo;
	pProgress->Update(0, MySslProgressCallbackMessage(++numCheckedTest));
}

X509* TestCanRunThroughSetupWizardCreatingEverything::LoadCertificate
(const wxFileName& rCertFile)
{
	BIO* pCertBio = BIO_new(BIO_s_file());
	CPPUNIT_ASSERT(pCertBio);
	
	wxCharBuffer buf = rCertFile.GetFullPath().mb_str(wxConvLibc);
	int result = BIO_read_filename(pCertBio, buf.data());
	CPPUNIT_ASSERT(result > 0);
	
	X509* pCert = PEM_read_bio_X509_AUX(pCertBio, NULL, NULL, NULL);
	CPPUNIT_ASSERT(pCert);
	
	return pCert; // caller must free with X509_free
}

void TestCanRunThroughSetupWizardCreatingEverything::SignRequestWithKey
(
	const wxFileName& rRequestFile, 
	const wxFileName& rKeyFile, 
	const wxFileName& rIssuerCertFile, 
	const wxFileName& rCertFile
)
{
	CPPUNIT_ASSERT(rRequestFile.FileExists());
	CPPUNIT_ASSERT(rKeyFile.FileExists());
	CPPUNIT_ASSERT(!rIssuerCertFile.IsOk() || rIssuerCertFile.FileExists());
	CPPUNIT_ASSERT(!rCertFile.FileExists());
	
	X509V3_CTX ctx2;
	X509V3_set_ctx_test(&ctx2);
	
	/*
	CONF *extconf = NULL;
	X509V3_set_nconf(&ctx2, extconf);
	
	char* extsect = "v3_ca";
	
	CPPUNIT_ASSERT_MESSAGE("Error loading extension section",
		X509V3_EXT_add_nconf(extconf, &ctx2, extsect, NULL));
	*/
	
	BIO* pRequestBio = BIO_new(BIO_s_file());
	CPPUNIT_ASSERT_MESSAGE("Failed to create an OpenSSL BIO", pRequestBio);
	
	wxCharBuffer buf = rRequestFile.GetFullPath().mb_str(wxConvLibc);
	char* filename = strdup(buf.data());
	CPPUNIT_ASSERT_MESSAGE("Failed to read certificate signing request",
		BIO_read_filename(pRequestBio, filename) > 0);
	free(filename);
	
	X509_REQ* pRequest = PEM_read_bio_X509_REQ(pRequestBio,NULL,NULL,NULL);
	CPPUNIT_ASSERT_MESSAGE("Failed to parse certificate signing request",
		pRequest);
	
	BIO_free(pRequestBio);
	pRequestBio = NULL;

	CPPUNIT_ASSERT_MESSAGE("The certificate does not appear to "
		"contain a public key", pRequest->req_info);

	CPPUNIT_ASSERT_MESSAGE("The certificate does not appear to "
		"contain a public key", pRequest->req_info->pubkey);

	CPPUNIT_ASSERT_MESSAGE("The certificate does not appear to "
		"contain a public key", 
		pRequest->req_info->pubkey->public_key);

	CPPUNIT_ASSERT_MESSAGE("The certificate does not appear to "
		"contain a public key", 
		pRequest->req_info->pubkey->public_key->data);

	EVP_PKEY* pRequestKey = X509_REQ_get_pubkey(pRequest);
	CPPUNIT_ASSERT_MESSAGE("Error unpacking request public key", pRequestKey);
	
	CPPUNIT_ASSERT_MESSAGE("Signature verification error",
		X509_REQ_verify(pRequest, pRequestKey) >= 0);

	CPPUNIT_ASSERT_MESSAGE("Signature did not match the "
		"certificate request",
		X509_REQ_verify(pRequest, pRequestKey) != 0);

	wxString msg;
	EVP_PKEY* pSigKey = LoadKey(rKeyFile.GetFullPath(), &msg);
	CPPUNIT_ASSERT_MESSAGE("Failed to load private key", pSigKey);	

	X509* pIssuerCert = NULL;
	if (rIssuerCertFile.IsOk())
	{
		pIssuerCert = LoadCertificate(rIssuerCertFile);
		CPPUNIT_ASSERT(pIssuerCert);
	
		{
			EVP_PKEY* pIssuerPublicKey = X509_get_pubkey(pIssuerCert);
			CPPUNIT_ASSERT(pIssuerPublicKey);
			
			EVP_PKEY_copy_parameters(pIssuerPublicKey, pSigKey);
			EVP_PKEY_free(pIssuerPublicKey);
		}
	
		// allow badly signed certificates for testing
		// CPPUNIT_ASSERT(X509_check_private_key(pIssuerCert, pSigKey));
	}
	
	X509_STORE * pStore = X509_STORE_new();
	CPPUNIT_ASSERT(pStore);
	CPPUNIT_ASSERT(X509_STORE_set_default_paths(pStore));
	
	X509* pNewCert = X509_new();
	CPPUNIT_ASSERT_MESSAGE("Error creating certificate object",
		pNewCert);
		
	X509_STORE_CTX pContext;
	CPPUNIT_ASSERT(X509_STORE_CTX_init(&pContext, pStore, pNewCert, NULL));
	X509_STORE_CTX_set_cert(&pContext, pNewCert);
	
	ASN1_INTEGER* pSerialNo = ASN1_INTEGER_new();
	CPPUNIT_ASSERT_MESSAGE("Failed to set serial number", pSerialNo);
	
	BIGNUM *btmp = BN_new();
	CPPUNIT_ASSERT(btmp);
	
	CPPUNIT_ASSERT(BN_pseudo_rand(btmp, 64, 0, 0));
	CPPUNIT_ASSERT(BN_to_ASN1_INTEGER(btmp, pSerialNo));
	BN_free(btmp);
	
	CPPUNIT_ASSERT_MESSAGE("Failed to set certificate serial number",
		X509_set_serialNumber(pNewCert, pSerialNo));
	ASN1_INTEGER_free(pSerialNo);

	if (pIssuerCert)
	{		
		CPPUNIT_ASSERT_MESSAGE("Failed to set certificate issuer",
			X509_set_issuer_name(pNewCert, 
				X509_get_subject_name(pIssuerCert)));
	}
	else
	{
		// self signed
		
		CPPUNIT_ASSERT_MESSAGE("Failed to set certificate issuer",
			X509_set_issuer_name(pNewCert, 
				pRequest->req_info->subject));
	}
		
	CPPUNIT_ASSERT_MESSAGE("Failed to set certificate subject",
		X509_set_subject_name(pNewCert, pRequest->req_info->subject));
	
	X509_set_pubkey(pNewCert, pRequestKey);

	/*
	EVP_PKEY_copy_parameters(pIssuerPublicKey, pSigKey);
	EVP_PKEY_save_parameters(pKey, 1);
	EVP_PKEY_free(pKey);
	pKey = NULL;
	*/

	CPPUNIT_ASSERT_MESSAGE("Failed to set certificate signing date",
		X509_gmtime_adj(X509_get_notBefore(pNewCert), 0));
	CPPUNIT_ASSERT_MESSAGE("Failed to set certificate expiry date",
		X509_gmtime_adj(X509_get_notAfter(pNewCert), (long)60*60*24*1));

	/*
	if (extconf)
	{
		X509V3_CTX ctx;
		X509_set_version(pCert, 2); // version 3 certificate
		X509V3_set_ctx(&ctx, pCert, pCert, NULL, NULL, 0);
		X509V3_set_nconf(&ctx, extconf);
		CPPUNIT_ASSERT(X509V3_EXT_add_nconf(extconf, &ctx, 
			extsect, pCert));
	}
	*/
	
	CPPUNIT_ASSERT_MESSAGE("Failed to sign certificate",
		X509_sign(pNewCert, pSigKey, EVP_sha1()));
	
	BIO* pOut = BIO_new(BIO_s_file());
	CPPUNIT_ASSERT_MESSAGE("Failed to create output BIO", pOut);

	buf = rCertFile.GetFullPath().mb_str(wxConvLibc);
	filename = strdup(buf.data());
	CPPUNIT_ASSERT_MESSAGE("Failed to set output filename", 
		BIO_write_filename(pOut, filename));
	free(filename);
	
	CPPUNIT_ASSERT_MESSAGE("Failed to write certificate", 
		PEM_write_bio_X509(pOut, pNewCert));
	
	BIO_free_all(pOut);
	X509_REQ_free(pRequest);
	X509_free(pNewCert);
	if (pIssuerCert) X509_free(pIssuerCert);
	X509_STORE_CTX_cleanup(&pContext);	
	X509_STORE_free(pStore);
	// NCONF_free(extconf);
	EVP_PKEY_free(pSigKey);
	EVP_PKEY_free(pRequestKey);

	CPPUNIT_ASSERT(rCertFile.FileExists());
}

#define CheckForwardError(messageId) \
	this->TestCanRunThroughSetupWizardCreatingEverything::CheckForwardErrorImpl \
	(messageId, _(#messageId), CPPUNIT_SOURCELINE())

void TestCanRunThroughSetupWizardCreatingEverything::CheckForwardErrorImpl
(
	message_t messageId, 
	const wxString& rMessageName, 
	const CppUnit::SourceLine& rLine
)
{
	CPPUNIT_ASSERT_AT(mpWizard, rLine);
	CPPUNIT_ASSERT_AT(mpForwardButton, rLine);

	SetupWizardPage_id_t oldPageId = mpWizard->GetCurrentPageId();
	MessageBoxSetResponse(messageId, wxID_OK);

	ClickButtonWaitEvent(mpForwardButton);
	
	wxString msg;
	msg.Printf(_("Expected error message was not shown: %s"),
		rMessageName.c_str());
	
	CPPUNIT_ASSERT_MESSAGE_AT(msg.mb_str(wxConvLibc).data(), 
		expMessageId == BM_UNKNOWN, rLine);
	
	CPPUNIT_ASSERT_EQUAL_AT(oldPageId, mpWizard->GetCurrentPageId(), rLine);
}

void TestCanRunThroughSetupWizardCreatingEverything::RunTest()
{
	MainFrame* pMainFrame = GetMainFrame();
	
	CPPUNIT_ASSERT(!FindWindow(ID_Setup_Wizard_Frame));
	ClickButtonWaitIdle(pMainFrame->FindWindow(
		ID_General_Setup_Wizard_Button));
	
	mpWizard = (SetupWizard*)FindWindow(ID_Setup_Wizard_Frame);
	CPPUNIT_ASSERT(mpWizard);
	CPPUNIT_ASSERT_EQUAL(BWP_INTRO, mpWizard->GetCurrentPageId());
	
	mpForwardButton = (wxButton*) mpWizard->FindWindow(wxID_FORWARD);
	wxWindow* pBackButton = mpWizard->FindWindow(wxID_BACKWARD);

	CPPUNIT_ASSERT(mpForwardButton);
	CPPUNIT_ASSERT(pBackButton);
	
	ClickButtonWaitEvent(mpForwardButton);
	CPPUNIT_ASSERT_EQUAL(BWP_ACCOUNT, mpWizard->GetCurrentPageId());
	
	wxTextCtrl* pStoreHost;
	wxTextCtrl* pAccountNo;
	{
		wxWizardPage* pPage = mpWizard->GetCurrentPage();
		pStoreHost = GetTextCtrl(pPage, 
			ID_Setup_Wizard_Store_Hostname_Ctrl);
		pAccountNo = GetTextCtrl(pPage,
			ID_Setup_Wizard_Account_Number_Ctrl);
	}
	CPPUNIT_ASSERT(pStoreHost->GetValue().IsSameAs(wxEmptyString));
	CPPUNIT_ASSERT(pAccountNo->GetValue().IsSameAs(wxEmptyString));
	
	MessageBoxSetResponse(BM_SETUP_WIZARD_BAD_STORE_HOST, wxOK);
	ClickButtonWaitEvent(mpForwardButton);
	MessageBoxCheckFired();
	CPPUNIT_ASSERT_EQUAL(BWP_ACCOUNT, mpWizard->GetCurrentPageId());

	ClientConfig* pConfig = pMainFrame->GetConfig();
	CPPUNIT_ASSERT(pConfig);
	CPPUNIT_ASSERT(!pConfig->StoreHostname.IsConfigured());
	
	pStoreHost->SetValue(wxT("no-such-host"));
	// calling SetValue should configure the property by itself
	CPPUNIT_ASSERT(pConfig->StoreHostname.IsConfigured());
	CheckForwardError(BM_SETUP_WIZARD_BAD_STORE_HOST);
	CPPUNIT_ASSERT(pConfig->StoreHostname.IsConfigured());
	
	pStoreHost->SetValue(wxT("localhost"));
	CheckForwardError(BM_SETUP_WIZARD_BAD_ACCOUNT_NO);
	
	pAccountNo->SetValue(wxT("localhost")); // invalid number
	CheckForwardError(BM_SETUP_WIZARD_BAD_ACCOUNT_NO);
	CPPUNIT_ASSERT(!pConfig->AccountNumber.IsConfigured());
	
	pAccountNo->SetValue(wxT("-1"));
	CheckForwardError(BM_SETUP_WIZARD_BAD_ACCOUNT_NO);
	CPPUNIT_ASSERT(pConfig->AccountNumber.IsConfigured());

	pAccountNo->SetValue(wxT("1"));
	ClickButtonWaitEvent(mpForwardButton);
	MessageBoxCheckFired();
	CPPUNIT_ASSERT_EQUAL(BWP_PRIVATE_KEY, mpWizard->GetCurrentPageId());
	
	wxString StoredHostname;
	CPPUNIT_ASSERT(pConfig->StoreHostname.GetInto(StoredHostname));
	CPPUNIT_ASSERT(StoredHostname.IsSameAs(wxT("localhost")));
	
	int AccountNumber;
	CPPUNIT_ASSERT(pConfig->AccountNumber.GetInto(AccountNumber));
	CPPUNIT_ASSERT_EQUAL(1, AccountNumber);

	wxFileName configTestDir;
	configTestDir.AssignTempFileName(wxT("boxi-configTestDir-"));
	CPPUNIT_ASSERT(wxRemoveFile(configTestDir.GetFullPath()));
	CPPUNIT_ASSERT(configTestDir.Mkdir(wxS_IRUSR | wxS_IWUSR | wxS_IXUSR));
	
	wxFileName tempdir;
	tempdir.AssignTempFileName(wxT("boxi-tempdir-"));
	CPPUNIT_ASSERT(wxRemoveFile(tempdir.GetFullPath()));
	
	wxFileName privateKeyFileName(configTestDir.GetFullPath(), 
		wxT("1-key.pem"));

	{
		CPPUNIT_ASSERT_EQUAL(BWP_PRIVATE_KEY, mpWizard->GetCurrentPageId());

		wxRadioButton* pNewFileRadioButton = 
			(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
				ID_Setup_Wizard_New_File_Radio);
		ClickRadioWaitEvent(pNewFileRadioButton);
		CPPUNIT_ASSERT(pNewFileRadioButton->GetValue());
		
		CheckForwardError(BM_SETUP_WIZARD_NO_FILE_NAME);
		
		wxTextCtrl* pFileName = GetTextCtrl(
			mpWizard->GetCurrentPage(),
			ID_Setup_Wizard_File_Name_Text);
		
		CPPUNIT_ASSERT(pFileName->GetValue().IsSameAs(wxEmptyString));

		wxFileName nonexistantfile(tempdir.GetFullPath(), 
			wxT("nonexistant"));
		pFileName->SetValue(nonexistantfile.GetFullPath());
	
		// filename refers to a path that doesn't exist
		// expect BM_SETUP_WIZARD_FILE_DIR_NOT_FOUND
		CheckForwardError(BM_SETUP_WIZARD_FILE_DIR_NOT_FOUND);
	
		// create the directory, but not the file
		// make the directory not writable
		// expect BM_SETUP_WIZARD_FILE_DIR_NOT_WRITABLE
		CPPUNIT_ASSERT(tempdir.Mkdir(wxS_IRUSR | wxS_IXUSR));
		CPPUNIT_ASSERT(tempdir.DirExists());
		
		CheckForwardError(BM_SETUP_WIZARD_FILE_DIR_NOT_WRITABLE);
		
		// change the filename to refer to the directory
		// expect BM_SETUP_WIZARD_FILE_IS_A_DIRECTORY
		pFileName->SetValue(tempdir.GetFullPath());
		CPPUNIT_ASSERT(pFileName->GetValue().IsSameAs(tempdir.GetFullPath()));
		CheckForwardError(BM_SETUP_WIZARD_FILE_IS_A_DIRECTORY);
		
		// make the directory read write
		assert(tempdir.Rmdir());
		assert(tempdir.Mkdir(wxS_IRUSR | wxS_IWUSR | wxS_IXUSR));
		
		// create a new file in the directory, make it read only, 
		// and change the filename to refer to it. 
		// expect BM_SETUP_WIZARD_FILE_OVERWRITE
		wxFileName existingfile(tempdir.GetFullPath(), wxT("existing"));
		wxString   existingpath = existingfile.GetFullPath();
		wxFile     existing;

		CPPUNIT_ASSERT(existing.Create(existingpath, false, wxS_IRUSR));
		CPPUNIT_ASSERT(  wxFile::Access(existingpath, wxFile::read));
		CPPUNIT_ASSERT(! wxFile::Access(existingpath, wxFile::write));
		
		pFileName->SetValue(existingpath);
		CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_WRITABLE);
	
		// make the file writable, expect BM_SETUP_WIZARD_FILE_OVERWRITE
		// reply No and check that we stay put
		
		CPPUNIT_ASSERT(wxRemoveFile(existingpath));
		CPPUNIT_ASSERT(existing.Create(existingpath, false, 
			wxS_IRUSR | wxS_IWUSR));
		CPPUNIT_ASSERT(wxFile::Access(existingpath, wxFile::read));
		CPPUNIT_ASSERT(wxFile::Access(existingpath, wxFile::write));
		
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_OVERWRITE, wxNO);
		ClickButtonWaitEvent(mpForwardButton);
		MessageBoxCheckFired();
		
		CPPUNIT_ASSERT_EQUAL(BWP_PRIVATE_KEY, mpWizard->GetCurrentPageId());
		
		// now try again, reply Yes, and check that we move forward
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_OVERWRITE, wxYES);
		ClickButtonWaitEvent(mpForwardButton);
		MessageBoxCheckFired();

		CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());

		// was the configuration updated?
		wxString keyfile;
		CPPUNIT_ASSERT(pConfig->PrivateKeyFile.GetInto(keyfile));
		CPPUNIT_ASSERT(keyfile.IsSameAs(existingpath));

		// go back, select "existing", choose a nonexistant file,
		// expect BM_SETUP_WIZARD_FILE_NOT_FOUND
		ClickButtonWaitEvent(pBackButton);
		CPPUNIT_ASSERT_EQUAL(BWP_PRIVATE_KEY, mpWizard->GetCurrentPageId());
		
		wxRadioButton* pExistingFileRadioButton = 
			(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
				ID_Setup_Wizard_Existing_File_Radio);
		CPPUNIT_ASSERT(pExistingFileRadioButton);
		ClickRadioWaitEvent(pExistingFileRadioButton);
		CPPUNIT_ASSERT(pExistingFileRadioButton->GetValue());
		
		CPPUNIT_ASSERT(pFileName->GetValue().IsSameAs(existingpath));
		
		pFileName->SetValue(nonexistantfile.GetFullPath());
		CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_FOUND);
		
		// set the path to a bogus path, 
		// expect BM_SETUP_WIZARD_FILE_NOT_FOUND
		wxString boguspath = nonexistantfile.GetFullPath();
		boguspath.Append(wxT("/foo/bar"));
		pFileName->SetValue(boguspath);
		CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_FOUND);
		
		// create another file, make it unreadable,
		// expect BM_SETUP_WIZARD_FILE_NOT_READABLE
		wxString anotherfilename = existingpath;
		anotherfilename.Append(wxT("2"));
		wxFile anotherfile;
		CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, false, 0));
		
		pFileName->SetValue(anotherfilename);
		CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_READABLE);
		
		// make it readable, but not a valid key,
		// expect BM_SETUP_WIZARD_BAD_PRIVATE_KEY
		CPPUNIT_ASSERT(wxRemoveFile(anotherfilename));
		CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, wxS_IRUSR));
		CheckForwardError(BM_SETUP_WIZARD_BAD_PRIVATE_KEY);
		
		// delete that file, move the private key file to the 
		// configTestDir, set the filename to that new location,
		// expect that Boxi can read the key file and we move on
		// to the next page
		CPPUNIT_ASSERT(::wxRemoveFile(anotherfilename));
		CPPUNIT_ASSERT(::wxRenameFile(existingpath, 
			privateKeyFileName.GetFullPath()));
		pFileName->SetValue(privateKeyFileName.GetFullPath());
		ClickButtonWaitEvent(mpForwardButton);

		CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());
		
		// was the configuration updated?
		CPPUNIT_ASSERT(pConfig->PrivateKeyFile.GetInto(keyfile));
		CPPUNIT_ASSERT(keyfile.IsSameAs(privateKeyFileName.GetFullPath()));
	}

	{
		CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());

		// ask for a new certificate request to be generated
		wxRadioButton* pNewFileRadioButton =
			(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
				ID_Setup_Wizard_New_File_Radio);
		ClickRadioWaitEvent(pNewFileRadioButton);
		CPPUNIT_ASSERT(pNewFileRadioButton->GetValue());
		
		ClickButtonWaitEvent(mpForwardButton);
		
		CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());
	}

	wxFileName clientCsrFileName(configTestDir.GetFullPath(), 
		wxT("1-csr.pem"));

	{
		CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());

		// ask for a new certificate request to be generated
		wxRadioButton* pNewFileRadioButton = 
			(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
				ID_Setup_Wizard_New_File_Radio);
		ClickRadioWaitEvent(pNewFileRadioButton);
		CPPUNIT_ASSERT(pNewFileRadioButton->GetValue());
		
		wxString tmp;
		
		CPPUNIT_ASSERT(pConfig->CertRequestFile.IsConfigured());
		CPPUNIT_ASSERT(pConfig->CertRequestFile.GetInto(tmp));
		CPPUNIT_ASSERT(tmp.IsSameAs(clientCsrFileName.GetFullPath()));
		
		// go back, and check that the property is unconfigured for us
		CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());
		ClickButtonWaitEvent(pBackButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());
		CPPUNIT_ASSERT(!pConfig->CertRequestFile.IsConfigured());
		
		// set the certificate file manually, 
		// check that it is not overwritten.
		wxFileName dummyName(tempdir.GetFullPath(), wxT("nosuchfile"));
		pConfig->CertRequestFile.Set(dummyName.GetFullPath());
		
		CPPUNIT_ASSERT(pConfig->CertRequestFile.IsConfigured());
		CPPUNIT_ASSERT(pConfig->CertRequestFile.GetInto(tmp));
		CPPUNIT_ASSERT(tmp.IsSameAs(dummyName.GetFullPath()));
		
		CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());
		ClickButtonWaitEvent(mpForwardButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());
		CPPUNIT_ASSERT(pConfig->CertRequestFile.GetInto(tmp));
		CPPUNIT_ASSERT(tmp.IsSameAs(dummyName.GetFullPath()));

		// clear the value again, check that the property is 
		// unconfigured
		wxTextCtrl* pText = GetTextCtrl(mpWizard->GetCurrentPage(), 
			ID_Setup_Wizard_File_Name_Text);
		CPPUNIT_ASSERT(pText);
		pText->SetValue(wxEmptyString);
		CPPUNIT_ASSERT(!pConfig->CertRequestFile.IsConfigured());
		
		// go back, forward again, check that the default 
		// value is inserted
		CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());

		ClickButtonWaitEvent(pBackButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());
		
		ClickButtonWaitEvent(mpForwardButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());
		
		CPPUNIT_ASSERT(pConfig->CertRequestFile.IsConfigured());
		CPPUNIT_ASSERT(pConfig->CertRequestFile.GetInto(tmp));
		CPPUNIT_ASSERT(tmp.IsSameAs(clientCsrFileName.GetFullPath()));
		
		// go forward, check that the file is created
		CPPUNIT_ASSERT(!clientCsrFileName.FileExists());
		CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());

		ClickButtonWaitEvent(mpForwardButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
		CPPUNIT_ASSERT(clientCsrFileName.FileExists());
		
		// go back, choose "existing cert request", check that
		// our newly generated request is accepted.
		CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
		
		ClickButtonWaitEvent(pBackButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());
		
		ClickButtonWaitEvent(mpForwardButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());
		
		wxRadioButton* pExistingFileRadioButton = 
			(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
				ID_Setup_Wizard_Existing_File_Radio);
		CPPUNIT_ASSERT(pExistingFileRadioButton);
		ClickRadioWaitEvent(pExistingFileRadioButton);
		CPPUNIT_ASSERT(pExistingFileRadioButton->GetValue());
			
		CPPUNIT_ASSERT(pConfig->CertRequestFile.IsConfigured());
		CPPUNIT_ASSERT(pConfig->CertRequestFile.GetInto(tmp));
		CPPUNIT_ASSERT(tmp.IsSameAs(clientCsrFileName.GetFullPath()));
		
		ClickButtonWaitEvent(mpForwardButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
	}
	
	// fake a server signing of this private keyfile

	wxFileName clientCertFileName(configTestDir.GetFullPath(), 
		wxT("1-cert.pem"));
	CPPUNIT_ASSERT(!clientCertFileName.FileExists());
	
	wxFileName caFileName(configTestDir.GetFullPath(), 
		wxT("client-ca-cert.pem"));
	CPPUNIT_ASSERT(!caFileName.FileExists());
	
	wxFileName clientBadSigCertFileName(configTestDir.GetFullPath(), 
		wxT("1-cert-badsig.pem"));
	CPPUNIT_ASSERT(!clientBadSigCertFileName.FileExists());
	
	wxFileName clientSelfSigCertFileName(configTestDir.GetFullPath(), 
		wxT("1-cert-selfsig.pem"));
	CPPUNIT_ASSERT(!clientSelfSigCertFileName.FileExists());
	
	{
		// generate a private key for the fake client CA
		
		wxFileName clientCaKeyFileName(configTestDir.GetFullPath(), 
			wxT("client-ca-key.pem"));
		CPPUNIT_ASSERT(!clientCaKeyFileName.FileExists());
		
		BIGNUM* pBigNum = BN_new();
		CPPUNIT_ASSERT_MESSAGE("Failed to create an OpenSSL BN object",
			pBigNum);
		
		RSA* pRSA = RSA_new();
		CPPUNIT_ASSERT_MESSAGE("Failed to create an OpenSSL RSA object",
			pRSA);

		BIO* pKeyOutBio = BIO_new(BIO_s_file());
		CPPUNIT_ASSERT_MESSAGE("Failed to create an OpenSSL BIO object",
			pKeyOutBio);
		
		char* filename = strdup
		(
			clientCaKeyFileName.GetFullPath().mb_str(wxConvLibc).data()
		);
		CPPUNIT_ASSERT_MESSAGE("Failed to open key file using an OpenSSL BIO",
			BIO_write_filename(pKeyOutBio, filename) >= 0);
		free(filename);
		
		CPPUNIT_ASSERT_MESSAGE("Failed to set key type to RSA F4",
			BN_set_word(pBigNum, RSA_F4));

		wxProgressDialog progress(wxT("Boxi: Generating Server Key"),
			MySslProgressCallbackMessage(0), 2, pMainFrame, 
			wxPD_APP_MODAL | wxPD_ELAPSED_TIME);
		numCheckedTest = 0;
		
		RSA* pRSA2 = RSA_generate_key(2048, 0x10001, 
			MySslProgressCallbackTest, &progress);
		CPPUNIT_ASSERT_MESSAGE("Failed to generate an RSA key", pRSA2);

		progress.Hide();
		
		CPPUNIT_ASSERT_MESSAGE("Failed to write the key "
			"to the specified file",
			PEM_write_bio_RSAPrivateKey(pKeyOutBio, pRSA2, NULL, 
				NULL, 0, NULL, NULL));
		
		BIO_free_all(pKeyOutBio);
		RSA_free(pRSA);
		BN_free(pBigNum);

		CPPUNIT_ASSERT(clientCaKeyFileName.FileExists());
		
		// generate a certificate signing request (CSR) for 
		// the fake client CA, which we will sign ourselves

		wxFileName clientCaCsrFileName(configTestDir.GetFullPath(), 
			wxT("client-ca-csr.pem"));
		CPPUNIT_ASSERT(!clientCaCsrFileName.FileExists());

		wxString msg;
		std::auto_ptr<SslConfig> apConfig(SslConfig::Get(&msg));
		CPPUNIT_ASSERT_MESSAGE("Error loading OpenSSL config", 
			apConfig.get());

		char* pExtensions = NCONF_get_string(apConfig->GetConf(), "req", 
			"x509_extensions");
		if (pExtensions) 
		{
			/* Check syntax of file */
			X509V3_CTX ctx;
			X509V3_set_ctx_test(&ctx);
			X509V3_set_nconf(&ctx, apConfig->GetConf());
			
			CPPUNIT_ASSERT_MESSAGE("Error in OpenSSL configuration: "
				"Failed to load certificate extensions section",
				X509V3_EXT_add_nconf(apConfig->GetConf(), &ctx, 
					pExtensions, NULL));
		}
		else
		{
			ERR_clear_error();
		}
		
		CPPUNIT_ASSERT_MESSAGE("Failed to set the OpenSSL string mask to "
			"'nombstr'", 
			ASN1_STRING_set_default_mask_asc("nombstr"));
		
		EVP_PKEY* pKey = LoadKey(clientCaKeyFileName.GetFullPath(), &msg);
		CPPUNIT_ASSERT_MESSAGE("Failed to load private key", pKey);

		BIO* pRequestBio = BIO_new(BIO_s_file());
		CPPUNIT_ASSERT_MESSAGE("Failed to create an OpenSSL BIO", pRequestBio);
		
		X509_REQ* pRequest = X509_REQ_new();
		CPPUNIT_ASSERT_MESSAGE("Failed to create a new OpenSSL "
			"request object", pRequest);
		
		CPPUNIT_ASSERT_MESSAGE("Failed to set version in "
			"OpenSSL request object",
			X509_REQ_set_version(pRequest,0L));
		
		int commonNameNid = OBJ_txt2nid("commonName");
		CPPUNIT_ASSERT_MESSAGE("Failed to find node ID of "
			"X.509 CommonName attribute", commonNameNid != NID_undef);
		
		wxString certSubject = wxT("Backup system client root");
		X509_NAME* pX509SubjectName = X509_REQ_get_subject_name(pRequest);
		CPPUNIT_ASSERT_MESSAGE("X.509 subject name was null", 
			pX509SubjectName);

		unsigned char* subject = (unsigned char *)strdup(
			certSubject.mb_str(wxConvLibc).data());
		CPPUNIT_ASSERT_MESSAGE("Failed to add common name to "
			"certificate request",
			X509_NAME_add_entry_by_NID(pX509SubjectName, commonNameNid, 
				MBSTRING_ASC, subject, -1, -1, 0));
		free(subject);

		CPPUNIT_ASSERT_MESSAGE("Failed to set public key of "
			"certificate request", X509_REQ_set_pubkey(pRequest, pKey));
		
		X509V3_CTX ext_ctx;
		X509V3_set_ctx(&ext_ctx, NULL, NULL, pRequest, NULL, 0);
		X509V3_set_nconf(&ext_ctx, apConfig->GetConf());
		
		char *pReqExtensions = NCONF_get_string(apConfig->GetConf(), 
			"section", "req_extensions");
		if (pReqExtensions)
		{
			X509V3_CTX ctx;
			X509V3_set_ctx_test(&ctx);
			X509V3_set_nconf(&ctx, apConfig->GetConf());
			
			CPPUNIT_ASSERT_MESSAGE("Failed to load request extensions "
				"section", 
				X509V3_EXT_REQ_add_nconf(apConfig->GetConf(), 
					&ctx, pReqExtensions, pRequest));
		}
		else
		{
			ERR_clear_error();
		}

		const EVP_MD* pDigestAlgo = EVP_get_digestbyname("sha1");
		CPPUNIT_ASSERT_MESSAGE("Failed to find OpenSSL digest "
			"algorithm SHA1", pDigestAlgo);
		
		CPPUNIT_ASSERT_MESSAGE("Failed to sign certificate request",
			X509_REQ_sign(pRequest, pKey, pDigestAlgo));
		
		CPPUNIT_ASSERT_MESSAGE("Failed to verify signature on "
			"certificate request", X509_REQ_verify(pRequest, pKey));
		
		wxCharBuffer buf = clientCaCsrFileName.GetFullPath().mb_str(wxConvLibc);
		filename = strdup(buf.data());
		CPPUNIT_ASSERT_MESSAGE("Failed to set certificate request "
			"output filename", 
			BIO_write_filename(pRequestBio, filename) > 0);
		free(filename);
		
		CPPUNIT_ASSERT_MESSAGE("Failed to write certificate request file",
			PEM_write_bio_X509_REQ(pRequestBio, pRequest));
		
		X509_REQ_free(pRequest);
		pRequest = NULL;
		
		FreeKey(pKey);
		pKey = NULL;
		
		BIO_free(pRequestBio);
		pRequestBio = NULL;

		CPPUNIT_ASSERT(clientCaCsrFileName.FileExists());
		
		// sign our own request with our own private key
		// (self signed)

		wxFileName clientCaCertFileName(configTestDir.GetFullPath(), 
			wxT("client-ca-cert.pem"));
		CPPUNIT_ASSERT(!clientCaCertFileName.FileExists());

		{
			wxFileName dummy;
			SignRequestWithKey
			(
				clientCaCsrFileName, 
				clientCaKeyFileName,
				dummy, 
				clientCaCertFileName
			);
		}

		CPPUNIT_ASSERT(clientCaCertFileName.FileExists());
		
		// sign the client's request with our private key

		CPPUNIT_ASSERT(!clientCertFileName.FileExists());

		SignRequestWithKey(clientCsrFileName, clientCaKeyFileName,
			clientCaCertFileName, clientCertFileName);
		CPPUNIT_ASSERT(clientCertFileName.FileExists());
	
		// make a "bad signature" copy of the client's request,
		// signed with its own private key (mismatches the cert)

		CPPUNIT_ASSERT(!clientBadSigCertFileName.FileExists());

		SignRequestWithKey(clientCsrFileName, privateKeyFileName,
			clientCaCertFileName, clientBadSigCertFileName);
		CPPUNIT_ASSERT(clientBadSigCertFileName.FileExists());		

		// make a "self signed" copy of the client's request,
		// signed with its own certificate and private key

		CPPUNIT_ASSERT(!clientSelfSigCertFileName.FileExists());

		SignRequestWithKey(clientCsrFileName, privateKeyFileName,
			wxFileName(), clientSelfSigCertFileName);
		CPPUNIT_ASSERT(clientSelfSigCertFileName.FileExists());		
	}

	{
		CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());

		{
			wxString oldcafile;
			CPPUNIT_ASSERT(pConfig->TrustedCAsFile.GetInto(oldcafile));
			wxString expected(_("/etc/box/bbackupd/serverCA.pem"));
			CPPUNIT_ASSERT_EQUAL(expected, oldcafile);
			pConfig->TrustedCAsFile.Clear();
		}
		
		// clear both fields, try to go forward, check that it fails
		CPPUNIT_ASSERT(!pConfig->CertificateFile.IsConfigured());
		CPPUNIT_ASSERT(!pConfig->TrustedCAsFile.IsConfigured());
		CheckForwardError(BM_SETUP_WIZARD_NO_FILE_NAME);
		
		// set the certificate file to a name that doesn't exist
		wxFileName nonexistantfile(tempdir.GetFullPath(), 
			wxT("nonexistant"));
		pConfig->CertificateFile.Set(nonexistantfile.GetFullPath());
		
		// try to go forward, check that it fails
		CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_FOUND);

		// create another file, make it unreadable,
		// expect BM_SETUP_WIZARD_FILE_NOT_READABLE
		wxString anotherfilename = clientCertFileName.GetFullPath();
		anotherfilename.Append(wxT("2"));
		wxFile anotherfile;
		CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, false, 0));
		
		pConfig->CertificateFile.Set(anotherfilename);
		CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_READABLE);
		
		// make it readable, but not a valid certificate,
		// expect BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR
		CPPUNIT_ASSERT(wxRemoveFile(anotherfilename));
		CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, wxS_IRUSR));
		CheckForwardError(BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR);
		CPPUNIT_ASSERT(wxRemoveFile(anotherfilename));

		// set the CertificateFile to the real certificate,
		// expect complaints about the Trusted CAs File
		pConfig->CertificateFile.Set(clientCertFileName.GetFullPath());
		CPPUNIT_ASSERT(!pConfig->TrustedCAsFile.IsConfigured());
		CheckForwardError(BM_SETUP_WIZARD_NO_FILE_NAME);

		// same for the trusted CAs file
		// set the certificate file to a name that doesn't exist
		pConfig->TrustedCAsFile.Set(nonexistantfile.GetFullPath());
		
		// try to go forward, check that it fails
		CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_FOUND);

		// create another file, make it unreadable,
		// expect BM_SETUP_WIZARD_FILE_NOT_READABLE
		CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, false, 0));		
		pConfig->TrustedCAsFile.Set(anotherfilename);
		CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_READABLE);
		
		// make it readable, but not a valid certificate,
		// expect BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR
		CPPUNIT_ASSERT(wxRemoveFile(anotherfilename));
		CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, wxS_IRUSR));
		CheckForwardError(BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR);
		CPPUNIT_ASSERT(wxRemoveFile(anotherfilename));

		// set it to the real trusted CAs certificate,
		pConfig->TrustedCAsFile.Set(caFileName.GetFullPath());

		// set CertificateFile to a valid certificate file, 
		// but with a bad signature, expect BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR
		pConfig->CertificateFile.Set(clientBadSigCertFileName.GetFullPath());
		CheckForwardError(BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR);

		// set CertificateFile to a valid certificate file, 
		// but self signed, expect BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR
		// (key mismatch between the certs)
		pConfig->CertificateFile.Set(clientSelfSigCertFileName.GetFullPath());
		CheckForwardError(BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR);

		// set the CertificateFile to the real one,
		// expect that we can go forward
		pConfig->CertificateFile.Set(clientCertFileName.GetFullPath());
		CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
		ClickButtonWaitEvent(mpForwardButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());
	}

	// tests for the crypto keys page
	
	wxFileName clientCryptoFileName(configTestDir.GetFullPath(), 
		wxT("1-FileEncKeys.raw"));
	CPPUNIT_ASSERT(!clientCryptoFileName.FileExists());
	
	{
		CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());

		// ask for new crypto keys to be generated
		wxRadioButton* pNewFileRadioButton = 
			(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
				ID_Setup_Wizard_New_File_Radio);
		ClickRadioWaitEvent(pNewFileRadioButton);
		CPPUNIT_ASSERT(pNewFileRadioButton->GetValue());
		
		wxString tmp;
		
		CPPUNIT_ASSERT(pConfig->KeysFile.IsConfigured());
		CPPUNIT_ASSERT(pConfig->KeysFile.GetInto(tmp));
		CPPUNIT_ASSERT(tmp.IsSameAs(clientCryptoFileName.GetFullPath()));
		
		// go back, and check that the property is unconfigured for us
		CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());
		ClickButtonWaitEvent(pBackButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
		CPPUNIT_ASSERT(!pConfig->KeysFile.IsConfigured());
		
		// set the crypto key file manually, 
		// check that it is not overwritten.
		wxFileName dummyName(tempdir.GetFullPath(), wxT("nosuchfile"));
		pConfig->KeysFile.Set(dummyName.GetFullPath());
		
		CPPUNIT_ASSERT(pConfig->KeysFile.IsConfigured());
		CPPUNIT_ASSERT(pConfig->KeysFile.GetInto(tmp));
		CPPUNIT_ASSERT(tmp.IsSameAs(dummyName.GetFullPath()));
		
		CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
		ClickButtonWaitEvent(mpForwardButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());
		CPPUNIT_ASSERT(pConfig->KeysFile.GetInto(tmp));
		CPPUNIT_ASSERT(tmp.IsSameAs(dummyName.GetFullPath()));

		// clear the value again, check that the property is 
		// unconfigured
		wxTextCtrl* pText = GetTextCtrl(mpWizard->GetCurrentPage(), 
			ID_Setup_Wizard_File_Name_Text);
		CPPUNIT_ASSERT(pText);
		pText->SetValue(wxEmptyString);
		CPPUNIT_ASSERT(!pConfig->KeysFile.IsConfigured());
		
		// go back, forward again, check that the default 
		// value is inserted
		CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());

		ClickButtonWaitEvent(pBackButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
		
		ClickButtonWaitEvent(mpForwardButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());
		
		CPPUNIT_ASSERT(pConfig->KeysFile.IsConfigured());
		CPPUNIT_ASSERT(pConfig->KeysFile.GetInto(tmp));
		CPPUNIT_ASSERT(tmp.IsSameAs(clientCryptoFileName.GetFullPath()));
		
		// go forward, check that the file is created
		CPPUNIT_ASSERT(!clientCryptoFileName.FileExists());
		CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());

		ClickButtonWaitEvent(mpForwardButton);
		CPPUNIT_ASSERT_EQUAL(BWP_BACKED_UP, mpWizard->GetCurrentPageId());
		CPPUNIT_ASSERT(clientCryptoFileName.FileExists());

		// go back
		ClickButtonWaitEvent(pBackButton);
		CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());

		// choose "existing keys file"
		wxRadioButton* pExistingFileRadioButton = 
			(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
				ID_Setup_Wizard_Existing_File_Radio);
		CPPUNIT_ASSERT(pExistingFileRadioButton);
		ClickRadioWaitEvent(pExistingFileRadioButton);
		CPPUNIT_ASSERT(pExistingFileRadioButton->GetValue());

		// specify a newly created empty file, 
		// expect BM_SETUP_WIZARD_ENCRYPTION_KEY_FILE_NOT_VALID
		wxString anotherfilename = clientCryptoFileName.GetFullPath();
		anotherfilename.Append(wxT(".invalid"));
		pConfig->KeysFile.Set(anotherfilename);
		wxFile anotherfile;
		CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, wxS_IRUSR));
		CheckForwardError(BM_SETUP_WIZARD_ENCRYPTION_KEY_FILE_NOT_VALID);
		CPPUNIT_ASSERT(wxRemoveFile(anotherfilename));
		
		// check that our newly generated key is accepted.	
		pConfig->KeysFile.Set(clientCryptoFileName.GetFullPath());
		
		ClickButtonWaitEvent(mpForwardButton);
		CPPUNIT_ASSERT_EQUAL(BWP_BACKED_UP, mpWizard->GetCurrentPageId());
	}

	{
		CPPUNIT_ASSERT_EQUAL(BWP_BACKED_UP, mpWizard->GetCurrentPageId());
		
		wxCheckBox* pCheckBox = (wxCheckBox*) FindWindow
			(ID_Setup_Wizard_Backed_Up_Checkbox);
		CPPUNIT_ASSERT(pCheckBox);
		CPPUNIT_ASSERT(!pCheckBox->GetValue());
		
		CheckForwardError(BM_SETUP_WIZARD_MUST_CHECK_THE_BOX_KEYS_BACKED_UP);
		
		pCheckBox->SetValue(true);
		ClickButtonWaitEvent(mpForwardButton);

		CPPUNIT_ASSERT_EQUAL(0, WxGuiTestHelper::FlushEventQueue());
		CPPUNIT_ASSERT(!FindWindow(ID_Setup_Wizard_Frame));
	}	
	
	MessageBoxSetResponse(BM_MAIN_FRAME_CONFIG_CHANGED_ASK_TO_SAVE,
		wxNO);
	CloseWindowWaitClosed(pMainFrame);
	MessageBoxCheckFired();
}
