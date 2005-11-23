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

#include "GeneralPanel.h"
#include "BackupDaemonPanel.h"
#include "BackupFilesPanel.h"
#include "BackupLocationsPanel.h"
#include "ClientInfoPanel.h"
#include "ServerFilesPanel.h"
#include "BackupPanel.h"
#include "BackupProgressPanel.h"

bool ViewDeleted = true;

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

void AddParam(wxPanel* panel, const wxChar* label, wxWindow* editor, 
	bool growToFit, wxSizer *sizer) 
{
	sizer->Add(new wxStaticText(panel, -1, wxString(label, wxConvLibc), 
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
	: wxFrame(NULL, -1, wxT("Boxi"), pos, size, style)
{
	// Initialise global SSL system configuration/state
	SSLLib::Initialise();

	mpConfig = new ClientConfig();

	mpConfig->AddListener(this);

	mpServerConnection = new ServerConnection(mpConfig);
	mpStatusBar = CreateStatusBar(1);
	
	mpTopNotebook = new wxNotebook(this, ID_Top_Notebook);

	mpClientConfigPanel = new ClientInfoPanel(
		mpConfig,
		mpTopNotebook, 
		ID_Client_Panel);
	mpClientConfigPanel->Hide();
	
	BackupProgressPanel *pBackupProgressPanel = new BackupProgressPanel(
		mpConfig, mpServerConnection, 
		mpTopNotebook, ID_Backup_Progress_Panel);
	pBackupProgressPanel->Hide();
	
	BackupPanel *pBackupPanel = new BackupPanel(
		mpConfig, pBackupProgressPanel, this, mpClientConfigPanel,
		mpTopNotebook, ID_Backup_Panel);
	mpConfig->AddListener(pBackupPanel);
	pBackupPanel->Hide();

	GeneralPanel* pGeneralPanel = new GeneralPanel(this, 
		pBackupPanel, mpTopNotebook);
		
	mpTopNotebook->AddPage(pGeneralPanel, wxT("General"));
	mpTopNotebook->AddPage(pBackupPanel, wxT("Backup"));
	mpTopNotebook->AddPage(pBackupProgressPanel, wxT("Backup Progress"));
	mpTopNotebook->AddPage(mpClientConfigPanel, wxT("Server Details"));

	wxPanel* pBackupDaemonPanel = new BackupDaemonPanel(
		mpConfig,
		rBoxiExecutablePath,
		mpTopNotebook,
		-1);
	mpTopNotebook->AddPage(pBackupDaemonPanel, wxT("Backup Process"));
	
	mpLocationsPanel = new BackupLocationsPanel(
		mpConfig,
		mpTopNotebook, 
		ID_Client_Panel);
	mpTopNotebook->AddPage(mpLocationsPanel, wxT("Backup"));

	/*
	theBackupFilesPanel = new BackupFilesPanel(
		mpConfig,
		mpServerConnection,
		theTopNotebook, 
		ID_Backup_Files_Panel);
	theTopNotebook->AddPage(theBackupFilesPanel, wxT("Local Files"));
	*/
	
	mpRestorePanel = new RestorePanel(
		mpConfig,
		mpServerConnection, 
		mpTopNotebook, 
		mpStatusBar, 
		ID_Restore_Files_Panel);
	mpTopNotebook->AddPage(mpRestorePanel, wxT("Restore"));
	
	wxMenu *menuFile = new wxMenu;
	menuFile->Append(ID_File_New,  wxT("&New...\tCtrl-N"), 
		wxT("New client config file"));
	menuFile->Append(ID_File_Open,  wxT("&Open...\tCtrl-O"), 
		wxT("Open client config file"));
	menuFile->Append(ID_File_Save,  wxT("&Save...\tCtrl-S"), 
		wxT("Save current configuration"));
	menuFile->Append(ID_File_SaveAs,  wxT("Save &As...\tCtrl-A"), 
		wxT("Save current config file with a new name"));
	menuFile->Append(wxID_EXIT,  wxT("E&xit\tAlt-F4"), 
		wxT("Quit Boxi"));

	mpViewMenu = new wxMenu;
	mpViewMenu->Append(ID_View_Old, 
		wxT("Show &Old Files\tCtrl-L"), 
		wxT("Show old versions of files"), wxITEM_CHECK);
	mpViewMenu->Append(ID_View_Deleted, 
		wxT("Show &Deleted Files\tCtrl-D"), 
		wxT("Show deleted files"), wxITEM_CHECK);
	mpViewMenu->Check(ID_View_Old,     TRUE);
	mpViewMenu->Check(ID_View_Deleted, TRUE);

	wxMenu *menuHelp = new wxMenu;
	menuHelp->Append(wxID_ABOUT, wxT("&About...\tF1"), 
		wxT("Show information about Boxi"));

	wxMenuBar *menuBar = new wxMenuBar;
	menuBar->Append(menuFile,   wxT("&File"));
	menuBar->Append(mpViewMenu, wxT("&View"));
	menuBar->Append(menuHelp,   wxT("&Help"));
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
	mConfigFileName = wxT("");
	UpdateTitle();
}

static const wxChar *FILETYPES = wxT( 
	"Box Backup client configuration file|bbackupd.conf|"
	"Configuration files|*.conf|"
	"All files|*.*"
	);

void MainFrame::OnFileOpen(wxCommandEvent& event) {
	wxFileDialog *openFileDialog = new wxFileDialog(
		this, wxT("Open file"), wxT(""), wxT("bbackupd.conf"), 
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
		str.Printf(wxT("Failed to load configuration file (%s)"), 
			path.c_str());
		wxMessageBox(str, wxT("Boxi Error"), 
			wxOK | wxICON_ERROR, NULL);
		return;
	} catch (wxString* e) {
		mpConfig->AddListener(this);
		wxString str;
		str.Printf(
			wxT("Failed to load configuration file (%s):\n\n%s"), 
			path.c_str(), e->c_str());
		wxMessageBox(str, wxT("Boxi Error"), 
			wxOK | wxICON_ERROR, NULL);
		return;
	}		
	
	mpClientConfigPanel->Reload();

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
			wxT("The configuration file has errors.\n"
			"Are you sure you want to save it?"),
			wxT("Boxi Warning"), wxYES_NO | wxICON_QUESTION);
		
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
			wxT("The configuration file has errors.\n"
			"Are you sure you want to save it?"),
			wxT("Boxi Warning"), wxYES_NO | wxICON_QUESTION);
		
		if (result == wxNO)
			return;
	}

	DoFileSaveAs2();
}

