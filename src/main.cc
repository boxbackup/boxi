/*
 * Main source code file for Boxi, a graphical user interface for
 * Box Backup (software developed by Ben Summers).
 * 
 * (C) Chris Wilson <chris-boxisource@qwirx.com>, 2004-06
 * Licensed under the GNU General Public License (GPL) version 2 or later
 *
 * Please note: This product includes software developed by Ben Summers.
 * The above attribution must not be removed or modified!
 */

#include <signal.h>

#include <wx/wx.h>
#include <wx/cmdline.h>

#include "main.h"
#include "TestFrame.h"
#include "MainFrame.h"

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

class BoxiApp : public wxApp
{
  	public:
	virtual bool OnInit();
};

IMPLEMENT_APP(BoxiApp)

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

	if (cmdParser.Found(wxT("t")))
	{
		TestFrame* pTestFrame = new TestFrame(wxString(argv[0]));
		pTestFrame->Show(TRUE);
	}
	else
	{
		wxFrame *frame = new MainFrame 
		(
			haveConfigFileArg ? &ConfigFileName : NULL,
			wxString(argv[0]),
			wxPoint(50, 50), wxSize(600, 500)
		);
		frame->Show(TRUE);
	}
	
  	return TRUE;
}
