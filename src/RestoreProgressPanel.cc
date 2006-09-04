/***************************************************************************
 *            RestoreProgressPanel.cc
 *
 *  Sun Sep 3 21:02:39 2006
 *  Copyright 2006 Chris Wilson
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
 *
 * Contains software developed by Ben Summers.
 * YOU MUST NOT REMOVE THIS ATTRIBUTION!
 */

#include <errno.h>
#include <mntent.h>
#include <stdio.h>
#include <regex.h>

#include <wx/statbox.h>
#include <wx/listbox.h>
#include <wx/button.h>
#include <wx/gauge.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>

#include "SandBox.h"

#define NDEBUG
#define EXCLUDELIST_IMPLEMENTATION_REGEX_T_DEFINED
#include "BackupClientContext.h"
#include "BackupClientDirectoryRecord.h"
#include "BackupClientCryptoKeys.h"
#include "BackupClientInodeToIDMap.h"
#include "FileModificationTime.h"
#include "MemBlockStream.h"
#include "BackupStoreConstants.h"
#include "BackupStoreException.h"
#include "Utils.h"
#undef EXCLUDELIST_IMPLEMENTATION_REGEX_T_DEFINED
#undef NDEBUG

#include "main.h"
#include "RestoreProgressPanel.h"
#include "ServerConnection.h"

//DECLARE_EVENT_TYPE(myEVT_CLIENT_NOTIFY, -1)
//DEFINE_EVENT_TYPE(myEVT_CLIENT_NOTIFY)

BEGIN_EVENT_TABLE(RestoreProgressPanel, wxPanel)
EVT_BUTTON(wxID_CANCEL, RestoreProgressPanel::OnStopCloseClicked)
END_EVENT_TABLE()

RestoreProgressPanel::RestoreProgressPanel(
	ClientConfig*     pConfig,
	ServerConnection* pConnection,
	wxWindow* parent, 
	wxWindowID id,
	const wxPoint& pos, 
	const wxSize& size,
	long style, 
	const wxString& name)
	: wxPanel(parent, id, pos, size, style, name),
	  mpConfig(pConfig),
	  mpConnection(pConnection),
	  mRestoreRunning(false)
{
	wxSizer* pMainSizer = new wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer* pSummaryBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Summary"));
	pMainSizer->Add(pSummaryBox, 0, wxGROW | wxALL, 8);
	
	wxSizer* pSummarySizer = new wxGridSizer(1, 2, 0, 4);
	pSummaryBox->Add(pSummarySizer, 0, wxGROW | wxALL, 4);
	
	mpSummaryText = new wxStaticText(this, wxID_ANY, 
		wxT("Restore not started yet"));
	pSummarySizer->Add(mpSummaryText, 0, wxALIGN_CENTER_VERTICAL, 0);
	
	mpProgressGauge = new wxGauge(this, wxID_ANY, 100);
	mpProgressGauge->SetValue(0);
	pSummarySizer->Add(mpProgressGauge, 0, wxALIGN_CENTER_VERTICAL | wxGROW, 0);
	mpProgressGauge->Hide();
	
	wxStaticBoxSizer* pCurrentBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Current Action"));
	pMainSizer->Add(pCurrentBox, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpCurrentText = new wxStaticText(this, wxID_ANY, 
		wxT("Idle (nothing to do)"));
	pCurrentBox->Add(mpCurrentText, 0, wxGROW | wxALL, 4);
	
	wxStaticBoxSizer* pErrorsBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Errors"));
	pMainSizer->Add(pErrorsBox, 1, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	mpErrorList = new wxListBox(this, ID_BackupProgress_ErrorList);
	pErrorsBox->Add(mpErrorList, 1, wxGROW | wxALL, 4);

	wxStaticBoxSizer* pStatsBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Statistics"));
	pMainSizer->Add(pStatsBox, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	wxFlexGridSizer* pStatsGrid = new wxFlexGridSizer(4, 4, 4);
	pStatsBox->Add(pStatsGrid, 1, wxGROW | wxALL, 4);

	pStatsGrid->AddGrowableCol(0);
	pStatsGrid->AddGrowableCol(1);
	pStatsGrid->AddGrowableCol(2);
	pStatsGrid->AddGrowableCol(3);

	pStatsGrid->AddSpacer(1);
	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, wxT("Elapsed")));
	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, wxT("Remaining")));
	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, wxT("Total")));

	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, wxT("Files")));

	mpNumFilesDone = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumFilesDone, 1, wxGROW, 0);
	
	mpNumFilesRemaining = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumFilesRemaining, 1, wxGROW, 0);
	
	mpNumFilesTotal = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumFilesTotal, 1, wxGROW, 0);

	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, wxT("Bytes")));

	mpNumBytesDone = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumBytesDone, 1, wxGROW, 0);
	
	mpNumBytesRemaining = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumBytesRemaining, 1, wxGROW, 0);
	
	mpNumBytesTotal = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumBytesTotal, 1, wxGROW, 0);

	wxSizer* pButtonSizer = new wxBoxSizer(wxHORIZONTAL);
	pMainSizer->Add(pButtonSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpStopCloseButton = new wxButton(this, wxID_CANCEL, wxT("Close"));
	pButtonSizer->Add(mpStopCloseButton, 0, wxGROW, 0);

	mRestoreRunning = false;
	mRestoreStopRequested = false;
	mNumFilesCounted = 0;
	mNumBytesCounted = 0;

	SetSizer(pMainSizer);
}

