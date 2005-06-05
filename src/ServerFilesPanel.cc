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

#include <wx/filename.h>
#include <wx/gdicmn.h>
#include <wx/splitter.h>
#include <wx/artprov.h>
#include <wx/imaglist.h>

#include "BackupClientRestore.h"

#include "main.h"
#include "ClientConfig.h"
#include "ServerFilesPanel.h"

// Image index constants
int	mImgDir, mImgFile, mImgExec, mImgDown, mImgUp;

BEGIN_EVENT_TABLE(ServerFilesPanel, wxPanel)
	EVT_TREE_ITEM_EXPANDING(ID_Server_File_Tree, 
		ServerFilesPanel::OnTreeNodeExpand)
	EVT_TREE_SEL_CHANGING(ID_Server_File_Tree,
		ServerFilesPanel::OnTreeNodeSelect)
	EVT_LIST_COL_CLICK(ID_Server_File_List, 
		ServerFilesPanel::OnListColumnClick)
	EVT_LIST_ITEM_ACTIVATED(ID_Server_File_List,
		ServerFilesPanel::OnListItemActivate)
	EVT_BUTTON(ID_Server_File_RestoreButton, 
		ServerFilesPanel::OnFileRestore)
	EVT_BUTTON(ID_Server_File_DeleteButton, 
		ServerFilesPanel::OnFileDelete)
END_EVENT_TABLE()

ServerFilesPanel::ServerFilesPanel(
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

	theServerFileTree = new wxTreeCtrl(theServerSplitter, 
		ID_Server_File_Tree, wxDefaultPosition, wxDefaultSize, 
		wxTR_DEFAULT_STYLE | wxNO_BORDER);
	
	wxPanel* theRightPanel = new wxPanel(theServerSplitter);
	wxBoxSizer *theRightPanelSizer = new wxBoxSizer( wxVERTICAL );

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
	
	theServerFileList = new wxListView(theRightPanel, 
		ID_Server_File_List, wxDefaultPosition, wxDefaultSize, 
		wxLC_REPORT | wxLC_VRULES);
	theRightPanelSizer->Add(theServerFileList, 1, wxGROW | wxALL, 8);

	theRightPanel->SetSizer( theRightPanelSizer );

	theServerSplitter->SetMinimumPaneSize(100);
	theServerSplitter->SplitVertically(theServerFileTree,
		theRightPanel);
	
	mpTreeRoot = new BoxiTreeNodeInfo();
	mpTreeRoot->mFileName		= "/";
	mpTreeRoot->treeId			= theServerFileTree->AddRoot(
		wxString("server"), -1, -1, mpTreeRoot);
	mpTreeRoot->boxFileID		= 
		BackupProtocolClientListDirectory::RootDirectory;
	mpTreeRoot->theTree			= theServerFileTree;
	mpTreeRoot->theConfig		= mpConfig;
	mpTreeRoot->theServerNode	= mpTreeRoot;
	mpTreeRoot->theParentNode	= NULL;
	mpTreeRoot->mpServerSettings = &mServerSettings;
	mpTreeRoot->mpServerConnection = mpServerConnection;
	mpTreeRoot->boxFileID = BackupProtocolClientListDirectory::RootDirectory;
	
	theServerFileTree->SetItemHasChildren(mpTreeRoot->treeId, TRUE);
	
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

	SetSizer( topSizer );
	topSizer->SetSizeHints( this );
	
	mServerSettings.mViewOldFiles     = TRUE;
	mServerSettings.mViewDeletedFiles = TRUE;
}

void ServerFilesPanel::OnTreeNodeExpand(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	BoxiTreeNodeInfo *node = 
		(BoxiTreeNodeInfo *)(theServerFileTree->GetItemData(item));
	if (!node->ShowChildren(NULL))
		event.Veto();
}

