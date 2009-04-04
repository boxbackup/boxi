/***************************************************************************
 *            ComparePanel.cc
 *
 *  Tue Mar  1 00:24:16 2005
 *  Copyright 2005-2006 Chris Wilson
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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "SandBox.h"

// #include <wx/arrstr.h>
#include <wx/dir.h>
#include <wx/dirctrl.h>
#include <wx/filename.h>
#include <wx/notebook.h>
#include <wx/splitter.h>

#include "ComparePanel.h"
#include "CompareProgressPanel.h"
#include "MainFrame.h"
#include "ParamPanel.h"

BEGIN_EVENT_TABLE(ComparePanel, wxPanel)
	EVT_BUTTON(ID_Function_Start_Button,  ComparePanel::OnClickStartButton)
	EVT_BUTTON(wxID_CANCEL,               ComparePanel::OnClickCloseButton)
	EVT_RADIOBUTTON(wxID_ANY,             ComparePanel::OnClickRadioButton)
END_EVENT_TABLE()

ComparePanel::ComparePanel
(
	ClientConfig*     pConfig,
	MainFrame*        pMainFrame,
	ServerConnection* pServerConnection,
	wxWindow*         pParent
)
: wxPanel(pParent, ID_Compare_Panel, wxDefaultPosition, wxDefaultSize, 
	wxTAB_TRAVERSAL, wxT("Compare Panel")),
  mpServerConnection(pServerConnection),
  mpConfig(pConfig),
  mpMainFrame(pMainFrame)
{
	/*
	mpFilesPanel = new CompareFilesPanel(pConfig, 
		mpMainFrame->GetConnection(), pParent);
	mpFilesPanel->Hide();
	*/
	
	mpProgressPanel = new CompareProgressPanel(pConfig, 
		mpMainFrame->GetConnection(), pParent);
	mpProgressPanel->Hide();

	wxSizer* pMainSizer = new wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer* pSourceBox = new wxStaticBoxSizer(wxVERTICAL, this, 
		_("Files to compare"));
	pMainSizer->Add(pSourceBox, 1, wxGROW | wxALL, 8);
	
	mpAllLocsRadio = new wxRadioButton(this, ID_Compare_Panel_All_Locs_Radio,
		_("&All Locations"), wxDefaultPosition, wxDefaultSize, 
		wxRB_GROUP);
	pSourceBox->Add(mpAllLocsRadio, 0, wxGROW | wxALL, 8);
	
	mpOneLocRadio = new wxRadioButton(this, ID_Compare_Panel_One_Loc_Radio,
		_("&One Location:"));
	pSourceBox->Add(mpOneLocRadio, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	wxSizer* pOneLocSizer = new wxBoxSizer(wxVERTICAL);
	pSourceBox->Add(pOneLocSizer, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	mpOneLocChoice = new wxChoice(this, ID_Compare_Panel_One_Loc_Choice);
	mpOneLocChoice->Disable();
	pOneLocSizer->Add(mpOneLocChoice, 0, wxGROW | wxLEFT, 20);

	mpDirRadio = new wxRadioButton(this, ID_Compare_Panel_Dir_Radio,
		_("&Specified Directory:"));
	pSourceBox->Add(mpDirRadio, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	/*
	wxFlexGridSizer* pDirSizer = new wxFlexGridSizer(3, 8, 8);
	pDirSizer->AddGrowableCol(1, 1);
	pSourceBox->Add(pDirSizer, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	pDirSizer->Add(new wxStaticText(this, wxID_ANY, _("&Local Directory:")),
		1, wxALIGN_CENTER_VERTICAL | wxLEFT, 20);

	mpDirLocalText = new wxTextCtrl(this, 
		ID_Compare_Panel_Dir_Local_Path_Text, wxEmptyString);
	pDirSizer->Add(mpDirLocalText, 1, wxGROW);

	wxBitmap FileOpenBitmap = wxArtProvider::GetBitmap(wxART_FILE_OPEN, 
		wxART_CMN_DIALOG, wxSize(16, 16));

	mpDirLocalButton = new DirSelButton(this, 
		ID_Compare_Panel_Dir_Local_Path_Button, mpDirLocalText);
	pDirSizer->Add(mpDirLocalButton, 0, wxGROW);
	
	pDirSizer->Add(new wxStaticText(this, wxID_ANY, 
		_("&Remote Directory:")), 1, 
		wxALIGN_CENTER_VERTICAL | wxLEFT, 20);
	
	mpDirRemoteText = new wxTextCtrl(this, 
		ID_Compare_Panel_Dir_Remote_Path_Text, wxEmptyString);
	pDirSizer->Add(mpDirRemoteText, 1, wxGROW);

	mpDirRemoteButton = new DirSelButton(this, 
		ID_Compare_Panel_Dir_Remote_Path_Button, mpDirRemoteText);
	pDirSizer->Add(mpDirRemoteButton, 0, wxGROW);
	*/
	
	wxSizer* pDirSizer = new wxBoxSizer(wxVERTICAL);
	pSourceBox->Add(pDirSizer, 1, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
		
	wxSplitterWindow* pSplitter = new wxSplitterWindow(this, 
		ID_Compare_Panel_Dir_Splitter);
	pDirSizer->Add(pSplitter, 1, wxGROW | wxLEFT, 20);
	
	mpDirLocalTree = new wxGenericDirCtrl(pSplitter, 
		ID_Compare_Panel_Dir_Local_Tree, wxDirDialogDefaultFolderStr,
		wxDefaultPosition, wxDefaultSize, wxDIRCTRL_3D_INTERNAL);
	mpDirRemoteTree = new wxTreeCtrl(pSplitter, 
		ID_Compare_Panel_Dir_Remote_Tree, wxDefaultPosition, 
		wxDefaultSize, wxTR_HAS_BUTTONS | wxSUNKEN_BORDER);
	pSplitter->SplitVertically(mpDirLocalTree, mpDirRemoteTree);
	pSplitter->SetMinimumPaneSize(20);
	
	wxSizer* pActionCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	pMainSizer->Add(pActionCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	wxButton* pStartButton = new wxButton(this, ID_Function_Start_Button, 
		_("Start &Compare"));
	pActionCtrlSizer->Add(pStartButton, 0, wxGROW, 0);

	wxButton* pCloseButton = new wxButton(this, wxID_CANCEL, _("Close"));
	pActionCtrlSizer->Add(pCloseButton, 0, wxGROW | wxLEFT, 8);
	
	SetSizer(pMainSizer);

	UpdateEnabledState();
	
	mpConfig->AddListener(this);
}

void ComparePanel::NotifyChange()
{
	Update();
}

void ComparePanel::Update()
{
	BoxiLocation* pOldSelectedLoc = NULL;
	int newSelectedIndex = wxNOT_FOUND;
	
	if (mpOneLocChoice->GetSelection() != wxNOT_FOUND)
	{
		pOldSelectedLoc = (BoxiLocation*)mpOneLocChoice->GetClientData
			(mpOneLocChoice->GetSelection());
	}
	
	mpOneLocChoice->Clear();
	
	const BoxiLocation::List& rLocations = mpConfig->GetLocations();
	for (BoxiLocation::ConstIterator cpLoc = rLocations.begin();
		cpLoc != rLocations.end(); cpLoc++)
	{
		BoxiLocation* pLoc = mpConfig->GetLocation(*cpLoc);

		wxString locString;
		locString.Printf(wxT("%s -> %s"), 
			pLoc->GetPath().c_str(), pLoc->GetName().c_str());

		int newIndex = mpOneLocChoice->Append(locString, pLoc);
		
		if (pOldSelectedLoc == pLoc)
		{
			newSelectedIndex = newIndex;
		}
	}
	
	if (newSelectedIndex == wxNOT_FOUND && mpOneLocChoice->GetCount() > 0)
	{
		newSelectedIndex = 0;
	}
	
	mpOneLocChoice->SetSelection(newSelectedIndex);

	/*
	mpSourceList->Clear();
	
	const RestoreSpecEntry::Vector rEntries = 
		mpFilesPanel->GetRestoreSpec().GetEntries();
	
	for (RestoreSpecEntry::ConstIterator pEntry = rEntries.begin();
		pEntry != rEntries.end(); pEntry++)
	{
		wxString path = pEntry->GetNode().GetFullPath();
		wxString entry;
		entry.Printf(wxT("%s %s"), (pEntry->IsInclude() ? wxT("+") : wxT("-")),
			path.c_str());
		mpSourceList->Append(entry);
	}
	*/
}

void ComparePanel::AddToNotebook(wxNotebook* pNotebook)
{
	pNotebook->AddPage(this, wxT("Compare"));
	// pNotebook->AddPage(mpFilesPanel, wxT("Compare Files"));
	pNotebook->AddPage(mpProgressPanel, wxT("Compare Progress"));
}

/*
void ComparePanel::OnClickSourceButton(wxCommandEvent& rEvent)
{
	mpMainFrame->ShowPanel(mpFilesPanel);
}
*/

void ComparePanel::OnClickStartButton(wxCommandEvent& rEvent)
{
	/*
	wxFileName dest(mpNewLocText->GetValue());
	
	if (!dest.IsOk())
	{
		wxGetApp().ShowMessageBox(BM_RESTORE_FAILED_INVALID_DESTINATION_PATH,
			_("Cannot start restore: the destination path is not set"),
			_("Boxi Error"), wxOK | wxICON_ERROR, this);
		return;
	}

	if (dest.DirExists() || dest.FileExists())
	{
		wxGetApp().ShowMessageBox(BM_RESTORE_FAILED_OBJECT_ALREADY_EXISTS,
			_("Cannot start restore: the destination path already exists"),
			_("Boxi Error"), wxOK | wxICON_ERROR, this);
		return;
	}
	
	mpMainFrame->ShowPanel(mpProgressPanel);
	wxYield();
	mpProgressPanel->StartRestore(mpFilesPanel->GetRestoreSpec(), dest);
	*/
	
	mpMainFrame->ShowPanel(mpProgressPanel);
	wxYield();
	
	BoxiCompareParams params;
	mpProgressPanel->StartCompare(params);
}

void ComparePanel::UpdateEnabledState()
{
	if (mpOneLocRadio->GetValue())
	{
		mpOneLocChoice->Enable();
	}
	else 
	{
		mpOneLocChoice->Disable();
	}
	
	if (mpDirRadio->GetValue())
	{
		mpDirLocalTree ->Enable();
		mpDirRemoteTree->Enable();
	}
	else
	{
		mpDirLocalTree ->Disable();
		mpDirRemoteTree->Disable();
	}
	
	/*
	if (mpOldLocRadio->GetValue())
	{
		mpNewLocText  ->Disable();
		mpNewLocButton->Disable();
		mpRestoreDirsCheck->Disable();
	}
	else if (mpNewLocRadio->GetValue())
	{
		mpNewLocText  ->Enable();
		mpNewLocButton->Enable();
		mpRestoreDirsCheck->Enable();
	}
	
	UpdateCompareSpec();
	*/
}

/*
void ComparePanel::UpdateCompareSpec()
{
	RestoreSpec& rSpec = mpFilesPanel->GetRestoreSpec();
	rSpec.SetRestoreToDateEnabled(mpToDateCheckBox->GetValue());
	
	wxDateTime epoch = mpDatePicker->GetValue();
	epoch.SetHour  (mpHourSpin->GetValue());
	epoch.SetMinute(mpMinSpin ->GetValue());
	epoch.SetSecond(0);
	epoch.SetMillisecond(0);
	rSpec.SetRestoreToDate(epoch);
}
*/

void ComparePanel::OnClickCloseButton(wxCommandEvent& rEvent)
{
	Hide();
}

void ComparePanel::OnClickRadioButton(wxCommandEvent& rEvent)
{
	UpdateEnabledState();
}
