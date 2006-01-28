/***************************************************************************
 *            ServerFilesPanel.cc
 *
 *  Mon Feb 28 22:45:09 2005
 *  Copyright 2005-2006 Chris Wilson
 *  chris-boxisource@qwirx.com
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

#include <iostream>

// #include <wx/artprov.h>
#include <wx/filename.h>
// #include <wx/gdicmn.h>
#include <wx/splitter.h>

#include "BackupClientRestore.h"

#include "main.h"

#include "ClientConfig.h"
#include "MainFrame.h"
#include "RestoreFilesPanel.h"
#include "StaticImage.h"

ServerFileVersion::ServerFileVersion(BackupStoreDirectory::Entry* pDirEntry)
{ 
	BackupStoreFilenameClear clear(pDirEntry->GetName());

	mBoxFileId     = pDirEntry->GetObjectID();
	mFlags         = pDirEntry->GetFlags();
	mIsDirectory   = mFlags & BackupStoreDirectory::Entry::Flags_Dir;
	mIsDeleted     = mFlags & BackupStoreDirectory::Entry::Flags_Deleted;
	mSizeInBlocks  = pDirEntry->GetSizeInBlocks();
	mDateTime      = wxDateTime(BoxTimeToSeconds(
		pDirEntry->GetModificationTime()));
	// mHasAttributes = FALSE;
	mCached = FALSE;
}

void RestoreSpec::Add(const RestoreSpecEntry& rNewEntry)
{
	for (RestoreSpec::Iterator i = mEntries.begin(); i != mEntries.end(); i++)
	{
		if (i->IsSameAs(rNewEntry))
			return;
	}
	
	mEntries.push_back(rNewEntry);
}

void RestoreSpec::Remove(const RestoreSpecEntry& rOldEntry)
{
	for (RestoreSpec::Iterator i = mEntries.begin(); i != mEntries.end(); i++)
	{
		if (i->IsSameAs(rOldEntry))
		{
			mEntries.erase(i);
			return;
		}
	}
}

class RestoreTreeNode : public FileNode 
{
	private:
	ServerCacheNode*   mpCacheNode;
	ServerSettings*    mpServerSettings;
	const RestoreSpec& mrRestoreSpec;
	const RestoreSpecEntry* mpMatchingEntry;
	bool               mIncluded;
	
	public:
	RestoreTreeNode
	(
		ServerCacheNode* pCacheNode,
		ServerSettings* pServerSettings, 
		const RestoreSpec& rRestoreSpec
	) 
	:	FileNode        (),
		mpCacheNode     (pCacheNode),
		mpServerSettings(pServerSettings),
		mrRestoreSpec   (rRestoreSpec),
		mpMatchingEntry (NULL),
		mIncluded       (FALSE)
	{ }

	RestoreTreeNode
	(
		RestoreTreeNode* pParent, 
		ServerCacheNode* pCacheNode
	)
	:	FileNode(pParent),
		mpCacheNode     (pCacheNode),
		mpServerSettings(pParent->mpServerSettings),
		mrRestoreSpec   (pParent->mrRestoreSpec),
		mpMatchingEntry (NULL),
		mIncluded       (FALSE)
	{ }

	// bool ShowChildren(wxListCtrl *targetList);

	ServerCacheNode* GetCacheNode() const { return mpCacheNode; }
	
	virtual const wxString& GetFileName() const 
	{ return mpCacheNode->GetFileName(); }
	
	virtual const wxString& GetFullPath() const 
	{ return mpCacheNode->GetFullPath(); }

	const bool IsDirectory() const 
	{ 
		return mpCacheNode->GetMostRecent()->IsDirectory();
	}
	
	const bool IsDeleted() const 
	{ 
		return mpCacheNode->GetMostRecent()->IsDirectory();
	}
	
	const RestoreSpecEntry* GetMatchingEntry() const { return mpMatchingEntry; }
	
	bool AddChildren(bool recurse);
	virtual int UpdateState(FileImageList& rImageList, bool updateParents);
	
	private:
	virtual bool _AddChildrenSlow(wxTreeCtrl* pTreeCtrl, bool recurse);
};

bool RestoreTreeNode::_AddChildrenSlow(wxTreeCtrl* pTreeCtrl, bool recursive) 
{
	// delete any existing children of the parent
	pTreeCtrl->DeleteChildren(GetId());
	
	const ServerCacheNode::Vector* pChildren = mpCacheNode->GetChildren();
	
	if (!pChildren)
	{
		return FALSE;
	}

	for (ServerCacheNode::ConstIterator i = pChildren->begin(); 
		i != pChildren->end(); i++)
	{
		RestoreTreeNode *pNewNode = new RestoreTreeNode(this, *i);

		wxTreeItemId newId = pTreeCtrl->AppendItem(GetId(),
				pNewNode->GetFileName(), -1, -1, pNewNode);
		pNewNode->SetId(newId);
		
		if (pNewNode->IsDeleted())
		{
			pTreeCtrl->SetItemTextColour(newId, 
				wxTheColourDatabase->Find(wxT("GREY")));
		}
			
		if (pNewNode->IsDirectory())
		{
			if (recursive) 
			{
				bool result = pNewNode->_AddChildrenSlow(pTreeCtrl, false);
				if (!result)
				{
					return FALSE;
				}
			} 
			else 
			{
				pTreeCtrl->SetItemHasChildren(newId, TRUE);
			}
		}
	}

	// sort the kids out
	pTreeCtrl->SortChildren(GetId());
	
	return TRUE;
}

int RestoreTreeNode::UpdateState(FileImageList& rImageList, bool updateParents) 
{
	RestoreTreeNode* pParentNode = (RestoreTreeNode*)GetParentNode();
	
	if (updateParents && pParentNode != NULL)
	{
		pParentNode->UpdateState(rImageList, TRUE);
	}
	
	// by default, inherit our include/exclude state
	// from our parent node, if we have one
	
	if (pParentNode) 
	{
		mpMatchingEntry = pParentNode->mpMatchingEntry;
		mIncluded       = pParentNode->mIncluded;
	}
	else
	{
		mpMatchingEntry = NULL;
		mIncluded       = FALSE;
	}

	const RestoreSpec::Vector& entries = mrRestoreSpec.GetEntries();
	for (RestoreSpec::Vector::const_iterator i = entries.begin(); 
		i != entries.end(); i++)
	{
		if (i->GetNode() == mpCacheNode)
		{
			mpMatchingEntry = &(*i);
			mIncluded = i->IsInclude();
			break;
		}
	}
	
	int iconId = rImageList.GetEmptyImageId();
	
	bool matchesParent = pParentNode && 
		pParentNode->mpMatchingEntry == mpMatchingEntry;
	
	if (!mpMatchingEntry)
	{
		// this node is not included, but if it contains one that is,
		// we should show a partial icon instead of an empty one

		wxString thisNodePathWithSlash = GetFullPath() + wxT("/");
		
		for (RestoreSpec::Vector::const_iterator i = entries.begin(); 
			i != entries.end(); i++)
		{
			wxString entryPath = i->GetNode()->GetFullPath();
			if (entryPath.StartsWith(thisNodePathWithSlash) &&
				i->IsInclude())
			{
				iconId = rImageList.GetPartialImageId();
				break;
			}
		}
	}
	else if (mpMatchingEntry->IsInclude())
	{
		if (matchesParent)
		{
			iconId = rImageList.GetCheckedGreyImageId();
		}
		else
		{
			iconId = rImageList.GetCheckedImageId();
		}
	}
	else 
	{
		if (matchesParent)
		{
			iconId = rImageList.GetCrossedGreyImageId();
		}
		else
		{
			iconId = rImageList.GetCrossedImageId();
		}
	}

	return iconId;
}

class RestoreTreeCtrl : public FileTree 
{
	public:
	RestoreTreeCtrl
	(
		wxWindow* pParent, 
		wxWindowID id,
		RestoreTreeNode* pRootNode
	)
	:	FileTree(pParent, id, pRootNode, wxT("/ (server root)"))
	{ }

	private:
	virtual int OnCompareItems(const wxTreeItemId& item1, 
		const wxTreeItemId& item2);
};

int RestoreTreeCtrl::OnCompareItems(
	const wxTreeItemId& item1, 
	const wxTreeItemId& item2)
{
	RestoreTreeNode *pNode1 = (RestoreTreeNode*)GetItemData(item1);
	RestoreTreeNode *pNode2 = (RestoreTreeNode*)GetItemData(item2);

	{
		bool dir1 = pNode1->IsDirectory();
		bool dir2 = pNode2->IsDirectory();
		
		if ( dir1 && !dir2 )
		{
			return -1;
		}
		
		if ( dir2 && !dir1 ) 
		{
			return 1;
		}
	}
	
	wxString name1 = pNode1->GetFileName();
	wxString name2 = pNode2->GetFileName();
	
	return name1.CompareTo(name2);
}

BEGIN_EVENT_TABLE(RestoreFilesPanel, wxPanel)
	EVT_TREE_SEL_CHANGING(ID_Server_File_Tree,
		RestoreFilesPanel::OnTreeNodeSelect)
	EVT_TREE_ITEM_ACTIVATED(ID_Server_File_Tree,
		RestoreFilesPanel::OnTreeNodeActivate)
	EVT_BUTTON(wxID_CANCEL, RestoreFilesPanel::OnCloseButtonClick)
/*
	EVT_BUTTON(ID_Server_File_RestoreButton, 
		RestoreFilesPanel::OnFileRestore)
	EVT_BUTTON(ID_Server_File_DeleteButton, 
		RestoreFilesPanel::OnFileDelete)
	EVT_IDLE(RestoreFilesPanel::OnIdle)
*/
END_EVENT_TABLE()