void ServerFilesPanel::GetUsageInfo() {
	if (mpUsage) delete mpUsage;
	
	theServerFileTree->SetCursor(*wxHOURGLASS_CURSOR);
	wxSafeYield();
	mpUsage = mpServerConnection->GetAccountUsage();
	theServerFileTree->SetCursor(*wxSTANDARD_CURSOR);
	wxSafeYield();
	
	if (!mpUsage) {
		// already logged by GetAccountUsage
	}
}

void ServerFilesPanel::OnTreeNodeSelect(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	BoxiTreeNodeInfo *node = 
		(BoxiTreeNodeInfo *)(theServerFileTree->GetItemData(item));

	if (!(node->isDirectory)) {
		mpDeleteButton->Enable(FALSE);
	} else {
		mpDeleteButton->Enable(TRUE);
		if (node->isDeleted) {
			mpDeleteButton->SetLabel("Undelete");
		} else {
			mpDeleteButton->SetLabel("Delete");
		}
	}
	
	if (!mpUsage) GetUsageInfo();
	
	if (node->isDirectory) {
		if (!node->ShowChildren(theServerFileList))
			event.Veto();
	} else {
		// node->ShowInfo(theServerFileList);
		// nothing yet
	}
}

void RestoreProgressCallback(RestoreState State, 
	std::string& rFileName, void* userData)
{
	((ServerFilesPanel*)userData)->RestoreProgress(State, rFileName);
}

void ServerFilesPanel::RestoreProgress(RestoreState State, 
	std::string& rFileName)
{
	mRestoreCounter++;
	wxString msg;
	msg.Printf("Restoring %s (completed %d files)",
		rFileName.c_str(), mRestoreCounter);
	mpStatusBar->SetStatusText(msg);
	wxSafeYield();
}

