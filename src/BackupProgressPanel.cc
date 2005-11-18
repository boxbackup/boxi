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

#include <errno.h>
#include <mntent.h>
#include <stdio.h>
#include <regex.h>

#include <wx/statbox.h>
#include <wx/listbox.h>
#include <wx/button.h>
#include <wx/gauge.h>
#include <wx/dir.h>

#define NDEBUG
#define EXCLUDELIST_IMPLEMENTATION_REGEX_T_DEFINED
#include "Box.h"
#include "BackupClientContext.h"
#include "BackupClientDirectoryRecord.h"
#include "BackupClientCryptoKeys.h"
#include "FileModificationTime.h"
#include "MemBlockStream.h"
#include "BackupStoreConstants.h"
#undef EXCLUDELIST_IMPLEMENTATION_REGEX_T_DEFINED
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
	
	mpErrorList = new wxListBox(this, wxID_ANY);
	pErrorsBox->Add(mpErrorList, 1, wxGROW | wxALL, 8);

	wxStaticBoxSizer* pStatsBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Statistics"));
	pMainSizer->Add(pStatsBox, 0, wxGROW | wxALL, 8);
	
	SetSizer( pMainSizer );
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

	int maxFileTimeInFutureSecs = 0;
	mpConfig->MaxFileTimeInFuture.GetInto(maxFileTimeInFutureSecs);
	params.mMaxFileTimeInFuture = SecondsToBoxTime(
		(uint32_t)maxFileTimeInFutureSecs);
	
	params.mpCommandSocket = NULL;
	
	// Set store marker - haven't contacted the store yet
	int64_t clientStoreMarker = BackupClientContext::ClientStoreMarker_NotKnown;
	clientContext.SetClientStoreMarker(clientStoreMarker);
}

#ifdef PLATFORM_USES_MTAB_FILE_FOR_MOUNTS
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
	if(!mLocations.empty())
	{
		// Looks correctly set up
		return;
	}

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
			BackupProtocolClientListDirectory::Flags_Deleted | BackupProtocolClientListDirectory::Flags_OldVersion, // exclude old/deleted stuff
			false /* no attributes */));

	// Retrieve the directory from the stream following
	BackupStoreDirectory dir;
	std::auto_ptr<IOStream> dirstream(connection.ReceiveStream());
	dir.ReadFromStream(*dirstream, connection.GetTimeout());
	
	// Map of mount names to ID map index
	std::map<std::string, int> mounts;
	int numIDMaps = 0;

#ifdef PLATFORM_USES_MTAB_FILE_FOR_MOUNTS
	// Linux can't tell you where a directory is mounted. So we have to
	// read the mount entries from /etc/mtab! Bizarre that the OS itself
	// can't tell you, but there you go.
	std::set<std::string, mntLenCompare> mountPoints;
	// BLOCK
	FILE *mountPointsFile = 0;
	try
	{
		// Open mounts file
		mountPointsFile = ::setmntent("/etc/mtab", "r");
		if(mountPointsFile == 0)
		{
			THROW_EXCEPTION(CommonException, OSFileError);
		}
		
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
		if(mountPointsFile != 0)
		{
			::endmntent(mountPointsFile);
		}
		throw;
	}
	// Check sorting and that things are as we expect
	ASSERT(mountPoints.size() > 0);
#ifndef NDEBUG
	{
		std::set<std::string, mntLenCompare>::const_reverse_iterator i(mountPoints.rbegin());
		ASSERT(*i == "/");
	}
#endif // n NDEBUG
#endif // PLATFORM_USES_MTAB_FILE_FOR_MOUNTS

	// Then... go through each of the entries in the configuration,
	// making sure there's a directory created for it.
	typedef const std::vector<Location*> tLocations;
	tLocations& rLocations = mpConfig->GetLocations();
	
	for (tLocations::const_iterator i = rLocations.begin();
		i != rLocations.end(); i++)
	{
TRACE0("new location\n");
		// Create a record for it
		Location* pLocation = *i;
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
#ifdef PLATFORM_USES_MTAB_FILE_FOR_MOUNTS
				// Warn in logs if the directory isn't absolute
				if(pLocRecord->mPath[0] != '/')
				{
					::syslog(LOG_ERR, "Location path '%s' isn't absolute", 
						pLocRecord->mPath.c_str());
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
#else
				// BSD style statfs -- includes mount point, which is nice.
				struct statfs s;
				if(::statfs(pLocRecord->mPath.c_str(), &s) != 0)
				{
					THROW_EXCEPTION(CommonException, OSFileError)
				}
				
				// Where the filesystem is mounted
				std::string mountName(s.f_mntonname);
#endif
				
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
			BackupStoreFilenameClear dirname(pLocRecord->mName);	// generate the filename
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
	if(dir.GetNumberOfEntries() > 0)
	{
		::syslog(LOG_INFO, "%d redundant locations in root directory found, "
			"will delete from store after %d seconds.",
			dir.GetNumberOfEntries(), 
			BACKUP_DELETE_UNUSED_ROOT_ENTRIES_AFTER);

		// Store directories in list of things to delete
		mUnusedRootDirEntries.clear();
		BackupStoreDirectory::Iterator iter(dir);
		BackupStoreDirectory::Entry *en = 0;
		while((en = iter.Next()) != 0)
		{
			// Add name to list
			BackupStoreFilenameClear clear(en->GetName());
			const std::string &name(clear.GetClearFilename());
			mUnusedRootDirEntries.push_back(std::pair<int64_t,std::string>(en->GetObjectID(), name));
			// Log this
			::syslog(LOG_INFO, "Unused location in root: %s", name.c_str());
		}
		ASSERT(mUnusedRootDirEntries.size() > 0);
		// Time to delete them
		mDeleteUnusedRootDirEntriesAfter =
			GetCurrentBoxTime() + SecondsToBoxTime((uint32_t)BACKUP_DELETE_UNUSED_ROOT_ENTRIES_AFTER);
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
