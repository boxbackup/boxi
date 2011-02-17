/***************************************************************************
 *            BackupPanel.cc
 *
 *  Mon Apr  4 20:36:25 2005
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

#include "SandBox.h"

#include <wx/button.h>
#include <wx/listbox.h>
#include <wx/notebook.h>
#include <wx/statbox.h>

#include "main.h"
#include "BackupPanel.h"
#include "BackupProgressPanel.h"
#include "BackupLocationsPanel.h"
#include "ClientInfoPanel.h"
#include "MainFrame.h"

BackupPanel::BackupPanel
(
	ClientConfig*    pConfig,
	ClientInfoPanel* pClientConfigPanel,
	MainFrame*       pMainFrame,
	wxWindow*        pParent
)
:	FunctionPanel(_("Backup Panel"), pConfig, pClientConfigPanel, 
		pMainFrame, pParent, ID_Backup_Panel)
{
	mpProgressPanel = new BackupProgressPanel(pConfig, 
		mpMainFrame->GetConnection(), pParent);
	mpProgressPanel->Hide();
	
	mpLocationsPanel = new BackupLocationsPanel(pConfig, pParent, 
		mpMainFrame, this, ID_Backup_Files_Panel);
	mpLocationsPanel->Hide();

	mpSourceBox->GetStaticBox()->SetLabel(_("&Files to back up"));
	mpDestBox  ->GetStaticBox()->SetLabel(_("Backup &Destination"));

	mpDestLabel = new wxStaticText(this, wxID_ANY, wxT(""));
	mpDestBox->Add(mpDestLabel, 0, wxGROW | wxALL, 8);

	mpDestCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	mpDestBox->Add(mpDestCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpDestEditButton = new wxButton(this, ID_Function_Dest_Button, 
		_("&Change Server"));
	mpDestCtrlSizer->Add(mpDestEditButton, 0, wxGROW, 0);

	mpSourceEditButton->SetLabel(_("&Edit List"));
	mpStartButton     ->SetLabel(_("&Start Backup"));
	
	Update();
}

void BackupPanel::AddToNotebook(wxNotebook* pNotebook)
{
	pNotebook->AddPage(this, _("Backup"));
	pNotebook->AddPage(mpLocationsPanel, _("Backup Files"));
	pNotebook->AddPage(mpProgressPanel,  _("Backup Progress"));
}

void BackupPanel::Update()
{
	mpSourceList->Clear();
	const BoxiLocation::List& rLocations = mpConfig->GetLocations();
	for (BoxiLocation::ConstIterator pLoc = rLocations.begin();
		pLoc != rLocations.end(); pLoc++)
	{
		mpSourceList->Append(pLoc->GetPath());
	}
	
	wxString label;
	wxString tmp;

	int account;
	if (mpConfig->AccountNumber.GetInto(account))
		tmp.Printf(_("Account 0x%08x"), account);
	else
		tmp = _("Unknown account number");
	label.Append(tmp);
	
	label.append(_(" on "));
	
	std::string server;
	if (mpConfig->StoreHostname.GetInto(server))
		tmp.Printf(_("server %s"), 
			wxString(server.c_str(), wxConvBoxi).c_str());
	else
		tmp = _("unknown server");
	label.Append(tmp);

	mpDestLabel->SetLabel(label);	
}

void BackupPanel::OnClickSourceButton(wxCommandEvent& rEvent)
{
	mpMainFrame->ShowPanel(mpLocationsPanel);
}

void BackupPanel::OnClickDestButton(wxCommandEvent& rEvent)
{
	mpMainFrame->ShowPanel(mpClientConfigPanel);
}

void BackupPanel::OnClickStartButton(wxCommandEvent& rEvent)
{
	mpMainFrame->ShowPanel(mpProgressPanel);
	wxYield();
	mpProgressPanel->StartBackup();
}
