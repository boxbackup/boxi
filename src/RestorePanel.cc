/***************************************************************************
 *            RestorePanel.cc
 *
 *  2005-12-31
 *  Copyright 2005-2006 Chris Wilson
 *  Email <chris-boxisource@qwirx.com>
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
 * Contains software developed by Ben Summers.
 * YOU MUST NOT REMOVE THIS ATTRIBUTION!
 */

#include <wx/statbox.h>
#include <wx/listbox.h>
#include <wx/button.h>

#include "main.h"

#include "BackupLocationsPanel.h"
#include "BackupProgressPanel.h"
#include "ClientInfoPanel.h"
#include "MainFrame.h"
#include "ParamPanel.h"
#include "RestorePanel.h"
#include "RestoreProgressPanel.h"

BEGIN_EVENT_TABLE(RestorePanel, FunctionPanel)
	EVT_RADIOBUTTON(wxID_ANY, RestorePanel::OnRadioButtonClick)
END_EVENT_TABLE()

RestorePanel::RestorePanel
(
	ClientConfig*         pConfig,
	ClientInfoPanel*      pClientConfigPanel,
	MainFrame*            pMainFrame,
	ServerConnection*     pServerConnection,
	wxWindow*             pParent
)
:	FunctionPanel(wxT("Restore Panel"), pConfig, pClientConfigPanel, 
		pMainFrame, pParent, ID_Restore_Panel)
{
	mpProgressPanel = new RestoreProgressPanel(pConfig, pServerConnection,
		pParent, ID_Restore_Progress_Panel);
	mpProgressPanel->Hide();
	
	mpSourceBox->GetStaticBox()->SetLabel(wxT("&Files to restore"));
	mpDestBox  ->GetStaticBox()->SetLabel(wxT("Restore &Destination"));

	mpOldLocRadio = new wxRadioButton(this, wxID_ANY, 
		wxT("&Original Locations"),	wxDefaultPosition, wxDefaultSize, 
		wxRB_GROUP);
	mpDestBox->Add(mpOldLocRadio, 0, wxGROW | wxALL, 8);

	wxSizer* mpNewDestSizer = new wxBoxSizer(wxHORIZONTAL);
	mpDestBox->Add(mpNewDestSizer, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	mpNewLocRadio = new wxRadioButton(this, ID_Restore_Panel_New_Location_Radio, 
		wxT("&New Location:"), wxDefaultPosition, wxDefaultSize, 0);
	mpNewDestSizer->Add(mpNewLocRadio, 0, wxGROW, 0);

	mpNewLocText = new wxTextCtrl(this, ID_Restore_Panel_New_Location_Text, 
		wxT(""));
	mpNewDestSizer->Add(mpNewLocText, 1, wxGROW | wxLEFT, 8);
	
	mpNewLocButton = new DirSelButton(this, wxID_ANY, mpNewLocText);
	mpNewDestSizer->Add(mpNewLocButton, 0, wxGROW | wxLEFT, 4);

	mpRestoreDirsCheck = new wxCheckBox(this, wxID_ANY, 
		wxT("Restore d&irectory structure"));
	mpDestBox->Add(mpRestoreDirsCheck, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpOverwriteCheck = new wxCheckBox(this, wxID_ANY, 
		wxT("O&verwrite existing files"));
	mpDestBox->Add(mpOverwriteCheck, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	mpSourceEditButton->SetLabel(wxT("&Select Files"));
	mpStartButton     ->SetLabel(wxT("Start &Restore"));
	
	mpFilesPanel = new RestoreFilesPanel(pConfig, 
		pServerConnection, pMainFrame, pParent, this, this);
	mpFilesPanel->Hide();

	Update();
	UpdateEnabledState();
	
	mpConfig->AddListener(this);
}

void RestorePanel::OnRestoreSpecChange()
{
	Update();
}

void RestorePanel::Update()
{
	mpSourceList->Clear();
	
	const RestoreSpecEntry::Vector rEntries = 
		mpFilesPanel->GetRestoreSpec().GetEntries();
	
	for (RestoreSpecEntry::ConstIterator pEntry = rEntries.begin();
		pEntry != rEntries.end(); pEntry++)
	{
		wxString path = pEntry->GetNode()->GetFullPath();
		wxString entry;
		entry.Printf(wxT("%s %s"), (pEntry->IsInclude() ? wxT("+") : wxT("-")),
			path.c_str());
		mpSourceList->Append(entry);
	}
}

void RestorePanel::AddToNotebook(wxNotebook* pNotebook)
{
	pNotebook->AddPage(this, wxT("Restore"));
	pNotebook->AddPage(mpFilesPanel, wxT("Restore Files"));
	pNotebook->AddPage(mpProgressPanel, wxT("Restore Progress"));
	
}

void RestorePanel::OnClickSourceButton(wxCommandEvent& rEvent)
{
	mpMainFrame->ShowPanel(mpFilesPanel);
}

void RestorePanel::OnClickStartButton(wxCommandEvent& rEvent)
{
	wxFileName dest(mpNewLocText->GetValue());
	
	if (!dest.IsOk())
	{
		wxGetApp().ShowMessageBox(BM_RESTORE_FAILED_INVALID_DESTINATION_PATH,
			_("Cannot start restore: the destination path is not set"),
			_("Boxi Error"), wxOK | wxICON_ERROR, this);
		return;
	}
	
	mpMainFrame->ShowPanel(mpProgressPanel);
	wxYield();
	mpProgressPanel->StartRestore(mpFilesPanel->GetRestoreSpec(), dest);
}

void RestorePanel::OnRadioButtonClick(wxCommandEvent& rEvent)
{
	UpdateEnabledState();
}

void RestorePanel::UpdateEnabledState()
{
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
}
