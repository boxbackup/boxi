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
 */

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/thread.h>

#include "MainFrame.h"
#include "SetupWizard.h"
#include "TestFrame.h"

DECLARE_EVENT_TYPE(CREATE_WINDOW_COMMAND, -1)
DECLARE_EVENT_TYPE(TEST_FINISHED_EVENT,   -1)

DEFINE_EVENT_TYPE (CREATE_WINDOW_COMMAND)
DEFINE_EVENT_TYPE (TEST_FINISHED_EVENT)

bool TestFrame::mTestsInProgress = FALSE;

void MessageBoxCheckFired();

class TestCase : private wxThread, public wxEvtHandler
{
	private:
	TestFrame* mpTestFrame;
	
	void* Entry();
	virtual void RunTest() = 0;

	wxMutex        mMutex;
	wxCondition    mEventHandlerFinished;

	wxCommandEvent GetButtonClickEvent(wxWindow* pButton);
	void _MainDoClickButton(wxCommandEvent& rEvent);
	
	protected:
	MainFrame*  OpenMainFrame();
	wxTextCtrl* GetTextCtrl  (wxWindow* pWindow, wxWindowID id);
	TestFrame*  GetTestFrame () { return mpTestFrame; }
	bool WaitForMain();
	void CloseWindowWaitClosed(wxWindow* pWindow);
	void ClickButtonWaitIdle  (wxWindow* pButton);
	void ClickButtonWaitEvent (wxWindow* pButton);
	void ClickRadioWaitIdle   (wxWindow* pButton);
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
	
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(TestCase, wxEvtHandler)
	EVT_BUTTON(wxID_ANY, TestCase::_MainDoClickButton)
END_EVENT_TABLE()

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
	AddPendingEvent(click);
	wxMutexLocker lock(mMutex);
	mEventHandlerFinished.Wait();
}

void TestCase::_MainDoClickButton(wxCommandEvent& rEvent)
{
	wxObject* pObject = rEvent.GetEventObject();
	assert(pObject);
	
	wxButton* pButton = wxDynamicCast(pObject, wxButton);
	assert(pButton);

	pButton->ProcessEvent(rEvent);
	
	wxMutexLocker lock(mMutex);
	mEventHandlerFinished.Signal();
}
	
