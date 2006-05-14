/***************************************************************************
 *            ClientConnection.h
 *
 *  Sat Apr  9 21:09:15 2005
 *  Copyright  2005  Chris Wilson
 *  Email boxi_ClientConnection.h@qwirx.com
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
 
#ifndef _CLIENTCONNECTION_H
#define _CLIENTCONNECTION_H

#include <iostream>

#include <wx/process.h>
#include <wx/log.h>
#include <wx/thread.h>

#define NDEBUG
#include "Socket.h"
#include "IOStreamGetLine.h"
#include "SocketStream.h"
#undef NDEBUG

#include "ClientConfig.h"

static const wxChar* mStateStrings[] = {
	wxT("Unknown"),
	wxT("Connecting"),
	wxT("Connected"),
	wxT("Start client requested"),
	wxT("Starting client"),
	wxT("Waiting for client to start"),
	wxT("Stop client requested"),
	wxT("Stopping client"),
	wxT("Restart client requested"),
	wxT("Stopping client for restart"),
	wxT("Client sync requested"),
	wxT("Asking client to sync now"),
	wxT("Client reload requested"),
	wxT("Asking client to reload"),
	wxT("Shutting down worker"),
};

static const wxChar* mErrorStrings[] = {
	wxT("None"),
	wxT("Unknown error"),
	wxT("In progress"),
	wxT("Client started successfully"),
	wxT("Connected to Client"),
	wxT("Invalid State for Request"),
	wxT("No socket configured"),
	wxT("Socket not found"),
	wxT("Socket connection refused"),
	wxT("No response from client"),
	wxT("Bad response from client"),
	wxT("Read from client timed out"),
	wxT("Disconnected by client"),
	wxT("Daemon executable not found"),
	wxT("File not found"),
	wxT("Executing program failed"),
	wxT("Client failed to start successfully"),
	wxT("PID File not configured"),
	wxT("PID File corrupted"),
	wxT("Insufficient system resources"),
	wxT("Failed to identify process"),
	wxT("Internal error"),
	wxT("Kill not allowed"),
	wxT("Last client command failed"),
	wxT("Command interrupted"),
};

static const wxChar* mClientStateStrings[] = {
	wxT("Unknown"),
	wxT("Initialising"),
	wxT("Idle"),
	wxT("Connected to Store"),
	wxT("Sync failed, sleeping"),
	wxT("Store limit exceeded, sleeping"),
};

class ClientConnection : public wxThread
{
	public:
	class Listener {
		public:
		virtual void NotifyStateChange() = 0;
		virtual void NotifyError() = 0;
		virtual void NotifyClientStateChange() = 0;
		virtual ~Listener() {}
	};

	enum WorkerState {
		BST_UNKNOWN = 0,
		BST_CONNECTING,
		BST_CONNECTED,
		BST_START,
		BST_STARTING,
		BST_STARTWAIT,
		BST_STOP,
		BST_STOPPING,
		BST_RESTART,
		BST_RESTARTING,
		BST_SYNC,
		BST_SYNCING,
		BST_RELOAD,
		BST_RELOADING,
		BST_SHUTDOWN,
	};
	typedef enum WorkerState WorkerState;
	
	enum Error {
		ERR_NONE = 0,
		ERR_UNKNOWN,
		ERR_INPROGRESS,
		ERR_STARTED,
		ERR_CONNECTED,
		ERR_INVALID_STATE,
		ERR_NOSOCKETCONFIG,
		ERR_SOCKETNOTFOUND,
		ERR_SOCKETREFUSED,
		ERR_NORESPONSE,
		ERR_BADRESPONSE,
		ERR_READTIMEOUT,
		ERR_DISCONNECTED,
		ERR_FILEUNKNOWN,
		ERR_FILENOTFOUND,
		ERR_EXECFAILED,
		ERR_STARTFAILED,
		ERR_NOPIDCONFIG,
		ERR_PIDFORMAT,
		ERR_RESOURCES,
		ERR_PROCESSNOTFOUND,
		ERR_INTERNAL,
    	ERR_ACCESSDENIED,
		ERR_CMDFAILED,
		ERR_INTERRUPTED,
	};
	typedef enum Error Error;
	
	enum ClientState {
		CS_UNKNOWN = -2,
		CS_INIT = -1,
		CS_IDLE = 0,
		CS_CONNECTED = 1,
		CS_ERROR = 2,
		CS_STORELIMIT = 3
	};
	typedef enum ClientState ClientState;
	
	private:
	ClientConfig* mpConfig;
	Listener*     mpListener;
	wxMutex       mMutex; 
	WorkerState   mCurrentState;
	ClientState   mClientState;
	Error         mLastError;
	wxString      mExecutablePath;
	int           mClientPid;
	
	public:
	ClientConnection(ClientConfig* pConfig, 
					 const wxString& rExecutablePath,
					 Listener* pListener);

	~ClientConnection();
	
	WorkerState GetState() {
		wxMutexLocker lock(mMutex);
		return mCurrentState;
	}

	const wxChar* GetStateStr() {
		return mStateStrings[GetState()];
	}

	const wxChar* GetStateStr(WorkerState state) {
		return mStateStrings[state];
	}
	
	Error GetLastErrorCode() {
		wxMutexLocker lock(mMutex);
		return mLastError;
	}
	
	const wxChar* GetLastErrorMsg() {
		return mErrorStrings[GetLastErrorCode()];
	}

	ClientState GetClientState() {
		wxMutexLocker lock(mMutex);
		return mClientState;
	}
	
	const wxChar* GetClientStateString() {
		return mClientStateStrings[GetClientState() + 2];
	}
	
	bool StartClient();
	bool StopClient();
	bool RestartClient();
	bool KillClient();
	bool SyncClient();
	bool ReloadClient();
	bool GetClientBinaryPath(wxString& rDestPath);
	long GetClientPidFast(); // returns -1 if unknown
	Error GetClientPidSlow(long& rDestPid);

	private:
	bool SetWorkerState(WorkerState newState, 
						WorkerState mOldState, const char *cmd);
	Error _GetClientPidSlow(long& rDestPid);
	
	virtual void * Entry();

	void OnStartClient();
	void OnStopClient();
	Error DoStartClient();
	Error DoReadLine();
	Error DoConnect();
	void OnConnect();
	void OnConnectedIdle();
	void OnRestartClient();
	void OnSyncClient();
	void OnReloadClient();

	bool _sendCommand(const char * cmd) 
	{
		mpSocket->Write(cmd, strlen(cmd));
		char *newline = "\n";
		mpSocket->Write(newline, strlen(newline));
		
		wxString cmd2(cmd, wxConvLibc);
		wxLogDebug(wxT("wrote to daemon: '%s'"), cmd2.c_str());
		return TRUE;
	}

	class ClientProcess : public wxProcess 
	{
		public:
		ClientProcess(ClientConnection* pClientConn) {
			mpClientConn = pClientConn;
		}
		
		private:
		ClientConnection* mpClientConn;
		
		void OnTerminate(int pid, int status)
		{
			mpClientConn->OnClientTerminate(this, pid, status);
			wxProcess::OnTerminate(pid, status);
		}
	};

	// only for use in worker thread
	SocketStream* mpSocket;
	std::auto_ptr<IOStreamGetLine> mapReader;
	std::auto_ptr<ClientProcess> mapBackupClientProcess;
	friend class ClientDaemonProcess;
	void OnClientTerminate(wxProcess* pProcess, int pid, int status);
	void LogProcessOutput(wxProcess* pProcess);
};

#endif /* _CLIENTCONNECTION_H */
