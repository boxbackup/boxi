/*
 * Main source code file for Boxi, a graphical user interface for
 * Box Backup (software developed by Ben Summers).
 * 
 * (C) Chris Wilson <boxi_main.cc@qwirx.com>, 2004-05
 * Licensed under the GNU General Public License (GPL) version 2 or later
 *
 * Please note: This product includes software developed by Ben Summers.
 * The above attribution must not be removed or modified!
 */

#include <signal.h>

#include "main.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/cmdline.h>

#include "BackupDaemonPanel.h"
#include "BackupFilesPanel.h"
#include "BackupLocationsPanel.h"
#include "ClientInfoPanel.h"
#include "ServerFilesPanel.h"

bool ViewDeleted = true;

class MainFrame : public wxFrame, public ConfigChangeListener {
	public:
	MainFrame(
		const wxString* pConfigFileName,
		const wxString& rBoxiExecutablePath,
		const wxPoint& pos, const wxSize& size, 
		long style = wxDEFAULT_FRAME_STYLE);

	private:
	void OnFileNew	  (wxCommandEvent& event);
	void OnFileOpen	  (wxCommandEvent& event);
	void OnFileSave	  (wxCommandEvent& event);
	void OnFileSaveAs (wxCommandEvent& event);
	void OnFileQuit	  (wxCommandEvent& event);
	void OnFileDir	  (wxCommandEvent& event);
	void OnViewOld    (wxCommandEvent& event);
	void OnViewDeleted(wxCommandEvent& event);
	void OnHelpAbout  (wxCommandEvent& event);
	void OnSize		  (wxSizeEvent&	   event);
	void OnClose      (wxCloseEvent&   event);

	void DoFileOpen   (const wxString& path);
	void DoFileNew    ();
	void DoFileSave   ();
	void DoFileSaveAs ();
	void DoFileSaveAs2();
	
	// implement ConfigChangeListener
	void NotifyChange();
	void UpdateTitle();
	
	wxStatusBar*        mpStatusBar;
	wxString            mConfigFileName;
	ClientConfig*       mpConfig;
	ServerConnection*   mpServerConnection;
	
	wxNotebook*         theTopNotebook;
	BackupFilesPanel*   theBackupFilesPanel;
	ClientInfoPanel*    theClientPanel;
	wxPanel*            theLocationsPanel;
	ServerFilesPanel*   theServerFilesPanel;
	wxMenu*             mpViewMenu;
	
	DECLARE_EVENT_TABLE()
};	

BEGIN_EVENT_TABLE(MainFrame, wxFrame)
	EVT_MENU(ID_File_New,     MainFrame::OnFileNew)
	EVT_MENU(ID_File_Open,    MainFrame::OnFileOpen)
	EVT_MENU(ID_File_Save,    MainFrame::OnFileSave)
	EVT_MENU(ID_File_SaveAs,  MainFrame::OnFileSaveAs)
	EVT_MENU(wxID_EXIT,       MainFrame::OnFileQuit)
	EVT_MENU(ID_View_Old,     MainFrame::OnViewOld)
	EVT_MENU(ID_View_Deleted, MainFrame::OnViewDeleted)
	EVT_MENU(wxID_ABOUT,      MainFrame::OnHelpAbout)
	EVT_SIZE(MainFrame::OnSize)
	EVT_CLOSE(MainFrame::OnClose)
END_EVENT_TABLE()

void AddParam(wxPanel* panel, const char* label, wxWindow* editor, 
	bool growToFit, wxSizer *sizer) 
{
	sizer->Add(new wxStaticText(panel, -1, wxString(label), 
		wxDefaultPosition), 0, wxALIGN_CENTER_VERTICAL);

	if (growToFit) {
		sizer->Add(editor, 1, wxGROW);
	} else {
		sizer->Add(editor, 1, wxALIGN_CENTER_VERTICAL);
	}
}

