/***************************************************************************
 *            MainFrame.cc
 *
 *  Sun Jan 22 22:36:58 2006
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
 */

#include "SandBox.h"

#include <wx/filename.h>
#include <wx/notebook.h>

#define TLS_CLASS_IMPLEMENTATION_CPP
#include "ServerConnection.h"
#undef TLS_CLASS_IMPLEMENTATION_CPP

#include "BoxiApp.h"
#include "GeneralPanel.h"
#include "MainFrame.h"
#include "TestFileDialog.h"

BEGIN_EVENT_TABLE(MainFrame, wxFrame)
	EVT_MENU(ID_File_New,     MainFrame::OnFileNew)
	EVT_MENU(ID_File_Open,    MainFrame::OnFileOpen)
	EVT_MENU(ID_File_Save,    MainFrame::OnFileSave)
	EVT_MENU(ID_File_SaveAs,  MainFrame::OnFileSaveAs)
	EVT_MENU(wxID_EXIT,       MainFrame::OnFileQuit)
	EVT_MENU(ID_View_Old,     MainFrame::OnViewOld)
	EVT_MENU(ID_View_Deleted, MainFrame::OnViewDeleted)
	EVT_MENU(wxID_ABOUT,      MainFrame::OnHelpAbout)
	EVT_SIZE (MainFrame::OnSize)
	EVT_CLOSE(MainFrame::OnClose)
END_EVENT_TABLE()

MainFrame::MainFrame
(
	const wxString* pConfigFileName,
	const wxString& rBoxiExecutablePath,
	const wxPoint& pos, 
	const wxSize& size, 
	long style
)
:	wxFrame(NULL, ID_Main_Frame, wxT("Boxi"), pos, size, style)
{	
	mpConfig = new ClientConfig();

	mpConfig->AddListener(this);

	mpServerConnection = new ServerConnection(mpConfig);
	mpStatusBar = CreateStatusBar(1);
	
	mpTopNotebook = new wxNotebook(this, ID_Top_Notebook);

	mpGeneralPanel = new GeneralPanel(this, mpConfig, mpServerConnection, 
		mpTopNotebook);
	mpGeneralPanel->AddToNotebook(mpTopNotebook);
	
	/*
	wxPanel* pBackupDaemonPanel = new BackupDaemonPanel(
		mpConfig,
		rBoxiExecutablePath,
		mpTopNotebook,
		-1);
	mpTopNotebook->AddPage(pBackupDaemonPanel, wxT("Backup Process"));
	*/
	
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

	if (pConfigFileName) 
	{
		DoFileOpen(*pConfigFileName);
	} 
	else 
	{
		DoFileNew();
	}
}

void MainFrame::OnFileNew(wxCommandEvent& event) 
{
	DoFileNew();
}

void MainFrame::DoFileNew() 
{
	mpConfig->Reset();
	UpdateTitle();
}

static const wxChar *FILETYPES = wxT( 
	"Box Backup client configuration file|bbackupd.conf|"
	"Configuration files|*.conf|"
	"All files|*.*"
	);

void MainFrame::OnFileOpen(wxCommandEvent& event) 
{
	TestFileDialog openFileDialog(
		this, wxT("Open file"), wxT(""), wxT("bbackupd.conf"), 
		FILETYPES, wxOPEN | wxFILE_MUST_EXIST);

	if (wxGetApp().ShowFileDialog(openFileDialog) != wxID_OK)
		return;

	/*
	There seems to be a wxFilename change bug here. Tests fail with
	this code:
	*/
	DoFileOpen(openFileDialog.GetPath());
	/*
	I had changed it to this, to make the tests pass:
	wxFileName fn(openFileDialog.GetPath(), openFileDialog.GetFilename());
	DoFileOpen(fn.GetFullPath());
	but that breaks the application.
	*/
}

