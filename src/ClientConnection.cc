/***************************************************************************
 *            ClientConnection.cc
 *
 *  Sat Apr  9 21:09:55 2005
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
 *
 * Contains software developed by Ben Summers
 */

#include <iostream>

#include <wx/file.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/process.h>
#include <wx/tokenzr.h>
#include <wx/txtstrm.h>

#include "SandBox.h"
#include "ClientConnection.h"
#include "BoxException.h"

ClientConnection::ClientConnection(ClientConfig* pConfig, 
				 const wxString& rExecutablePath,
				 Listener* pListener)
: wxThread(wxTHREAD_JOINABLE)
{
	mpConfig = pConfig;
	mpListener = pListener;
	mLastError = ERR_NONE;
	mClientState = CS_UNKNOWN;
	mCurrentState = BST_CONNECTING;
	mExecutablePath = rExecutablePath;
	mClientPid = -1;
	Create();
	Run();
}

ClientConnection::~ClientConnection() 
{
	// Delete();
	// Wait();
}

// temporary hack since the old GetClientBinaryPath doesn't compile
void* ClientConnection::Entry() { }

/*
void* ClientConnection::Entry()
{
	wxLogDebug(wxT("Client worker thread starting"));

	// unlocked use of mCurrentState, might possibly be dangerous?
	while (mCurrentState != BST_SHUTDOWN)
	{
		WorkerState PrevWorkerState;
		ClientState PrevClientState;
		
		{
			wxMutexLocker lock(mMutex);
			PrevWorkerState = mCurrentState;
			PrevClientState = mClientState;
			mLastError = ERR_NONE;
		}
		
		// wxLogDebug("Current worker state: %s", GetStateStr(PrevState));
		
		try {
			switch (PrevWorkerState)
			{
				case BST_CONNECTING:
					OnConnect(); break;
				case BST_CONNECTED:
				case BST_RESTARTING:
					OnConnectedIdle(); break;
				case BST_START:
					OnStartClient(); break;
				case BST_STOP:
					OnStopClient(); break;
				case BST_RESTART:
					OnRestartClient(); break;
				case BST_SYNC:
					OnSyncClient(); break;
				case BST_RELOAD:
					OnReloadClient(); break;
				case BST_SHUTDOWN:
					break;
				default:
					wxLogError(
						wxT("Unknown client worker "
							"state %d"), 
						PrevWorkerState);
			}
		} catch (...) {
			wxLogError(wxT("Caught exception"));
		}

		{
			long Dummy;
			GetClientPidSlow(Dummy);
		}

		{
			wxMutexLocker lock(mMutex);
		
			if (mapBackupClientProcess.get() &&
				(mapBackupClientProcess->IsInputAvailable() ||
				mapBackupClientProcess->IsErrorAvailable()))
			{
				LogProcessOutput(mapBackupClientProcess.get());
			}
		}

		if (TestDestroy()) {
			wxMutexLocker lock(mMutex);
			mCurrentState = BST_SHUTDOWN;
			PrevWorkerState = mCurrentState;
			break;
		}
		
		if (mpListener)
		{
			if (mCurrentState != PrevWorkerState) {
				mpListener->NotifyStateChange();
				wxLogDebug(wxT(
					"Worker state change: '%s' to '%s'"),
					GetStateStr(PrevWorkerState),
					GetStateStr(mCurrentState));
			}
			if (mClientState != PrevClientState) {
				mpListener->NotifyClientStateChange();
			}
			mpListener->NotifyError();
			if (mLastError != ERR_NONE) {
				wxLogDebug(wxT("Worker error: '%s'"),
					GetLastErrorMsg());
			}
		}
			
		if (mCurrentState == BST_CONNECTING || 
			mCurrentState == BST_STARTWAIT)
		{
			Sleep(1000);
		}
	}
	
	wxLogDebug(wxT("Client worker thread shutdown"));
	return NULL;
}
*/

long ClientConnection::GetClientPidFast()
{
	wxMutexLocker lock(mMutex);
	return mClientPid;
}
    
ClientConnection::Error ClientConnection::GetClientPidSlow(long& rPid)
{
	ClientConnection::Error result =
		_GetClientPidSlow(rPid);
	
	wxMutexLocker lock(mMutex);
	if (result == ERR_NONE)
		mClientPid = rPid;
	else
		mClientPid = -1;
	
	return result;
}