MainFrame::MainFrame(
	const wxString* pConfigFileName,
	const wxString& rBoxiExecutablePath,
	const wxPoint& pos, const wxSize& size, long style)
	: wxFrame(NULL, -1, "Boxi", pos, size, style)
{
	// Initialise global SSL system configuration/state
	SSLLib::Initialise();

	mpConfig = new ClientConfig();

	mpConfig->AddListener(this);

	mpServerConnection = new ServerConnection(mpConfig);
	mpStatusBar = CreateStatusBar(1);
	
	theTopNotebook = new wxNotebook(this, ID_Top_Notebook);

	theClientPanel = new ClientInfoPanel(
		mpConfig,
		theTopNotebook, 
		ID_Client_Panel);
	theTopNotebook->AddPage(theClientPanel, "Settings");

	wxPanel* pBackupDaemonPanel = new BackupDaemonPanel(
		mpConfig,
		rBoxiExecutablePath,
		theTopNotebook,
		-1);
	theTopNotebook->AddPage(pBackupDaemonPanel, "Backup Process");
	
	theLocationsPanel = new BackupLocationsPanel(
		mpConfig,
		theTopNotebook, 
		ID_Client_Panel);
	theTopNotebook->AddPage(theLocationsPanel, "Backup");

	/*
	theBackupFilesPanel = new BackupFilesPanel(
		mpConfig,
		mpServerConnection,
		theTopNotebook, 
		ID_Backup_Files_Panel);
	theTopNotebook->AddPage(theBackupFilesPanel, "Local Files");
	*/
	
	theServerFilesPanel = new ServerFilesPanel(
		mpConfig,
		mpServerConnection, 
		theTopNotebook, 
		mpStatusBar, 
		ID_Backup_Files_Panel);
	theTopNotebook->AddPage(theServerFilesPanel, "Restore");
	
	wxMenu *menuFile = new wxMenu;
	menuFile->Append(ID_File_New,  "&New...\tCtrl-N", 
		"New client config file");
	menuFile->Append(ID_File_Open,  "&Open...\tCtrl-O", 
		"Open client config file");
	menuFile->Append(ID_File_Save,  "&Save...\tCtrl-S", 
		"Save current configuration");
	menuFile->Append(ID_File_SaveAs,  "Save &As...\tCtrl-A", 
		"Save current config file with a new name");
	menuFile->Append(wxID_EXIT,  "E&xit\tAlt-F4", 
		"Quit Boxi");

	mpViewMenu = new wxMenu;
	mpViewMenu->Append(ID_View_Old, "Show &Old Files\tCtrl-L", 
		"Show old versions of files", wxITEM_CHECK);
	mpViewMenu->Append(ID_View_Deleted, "Show &Deleted Files\tCtrl-D", 
		"Show deleted files", wxITEM_CHECK);
	mpViewMenu->Check(ID_View_Old,     TRUE);
	mpViewMenu->Check(ID_View_Deleted, TRUE);

	wxMenu *menuHelp = new wxMenu;
	menuHelp->Append(wxID_ABOUT, "&About...\tF1", 
		"Show information about Boxi");

	wxMenuBar *menuBar = new wxMenuBar;
	menuBar->Append(menuFile,   "&File");
	menuBar->Append(mpViewMenu, "&View");
	menuBar->Append(menuHelp,   "&Help");
	SetMenuBar(menuBar);

	if (pConfigFileName) {
		DoFileOpen(*pConfigFileName);
	} else {
		DoFileNew();
	}
}

void MainFrame::OnFileNew(wxCommandEvent& event) {
	DoFileNew();
}

void MainFrame::DoFileNew() 
{
	mConfigFileName = "";
	UpdateTitle();
}

static const
 wxChar
  *FILETYPES = _T( "Box Backup client configuration file|bbackupd.conf|"
                   "Configuration files|*.conf|"
                   "All files|*.*"
                  );

void MainFrame::OnFileOpen(wxCommandEvent& event) {
	wxFileDialog *openFileDialog = new wxFileDialog(
		this, "Open file", "", "bbackupd.conf", 
		FILETYPES, wxOPEN | wxFILE_MUST_EXIST, 
		wxDefaultPosition);

	if (openFileDialog->ShowModal() != wxID_OK)
		return;

	DoFileOpen(openFileDialog->GetPath());
}

void MainFrame::DoFileOpen(const wxString& path)
{
	mpConfig->RemoveListener(this);
	
	try {
		mpConfig->Load(path);
	} catch (BoxException &e) {
		mpConfig->AddListener(this);
		wxString str;
		str.Printf("Failed to load configuration file (%s)", 
			path.c_str());
		wxMessageBox(str, "Boxi Error", wxOK | wxICON_ERROR, NULL);
		return;
	} catch (wxString* e) {
		mpConfig->AddListener(this);
		wxString str;
		str.Printf("Failed to load configuration file (%s):\n\n%s", 
			path.c_str(), e->c_str());
		wxMessageBox(str, "Boxi Error", wxOK | wxICON_ERROR, NULL);
		return;
	}		
	
	theClientPanel->Reload();

	mConfigFileName = path;
	UpdateTitle();
	mpConfig->AddListener(this);
}

void MainFrame::OnFileSave(wxCommandEvent& event) {
	DoFileSave();
}

void MainFrame::OnFileSaveAs(wxCommandEvent& event) {
	DoFileSaveAs();
}

void MainFrame::DoFileSave() 
{
	wxString msg;
	if (!(mpConfig->Check(msg)))
	{
		int result = wxMessageBox(
			"The configuration file has errors.\n"
			"Are you sure you want to save it?",
			"Boxi Warning", wxYES_NO | wxICON_QUESTION);
		
		if (result == wxNO)
			return;
	}

	if (mConfigFileName.Length() == 0) 
	{
		DoFileSaveAs2();
	}
	else
	{
		mpConfig->Save();
	}
}

