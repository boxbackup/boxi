/***************************************************************************
 *            BackupFilesPanel.cc
 *
 *  Tue Mar  1 00:24:16 2005
 *  Copyright  2005  Chris Wilson
 *  anjuta@qwirx.com
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex.h>
#include <errno.h>

#include <iostream>

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/mstream.h>
#include <wx/image.h>
#include <wx/icon.h>

#include "BackupFilesPanel.h"
#include "StaticImage.h"

LocalFileTreeNode::LocalFileTreeNode() {
	checked      = FALSE;
	mpConfig     = NULL;
	mpTreeCtrl   = NULL;
	mpRootNode   = NULL;
	mpParentNode = NULL;
	mpLocation   = NULL;
	mpExcludedBy = NULL;
	mpIncludedBy = NULL;
	mpServerConnection = NULL;
	mBoxFileId = ID_UNKNOWN;
	mFoundOnServer = FALSE;
}

LocalFileTreeNode::~LocalFileTreeNode() {
	
}

void LocalFileTreeNode::AddChildren() {
	ShowChildren(NULL);
}

void LocalFileTreeNode::ShowChildren(wxListCtrl *targetList) {
	mpRootNode->mpTreeCtrl->SetCursor(*wxHOURGLASS_CURSOR);
	wxSafeYield();
	
	try {
		ScanChildrenAndList(targetList, false);
	} catch (std::exception& e) {
		mpRootNode->mpTreeCtrl->SetCursor(*wxSTANDARD_CURSOR);
		throw(e);
	}
	
	mpRootNode->mpTreeCtrl->SetCursor(*wxSTANDARD_CURSOR);
}

void LocalFileTreeNode::ScanChildrenAndList(wxListCtrl *targetList,
	bool recursive) 
{
	wxDir dir(mFullPath);
	
	if ( !dir.IsOpened() )
    {
        // deal with the error here - wxDir would already show
		// an error message explaining the exact reason of the failure
        return;
    }

	// delete any existing children of the parent
	mpTreeCtrl->DeleteChildren(mTreeId);
	
	if (targetList != NULL)
		targetList->DeleteAllItems();
	
	// update the exclude status of this node 
	// and its parents, just once for efficiency (!)
	this->UpdateExcludedState(TRUE);
	
	wxString theCurrentFileName;
	
	bool cont = dir.GetFirst(&theCurrentFileName);
    while ( cont )
    {
		// add to the tree
		LocalFileTreeNode *newNode = new LocalFileTreeNode();
		newNode->mTreeId = mpTreeCtrl->AppendItem(this->mTreeId,
			theCurrentFileName, -1, -1, newNode);
		newNode->mpTreeCtrl = this->mpTreeCtrl;
		newNode->mpRootNode = this->mpRootNode;
		newNode->mpParentNode = this;
		newNode->mFileName = theCurrentFileName;
		newNode->mpServerSettings  = mpServerSettings;
		newNode->mpServerConnection = mpServerConnection;
		
		if (mpServerConnection->IsConnected() ||
			mpServerSettings->mConnectOnDemand)
		{
			newNode->mServerStateKnown = newNode->GetBoxFileId();
		}
		else 
		{
			newNode->mBoxFileId = ID_UNKNOWN;
			newNode->mServerStateKnown = FALSE;
		}
		
		wxFileName fileNameObject = wxFileName(mFullPath, theCurrentFileName);
		newNode->mFullPath = fileNameObject.GetFullPath();
		newNode->mIsDirectory = wxFileName::DirExists(newNode->mFullPath);
		
		if (newNode->mIsDirectory)
		{
			if (recursive) {
				newNode->ScanChildrenAndList(NULL, false);
			} else {
				mpTreeCtrl->SetItemHasChildren(newNode->mTreeId, TRUE);
			}
		}

        cont = dir.GetNext(&theCurrentFileName);
    }
}

void LocalFileTreeNode::UpdateExcludedState(bool updateParents) 
{
	if (updateParents && mpParentNode != NULL)
		mpParentNode->UpdateExcludedState(TRUE);
	
	// by default, inherit our include/exclude state
	// from our parent node, if we have one
	
	if (mpParentNode) {
		mpLocation = mpParentNode->mpLocation;
		mpExcludedBy = mpParentNode->mpExcludedBy;
		mpIncludedBy = mpParentNode->mpIncludedBy;
		mLocationString = mpParentNode->mLocationString;
		mExcludedByString = mpParentNode->mExcludedByString;
		mIncludedByString = mpParentNode->mIncludedByString;
		mExcluded = mpParentNode->mExcluded;
	} else {
		mpLocation    = NULL;
		mpExcludedBy  = NULL;
		mpIncludedBy  = NULL;
		mLocationString = wxT("(none)");
		mExcludedByString = wxT("");
		mIncludedByString = wxT("");
		mExcluded = TRUE;
	}
		
	const std::vector<Location*>& rLocations = 
		mpRootNode->mpConfig->GetLocations();

	if (!mpLocation) {
		// determine whether or not this node's path
		// is inside a backup location.
	
		for (size_t i = 0; i < rLocations.size(); i++) {
			Location* pLoc = rLocations[i];
			const wxString& rPath = pLoc->GetPath();
			// std::cout << "Compare " << mFullPath 
			//	<< " against " << rPath << "\n";
			if (mFullPath.CompareTo(rPath) == 0) {
				// std::cout << "Found location: " << pLoc->GetName() << "\n";
				mpLocation = pLoc;
				break;
			}
		}
		
		// if this node doesn't belong to a location, then by definition
		// our parent node doesn't have one either, so the inherited
		// values are fine, leave them alone.
		
		if (!mpLocation)
			return;
		
		// default for a location root is included (not excluded)
		
		mExcluded = FALSE;
		mLocationString = mpLocation->GetName();
		mLocationString.Append(wxT(" ("));
		mLocationString.Append(mpLocation->GetPath());
		mLocationString.Append(wxT(")"));
	}
	
	if (!(mFullPath.StartsWith(mpLocation->GetPath().c_str(), 
			&mLocationPath))) 
	{
		wxMessageBox(
			wxT("Full path does not start with location path!"),
			wxT("Boxi Error"), wxOK | wxICON_ERROR, NULL);
	}
		
	// std::cout << "Checking " << mFullPath << " against exclude list for " 
	// 	<< mpLocation->GetPath() << "\n";

	const std::vector<MyExcludeEntry*>& rExcludeList =
		mpLocation->GetExcludeList().GetEntries();
	
	// on pass 1, remove Excluded files
	// on pass 2, re-add AlwaysInclude files
	
	for (int pass = 1; pass <= 2; pass++) {
		// std::cout << "Pass " << pass << "\n";

		if (pass == 1 && mExcluded) {
			// not included, so don't bother checking the Exclude entries
			continue;
		} else if (pass == 2 && !mExcluded) {
			// not excluded, so don't bother checking the AlwaysInclude entries
			continue;
		}
		
		for (size_t i = 0; i < rExcludeList.size(); i++) {
			MyExcludeEntry* pExclude = rExcludeList[i];
			ExcludeMatch match = pExclude->GetMatch();
			std::string  value = pExclude->GetValue();
			wxString value2(value.c_str(), wxConvLibc);
			bool matched = false;
			
			// std::cout << "Checking against " << pExclude->ToString() << ": ";
			
			ExcludeSense sense = pExclude->GetSense();
			if (pass == 1 && sense != ES_EXCLUDE) {
				// std::cout << "Not an Exclude entry\n";
				continue;
			}
			if (pass == 2 && sense != ES_ALWAYSINCLUDE) {
				// std::cout << "Not an AlwaysInclude entry\n";
				continue;
			}
			
			ExcludeFileDir fileOrDir = pExclude->GetFileDir();
			if (fileOrDir == EFD_FILE && mIsDirectory) {
				// std::cout << "Doesn't match directories\n";
				continue;
			}
			if (fileOrDir == EFD_DIR && !mIsDirectory) {
				// std::cout << "Doesn't match files\n";
				continue;
			}
			
			if (match == EM_EXACT) {
				if (mFullPath.IsSameAs(value2))
				{
					// std::cout << "Exact match!\n";
					matched = true;
				} 
				else 
				{
					// std::cout << "No exact match\n";
				}
			} else if (match == EM_REGEX) {
				std::auto_ptr<regex_t> apr = 
					std::auto_ptr<regex_t>(new regex_t);
				if (::regcomp(apr.get(), value.c_str(),
					REG_EXTENDED | REG_NOSUB) != 0) 
				{
					wxLogError(wxT("Regular expression "
						"compile failed (%s)"),
						value2.c_str());
				}
				else
				{
					wxCharBuffer buf = 
						mFullPath.mb_str(wxConvLibc);
					int result = regexec(apr.get(), 
						buf.data(), 0, 0, 0);
					matched = (result == 0);
				}
			}
			
			if (!matched) {
				// std::cout << "No match.\n";
				continue;
			}
			
			// std::cout << "Matched!\n";
			
			if (sense == ES_EXCLUDE) {
				mExcluded = TRUE;
				mpExcludedBy = pExclude;
				mExcludedByString = wxString(
					pExclude->ToString().c_str(),
					wxConvLibc);
			} else if (sense == ES_ALWAYSINCLUDE) {
				mExcluded = FALSE;
				mpIncludedBy = pExclude;
				mIncludedByString = wxString(
					pExclude->ToString().c_str(),
					wxConvLibc);
			}
		}
	}
}

bool LocalFileTreeNode::GetBoxFileId()
{
	if (mBoxFileId != ID_UNKNOWN) return TRUE;
		
	mFoundOnServer = FALSE;
	if (!mpLocation) 
	{
		// std::cout << mFileName << ": no location\n";
		return FALSE;
	}
		
	int64_t theParentDirectoryId = ID_UNKNOWN;
	
	if (!mpParentNode) 
	{
		// reached root of tree without finding a location root!
		// std::cout << mFileName << ": reached root without finding location\n";
		return FALSE;
	}
	
	wxString LookupName;
	
	if (!(mpParentNode->mpLocation)) 
	{
		// this is a location root
		theParentDirectoryId = 
			BackupProtocolClientListDirectory::RootDirectory;
		LookupName = mpLocation->GetName();
	} 
	else 
	{
		if (mpParentNode->mBoxFileId == ID_UNKNOWN)
		{
			if (!(mpParentNode->GetBoxFileId()))
			{
				// std::cout << mFileName << ": failed to stat parent\n";
				return FALSE;
			}
		}
		
		theParentDirectoryId = mpParentNode->mBoxFileId;
		LookupName = mFileName;
	}
	
	assert(theParentDirectoryId != ID_UNKNOWN);
	BackupStoreDirectory dir;
	
	if (!(mpServerConnection->ListDirectory(theParentDirectoryId, 
		BackupProtocolClientListDirectory::Flags_EXCLUDE_NOTHING, dir))) 
	{
		// std::cout << mFileName << ": failed to list directory\n";
		return FALSE;
	}

	BackupStoreDirectory::Iterator i(dir);
	wxCharBuffer buf = LookupName.mb_str(wxConvLibc);
	BackupStoreFilenameClear fn(buf.data());
	BackupStoreDirectory::Entry *en = i.FindMatchingClearName(fn);
	
	if(en == 0) 
	{
		// std::cout << mFileName << ": not found in directory\n"; 
		return FALSE;
	}
	
	// BackupStoreFilenameClear clear(en->GetName());
	// std::cout << clear.GetClearFilename().c_str() << "\n";
	
	mBoxFileId = en->GetObjectID();
	
	time_t LastModTimeT = BoxTimeToSeconds(en->GetModificationTime());
	mBoxFileLastModified = wxDateTime(LastModTimeT);
	
	mFoundOnServer = TRUE;
	return TRUE;
}
	

BEGIN_EVENT_TABLE(BackupFilesPanel, wxPanel)
	EVT_TREE_ITEM_EXPANDING(ID_Backup_Files_Tree, 
		BackupFilesPanel::OnTreeNodeExpand)
	EVT_TREE_ITEM_COLLAPSING(ID_Backup_Files_Tree, 
		BackupFilesPanel::OnTreeNodeCollapse)
	EVT_TREE_ITEM_ACTIVATED(ID_Backup_Files_Tree, 
		BackupFilesPanel::OnTreeNodeActivated)
	EVT_TREE_SEL_CHANGED(ID_Backup_Files_Tree, 
		BackupFilesPanel::OnTreeNodeSelect)
	EVT_CHECKBOX(ID_Local_ServerConnectCheckbox,
		BackupFilesPanel::OnServerConnectClick)
END_EVENT_TABLE()

static int AddImage(
	const struct StaticImage & rImageData, 
	wxImageList & rImageList)
{
	wxMemoryInputStream byteStream(rImageData.data, rImageData.size);
	wxImage img(byteStream, wxBITMAP_TYPE_PNG);
	return rImageList.Add(img);
}

BackupFilesPanel::BackupFilesPanel(
	ClientConfig *config,
	ServerConnection* pServerConnection,
	wxWindow* parent, 
	wxWindowID id,
	const wxPoint& pos, 
	const wxSize& size,
	long style, 
	const wxString& name)
	: wxPanel(parent, id, pos, size, style, name)
{
	mpConfigListener = new MyConfigChangeListener(this);
	theConfig = config;
	// theConfig->AddListener(mpConfigListener);
	mpServerConnection = pServerConnection;
	mpServerSettings = new LocalServerSettings();
	mpServerSettings->mConnectOnDemand = FALSE;
	
	mpMainSizer = new wxBoxSizer( wxVERTICAL );

	theBackupFileTree = new wxTreeCtrl(this, ID_Backup_Files_Tree,
		wxDefaultPosition, wxDefaultSize, 
		wxTR_HAS_BUTTONS | wxSUNKEN_BORDER | wxTR_LINES_AT_ROOT);
	mpMainSizer->Add(theBackupFileTree, 1, wxGROW | wxALL, 8);
	
	// wxPanel* theBottomPanel = new wxPanel(this);
	
	// wxBoxSizer *theBottomPanelSizer = new wxBoxSizer( wxVERTICAL );
	
	mpBottomInfoSizer = new wxGridSizer(2, 4, 4);

	mpFullPathLabel = new wxStaticText(this, -1, wxT(""));
	AddParam(this, wxT("Full Path:"), mpFullPathLabel, false, 
		mpBottomInfoSizer);

	mpLocationLabel = new wxStaticText(this, -1, wxT(""));
	AddParam(this, wxT("Location:"), mpLocationLabel, false, 
		mpBottomInfoSizer);

	mpExcludedByLabel = new wxStaticText(this, -1, wxT(""));
	AddParam(this, wxT("Excluded By:"), mpExcludedByLabel, false, 
		mpBottomInfoSizer);

	mpIncludedByLabel = new wxStaticText(this, -1, wxT(""));
	AddParam(this, wxT("Included By:"), mpIncludedByLabel, false, 
		mpBottomInfoSizer);

	mpExcludeResultLabel = new wxStaticText(this, -1, wxT(""));
	AddParam(this, wxT("Result:"), mpExcludeResultLabel, false, 
		mpBottomInfoSizer);
		
	mpLocalVersionLabel = new wxStaticText(this, -1, wxT(""));
	AddParam(this, wxT("Local Version:"), mpLocalVersionLabel, 
		false, mpBottomInfoSizer);

	mpRemoteVersionLabel = new wxStaticText(this, -1, wxT(""));
	AddParam(this, wxT("Remote Version:"), mpRemoteVersionLabel, 
		false, mpBottomInfoSizer);

	mpServerConnectCheckbox = new wxCheckBox(
		this, ID_Local_ServerConnectCheckbox, 
		wxT("Connect to Server"));
	AddParam(this, wxT("Status on Server:"), mpServerConnectCheckbox, 
		false, mpBottomInfoSizer);

	mpServerStatus = new wxStaticText(this, -1, wxT(""));
	mpServerStatus->Hide();

	// theBottomPanelSizer->Add(theBottomInfoSizer, 1, wxGROW | wxALL, 0);
	// theBottomPanel->SetSizer(theBottomInfoSizer);
	mpMainSizer->Add(mpBottomInfoSizer, 0, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	SetSizer( mpMainSizer );
	mpMainSizer->SetSizeHints( this );

	LocalFileTreeNode *rootNode = new LocalFileTreeNode();
	rootNode->mFileName		= wxT("");
	rootNode->mpTreeCtrl		= theBackupFileTree;
	rootNode->mpConfig		= config;
	rootNode->mpRootNode		= rootNode;
	rootNode->mpParentNode		= NULL;
	#ifdef __CYGWIN__
	rootNode->mFullPath		= wxT("c:\\");
	#else
	rootNode->mFullPath		= wxT("/");
	#endif
	rootNode->mpServerSettings  = mpServerSettings;
	rootNode->mpServerConnection = mpServerConnection;
	rootNode->mBoxFileId = LocalFileTreeNode::ID_UNKNOWN;

	wxString rootName;
	rootName.Printf(wxT("%s (local root)"), 
			rootNode->mFullPath.c_str());

	theTreeRootItem = theBackupFileTree->AddRoot(
		rootName, -1, -1, rootNode);
	rootNode->mTreeId = theTreeRootItem;
	
	theBackupFileTree->SetItemHasChildren(theTreeRootItem, TRUE);
	
	wxInitAllImageHandlers();
	
	mCheckedImageId  = AddImage(tick16_png,      mTreeImages);
	mCrossedImageId  = AddImage(cross16_png,     mTreeImages);
	mUnknownImageId  = AddImage(unknown16_png,   mTreeImages);
	mWaitingImageId  = AddImage(hourglass16_png, mTreeImages);
	mEqualImageId    = AddImage(equal16_png,     mTreeImages);
	mNotEqualImageId = AddImage(notequal16_png,  mTreeImages);
	mSameTimeImageId = AddImage(sametime16_png,  mTreeImages);
	mOldFileImageId  = AddImage(oldfile16_png,   mTreeImages);
	mExclamImageId   = AddImage(exclam16_png,    mTreeImages);
	mFolderImageId   = AddImage(folder16_png,    mTreeImages);
	mPartialImageId  = AddImage(partial16_png,   mTreeImages);
	mAlienImageId    = AddImage(alien16_png,     mTreeImages);

	theBackupFileTree->SetImageList(&mTreeImages);
	theBackupFileTree->SetItemImage(theTreeRootItem, -1, 
		wxTreeItemIcon_Normal);
}

BackupFilesPanel::~BackupFilesPanel()
{
	delete mpConfigListener;
}

void BackupFilesPanel::OnTreeNodeExpand(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	LocalFileTreeNode *node = 
		(LocalFileTreeNode *)(theBackupFileTree->GetItemData(item));
	node->AddChildren();
	UpdateExcludedState(node->mTreeId);
}

void BackupFilesPanel::OnTreeNodeCollapse(wxTreeEvent& event)
{

}

void BackupFilesPanel::OnTreeNodeActivated(wxTreeEvent& event)
{
	/*
	wxTreeItemId item = event.GetItem();
	
	LocalFileTreeNode *node = 
		(LocalFileTreeNode *)(theBackupFileTree->GetItemData(item));
	
	node->SetChecked(! node->IsChecked() );
	
	theBackupFileTree->SetItemImage(item, 
		node->IsChecked() ? 1 : 0, wxTreeItemIcon_Normal);
	*/
}

