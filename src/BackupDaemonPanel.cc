/***************************************************************************
 *            BackupDaemonPanel.cc
 *
 *  Mon Apr  4 20:36:25 2005
 *  Copyright  2005  Chris Wilson
 *  Email <boxi_BackupDaemonPanel.cc@qwirx.com>
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

#include <wx/html/htmlwin.h>

#include "main.h"
#include "BackupDaemonPanel.h"

DECLARE_EVENT_TYPE(myEVT_CLIENT_NOTIFY, -1)
DEFINE_EVENT_TYPE(myEVT_CLIENT_NOTIFY)

BEGIN_EVENT_TABLE(BackupDaemonPanel, wxPanel)
EVT_BUTTON(ID_Daemon_Start,   BackupDaemonPanel::OnClientStartClick)
EVT_BUTTON(ID_Daemon_Stop,    BackupDaemonPanel::OnClientStopClick)
EVT_BUTTON(ID_Daemon_Restart, BackupDaemonPanel::OnClientRestartClick)
EVT_BUTTON(ID_Daemon_Kill,    BackupDaemonPanel::OnClientKillClick)
EVT_BUTTON(ID_Daemon_Sync,    BackupDaemonPanel::OnClientSyncClick)
EVT_BUTTON(ID_Daemon_Reload,  BackupDaemonPanel::OnClientReloadClick)
EVT_COMMAND(wxID_ANY, myEVT_CLIENT_NOTIFY, BackupDaemonPanel::OnClientNotify)
END_EVENT_TABLE()

BackupDaemonPanel::BackupDaemonPanel(
	ClientConfig *pConfig,
	const wxString& rExecutablePath,
	wxWindow* parent, 
	wxWindowID id,
	const wxPoint& pos, 
	const wxSize& size,
	long style, 
	const wxString& name)
	: wxPanel(parent, id, pos, size, style, name),
	  mpConfig(pConfig),
	  mClientConn(pConfig, rExecutablePath, this)
{
	wxSizer* pMainSizer = new wxBoxSizer( wxVERTICAL );

	wxSizer* pParamSizer = new wxGridSizer(2, 4, 4);
	
	wxTextCtrl* pBoxLocationCtrl = new wxTextCtrl(this, -1);
	AddParam(this, wxT("Client Location:"), pBoxLocationCtrl, TRUE,
		pParamSizer);
	
	mpClientConnStatus = new wxTextCtrl(this, -1, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	AddParam(this, wxT("Connection to Client:"), mpClientConnStatus, TRUE,
		pParamSizer);

	mpClientError = new wxTextCtrl(this, -1, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	AddParam(this, wxT("Connection Error:"), mpClientError , TRUE,
		pParamSizer);

	mpClientState = new wxTextCtrl(this, -1, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	AddParam(this, wxT("Client State:"), mpClientState, TRUE,
		pParamSizer);

	pMainSizer->Add(pParamSizer, 0, wxGROW | wxALL, 8);
	
	// wxFlexGridSizer* pButtonSizer = new wxFlexGridSizer(2, 4, 4);
	// pButtonSizer->AddGrowableCol(1);

	wxSizer* pButtonSizer = new wxGridSizer(6, 4, 4);

	/*
	wxHtmlWindow* pStartText = new wxHtmlWindow(this, -1, 
		wxDefaultPosition, wxDefaultSize, wxHW_SCROLLBAR_NEVER);
	pStartText->SetPage("Start the Backup Client");
	pMainSizer->Add(pStartText, 0, wxGROW, 0);
	*/

	mpStartButton = new wxButton(this, ID_Daemon_Start, wxT("Start"));
	pButtonSizer->Add(mpStartButton, 0, wxGROW, 0);
	
	mpStopButton = new wxButton(this, ID_Daemon_Stop, wxT("Stop"));
	pButtonSizer->Add(mpStopButton, 0, wxGROW);
	
	mpRestartButton = new wxButton(this, ID_Daemon_Restart, wxT("Restart"));
	pButtonSizer->Add(mpRestartButton, 0, wxGROW);

	mpKillButton = new wxButton(this, ID_Daemon_Kill, wxT("Kill"));
	pButtonSizer->Add(mpKillButton, 0, wxGROW);

	mpSyncButton = new wxButton(this, ID_Daemon_Sync, wxT("Sync"));
	pButtonSizer->Add(mpSyncButton, 0, wxGROW);

	mpReloadButton = new wxButton(this, ID_Daemon_Reload, wxT("Reload"));
	pButtonSizer->Add(mpReloadButton, 0, wxGROW);

	pMainSizer->Add(pButtonSizer, 0, wxGROW | wxALL, 8);

	SetSizer( pMainSizer );
	pMainSizer->SetSizeHints( this );
	
	wxString DaemonPath;
	/*
	if (mClientConn.GetClientBinaryPath(DaemonPath))
	{
		pBoxLocationCtrl->SetValue(DaemonPath);
	}
	*/

	HandleClientEvent();
}

void BackupDaemonPanel::HandleClientEvent() {
	mpClientConnStatus->SetValue(
		wxString(mClientConn.GetStateStr(), wxConvBoxi));
	mpClientError->SetValue(mClientConn.GetLastErrorMsg());

	wxString ClientState;
	ClientState.Printf(wxT("%s (pid %ld)"), 
		mClientConn.GetClientStateString(),
		mClientConn.GetClientPidFast());
	mpClientState->SetValue(ClientState);
	EnableButtons();
}

void BackupDaemonPanel::SendClientEvent() {
	wxCommandEvent event( myEVT_CLIENT_NOTIFY, GetId() );
	event.SetEventObject( this );
	GetEventHandler()->AddPendingEvent( event );
}

void BackupDaemonPanel::NotifyStateChange() {
	SendClientEvent();
}

void BackupDaemonPanel::NotifyClientStateChange() {
	SendClientEvent();
}

void BackupDaemonPanel::NotifyError() {
	SendClientEvent();
}

void BackupDaemonPanel::EnableButtons()
{
	long pid = mClientConn.GetClientPidFast();
	int CurrentState = mClientConn.GetState();
	mpStartButton   ->Enable(
		CurrentState == ClientConnection::BST_CONNECTING &&	pid == -1);
	mpStopButton    ->Enable(CurrentState == ClientConnection::BST_CONNECTED);
	mpRestartButton ->Enable(CurrentState == ClientConnection::BST_CONNECTED);
	mpKillButton    ->Enable(pid != -1);
	mpSyncButton    ->Enable(CurrentState == ClientConnection::BST_CONNECTED);
	mpReloadButton  ->Enable(CurrentState == ClientConnection::BST_CONNECTED);
}
