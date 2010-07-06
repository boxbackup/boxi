/***************************************************************************
 *            BackupProgressPanel.h
 *
 *  Mon Apr  4 20:35:39 2005
 *  Copyright 2005-2008 Chris Wilson
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
 */
 
#ifndef _BACKUP_PROGRESS_PANEL_H
#define _BACKUP_PROGRESS_PANEL_H

#include "TLSContext.h"
#include "BackupDaemonInterface.h"
#include "RunStatusProvider.h"
#include "ProgressPanel.h"

/** 
 * BackupProgressPanel
 * Shows status of a running backup and allows user to pause or cancel it.
 */

class BackupClientDirectoryRecord;
class BackupClientContext;

class ClientConfig;
class ServerConnection;

class BackupProgressPanel : public ProgressPanel, RunStatusProvider,
	ProgressNotifier
{
	public:
	BackupProgressPanel
	(
		ClientConfig*     pConfig,
		ServerConnection* pConnection,
		wxWindow*         pParent
	);

	void StartBackup();

	private:
	ClientConfig*     mpConfig;
	/*
	ServerConnection* mpConnection;
	TLSContext        mTlsContext;
	bool              mStorageLimitExceeded;
	*/
	bool              mBackupRunning;
	bool              mBackupStopRequested;
		
	virtual bool IsStopRequested() { return mBackupStopRequested; }
		
	std::auto_ptr<BackupDaemon> mapDaemon;
	
	// BackupDaemon      mBackupDaemon;
	// std::vector<LocationRecord *> mLocations;

	// Unused entries in the root directory wait a while before being deleted
	/*
	box_time_t mDeleteUnusedRootDirEntriesAfter;	// time to delete them
	std::vector<std::pair<int64_t,std::string> > mUnusedRootDirEntries;

	std::vector<std::string> mIDMapMounts;
	std::vector<BackupClientInodeToIDMap *> mCurrentIDMaps;
	std::vector<BackupClientInodeToIDMap *> mNewIDMaps;
	*/

	/* LocationResolver interface */
	/*
	virtual bool FindLocationPathName(const std::string &rLocationName, 
		std::string &rPathOut) const;
	*/
	
	/* RunStatusProvider interface */
	virtual bool StopRun() { return mBackupStopRequested; }
	
	virtual void OnStopCloseClicked(wxCommandEvent& event) 
	{ 
		if (mBackupRunning)
		{
			mBackupStopRequested = TRUE;
		}
		else
		{
			Hide();
		}
	}
	
	/* SysadminNotifier interface */
	/*
	virtual void NotifySysadmin(int Event) { }
	*/
	
	/*
	void SetupLocations(BackupClientContext &rClientContext);
	void DeleteAllLocations();
	void SetupIDMapsForSync();
	void DeleteIDMapVector(std::vector<BackupClientInodeToIDMap *> &rVector);
	void DeleteAllIDMaps()
	{
		DeleteIDMapVector(mCurrentIDMaps);
		DeleteIDMapVector(mNewIDMaps);
	}
	void FillIDMapVector(std::vector<BackupClientInodeToIDMap *> &rVector, bool NewMaps);
	void MakeMapBaseName(unsigned int MountNumber, std::string &rNameOut) const;
	void DeleteUnusedRootDirEntries(BackupClientContext &rContext);
	*/
	
	/* ProgressNotifier interface */
	virtual void NotifyIDMapsSetup(BackupClientContext& rContext);

	virtual void NotifyScanDirectory(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath)
	{
		wxString msg;
		msg.Printf(wxT("Scanning directory '%s'"), 
			wxString(rLocalPath.c_str(), wxConvBoxi).c_str());
		SetCurrentText(msg);
		Layout();
		wxYield();
	}

	virtual void NotifyDirStatFailed(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath,
		const std::string& rErrorMsg)
	{
		wxString msg;
		msg.Printf(wxT("Failed to read directory '%s': %s"), 
			wxString(rLocalPath.c_str(), wxConvBoxi).c_str(),
			wxString(rErrorMsg.c_str(),  wxConvBoxi).c_str());
		mpErrorList->Append(msg);
		wxYield();
	}

	virtual void NotifyFileStatFailed(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath,
		const std::string& rErrorMsg)
	{
		NotifyFileReadFailed(pDirRecord, rLocalPath, rErrorMsg);
	}

	virtual void NotifyDirListFailed(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath,
		const std::string& rErrorMsg)
	{
		wxString msg;
		msg.Printf(wxT("Failed to list directory '%s': %s"), 
			wxString(rLocalPath.c_str(), wxConvBoxi).c_str(),
			wxString(rErrorMsg.c_str(),  wxConvBoxi).c_str());
		mpErrorList->Append(msg);
		wxYield();
	}

	virtual void NotifyFileReadFailed(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath,
		const std::string& rErrorMsg)
	{
		wxString msg;
		msg.Printf(wxT("Failed to read file '%s': %s"), 
			wxString(rLocalPath.c_str(), wxConvBoxi).c_str(),
			wxString(rErrorMsg.c_str(),  wxConvBoxi).c_str());
		mpErrorList->Append(msg);
		wxYield();
	}

	virtual void NotifyFileModifiedInFuture(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath)
	{
		wxString msg;
		msg.Printf(wxT("Warning: file modified in the future "
				"(check your system clock): '%s'"), 
			wxString(rLocalPath.c_str(), wxConvBoxi).c_str());
		mpErrorList->Append(msg);
		wxYield();
	}

	virtual void NotifyFileSkippedServerFull(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath)
	{ 
		wxString msg;
		msg.Printf(wxT("Failed to send file '%s': no more space available"), 
			wxString(rLocalPath.c_str(), wxConvBoxi).c_str());
		mpErrorList->Append(msg);
		wxYield();
	}

	virtual void NotifyFileUploadException(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath,
		const BoxException& rException)
	{
		wxString msg;
		if (rException.GetType() == CommonException::ExceptionType &&
			rException.GetSubType() == CommonException::AccessDenied)
		{
			msg.Printf(wxT("Failed to send file '%s': Access denied"), 
				wxString(rLocalPath.c_str(), wxConvBoxi).c_str());
		}
		else
		{
			msg.Printf(wxT("Failed to send file '%s': %s"), 
				wxString(rLocalPath.c_str(), wxConvBoxi).c_str(),
				wxString(rException.what(),  wxConvBoxi).c_str());
		}
		mpErrorList->Append(msg);
		wxYield();
	}

	virtual void NotifyFileUploading(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath)
	{
		wxString msg;
		msg.Printf(wxT("Backing up file '%s'"), 
			wxString(rLocalPath.c_str(), wxConvBoxi).c_str());
		SetCurrentText(msg);
		Layout();
		wxYield();
	}

	virtual void NotifyFileUploadingPatch(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath)
	{
		wxString msg;
		msg.Printf(wxT("Backing up file '%s' (sending patch)"), 
			wxString(rLocalPath.c_str(), wxConvBoxi).c_str());
		SetCurrentText(msg);
		Layout();
		wxYield();
	}

	virtual void NotifyFileUploadingAttributes(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath)
	{
		wxString msg;
		msg.Printf(wxT("Backing up file '%s' (uploading new "
			"attributes)"),
			wxString(rLocalPath.c_str(), wxConvBoxi).c_str());
		SetCurrentText(msg);
		Layout();
		wxYield();
	}

	virtual void NotifyFileUploaded(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath,
		int64_t FileSize) 
	{
		/*
		mNumFilesUploaded++;
		mNumBytesUploaded += FileSize;
		*/
	}

	virtual void NotifyFileSynchronised(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath,
		int64_t FileSize) 
	{
		NotifyMoreFilesDone(1, FileSize);
	}

	virtual void NotifyMountPointSkipped(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath) { }
	virtual void NotifyFileExcluded(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath) { }
	virtual void NotifyDirExcluded(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath) { }
	virtual void NotifyUnsupportedFileType(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath) { }
	virtual void NotifyFileUploadServerError(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath,
		int type, int subtype) { }
	virtual void NotifyDirectoryDeleted(
		int64_t ObjectID,
		const std::string& rRemotePath) { }
	virtual void NotifyFileDeleted(
		int64_t ObjectID,
		const std::string& rRemotePath) { }
	virtual void NotifyReadProgress(int64_t readSize, int64_t offset,
		int64_t length, box_time_t elapsed, box_time_t finish)
	{
		wxYield();
	}
	virtual void NotifyReadProgress(int64_t readSize, int64_t offset,
		int64_t length)
	{
		wxYield();
	}
	virtual void NotifyReadProgress(int64_t readSize, int64_t offset)
	{
		wxYield();
	}

	DECLARE_EVENT_TABLE()
};

#endif /* _BACKUP_PROGRESS_PANEL_H */
