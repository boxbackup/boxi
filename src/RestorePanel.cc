/***************************************************************************
 *            RestorePanel.cc
 *
 *  2005-12-31
 *  Copyright 2005-2008 Chris Wilson
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
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/filename.h>
#include <wx/listbox.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>

#include "main.h"

#include "BackupLocationsPanel.h"
#include "BackupProgressPanel.h"
#include "ClientInfoPanel.h"
#include "MainFrame.h"
#include "ParamPanel.h"
#include "RestorePanel.h"
#include "RestoreProgressPanel.h"

BEGIN_EVENT_TABLE(RestorePanel, FunctionPanel)
	EVT_CHECKBOX(ID_Restore_Panel_To_Date_Checkbox, 
		RestorePanel::OnClickToDateCheckBox)
	EVT_CHECKBOX(wxID_ANY, RestorePanel::OnCheckBoxClick)
	EVT_DATE_CHANGED(ID_Restore_Panel_Date_Picker, 
		RestorePanel::OnChangeRestoreToDate)
	EVT_SPINCTRL(ID_Restore_Panel_Hour_Spin, 
		RestorePanel::OnChangeRestoreToTime)
	EVT_SPINCTRL(ID_Restore_Panel_Min_Spin,  
		RestorePanel::OnChangeRestoreToTime)
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
		pParent);
	mpProgressPanel->Hide();
	
	mpSourceBox->GetStaticBox()->SetLabel(wxT("&Files to restore"));
	mpDestBox  ->GetStaticBox()->SetLabel(wxT("Restore &destination"));

	/*
	mpOldLocRadio = new wxRadioButton(this, wxID_ANY, 
		wxT("&Original Locations"),	wxDefaultPosition, wxDefaultSize, 
		wxRB_GROUP);
	mpDestBox->Add(mpOldLocRadio, 0, wxGROW | wxALL, 8);
	*/

	wxSizer* mpNewDestSizer = new wxBoxSizer(wxHORIZONTAL);
	mpDestBox->Add(mpNewDestSizer, 0, wxGROW | wxALL, 8);
	
	wxStaticText* pNewLocLabel = new wxStaticText(this, wxID_ANY, 
		_("&New location:"), wxDefaultPosition, wxDefaultSize, 0);
	mpNewDestSizer->Add(pNewLocLabel, 0, wxALIGN_CENTER, 0);

	mpNewLocText = new wxTextCtrl(this, ID_Restore_Panel_New_Location_Text, 
		wxT(""));
	mpNewDestSizer->Add(mpNewLocText, 1, wxGROW | wxLEFT, 8);
	
	mpNewLocButton = new DirSelButton(this, wxID_ANY, mpNewLocText);
	mpNewDestSizer->Add(mpNewLocButton, 0, wxGROW | wxLEFT, 4);

	wxBoxSizer* pDateSelSizer = new wxBoxSizer(wxHORIZONTAL);
	mpDestBox->Add(pDateSelSizer, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpToDateCheckBox = new wxCheckBox(this, 
		ID_Restore_Panel_To_Date_Checkbox, _("Restore &to date:"));
	pDateSelSizer->Add(mpToDateCheckBox, 0, wxGROW, 0);
	
	mpDatePicker = new wxDatePickerCtrl(this, ID_Restore_Panel_Date_Picker);
	pDateSelSizer->Add(mpDatePicker, 1, wxGROW | wxLEFT, 8);
	
	wxStaticText* pAtLabel = new wxStaticText(this, wxID_ANY, 
		_("&at"), wxDefaultPosition, wxDefaultSize, 0);
	pDateSelSizer->Add(pAtLabel, 0, wxALIGN_CENTER | wxLEFT, 8);

	mpHourSpin = new wxSpinCtrl(this, ID_Restore_Panel_Hour_Spin, 
		wxEmptyString, wxDefaultPosition, wxDefaultSize, 
		wxSP_ARROW_KEYS | wxSP_WRAP, 0, 23);
	pDateSelSizer->Add(mpHourSpin, 0, wxGROW | wxLEFT, 8);

	wxStaticText* pColonLabel = new wxStaticText(this, wxID_ANY, 
		_("&:"), wxDefaultPosition, wxDefaultSize, 0);
	pDateSelSizer->Add(pColonLabel, 0, wxALIGN_CENTER | wxLEFT, 4);

	mpMinSpin = new wxSpinCtrl(this, ID_Restore_Panel_Min_Spin, 
		wxEmptyString, wxDefaultPosition, wxDefaultSize, 
		wxSP_ARROW_KEYS | wxSP_WRAP, 0, 59);
	pDateSelSizer->Add(mpMinSpin, 0, wxGROW | wxLEFT, 4);

	/*
	mpRestoreDirsCheck = new wxCheckBox(this, wxID_ANY, 
		wxT("Restore d&irectory structure"));
	mpDestBox->Add(mpRestoreDirsCheck, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpOverwriteCheck = new wxCheckBox(this, wxID_ANY, 
		wxT("O&verwrite existing files"));
	mpDestBox->Add(mpOverwriteCheck, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	*/
	
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
		wxString path = pEntry->GetNode().GetFullPath();
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

	if (wxFileName(dest.GetFullPath(), _("")).DirExists() || dest.FileExists())
	{
		wxGetApp().ShowMessageBox(BM_RESTORE_FAILED_OBJECT_ALREADY_EXISTS,
			_("Cannot start restore: the destination path already exists"),
			_("Boxi Error"), wxOK | wxICON_ERROR, this);
		return;
	}
	
	mpMainFrame->ShowPanel(mpProgressPanel);
	wxYield();
	mpProgressPanel->StartRestore(mpFilesPanel->GetRestoreSpec(), dest);
}

void RestorePanel::OnCheckBoxClick(wxCommandEvent& rEvent)
{
	UpdateEnabledState();
}

void RestorePanel::UpdateEnabledState()
{
	if (mpToDateCheckBox->GetValue())
	{
		mpDatePicker->Enable();
		mpHourSpin  ->Enable();
		mpMinSpin   ->Enable();
	}
	else
	{
		mpDatePicker->Disable();
		mpHourSpin  ->Disable();
		mpMinSpin   ->Disable();
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
	*/
	
	UpdateRestoreSpec();
}

void RestorePanel::OnClickToDateCheckBox(wxCommandEvent& rEvent)
{
	wxDateTime now = wxDateTime::Now();
	mpDatePicker->SetValue(now);
	mpHourSpin  ->SetValue(now.GetHour());
	mpMinSpin   ->SetValue(now.GetMinute());
	UpdateEnabledState();
}

void RestorePanel::OnChangeRestoreToDate(wxDateEvent& rEvent)
{
	UpdateRestoreSpec();
}

void RestorePanel::UpdateRestoreSpec()
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

void RestorePanel::OnChangeRestoreToTime(wxSpinEvent& rEvent)
{
	UpdateRestoreSpec();
}
