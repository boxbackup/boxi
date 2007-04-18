/***************************************************************************
 *            BackupProgressPanel.cc
 *
 *  Mon Apr  4 20:36:25 2005
 *  Copyright 2005-2006 Chris Wilson
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

#include <errno.h>
#include <stdio.h>
#include <regex.h>

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

#ifdef HAVE_MNTENT_H
	#include <mntent.h>
#endif

#include "main.h"
#include "BackupProgressPanel.h"
#include "ServerConnection.h"

//DECLARE_EVENT_TYPE(myEVT_CLIENT_NOTIFY, -1)
//DEFINE_EVENT_TYPE(myEVT_CLIENT_NOTIFY)

BEGIN_EVENT_TABLE(BackupProgressPanel, wxPanel)
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
  mpConnection(pConnection),
  mBackupRunning(false),
  mBackupStopRequested(false)
{ }

void BackupProgressPanel::StartBackup()
{
	mpErrorList->Clear();

	wxString errorMsg;
	if (!mpConnection->InitTlsContext(mTlsContext, errorMsg))
	{
		wxString msg;
		msg.Printf(_("Error: cannot start backup: %s"), errorMsg.c_str());
		ReportFatalError(BM_BACKUP_FAILED_CANNOT_INIT_ENCRYPTION, msg);
		return;
	}
	
	std::string storeHost;
	mpConfig->StoreHostname.GetInto(storeHost);

	if (storeHost.length() == 0) 
	{
		ReportFatalError(BM_BACKUP_FAILED_NO_STORE_HOSTNAME,
			_("Error: cannot start backup: "
			"You have not configured the Store Hostname!"));
		return;
	}

	std::string keysFile;
	mpConfig->KeysFile.GetInto(keysFile);

	if (keysFile.length() == 0) 
	{
		ReportFatalError(BM_BACKUP_FAILED_NO_KEYS_FILE,
			_("Error: cannot start backup: "
			"you have not configured the Keys File"));
		return;
	}

	int acctNo;
	if (!mpConfig->AccountNumber.GetInto(acctNo))
	{
		ReportFatalError(BM_BACKUP_FAILED_NO_ACCOUNT_NUMBER,
			_("Error: cannot start backup: "
			"you have not configured the Account Number"));
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
		ReportFatalError(BM_BACKUP_FAILED_NO_MINIMUM_FILE_AGE,
			_("Error: cannot start backup: "
			"You have not configured the minimum file age"));
		return;
	}
	box_time_t minimumFileAge = SecondsToBoxTime((uint32_t)minimumFileAgeSecs);

	// The maximum time we'll wait to upload a file, 
	// regardless of how often it's modified
	int maxUploadWaitSecs;
	if (!mpConfig->MaxUploadWait.GetInto(maxUploadWaitSecs))
	{
		ReportFatalError(BM_BACKUP_FAILED_NO_MAXIMUM_UPLOAD_WAIT,
			_("Error: cannot start backup: "
			"You have not configured the maximum upload wait"));
		return;
	}
	box_time_t maxUploadWait = SecondsToBoxTime((uint32_t)maxUploadWaitSecs);

	// Adjust by subtracting the minimum file age, 
	// so is relative to sync period end in comparisons
	maxUploadWait = (maxUploadWait > minimumFileAge)
		? (maxUploadWait - minimumFileAge) : (0);

	// Set up the sync parameters
	BackupClientDirectoryRecord::SyncParams params(*this, *this, *this,
		clientContext);
	
	params.mMaxUploadWait = maxUploadWait;
	
	if (!mpConfig->FileTrackingSizeThreshold.GetInto(
		params.mFileTrackingSizeThreshold))
	{
		ReportFatalError(BM_BACKUP_FAILED_NO_TRACKING_THRESHOLD,
			_("Error: cannot start backup: "
			"You have not configured the file tracking size threshold"));
		return;
	}

	if (!mpConfig->DiffingUploadSizeThreshold.GetInto(
		params.mDiffingUploadSizeThreshold))
	{
		ReportFatalError(BM_BACKUP_FAILED_NO_DIFFING_THRESHOLD,
			_("Error: cannot start backup: "
			"You have not configured the diffing upload size threshold"));
		return;
	}

	int maxFileTimeInFutureSecs = 0;
	mpConfig->MaxFileTimeInFuture.GetInto(maxFileTimeInFutureSecs);
	params.mMaxFileTimeInFuture = SecondsToBoxTime(
		(uint32_t)maxFileTimeInFutureSecs);
	
	// Calculate the sync period of files to examine
	box_time_t syncPeriodStart = 0;
	box_time_t syncPeriodEnd = GetCurrentBoxTime() - minimumFileAge;

	// Paranoid check on sync times
	if (syncPeriodStart >= syncPeriodEnd)
	{
		wxString msg;
		msg.Printf(_("Error: cannot start backup: "
			"Sync time window calculation failed: %lld >= %lld"),
			syncPeriodStart, syncPeriodEnd);
		ReportFatalError(BM_BACKUP_FAILED_INVALID_SYNC_PERIOD, msg);
		return;
	}

	// Adjust syncPeriodEnd to emulate snapshot behaviour properly
	box_time_t syncPeriodEndExtended = syncPeriodEnd;
	// Using zero min file age?
	if(minimumFileAge == 0)
	{
		// Add a year on to the end of the end time, to make sure we sync
		// files which are modified after the scan run started.
		// Of course, they may be eligable to be synced again the next time round,
		// but this should be OK, because the changes only upload should upload no data.
		syncPeriodEndExtended += SecondsToBoxTime((uint32_t)(356*24*3600));
	}

	params.mSyncPeriodStart = syncPeriodStart;
	params.mSyncPeriodEnd = syncPeriodEndExtended; // use potentially extended end time
	
	// Set store marker - haven't contacted the store yet
	int64_t clientStoreMarker = BackupClientContext::ClientStoreMarker_NotKnown;
	clientContext.SetClientStoreMarker(clientStoreMarker);
	
	// Touch a file to record times in filesystem
	{
		wxString datadir;
		if (mpConfig->DataDirectory.GetInto(datadir))
		{
			wxFile f;
			f.Create(wxFileName(datadir, 
				_("last_sync_start")).GetFullPath(), true);
		}
	}
	
	ResetCounters();
	
	SetSummaryText(_("Starting Backup"));
	SetStopButtonLabel(_("Stop Backup"));
	
	mBackupRunning = true;
	mBackupStopRequested = false;

	Layout();
	wxYield();

	#ifdef WIN32
	// init our own timer for file diff timeouts
	InitTimer();
	#endif

	try 
	{
		SetupLocations(clientContext);
	
		// Get some ID maps going
		SetupIDMapsForSync();
	
		SetSummaryText(_("Deleting unused root entries on server"));
		wxYield();
		
		DeleteUnusedRootDirEntries(clientContext);
								
		SetSummaryText(_("Counting files to back up"));
		wxYield();
		
		typedef const std::vector<LocationRecord *> tLocationRecords;
	
		// Go through the records, counting files and bytes
		for (tLocationRecords::const_iterator i = mLocations.begin();
			i != mLocations.end(); i++)
		{
			LocationRecord* pLocRecord = *i;
	
			// Set exclude lists (context doesn't take ownership)
			clientContext.SetExcludeLists(
				pLocRecord->mpExcludeFiles, 
				pLocRecord->mpExcludeDirs);
	
			CountDirectory(clientContext, pLocRecord->mPath);
		}
		
		mpProgressGauge->SetRange(GetNumFilesTotal());
		mpProgressGauge->SetValue(0);
		mpProgressGauge->Show();

		SetSummaryText(_("Backing up files"));
		wxYield();
		
		// Go through the records agains, this time syncing them
		for (tLocationRecords::const_iterator i = mLocations.begin();
			i != mLocations.end(); i++)
		{
			LocationRecord* pLocRecord = *i;
			int IDMapIndex = pLocRecord->mIDMapIndex;
	
			// Set current and new ID map pointers in the context
			clientContext.SetIDMaps(
				mCurrentIDMaps[IDMapIndex], 
				mNewIDMaps    [IDMapIndex]);
		
			// Set exclude lists (context doesn't take ownership)
			clientContext.SetExcludeLists(
				pLocRecord->mpExcludeFiles, 
				pLocRecord->mpExcludeDirs);
	
			// Sync the directory
			pLocRecord->mpDirectoryRecord->SyncDirectory(params, 
				BackupProtocolClientListDirectory::RootDirectory, 
				pLocRecord->mPath);
	
			// Unset exclude lists (just in case)
			clientContext.SetExcludeLists(0, 0);
		}
		
		// Perform any deletions required -- these are delayed until
		// the end to allow renaming to happen neatly.
		clientContext.PerformDeletions();
		
		if (clientContext.StorageLimitExceeded())
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
		
		mpProgressGauge->Hide();
	}
	catch (ConnectionException& e) 
	{
		SetSummaryText(_("Backup Failed"));
		wxString msg;
		msg.Printf(_("Error: cannot start backup: "
			"Failed to connect to server: %s"),
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
			throw;
		}
	}
	catch (...)
	{
		SetSummaryText(_("Backup Failed"));
		ReportFatalError(BM_BACKUP_FAILED_UNKNOWN_ERROR,
			_("Unknown error"));
	}	

	#ifdef WIN32
	FiniTimer();
	#endif

	// Touch a file to record times in filesystem
	{
		wxString datadir;
		if (mpConfig->DataDirectory.GetInto(datadir))
		{
			wxFile f;
			f.Create(wxFileName(datadir, 
				_("last_sync_finish")).GetFullPath(), true);
		}
	}

	mBackupRunning = false;
	mBackupStopRequested = false;
	
	SetCurrentText(_("Idle (nothing to do)"));
	SetStopButtonLabel(_("Close"));
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

#ifndef HAVE_STRUCT_STATFS_F_MNTONNAME
	// string comparison ordering for when mount points are handled
	// by code, rather than the OS.
	typedef struct
	{
		bool operator()(const std::string &s1, const std::string &s2)
		{
			if(s1.size() == s2.size())
			{
				// Equal size, sort according to natural sort order
				return s1 < s2;
			}
			else
			{
				// Make sure longer strings go first
				return s1.size() > s2.size();
			}
		}
	} mntLenCompare;
#endif

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupProgressPanel::SetupLocations(
//		             BackupClientContext &, const Configuration &)
//		Purpose: Makes sure that the list of directories records is correctly set up
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------
void BackupProgressPanel::SetupLocations(BackupClientContext &rClientContext)
{
	/*
	if(!mLocations.empty())
	{
		// Looks correctly set up
		return;
	}
	*/

	// Make sure that if a directory is reinstated, then it doesn't get deleted	
	mDeleteUnusedRootDirEntriesAfter = 0;
	mUnusedRootDirEntries.clear();

	// Just a check to make sure it's right.
	DeleteAllLocations();
	
	// Going to need a copy of the root directory. Get a connection, and fetch it.
	BackupProtocolClient &connection(rClientContext.GetConnection());
	
	// Ask server for a list of everything in the root directory, which is a directory itself
	std::auto_ptr<BackupProtocolClientSuccess> dirreply(connection.QueryListDirectory(
			BackupProtocolClientListDirectory::RootDirectory,
			BackupProtocolClientListDirectory::Flags_Dir,	// only directories
			BackupProtocolClientListDirectory::Flags_Deleted 
			| BackupProtocolClientListDirectory::Flags_OldVersion, // exclude old/deleted stuff
			false /* no attributes */));

	// Retrieve the directory from the stream following
	BackupStoreDirectory dir;
	std::auto_ptr<IOStream> dirstream(connection.ReceiveStream());
	dir.ReadFromStream(*dirstream, connection.GetTimeout());
	
	// Map of mount names to ID map index
	std::map<std::string, int> mounts;
	int numIDMaps = 0;