void MainFrame::DoFileSaveAs() 
{
	wxString msg;
	if (!(mpConfig->Check(msg)))
	{
		int result = wxMessageBox(
			"The configuration file has errors.\n"
			"Are you sure you want to save it?",
			"Boxi Warning", wxYES_NO | wxICON_QUESTION);
		
		if (result == wxNO)
			return;
	}

	DoFileSaveAs2();
}

void MainFrame::DoFileSaveAs2() 
{
	wxFileDialog *saveFileDialog = new wxFileDialog(
		this, "Save file", "", "bbackupd.conf", 
		FILETYPES, wxSAVE | wxOVERWRITE_PROMPT, 
		wxDefaultPosition);

	if (saveFileDialog->ShowModal() != wxID_OK)
		return;

	mConfigFileName = saveFileDialog->GetPath();
	mpConfig->Save(mConfigFileName);
}

void MainFrame::OnFileQuit(wxCommandEvent& event) {
	Close();
}

void MainFrame::OnClose(wxCloseEvent& event) {
	if (event.CanVeto() && !(mpConfig->IsClean()))
	{
		int result = wxMessageBox(
			"The configuration file has not been saved.\n"
			"Do you want to save your changes?",
			"Boxi Warning", wxYES_NO | wxCANCEL | wxICON_QUESTION);
		
		if (result == wxCANCEL)
		{
			event.Veto();
			return;
		}
		
		if (result == wxYES)
		{
			DoFileSave();
			if (!(mpConfig->IsClean()))
			{
				event.Veto();
				return;
			}
		}
	}
	
	event.Skip();
}

void MainFrame::OnViewOld(wxCommandEvent& event) {
	theServerFilesPanel->SetViewOldFlag(
		mpViewMenu->IsChecked(ID_View_Old));
}

void MainFrame::OnViewDeleted(wxCommandEvent& event) {
	theServerFilesPanel->SetViewDeletedFlag(
		mpViewMenu->IsChecked(ID_View_Deleted));
}

void MainFrame::OnFileDir(wxCommandEvent& event) {
	wxDirDialog *d = new wxDirDialog(
		this, "Choose a Directory", wxGetCwd(), 0, wxDefaultPosition);

	if (d->ShowModal() == wxID_OK)
		wxSetWorkingDirectory(d->GetPath());
}

void MainFrame::OnHelpAbout(wxCommandEvent& event) {
	wxMessageBox("Boxi " BOXI_VERSION " by Chris Wilson\n"
		"A Graphical User Interface for Box Backup\n"
		"Home Page: http://www.qwirx.com/boxi/\n"
		"\n"
		"Licensed under the GNU General Public License (GPL)\n"
		"This product includes software developed by Ben Summers.",
		"About Boxi", wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnSize(wxSizeEvent& event) 
{
	wxString theSizeText;
	wxSize theNewSize = event.GetSize();
	theSizeText.Printf("%d x %d", theNewSize.GetWidth(), 
		theNewSize.GetHeight());
	// theText->SetValue(theSizeText);
	wxFrame::OnSize(event);
}

static wxCmdLineEntryDesc theCmdLineParams[] = 
{
	{ wxCMD_LINE_SWITCH, "c", "", 
		"ignored for compatibility with boxbackup command-line tools",
		wxCMD_LINE_VAL_NONE, 0 },
	{ wxCMD_LINE_PARAM, "", "", "<bbackupd-config-file>",
		wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
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
	wxCmdLineParser cmdParser(theCmdLineParams, argc, argv);
	int result = cmdParser.Parse();
	
	if (result != 0)
		return FALSE;
	
	bool haveConfigFileArg = FALSE;
	wxString ConfigFileName;
	
	if (cmdParser.GetParamCount() == 1) {
		haveConfigFileArg = TRUE;
		ConfigFileName = cmdParser.GetParam();
	}
	
  	wxFrame *frame = new MainFrame (
		haveConfigFileArg ? &ConfigFileName : NULL,
		wxString(argv[0]),
		wxPoint(50, 50), wxSize(600, 500)
	);
	
  	frame->Show(TRUE);
	
	signal(SIGPIPE, sigpipe_handler);
	
  	return TRUE;
}

void MainFrame::NotifyChange() {
	UpdateTitle();
}

void MainFrame::UpdateTitle() {
	wxString title;
	if (mConfigFileName.Length() == 0) 
	{
		title.Printf("Boxi %s - [untitled]", BOXI_VERSION);
	}
	else
	{
		bool clean = mpConfig->IsClean();
		title.Printf("Boxi %s - %s%s", BOXI_VERSION, 
			mConfigFileName.c_str(), clean ? "" : "*");
	}
	SetTitle(title);
}
