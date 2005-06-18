/***************************************************************************
 *            ServerFilesPanel.cc
 *
 *  Mon Feb 28 22:45:09 2005
 *  Copyright  2005  Chris Wilson
 *  boxi_ServerFilesPanel.cc@qwirx.com
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

#include <wx/artprov.h>
#include <wx/filename.h>
#include <wx/gdicmn.h>
#include <wx/imaglist.h>
#include <wx/mstream.h>
#include <wx/splitter.h>

#include "BackupClientRestore.h"

#include "main.h"
#include "ClientConfig.h"
#include "ServerFilesPanel.h"
#include "StaticImage.h"

void RestoreTreeCtrl::UpdateExcludedStateIcon(
	RestoreTreeNode* pNode, bool updateParents, bool updateChildren) 
{
	int iconId = mEmptyImageId;
	if (updateParents && pNode->GetParentNode() != NULL)
	{
		UpdateExcludedStateIcon(pNode->GetParentNode(), TRUE, FALSE);
	}
	
	/*
	pNode->UpdateExcludedState(updateParents);
	RestoreTreeNode* pParent = pNode->GetParentNode();
	
	if (pNode->GetIncludedBy() != NULL)
	{
		if (pParent && pNode->GetIncludedBy() == pParent->GetIncludedBy())
			iconId = mAlwaysGreyImageId;
		else
			iconId = mAlwaysImageId;
	}
	else if (pNode->GetExcludedBy() != NULL)
	{
		if (pParent && pNode->GetExcludedBy() == pParent->GetExcludedBy())
			iconId = mCrossedGreyImageId;
		else
			iconId = mCrossedImageId;
	}
	else if (pNode->GetLocation() != NULL)
	{
		if (pParent && pNode->GetLocation() == pParent->GetLocation())
			iconId = mCheckedGreyImageId;
		else
			iconId = mCheckedImageId;
	}
	*/
	SetItemImage(pNode->GetTreeId(), iconId, wxTreeItemIcon_Normal);
	
	if (updateChildren)
	{
		wxTreeItemId thisId = pNode->GetTreeId();
		wxTreeItemIdValue cookie;
		wxTreeItemId childId = GetFirstChild(thisId, cookie);
		while (childId.IsOk())
		{
			RestoreTreeNode* pChildNode = 
				(RestoreTreeNode*)( GetItemData(childId) );
			UpdateExcludedStateIcon(pChildNode, FALSE, TRUE);
			childId = GetNextChild(thisId, cookie);
		}
	}
}

static int AddImage(
	const struct StaticImage & rImageData, 
	wxImageList & rImageList)
{
	wxMemoryInputStream byteStream(rImageData.data, rImageData.size);
	wxImage img(byteStream, wxBITMAP_TYPE_PNG);
	return rImageList.Add(img);
}

RestoreTreeCtrl::RestoreTreeCtrl(
	wxWindow* parent, 
	wxWindowID id, 
	const wxPoint& pos, 
	const wxSize& size, 
	long style, 
	const wxValidator& validator, 
	const wxString& name
)
: wxTreeCtrl(parent, id, pos, size, style, validator, name),
  mImages(16, 16, true)
{
	mEmptyImageId        = AddImage(empty16_png,     mImages);
	mCheckedImageId      = AddImage(tick16_png,      mImages);
	mCheckedGreyImageId  = AddImage(tickgrey16_png,  mImages);
	mCrossedImageId      = AddImage(cross16_png,     mImages);
	mCrossedGreyImageId  = AddImage(crossgrey16_png, mImages);
	mAlwaysImageId       = AddImage(plus16_png,      mImages);
	mAlwaysGreyImageId   = AddImage(plusgrey16_png,  mImages);

	SetImageList(&mImages);
}

void RestoreTreeCtrl::SetRoot(RestoreTreeNode* pRootNode)
{
	wxString lRootName = "/ (server root)";
	wxTreeItemId lRootId = AddRoot(lRootName, -1, -1, pRootNode);
	pRootNode->SetTreeId(lRootId);
	SetItemHasChildren(lRootId, TRUE);
	UpdateExcludedStateIcon(pRootNode, false, false);
}

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

// Image index constants
int	mImgDir, mImgFile, mImgExec, mImgDown, mImgUp;

