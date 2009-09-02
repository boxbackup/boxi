/***************************************************************************
 *            TestFrame.cc
 *
 *  Sun Jan 22 22:37:04 2006
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
 *
 *  Includes (small) portions of OpenSSL by Eric Young (eay@cryptsoft.com) and
 *  Tim Hudson (tjh@cryptsoft.com), under their source redistribution license.
 */

#include "SandBox.h"

// #include <wx/thread.h>
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/spinctrl.h>
#include <wx/treectrl.h>

#include <cppunit/SourceLine.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestSuite.h>
#include <cppunit/extensions/HelperMacros.h>

#define TLS_CLASS_IMPLEMENTATION_CPP

#include "Box.h"
#include "MainFrame.h"
#include "TestFrame.h"

/*
#include "SetupWizard.h"
#include "SslConfig.h"
#include "TestBackup.h"
#include "TestBackupConfig.h"
#include "TestCompare.h"
#include "TestRestore.h"
#include "TestConfig.h"
#include "TestWizard.h"
*/

#undef TLS_CLASS_IMPLEMENTATION_CPP

DECLARE_EVENT_TYPE(CREATE_WINDOW_COMMAND, -1)
DECLARE_EVENT_TYPE(TEST_FINISHED_EVENT,   -1)

DEFINE_EVENT_TYPE (CREATE_WINDOW_COMMAND)
DEFINE_EVENT_TYPE (TEST_FINISHED_EVENT)

bool TestFrame::mTestsInProgress = FALSE;

class SafeWindowWrapper
{
	private:
	wxWindow* mpWindow;
	
	public:
	SafeWindowWrapper(wxWindow* pWindow) : mpWindow(pWindow) { }
	wxWindowID GetId() { return mpWindow->GetId(); }
	void AddPendingEvent(wxEvent& rEvent) { mpWindow->AddPendingEvent(rEvent); }
};

/*
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

class TestCanRunThroughSetupWizardCreatingEverything : public TestCase
{
	public:
	TestCanRunThroughSetupWizardCreatingEverything(TestFrame* pTestFrame) 
	: TestCase(pTestFrame) { }

	protected:
	virtual void RunTest();	
};

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
*/

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

wxLogAsserter::wxLogAsserter()
: wxLog(), mpOldTarget(wxLog::GetActiveTarget()) 
{
	wxLog::SetActiveTarget(this);		
}

wxLogAsserter::~wxLogAsserter() { wxLog::SetActiveTarget(mpOldTarget); }

#if wxUSE_STACKWALKER
void StackWalker::OnStackFrame(const wxStackFrame& frame)
{
	if (frame.HasSourceLocation())
	{
		m_StackTrace.Append(frame.GetFileName());
		m_StackTrace.Append(_(":"));
		m_StackTrace << frame.GetLine();
	}

	if (frame.GetModule() != _(""))
	{
		m_StackTrace.Append(_("("));
		m_StackTrace.Append(frame.GetModule());
		m_StackTrace.Append(_(")"));
	}
	
	m_StackTrace.Append(_(" "));
	m_StackTrace.Append(frame.GetName());
	m_StackTrace.Append(_("\n"));
}

wxString StackWalker::GetStackTrace()
{ 
	StackWalker walker;
	walker.Walk();
	return walker.m_StackTrace;
}

wxString StackWalker::AppendTo(const wxString& rMessage)
{
	wxString newMessage = rMessage;
	newMessage.Append(_("\n"));
	newMessage.Append(GetStackTrace());
	return newMessage;
}
#endif // wxUSE_STACKWALKER

void wxLogAsserter::DoLog(wxLogLevel level, const wxChar *msg, time_t timestamp)
{
	wxString msgOut;
	
	switch(level)
	{
		case wxLOG_FatalError:
			msgOut = _("Fatal: "); break;
		case wxLOG_Error:
			msgOut = _("Error: "); break;
		case wxLOG_Warning:
			msgOut = _("Warning: "); break;
		case wxLOG_Message:
			msgOut = _("Message: "); break;
		case wxLOG_Status: 
			msgOut = _("Status: "); break;
		case wxLOG_Info:
			msgOut = _("Info: "); break;
		case wxLOG_Debug:
			msgOut = _("Debug: "); break;
		case wxLOG_Trace:
			msgOut = _("Trace: "); break;
		default:
			msgOut.Printf(_("Unknown (level %d): "), level);
	}

	msgOut.Append(msg);

	msgOut.Append(_("\n"));

	#if wxUSE_STACKWALKER
	msgOut.Append(StackWalker::GetStackTrace());
	#endif

	wxCharBuffer buf = msgOut.mb_str();
	CPPUNIT_ASSERT_MESSAGE(buf.data(), false);
}

