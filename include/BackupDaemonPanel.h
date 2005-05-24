/***************************************************************************
 *            BackupDaemonPanel.h
 *
 *  Mon Apr  4 20:35:39 2005
 *  Copyright  2005  Chris Wilson
 *  Email <boxi_BackupDaemonPanel.h@qwirx.com>
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
 
#ifndef _BACKUPDAEMONPANEL_H
#define _BACKUPDAEMONPANEL_H

#include <wx/wx.h>
#include <wx/thread.h>

#include "ClientConfig.h"
#include "ClientConnection.h"

/** 
 * BackupDaemonPanel
 * Allows the user to start and stop the bbackupd daemon.
 */

class BackupDaemonPanel 
: public wxPanel,
  private ClientConnection::Listener 
{
	public:
	BackupDaemonPanel(
		ClientConfig *pConfig,
		const wxString& rExecutablePath,
		wxWindow* parent, wxWindowID id = -1,
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL, 
		const wxString& name = "Backup Daemon Panel");
	// ~BackupDaemonPanel();

	virtual void NotifyStateChange();
	virtual void NotifyError();
	virtual void NotifyClientStateChange();
	
	private:
	ClientConfig* mpConfig;
	int  mDaemonProcessID;
	bool mDaemonIsRunning;
	wxTextCtrl* mpClientConnStatus;
	wxTextCtrl* mpClientError;
	wxTextCtrl* mpClientState;
	ClientConnection mClientConn;
	wxButton* mpStartButton;
	wxButton* mpStopButton;
	wxButton* mpRestartButton;
	wxButton* mpKillButton;
	wxButton* mpSyncButton;
	wxButton* mpReloadButton;
	
	void OnTimer(wxTimerEvent& event);	
	void OnClientStartClick(wxCommandEvent& event) { 
		mClientConn.StartClient(); 
		EnableButtons();
	}
	void OnClientStopClick(wxCommandEvent& event) {
		mClientConn.StopClient(); 
		EnableButtons();
	}
	void OnClientKillClick(wxCommandEvent& event) {
		if (mClientConn.KillClient()) {
			EnableButtons();
			return;
		}
		wxLogError(mClientConn.GetLastErrorMsg());
	}
	void OnClientRestartClick(wxCommandEvent& event) { 
		if (mClientConn.RestartClient()) {
			EnableButtons();
			return;
		}
		wxLogError(mClientConn.GetLastErrorMsg());
	}
	void OnClientSyncClick(wxCommandEvent& event) { 
		if (mClientConn.SyncClient()) {
			EnableButtons();
			return;
		}
		wxLogError(mClientConn.GetLastErrorMsg());
	}
	void OnClientReloadClick(wxCommandEvent& event) { 
		if (mClientConn.ReloadClient()) {
			EnableButtons();
			return;
		}
		wxLogError(mClientConn.GetLastErrorMsg());
	}
	void OnClientNotify(wxCommandEvent& event) {
		HandleClientEvent();
	}

	void HandleClientEvent();
	void SendClientEvent();
	void EnableButtons();
	
	DECLARE_EVENT_TABLE()
};

#endif /* _BACKUPDAEMONPANEL_H */