void TestCase::ClickRadioWaitIdle(wxWindow* pWindow)
{
	assert(pWindow);
	
	wxRadioButton* pButton = wxDynamicCast(pWindow, wxRadioButton);
	assert(pButton);

#ifdef __WXGTK__
	// no way to send an event to change the current value
	// this not thread safe and must be fixed - TODO FIXME
	pButton->SetValue(TRUE);
#else
	wxCommandEvent click(wxEVT_COMMAND_RADIOBUTTON_SELECTED, 
		pButton->GetId());
	click.SetEventObject(pButton);
	click.SetInt(1);
	
	pButton->AddPendingEvent(click);
	assert(WaitForMain());
#endif
	
	assert(pButton->GetValue());
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

void TestCanRunThroughSetupWizardCreatingEverything::RunTest()
{
	MainFrame* pMainFrame = OpenMainFrame();
	
	assert(!FindWindow(ID_Setup_Wizard_Frame));
	ClickButtonWaitIdle(pMainFrame->FindWindow(
		ID_General_Setup_Wizard_Button));
	
	SetupWizard* pWizard = (SetupWizard*)
		FindWindow(ID_Setup_Wizard_Frame);
	assert(pWizard);
	assert(pWizard->GetCurrentPageId() == BWP_INTRO);
	
	wxWindow* pForwardButton = pWizard->FindWindow(wxID_FORWARD);
	ClickButtonWaitEvent(pForwardButton);
	assert(pWizard->GetCurrentPageId() == BWP_ACCOUNT);
	
	wxTextCtrl* pStoreHost;
	wxTextCtrl* pAccountNo;
	{
		wxWizardPage* pPage = pWizard->GetCurrentPage();
		pStoreHost = GetTextCtrl(pPage, 
			ID_Setup_Wizard_Store_Hostname_Ctrl);
		pAccountNo = GetTextCtrl(pPage,
			ID_Setup_Wizard_Account_Number_Ctrl);
	}
	assert(pStoreHost->GetValue().IsSameAs(wxEmptyString));
	assert(pAccountNo->GetValue().IsSameAs(wxEmptyString));
	
	MessageBoxSetResponse(BM_SETUP_WIZARD_BAD_STORE_HOST, wxOK);
	ClickButtonWaitEvent(pForwardButton);
	MessageBoxCheckFired();
	assert(pWizard->GetCurrentPageId() == BWP_ACCOUNT);

	ClientConfig* pConfig = pMainFrame->GetConfig();
	assert(pConfig);
	assert(!pConfig->StoreHostname.IsConfigured());
	
	pStoreHost->SetValue(wxT("no-such-host"));
	MessageBoxSetResponse(BM_SETUP_WIZARD_BAD_STORE_HOST, wxOK);
	ClickButtonWaitEvent(pForwardButton);
	MessageBoxCheckFired();
	assert(pWizard->GetCurrentPageId() == BWP_ACCOUNT);
	assert(pConfig->StoreHostname.IsConfigured());
	
	pStoreHost->SetValue(wxT("localhost"));
	MessageBoxSetResponse(BM_SETUP_WIZARD_BAD_ACCOUNT_NO, wxOK);
	ClickButtonWaitEvent(pForwardButton);
	MessageBoxCheckFired();
	assert(pWizard->GetCurrentPageId() == BWP_ACCOUNT);
	
	pAccountNo->SetValue(wxT("localhost")); // invalid number
	MessageBoxSetResponse(BM_SETUP_WIZARD_BAD_ACCOUNT_NO, wxOK);
	ClickButtonWaitEvent(pForwardButton);
	MessageBoxCheckFired();
	assert(pWizard->GetCurrentPageId() == BWP_ACCOUNT);
	assert(!pConfig->AccountNumber.IsConfigured());
	
	pAccountNo->SetValue(wxT("-1"));
	MessageBoxSetResponse(BM_SETUP_WIZARD_BAD_ACCOUNT_NO, wxOK);
	ClickButtonWaitEvent(pForwardButton);
	MessageBoxCheckFired();
	assert(pConfig->AccountNumber.IsConfigured());

	pAccountNo->SetValue(wxT("1"));
	ClickButtonWaitEvent(pForwardButton);
	MessageBoxCheckFired();
	assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
	
	wxString StoredHostname;
	assert(pConfig->StoreHostname.GetInto(StoredHostname));
	assert(StoredHostname.IsSameAs(wxT("localhost")));
	
	int AccountNumber;
	assert(pConfig->AccountNumber.GetInto(AccountNumber));
	assert(AccountNumber == 1);
	
	{
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);

		ClickRadioWaitIdle(pWizard->GetCurrentPage()->FindWindow(
			ID_Setup_Wizard_New_File_Radio));
		MessageBoxSetResponse(BM_SETUP_WIZARD_NO_FILE_NAME, wxOK);
		ClickButtonWaitEvent(pForwardButton);
		MessageBoxCheckFired();
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
		
		wxFileName tempdir;
		tempdir.AssignTempFileName(wxT("boxi-tempdir-"));
		assert(wxRemoveFile(tempdir.GetFullPath()));
		
		wxTextCtrl* pFileName = GetTextCtrl(
			pWizard->GetCurrentPage(),
			ID_Setup_Wizard_File_Name_Text);
		assert(pFileName->GetValue().IsSameAs(wxEmptyString));
	
		wxFileName nonexistantfile(tempdir.GetFullPath(), 
			wxT("nonexistant"));
		pFileName->SetValue(nonexistantfile.GetFullPath());
	
		// filename refers to a path that doesn't exist
		// expect BM_SETUP_WIZARD_FILE_DIR_NOT_FOUND
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_DIR_NOT_FOUND, wxOK);
		ClickButtonWaitEvent(pForwardButton);
		MessageBoxCheckFired();
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
	
		// create the directory, but not the file
		// make the directory not writable
		// expect BM_SETUP_WIZARD_FILE_DIR_NOT_WRITABLE
		assert(tempdir.Mkdir(wxS_IRUSR | wxS_IXUSR));
		assert(tempdir.DirExists());
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_DIR_NOT_WRITABLE, wxOK);
		ClickButtonWaitEvent(pForwardButton);
		MessageBoxCheckFired();
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
		
		// change the filename to refer to the directory
		// expect BM_SETUP_WIZARD_FILE_IS_A_DIRECTORY
		pFileName->SetValue(tempdir.GetFullPath());
		assert(pFileName->GetValue().IsSameAs(tempdir.GetFullPath()));
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_IS_A_DIRECTORY, wxOK);
		ClickButtonWaitEvent(pForwardButton);
		MessageBoxCheckFired();
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
		
		// make the directory read write
		assert(tempdir.Rmdir());
		assert(tempdir.Mkdir(wxS_IRUSR | wxS_IWUSR | wxS_IXUSR));
		
		// create a new file in the directory, make it read only, 
		// and change the filename to refer to it. 
		// expect BM_SETUP_WIZARD_FILE_OVERWRITE
		wxFileName existingfile(tempdir.GetFullPath(), wxT("existing"));
		wxString   existingpath = existingfile.GetFullPath();
		wxFile     existing;
		assert(existing.Create(existingpath, false, wxS_IRUSR));
		assert(  wxFile::Access(existingpath, wxFile::read));
		assert(! wxFile::Access(existingpath, wxFile::write));
		pFileName->SetValue(existingpath);
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_NOT_WRITABLE, wxOK);
		ClickButtonWaitEvent(pForwardButton);
		MessageBoxCheckFired();
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
	
		// make the file writable, expect BM_SETUP_WIZARD_FILE_OVERWRITE
		// reply No and check that we stay put
		assert(wxRemoveFile(existingpath));
		assert(existing.Create(existingpath, false, 
			wxS_IRUSR | wxS_IWUSR));
		assert(wxFile::Access(existingpath, wxFile::read));
		assert(wxFile::Access(existingpath, wxFile::write));
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_OVERWRITE, wxNO);
		ClickButtonWaitEvent(pForwardButton);
		MessageBoxCheckFired();
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
		
		// now try again, reply Yes, and check that we move forward
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_OVERWRITE, wxYES);
		ClickButtonWaitEvent(pForwardButton);
		MessageBoxCheckFired();
		assert(pWizard->GetCurrentPageId() == BWP_CERT_EXISTS);

		// was the configuration updated?
		wxString keyfile;
		assert(pConfig->PrivateKeyFile.GetInto(keyfile));
		assert(keyfile.IsSameAs(existingpath));

		// go back, select "existing", choose a nonexistant file,
		// expect BM_SETUP_WIZARD_FILE_NOT_FOUND
		wxWindow* pBackButton = pWizard->FindWindow(wxID_BACKWARD);
		ClickButtonWaitEvent(pBackButton);
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
		ClickRadioWaitIdle(pWizard->GetCurrentPage()->FindWindow(
			ID_Setup_Wizard_Existing_File_Radio));
		assert(pFileName->GetValue().IsSameAs(existingpath));
		pFileName->SetValue(nonexistantfile.GetFullPath());
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_NOT_FOUND, wxOK);
		ClickButtonWaitEvent(pForwardButton);
		MessageBoxCheckFired();
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
		
		// set the path to a bogus path, 
		// expect BM_SETUP_WIZARD_FILE_NOT_FOUND
		wxString boguspath = nonexistantfile.GetFullPath();
		boguspath.Append(wxT("/foo/bar"));
		pFileName->SetValue(boguspath);
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_NOT_FOUND, wxOK);
		ClickButtonWaitEvent(pForwardButton);
		MessageBoxCheckFired();
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
		
		// create another file, make it unreadable,
		// expect BM_SETUP_WIZARD_FILE_NOT_READABLE
		wxString anotherfilename = existingpath;
		anotherfilename.Append(wxT("2"));
		wxFile anotherfile;
		assert(anotherfile.Create(anotherfilename, false, 0));
		pFileName->SetValue(anotherfilename);
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_NOT_READABLE, wxOK);
		ClickButtonWaitEvent(pForwardButton);
		MessageBoxCheckFired();
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
		
		// make it readable, but not a valid key,
		// expect BM_SETUP_WIZARD_BAD_PRIVATE_KEY
		assert(wxRemoveFile(anotherfilename));
		assert(anotherfile.Create(anotherfilename, wxS_IRUSR));
		MessageBoxSetResponse(BM_SETUP_WIZARD_BAD_PRIVATE_KEY, wxOK);
		ClickButtonWaitEvent(pForwardButton);
		MessageBoxCheckFired();
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
		
		// delete that file, set the filename back to the
		// key file we just created, expect that the certificate
		// checks out okay, and we move on to the next page
		assert(wxRemoveFile(anotherfilename));
		pFileName->SetValue(existingpath);
		ClickButtonWaitEvent(pForwardButton);
		assert(pWizard->GetCurrentPageId() == BWP_CERT_EXISTS);
		
		// was the configuration updated?
		assert(pConfig->PrivateKeyFile.GetInto(keyfile));
		assert(keyfile.IsSameAs(existingpath));
	}

	{
		assert(pWizard->GetCurrentPageId() == BWP_CERT_EXISTS);

		// ask for a new certificate request to be generated
		ClickRadioWaitIdle(pWizard->GetCurrentPage()->FindWindow(
			ID_Setup_Wizard_New_File_Radio));
		ClickButtonWaitEvent(pForwardButton);
		assert(pWizard->GetCurrentPageId() == BWP_CERT_REQUEST);
	}

	/*
	{
		assert(pWizard->GetCurrentPageId() == BWP_CERT_REQUEST);

		// ask for a new certificate request to be generated
		ClickRadioWaitIdle(pWizard->GetCurrentPage()->FindWindow(
			ID_Setup_Wizard_New_File_Radio));
		ClickButtonWaitEvent(pForwardButton);
		assert(pWizard->GetCurrentPageId() == BWP_CERT_REQUEST);
	}
	*/

	CloseWindowWaitClosed(pWizard);
	
	MessageBoxSetResponse(BM_MAIN_FRAME_CONFIG_CHANGED_ASK_TO_SAVE,
		wxNO);
	CloseWindowWaitClosed(pMainFrame);
	MessageBoxCheckFired();
}