ClientConnection::Error ClientConnection::_GetClientPidSlow(long& rPid)
{
	std::string PidFilePathStr;
	if (!mpConfig->PidFile.GetInto(PidFilePathStr))
		return ERR_NOPIDCONFIG;
	wxString PidFilePath(PidFilePathStr.c_str(), wxConvLibc);
		
	wxFileName DaemonPidFile(wxString(PidFilePath.c_str(), wxConvLibc));
	if (!wxFileName::FileExists(DaemonPidFile.GetFullPath()))
	{
		wxLogDebug(wxT("PID file not found (%s)"), 
			DaemonPidFile.GetFullPath().c_str());
		return ERR_FILENOTFOUND;
	}
	
	wxFile PidFile(PidFilePath);
	if (!PidFile.IsOpened())
	{
		wxLogDebug(wxT("Unable to read PID file (%s)"), 
			PidFilePath.c_str());
		return ERR_ACCESSDENIED;
	}
	
	int length = PidFile.Length();
	char *buffer = new char [length + 1];
	if (!buffer)
	{
		wxLogDebug(wxT("Out of memory reading PID file (%s)"), 
			PidFilePath.c_str());
		return ERR_RESOURCES;
	}
	memset(buffer, 0, length + 1);
	
	int BytesRead = PidFile.Read(buffer, length);
	if (BytesRead != length)
	{
		wxLogDebug(wxT("Short read on PID file (%s)"), 
			PidFilePath.c_str());
		delete[] buffer;
		return ERR_PIDFORMAT;
	}
	
	char *endptr;
	long ProcessID = ::strtol(buffer, &endptr, 10);
	if (ProcessID <= 0)
	{
		wxLogDebug(wxT("Invalid character code %d in PID file (%s)"),
			*endptr, PidFilePath.c_str());
		delete[] buffer;
		return ERR_PIDFORMAT;
	}

	delete[] buffer;
	
	wxKillError err = wxProcess::Kill(ProcessID, wxSIGNONE);

	switch (err)
	{
		case wxKILL_OK:
			rPid = ProcessID;
			return ERR_NONE;

		case wxKILL_BAD_SIGNAL: // no such signal
			wxLogDebug(wxT("No such signal wxSIGTERM?"));
			return ERR_INTERNAL;

		case wxKILL_ACCESS_DENIED: // permission denied
			// wxLogDebug("Failed to kill process: permission denied");
			rPid = ProcessID;
			return ERR_ACCESSDENIED;

		case wxKILL_NO_PROCESS: // no such process
			// wxLogDebug(wxT("Failed to kill process: 
			// no such process %ld"), ProcessID);
			return ERR_PROCESSNOTFOUND;
		
		case wxKILL_ERROR: // another, unspecified error
		default:
			wxLogDebug(wxT("Failed to kill process %ld: "
				"unspecified error"), ProcessID);
			return ERR_UNKNOWN;
	}			

	return ERR_UNKNOWN;
}

/*
	if (!wxProcess::Exists(ProcessID))
	{
		msg.Printf("Process %ld not running, may have crashed?", ProcessID);
		return FALSE;
	}
*/

void ClientConnection::OnConnect() {
	{
		wxMutexLocker lock(mMutex);
		assert(mCurrentState == BST_CONNECTING);
	}
	Error result = DoConnect();
	{
		wxMutexLocker lock(mMutex);
		if (mCurrentState != BST_CONNECTING) {
			wxLogDebug(wxT("Client connect interrupted"));
			return;
		}
		if (result == ERR_NONE)
			mCurrentState = BST_CONNECTED;
		if (result != ERR_INTERRUPTED)
			mLastError = result;
	}
}

