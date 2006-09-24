/***************************************************************************
 *            ComparePanel.cc
 *
 *  Tue Mar  1 00:24:16 2005
 *  Copyright 2005-2006 Chris Wilson
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
 */

// #include <wx/arrstr.h>
#include <wx/dir.h>
#include <wx/filename.h>

#include "ComparePanel.h"
#include "ParamPanel.h"

BEGIN_EVENT_TABLE(ComparePanel, FunctionPanel)
END_EVENT_TABLE()

ComparePanel::ComparePanel
(
	ClientConfig*     pConfig,
	ClientInfoPanel*  pClientConfigPanel,
	MainFrame*        pMainFrame,
	ServerConnection* pServerConnection,
	wxWindow*         pParent
)
: FunctionPanel(wxT("Compare Panel"), pConfig, pClientConfigPanel, 
	pMainFrame, pParent, ID_Compare_Panel)
{
	mpSourceBox->GetStaticBox()->SetLabel(wxT("&Files to compare"));
	mpDestBox  ->GetStaticBox()->SetLabel(wxT("Compare &with"));

	mpOldLocRadio = new wxRadioButton(this, 
		ID_Compare_Panel_Old_Location_Radio, 
		wxT("&Original Locations"),
		wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	mpDestBox->Add(mpOldLocRadio, 0, wxGROW | wxALL, 8);

	wxSizer* mpNewDestSizer = new wxBoxSizer(wxHORIZONTAL);
	mpDestBox->Add(mpNewDestSizer, 0, wxGROW | wxALL, 8);
	
	mpNewLocRadio = new wxRadioButton(this, 
		ID_Compare_Panel_New_Location_Radio, 
		_("&New location:"), 
		wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	mpNewDestSizer->Add(mpNewLocRadio, 0, wxALIGN_CENTER, 0);

	mpNewLocText = new wxTextCtrl(this, ID_Compare_Panel_New_Location_Text, 
		wxT(""));
	mpNewDestSizer->Add(mpNewLocText, 1, wxGROW | wxLEFT, 8);
	
	mpNewLocButton = new DirSelButton(this, 
		ID_Compare_Panel_New_Location_Button, mpNewLocText);
	mpNewDestSizer->Add(mpNewLocButton, 0, wxGROW | wxLEFT, 4);
	
	mpSourceEditButton->SetLabel(wxT("&Select Files"));
	mpStartButton     ->SetLabel(wxT("Start &Compare"));

	/*
	mpFilesPanel = new CompareFilesPanel(pConfig, 
		pServerConnection, pMainFrame, pParent, this, this);
	mpFilesPanel->Hide();

	mpProgressPanel = new RestoreProgressPanel(pConfig, pServerConnection,
		pParent, ID_Restore_Progress_Panel);
	mpProgressPanel->Hide();
	*/

	// Update();
	UpdateEnabledState();
	
	mpConfig->AddListener(this);
}

void ComparePanel::OnCompareSpecChange()
{
	Update();
}

void ComparePanel::Update()
{
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
	// pNotebook->AddPage(mpProgressPanel, wxT("Compare Progress"));
}

void ComparePanel::OnClickSourceButton(wxCommandEvent& rEvent)
{
	// mpMainFrame->ShowPanel(mpFilesPanel);
}

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
}

void ComparePanel::UpdateEnabledState()
{
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
	*/
	
	UpdateCompareSpec();
}

void ComparePanel::UpdateCompareSpec()
{
	/*
	RestoreSpec& rSpec = mpFilesPanel->GetRestoreSpec();
	rSpec.SetRestoreToDateEnabled(mpToDateCheckBox->GetValue());
	
	wxDateTime epoch = mpDatePicker->GetValue();
	epoch.SetHour  (mpHourSpin->GetValue());
	epoch.SetMinute(mpMinSpin ->GetValue());
	epoch.SetSecond(0);
	epoch.SetMillisecond(0);
	rSpec.SetRestoreToDate(epoch);
	*/
}
