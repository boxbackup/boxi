/***************************************************************************
 *            BackupProgressPanel.cc
 *
 *  Mon Apr  4 20:36:25 2005
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
 *
 * Contains software developed by Ben Summers.
 * YOU MUST NOT REMOVE THIS ATTRIBUTION!
 */

#include "SandBox.h"

#include <errno.h>
#include <stdio.h>

#ifdef HAVE_REGEX_H
	#include <regex.h>
	#define EXCLUDELIST_IMPLEMENTATION_REGEX_T_DEFINED
#endif

#ifdef HAVE_PCREPOSIX_H
	#include <pcreposix.h>
	#define EXCLUDELIST_IMPLEMENTATION_REGEX_T_DEFINED
#endif

#ifdef HAVE_MNTENT_H
	#include <mntent.h>
#endif

#include <wx/statbox.h>
#include <wx/listbox.h>
#include <wx/button.h>
#include <wx/gauge.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>

// #define NDEBUG
// #define EXCLUDELIST_IMPLEMENTATION_REGEX_T_DEFINED
#include "BackupClientContext.h"
#include "BackupClientDirectoryRecord.h"
#include "BackupClientCryptoKeys.h"
#include "BackupClientInodeToIDMap.h"
#include "FileModificationTime.h"
#include "MemBlockStream.h"
#include "BackupStoreConstants.h"
#include "BackupStoreException.h"
#include "Utils.h"
// #undef EXCLUDELIST_IMPLEMENTATION_REGEX_T_DEFINED
// #undef NDEBUG

#include "main.h"
#include "BackupProgressPanel.h"
#include "ServerConnection.h"

//DECLARE_EVENT_TYPE(myEVT_CLIENT_NOTIFY, -1)
//DEFINE_EVENT_TYPE(myEVT_CLIENT_NOTIFY)

BEGIN_EVENT_TABLE(BackupProgressPanel, ProgressPanel)
	EVT_BUTTON(wxID_CANCEL, BackupProgressPanel::OnStopCloseClicked)
END_EVENT_TABLE()

BackupProgressPanel::BackupProgressPanel
(
	ClientConfig*     pConfig,
	ServerConnection* pConnection,
	wxWindow*         pParent
)
: ProgressPanel(pParent, ID_Backup_Progress_Panel, _("Backup Progress Panel")),
  mpConfig(pConfig),
  mBackupRunning(false),
  mBackupStopRequested(false)
{ }

void BackupProgressPanel::StartBackup()
{
	mpErrorList->Clear();
	
	wxString errors;
	
	if (!mpConfig->Check(errors))
	{
		ReportFatalError(BM_BACKUP_FAILED_CONFIG_ERROR, errors);
		return;
	}

	ResetCounters();
	
	SetSummaryText(_("Starting Backup"));
	SetStopButtonLabel(_("Stop Backup"));
	
	mBackupRunning = true;
	mBackupStopRequested = false;

	Layout();
	wxYield();

	Timers::Init();
	
	mapDaemon.reset(new BackupDaemon());
	mapDaemon->Configure(mpConfig->GetBoxConfig());
	mapDaemon->InitCrypto();
	mapDaemon->SetProgressNotifier(this);
	mapDaemon->SetRunStatusProvider(this);
	
	try
	{
		mapDaemon->RunSyncNow();

		if (mapDaemon->StorageLimitExceeded())
		{
			ReportFatalError(BM_BACKUP_FAILED_STORE_FULL,
				_("Error: cannot finish backup: "
				"out of space on server"));
			SetSummaryText(_("Backup Failed"));
		}
		else
		{
			SetSummaryText(_("Backup Finished"));
			mpErrorList->Append(_("Backup Finished"));
		}
		
	}
	catch (ConnectionException& e) 
	{
		SetSummaryText(_("Backup Failed"));
		wxString msg;
		msg.Printf(_("Error: cannot start backup: "
			"Failed to connect to server\n\n%s"),
			wxString(e.what(), wxConvLibc).c_str());
		ReportFatalError(BM_BACKUP_FAILED_CONNECT_FAILED, msg);
	}
	catch (BackupStoreException& be)
	{
		if (be.GetSubType() == BackupStoreException::SignalReceived)
		{
			SetSummaryText(_("Backup Interrupted"));
			ReportFatalError(BM_BACKUP_FAILED_INTERRUPTED,
				_("Backup interrupted by user"));
		}
		else		
		{
			SetSummaryText(_("Backup Failed"));
			wxString msg;
			msg.Printf(_("Backup Failed: %s"),
				wxString(be.what(), wxConvLibc).c_str());
			ReportFatalError(BM_BACKUP_FAILED_UNKNOWN_ERROR, msg);
		}
	}
	catch (std::exception& e) 
	{
		SetSummaryText(_("Backup Failed"));
		wxString msg;
		msg.Printf(_("Backup Failed: %s"),
			wxString(e.what(), wxConvLibc).c_str());
		ReportFatalError(BM_BACKUP_FAILED_UNKNOWN_ERROR, msg);
	}
	catch (...) 
	{
		SetSummaryText(_("Backup Failed"));
		wxString msg = _("Backup Failed: caught unknown exception");
		ReportFatalError(BM_BACKUP_FAILED_UNKNOWN_ERROR, msg);
	}

	mapDaemon.reset();
	Timers::Cleanup();

	mBackupRunning = false;
	mBackupStopRequested = false;
	
	SetCurrentText(_("Idle (nothing to do)"));
	SetStopButtonLabel(_("Close"));
	mpProgressGauge->Hide();	
}