ClientConnection::Error ClientConnection::DoConnect()
{
	std::string CommandSocket;
	if (!mpConfig->CommandSocket.GetInto(CommandSocket))
	{
		return ERR_NOSOCKETCONFIG;
	}

	mapReader->DetachFile();
	mapReader.reset();

	// should be freed by the IOStreamGetLine that controls it
	mpSocket = NULL;
	
	// apSocket is a smart pointer and should dispose of the
	// old object if we return before release()ing it to
	// the control of the IOStreamGetLine
	std::auto_ptr<SocketStream> apSocket(new SocketStream());

	// wxLogDebug("open socket to client");
	
	try
	{
		apSocket->Open(Socket::TypeUNIX, CommandSocket.c_str());
	}
	catch(...)
	{
		return ERR_SOCKETREFUSED;
	}

	{
		wxMutexLocker lock(mMutex);
		if (mCurrentState != BST_CONNECTING) {
			wxLogDebug(wxT("DoConnect interrupted"));
			return ERR_INTERRUPTED;
		}
	}
	
	mpSocket = apSocket.release();
	mapReader.reset(new IOStreamGetLine(*mpSocket));

	// Wait for the configuration summary
	std::string ConfigSummary;
	
	try {
		if (!mapReader->GetLine(ConfigSummary)) {
			wxLogDebug(wxT("Failed to read status from client"));
			return ERR_NORESPONSE;
		}
	} catch (...) {
		wxLogDebug(wxT("Exception while reading status from client"));
		return ERR_NORESPONSE;
	}

	{
		wxMutexLocker lock(mMutex);
		if (mCurrentState != BST_CONNECTING) {
			wxLogDebug(wxT("GetLine interrupted"));
			return ERR_INTERRUPTED;
		}
	}
	
	// Was the connection rejected by the server?
	if (mapReader->IsEOF())
		return ERR_DISCONNECTED;

	return ERR_NONE;
}

ClientConnection::Error ClientConnection::DoReadLine()
{
	std::string Line;
	bool result = FALSE;
	
	try {
		result = mapReader->GetLine(Line, 0, 1000);
	} catch (BoxException& be) {
		return ERR_DISCONNECTED;
	}
	
	if (!result)
		return ERR_READTIMEOUT;
	
	if (mapReader->IsEOF())
		return ERR_DISCONNECTED;

	if (Line == "ok")
		return ERR_NONE;
	
	if (Line == "error")
		return ERR_CMDFAILED;
	
	wxString line2(Line.c_str(), wxConvLibc);
	wxString rest;
	if (line2.StartsWith(wxT("state "), &rest)) 
	{
		long value;
		if (!rest.ToLong(&value))
			return ERR_BADRESPONSE;
		if (value >= CS_INIT && value <= CS_STORELIMIT)
		{
			mClientState = (ClientState)value;
		}
		else
		{
			mClientState = CS_UNKNOWN;
			return ERR_BADRESPONSE;
		}
	}

	wxLogDebug(wxT("read from daemon: '%s'"), line2.c_str());
	return ERR_NONE;
}

/*
bool ClientConnection::GetClientBinaryPath(wxString& rDestPath)
{
	// search for bbackupd
	wxPathList pl;
	pl.EnsureFileAccessible(mExecutablePath);
	wxFileName NativeBoxSourcePath(mExecutablePath);
	NativeBoxSourcePath.MakeAbsolute();

	const wxArrayString& dirs = NativeBoxSourcePath.GetDirs();
	if (dirs.Last().IsSameAs(wxT(".libs")))
		NativeBoxSourcePath.RemoveDir(
			NativeBoxSourcePath.GetDirCount() - 1);

	// dirs = NativeBoxSourcePath.GetDirs();	
	if (dirs.Last().IsSameAs(wxT("src")))
		NativeBoxSourcePath.RemoveDir(
			NativeBoxSourcePath.GetDirCount() - 1);

	NativeBoxSourcePath.AppendDir(wxT("boxbackup"));
	NativeBoxSourcePath.AppendDir(wxT("release"));
	NativeBoxSourcePath.AppendDir(wxT("bin"));
	NativeBoxSourcePath.AppendDir(wxT("bbackupd"));
	NativeBoxSourcePath.SetName(wxT(""));
	NativeBoxSourcePath.SetExt(wxT(""));
	pl.Add(NativeBoxSourcePath.GetFullPath());
	pl.AddEnvList(wxT("PATH"));
	
	wxLogDebug(wxT("Searching for boxbackup in:"));
    for ( wxStringListNode *node = pl.GetFirst(); node; node = node->GetNext() )
    {
		wxString string(node->GetData());
		wxLogDebug(string);
    }
	
	wxString BBackupdPath = 
#ifdef __CYGWIN__
		pl.FindValidPath(wxT("bbackupd.exe"));
#else
		pl.FindValidPath(wxT("bbackupd"));
#endif
	
	if (BBackupdPath.Length() == 0) {
		return FALSE;
	}
		
	rDestPath = BBackupdPath;
	return TRUE;
}
*/

