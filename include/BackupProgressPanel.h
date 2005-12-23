/***************************************************************************
 *            BackupProgressPanel.h
 *
 *  Mon Apr  4 20:35:39 2005
 *  Copyright  2005  Chris Wilson
 *  Email <boxi_BackupProgressPanel.h@qwirx.com>
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

#include <wx/wx.h>
#include <wx/thread.h>

// Box Backup's config.h tries to redefine PACKAGE_NAME, etc.
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_BUGREPORT
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#define NDEBUG
#include "Box.h"
#include "TLSContext.h"
#include "BackupClientContext.h"
#include "BackupClientDirectoryRecord.h"
#include "BackupDaemon.h"
#include "BackupStoreException.h"
#undef NDEBUG

// now get our old #defines back
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_BUGREPORT
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "ClientConfig.h"
#include "ClientConnection.h"

/** 
 * BackupProgressPanel
 * Shows status of a running backup and allows user to pause or cancel it.
 */

class ServerConnection;

class BackupProgressPanel 
: public wxPanel, LocationResolver, RunStatusProvider, SysadminNotifier,
	ProgressNotifier
{
	public:
	BackupProgressPanel(
		ClientConfig*     pConfig,
		ServerConnection* pConnection,
		wxWindow* parent, wxWindowID id = -1,
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL, 
		const wxString& name = wxT("Backup Progress Panel"));

	void StartBackup();

	private:
	class LocationRecord
	{
		public:
		LocationRecord();
		~LocationRecord();

		private:
		LocationRecord(const LocationRecord &);	// copy not allowed
		LocationRecord &operator=(const LocationRecord &);

		public:
		std::string mName;
		std::string mPath;
		std::auto_ptr<BackupClientDirectoryRecord> mpDirectoryRecord;
		int mIDMapIndex;
		ExcludeList *mpExcludeFiles;
		ExcludeList *mpExcludeDirs;
	};

	ClientConfig*     mpConfig;
	ServerConnection* mpConnection;
	TLSContext        mTlsContext;
	wxListBox*        mpErrorList;
	wxStaticText*     mpSummaryText;
	wxStaticText*     mpCurrentText;
	wxTextCtrl*       mpNumFilesDone;
	wxTextCtrl*       mpNumFilesRemaining;
	wxTextCtrl*       mpNumFilesTotal;
	wxTextCtrl*       mpNumBytesDone;
	wxTextCtrl*       mpNumBytesRemaining;
	wxTextCtrl*       mpNumBytesTotal;
	wxButton*         mpStopCloseButton;
	wxGauge*          mpProgressGauge;
	bool              mStorageLimitExceeded;
	bool              mBackupRunning;
	bool              mBackupStopRequested;
	std::vector<LocationRecord *> mLocations;

	// Unused entries in the root directory wait a while before being deleted
	box_time_t mDeleteUnusedRootDirEntriesAfter;	// time to delete them
	std::vector<std::pair<int64_t,std::string> > mUnusedRootDirEntries;

	std::vector<std::string> mIDMapMounts;
	std::vector<BackupClientInodeToIDMap *> mCurrentIDMaps;
	std::vector<BackupClientInodeToIDMap *> mNewIDMaps;

	size_t  mNumFilesCounted;
	int64_t mNumBytesCounted;

	size_t  mNumFilesUploaded;
	int64_t mNumBytesUploaded;

	size_t  mNumFilesSynchronised;
	int64_t mNumBytesSynchronised;

	/* LocationResolver interface */
	virtual bool FindLocationPathName(const std::string &rLocationName, 
		std::string &rPathOut) const;
		
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
	virtual void NotifySysadmin(int Event) { }
	
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

	/* ProgressNotifier interface */
	virtual void NotifyScanDirectory(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath)
	{
		wxString msg;
		msg.Printf(wxT("Scanning directory '%s'"), 
			wxString(rLocalPath.c_str(), wxConvLibc).c_str());
		mpCurrentText->SetLabel(msg);
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
			wxString(rLocalPath.c_str(), wxConvLibc).c_str(),
			wxString(rErrorMsg.c_str(),  wxConvLibc).c_str());
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
	virtual void NotifyFileReadFailed(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath,
		const std::string& rErrorMsg)
	{
		wxString msg;
		msg.Printf(wxT("Failed to read file '%s': %s"), 
			wxString(rLocalPath.c_str(), wxConvLibc).c_str(),
			wxString(rErrorMsg.c_str(),  wxConvLibc).c_str());
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
			wxString(rLocalPath.c_str(), wxConvLibc).c_str());
		mpErrorList->Append(msg);
		wxYield();
	}
	virtual void NotifyFileSkippedServerFull(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath)
	{ 
		wxString msg;
		msg.Printf(wxT("Failed to send file '%s': no more space available"), 
			wxString(rLocalPath.c_str(), wxConvLibc).c_str());
		mpErrorList->Append(msg);
		wxYield();
	}
	virtual void NotifyFileUploadException(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath,
		const BoxException& rException)
	{
		wxString msg;
		msg.Printf(wxT("Failed to send file '%s': %s"), 
			wxString(rLocalPath.c_str(), wxConvLibc).c_str(),
			wxString(rException.what(),  wxConvLibc).c_str());
		mpErrorList->Append(msg);
		wxYield();
	}
	virtual void NotifyFileUploading(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath)
	{
		wxString msg;
		msg.Printf(wxT("Backing up file '%s'"), 
			wxString(rLocalPath.c_str(), wxConvLibc).c_str());
		mpCurrentText->SetLabel(msg);
		Layout();
		wxYield();
	}
	virtual void NotifyFileUploadingPatch(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath)
	{
		wxString msg;
		msg.Printf(wxT("Backing up file '%s' (sending patch)"), 
			wxString(rLocalPath.c_str(), wxConvLibc).c_str());
		mpCurrentText->SetLabel(msg);
		Layout();
		wxYield();
	}
	virtual void NotifyFileUploaded(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath,
		int64_t FileSize) 
	{
		mNumFilesUploaded++;
		mNumBytesUploaded += FileSize;
	}
	virtual void NotifyFileSynchronised(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath,
		int64_t FileSize) 
	{
		mNumFilesSynchronised++;
		mNumBytesSynchronised += FileSize;

		wxString str;
		str.Printf(wxT("%d"), mNumFilesSynchronised);
		mpNumFilesDone->SetValue(str);
		mpNumBytesDone->SetValue(FormatNumBytes(mNumBytesSynchronised));

		str.Printf(wxT("%d"), mNumFilesCounted - mNumFilesSynchronised);
		mpNumFilesRemaining->SetValue(str);
		mpNumBytesRemaining->SetValue(
			FormatNumBytes(mNumBytesCounted - mNumBytesSynchronised));
		
		mpProgressGauge->SetValue(mNumFilesSynchronised);
	}
	
	void BackupProgressPanel::CountDirectory(BackupClientContext& rContext,
		const std::string &rLocalPath);
	void NotifyCountDirectory(const std::string& rLocalPath)
	{
		wxString msg;
		msg.Printf(wxT("Counting files in directory '%s'"), 
			wxString(rLocalPath.c_str(), wxConvLibc).c_str());
		mpCurrentText->SetLabel(msg);
		Layout();
		wxYield();
	}
	
	wxString FormatNumBytes(int64_t bytes)
	{
		wxString units = wxT("B");
		
		if (bytes > 1024)
		{
			bytes >>= 10;
			units = wxT("kB");
		}
		
		if (bytes > 1024)
		{
			bytes >>= 10;
			units = wxT("MB");
		}

		if (bytes > 1024)
		{
			bytes >>= 10;
			units = wxT("GB");
		}

		wxString str;		
		str.Printf(wxT("%lld %s"), bytes, units.c_str());
		return str;
	}
	
	void NotifyMoreFilesCounted(size_t numAdditionalFiles, 
		int64_t numAdditionalBytes)
	{
		mNumFilesCounted += numAdditionalFiles;
		mNumBytesCounted += numAdditionalBytes;

		wxString str;
		str.Printf(wxT("%d"), mNumFilesCounted);
		mpNumFilesTotal->SetValue(str);
		mpNumBytesTotal->SetValue(FormatNumBytes(mNumBytesCounted));
	}

	void ReportBackupFatalError(wxString msg)
	{
		wxMessageBox(msg, wxT("Boxi Error"), wxOK | wxICON_ERROR, this);
		mpErrorList->Append(msg);
	}

	DECLARE_EVENT_TABLE()
};

#endif /* _BACKUP_PROGRESS_PANEL_H */