BEGIN_EVENT_TABLE(TestFrame, wxFrame)
	EVT_IDLE (TestFrame::OnIdle)
	EVT_COMMAND(wxID_ANY, CREATE_WINDOW_COMMAND, 
		TestFrame::OnCreateWindowCommand)
	EVT_COMMAND(wxID_ANY, TEST_FINISHED_EVENT, 
		TestFrame::OnTestFinishedEvent)
END_EVENT_TABLE()

TestFrame::TestFrame(const wxString PathToExecutable)
:	wxFrame(NULL, wxID_ANY, wxT("Boxi (test mode)")),
	mPathToExecutable(PathToExecutable),
	mTestThreadIsWaiting(false),
	mMainThreadIsIdle(mMutex),
	mpMainFrame(NULL)
{
	mTests.push_back(new TestCanOpenAndCloseMainWindow(this));
	mTests.push_back(new TestCanOpenAndCloseSetupWizard(this));
	mTests.push_back(new TestCanRunThroughSetupWizardCreatingEverything(this));
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

	mMainThreadIsIdle.Wait();
	
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
	
	assert (messageId == expMessageId);
	int response = expMessageResp;
	
	expMessageId   = BM_UNKNOWN;
	expMessageResp = -1;
	
	return response;
}

void MessageBoxSetResponse(message_t messageId, int response)
{
	assert(expMessageId == 0);
	expMessageId   = messageId;
	expMessageResp = response;
}

void MessageBoxCheckFired()
{
	assert(expMessageId == BM_UNKNOWN);
}
