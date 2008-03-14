/***************************************************************************
 *            main.cc
 *
 *  Main source code file for Boxi, a graphical user interface for
 *  Box Backup (software developed by Ben Summers).
 *
 *  Copyright 2005-2008 Chris Wilson
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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Portions copied from swWxGuiTesting by Reinhold Fureder.
 * Used under the wxWidgets source license.
 *
 * Please note: This product includes software developed by Ben Summers.
 * The above attribution must not be removed or modified!
 */

#include <signal.h>

#include <wx/wx.h>
#include <wx/cmdline.h>
#include <wx/filename.h>
#include <wx/image.h>

#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/extensions/TestSetUp.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#include "main.h"
#include "TestFrame.h"
#include "MainFrame.h"
#include "WxGuiTestHelper.h"

// #ifdef HAVE_CONFIG_H
// #  include <config.h>
// #endif

#define NDEBUG
#include "SSLLib.h"
#undef NDEBUG

bool ViewDeleted = true;

void AddParam(wxPanel* panel, const wxChar* label, wxWindow* editor, 
	bool growToFit, wxSizer *sizer) 
{
	sizer->Add(new wxStaticText(panel, -1, wxString(label, wxConvLibc), 
		wxDefaultPosition), 0, wxALIGN_CENTER_VERTICAL);

	if (growToFit) 
	{
		sizer->Add(editor, 1, wxGROW);
	} 
	else 
	{
		sizer->Add(editor, 1, wxALIGN_CENTER_VERTICAL);
	}
}

static wxCmdLineEntryDesc theCmdLineParams[] = 
{
	{ wxCMD_LINE_SWITCH, wxT("c"), NULL, 
		wxT("ignored for compatibility with "
		    "boxbackup command-line tools"),
		wxCMD_LINE_VAL_NONE, 0 },
	{ wxCMD_LINE_PARAM, wxT(""), wxT(""), 
		wxT("<bbackupd-config-file>"),
		wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_SWITCH, wxT("t"), NULL, 
		wxT("run the unit tests"),
		wxCMD_LINE_VAL_NONE, 0 },
	{ wxCMD_LINE_NONE },
};

int    g_argc = 0;
char** g_argv = NULL;

// IMPLEMENT_APP(BoxiApp)
IMPLEMENT_APP_NO_MAIN(BoxiApp)

BEGIN_EVENT_TABLE(BoxiApp, wxApp)
    EVT_IDLE(BoxiApp::OnIdle)
END_EVENT_TABLE()

int main(int argc, char **argv)
{
	if (argc >= 2 && strcmp(argv[1], "-t") == 0)
	{
		g_argc = argc;
		g_argv = argv;

		#ifdef WIN32
		// Under win32 we must initialise the Winsock library
		// before using sockets

		WSADATA info;

		if (WSAStartup(0x0101, &info) == SOCKET_ERROR)
		{
			// will not run without sockets
			::fprintf(stderr, "Failed to initialise "
				"Windows Sockets");
			return 1;
		}
		#endif
		
		CppUnit::TestResult controller;
		
		CppUnit::TestResultCollector result;
		controller.addListener(&result);
		
		CppUnit::BriefTestProgressListener progress;
		controller.addListener(&progress);
		
		CppUnit::Test *suite = CppUnit::TestFactoryRegistry
			::getRegistry().makeTest();
		
		CppUnit::TestRunner runner;
		runner.addTest(suite);
		
		/*
		runner.setOutputter(new CppUnit::CompilerOutputter(
			&runner.result(), std::cerr));
		*/
		
		CppUnit::CompilerOutputter outputter(&result, std::cerr);
		
		runner.run(controller, "");
		
		outputter.write();

		#ifdef WIN32
		WSACleanup();
		#endif
		
		return result.wasSuccessful() ? 0 : 1;
	}
	else
	{
		return ::wxEntry(argc, argv);
	}
}

#ifndef WIN32
void sigpipe_handler(int signum) {
	signal(SIGPIPE, sigpipe_handler);
}
#endif

