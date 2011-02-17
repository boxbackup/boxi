/***************************************************************************
 *            GeneralPanel.cc
 *
 *  Mon Apr  4 20:36:25 2005
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
 *
 * Contains software developed by Ben Summers.
 * YOU MUST NOT REMOVE THIS ATTRIBUTION!
 */

#include "SandBox.h"

#include <wx/notebook.h>

#include "main.h"

#include "BackupPanel.h"
#include "ClientInfoPanel.h"
#include "ComparePanel.h"
#include "GeneralPanel.h"
#include "MainFrame.h"
#include "RestorePanel.h"
#include "SetupWizard.h"

BEGIN_EVENT_TABLE(GeneralPanel, wxPanel)
	EVT_BUTTON(ID_General_Backup_Button,  GeneralPanel::OnBackupButtonClick)
	EVT_BUTTON(ID_General_Restore_Button, GeneralPanel::OnRestoreButtonClick)
	EVT_BUTTON(ID_General_Compare_Button, GeneralPanel::OnCompareButtonClick)
	EVT_BUTTON(ID_General_Setup_Wizard_Button, 
		GeneralPanel::OnSetupWizardButtonClick)
	EVT_BUTTON(ID_General_Setup_Advanced_Button, 
		GeneralPanel::OnSetupAdvancedButtonClick)
	EVT_IDLE(GeneralPanel::OnIdle)
END_EVENT_TABLE()

GeneralPanel::GeneralPanel
(
	MainFrame* pMainFrame,
	ClientConfig* pConfig,
	ServerConnection* pServerConnection,
	wxWindow* pParent
)
:	wxPanel(pParent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 
		wxTAB_TRAVERSAL, _("General Panel")),
	mpMainFrame(pMainFrame),
	mpConfig(pConfig),
	mpWizard(NULL)
{
	wxSizer* pMainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(pMainSizer);

	/* setup */
	
	mpConfigPanel = new ClientInfoPanel
	(
		mpConfig,
		pParent, 
		ID_Client_Panel
	);
	mpConfigPanel->Hide();

	wxStaticBoxSizer* pSetupBox = new wxStaticBoxSizer(wxHORIZONTAL,
		this, _("Setup"));
	pMainSizer->Add(pSetupBox, 0, wxGROW | wxALL, 8);
	
	wxStaticText* pSetupText = new wxStaticText(this, wxID_ANY, 
		_("Configure Boxi for a backup store provider"));
	pSetupBox->Add(pSetupText, 1, wxALIGN_CENTER_VERTICAL | wxALL, 8);
	
	wxButton* pWizardButton = new wxButton(this, 
		ID_General_Setup_Wizard_Button, _("&Wizard..."));
	pSetupBox->Add(pWizardButton, 0, 
		wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 8);

	wxButton* pAdvancedButton = new wxButton(this, 
		ID_General_Setup_Advanced_Button, _("&Advanced..."));
	pSetupBox->Add(pAdvancedButton, 0, 
		wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 8);

	/* backup */
	
	mpBackupPanel = new BackupPanel
	(
		mpConfig,
		mpConfigPanel,
		mpMainFrame,
		pParent
	);
	mpConfig->AddListener(mpBackupPanel);
	mpBackupPanel->Hide();

	wxStaticBoxSizer* pBackupBox = new wxStaticBoxSizer(wxHORIZONTAL,
		this, _("Backup"));
	pMainSizer->Add(pBackupBox, 0, wxGROW | wxALL, 8);
	
	wxStaticText* pBackupText = new wxStaticText(this, wxID_ANY, 
		_("Copy files from this computer to the backup server"));
	pBackupBox->Add(pBackupText, 1, wxALIGN_CENTER_VERTICAL | wxALL, 8);
	
	wxButton* pBackupButton = new wxButton(this, 
		ID_General_Backup_Button, _("&Backup"));
	pBackupBox->Add(pBackupButton, 0, 
		wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 8);

	/* restore */
	
	mpRestorePanel = new RestorePanel
	(
		mpConfig,
		mpConfigPanel,
		pMainFrame,
		pServerConnection,
		pParent
	);
	mpRestorePanel->Hide();

	wxStaticBoxSizer* pRestoreBox = new wxStaticBoxSizer(wxHORIZONTAL,
		this, _("Restore"));
	pMainSizer->Add(pRestoreBox, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	wxStaticText* pRestoreText = new wxStaticText(this, wxID_ANY, 
		_("Copy files from the backup server to this computer"));
	pRestoreBox->Add(pRestoreText, 1, wxALIGN_CENTER_VERTICAL | wxALL, 8);
	
	wxButton* pRestoreButton = new wxButton(this, 
		ID_General_Restore_Button, _("&Restore"));
	pRestoreBox->Add(pRestoreButton, 0, 
		wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 8);

	/* compare */

	mpComparePanel = new ComparePanel
	(
		mpConfig,
		pMainFrame,
		pServerConnection,
		pParent
	);
	mpComparePanel->Hide();

	wxStaticBoxSizer* pCompareBox = new wxStaticBoxSizer(wxHORIZONTAL,
		this, _("Compare"));
	pMainSizer->Add(pCompareBox, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	wxStaticText* pCompareText = new wxStaticText(this, wxID_ANY, 
		_("Compare files on the backup server with this computer"));
	pCompareBox->Add(pCompareText, 1, wxALIGN_CENTER_VERTICAL | wxALL, 8);
	
	wxButton* pCompareButton = new wxButton(this, 
		ID_General_Compare_Button, _("&Compare"));
	pCompareBox->Add(pCompareButton, 0, 
		wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 8);
}

void GeneralPanel::AddToNotebook(wxNotebook* pNotebook)
{
	pNotebook->AddPage(this,           _("General"));
	mpConfigPanel ->AddToNotebook(pNotebook);
	mpBackupPanel ->AddToNotebook(pNotebook);
	mpRestorePanel->AddToNotebook(pNotebook);
	mpComparePanel->AddToNotebook(pNotebook);
}

void GeneralPanel::OnBackupButtonClick(wxCommandEvent& event)
{
	mpMainFrame->ShowPanel(mpBackupPanel);
}

void GeneralPanel::OnRestoreButtonClick(wxCommandEvent& event)
{
	mpMainFrame->ShowPanel(mpRestorePanel);
}

void GeneralPanel::OnCompareButtonClick(wxCommandEvent& event)
{
	mpMainFrame->ShowPanel(mpComparePanel);
}

void GeneralPanel::OnIdle(wxIdleEvent& event)
{
	if (mpWizard)
	{
		if (mpWizard->IsShown())
			return;
		mpWizard->Destroy();
		mpWizard = NULL;
	}
}

void GeneralPanel::OnSetupWizardButtonClick(wxCommandEvent& event)
{
	// SetupWizard wizard(mpConfig, this);
	// wizard.Run();
	mpWizard = new SetupWizard(mpConfig, this);
	mpWizard->Run();
	// pWizard->Destroy();
	// delete pWizard;
}

void GeneralPanel::OnSetupAdvancedButtonClick(wxCommandEvent& event)
{
	mpMainFrame->ShowPanel(mpConfigPanel);
}

void GeneralPanel::RefreshConfig()
{
	mpConfigPanel->Reload();
}