#ifdef HAVE_MOUNTS
#ifndef HAVE_STRUCT_STATFS_F_MNTONNAME
	// Linux and others can't tell you where a directory is mounted. So we
	// have to read the mount entries from /etc/mtab! Bizarre that the OS
	// itself can't tell you, but there you go.
	std::set<std::string, mntLenCompare> mountPoints;
	// BLOCK
	FILE *mountPointsFile = 0;

#ifdef HAVE_STRUCT_MNTENT_MNT_DIR
	// Open mounts file
	mountPointsFile = ::setmntent("/proc/mounts", "r");
	if(mountPointsFile == 0)
	{
		mountPointsFile = ::setmntent("/etc/mtab", "r");
	}
	if(mountPointsFile == 0)
	{
		THROW_EXCEPTION(CommonException, OSFileError);
	}

	try
	{
		// Read all the entries, and put them in the set
		struct mntent *entry = 0;
		while((entry = ::getmntent(mountPointsFile)) != 0)
		{
			TRACE1("Found mount point at %s\n", entry->mnt_dir);
			mountPoints.insert(std::string(entry->mnt_dir));
		}

		// Close mounts file
		::endmntent(mountPointsFile);
	}
	catch(...)
	{
			::endmntent(mountPointsFile);
		throw;
	}
