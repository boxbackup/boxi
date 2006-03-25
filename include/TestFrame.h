/***************************************************************************
 *            TestFrame.h
 *
 *  Sun Jan 22 22:35:37 2006
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
 
#ifndef _TESTFRAME_H
#define _TESTFRAME_H

#include <wx/wx.h>

class MainFrame;

class TestCase;

class TestFrame : public wxFrame
{
	public:
	TestFrame(const wxString PathToExecutable);
	void WaitForIdle();
	void TestThreadFinished();
	// wxMutex& GetTestMutex()  { return mMutex; }
	MainFrame* GetMainFrame()   { return mpMainFrame; }
	static bool IsTestsInProgress() 
	{ 
		return mTestsInProgress; 
	}
	
	private:
	wxString       mPathToExecutable;
	wxMutex        mMutex;
	static bool    mTestsInProgress;
	bool           mTestThreadIsWaiting;
	wxCondition    mMainThreadIsIdle;
	MainFrame*     mpMainFrame;
	
	typedef std::vector<TestCase*> tTestList;
	tTestList      mTests;
	tTestList::iterator mipCurrentTest;
	
	void OnIdle(wxIdleEvent& rEvent);
	void OnCreateWindowCommand(wxCommandEvent& rEvent);
	void OnTestFinishedEvent  (wxCommandEvent& rEvent);
	
	DECLARE_EVENT_TABLE()
};

typedef enum
{
	BM_UNKNOWN = 0,
	BM_SETUP_WIZARD_BAD_ACCOUNT_NO,
	BM_SETUP_WIZARD_BAD_STORE_HOST,
	BM_SETUP_WIZARD_NO_FILE_NAME,
	BM_SETUP_WIZARD_FILE_NOT_FOUND,
	BM_SETUP_WIZARD_FILE_IS_A_DIRECTORY,
	BM_SETUP_WIZARD_FILE_NOT_READABLE,
	BM_SETUP_WIZARD_FILE_OVERWRITE,
	BM_SETUP_WIZARD_FILE_NOT_WRITABLE,
	BM_SETUP_WIZARD_FILE_DIR_NOT_FOUND,
	BM_SETUP_WIZARD_FILE_DIR_NOT_WRITABLE,
	BM_SETUP_WIZARD_BAD_PRIVATE_KEY,
	BM_SETUP_WIZARD_OPENSSL_ERROR,
	BM_SETUP_WIZARD_RANDOM_WARNING,
	BM_SETUP_WIZARD_PRIVATE_KEY_FILE_NOT_SET,
	BM_SETUP_WIZARD_ERROR_LOADING_PRIVATE_KEY,
	BM_SETUP_WIZARD_ERROR_READING_COMMON_NAME,
	BM_SETUP_WIZARD_WRONG_COMMON_NAME,
	BM_SETUP_WIZARD_FAILED_VERIFY_SIGNATURE,
	BM_SETUP_WIZARD_ERROR_LOADING_OPENSSL_CONFIG,
	BM_SETUP_WIZARD_ERROR_SETTING_STRING_MASK,
	BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR,
	BM_SETUP_WIZARD_ENCRYPTION_KEY_FILE_NOT_VALID,
	BM_SETUP_WIZARD_MUST_CHECK_THE_BOX_KEYS_BACKED_UP,
	BM_MAIN_FRAME_CONFIG_CHANGED_ASK_TO_SAVE,
} 
message_t;

int MessageBoxHarness
(
	message_t messageId,
	const wxString& message,
	const wxString& caption = wxMessageBoxCaptionStr,
	long style = wxOK | wxCENTRE,
	wxWindow *parent = NULL
);

void MessageBoxSetResponse(message_t messageId, int response);

#endif /* _TESTFRAME_H */
