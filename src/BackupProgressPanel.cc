/***************************************************************************
 *            BackupProgressPanel.cc
 *
 *  Mon Apr  4 20:36:25 2005
 *  Copyright  2005  Chris Wilson
 *  Email <boxi_BackupProgressPanel.cc@qwirx.com>
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
#include <wx/gauge.h>

#define NDEBUG
#include "Box.h"
#include "BackupClientContext.h"
#include "BackupClientDirectoryRecord.h"
#include "BackupClientCryptoKeys.h"
#undef NDEBUG

#include "main.h"
#include "BackupProgressPanel.h"
#include "ServerConnection.h"

//DECLARE_EVENT_TYPE(myEVT_CLIENT_NOTIFY, -1)
//DEFINE_EVENT_TYPE(myEVT_CLIENT_NOTIFY)

BEGIN_EVENT_TABLE(BackupProgressPanel, wxPanel)
//EVT_BUTTON(ID_Daemon_Start,   BackupProgressPanel::OnClientStartClick)
//EVT_BUTTON(ID_Daemon_Stop,    BackupProgressPanel::OnClientStopClick)
//EVT_BUTTON(ID_Daemon_Restart, BackupProgressPanel::OnClientRestartClick)
//EVT_BUTTON(ID_Daemon_Kill,    BackupProgressPanel::OnClientKillClick)
//EVT_BUTTON(ID_Daemon_Sync,    BackupProgressPanel::OnClientSyncClick)
//EVT_BUTTON(ID_Daemon_Reload,  BackupProgressPanel::OnClientReloadClick)
//EVT_COMMAND(wxID_ANY, myEVT_CLIENT_NOTIFY, BackupProgressPanel::OnClientNotify)
END_EVENT_TABLE()

BackupProgressPanel::BackupProgressPanel(
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
	  mBackupRunning(FALSE)
{
	wxSizer* pMainSizer = new wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer* pSummaryBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Summary"));
	pMainSizer->Add(pSummaryBox, 0, wxGROW | wxALL, 8);
	
	wxSizer* pSummarySizer = new wxGridSizer(1, 2, 0, 8);
	pSummaryBox->Add(pSummarySizer, 0, wxGROW | wxALL, 8);
	
	wxStaticText* pSummaryText = new wxStaticText(this, wxID_ANY, 
		wxT("Backup not started yet"));
	pSummarySizer->Add(pSummaryText, 0, wxALIGN_CENTER_VERTICAL, 0);
	
	wxGauge* pProgressGauge = new wxGauge(this, wxID_ANY, 100);
	pSummarySizer->Add(pProgressGauge, 0, wxALIGN_CENTER_VERTICAL | wxGROW, 0);
	
	wxStaticBoxSizer* pCurrentBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Current Action"));
	pMainSizer->Add(pCurrentBox, 0, wxGROW | wxALL, 8);

	wxStaticText* pCurrentText = new wxStaticText(this, wxID_ANY, 
		wxT("Backing up file /foo/bar"));
	pCurrentBox->Add(pCurrentText, 0, wxGROW | wxALL, 8);
	
	wxStaticBoxSizer* pErrorsBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Errors"));
	pMainSizer->Add(pErrorsBox, 1, wxGROW | wxALL, 8);
	
	wxListBox* pErrorList = new wxListBox(this, wxID_ANY);
	pErrorsBox->Add(pErrorList, 1, wxGROW | wxALL, 8);

	wxStaticBoxSizer* pStatsBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Statistics"));
	pMainSizer->Add(pStatsBox, 0, wxGROW | wxALL, 8);
	
	SetSizer( pMainSizer );
	
	StartBackup();
}

void BackupProgressPanel::StartBackup()
{
	wxString errorMsg;
	if (!mpConnection->InitTlsContext(mTlsContext, errorMsg))
	{
		wxString msg2;
		msg2.Printf(wxT("Error: cannot start backup: %s"), errorMsg.c_str());
		wxMessageBox(msg2, wxT("Boxi Error"), wxOK | wxICON_ERROR, this);
		return;
	}
	
	std::string storeHost;
	mpConfig->StoreHostname.GetInto(storeHost);

	if (storeHost.length() == 0) 
	{
		wxMessageBox(wxT("You have not configured the Store Hostname!"), 
			wxT("Boxi Error"), wxOK | wxICON_ERROR, this);
		return;
	}

	std::string keysFile;
	mpConfig->KeysFile.GetInto(keysFile);

	if (keysFile.length() == 0) 
	{
		wxString msg = wxT("Error: cannot start backup: "
			"you have not configured the Keys File");
		wxMessageBox(msg, wxT("Boxi Error"), wxOK | wxICON_ERROR, this);
		return;
	}

	int acctNo;
	if (!mpConfig->AccountNumber.GetInto(acctNo))
	{
		wxString msg = wxT("Error: cannot start backup: "
			"you have not configured the Account Number");
		wxMessageBox(msg, wxT("Boxi Error"), wxOK | wxICON_ERROR, this);
		return;
	}
	
	BackupClientCryptoKeys_Setup(keysFile.c_str());

	BackupClientContext clientContext(*this, mTlsContext, storeHost,
		acctNo, TRUE);

	// The minimum age a file needs to be, 
	// before it will be considered for uploading
	int minimumFileAgeSecs;
	if (!mpConfig->MinimumFileAge.GetInto(minimumFileAgeSecs))
	{
		wxMessageBox(wxT("You have not configured the minimum file age"), 
			wxT("Boxi Error"), wxOK | wxICON_ERROR, this);
		return;
	}
	box_time_t minimumFileAge = SecondsToBoxTime((uint32_t)minimumFileAgeSecs);

	// The maximum time we'll wait to upload a file, 
	// regardless of how often it's modified
	int maxUploadWaitSecs;
	if (!mpConfig->MaxUploadWait.GetInto(maxUploadWaitSecs))
	{
		wxMessageBox(wxT("You have not configured the maximum upload wait"), 
			wxT("Boxi Error"), wxOK | wxICON_ERROR, this);
		return;
	}
	box_time_t maxUploadWait = SecondsToBoxTime((uint32_t)maxUploadWaitSecs);

	// Adjust by subtracting the minimum file age, 
	// so is relative to sync period end in comparisons
	maxUploadWait = (maxUploadWait > minimumFileAge)
		? (maxUploadWait - minimumFileAge) : (0);

	// Set up the sync parameters
	BackupClientDirectoryRecord::SyncParams params(*this, *this,
		clientContext);
	params.mMaxUploadWait = maxUploadWait;
	
	if (!mpConfig->FileTrackingSizeThreshold.GetInto(
		params.mFileTrackingSizeThreshold))
	{
		wxMessageBox(wxT("You have not configured the "
			"file tracking size threshold"), 
			wxT("Boxi Error"), wxOK | wxICON_ERROR, this);
		return;
	}

	if (!mpConfig->DiffingUploadSizeThreshold.GetInto(
		params.mDiffingUploadSizeThreshold))
	{
		wxMessageBox(wxT("You have not configured the "
			"diffing upload size threshold"), 
			wxT("Boxi Error"), wxOK | wxICON_ERROR, this);
		return;
	}

	unsigned int maxFileTimeInFutureSecs;
	if (!mpConfig->MaxFileTimeInFuture.GetInto(maxFileTimeInFutureSecs))
	{
		wxMessageBox(wxT("You have not configured the "
			"maximum file time in future threshold"), 
			wxT("Boxi Error"), wxOK | wxICON_ERROR, this);
		return;
	}
	params.mMaxFileTimeInFuture = SecondsToBoxTime(
		(uint32_t)maxFileTimeInFutureSecs);
	
	params.mpCommandSocket = NULL;
	
	// Set store marker
	clientContext.SetClientStoreMarker(clientStoreMarker);
}