void BackupFilesPanel::OnTreeNodeSelect(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	LocalFileTreeNode *node = 
		(LocalFileTreeNode *)(theBackupFileTree->GetItemData(item));
	mpFullPathLabel->SetLabel(node->mFullPath);
	mpLocationLabel->SetLabel(node->mLocationString);
	mpExcludedByLabel->SetLabel(node->mExcludedByString);
	mpIncludedByLabel->SetLabel(node->mIncludedByString);
	mpExcludeResultLabel->SetLabel(
		node->mExcluded ? wxT("Excluded") : wxT("Included"));
	
	{
		wxString TimeString;
		TimeString.Append(node->mLocalFileLastModified.FormatISODate());
		TimeString.Append(wxT(" "));
		TimeString.Append(node->mLocalFileLastModified.FormatISOTime());
		mpLocalVersionLabel->SetLabel(TimeString);
	}
	
	if (! (mpServerConnection->IsConnected()) &&
		! (mpServerSettings->mConnectOnDemand))
	{
		mpRemoteVersionLabel->SetLabel(wxT("Not connected"));
		mpServerStatus->SetLabel(wxT("Not connected"));
		return;
	}
	
	if (!(node->GetBoxFileId()))
	{
		std::cout << "Can't get file ID\n";
		return;
	}
	
	{
		wxString RemoteVersion;
		RemoteVersion.Append(node->mBoxFileLastModified.FormatISODate());
		RemoteVersion.Append(wxT(" "));
		RemoteVersion.Append(node->mBoxFileLastModified.FormatISOTime());
		mpRemoteVersionLabel->SetLabel(RemoteVersion);
	}
	
	if (node->mLocalFileLastModified.IsEqualTo(node->mBoxFileLastModified))
	{
		mpServerStatus->SetLabel(wxT("Backed up OK!"));
	}
	else 
	{
		wxString msg;
		msg.Printf(
			wxT("Version on server is out of date (file %lld)"),
			node->mBoxFileId);
		mpServerStatus->SetLabel(msg);
		wxString ServerPath = node->mLocationString 
			+ wxT("/") + node->mLocationPath;
	}
	
	UpdateExcludedState(node->mTreeId);
}