GuiTestBase::GuiTestBase() 
: CppUnit::TestCase("GuiTestBase") { }

void GuiTestBase::setUp()
{
	mapAsserter.reset(new wxLogAsserter);
}

void GuiTestBase::tearDown()
{
	wxGetApp().ResetMessageBox();
	
	for (size_t i = 0; i < wxTopLevelWindows.GetCount(); i++)
	{
		wxWindow* pOpenWindow = 
			wxTopLevelWindows.Item(i)->GetData();
		CloseWindow(pOpenWindow);
	}

	try
	{	
		while (wxTopLevelWindows.GetCount() > 0)
		{
			CPPUNIT_ASSERT_EQUAL(0, WxGuiTestHelper::FlushEventQueue());
		}
	}
	catch (std::exception &e)
	{
		printf("Caught exception %s in GuiTestBase::tearDown\n",
			e.what());
	}
	catch (...)
	{
		printf("Caught unknown exception in "
			"GuiTestBase::tearDown\n");
	}

	mapAsserter.reset();
}	

void GuiStarter::run(CppUnit::TestResult *pResult)
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

void GuiStarter::RunAsDecorator()
{
	wxASSERT(mpResult != NULL);
	TestDecorator::run(mpResult);
}

void GuiStarter::setUp()
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

class GuiTestSuite : public CppUnit::TestFixture
{
	public:
	GuiTestSuite() { }
	static CppUnit::Test *suite()
	{
		CPPUNIT_NS::TestSuite *suite = new CPPUNIT_NS::TestSuite ();
	
		// Add all tests of specially named registry WxGuiTest
		/*
		CPPUNIT_NS::Test *wxGuiTestSuite = CppUnit::TestFactoryRegistry
			::getRegistry("WxGuiTest").makeTest ();
		suite->addTest(wxGuiTestSuite);
		*/

		#define ADD_TEST(name) \
		suite->addTest \
		( \
			new CppUnit::TestCaller<name> \
			( \
                       		#name, \
                       		&name::RunTest \
			) \
		)
	
		/*	
		ADD_TEST(TestWizard);
		ADD_TEST(TestBackupConfig);
		ADD_TEST(TestBackup);
		ADD_TEST(TestConfig);
		ADD_TEST(TestRestore);
		ADD_TEST(TestCompare);
		*/
		
		#undef ADD_TEST
	
		GuiStarter *starter = new GuiStarter(suite);
		return starter;
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(GuiTestSuite);

wxCommandEvent GuiTestBase::GetButtonClickEvent(wxWindow* pWindow)
{
	BOXI_ASSERT(pWindow);
	
	wxButton* pButton = wxDynamicCast(pWindow, wxButton);
	BOXI_ASSERT(pButton);
	BOXI_ASSERT(pButton->IsEnabled());

	wxCommandEvent click(wxEVT_COMMAND_BUTTON_CLICKED, pButton->GetId());
	click.SetEventObject(pButton);
	
	return click;
}

void GuiTestBase::ClickButtonWaitIdle(wxWindow* pWindow)
{
	wxCommandEvent click = GetButtonClickEvent(pWindow);
	pWindow->AddPendingEvent(click);
	BOXI_ASSERT(WxGuiTestHelper::FlushEventQueue() == 0);
}

void GuiTestBase::ClickButtonWaitEvent(wxWindowID ParentID, wxWindowID ButtonID)
{
	wxWindow* pParentWindow = wxWindow::FindWindowById(ParentID);
	BOXI_ASSERT(pParentWindow);

	wxButton* pButton = wxDynamicCast
	(
		pParentWindow->FindWindow(ButtonID), wxButton
	);
	BOXI_ASSERT(pButton);
	ClickButtonWaitEvent(pButton);
}

void GuiTestBase::ClickButtonWaitEvent(wxWindowID ButtonID)
{
	wxButton* pButton = wxDynamicCast
	(
		wxWindow::FindWindowById(ButtonID), wxButton
	);
	BOXI_ASSERT(pButton);
	ClickButtonWaitEvent(pButton);
}

void GuiTestBase::ClickButtonWaitEvent(wxButton* pButton)
{
	BOXI_ASSERT(pButton);	
	wxCommandEvent click = GetButtonClickEvent(pButton);
	pButton->ProcessEvent(click);
}

void GuiTestBase::ClickRadioWaitEvent(wxWindow* pWindow)
{
	BOXI_ASSERT(pWindow);
	
	wxRadioButton* pButton = wxDynamicCast(pWindow, wxRadioButton);
	BOXI_ASSERT(pButton);
	
	pButton->SetValue(true);

	wxCommandEvent click(wxEVT_COMMAND_RADIOBUTTON_SELECTED, 
		pButton->GetId());
	click.SetEventObject(pButton);
	click.SetInt(1);

	pWindow->ProcessEvent(click);
}

void GuiTestBase::ClickRadioWaitEvent(wxWindowID ButtonID)
{
	wxRadioButton* pButton = wxDynamicCast
	(
		wxWindow::FindWindowById(ButtonID), wxRadioButton
	);
	BOXI_ASSERT(pButton);
	ClickRadioWaitEvent(pButton);
}

void GuiTestBase::CloseWindow(wxWindow* pWindow)
{
	BOXI_ASSERT(pWindow);
	
	wxTopLevelWindow* pTopLevelWindow = 
		wxDynamicCast(pWindow, wxTopLevelWindow);
	BOXI_ASSERT(pTopLevelWindow);
	
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
		BOXI_ASSERT_EQUAL(0, WxGuiTestHelper::FlushEventQueue());
	}
	
	BOXI_ASSERT(!pWindow);	
}

void GuiTestBase::ActivateTreeItemWaitEvent(wxTreeCtrl* pTree, wxTreeItemId& rItem)
{
	BOXI_ASSERT(pTree);
	BOXI_ASSERT(rItem.IsOk());

	wxTreeEvent click(wxEVT_COMMAND_TREE_ITEM_ACTIVATED, 
		pTree->GetId());
	click.SetEventObject(pTree);
	click.SetItem(rItem);
	
	pTree->ProcessEvent(click);
}

void GuiTestBase::ExpandTreeItemWaitEvent(wxTreeCtrl* pTree, wxTreeItemId& rItem)
{
	BOXI_ASSERT(pTree);
	BOXI_ASSERT(rItem.IsOk());

	wxTreeEvent click(wxEVT_COMMAND_TREE_ITEM_ACTIVATED, pTree->GetId());
	click.SetEventObject(pTree);
	click.SetItem(rItem);
	
	pTree->ProcessEvent(click);
}

void GuiTestBase::CollapseTreeItemWaitEvent(wxTreeCtrl* pTree, wxTreeItemId& rItem)
{
	BOXI_ASSERT(pTree);
	BOXI_ASSERT(rItem.IsOk());

	wxTreeEvent click(wxEVT_COMMAND_TREE_ITEM_ACTIVATED, pTree->GetId());
	click.SetEventObject(pTree);
	click.SetItem(rItem);
	
	pTree->ProcessEvent(click);
}

void GuiTestBase::SetTextCtrlValue(wxTextCtrl* pTextCtrl, const wxString& rValue)
{
	BOXI_ASSERT(pTextCtrl);
	BOXI_ASSERT(pTextCtrl->IsEnabled());
	pTextCtrl->SetValue(rValue);
}

void GuiTestBase::SetValueAndDefocus(wxTextCtrl* pTextCtrl, const wxString& rValue)
{
	SetTextCtrlValue(pTextCtrl, rValue);
	wxFocusEvent focus(wxEVT_KILL_FOCUS);
	focus.SetEventObject(pTextCtrl);
	pTextCtrl->ProcessEvent(focus);
}

void GuiTestBase::SetValueDefocusCheck(wxTextCtrl* pTextCtrl, const wxString& rValue)
{
	SetValueAndDefocus(pTextCtrl, rValue);
	BOXI_ASSERT_EQUAL(rValue, pTextCtrl->GetValue());
}

void GuiTestBase::SetSpinCtrlValue(wxSpinCtrl* pSpinCtrl, int newValue)
{
	BOXI_ASSERT(pSpinCtrl);
	BOXI_ASSERT(pSpinCtrl->IsEnabled());
	pSpinCtrl->SetValue(newValue);
	
	wxSpinEvent spin(wxEVT_COMMAND_SPINCTRL_UPDATED, pSpinCtrl->GetId());
	spin.SetPosition(newValue);
	spin.SetEventObject(pSpinCtrl);
	pSpinCtrl->ProcessEvent(spin);
}

void GuiTestBase::SetDatePickerValue(wxDatePickerCtrl* pPicker, 
	const wxDateTime& rNewValue)
{
	BOXI_ASSERT(pPicker);
	BOXI_ASSERT(pPicker->IsEnabled());
	pPicker->SetValue(rNewValue);
	wxDateEvent pick(pPicker, rNewValue, wxEVT_DATE_CHANGED);
	pPicker->ProcessEvent(pick);	
}

void GuiTestBase::SetSelection(wxListBox* pListCtrl, int value)
{
	BOXI_ASSERT(pListCtrl);
	BOXI_ASSERT(pListCtrl->IsEnabled());
	pListCtrl->SetSelection(value);
	wxCommandEvent click(wxEVT_COMMAND_LISTBOX_SELECTED, pListCtrl->GetId());
	click.SetEventObject(pListCtrl);
	click.SetInt(value);
	pListCtrl->ProcessEvent(click);
}

void GuiTestBase::SetSelection(wxChoice* pChoiceCtrl, int value)
{
	BOXI_ASSERT(pChoiceCtrl);
	BOXI_ASSERT(pChoiceCtrl->IsEnabled());
	pChoiceCtrl->SetSelection(value);
	wxCommandEvent click(wxEVT_COMMAND_CHOICE_SELECTED, pChoiceCtrl->GetId());
	click.SetEventObject(pChoiceCtrl);
	click.SetInt(value);
	pChoiceCtrl->ProcessEvent(click);
}

void GuiTestBase::CheckBoxWaitEvent(wxCheckBox* pCheckBox, bool newValue)
{
	BOXI_ASSERT(pCheckBox);
	BOXI_ASSERT(pCheckBox->IsEnabled());
	BOXI_ASSERT(pCheckBox->GetValue() != newValue);
	pCheckBox->SetValue(newValue);
	wxCommandEvent click(wxEVT_COMMAND_CHECKBOX_CLICKED, pCheckBox->GetId());
	click.SetEventObject(pCheckBox);
	click.SetInt(newValue);
	pCheckBox->ProcessEvent(click);
	BOXI_ASSERT(pCheckBox->GetValue() == newValue);
}

MainFrame* GuiTestBase::GetMainFrame()
{
	MainFrame* pFrame = (MainFrame*)wxWindow::FindWindowById(ID_Main_Frame);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("There can be only one main frame at a time",
		(MainFrame*)NULL, pFrame);

	pFrame = new MainFrame 
	(
		NULL,
		wxString(g_argv[0], wxConvBoxi),
		wxPoint(50, 50), wxSize(600, 500)
	);
	pFrame->Show(TRUE);

	MainFrame* pFrame2 = (MainFrame*)wxWindow::FindWindowById(ID_Main_Frame);
	CPPUNIT_ASSERT_EQUAL(pFrame, pFrame2);
	
	return pFrame;
}

wxTextCtrl* GuiTestBase::GetTextCtrl(wxWindow* pWindow, wxWindowID id)
{
	BOXI_ASSERT(pWindow);
	BOXI_ASSERT(id);
	BOXI_ASSERT(id != wxID_ANY);
	
	wxWindow* pTextCtrlWindow = pWindow->FindWindow(id);
	BOXI_ASSERT(pTextCtrlWindow);
	
	wxTextCtrl *pTextCtrl = wxDynamicCast(pTextCtrlWindow, wxTextCtrl);
	BOXI_ASSERT(pTextCtrl);
	
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