void MainFrame::DoFileOpen(const wxString& path)
{
	mpConfig->RemoveListener(this);
	
	try 
	{
		mpConfig->Load(path);
	} 
	catch (BoxException &e) 
	{
		mpConfig->AddListener(this);
		wxString str;
		str.Printf(wxT("Failed to load configuration file (%s):\n\n"), 
			path.c_str());
		str.Append(wxString(e.what(), wxConvBoxi));
		wxGetApp().ShowMessageBox(BM_MAIN_FRAME_CONFIG_LOAD_FAILED,
			str, wxT("Boxi Error"), wxOK | wxICON_ERROR, NULL);
		return;
	} 
	catch (wxString* e) 
	{
		mpConfig->AddListener(this);
		wxString str;
		str.Printf(
			wxT("Failed to load configuration file (%s):\n\n%s"), 
			path.c_str(), e->c_str());
		wxGetApp().ShowMessageBox(BM_MAIN_FRAME_CONFIG_LOAD_FAILED,
			str, wxT("Boxi Error"), wxOK | wxICON_ERROR, NULL);
		return;
	}		
	
	mpGeneralPanel->RefreshConfig();

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
		int result = wxGetApp().ShowMessageBox(
			BM_MAIN_FRAME_CONFIG_HAS_ERRORS_WHEN_SAVING,
			wxT("The configuration file has errors.\n"
			"Are you sure you want to save it?"),
			wxT("Boxi Warning"), wxYES_NO | wxICON_QUESTION, this);
		
		if (result == wxNO)
			return;
	}

	if (mpConfig->GetFileName().Length() == 0) 
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
		int result = wxGetApp().ShowMessageBox(
			BM_MAIN_FRAME_CONFIG_HAS_ERRORS_WHEN_SAVING,
			wxT("The configuration file has errors.\n"
			"Are you sure you want to save it?"),
			wxT("Boxi Warning"), wxYES_NO | wxICON_QUESTION, this);
		
		if (result == wxNO)
			return;
	}

	DoFileSaveAs2();
}

void MainFrame::DoFileSaveAs2() 
{
	TestFileDialog saveFileDialog(
		this, wxT("Save file"), wxT(""), wxT("bbackupd.conf"), 
		FILETYPES, wxSAVE | wxOVERWRITE_PROMPT);

	if (wxGetApp().ShowFileDialog(saveFileDialog) != wxID_OK)
		return;

	wxFileName fn(saveFileDialog.GetDirectory(), 
		saveFileDialog.GetFilename());
	mpConfig->Save(fn.GetFullPath());
}

void MainFrame::OnFileQuit(wxCommandEvent& event) {
	Close();
}

void MainFrame::OnClose(wxCloseEvent& event) 
{
	if (event.CanVeto() && !(mpConfig->IsClean()))
	{
		int result = wxGetApp().ShowMessageBox
		(
			BM_MAIN_FRAME_CONFIG_CHANGED_ASK_TO_SAVE,
			wxT("The configuration file has not been saved.\n"
			"Do you want to save your changes?"),
			wxT("Boxi Warning"), 
			wxYES_NO | wxCANCEL | wxICON_QUESTION,
			this
		);
		
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

void MainFrame::OnViewOld(wxCommandEvent& event) 
{
	/*
	mpRestorePanel->SetViewOldFlag(
		mpViewMenu->IsChecked(ID_View_Old));
	*/
}

void MainFrame::OnViewDeleted(wxCommandEvent& event) 
{
	/*
	mpRestorePanel->SetViewDeletedFlag(
		mpViewMenu->IsChecked(ID_View_Deleted));
	*/
}

void MainFrame::OnFileDir(wxCommandEvent& event) {
	wxDirDialog *d = new wxDirDialog(
		this, wxT("Choose a Directory"), 
		wxGetCwd(), 0, wxDefaultPosition);

	if (d->ShowModal() == wxID_OK)
		wxSetWorkingDirectory(d->GetPath());
}

void MainFrame::OnHelpAbout(wxCommandEvent& event)
{
	wxString msg;
	msg.Printf(wxT("Boxi " BOXI_VERSION " by Chris Wilson\n"
		"A Graphical User Interface for Box Backup\n"
		"Home Page: http://boxi.sourceforge.net/\n"
		"\n"
		"Licensed under the GNU General Public License (GPL)\n"
		"This product includes software developed by Ben Summers.\n"
		"\n"
		"Boxi: %s\n"
		"Box Backup: %s"),
		wxT(BOXI_VERSION_ADVANCED),
		wxString(GetBoxBackupVersion().c_str(), wxConvLibc).c_str());
	wxMessageBox(msg, wxT("About Boxi"), wxOK | wxICON_INFORMATION, this);
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

void MainFrame::NotifyChange() 
{
	UpdateTitle();
}

void MainFrame::UpdateTitle() 
{
	wxString title;
	wxString filename;
	
	if (mpConfig->GetFileName().Length() == 0) 
	{
		filename = wxT("[untitled]");
	}
	else
	{
		filename = mpConfig->GetFileName();
	}
	
	bool clean = mpConfig->IsClean();
	title.Printf(wxT("Boxi " BOXI_VERSION " - %s%s"), 
		filename.c_str(), clean ? wxT("") : wxT("*"));
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

bool MainFrame::IsTopPanel(wxPanel* pTargetPanel)
{
	size_t index = mpTopNotebook->GetSelection();
	wxWindow* pPage = mpTopNotebook->GetPage(index);
	return (pPage == pTargetPanel);
}