BEGIN_EVENT_TABLE(RestorePanel, wxPanel)
	EVT_TREE_ITEM_EXPANDING(ID_Server_File_Tree, 
		RestorePanel::OnTreeNodeExpand)
	EVT_TREE_SEL_CHANGING(ID_Server_File_Tree,
		RestorePanel::OnTreeNodeSelect)
	/*
	EVT_LIST_COL_CLICK(ID_Server_File_List, 
		RestorePanel::OnListColumnClick)
	EVT_LIST_ITEM_ACTIVATED(ID_Server_File_List,
		RestorePanel::OnListItemActivate)
	*/
	EVT_BUTTON(ID_Server_File_RestoreButton, 
		RestorePanel::OnFileRestore)
	EVT_BUTTON(ID_Server_File_DeleteButton, 
		RestorePanel::OnFileDelete)
END_EVENT_TABLE()

RestorePanel::RestorePanel(
	ClientConfig*	config,
	ServerConnection* pServerConnection,
	wxWindow*		parent, 
	wxStatusBar*	pStatusBar,
	wxWindowID 		id, 
	const wxPoint& 	pos, 
	const wxSize& 	size,
	long 			style, 
	const wxString& name)
: wxPanel(parent, id, pos, size, style, name),
  mImageList(16, 16, true)
{
	mpConfig = config;
	mpStatusBar = pStatusBar;
	mpServerConnection = pServerConnection;

	wxBoxSizer *topSizer = new wxBoxSizer( wxVERTICAL );
	
	wxSplitterWindow *theServerSplitter = new wxSplitterWindow(this, 
		ID_Server_Splitter);
	topSizer->Add(theServerSplitter, 1, wxGROW | wxALL, 8);

	mpTreeCtrl = new RestoreTreeCtrl(theServerSplitter, 
		ID_Server_File_Tree, wxDefaultPosition, wxDefaultSize, 
		wxTR_DEFAULT_STYLE | wxNO_BORDER);

	mpTreeRoot = new RestoreTreeNode(mpTreeCtrl, mpConfig, 
		&mServerSettings, mpServerConnection);
	mpTreeCtrl->SetRoot(mpTreeRoot);
	
	wxPanel* theRightPanel = new wxPanel(theServerSplitter);
	wxBoxSizer *theRightPanelSizer = new wxBoxSizer( wxVERTICAL );
	
	/*
	wxGridSizer* theRightParamSizer = new wxGridSizer(2, 4, 4);
	theRightPanelSizer->Add(theRightParamSizer, 0, wxGROW | wxALL, 4);
	
	AddParam(theRightPanel, "Date", 
		new wxStaticText(theRightPanel, wxID_ANY, "Anytime"),
		TRUE, theRightParamSizer);
	*/

	wxBoxSizer *theRightButtonSizer = new wxBoxSizer( wxHORIZONTAL );
	theRightPanelSizer->Add(theRightButtonSizer, 0, wxGROW | wxALL, 4);

	wxButton *theRestoreButton = new wxButton(theRightPanel, 
		ID_Server_File_RestoreButton, "Restore", wxDefaultPosition);
	theRightButtonSizer->Add(theRestoreButton, 1, wxALL, 4);

	wxButton *theCompareButton = new wxButton(theRightPanel, 
		ID_Server_File_CompareButton, "Compare", wxDefaultPosition);
	theRightButtonSizer->Add(theCompareButton, 1, wxALL, 4);

	mpDeleteButton = new wxButton(theRightPanel, 
		ID_Server_File_DeleteButton, "Delete", wxDefaultPosition);
	theRightButtonSizer->Add(mpDeleteButton, 1, wxALL, 4);
	
	/*
	theServerFileList = new wxListView(theRightPanel, 
		ID_Server_File_List, wxDefaultPosition, wxDefaultSize, 
		wxLC_REPORT | wxLC_VRULES);
	theRightPanelSizer->Add(theServerFileList, 1, wxGROW | wxALL, 8);
	*/
	
	theRightPanel->SetSizer( theRightPanelSizer );

	theServerSplitter->SetMinimumPaneSize(100);
	theServerSplitter->SplitVertically(mpTreeCtrl,
		theRightPanel);
	
	/*
	wxTreeItemId id = mpTreeCtrl->AddRoot(
		wxString("server"), -1, -1, mpTreeRoot);
	mpTreeRoot->SetTreeId(id); 
	
	mImgDir = mImageList.Add(wxArtProvider::GetIcon(wxART_FOLDER, 
		wxART_CMN_DIALOG, wxSize(16, 16)));
	mImgFile = mImageList.Add(wxArtProvider::GetIcon(wxART_NORMAL_FILE, 
		wxART_CMN_DIALOG, wxSize(16, 16)));
	mImgExec = mImageList.Add(wxArtProvider::GetIcon(wxART_EXECUTABLE_FILE, 
		wxART_CMN_DIALOG, wxSize(16, 16)));
	mImgDown = mImageList.Add(wxArtProvider::GetIcon(wxART_GO_DOWN, 
		wxART_CMN_DIALOG, wxSize(16, 16)));
	mImgUp = mImageList.Add(wxArtProvider::GetIcon(wxART_GO_UP, 
		wxART_CMN_DIALOG, wxSize(16, 16)));

	// theServerFileTree->SetImageList(&mImageList);
	theServerFileList->SetImageList(&mImageList, wxIMAGE_LIST_SMALL);

	wxListItem itemCol;
	itemCol.SetMask(wxLIST_MASK_TEXT | wxLIST_MASK_IMAGE | wxLIST_MASK_FORMAT);
	itemCol.SetAlign(wxLIST_FORMAT_LEFT);
	itemCol.SetText("Name");
	itemCol.SetImage(-1);
	theServerFileList->InsertColumn(0, itemCol);
	itemCol.SetText("Size");
	itemCol.SetAlign(wxLIST_FORMAT_RIGHT);
	theServerFileList->InsertColumn(1, itemCol);
	// itemCol.SetAlign(wxLIST_FORMAT_LEFT);
	itemCol.SetText("Last Modified");
	theServerFileList->InsertColumn(2, itemCol);

	for (int i = 0; i < theServerFileList->GetColumnCount(); i++) 
    	theServerFileList->SetColumnWidth( i, i == 0 ? 120 : 60 );
	*/
	
	SetSizer( topSizer );
	topSizer->SetSizeHints( this );
	
	mServerSettings.mViewOldFiles     = FALSE;
	mServerSettings.mViewDeletedFiles = FALSE;
}