void RestoreProgressPanel::StartRestore(const RestoreSpec& rSpec)
{
	RestoreSpec spec(rSpec);
	
	mpErrorList->Clear();

	wxString errorMsg;
	if (!mpConnection->InitTlsContext(mTlsContext, errorMsg))
	{
		wxString msg;
		msg.Printf(wxT("Error: cannot start restore: %s"), errorMsg.c_str());
		ReportRestoreFatalError(BM_BACKUP_FAILED_CANNOT_INIT_ENCRYPTION, msg);
		return;
	}
	
	std::string storeHost;
	mpConfig->StoreHostname.GetInto(storeHost);

	if (storeHost.length() == 0) 
	{
		ReportRestoreFatalError(BM_BACKUP_FAILED_NO_STORE_HOSTNAME,
			wxT("Error: cannot start restore: "
			"You have not configured the Store Hostname!"));
		return;
	}

	std::string keysFile;
	mpConfig->KeysFile.GetInto(keysFile);

	if (keysFile.length() == 0) 
	{
		ReportRestoreFatalError(BM_BACKUP_FAILED_NO_KEYS_FILE,
			wxT("Error: cannot start restore: "
			"you have not configured the Keys File"));
		return;
	}

	int acctNo;
	if (!mpConfig->AccountNumber.GetInto(acctNo))
	{
		ReportRestoreFatalError(BM_BACKUP_FAILED_NO_ACCOUNT_NUMBER,
			wxT("Error: cannot start restore: "
			"you have not configured the Account Number"));
		return;
	}
	
	BackupClientCryptoKeys_Setup(keysFile.c_str());
	
	mNumFilesCounted = 0;
	mNumBytesCounted = 0;
	mNumFilesDownloaded = 0;
	mNumBytesDownloaded = 0;
	
	mpSummaryText->SetLabel(wxT("Starting Restore"));
	mRestoreRunning = true;
	mRestoreStopRequested = false;
	mpStopCloseButton->SetLabel(wxT("Stop Restore"));
	mpNumFilesTotal->SetValue(wxT(""));
	mpNumBytesTotal->SetValue(wxT(""));
	mpNumFilesDone->SetValue(wxT(""));
	mpNumBytesDone->SetValue(wxT(""));
	mpNumFilesRemaining->SetValue(wxT(""));
	mpNumBytesRemaining->SetValue(wxT(""));
	Layout();
	wxYield();
	
	try 
	{
		// count files to restore
		RestoreSpecEntry::Vector entries = spec.GetEntries();
		for (RestoreSpecEntry::ConstIterator i = entries.begin();
			i != entries.end(); i++)
		{
			ServerCacheNode* pNode = i->GetNode();
			CountFilesRecursive(spec, pNode);
		}
		
		mpProgressGauge->SetRange(mNumFilesCounted);
		mpProgressGauge->SetValue(0);
		mpProgressGauge->Show();

		mpSummaryText->SetLabel(_("Restoring files"));
		wxYield();
		
		// Go through the records agains, this time syncing them
		
		if (mRestoreStopRequested)
		{
			mpSummaryText->SetLabel(wxT("Restore Interrupted"));
			ReportRestoreFatalError(BM_BACKUP_FAILED_INTERRUPTED,
				wxT("Restore interrupted by user"));
			mpSummaryText->SetLabel(wxT("Restore Interrupted"));
		}
		else
		{
			mpSummaryText->SetLabel(wxT("Restore Finished"));
			mpErrorList  ->Append  (wxT("Restore Finished"));
		}
		
		mpProgressGauge->Hide();
	}
	catch (ConnectionException& e) 
	{
		mpSummaryText->SetLabel(wxT("Restore Failed"));
		wxString msg;
		msg.Printf(_("Error: cannot start restore: "
			"Failed to connect to server: %s"),
			wxString(e.what(), wxConvLibc).c_str());
		ReportRestoreFatalError(BM_BACKUP_FAILED_CONNECT_FAILED, msg);
	}
	catch (std::exception& e) 
	{
		mpSummaryText->SetLabel(_("Restore Failed"));
		wxString msg;
		msg.Printf(_("Error: failed to finish restore: %s"),
			wxString(e.what(), wxConvLibc).c_str());
		ReportRestoreFatalError(BM_BACKUP_FAILED_UNKNOWN_ERROR, msg);
	}
	catch (...)
	{
		mpSummaryText->SetLabel(wxT("Restore Failed"));
		ReportRestoreFatalError(BM_BACKUP_FAILED_UNKNOWN_ERROR,
			wxT("Error: failed to finish restore: unknown error"));
	}	

	mpCurrentText->SetLabel(wxT("Idle (nothing to do)"));
	mRestoreRunning = FALSE;
	mRestoreStopRequested = FALSE;
	mpStopCloseButton->SetLabel(wxT("Close"));
}

void CountFilesRecursive(const RestoreSpec& rSpec, ServerFileNode* pNode)
{
	ServerFileVersion* pVersion = pNode->GetMostRecent();
	if (pVersion->IsDirectory())
	{
		
				NotifyMoreFilesCounted(