bool BoxiApp::OnInit()
{
	// Initialise global SSL system configuration/state
	SSLLib::Initialise();

	wxInitAllImageHandlers();
	
	wxCmdLineParser cmdParser(theCmdLineParams, argc, argv);
	int result = cmdParser.Parse();
	
	if (result != 0)
	{
		return false;
	}
	
	mHaveConfigFile = false;
	
	if (cmdParser.GetParamCount() == 1) 
	{
		mHaveConfigFile = true;
		mConfigFileName = cmdParser.GetParam();
	}

	#ifndef WIN32
	signal(SIGPIPE, sigpipe_handler);	
	#endif

	/*
	if (cmdParser.Found(wxT("t")))
	{
		TestFrame* pTestFrame = new TestFrame(wxString(argv[0]));
		pTestFrame->Show(TRUE);
	}
	else
	{
	*/
	
	if (cmdParser.Found(wxT("t")))
	{
		mTesting = true;
	}
	else
	{
		wxFrame *frame = new MainFrame 
		(
			mHaveConfigFile ? &mConfigFileName : NULL,
			wxString(argv[0]),
			wxPoint(50, 50), wxSize(600, 500)
		);
		frame->Show(TRUE);
	}
	
	/*
	}
	*/
	
  	return TRUE;
}

int BoxiApp::OnRun()
{
	// see the comment in wxApp::wxApp(): if the initial value 
	// hasn't been changed, use the default Yes from now on.
	if (m_exitOnFrameDelete == Later) 
	{
		m_exitOnFrameDelete = Yes;
	}
	
	WxGuiTestHelper::SetUseExitMainLoopOnIdleFlag(false);
	WxGuiTestHelper::SetExitMainLoopOnIdleFlag(false);

	if (mpTestRunner)
	{
		mpTestRunner->RunAsDecorator();
		
		if (wxTheApp->GetTopWindow()) 
		{
			wxTheApp->GetTopWindow()->Close();
		}
		
		// Finally process all pending events:
		WxGuiTestHelper::SetUseExitMainLoopOnIdleFlag(false);
	}
	
	// should do this, but it hangs on exit for no good reason
	if (!mpTestRunner)
	{
		return wxTheApp->MainLoop();
	}
	
	return 0;
}

void BoxiApp::OnIdle(wxIdleEvent& rEvent)
{
	this->wxApp::OnIdle(rEvent);
	
	if (WxGuiTestHelper::GetUseExitMainLoopOnIdleFlag () &&
		WxGuiTestHelper::GetExitMainLoopOnIdleFlag ()) 
	{
		wxTheApp->ExitMainLoop ();
	}
}

int BoxiApp::ShowFileDialog(wxFileDialog& rDialog)
{
	if (!mTesting)
	{
		return rDialog.ShowModal();
	}
	
	wxASSERT(mExpectingFileDialog);
	mExpectingFileDialog = false;
	rDialog.SetPath(mExpectedFileDialogResult);

	wxFileName fn(mExpectedFileDialogResult);
	rDialog.SetPath     (fn.GetPath());
	rDialog.SetDirectory(fn.GetPath());
	rDialog.SetFilename (fn.GetFullName());

	return wxID_OK;
}

void BoxiApp::ExpectFileDialog(const wxString& rPathToReturn)
{
	wxASSERT(mTesting);
	wxASSERT(!mExpectingFileDialog);
	mExpectingFileDialog = true;
	mExpectedFileDialogResult = rPathToReturn;
}

int BoxiApp::ShowMessageBox
(
	message_t messageId,
	const wxString& message,
	const wxString& caption,
	long style,
	wxWindow *parent
)
{
	if (!mTesting)
	{
		return wxMessageBox(message, caption, style, parent);
	}
	
	if (mExpectedMessageId != messageId)
	{
		wxString msg2 = StackWalker::AppendTo(message);
		wxCharBuffer buf = msg2.mb_str();
		CPPUNIT_ASSERT_EQUAL_MESSAGE(buf.data(), 
			mExpectedMessageId, messageId);
	}
	
	mExpectedMessageId = BM_UNKNOWN;

	int response = mExpectedMessageResponse;
	mExpectedMessageResponse = -1;

	return response;
}

void BoxiApp::ExpectMessageBox(message_t messageId, int response, 
	const CppUnit::SourceLine& rLine)
{
	CPPUNIT_ASSERT_MESSAGE_AT("There is already an expected message ID",
		mExpectedMessageId == 0, rLine);
	mExpectedMessageId       = messageId;
	mExpectedMessageResponse = response;
}

void BoxiApp::OnAssertFailure(const wxChar *file, int line,
	const wxChar *func, const wxChar *cond, const wxChar *msg)
{
	if (mTesting)
	{
		wxCharBuffer msgBuf  = wxString(msg).mb_str();
		wxCharBuffer condBuf = wxString(cond).mb_str();
		wxCharBuffer fileBuf = wxString(file).mb_str();
		CPPUNIT_ASSERT_MESSAGE_AT_FILE_LINE(msgBuf.data(), 
			condBuf.data(), fileBuf.data(), line);
	}
	else
	{
		wxApp::OnAssertFailure(file, line, func, cond, msg);
	}
}