void ServerFilesPanel::OnFileRestore(wxCommandEvent& event)
{
	wxTreeItemId item = theServerFileTree->GetSelection();
	BoxiTreeNodeInfo *node = 
		(BoxiTreeNodeInfo *)(theServerFileTree->GetItemData(item));
	
	int64_t boxId = node->boxFileID;
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
		if (node->isDirectory) {
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
				node->theParentNode->boxFileID,
				node->boxFileID,
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
			node->theServerNode->theTree->CollapseAndReset(
				node->theServerNode->treeId);
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

void ServerFilesPanel::OnFileDelete(wxCommandEvent& event)
{
	wxTreeItemId item = theServerFileTree->GetSelection();
	BoxiTreeNodeInfo *node = 
		(BoxiTreeNodeInfo *)(theServerFileTree->GetItemData(item));

	bool success = FALSE;
		
	if (node->isDeleted) {
		success = mpServerConnection->UndeleteDirectory(node->boxFileID);
	} else {
		success = mpServerConnection->DeleteDirectory(node->boxFileID);
	}
	
	if (!success) {
		return;
	}

	if (node->isDeleted) {
		node->isDeleted = FALSE;
		theServerFileTree->SetItemTextColour(node->treeId, 
			*wxTheColourDatabase->FindColour("BLACK"));
		mpDeleteButton->SetLabel("Delete");
	} else {
		mpServerConnection->DeleteDirectory(node->boxFileID);
		node->isDeleted = TRUE;
		theServerFileTree->SetItemTextColour(node->treeId, 
			*wxTheColourDatabase->FindColour("GREY"));
		mpDeleteButton->SetLabel("Undelete");
	}
}

int wxCALLBACK myCompareFunction(long item1, long item2, long sortData) {
	BoxiTreeNodeInfo* pItem1 = (BoxiTreeNodeInfo *)item1;
	BoxiTreeNodeInfo* pItem2 = (BoxiTreeNodeInfo *)item2;
	ServerFilesPanel* pPanel = (ServerFilesPanel *)sortData;
	
	int  Column  = pPanel->GetListSortColumn();
	bool Reverse = pPanel->GetListSortReverse();
	int  Result  = 0;
	
	if (pItem1->isDirectory && !(pItem2->isDirectory)) {
		Result = -1;
	} else if (!(pItem1->isDirectory) && pItem2->isDirectory) {
		Result = 1;
	} else if (Column == 0) {
		Result = pItem1->mFileName.CompareTo(pItem2->mFileName);
	} else if (Column == 1) {
		Result = pItem1->mSizeInBlocks - pItem2->mSizeInBlocks;
	} else if (Column == 2) {
		wxTimeSpan Span = pItem1->mDateTime.Subtract(pItem2->mDateTime);
		wxLongLong llResult = Span.GetSeconds();
		if      (llResult < 0) Result = -1;
		else if (llResult > 0) Result = 1;
		else Result = 0;
	}

	if (Reverse) Result = -Result;
	return Result;
}

void ServerFilesPanel::OnListColumnClick(wxListEvent& event)
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

void ServerFilesPanel::OnListItemActivate(wxListEvent& event)
{
	BoxiTreeNodeInfo *pNode = (BoxiTreeNodeInfo *)event.GetData();
	theServerFileTree->SelectItem(pNode->treeId);
}

void ServerFilesPanel::SetViewOldFlag(bool NewValue)
{
	mServerSettings.mViewOldFiles = NewValue;
	mpTreeRoot->ShowChildren(NULL);
}

void ServerFilesPanel::SetViewDeletedFlag(bool NewValue)
{
	mServerSettings.mViewDeletedFiles = NewValue;
	mpTreeRoot->ShowChildren(NULL);
}

BoxiTreeNodeInfo::BoxiTreeNodeInfo() 
{
	// memset(this, 0, sizeof(*this));
	
	theTree            = NULL;
	theConfig          = NULL;
	theServerNode      = NULL;
	theParentNode      = NULL;
	mpServerSettings   = NULL;
	mpServerConnection = NULL;
	mpUsage            = NULL;
	
	boxFileID       = 0;
	isDirectory     = FALSE;
	isDeleted       = FALSE;
	mFlags          = 0;
	mSizeInBlocks   = 0;
	mHasAttributes  = FALSE;
}

BoxiTreeNodeInfo::~BoxiTreeNodeInfo() 
{
	if (mpUsage != NULL) {
		delete mpUsage;
		mpUsage = NULL;
	}
}

bool BoxiTreeNodeInfo::ShowChildren(wxListCtrl *targetList) {
	theServerNode->theTree->SetCursor(*wxHOURGLASS_CURSOR);
	wxSafeYield();
	bool result = FALSE;
	
	result = ScanChildrenAndList(targetList, false);
	if (!result) {
		theServerNode->theTree->CollapseAndReset(theServerNode->treeId);
	}
	
	theServerNode->theTree->SetCursor(*wxSTANDARD_CURSOR);
	return result;
}

WX_DEFINE_LIST(DirEntryPtrList);

int TreeNodeCompare(const BoxiTreeNodeInfo **a, const BoxiTreeNodeInfo **b)
{
	if ( (*a)->isDirectory && !(*b)->isDirectory) return -1;
	if (!(*a)->isDirectory &&  (*b)->isDirectory) return  1;
    return((*a)->mFileName.CompareTo((*b)->mFileName));
}

/* Based on BackupQueries.cpp from boxbackup 0.09 */

bool BoxiTreeNodeInfo::ScanChildrenAndList(wxListCtrl *targetList,
	bool recursive) {
	int64_t baseDir = boxFileID;
	
	// Generate exclude flags
	int16_t excludeFlags = 
		BackupProtocolClientListDirectory::Flags_EXCLUDE_NOTHING;
	if (!(mpServerSettings->mViewOldFiles))
		excludeFlags |= BackupProtocolClientListDirectory::Flags_OldVersion;
	if (!(mpServerSettings->mViewDeletedFiles))
		excludeFlags |= BackupProtocolClientListDirectory::Flags_Deleted;

	// delete any existing children of the parent
	theTree->DeleteChildren(treeId);
	
	if (targetList != NULL)
		targetList->DeleteAllItems();
	
	BackupStoreDirectory dir;
	
	if (!(mpServerConnection->ListDirectory(baseDir, excludeFlags, dir))) {
		return FALSE;
	}

	DirEntryPtrList entries;
	
	// create new nodes for all real, current children of this node
	{
		BackupStoreDirectory::Iterator i(dir);
		BackupStoreDirectory::Entry *en = 0;
		
		while ((en = i.Next()) != 0)
		{
			// decrypt file name
			BackupStoreFilenameClear clear(en->GetName());
	
			// create a new tree node to represent the directory entry
			BoxiTreeNodeInfo *treeNode = new BoxiTreeNodeInfo();
			treeNode->theTree = theTree;
			treeNode->theServerNode = theServerNode;
			treeNode->theParentNode = this;
			treeNode->isDeleted = 
				(en->GetFlags ()& BackupStoreDirectory::Entry::Flags_Deleted) != 0;
			treeNode->isDirectory =
				(en->GetFlags() & BackupStoreDirectory::Entry::Flags_Dir) != 0;
			treeNode->boxFileID = en->GetObjectID();
			treeNode->mFlags    = en->GetFlags();
			treeNode->mFileName = clear.GetClearFilename().c_str();
			treeNode->mSizeInBlocks  = en->GetSizeInBlocks();
			treeNode->mHasAttributes = en->HasAttributes();
			treeNode->mpServerSettings = theServerNode->mpServerSettings;
			treeNode->mpServerConnection = mpServerConnection;
	
			time_t ModTime = BoxTimeToSeconds(en->GetModificationTime());
			treeNode->mDateTime = wxDateTime(ModTime);
			
			// add to the list
			entries.Append(treeNode);
		}
	}
	
	// sort the list
	entries.Sort(TreeNodeCompare);	

	int listIndex = 0;
	
	// iterate over the list
    for ( DirEntryPtrList::Node *iter = entries.GetFirst(); 
		iter; iter = iter->GetNext() )
    {
        BoxiTreeNodeInfo *treeNode = iter->GetData();

		treeNode->treeId = theTree->AppendItem(this->treeId,
			treeNode->mFileName, -1, -1, treeNode);
		
		if (treeNode->isDeleted)
			theTree->SetItemTextColour(treeNode->treeId, 
				*wxTheColourDatabase->FindColour("GREY"));
			
		if (treeNode->isDirectory)
		{
			if (recursive) {
				treeNode->ScanChildrenAndList(NULL, false);
			} else {
				theTree->SetItemHasChildren(treeNode->treeId, TRUE);
			}
		}

		if (targetList == NULL)
			continue;
		
		// add to list
		long newItemListId = targetList->InsertItem(listIndex++,
			treeNode->mFileName, -1);
		
		// Size in blocks
		if (mpUsage) {
			wxString sizeString = "";
			int64_t size = treeNode->mSizeInBlocks * 
				theServerNode->mpUsage->GetBlockSize();
			
			while (size > 1000) {
				int64_t rem = size % 1000;
				size /= 1000;
				wxString newSize;
				newSize.Printf(",%03lld", rem);
				newSize.Append(sizeString);
				sizeString = newSize;
			}

			wxString newSize;
			newSize.Printf("%lld", size);
			newSize.Append(sizeString);
			sizeString = newSize;

			targetList->SetItem(newItemListId, 1, sizeString);
		} else {
			targetList->SetItem(newItemListId, 1, "Unknown");
		}
		
		// convert date and time from Box Backup format to wxDateTime
		{
			wxString DateTimeString;
			DateTimeString.Append(treeNode->mDateTime.FormatISODate());
			DateTimeString.Append(" ");
			DateTimeString.Append(treeNode->mDateTime.FormatISOTime());
			targetList->SetItem(newItemListId, 2, DateTimeString);
		}
		
		// choose correct icon: file or directory
		{
			if (treeNode->isDirectory)
				targetList->SetItemImage(newItemListId, mImgDir, 0);
			else
				targetList->SetItemImage(newItemListId, mImgFile, 0);
		}
		
		// associate with TreeNode
		targetList->SetItemData(newItemListId, (long)treeNode);
	}
	
	return TRUE;
}