void ClientConnection::OnConnectedIdle() {
	{
		wxMutexLocker lock(mMutex);
		assert(mCurrentState == BST_CONNECTED ||
			mCurrentState == BST_RESTARTING);
	}
	Error result = DoReadLine();
	{
		wxMutexLocker lock(mMutex);
		if (mCurrentState != BST_CONNECTED &&
			mCurrentState != BST_RESTARTING)
		{
			mLastError = ERR_INTERRUPTED;
			return;
		}
		if (result == ERR_DISCONNECTED) {
			mClientState = CS_UNKNOWN;
			if (mCurrentState == BST_CONNECTED)
				mCurrentState = BST_CONNECTING;
			else if (mCurrentState == BST_RESTARTING)
			{
				mCurrentState = BST_START;
				result = ERR_NONE;
			}
			else
				assert(FALSE);
		}
		if (result == ERR_READTIMEOUT)
			mLastError = ERR_NONE;
		else
			mLastError = result;
	}
}

/*
void ClientConnection::OnStartClient() {
	{
		wxMutexLocker lock(mMutex);
		assert(mCurrentState == BST_START);
		mCurrentState = BST_STARTING;
	}
	Error result = DoStartClient();
	{
		wxMutexLocker lock(mMutex);
		assert(mCurrentState == BST_STARTING);
		// no notification until forked process ends? don't wait for it!
		// mCurrentState = (result == ERR_NONE) ? BST_STARTWAIT : BST_CONNECTING;
		mCurrentState = BST_CONNECTING;
		mLastError = result;
	}
}
*/

/*
ClientConnection::Error ClientConnection::DoStartClient() {
	wxString ClientPath;
	if (!GetClientBinaryPath(ClientPath))
	{
		return ERR_FILEUNKNOWN;
	}
	
	if (ClientPath.Length() == 0)
	{
		return ERR_FILEUNKNOWN;
	}

	wxChar * path = wxStrdup(ClientPath);
	wxChar * config = wxStrdup(mpConfig->GetFileName());
	wxChar * argv[] = { path, config, NULL };

	{
		wxLogDebug(wxT("Start client: creating new process object"));
		wxMutexLocker lock(mMutex);
		// the old object will delete itself 
		// when the process terminates
		mapBackupClientProcess.release();
		mapBackupClientProcess = std::auto_ptr<ClientProcess>(
			new ClientProcess(this));
		mapBackupClientProcess->Redirect();
	}

	long result = wxExecute(argv, wxEXEC_ASYNC, 
		mapBackupClientProcess.get());

	free(path);
	free(config);

	if (result == 0)	
		return ERR_EXECFAILED;
	
	// the ClientProcess class will call our OnClientStarted method
	// when the BoxBackup master process terminates (and hopefully
	// it has forked into the background!)
	
	return ERR_NONE;
}
*/

void ClientConnection::OnStopClient() {
	{
		wxMutexLocker lock(mMutex);
		assert(mCurrentState == BST_STOP);
		mCurrentState = BST_STOPPING;
	}
	bool result = _sendCommand("terminate");
	{
		wxMutexLocker lock(mMutex);
		assert(mCurrentState == BST_STOPPING);
		mCurrentState = BST_CONNECTED;
		mLastError = result ? ERR_NONE : ERR_UNKNOWN;
	}
}

void ClientConnection::OnRestartClient() {
	{
		wxMutexLocker lock(mMutex);
		assert(mCurrentState == BST_RESTART);
		mCurrentState = BST_RESTARTING;
	}
	bool result = _sendCommand("terminate");
	{
		wxMutexLocker lock(mMutex);
		mLastError = result ? ERR_NONE : ERR_UNKNOWN;
	}
}

void ClientConnection::OnSyncClient() {
	{
		wxMutexLocker lock(mMutex);
		assert(mCurrentState == BST_SYNC);
		mCurrentState = BST_SYNCING;
	}
	bool result = _sendCommand("force-sync");
	{
		wxMutexLocker lock(mMutex);
		assert(mCurrentState == BST_SYNCING);
		mCurrentState = BST_CONNECTED;
		mLastError = result ? ERR_NONE : ERR_UNKNOWN;
	}
}

void ClientConnection::OnReloadClient() {
	{
		wxMutexLocker lock(mMutex);
		assert(mCurrentState == BST_RELOAD);
		mCurrentState = BST_RELOADING;
	}
	bool result = _sendCommand("reload");
	{
		wxMutexLocker lock(mMutex);
		assert(mCurrentState == BST_RELOADING);
		mCurrentState = BST_CONNECTED;
		mLastError = result ? ERR_NONE : ERR_UNKNOWN;
	}
}

