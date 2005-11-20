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

#define NDEBUG
#include "Box.h"
#include "TLSContext.h"
#include "BackupClientContext.h"
#include "BackupClientDirectoryRecord.h"
#include "BackupDaemon.h"
#undef NDEBUG

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
	bool              mStorageLimitExceeded;
	bool              mBackupRunning;
	std::vector<LocationRecord *> mLocations;

	// Unused entries in the root directory wait a while before being deleted
	box_time_t mDeleteUnusedRootDirEntriesAfter;	// time to delete them
	std::vector<std::pair<int64_t,std::string> > mUnusedRootDirEntries;

	std::vector<std::string> mIDMapMounts;
	std::vector<BackupClientInodeToIDMap *> mCurrentIDMaps;
	std::vector<BackupClientInodeToIDMap *> mNewIDMaps;

	/* LocationResolver interface */
	virtual bool FindLocationPathName(const std::string &rLocationName, 
		std::string &rPathOut) const { return FALSE; }
		
	/* RunStatusProvider interface */
	virtual bool StopRun() { return FALSE; }
	
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
		const std::string& rLocalPath) const 
	{
		wxString msg;
		msg.Printf(wxT("Scanning directory '%s'"), 
			wxString(rLocalPath.c_str(), wxConvLibc).c_str());
		mpCurrentText->SetLabel(msg);
		wxYield();
	}
	virtual void NotifyDirStatFailed(
		const BackupClientDirectoryRecord* pDirRecord,
		const std::string& rLocalPath,
		const std::string& rErrorMsg) const
	{
		wxString msg;
		msg.Printf(wxT("Failed to read directory '%s': %s"), 
			wxString(rLocalPath.c_str(), wxConvLibc).c_str(),
			wxString(rErrorMsg.c_str(),  wxConvLibc).c_str());
	}
	
	void ReportBackupFatalError(wxString msg)
	{
		wxMessageBox(msg, wxT("Boxi Error"), wxOK | wxICON_ERROR, this);
		mpErrorList->Append(msg);
	}

	DECLARE_EVENT_TABLE()
};

#endif /* _BACKUP_PROGRESS_PANEL_H */