void BackupProgressPanel::NotifyIDMapsSetup(BackupClientContext& rContext)
{
	BackupDaemon::Locations& rLocs(mapDaemon->GetLocations());

	// Go through the records, counting files and bytes
	for (BackupDaemon::Locations::const_iterator
		i  = rLocs.begin();
		i != rLocs.end(); i++)
	{
		BackupDaemon::Location* pLocation = *i;
		CountDirectory(rContext, pLocation->mPath);
	}
	
	mpProgressGauge->SetRange(GetNumFilesTotal());
	mpProgressGauge->SetValue(0);
	mpProgressGauge->Show();

	SetSummaryText(_("Backing up files"));
	wxYield();
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupClientDirectoryRecord::SyncDirectory(BackupClientDirectoryRecord::SyncParams &, int64_t, const std::string &, bool)
//		Purpose: Recursively synchronise a local directory with the server.
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------
void BackupProgressPanel::CountDirectory(BackupClientContext& rContext,
	const std::string &rLocalPath)
{
	NotifyCountDirectory(rLocalPath);
	
	// Signal received by daemon?
	if(StopRun())
	{
		// Yes. Stop now.
		THROW_EXCEPTION(BackupStoreException, SignalReceived)
	}
	
	// Read directory entries, counting number and size of files,
	// and recursing into directories.
	size_t  filesCounted = 0;
	int64_t bytesCounted = 0;
	
	// BLOCK
	{		
		// read the contents...
		DIR *dirHandle = 0;
		try
		{
			dirHandle = ::opendir(rLocalPath.c_str());
			if(dirHandle == 0)
			{
				// Ignore this directory for now.
				return;
			}
			
			struct dirent *en = 0;
			struct stat st;
			std::string filename;
			while((en = ::readdir(dirHandle)) != 0)
			{
				// Don't need to use LinuxWorkaround_FinishDirentStruct(en, rLocalPath.c_str());
				// on Linux, as a stat is performed to get all this info

				if(en->d_name[0] == '.' && 
					(en->d_name[1] == '\0' || (en->d_name[1] == '.' && en->d_name[2] == '\0')))
				{
					// ignore, it's . or ..
					continue;
				}

				// Stat file to get info
				filename = rLocalPath + DIRECTORY_SEPARATOR + en->d_name;
				if(::lstat(filename.c_str(), &st) != 0)
				{
					/*
					rParams.GetProgressNotifier().NotifyFileStatFailed(
						(BackupClientDirectoryRecord*)NULL, 
						filename, strerror(errno));
					THROW_EXCEPTION(CommonException, OSFileError)
					*/
					wxString msg;
					msg.Printf(wxT("Error counting files in '%s': %s"),
						wxString(filename.c_str(), wxConvLibc).c_str(),
						wxString(strerror(errno),  wxConvLibc).c_str());
					mpErrorList->Append(msg);
				}

				int type = st.st_mode & S_IFMT;
				if(type == S_IFREG || type == S_IFLNK)
				{
					// File or symbolic link

					// Exclude it?
					if (rContext.ExcludeFile(filename))
					{
						// Next item!
						continue;
					}

					filesCounted++;
					bytesCounted += st.st_size;
				}
				else if(type == S_IFDIR)
				{
					// Directory
					
					// Exclude it?
					if (rContext.ExcludeDir(filename))
					{
						// Next item!
						continue;
					}
					
					CountDirectory(rContext, filename);
				}
				else
				{
					continue;
				}
			}
	
			if(::closedir(dirHandle) != 0)
			{
				THROW_EXCEPTION(CommonException, OSFileError)
			}
			dirHandle = 0;
			
		}
		catch(...)
		{
			if(dirHandle != 0)
			{
				::closedir(dirHandle);
			}
			throw;
		}
	}

	NotifyMoreFilesCounted(filesCounted, bytesCounted);
}
