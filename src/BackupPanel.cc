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
#include "BackupProgressPanel.h"
#include "BackupLocationsPanel.h"
#include "ClientInfoPanel.h"

BackupPanel::BackupPanel(
	ClientConfig*    pConfig,
	ClientInfoPanel* pClientConfigPanel,
	MainFrame*       pMainFrame,
	wxWindow*        pParent,
	BackupProgressPanel*  pProgressPanel,
	BackupLocationsPanel* pBackupLocationsPanel
	)
:	FunctionPanel(wxT("Backup Panel"), pConfig, pClientConfigPanel, 
	pMainFrame, pParent),
	mpProgressPanel(pProgressPanel),
	mpBackupLocationsPanel(pBackupLocationsPanel)
{
	mpSourceBox->GetStaticBox()->SetLabel(wxT("&Files to back up"));
	mpDestBox  ->GetStaticBox()->SetLabel(wxT("Backup &Destination"));

	mpSourceEditButton->SetLabel(wxT("&Edit List"));
	mpDestEditButton  ->SetLabel(wxT("&Change Server"));
	mpStartButton     ->SetLabel(wxT("&Start Backup"));
	
	Update();
}

void BackupPanel::Update()
{
	mpSourceList->Clear();
	const std::vector<Location>& rLocations = mpConfig->GetLocations();
	for (std::vector<Location>::const_iterator pLoc = rLocations.begin();
		pLoc != rLocations.end(); pLoc++)
	{
		mpSourceList->Append(pLoc->GetPath());
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

void BackupPanel::OnClickSourceButton(wxCommandEvent& rEvent)
{
	mpMainFrame->ShowPanel(mpBackupLocationsPanel);
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
