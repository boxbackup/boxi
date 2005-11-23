/***************************************************************************
 *            BackupPanel.cc
 *
 *  Mon Apr  4 20:36:25 2005
 *  Copyright  2005  Chris Wilson
 *  Email <boxi_BackupPanel.cc@qwirx.com>
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
#include "BackupPanel.h"
#include "BackupLocationsPanel.h"
#include "ClientInfoPanel.h"

BEGIN_EVENT_TABLE(BackupPanel, wxPanel)
EVT_BUTTON(ID_Backup_Locations_Button, BackupPanel::OnClickLocationsButton)
EVT_BUTTON(ID_Backup_Config_Button,    BackupPanel::OnClickServerButton)
EVT_BUTTON(ID_Backup_Start_Button,     BackupPanel::OnClickStartButton)
EVT_BUTTON(wxID_CANCEL,                BackupPanel::OnClickCloseButton)
END_EVENT_TABLE()

BackupPanel::BackupPanel(
	ClientConfig *pConfig,
	BackupProgressPanel* pProgressPanel,
	MainFrame* pMainFrame,
	BackupLocationsPanel* pBackupLocationsPanel,
	ClientInfoPanel* pClientConfigPanel,
	wxWindow* parent, 
	wxWindowID id,
	const wxPoint& pos, 
	const wxSize& size,
	long style, 
	const wxString& name)
	: wxPanel(parent, id, pos, size, style, name),
	  mpConfig(pConfig),
	  mpBackupLocationsPanel(pBackupLocationsPanel),
	  mpClientConfigPanel(pClientConfigPanel),
	  mpProgressPanel(pProgressPanel),
	  mpMainFrame(pMainFrame)
{
	wxSizer* pMainSizer = new wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer* pSourceBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Files to back up"));
	pMainSizer->Add(pSourceBox, 1, wxGROW | wxALL, 8);
	
	mpSourceList = new wxListBox(this, wxID_ANY);
	pSourceBox->Add(mpSourceList, 1, wxGROW | wxALL, 8);
	
	wxSizer* pSourceCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	pSourceBox->Add(pSourceCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	wxButton* pSourceEditButton = new wxButton(this, 
		ID_Backup_Locations_Button, wxT("Edit List"));
	pSourceCtrlSizer->Add(pSourceEditButton, 0, wxGROW, 0);
	
	wxStaticBoxSizer* pDestBox = new wxStaticBoxSizer(wxVERTICAL, 
		this, wxT("Backup Destination"));
	pMainSizer->Add(pDestBox, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	mpDestLabel = new wxStaticText(this, wxID_ANY, wxT(""));
	pDestBox->Add(mpDestLabel, 0, wxGROW | wxALL, 8);

	wxSizer* pDestCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	pDestBox->Add(pDestCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	wxButton* pDestEditButton = new wxButton(this, ID_Backup_Config_Button, 
		wxT("Change Server"));
	pDestCtrlSizer->Add(pDestEditButton, 0, wxGROW, 0);

	wxSizer* pActionCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	pMainSizer->Add(pActionCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	wxButton* pBackupStartButton = new wxButton(this, 
		ID_Backup_Start_Button, wxT("Start Backup"));
	pActionCtrlSizer->Add(pBackupStartButton, 0, wxGROW, 0);

	wxButton* pCloseButton = new wxButton(this, 
		wxID_CANCEL, wxT("Close"));
	pActionCtrlSizer->Add(pCloseButton, 0, wxGROW | wxLEFT, 8);

	SetSizer( pMainSizer );
	
	Update();
}

void BackupPanel::Update()
{
	mpSourceList->Clear();
	const std::vector<Location*>& rLocations = mpConfig->GetLocations();
	for (std::vector<Location*>::const_iterator i = rLocations.begin();
		i != rLocations.end(); i++)
	{
		const Location* pLocation = *i;
		mpSourceList->Append(pLocation->GetPath());
	}
	
	wxString label;
	wxString tmp;

	int account;
	if (mpConfig->AccountNumber.GetInto(account))
		tmp.Printf(wxT("Account %d"), account);
	else
		tmp = wxT("Unknown account number");
	label.Append(tmp);
	
	label.append(wxT(" on "));
	
	std::string server;
	if (mpConfig->StoreHostname.GetInto(server))
		tmp.Printf(wxT("server %s"), 
			wxString(server.c_str(), wxConvLibc).c_str());
	else
		tmp = wxT("unknown server");
	label.Append(tmp);

	mpDestLabel->SetLabel(label);	
}

void BackupPanel::NotifyChange()
{
	Update();
}

void BackupPanel::OnClickLocationsButton(wxCommandEvent& rEvent)
{
	mpMainFrame->ShowPanel(mpBackupLocationsPanel);
}

void BackupPanel::OnClickServerButton(wxCommandEvent& rEvent)
{
	mpMainFrame->ShowPanel(mpClientConfigPanel);
}

void BackupPanel::OnClickStartButton(wxCommandEvent& rEvent)
{
	mpMainFrame->ShowPanel(mpProgressPanel);
	wxYield();
	mpProgressPanel->StartBackup();
}

void BackupPanel::OnClickCloseButton(wxCommandEvent& rEvent)
{
	Hide();
}