void RestorePanel::OnTreeNodeExpand(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	RestoreTreeNode *node = 
		(RestoreTreeNode *)(mpTreeCtrl->GetItemData(item));
	if (!node->AddChildren(true))
		event.Veto();
}

void RestorePanel::GetUsageInfo() {
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

void RestorePanel::OnTreeNodeSelect(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	RestoreTreeNode *node = 
		(RestoreTreeNode *)(mpTreeCtrl->GetItemData(item));

	if (!(node->IsDirectory())) {
		mpDeleteButton->Enable(FALSE);
	} else {
		mpDeleteButton->Enable(TRUE);
		if (node->IsDeleted()) {
			mpDeleteButton->SetLabel("Undelete");
		} else {
			mpDeleteButton->SetLabel("Delete");
		}
	}
	
	/*
	if (!mpUsage) GetUsageInfo();
	
	if (node->IsDirectory()) {
		if (!node->AddChildren(false))
			event.Veto();
	} else {
		// node->ShowInfo(theServerFileList);
		// nothing yet
	}
	*/
}

void RestoreProgressCallback(RestoreState State, 
	std::string& rFileName, void* userData)
{
	((RestorePanel*)userData)->RestoreProgress(State, rFileName);
}

void RestorePanel::RestoreProgress(RestoreState State, 
	std::string& rFileName)
{
	mRestoreCounter++;
	wxString msg;
	msg.Printf("Restoring %s (completed %d files)",
		rFileName.c_str(), mRestoreCounter);
	mpStatusBar->SetStatusText(msg);
	wxSafeYield();
}

void RestorePanel::OnFileRestore(wxCommandEvent& event)
{
	wxTreeItemId item = mpTreeCtrl->GetSelection();
	RestoreTreeNode *node = 
		(RestoreTreeNode *)(mpTreeCtrl->GetItemData(item));
	
	int64_t boxId = node->GetBoxFileId();
	if (boxId == BackupProtocolClientListDirectory::RootDirectory) {
		wxMessageBox("You cannot restore the whole server. "
			"Try restoring individual locations", "Nice Try", 
			wxOK | wxICON_ERROR, this);
		return;
	}
	
	wxFileDialog *restoreToDialog = new wxFileDialog(
		this, "Restore To", "", "", "", wxSAVE, wxDefaultPosition);

	if (restoreToDialog->ShowModal() != wxID_OK) {
		wxMessageBox("Restore cancelled.", "Boxi Error", 
			wxOK | wxICON_ERROR, this);
		return;
	}

	wxFileName destFile(restoreToDialog->GetPath());
	if (destFile.DirExists() || destFile.FileExists()) {
		wxString msg = "The selected destination file or directory "
			"already exists: ";
		msg.Append(destFile.GetFullPath());
		wxMessageBox(msg, "Boxi Error", wxOK | wxICON_ERROR, this);
		return;
	}

	wxFileName destParentDir(destFile.GetPath());
	
	if (wxFileName::FileExists(destParentDir.GetFullPath())) {
		wxString msg = "The selected destination directory "
			"is actually a file: ";
		msg.Append(destParentDir.GetFullPath());
		wxMessageBox(msg, "Boxi Error", wxOK | wxICON_ERROR, this);
		return;
	}
	
	if (! wxFileName::DirExists(destParentDir.GetFullPath())) {
		wxString msg = "The selected destination directory "
			"does not exist: ";
		msg.Append(destParentDir.GetFullPath());
		wxMessageBox(msg, "Boxi Error", wxOK | wxICON_ERROR, this);
		return;
	}
	
	int result;
	char *destFileName = ::strdup(destFile.GetFullPath().c_str());
	
	try {
		// Go and restore...
		if (node->IsDirectory()) {
			mRestoreCounter = 0;
			result = mpServerConnection->Restore(
				boxId, destFileName, 
				&RestoreProgressCallback,
				this, /* user data for callback function */
				false /* restore deleted */, 
				false /* don't undelete after restore! */, 
				false /* resume? */);
		} else {
			mpServerConnection->GetFile(
				node->GetParentNode()->GetBoxFileId(),
				node->GetBoxFileId(),
				destFileName);
			
			result = Restore_Complete;
		}
	} catch (BoxException &e) {
		::free(destFileName);
		wxString msg = "ERROR: Restore failed: ";

		if (e.GetType()    == ConnectionException::ExceptionType &&
			e.GetSubType() == ConnectionException::TLSReadFailed) 
		{
			// protocol object has more details than just TLSReadFailed
			msg.append(mpServerConnection->ErrorString());
			
			// connection to server is probably dead, so close it now.
			mpServerConnection->Disconnect();
			mpTreeCtrl->CollapseAndReset(node->GetTreeId());
		} else {
			msg.Append(e.what());
		}

		wxMessageBox(msg, "Boxi Error", wxOK | wxICON_ERROR, this);
		
		return;
	}
	
	::free(destFileName);
	
	switch (result) {
	case Restore_Complete:
		wxMessageBox("Restore complete.", "Boxi Message", 
			wxOK | wxICON_INFORMATION, this);
		break;
	
	case Restore_ResumePossible:
		wxMessageBox("Resume possible?", "Boxi Error", 
			wxOK | wxICON_ERROR, this);
		break;
	
	case Restore_TargetExists:
		wxMessageBox("The target directory exists. You cannot restore "
			"over an existing directory.", "Boxi Error", 
			wxOK | wxICON_ERROR, this);
		break;
		
	default:
		wxMessageBox("ERROR: Unknown restore result.", "Boxi Error", 
			wxOK | wxICON_ERROR, this);
		break;
	}
}

void RestorePanel::OnFileDelete(wxCommandEvent& event)
{
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

int wxCALLBACK myCompareFunction(long item1, long item2, long sortData) {
	RestoreTreeNode* pItem1 = (RestoreTreeNode *)item1;
	RestoreTreeNode* pItem2 = (RestoreTreeNode *)item2;
	RestorePanel* pPanel = (RestorePanel *)sortData;
	
	int  Column  = pPanel->GetListSortColumn();
	bool Reverse = pPanel->GetListSortReverse();
	int  Result  = 0;
	
	if (pItem1->IsDirectory() && !(pItem2->IsDirectory())) {
		Result = -1;
	} else if (!(pItem1->IsDirectory()) && pItem2->IsDirectory()) {
		Result = 1;
	} else if (Column == 0) {
		Result = pItem1->GetFileName().CompareTo(pItem2->GetFileName());
	/*
	} else if (Column == 1) {
		Result = pItem1->mSizeInBlocks - pItem2->mSizeInBlocks;
	} else if (Column == 2) {
		wxTimeSpan Span = pItem1->mDateTime.Subtract(pItem2->mDateTime);
		wxLongLong llResult = Span.GetSeconds();
		if      (llResult < 0) Result = -1;
		else if (llResult > 0) Result = 1;
		else Result = 0;
	*/
	}

	if (Reverse) Result = -Result;
	return Result;
}

/*
void RestorePanel::OnListColumnClick(wxListEvent& event)
{
	wxListItem column;
	theServerFileList->GetColumn(mListSortColumn, column);
	column.SetImage(-1);
	theServerFileList->SetColumn(mListSortColumn, column);

	int NewColumn = event.GetColumn();

	if (NewColumn == mListSortColumn) {
		mListSortReverse = !mListSortReverse;
	} else {		
		mListSortColumn = NewColumn;
		mListSortReverse = false;
	}
	
	theServerFileList->GetColumn(mListSortColumn, column);
	column.SetImage(mListSortReverse ? mImgUp : mImgDown);
	theServerFileList->SetColumn(mListSortColumn, column);
	
	theServerFileList->SortItems(myCompareFunction, (long)this);
}

void RestorePanel::OnListItemActivate(wxListEvent& event)
{
	RestoreTreeNode *pNode = (RestoreTreeNode *)event.GetData();
	mpTreeCtrl->SelectItem(pNode->treeId);
}
*/

void RestorePanel::SetViewOldFlag(bool NewValue)
{
	mServerSettings.mViewOldFiles = NewValue;
	// mpTreeRoot->ShowChildren(NULL);
}

void RestorePanel::SetViewDeletedFlag(bool NewValue)
{
	mServerSettings.mViewDeletedFiles = NewValue;
	// mpTreeRoot->ShowChildren(NULL);
}

bool RestoreTreeNode::AddChildren(bool recurse) {
	mpTreeCtrl->SetCursor(*wxHOURGLASS_CURSOR);
	wxSafeYield();
	
	bool result;
	
	try {
		result = _AddChildrenSlow(recurse);
	} catch (std::exception& e) {
		mpTreeCtrl->SetCursor(*wxSTANDARD_CURSOR);
		throw(e);
	}
	
	mpTreeCtrl->SetCursor(*wxSTANDARD_CURSOR);
	return result;
}

WX_DEFINE_LIST(DirEntryPtrList);

int TreeNodeCompare(const RestoreTreeNode **a, const RestoreTreeNode **b)
{
	if ( (*a)->IsDirectory() && !(*b)->IsDirectory()) return -1;
	if (!(*a)->IsDirectory() &&  (*b)->IsDirectory()) return  1;
    return((*a)->GetFileName().CompareTo((*b)->GetFileName()));
}

/* Based on BackupQueries.cpp from boxbackup 0.09 */

bool RestoreTreeNode::_AddChildrenSlow(bool recursive) 
{
	int64_t baseDir = mBoxFileId;
	
	// Generate exclude flags
	int16_t excludeFlags = 
		BackupProtocolClientListDirectory::Flags_EXCLUDE_NOTHING;
	if (!(mpServerSettings->mViewOldFiles))
		excludeFlags |= BackupProtocolClientListDirectory::Flags_OldVersion;
	if (!(mpServerSettings->mViewDeletedFiles))
		excludeFlags |= BackupProtocolClientListDirectory::Flags_Deleted;

	// delete any existing children of the parent
	mpTreeCtrl->DeleteChildren(mTreeId);
	
	/*
	if (targetList != NULL)
		targetList->DeleteAllItems();
	*/
	
	BackupStoreDirectory dir;
	
	if (!(mpServerConnection->ListDirectory(baseDir, excludeFlags, dir))) {
		return FALSE;
	}

	DirEntryPtrList entries;
	
	// create new nodes for all real, current children of this node
	BackupStoreDirectory::Iterator i(dir);
	BackupStoreDirectory::Entry *en = 0;
	
	while ((en = i.Next()) != 0)
	{
		// decrypt file name
		BackupStoreFilenameClear clear(en->GetName());
		
		int64_t boxFileId = en->GetObjectID();
		
		wxString newFileName = clear.GetClearFilename().c_str();
		
		bool isDirectory =
			(en->GetFlags() & BackupStoreDirectory::Entry::Flags_Dir) != 0;
		
		// add to the tree
		RestoreTreeNode *pNewNode = new RestoreTreeNode(
			this, boxFileId, newFileName, isDirectory);
		
		pNewNode->SetTreeId(
			mpTreeCtrl->AppendItem(this->mTreeId,
				newFileName, -1, -1, pNewNode)
			);
		
		mpTreeCtrl->UpdateExcludedStateIcon(pNewNode, false, false);
		
		if (pNewNode->mIsDeleted)
			mpTreeCtrl->SetItemTextColour(pNewNode->mTreeId, 
				*wxTheColourDatabase->FindColour("GREY"));
			
		if (pNewNode->mIsDirectory)
		{
			if (recursive) {
				pNewNode->_AddChildrenSlow(false);
			} else {
				mpTreeCtrl->SetItemHasChildren(pNewNode->mTreeId, TRUE);
			}
		}
	}

	// sort the list
	mpTreeCtrl->SortChildren(this->mTreeId);
	
	return TRUE;
}