RestoreFilesPanel::RestoreFilesPanel
(
	ClientConfig*     pConfig,
	ServerConnection* pServerConnection,
	MainFrame*        pMainFrame,
	wxWindow*         pParent,
	RestoreSpecChangeListener* pListener,
	wxPanel*          pPanelToShowOnClose
)
:	wxPanel(pParent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 
		wxTAB_TRAVERSAL, wxT("RestoreFilesPanel")),
	mpMainFrame(pMainFrame),
	mpPanelToShowOnClose(pPanelToShowOnClose),
	mpListener(pListener)
{
	mpConfig = pConfig;
	// mpStatusBar = pMainFrame->GetStatusBar();
	mpServerConnection = pServerConnection;
	mpCache = new ServerCache(pServerConnection);
	
	wxBoxSizer *topSizer = new wxBoxSizer( wxVERTICAL );
	SetSizer(topSizer);

	/*	
	wxSplitterWindow *theServerSplitter = new wxSplitterWindow(this, 
		ID_Server_Splitter);
	topSizer->Add(theServerSplitter, 1, wxGROW | wxALL, 8);
	*/
	
	mpTreeRoot = new RestoreTreeNode(mpCache->GetRoot(), 
		&mServerSettings, mRestoreSpec);

	mpTreeCtrl = new RestoreTreeCtrl(this, ID_Server_File_Tree,
		mpTreeRoot);
	topSizer->Add(mpTreeCtrl, 1, wxGROW | wxALL, 8);
	
	/*
	mpTreeCtrl = new RestoreTreeCtrl(theServerSplitter, ID_Server_File_Tree,
		mpTreeRoot);

	wxPanel* theRightPanel = new wxPanel(theServerSplitter);
	wxBoxSizer *theRightPanelSizer = new wxBoxSizer( wxVERTICAL );
	
	wxStaticBox* pRestoreSelBox = new wxStaticBox(theRightPanel, wxID_ANY, 
		wxT("Selected to Restore"));

	wxStaticBoxSizer* pStaticBoxSizer = new wxStaticBoxSizer(
		pRestoreSelBox, wxVERTICAL);
	theRightPanelSizer->Add(pStaticBoxSizer, 0, wxGROW | wxALL, 8);
		
	wxGridSizer* theRightParamSizer = new wxGridSizer(2, 4, 4);
	pStaticBoxSizer->Add(theRightParamSizer, 0, wxGROW | wxALL, 4);
	
	mpCountFilesBox = new wxTextCtrl(theRightPanel, wxID_ANY, wxT(""));
	AddParam(theRightPanel, wxT("Files"), mpCountFilesBox, TRUE, 
			theRightParamSizer);

	mpCountBytesBox = new wxTextCtrl(theRightPanel, wxID_ANY, wxT(""));
	AddParam(theRightPanel, wxT("Bytes"), mpCountBytesBox, TRUE, 
			theRightParamSizer);

	wxBoxSizer *theRightButtonSizer = new wxBoxSizer( wxHORIZONTAL );
	theRightPanelSizer->Add(theRightButtonSizer, 0, wxGROW | wxALL, 4);

	wxButton *theRestoreButton = new wxButton(theRightPanel, 
		ID_Server_File_RestoreButton, wxT("Restore"), 
		wxDefaultPosition);
	theRightButtonSizer->Add(theRestoreButton, 1, wxALL, 4);

	wxButton *theCompareButton = new wxButton(theRightPanel, 
		ID_Server_File_CompareButton, wxT("Compare"), 
		wxDefaultPosition);
	theRightButtonSizer->Add(theCompareButton, 1, wxALL, 4);

	mpDeleteButton = new wxButton(theRightPanel, 
		ID_Server_File_DeleteButton, wxT("Delete"), 
		wxDefaultPosition);
	theRightButtonSizer->Add(mpDeleteButton, 1, wxALL, 4);
	
	theRightPanel->SetSizer( theRightPanelSizer );

	theServerSplitter->SetMinimumPaneSize(100);
	theServerSplitter->SplitVertically(mpTreeCtrl,
		theRightPanel);
	*/
	
	wxSizer* pActionCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	topSizer->Add(pActionCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	wxButton* pCloseButton = new wxButton(this, wxID_CANCEL, wxT("Close"));
	pActionCtrlSizer->Add(pCloseButton, 0, wxGROW | wxLEFT, 8);
	
	mServerSettings.mViewOldFiles     = FALSE;
	mServerSettings.mViewDeletedFiles = FALSE;

	/*
	mCountedFiles = 0;
	mCountedBytes = 0;
	UpdateFileCount();
	*/
}

/*
void RestoreFilesPanel::GetUsageInfo() {
	if (mpUsage) delete mpUsage;
	
	mpTreeCtrl->SetCursor(*wxHOURGLASS_CURSOR);
	wxSafeYield();
	mpUsage = mpServerConnection->GetAccountUsage();
	mpTreeCtrl->SetCursor(*wxSTANDARD_CURSOR);
	wxSafeYield();
	
	if (!mpUsage) {
		// already logged by GetAccountUsage
	}
}
*/

void RestoreFilesPanel::OnTreeNodeSelect(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();

	/*
	RestoreTreeNode *node = 
		(RestoreTreeNode *)(mpTreeCtrl->GetItemData(item));
	*/
	
	wxPoint lClickPosition = event.GetPoint();
	wxString msg;
	msg.Printf(wxT("Click at %d,%d"), lClickPosition.x, lClickPosition.y);
	wxLogDebug(msg);
	
	int flags;
	wxTreeItemId lClickedItemId = 
		mpTreeCtrl->HitTest(lClickPosition, flags);

	if (flags & wxTREE_HITTEST_ONITEMICON)
	{
		wxLogDebug(wxT("Icon clicked"));
		event.Veto();
		return;
	}
	else
	{
		wxString msg;
		msg.Printf(wxT("Flags are: %d"), flags);
		wxLogDebug(msg);
	}
	
	/*
	if (!(node->IsDirectory())) 
	{
		mpDeleteButton->Enable(FALSE);
	} 
	else 
	{
		mpDeleteButton->Enable(TRUE);
		if (node->IsDeleted()) {
			mpDeleteButton->SetLabel(wxT("Undelete"));
		} else {
			mpDeleteButton->SetLabel(wxT("Delete"));
		}
	}

	if (!mpUsage) GetUsageInfo();
	
	if (node->IsDirectory()) {
		if (!node->AddChildren(false))
			event.Veto();
	} else {
		// node->ShowInfo(theServerFileList);
		// nothing yet
	}
	*/
	
	event.Skip();
}

void RestoreFilesPanel::OnTreeNodeActivate(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	RestoreTreeNode *pNode = (RestoreTreeNode *)(mpTreeCtrl->GetItemData(item));
	
	const RestoreSpecEntry* pEntry = pNode->GetMatchingEntry();

	// does the entry apply specifically to this item?
	RestoreTreeNode* pParent = (RestoreTreeNode*)( pNode->GetParentNode() );
	
	if (!pParent && pEntry && pEntry->IsInclude())
	{
		// root node matches an include entry,
		// it must be ours because we don't have any parents
		mRestoreSpec.Remove(*pEntry);
	}
	else if (pParent && pParent->GetMatchingEntry() != pEntry)
	{
		// non-root node, and parent matches a 
		// different entry, this one must be ours 
		mRestoreSpec.Remove(*pEntry);
	}
	else
	{
		// non-root node, and parent matches the same one,
		// it doesn't point to us. create a new entry for 
		// this node, with the opposite sense.
		
		bool include = TRUE; // include by default
		
		if (pEntry)
			include = !(pEntry->IsInclude());
		
		RestoreSpecEntry newEntry(pNode->GetCacheNode(), include);
		mRestoreSpec.Add(newEntry);
	}
	
	mpTreeCtrl->UpdateStateIcon(pNode, TRUE, TRUE);
	mpListener->OnRestoreSpecChange();
	// StartCountingFiles();
}

/*
void RestoreProgressCallback(RestoreState State, 
	std::string& rFileName, void* userData)
{
	((RestoreFilesPanel*)userData)->RestoreProgress(State, rFileName);
}

void RestoreFilesPanel::RestoreProgress(RestoreState State, 
	std::string& rFileName)
{
	mRestoreCounter++;
	wxString msg;
	msg.Printf(wxT("Restoring %s (completed %d files)"),
		rFileName.c_str(), mRestoreCounter);
	mpStatusBar->SetStatusText(msg);
	wxSafeYield();
}

void RestoreFilesPanel::OnFileRestore(wxCommandEvent& event)
{
	wxTreeItemId item = mpTreeCtrl->GetSelection();
	RestoreTreeNode *node = 
		(RestoreTreeNode *)(mpTreeCtrl->GetItemData(item));
	
	if (node->GetCacheNode()->IsRoot()) {
		wxMessageBox(
			wxT("You cannot restore the whole server. "
			"Try restoring individual locations"), 
			wxT("Nice Try"), 
			wxOK | wxICON_ERROR, this);
		return;
	}

	ServerCacheNode*   pFileName = node->GetCacheNode();
	ServerFileVersion* pVersion  = pFileName->GetMostRecent();
	// int64_t boxId = pVersion->GetBoxFileId();
	
	wxFileDialog *restoreToDialog = new wxFileDialog(
		this, wxT("Restore To"), wxT(""), wxT(""), wxT(""), 
		wxSAVE, wxDefaultPosition);

	if (restoreToDialog->ShowModal() != wxID_OK) {
		wxMessageBox(wxT("Restore cancelled."), wxT("Boxi Error"), 
			wxOK | wxICON_ERROR, this);
		return;
	}

	wxFileName destFile(restoreToDialog->GetPath());
	if (destFile.DirExists() || destFile.FileExists()) {
		wxString msg("The selected destination file or directory "
			"already exists: ", wxConvLibc);
		msg.Append(destFile.GetFullPath());
		wxMessageBox(msg, wxT("Boxi Error"), 
			wxOK | wxICON_ERROR, this);
		return;
	}

	wxFileName destParentDir(destFile.GetPath());
	
	if (wxFileName::FileExists(destParentDir.GetFullPath())) {
		wxString msg("The selected destination directory "
			"is actually a file: ", wxConvLibc);
		msg.Append(destParentDir.GetFullPath());
		wxMessageBox(msg, wxT("Boxi Error"), 
			wxOK | wxICON_ERROR, this);
		return;
	}
	
	if (! wxFileName::DirExists(destParentDir.GetFullPath())) {
		wxString msg("The selected destination directory "
			"does not exist: ", wxConvLibc);
		msg.Append(destParentDir.GetFullPath());
		wxMessageBox(msg, wxT("Boxi Error"), 
			wxOK | wxICON_ERROR, this);
		return;
	}
	
	int result;
	wxString destFileName(destFile.GetFullPath().c_str(), wxConvLibc);
	wxCharBuffer destFileBuf = destFileName.mb_str(wxConvLibc);	

	try {
		// Go and restore...
		if (pVersion->IsDirectory()) {
			mRestoreCounter = 0;
			result = mpServerConnection->Restore(
				pVersion->GetBoxFileId(), 
				destFileBuf.data(), 
				&RestoreProgressCallback,
				this,  // user data for callback function
				false, // restore deleted
				false, // don't undelete after restore!
				false, // resume?
				);
		} else {
			
			mpServerConnection->GetFile(
				pFileName->GetParent()->GetMostRecent()->GetBoxFileId(),
				pVersion->GetBoxFileId(),
				destFileBuf.data());
			
			result = Restore_Complete;
		}
	} catch (BoxException &e) {
		wxString msg("ERROR: Restore failed: ", wxConvLibc);

		if (e.GetType()    == ConnectionException::ExceptionType &&
			e.GetSubType() == ConnectionException::TLSReadFailed) 
		{
			// protocol object has more details than just TLSReadFailed
			msg.append(wxString(
				mpServerConnection->ErrorString(), wxConvLibc));
			
			// connection to server is probably dead, so close it now.
			mpServerConnection->Disconnect();
			mpTreeCtrl->CollapseAndReset(node->GetId());
		} else {
			msg.Append(wxString(e.what(), wxConvLibc));
		}

		wxMessageBox(msg, wxT("Boxi Error"), 
			wxOK | wxICON_ERROR, this);
		
		return;
	}
	
	switch (result) {
	case Restore_Complete:
		wxMessageBox(wxT("Restore complete."), wxT("Boxi Message"), 
			wxOK | wxICON_INFORMATION, this);
		break;
	
	case Restore_ResumePossible:
		wxMessageBox(wxT("Resume possible?"), wxT("Boxi Error"), 
			wxOK | wxICON_ERROR, this);
		break;
	
	case Restore_TargetExists:
		wxMessageBox(
			wxT("The target directory exists. "
			"You cannot restore over an existing directory."),
			wxT("Boxi Error"), 
			wxOK | wxICON_ERROR, this);
		break;
		
	default:
		wxMessageBox(wxT("ERROR: Unknown restore result."), 
			wxT("Boxi Error"), 
			wxOK | wxICON_ERROR, this);
		break;
	}
}

void RestoreFilesPanel::OnFileDelete(wxCommandEvent& event)
{
	wxMessageBox(
		wxT("Not supported yet"), wxT("Boxi Error"), 
		wxOK | wxICON_ERROR, this);
	return;
	
	wxTreeItemId item = mpTreeCtrl->GetSelection();
	RestoreTreeNode *node = 
		(RestoreTreeNode *)(mpTreeCtrl->GetItemData(item));

	bool success = FALSE;
		
	if (node->IsDeleted()) {
		success = mpServerConnection->UndeleteDirectory(node->GetBoxFileId());
	} else {
		success = mpServerConnection->DeleteDirectory(node->GetBoxFileId());
	}
	
	if (!success) {
		return;
	}

	if (node->IsDeleted()) {
		node->SetDeleted(FALSE);
		mpTreeCtrl->SetItemTextColour(node->GetTreeId(), 
			*wxTheColourDatabase->FindColour("BLACK"));
		mpDeleteButton->SetLabel("Delete");
	} else {
		mpServerConnection->DeleteDirectory(node->GetBoxFileId());
		node->SetDeleted(TRUE);
		mpTreeCtrl->SetItemTextColour(node->GetTreeId(), 
			*wxTheColourDatabase->FindColour("GREY"));
		mpDeleteButton->SetLabel("Undelete");
	}
}
*/

void RestoreFilesPanel::SetViewOldFlag(bool NewValue)
{
	mServerSettings.mViewOldFiles = NewValue;
	// mpTreeRoot->ShowChildren(NULL);
}

void RestoreFilesPanel::SetViewDeletedFlag(bool NewValue)
{
	mServerSettings.mViewDeletedFiles = NewValue;
	// mpTreeRoot->ShowChildren(NULL);
}

/*
int TreeNodeCompare(const RestoreTreeNode **a, const RestoreTreeNode **b)
{
	if ( (*a)->IsDirectory() && !(*b)->IsDirectory()) return -1;
	if (!(*a)->IsDirectory() &&  (*b)->IsDirectory()) return  1;
    return((*a)->GetFileName().CompareTo((*b)->GetFileName()));
}
*/

const ServerCacheNode::Vector* ServerCacheNode::GetChildren()
{
	if (mCached)
		return &mChildren;
	
	FreeChildren();
	
	int16_t lExcludeFlags = 
		BackupProtocolClientListDirectory::Flags_EXCLUDE_NOTHING;

	ServerFileVersion* pMostRecent = GetMostRecent();
	if (!pMostRecent)
	{
		// no versions! cannot return anything
		return NULL;
	}
	
	BackupStoreDirectory dir;
	
	if (!(mpServerConnection->ListDirectory(
		pMostRecent->GetBoxFileId(), lExcludeFlags, dir))) 
	{
		// on error, return NULL
		return NULL;
	}
	
	WX_DECLARE_STRING_HASH_MAP( ServerCacheNode*, FileTable );
	FileTable lFileTable;
	
	// create new nodes for all real, current children of this node
	BackupStoreDirectory::Iterator i(dir);
	BackupStoreDirectory::Entry *en = 0;
	
	while ((en = i.Next()) != 0)
	{
		wxString name = GetDecryptedName(en);
		ServerCacheNode* pChildNode = lFileTable[name];
		if (pChildNode == NULL)
		{
			pChildNode = new ServerCacheNode(this, en);
			lFileTable[name] = pChildNode;
			mChildren.push_back(pChildNode);
		}
		pChildNode->mVersions.push_back(new ServerFileVersion(en));
	}
	
	mCached = TRUE;
	return &mChildren;
}

ServerFileVersion* ServerCacheNode::GetMostRecent()
{
	wxMutexLocker lock(mMutex);
	
	if (mpMostRecent) 
		return mpMostRecent;
	
	for (ServerFileVersion::Vector::const_iterator i = mVersions.begin(); 
		i != mVersions.end(); i++)
	{
		if (mpMostRecent == NULL ||
			mpMostRecent->GetDateTime().IsEarlierThan((*i)->GetDateTime()))
		{
			mpMostRecent = *i;
		}
	}
	
	return mpMostRecent;
}

/*
void RestoreFilesPanel::StartCountingFiles()
{
	mCountFilesStack.clear();
	mCountedFiles = 0;
	mCountedBytes = 0;
	UpdateFileCount();


	RestoreSpec::Vector entries = mRestoreSpec.GetEntries();
	for (RestoreSpec::Iterator i = entries.begin(); i != entries.end(); i++)
	{
		RestoreSpecEntry entry = *i;
		mCountFilesStack.push_back(entry.GetNode());
	}
}

void RestoreFilesPanel::OnIdle(wxIdleEvent& event)
{
	if (mCountFilesStack.size() > 0)
	{
		ServerCacheNode::Iterator i = mCountFilesStack.begin();
		ServerCacheNode* pNode = *i;
		mCountFilesStack.erase(i);
		
		ServerFileVersion* pVersion = pNode->GetMostRecent();
		if (!(pVersion->IsDirectory()))
		{
			mCountedFiles++;
			mCountedBytes += pVersion->GetSizeBlocks();
		}
		else
		{
			const ServerCacheNode::Vector* pChildren = 
				pNode->GetChildren();

			if (!pChildren)
			{
				// if we fail to read a directory,
				// stop counting to avoid annoying the user
				mCountFilesStack.clear();
				return;
			}

			typedef ServerCacheNode::Vector::const_iterator 
				iterator;

			for (iterator i = pChildren->begin(); 
				i != pChildren->end(); i++)
			{
				ServerCacheNode* pChild = *i;
				mCountFilesStack.push_back(pChild);
			}
		}
	
		UpdateFileCount();

		event.RequestMore();
	}
}

void RestoreFilesPanel::UpdateFileCount()
{
	wxString str;
	
	str.Printf(wxT("%lld"), mCountedFiles);
	if (str.CompareTo(mpCountFilesBox->GetValue()) != 0)
		mpCountFilesBox->SetValue(str);
	
	str.Printf(wxT("%lld"), mCountedBytes);
	if (str.CompareTo(mpCountBytesBox->GetValue()) != 0)
		mpCountBytesBox->SetValue(str);
}
*/

void RestoreFilesPanel::OnCloseButtonClick(wxCommandEvent& rEvent)
{
	Hide();
	mpMainFrame->ShowPanel(mpPanelToShowOnClose);
}
