/*
 * Main source code file for Boxi, a graphical user interface for
 * Box Backup (software developed by Ben Summers).
 * 
 * (C) Chris Wilson <chris-boxisource@qwirx.com>, 2004-06
 * Licensed under the GNU General Public License (GPL) version 2 or later
 *
 * Please note: This product includes software developed by Ben Summers.
 * The above attribution must not be removed or modified!
 *
 * Portions copied from swWxGuiTesting by Reinhold Fureder.
 * Used under the wxWidgets source license.
 */

#include <signal.h>

#include <wx/wx.h>
#include <wx/cmdline.h>

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
		
		return result.wasSuccessful() ? 0 : 1;
	}
	else
	{
		return ::wxEntry(argc, argv);
	}
}

void sigpipe_handler(int signum) {
	signal(SIGPIPE, sigpipe_handler);
}

bool BoxiApp::OnInit()
{
	// Initialise global SSL system configuration/state
	SSLLib::Initialise();

	wxInitAllImageHandlers();
	
	wxCmdLineParser cmdParser(theCmdLineParams, argc, argv);
	int result = cmdParser.Parse();
	
	if (result != 0)
	{
		return FALSE;
	}
	
	bool haveConfigFileArg = FALSE;
	wxString ConfigFileName;
	
	if (cmdParser.GetParamCount() == 1) 
	{
		haveConfigFileArg = TRUE;
		ConfigFileName = cmdParser.GetParam();
	}

	signal(SIGPIPE, sigpipe_handler);	

	/*
	if (cmdParser.Found(wxT("t")))
	{
		TestFrame* pTestFrame = new TestFrame(wxString(argv[0]));
		pTestFrame->Show(TRUE);
	}
	else
	{
	*/
	
	if (!cmdParser.Found(wxT("t")))
	{
		wxFrame *frame = new MainFrame 
		(
			haveConfigFileArg ? &ConfigFileName : NULL,
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
	return wxTheApp->MainLoop();
	
	// return 0;
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