void ClientConnection::LogProcessOutput(wxProcess* pProcess)
{
	{
		wxInputStream* pInput = pProcess->GetInputStream();
		wxTextInputStream InputText(*pInput);

		wxString line = InputText.ReadLine();
		while (line.Length() != 0) {
			wxLogWarning(line);
			line = InputText.ReadLine();
		}
	}

	{
		wxInputStream* pError = pProcess->GetErrorStream();
		wxTextInputStream ErrorText(*pError);

		wxString line = ErrorText.ReadLine();
		while (line.Length() != 0) {
			wxLogError(line);
			line = ErrorText.ReadLine();
		}
	}
}

// may be called in any thread, but probably main GUI thread
void ClientConnection::OnClientTerminate(wxProcess *pProcess, 
	int pid, int status)
{
	wxMutexLocker lock(mMutex);

	LogProcessOutput(pProcess);

	// the ClientProcess object is about to delete itself
	// (in wxProcess::OnTerminate)
	if (mapBackupClientProcess.get() == pProcess) {
		wxLogDebug(wxT("Client terminated, releasing process"));
		mapBackupClientProcess.release();
	} else {
		wxLogDebug(wxT("Client terminated but "
			"process already released!"));
	}
	
	if (mCurrentState != BST_STARTWAIT && 
	    mCurrentState != BST_CONNECTING &&
	    mCurrentState != BST_RESTARTING) 
	{
		wxLogDebug(wxT("Unexpected start notification "
			"while in state %s"),
			mStateStrings[mCurrentState]);
		return;
	}

	if (mCurrentState == BST_RESTARTING)
		mCurrentState = BST_START;
	else
		mCurrentState = BST_CONNECTING;
	
	if (status == 0)
	{
		// mLastError = ERR_STARTED;
	}
	else
	{
		wxLogError(wxT("Failed to start client process: "
			"exit code %d"), status);
		mLastError = ERR_STARTFAILED;
	}
}

bool ClientConnection::SetWorkerState(WorkerState oldState, 
	WorkerState newState, const char *cmd) 
{
	wxMutexLocker lock(mMutex);
	
	if (mCurrentState != oldState) {
		wxLogDebug(wxT(
			"Wrong state for %s: "
			"should be '%s' (%d) but currently '%s' (%d)"),
			cmd,
			GetStateStr(oldState), oldState,
			GetStateStr(mCurrentState), mCurrentState);
		mLastError = ERR_INVALID_STATE;
		return FALSE;
	}
	
	mCurrentState = newState;
	return TRUE;
}

bool ClientConnection::StartClient() {
	return SetWorkerState(BST_CONNECTING, BST_START, "start client");
}

bool ClientConnection::StopClient() {
	return SetWorkerState(BST_CONNECTED, BST_STOP, "stop client");
}

bool ClientConnection::RestartClient() {
	return SetWorkerState(BST_CONNECTED, BST_RESTART, "restart client");
}

bool ClientConnection::SyncClient() {
	return SetWorkerState(BST_CONNECTED, BST_SYNC, "sync client");
}

bool ClientConnection::ReloadClient() {
	return SetWorkerState(BST_CONNECTED, BST_RELOAD, "reload client");
}

bool ClientConnection::KillClient() {
	long pid = GetClientPidFast();

	if (pid == -1) {
		wxLogDebug(wxT("Failed to get client PID"));
		mLastError = ERR_PROCESSNOTFOUND;
		return FALSE;
	}

	wxKillError error = wxProcess::Kill(pid, wxSIGTERM);
	if (error == wxKILL_OK)
		return TRUE;

	switch (error)
	{
		case wxKILL_BAD_SIGNAL: // no such signal
			wxLogDebug(wxT("No such signal wxSIGTERM?"));
			mLastError = ERR_INTERNAL;
			break;
		case wxKILL_ACCESS_DENIED: // permission denied
			wxLogDebug(wxT("Failed to kill process: "
				"permission denied"));
			mLastError = ERR_ACCESSDENIED;
			break;
		case wxKILL_NO_PROCESS: // no such process
			wxLogDebug(wxT("Failed to kill process: "
				"no such process %ld"), pid);
			mLastError = ERR_PROCESSNOTFOUND;
			break;
		case wxKILL_ERROR: // another, unspecified error
			wxLogDebug(wxT("Failed to kill process: "
				"unspecified error"));
			mLastError = ERR_UNKNOWN;
			break;
		default:
			wxLogError(wxT("Unknown error while "
				"trying to kill process"));
			mLastError = ERR_UNKNOWN;
	}
	
	return FALSE;
}