#else // ! HAVE_STRUCT_MNTENT_MNT_DIR
	// Open mounts file
	mountPointsFile = ::fopen("/etc/mnttab", "r");
	if(mountPointsFile == 0)
	{
		THROW_EXCEPTION(CommonException, OSFileError);
	}

	try
	{

		// Read all the entries, and put them in the set
		struct mnttab entry;
		while(getmntent(mountPointsFile, &entry) == 0)
		{
			TRACE1("Found mount point at %s\n", entry.mnt_mountp);
			mountPoints.insert(std::string(entry.mnt_mountp));
		}

		// Close mounts file
		::fclose(mountPointsFile);
	}
	catch(...)
	{
		::fclose(mountPointsFile);
		throw;
	}
#endif // HAVE_STRUCT_MNTENT_MNT_DIR
	// Check sorting and that things are as we expect
	ASSERT(mountPoints.size() > 0);
#ifndef NDEBUG
	{
		std::set<std::string, mntLenCompare>::const_reverse_iterator i(mountPoints.rbegin());
		ASSERT(*i == "/");
	}
#endif // n NDEBUG
#endif // n HAVE_STRUCT_STATFS_F_MNTONNAME
#endif // HAVE_MOUNTS

	// Then... go through each of the entries in the configuration,
	// making sure there's a directory created for it.
	const Location::List& rLocations = mpConfig->GetLocations();
	
	for (Location::ConstIterator pLocation = rLocations.begin();
		pLocation != rLocations.end(); pLocation++)
	{
		TRACE0("new location\n");
		// Create a record for it
		LocationRecord *pLocRecord = new LocationRecord;
		try
		{
			
			// Setup names in the location record
			wxCharBuffer buf = pLocation->GetName().mb_str(wxConvLibc);
			pLocRecord->mName = buf.data();

			buf = pLocation->GetPath().mb_str(wxConvLibc);
			pLocRecord->mPath = buf.data();
			
			// Read the exclude lists from the Configuration
			pLocRecord->mpExcludeFiles = pLocation->GetBoxExcludeList(FALSE);
			pLocRecord->mpExcludeDirs  = pLocation->GetBoxExcludeList(TRUE);
			
			// Do a fsstat on the pathname to find out which mount it's on
			{
#if defined HAVE_STRUCT_STATFS_F_MNTONNAME || defined WIN32

				// BSD style statfs -- includes mount point, which is nice.
				struct statfs s;
				if(::statfs(pLocRecord->mPath.c_str(), &s) != 0)
				{
					THROW_EXCEPTION(CommonException, OSFileError)
				}

				// Where the filesystem is mounted
				std::string mountName((char *)&(s.f_mntonname));

#else // !HAVE_STRUCT_STATFS_F_MNTONNAME && !WIN32

				// Warn in logs if the directory isn't absolute
				if(pLocRecord->mPath[0] != '/')
				{
					wxString msg;
					msg.Printf(wxT("Location path '%s' isn't absolute"), 
						wxString(pLocRecord->mPath.c_str(), wxConvLibc).c_str());
					mpErrorList->Append(msg);
				}
				// Go through the mount points found, and find a suitable one
				std::string mountName("/");
				{
					std::set<std::string, mntLenCompare>::const_iterator i(mountPoints.begin());
					TRACE1("%d potential mount points\n", mountPoints.size());
					for(; i != mountPoints.end(); ++i)
					{
						// Compare first n characters with the filename
						// If it matches, the file belongs in that mount point
						// (sorting order ensures this)
						TRACE1("checking against mount point %s\n", i->c_str());
						if(::strncmp(i->c_str(), pLocRecord->mPath.c_str(), i->size()) == 0)
						{
							// Match
							mountName = *i;
							break;
						}
					}
					TRACE2("mount point chosen for %s is %s\n", 
						pLocRecord->mPath.c_str(), mountName.c_str());
				}

#endif // HAVE_STRUCT_STATFS_F_MNTONNAME || WIN32
				
				// Got it?
				std::map<std::string, int>::iterator f(mounts.find(mountName));
				if(f != mounts.end())
				{
					// Yes -- store the index
					pLocRecord->mIDMapIndex = f->second;
				}
				else
				{
					// No -- new index
					pLocRecord->mIDMapIndex = numIDMaps;
					mounts[mountName] = numIDMaps;
					
					// Store the mount name
					mIDMapMounts.push_back(mountName);
					
					// Increment number of maps
					++numIDMaps;
				}
			}
		
			// Does this exist on the server?
			BackupStoreDirectory::Iterator iter(dir);
			
			// generate the filename
			BackupStoreFilenameClear dirname(pLocRecord->mName);	
			BackupStoreDirectory::Entry *en = iter.FindMatchingClearName(dirname);
			int64_t oid = 0;
			
			if(en != 0)
			{
				oid = en->GetObjectID();
				
				// Delete the entry from the directory, so we get a list of
				// unused root directories at the end of this.
				dir.DeleteEntry(oid);
			}
			else
			{
				// Doesn't exist, so it has to be created on the server. Let's go!
				// First, get the directory's attributes and modification time
				box_time_t attrModTime = 0;
				BackupClientFileAttributes attr;
				attr.ReadAttributes(pLocRecord->mPath.c_str(), 
					true /* directories have zero mod times */,
					0 /* not interested in mod time */, 
					&attrModTime /* get the attribute modification time */);
				
				// Execute create directory command
				MemBlockStream attrStream(attr);
				std::auto_ptr<BackupProtocolClientSuccess> dirCreate(connection.QueryCreateDirectory(
					BackupProtocolClientListDirectory::RootDirectory,
					attrModTime, dirname, attrStream));
					
				// Object ID for later creation
				oid = dirCreate->GetObjectID();
			}

			// Create and store the directory object for the root of this location
			ASSERT(oid != 0);
			BackupClientDirectoryRecord *pDirRecord = 
				new BackupClientDirectoryRecord(oid, pLocRecord->mName);
			pLocRecord->mpDirectoryRecord.reset(pDirRecord);
			
			// Push it back on the vector of locations
			mLocations.push_back(pLocRecord);
		}
		catch(...)
		{
			delete pLocRecord;
			pLocRecord = 0;
			throw;
		}
	}
	
	// Any entries in the root directory which need deleting?
	if (dir.GetNumberOfEntries() > 0)
	{
		wxString msg;
		msg.Printf(wxT("%d redundant locations in root directory found, "
			"will delete from store after %d seconds."),
			dir.GetNumberOfEntries(), 
			BACKUP_DELETE_UNUSED_ROOT_ENTRIES_AFTER);
		mpErrorList->Append(msg);
		
		// Store directories in list of things to delete
		mUnusedRootDirEntries.clear();
		BackupStoreDirectory::Iterator iter(dir);
		BackupStoreDirectory::Entry *en = 0;
		while((en = iter.Next()) != 0)
		{
			// Add name to list
			BackupStoreFilenameClear clear(en->GetName());
			const std::string &name(clear.GetClearFilename());
			mUnusedRootDirEntries.push_back(
				std::pair<int64_t,std::string>(en->GetObjectID(), name));
			// Log this
			msg.Printf(wxT("Unused location in root: %s"), 
				wxString(name.c_str(), wxConvLibc).c_str());
		}
		ASSERT(mUnusedRootDirEntries.size() > 0);
		// Time to delete them
		mDeleteUnusedRootDirEntriesAfter =
			GetCurrentBoxTime() + 
			SecondsToBoxTime((uint32_t)BACKUP_DELETE_UNUSED_ROOT_ENTRIES_AFTER);
	}
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::DeleteAllLocations()
//		Purpose: Deletes all records stored
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------
void BackupProgressPanel::DeleteAllLocations()
{
	// Run through, and delete everything
	for(std::vector<LocationRecord *>::iterator i = mLocations.begin();
		i != mLocations.end(); ++i)
	{
		delete *i;
	}

	// Clear the contents of the map, so it is empty
	mLocations.clear();
	
	// And delete everything from the assoicated mount vector
	mIDMapMounts.clear();
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::SetupIDMapsForSync()
//		Purpose: Sets up ID maps for the sync process -- make sure they're all there
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
void BackupProgressPanel::SetupIDMapsForSync()
{
	// Need to do different things depending on whether it's an in memory implementation,
	// or whether it's all stored on disc.
	
#ifdef BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION

	// Make sure we have some blank, empty ID maps
	DeleteIDMapVector(mNewIDMaps);
	FillIDMapVector(mNewIDMaps, true /* new maps */);

	// Then make sure that the current maps have objects, even if they are empty
	// (for the very first run)
	if(mCurrentIDMaps.empty())
	{
		FillIDMapVector(mCurrentIDMaps, false /* current maps */);
	}

#else

	// Make sure we have some blank, empty ID maps
	DeleteIDMapVector(mNewIDMaps);
	FillIDMapVector(mNewIDMaps, true /* new maps */);
	DeleteIDMapVector(mCurrentIDMaps);
	FillIDMapVector(mCurrentIDMaps, false /* new maps */);

#endif
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::FillIDMapVector(std::vector<BackupClientInodeToIDMap *> &)
//		Purpose: Fills the vector with the right number of empty ID maps
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
void BackupProgressPanel::FillIDMapVector(std::vector<BackupClientInodeToIDMap *> &rVector, bool NewMaps)
{
	ASSERT(rVector.size() == 0);
	rVector.reserve(mIDMapMounts.size());
	
	for(unsigned int l = 0; l < mIDMapMounts.size(); ++l)
	{
		// Create the object
		BackupClientInodeToIDMap *pmap = new BackupClientInodeToIDMap();
		try
		{
#ifdef BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
			pmap->OpenEmpty();
#else
			// Get the base filename of this map
			std::string filename;
			MakeMapBaseName(l, filename);
			
			// If it's a new one, add a suffix
			if(NewMaps)
			{
				filename += ".n";
			}

			// If it's not a new map, it may not exist in which case an empty map should be created
			if(!NewMaps && !FileExists(filename.c_str()))
			{
				pmap->OpenEmpty();
			}
			else
			{
				// Open the map
				pmap->Open(filename.c_str(), !NewMaps /* read only */, NewMaps /* create new */);
			}
#endif // BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
			
			// Store on vector
			rVector.push_back(pmap);
		}
		catch(...)
		{
			delete pmap;
			throw;
		}
	}
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::DeleteIDMapVector(std::vector<BackupClientInodeToIDMap *> &)
//		Purpose: Deletes the contents of a vector of ID maps
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
void BackupProgressPanel::DeleteIDMapVector(std::vector<BackupClientInodeToIDMap *> &rVector)
{
	while(!rVector.empty())
	{
		// Pop off list
		BackupClientInodeToIDMap *toDel = rVector.back();
		rVector.pop_back();
		
		// Close and delete
		toDel->Close();
		delete toDel;
	}
	ASSERT(rVector.size() == 0);
}

#ifndef BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
// --------------------------------------------------------------------------
//
// Function
//		Name:    MakeMapBaseName(unsigned int, std::string &)
//		Purpose: Makes the base name for a inode map
//		Created: 20/11/03
//
// --------------------------------------------------------------------------
void BackupProgressPanel::MakeMapBaseName(unsigned int MountNumber, 
	std::string &rNameOut) const
{
	std::string dir;
	assert(mpConfig->DataDirectory.GetInto(dir));

	// Make a leafname
	std::string leaf(mIDMapMounts[MountNumber]);
	for(unsigned int z = 0; z < leaf.size(); ++z)
	{
		if(leaf[z] == DIRECTORY_SEPARATOR_ASCHAR)
		{
			leaf[z] = '_';
		}
	}

	// Build the final filename
	rNameOut = dir + DIRECTORY_SEPARATOR "mnt" + leaf;
}
#endif

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::DeleteUnusedRootDirEntries(BackupClientContext &)
//		Purpose: Deletes any unused entries in the root directory, if they're scheduled to be deleted.
//		Created: 13/5/04
//
// --------------------------------------------------------------------------
void BackupProgressPanel::DeleteUnusedRootDirEntries(BackupClientContext &rContext)
{
	if (mUnusedRootDirEntries.empty() || mDeleteUnusedRootDirEntriesAfter == 0)
	{
		// Nothing to do.
		return;
	}
	
	// Check time
	if (GetCurrentBoxTime() < mDeleteUnusedRootDirEntriesAfter)
	{
		// Too early to delete files
		return;
	}

	// Entries to delete, and it's the right time to do so...
	::syslog(LOG_INFO, "Deleting unused locations from store root...");
	BackupProtocolClient &connection(rContext.GetConnection());
	
	typedef std::vector<std::pair<int64_t,std::string> > tUnusedRootDirEntries;
	
	for (tUnusedRootDirEntries::iterator i = mUnusedRootDirEntries.begin(); 
		i != mUnusedRootDirEntries.end(); i++)
	{
		connection.QueryDeleteDirectory(i->first);
		
		// Log this
		::syslog(LOG_INFO, "Deleted %s (ID %08llx) from store root", 
			i->second.c_str(), i->first);
	}

	// Reset state
	mDeleteUnusedRootDirEntriesAfter = 0;
	mUnusedRootDirEntries.clear();
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::FindLocationPathName(const std::string &, std::string &) const
//		Purpose: Tries to find the path of the root of a backup location. Returns true (and path in rPathOut)
//				 if it can be found, false otherwise.
//		Created: 12/11/03
//
// --------------------------------------------------------------------------
bool BackupProgressPanel::FindLocationPathName(
	const std::string &rLocationName, std::string &rPathOut) const
{
	// Search for the location
	typedef std::vector<LocationRecord *> tLocationRecords;
	for (tLocationRecords::const_iterator i = mLocations.begin(); 
		i != mLocations.end(); i++)
	{
		if((*i)->mName == rLocationName)
		{
			rPathOut = (*i)->mPath;
			return true;
		}
	}
	
	// Didn't find it
	return false;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupProgressPanel::LocationRecord::LocationRecord()
//		Purpose: Constructor
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
BackupProgressPanel::LocationRecord::LocationRecord()
	: mIDMapIndex(0),
	  mpExcludeFiles(0),
	  mpExcludeDirs(0)
{
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupProgressPanel::LocationRecord::~LocationRecord()
//		Purpose: Destructor
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
BackupProgressPanel::LocationRecord::~LocationRecord()
{
	// Clean up exclude locations
	if (mpExcludeDirs != 0)
	{
		delete mpExcludeDirs;
		mpExcludeDirs = 0;
	}
	if (mpExcludeFiles != 0)
	{
		delete mpExcludeFiles;
		mpExcludeFiles = 0;
	}
}