void MainFrame::DoFileSaveAs2() 
{
	wxFileDialog *saveFileDialog = new wxFileDialog(
		this, wxT("Save file"), wxT(""), wxT("bbackupd.conf"), 
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
			wxT("The configuration file has not been saved.\n"
			"Do you want to save your changes?"),
			wxT("Boxi Warning"), 
			wxYES_NO | wxCANCEL | wxICON_QUESTION);
		
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
	mpRestorePanel->SetViewOldFlag(
		mpViewMenu->IsChecked(ID_View_Old));
}

void MainFrame::OnViewDeleted(wxCommandEvent& event) {
	mpRestorePanel->SetViewDeletedFlag(
		mpViewMenu->IsChecked(ID_View_Deleted));
}

void MainFrame::OnFileDir(wxCommandEvent& event) {
	wxDirDialog *d = new wxDirDialog(
		this, wxT("Choose a Directory"), 
		wxGetCwd(), 0, wxDefaultPosition);

	if (d->ShowModal() == wxID_OK)
		wxSetWorkingDirectory(d->GetPath());
}

void MainFrame::OnHelpAbout(wxCommandEvent& event) {
	wxMessageBox(wxT("Boxi " BOXI_VERSION " by Chris Wilson\n"
		"A Graphical User Interface for Box Backup\n"
		"Home Page: http://www.qwirx.com/boxi/\n"
		"\n"
		"Licensed under the GNU General Public License (GPL)\n"
		"This product includes software developed by Ben Summers."),
		wxT("About Boxi"), wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnSize(wxSizeEvent& event) 
{
	wxString theSizeText;
	wxSize theNewSize = event.GetSize();
	theSizeText.Printf(wxT("%d x %d"), theNewSize.GetWidth(), 
		theNewSize.GetHeight());
	// theText->SetValue(theSizeText);
	wxFrame::OnSize(event);
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
		title.Printf(wxT("Boxi " BOXI_VERSION " - [untitled]"));
	}
	else
	{
		bool clean = mpConfig->IsClean();
		title.Printf(wxT("Boxi " BOXI_VERSION " - %s%s"), 
			mConfigFileName.c_str(), clean ? wxT("") : wxT("*"));
	}
	SetTitle(title);
}

void MainFrame::ShowPanel(wxPanel* pTargetPanel)
{
	pTargetPanel->Show();
	for (size_t index = 0; index < mpTopNotebook->GetPageCount(); index++)
	{
		wxWindow* pPage = mpTopNotebook->GetPage(index);
		if (pPage == pTargetPanel)
		{
			mpTopNotebook->SetSelection(index);
			break;
		}
	}
}
