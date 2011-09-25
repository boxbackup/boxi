/***************************************************************************
 *            main.cc
 *
 *  Main source code file for Boxi, a graphical user interface for
 *  Box Backup (software developed by Ben Summers).
 *
 *  Copyright 2005-2011 Chris Wilson <chris-boxisource@qwirx.com>
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

#include "SandBox.h"

#include <signal.h>

#include <wx/wx.h>
#include <wx/cmdline.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <wx/log.h>

#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/extensions/TestSetUp.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#ifdef __WXMAC__
#include <ApplicationServices/ApplicationServices.h>
#endif

#include "main.h"
#include "MainFrame.h"
#include "TestFrame.h"
#include "TestFileDialog.h"
#include "WxGuiTestHelper.h"
#include "boxi.xpm"

#define FOR_ALL_TESTS(x) \
	x(TestWizard); \
	x(TestBackupConfig); \
	x(TestBackup); \
	x(TestConfig); \
	x(TestRestore); \
	x(TestCompare);

#include "TestWizard.h"
#include "TestBackupConfig.h"
#include "TestBackup.h"
#include "TestConfig.h"
#include "TestRestore.h"
#include "TestCompare.h"

#include "SSLLib.h"

bool ViewDeleted = true;

void AddParam(wxPanel* panel, const wxChar* label, wxWindow* editor, 
	bool growToFit, wxSizer *sizer) 
{
	sizer->Add(new wxStaticText(panel, -1, wxString(label, wxConvBoxi), 
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
	{ wxCMD_LINE_SWITCH, _("c"), NULL, 
		_("ignored for compatibility with "
		    "boxbackup command-line tools"),
		wxCMD_LINE_VAL_NONE, 0 },
	{ wxCMD_LINE_PARAM, wxT(""), wxT(""), 
		_("<bbackupd-config-file>"),
		wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, _("t"), _("test"), 
		_("run the specified unit test, or ALL"),
		wxCMD_LINE_VAL_STRING, 0 },
	{ wxCMD_LINE_OPTION, _("l"), _("lang"), 
		_("load the specified language or translation"),
		wxCMD_LINE_VAL_STRING, 0 },
	{ wxCMD_LINE_SWITCH, _("h"), _("help"), 
		_("displays this help text"),
		wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
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
	// ensure that parser/usage messages at this stage get sent to stderr
	wxMessageOutput* pNewMsg = new wxMessageOutputStderr();
	wxMessageOutput* pOldMsg = wxMessageOutput::Set(pNewMsg);
	if (pOldMsg != NULL)
	{
		delete pOldMsg;
	}
	// wxLogStderr logger;
	// wxLog::SetActiveTarget(&logger); 

	wxCmdLineParser cmdParser(theCmdLineParams, argc, argv);
	int result = cmdParser.Parse();

	if (result == -1)
	{
		return 0; // help requested and given
	}

	if (result != 0)
	{
		return 2; // invalid command line
	}

	// Copied from wxWidgets internat sample by Vadim Zeitlin/Julian Smart 
	wxLanguage lang = wxLANGUAGE_DEFAULT;
	wxString langName;
	const wxLanguageInfo * pLangInfo = NULL;

	if (cmdParser.Found(_("l"), &langName))
	{
		pLangInfo = wxLocale::FindLanguageInfo(langName);

		if (!pLangInfo)
		{
			wxLogFatalError(_("Locale \"%s\" is unknown."),
				langName.c_str());
			return 2; // invalid command line
		}
	
		lang = static_cast<wxLanguage>(pLangInfo->Language);
	}

	wxLocale locale;

	if (!locale.Init(lang, wxLOCALE_CONV_ENCODING))
	{
		wxLogWarning(_("Language is not supported by the system: "
			"%s (%d)"), langName.c_str(), lang);
		// continue nevertheless
	}

	// normally this wouldn't be necessary as the catalog files would be found
	// in the default locations, but when the program is not installed the
	// catalogs are in the build directory where we wouldn't find them by
	// default
	wxLocale::AddCatalogLookupPathPrefix(wxT("../po")); 

	// Initialize the catalogs we'll be using
	if (!locale.AddCatalog(locale.GetCanonicalName()))
	{
		wxLogError(_("Couldn't find/load the catalog for locale '%s'."),
			pLangInfo ? pLangInfo->CanonicalName.c_str() : _("unknown"));
	} 

	// Now try to add wxstd.mo so that loading nonexistent files will
	// produce a localized error message:
	locale.AddCatalog(wxT("wxstd"));
	// NOTE: it's not an error if we couldn't find it! 
	
	wxString testName;
	if (cmdParser.Found(_("t"), &testName))
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
	
		CppUnit::TestSuite *suite = new CppUnit::TestSuite("selected tests");

		bool foundNamedTest = false;
		wxString errorMsg;
		errorMsg.Printf(_("Test not found: %s. Available "
			"tests are: "), testName.c_str());

		#define ADD_IF_SELECTED(TestClass) \
		if (testName == _(#TestClass) || testName == _("all")) \
		{ \
			/* suite->addTest(new GuiStarter(new TestClass())); */ \
			/* suite->addTest(new GuiStarter( */ \
			suite->addTest(new CppUnit::TestCaller<TestClass> \
				( \
					#TestClass, \
					&TestClass::RunTest \
				)); \
			foundNamedTest = true; \
		} \
		errorMsg.Append(_(#TestClass)); \
		errorMsg.Append(_(", "));
		FOR_ALL_TESTS(ADD_IF_SELECTED);
		#undef ADD_IF_SELECTED

		errorMsg.Append(_("all."));
		if (!foundNamedTest)
		{
			wxLogFatalError(errorMsg);
			return 2;
		}
		
		CppUnit::TestRunner runner;
		runner.addTest(new GuiStarter(suite));
		
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

	#ifdef __WXMAC__
	// This lets a stdio app become a GUI app. Otherwise, you won't get
	// GUI events from the system and other things will fail to work.
	// Putting the app in an application bundle does the same thing.
	ProcessSerialNumber PSN;
	GetCurrentProcess(&PSN);
	TransformProcessType(&PSN,kProcessTransformToForegroundApplication);
	#endif

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

	if (cmdParser.Found(_("t")))
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
		frame->SetIcon(wxIcon(boxi_xpm));
//		Should work on resource file, but does not...
//		frame->SetIcon(wxICON(WXICON_AAA));
		frame->Show(TRUE);
	}
	
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

int BoxiApp::ShowFileDialog(TestFileDialog& rDialog)
{
	if (!mTesting)
	{
		return rDialog.ShowModal();
	}
	
	wxASSERT(mExpectingFileDialog);
	mExpectingFileDialog = false;

	if (mExpectingFileDialogPattern)
	{
		BOXI_ASSERT_EQUAL(mExpectedFileDialogPattern,
			rDialog.GetWildcard());
		mExpectingFileDialogPattern = false;
	}

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

void BoxiApp::ExpectFileDialog(const wxString& Pattern,
                const wxString& PathToReturn)
{
	ExpectFileDialog(PathToReturn);
	wxASSERT(!mExpectingFileDialogPattern);
	mExpectingFileDialogPattern = true;
	mExpectedFileDialogPattern = Pattern;
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
		#if wxUSE_STACKWALKER
		wxString msg2 = StackWalker::AppendTo(message);
		#else
		wxString msg2 = message;
		#endif

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

#if wxCHECK_VERSION(2,8,0)
void BoxiApp::OnAssertFailure(const wxChar *file, int line,
	const wxChar *func, const wxChar *cond, const wxChar *msg)
#else
void BoxiApp::OnAssert(const wxChar *file, int line,
	const wxChar *cond, const wxChar *msg)
#endif
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
		#ifdef __WXDEBUG__
			#if wxCHECK_VERSION(2,8,0)
			wxApp::OnAssertFailure(file, line, func, cond, msg);
			#else
			wxApp::OnAssert(file, line, cond, msg);
			#endif
		#endif // __WXDEBUG__
	}
}