void BackupFilesPanel::OnServerConnectClick(wxCommandEvent& event)
{
	mpServerConnectCheckbox->Hide();
	mpBottomInfoSizer->Remove(mpServerConnectCheckbox);
	mpBottomInfoSizer->Add(mpServerStatus, 0, wxALIGN_CENTER_VERTICAL, 0);
	mpServerStatus->Show();
	mpBottomInfoSizer->Layout();
	mpServerSettings->mConnectOnDemand = TRUE;
}

void BackupFilesPanel::MyConfigChangeListener::NotifyChange() {
	mpPanel->UpdateExcludedState(mpPanel->GetTreeRootItemId());
}

void BackupFilesPanel::UpdateExcludedState(wxTreeItemId &rNodeId) 
{
	SetCursor(*wxHOURGLASS_CURSOR);
	wxSafeYield();
	UpdateExcludedStatePrivate(rNodeId);
	SetCursor(*wxSTANDARD_CURSOR);
}

void BackupFilesPanel::UpdateExcludedStatePrivate(wxTreeItemId &rNodeId) 
{
	LocalFileTreeNode* pNode = (LocalFileTreeNode*)
		(theBackupFileTree->GetItemData(rNodeId));
	pNode->UpdateExcludedState(FALSE);

	pNode->mServerState = LocalFileTreeNode::SS_UNKNOWN;

	{
		struct stat st;
		wxCharBuffer buf = pNode->mFullPath.mb_str(wxConvLibc);
		if (::stat(buf.data(), &st) 
			== 0) 
		{
			pNode->mLocalFileLastModified = wxDateTime(st.st_mtime);
			if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))
				pNode->mServerState = LocalFileTreeNode::SS_ALIEN;
		} 
		else 
		{
// System files are unreadable under Cygwin?
#ifndef __CYGWIN__
			wxString msg;
			msg.Printf(wxT("Error getting information about "
					"local file %s:\n\n%s"),
				pNode->mFullPath.c_str(), strerror(errno));
			wxMessageBox(msg, wxT("Boxi Error"), 
				wxOK | wxICON_ERROR, NULL);
#endif
		}
	}
	
	if (mpServerConnection->IsConnected() ||
		mpServerSettings->mConnectOnDemand)
	{
		// std::cout << pNode->mFileName << ": getting file state: ";
		// if (
		pNode->GetBoxFileId();
		// )
		// std::cout << "succeeded\n";
		// else
		// std::cout << "failed\n";
	}
	
	if (pNode->mServerState == LocalFileTreeNode::SS_ALIEN) 
	{
		// don't change it
	} 
	else if (!(pNode->mpLocation)) 
	{
		pNode->mServerState = LocalFileTreeNode::SS_OUTSIDE;
	} 
	else if (pNode->mExcluded) 
	{
		pNode->mServerState = LocalFileTreeNode::SS_EXCLUDED;
	} 
	else if (!(mpServerSettings->mConnectOnDemand))
	{
		// included by definition
		pNode->mServerState = LocalFileTreeNode::SS_UNKNOWN;
	}
	else if (!(pNode->mFoundOnServer))
	{
		if (pNode->mLocalFileLastModified.IsLaterThan(
			wxDateTime::Now().Subtract(wxTimeSpan(0, 0, 21800))))
			pNode->mServerState = LocalFileTreeNode::SS_TOONEW;
		else
			pNode->mServerState = LocalFileTreeNode::SS_MISSING;
	}
	else if (pNode->mIsDirectory)
	{
		pNode->mServerState = LocalFileTreeNode::SS_DIRECTORY;
	}
	else if (pNode->mLocalFileLastModified.IsLaterThan(pNode->mBoxFileLastModified))
	{
		pNode->mServerState = LocalFileTreeNode::SS_OUTOFDATE;
	}
	else if (pNode->mLocalFileLastModified.IsEqualTo(pNode->mBoxFileLastModified))
	{
		pNode->mServerState = LocalFileTreeNode::SS_UPTODATE;
	}
	else
	{
		// std::cout << pNode->mFileName << ": something funny!\n";
		// std::cout << "Local mod: " << pNode->mLocalFileLastModified.Format() << "\n";
		// std::cout << "Remote mod: " << pNode->mBoxFileLastModified.Format() << "\n";
		pNode->mServerState = LocalFileTreeNode::SS_UNKNOWN;
	}
		
	wxTreeItemIdValue cookie;
	wxTreeItemId itemId = theBackupFileTree->GetFirstChild(rNodeId, cookie);

	bool HasDirectoryChild = FALSE;
	bool HasMissingChild   = FALSE;
	
	while (itemId.IsOk()) {
		UpdateExcludedStatePrivate(itemId);
		
		LocalFileTreeNode* pChildNode = (LocalFileTreeNode*)
			(theBackupFileTree->GetItemData(itemId));
		
		if (pChildNode->mIsDirectory)
			HasDirectoryChild = TRUE;
		if (pChildNode->mServerState != LocalFileTreeNode::SS_UPTODATE &&
			pChildNode->mServerState != LocalFileTreeNode::SS_DIRECTORY)
			HasMissingChild = TRUE;
		
		itemId = theBackupFileTree->GetNextChild(rNodeId, cookie);
	}
	
	if (pNode->mIsDirectory)
	{
		if (HasMissingChild)
			pNode->mServerState = LocalFileTreeNode::SS_PARTIAL;
	}
	
	switch (pNode->mServerState)
	{
		case LocalFileTreeNode::SS_ALIEN:
			theBackupFileTree->SetItemImage(rNodeId, mAlienImageId);
			break;

		case LocalFileTreeNode::SS_OUTSIDE:
			theBackupFileTree->SetItemImage(rNodeId, -1);
			break;
		
		case LocalFileTreeNode::SS_EXCLUDED:
			theBackupFileTree->SetItemImage(rNodeId, mCrossedImageId);
			break;
		
		case LocalFileTreeNode::SS_UNKNOWN:
			if (mpServerSettings->mConnectOnDemand)
				theBackupFileTree->SetItemImage(rNodeId, mUnknownImageId);
			else 
				theBackupFileTree->SetItemImage(rNodeId, mCheckedImageId);
			break;
		
		case LocalFileTreeNode::SS_OUTOFDATE:
			theBackupFileTree->SetItemImage(rNodeId, mOldFileImageId);
			break;

		case LocalFileTreeNode::SS_TOONEW:
			theBackupFileTree->SetItemImage(rNodeId, mWaitingImageId);
			break;

		case LocalFileTreeNode::SS_MISSING:
			theBackupFileTree->SetItemImage(rNodeId, mExclamImageId);
			break;

		case LocalFileTreeNode::SS_DIRECTORY:
			theBackupFileTree->SetItemImage(rNodeId, mFolderImageId);
			break;

		case LocalFileTreeNode::SS_PARTIAL:
			theBackupFileTree->SetItemImage(rNodeId, mPartialImageId);
			break;

		case LocalFileTreeNode::SS_UPTODATE:
			theBackupFileTree->SetItemImage(rNodeId, mCheckedImageId);
			break;
		
		default:
			std::cout << "Unknown state " << pNode->mServerState << "!\n";
			theBackupFileTree->SetItemImage(rNodeId, -1);
	}
}
