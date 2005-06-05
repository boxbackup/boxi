/***************************************************************************
 *            BackupLocationsPanel.cc
 *
 *  Tue Mar  1 00:26:30 2005
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

#include <regex.h>

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/mstream.h>
#include <wx/splitter.h>
#include <wx/treectrl.h>

#include "BackupLocationsPanel.h"
#include "StaticImage.h"
#include "../images/tick16.xpm"

void BackupTreeNode::AddChildren() {
	mpTreeCtrl->SetCursor(*wxHOURGLASS_CURSOR);
	wxSafeYield();
	
	try {
		_AddChildrenSlow(false);
	} catch (std::exception& e) {
		mpTreeCtrl->SetCursor(*wxSTANDARD_CURSOR);
		throw(e);
	}
	
	mpTreeCtrl->SetCursor(*wxSTANDARD_CURSOR);
}

void BackupTreeNode::_AddChildrenSlow(bool recursive) 
{
	wxDir dir(mFullPath);
	
	if ( !dir.IsOpened() )
    {
        // wxDir would already show an error message explaining the 
		// exact reason of the failure, and the user can always click [+]
		// another time :-)
        return;
    }

	// delete any existing children of the parent
	mpTreeCtrl->DeleteChildren(mTreeId);
	
	/*
	// update the exclude status of this node 
	// and its parents, just once for efficiency (!)
	this->UpdateExcludedState(TRUE);
	*/
	
	wxString theCurrentFileName;
	
	bool cont = dir.GetFirst(&theCurrentFileName);
	while ( cont )
	{
		wxFileName fileNameObject = wxFileName(mFullPath, theCurrentFileName);

		// add to the tree
		BackupTreeNode *newNode = new BackupTreeNode(
			this, fileNameObject.GetFullPath()
		);
		newNode->SetTreeId(
			mpTreeCtrl->AppendItem(this->mTreeId,
				theCurrentFileName, -1, -1, newNode)
			);
		mpTreeCtrl->UpdateExcludedStateIcon(newNode, false, false);
		
		/*
		newNode->mpTreeCtrl = this->mpTreeCtrl;
		newNode->mpRootNode = this->mpRootNode;
		newNode->mpParentNode = this;
		newNode->mFileName = theCurrentFileName;
		newNode->mpServerSettings  = mpServerSettings;
		newNode->mpServerConnection = mpServerConnection;
		*/
		
		if (newNode->IsDirectory())
		{
			if (recursive) {
				newNode->_AddChildrenSlow(false);
			} else {
				mpTreeCtrl->SetItemHasChildren(newNode->GetTreeId(), TRUE);
			}
		}

		cont = dir.GetNext(&theCurrentFileName);
    }
	
	mpTreeCtrl->SortChildren(this->mTreeId);
}

void BackupTreeCtrl::UpdateExcludedStateIcon(
	BackupTreeNode* pNode, bool updateParents, bool updateChildren) 
{
	int iconId = mEmptyImageId;
	if (updateParents && pNode->GetParentNode() != NULL)
	{
		UpdateExcludedStateIcon(pNode->GetParentNode(), TRUE, FALSE);
	}
	
	pNode->UpdateExcludedState(updateParents);
	BackupTreeNode* pParent = pNode->GetParentNode();
	
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
	SetItemImage(pNode->GetTreeId(), iconId, wxTreeItemIcon_Normal);
	
	if (updateChildren)
	{
		wxTreeItemId thisId = pNode->GetTreeId();
		wxTreeItemIdValue cookie;
		wxTreeItemId childId = GetFirstChild(thisId, cookie);
		while (childId.IsOk())
		{
			BackupTreeNode* pChildNode = 
				(BackupTreeNode*)( GetItemData(childId) );
			UpdateExcludedStateIcon(pChildNode, FALSE, TRUE);
			childId = GetNextChild(thisId, cookie);
		}
	}
}

void BackupTreeNode::UpdateExcludedState(bool updateParents) 
{
	if (updateParents && mpParentNode != NULL)
		mpParentNode->UpdateExcludedState(TRUE);
	
	// by default, inherit our include/exclude state
	// from our parent node, if we have one
	
	if (mpParentNode) {
		mpLocation = mpParentNode->mpLocation;
		mpExcludedBy = mpParentNode->mpExcludedBy;
		mpIncludedBy = mpParentNode->mpIncludedBy;
	} else {
		mpLocation    = NULL;
		mpExcludedBy  = NULL;
		mpIncludedBy  = NULL;
	}
		
	const std::vector<Location*>& rLocations = 
		mpConfig->GetLocations();

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
	}
	
	if (!(mFullPath.StartsWith(mpLocation->GetPath().c_str())))
	{
		wxMessageBox("Full path does not start with location path!",
			"Boxi Error", wxOK | wxICON_ERROR, NULL);
	}
		
	wxLogDebug("Checking %s against exclude list for %s",
		mFullPath.c_str(), mpLocation->GetPath().c_str());

	const std::vector<MyExcludeEntry*>& rExcludeList =
		mpLocation->GetExcludeList().GetEntries();
	
	// on pass 1, remove Excluded files
	// on pass 2, re-add AlwaysInclude files
	
	for (int pass = 1; pass <= 2; pass++) {
		wxLogDebug(" pass %d", pass);
		
		// std::cout << "Pass " << pass << "\n";

		if (pass == 1 && !mpLocation) {
			// not included, so don't bother checking the Exclude entries
			continue;
		} else if (pass == 2 && !mpExcludedBy) {
			// not excluded, so don't bother checking the AlwaysInclude entries
			continue;
		}
		
		for (size_t i = 0; i < rExcludeList.size(); i++) {
			MyExcludeEntry* pExclude = rExcludeList[i];
			ExcludeMatch match = pExclude->GetMatch();
			std::string  value = pExclude->GetValue();
			bool matched = false;
			
			wxLogDebug("  checking against %s",
				pExclude->ToString().c_str());
			
			ExcludeSense sense = pExclude->GetSense();
			if (pass == 1 && sense != ES_EXCLUDE) {
				wxLogDebug("   not an Exclude entry");
				continue;
			}
			if (pass == 2 && sense != ES_ALWAYSINCLUDE) {
				wxLogDebug("   not an AlwaysInclude entry");
				continue;
			}
			
			ExcludeFileDir fileOrDir = pExclude->GetFileDir();
			if (fileOrDir == EFD_FILE && mIsDirectory) {
				wxLogDebug("   doesn't match directories");
				continue;
			}
			if (fileOrDir == EFD_DIR && !mIsDirectory) {
				wxLogDebug("   doesn't match files");
				continue;
			}
			
			if (match == EM_EXACT) {
				if (mFullPath.CompareTo(value.c_str()) == 0) {
					matched = true;
				}
			} else if (match == EM_REGEX) {
				std::auto_ptr<regex_t> apr = 
					std::auto_ptr<regex_t>(new regex_t);
				if (::regcomp(apr.get(), value.c_str(),
					REG_EXTENDED | REG_NOSUB) != 0) 
				{
					wxLogError("Regular expression "
						"compile failed (%s)",
						value.c_str());
				}
				else
				{
					matched = ( regexec(apr.get(), 
							mFullPath.c_str(),
							0, 0, 0)
						    == 0 );
				}
			}
			
			if (!matched) {
				wxLogDebug("   no match.");
				// std::cout << "No match.\n";
				continue;
			}

			wxLogDebug("   matched!");
			
			// std::cout << "Matched!\n";
			
			if (sense == ES_EXCLUDE) {
				mpExcludedBy = pExclude;
			} else if (sense == ES_ALWAYSINCLUDE) {
				mpIncludedBy = pExclude;
			}
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

BackupTreeCtrl::BackupTreeCtrl(
	ClientConfig* pConfig,
	wxWindow* parent, 
	wxWindowID id, 
	const wxPoint& pos, 
	const wxSize& size, 
	long style, 
	const wxValidator& validator, 
	const wxString& name
)
: wxTreeCtrl(parent, id, pos, size, style, validator, name)
{
	wxTreeItemId lRootId;
	BackupTreeNode *pRootNode;
	
	{
		wxString lFullPath;
		
		#ifdef __CYGWIN__
		lFullPath = "c:\\";
		#else
		lFullPath = "/";
		#endif
		
		pRootNode = new BackupTreeNode(this, pConfig, lFullPath);

		wxString lRootName;
		lRootName.Printf("%s (local root)", lFullPath.c_str());
	
		lRootId = AddRoot(lRootName, -1, -1, pRootNode);
		pRootNode->SetTreeId(lRootId);
		SetItemHasChildren(lRootId, TRUE);
	}

	mEmptyImageId        = AddImage(empty16_png,     mImages);
	// mCheckedImageId   = AddImage(tick16_png,      mImages);
	mCheckedImageId      = mImages.Add(wxBitmap(tick16_xpm));	
	mCheckedGreyImageId  = AddImage(tickgrey16_png,  mImages);
	mCrossedImageId      = AddImage(cross16_png,     mImages);
	mCrossedGreyImageId  = AddImage(crossgrey16_png, mImages);
	mAlwaysImageId       = AddImage(plus16_png,      mImages);
	mAlwaysGreyImageId   = AddImage(plusgrey16_png,  mImages);

	SetImageList(&mImages);
	UpdateExcludedStateIcon(pRootNode, false, false);
}

int BackupTreeCtrl::OnCompareItems(
	const wxTreeItemId& item1, 
	const wxTreeItemId& item2)
{
	BackupTreeNode *pNode1 = (BackupTreeNode*)GetItemData(item1);
	BackupTreeNode *pNode2 = (BackupTreeNode*)GetItemData(item2);

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
	
	wxString name1 = pNode1->GetFileName().GetFullPath();
	wxString name2 = pNode2->GetFileName().GetFullPath();
	
	return name1.CompareTo(name2);
}
	
BEGIN_EVENT_TABLE(BackupLocationsPanel, wxPanel)
	EVT_TREE_ITEM_EXPANDING(ID_Backup_Locations_Tree, 
		BackupLocationsPanel::OnTreeNodeExpand)
	EVT_TREE_ITEM_COLLAPSING(ID_Backup_Locations_Tree, 
		BackupLocationsPanel::OnTreeNodeCollapse)
	EVT_TREE_ITEM_ACTIVATED(ID_Backup_Locations_Tree, 
		BackupLocationsPanel::OnTreeNodeActivate)
	EVT_LIST_ITEM_SELECTED(ID_Backup_LocationsList, 
		BackupLocationsPanel::OnBackupLocationClick)
	EVT_LIST_ITEM_SELECTED(ID_BackupLoc_ExcludeList, 
		BackupLocationsPanel::OnLocationExcludeClick)
	EVT_CHOICE(ID_BackupLoc_ExcludeTypeList, 
		BackupLocationsPanel::OnExcludeTypeChoice)
	EVT_TEXT(ID_BackupLoc_ExcludePathCtrl, 	
		BackupLocationsPanel::OnExcludePathChange)
	EVT_BUTTON(ID_BackupLoc_ExcludeAddButton,    
		BackupLocationsPanel::OnExcludeCmdClick)
	EVT_BUTTON(ID_BackupLoc_ExcludeEditButton,   
		BackupLocationsPanel::OnExcludeCmdClick)
	EVT_BUTTON(ID_BackupLoc_ExcludeRemoveButton, 
		BackupLocationsPanel::OnExcludeCmdClick)
	EVT_BUTTON(ID_Backup_LocationsAddButton,  
		BackupLocationsPanel::OnLocationCmdClick)
	EVT_BUTTON(ID_Backup_LocationsEditButton, 
		BackupLocationsPanel::OnLocationCmdClick)
	EVT_BUTTON(ID_Backup_LocationsDelButton,  
		BackupLocationsPanel::OnLocationCmdClick)
	EVT_TEXT(ID_Backup_LocationNameCtrl, 
		BackupLocationsPanel::OnLocationChange)
	EVT_TEXT(ID_Backup_LocationPathCtrl, 
		BackupLocationsPanel::OnLocationChange)
END_EVENT_TABLE()

BackupLocationsPanel::BackupLocationsPanel(ClientConfig *config,
	wxWindow* parent, wxWindowID id,
	const wxPoint& pos, const wxSize& size,
	long style, const wxString& name)
	: wxPanel(parent, id, pos, size, style, name)
{
	mpConfig = config;
	mpConfig->AddListener(this);

	wxBoxSizer *topSizer = new wxBoxSizer( wxVERTICAL );

	wxSplitterWindow *pSplitter = new wxSplitterWindow(this, -1);
	topSizer->Add(pSplitter, 1, wxGROW | wxALL, 8);

	mpTree = new BackupTreeCtrl(mpConfig, pSplitter, ID_Backup_Locations_Tree, 
		wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS);
	
	wxPanel*    pRightPanel = new wxPanel(pSplitter);
	
	pSplitter->SplitVertically(mpTree, pRightPanel, 300);
	
	wxBoxSizer* pRightSizer = new wxBoxSizer( wxVERTICAL );
	pRightPanel->SetSizer(pRightSizer);
	
	theLocationList = new wxListCtrl(
		pRightPanel, ID_Backup_LocationsList,
		wxDefaultPosition, wxDefaultSize, 
		wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_VRULES | wxLC_HRULES);
	pRightSizer->Add(theLocationList, 1, 
		wxGROW | wxLEFT | wxTOP | wxRIGHT | wxBOTTOM, 8);

	wxListItem itemCol;
	itemCol.m_mask = wxLIST_MASK_TEXT | wxLIST_MASK_IMAGE;
	itemCol.m_text = _T("Name");
	itemCol.m_image = -1;
	theLocationList->InsertColumn(0, itemCol);
	itemCol.m_text = _T("Path");
	theLocationList->InsertColumn(1, itemCol);
	itemCol.m_text = _T("Exclusions");
	theLocationList->InsertColumn(2, itemCol);

	wxSizer *theLocationCmdSizer = new wxGridSizer( 1, 0, 4, 4 );
	pRightSizer->Add(theLocationCmdSizer, 0, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	theLocationAddBtn = new wxButton(pRightPanel, 
		ID_Backup_LocationsAddButton, "Add", wxDefaultPosition);
	theLocationCmdSizer->Add(theLocationAddBtn, 1, wxGROW, 0);
	theLocationAddBtn->Disable();
	
	theLocationEditBtn = new wxButton(pRightPanel, 
		ID_Backup_LocationsEditButton, "Edit", wxDefaultPosition);
	theLocationCmdSizer->Add(theLocationEditBtn, 1, wxGROW, 0);
	theLocationEditBtn->Disable();
	
	theLocationRemoveBtn = new wxButton(pRightPanel, 
		ID_Backup_LocationsDelButton, "Remove", wxDefaultPosition);
	theLocationCmdSizer->Add(theLocationRemoveBtn, 1, wxGROW, 0);
	theLocationRemoveBtn->Disable();

	wxGridSizer *theLocParamSizer = new wxGridSizer(2, 4, 4);
	pRightSizer->Add(theLocParamSizer, 0, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	theLocationNameCtrl = new wxTextCtrl(pRightPanel, 
		ID_Backup_LocationNameCtrl, "");
	AddParam(pRightPanel, "Location Name:", theLocationNameCtrl, 
		true, theLocParamSizer);
		
	theLocationPathCtrl = new wxTextCtrl(pRightPanel, 
		ID_Backup_LocationPathCtrl, "");
	AddParam(pRightPanel, "Location Path:", theLocationPathCtrl, 
		true, theLocParamSizer);
		
	theExclusionList = new wxListCtrl(
		pRightPanel, ID_BackupLoc_ExcludeList,
		wxDefaultPosition, wxDefaultSize, 
		wxLC_REPORT | wxLC_VRULES | wxLC_HRULES);
	pRightSizer->Add(theExclusionList, 1, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	itemCol.m_text = _T("Type");
	theExclusionList->InsertColumn(0, itemCol);
	itemCol.m_text = _T("Path or File");
	theExclusionList->InsertColumn(1, itemCol);

	for (int i = 0; i < theExclusionList->GetColumnCount(); i++) 
    	theExclusionList->SetColumnWidth( i, wxLIST_AUTOSIZE_USEHEADER );
	
	wxGridSizer *theExcludeParamSizer = new wxGridSizer(2, 4, 4);
	pRightSizer->Add(theExcludeParamSizer, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	wxString excludeTypes[sizeof(theExcludeTypes) / sizeof(theExcludeTypes[0])];
	for (size_t i = 0; i < sizeof(theExcludeTypes) / sizeof(theExcludeTypes[0]); i++) {
		excludeTypes[i] = wxString(theExcludeTypes[i].ToString().c_str());
	}
	
	theExcludeTypeCtrl = new wxChoice(pRightPanel, ID_BackupLoc_ExcludeTypeList, 
		wxDefaultPosition, wxDefaultSize, 8, excludeTypes);
	AddParam(pRightPanel, "Exclude Type:", theExcludeTypeCtrl, true, 
		theExcludeParamSizer);
		
	theExcludePathCtrl = new wxTextCtrl(pRightPanel, ID_BackupLoc_ExcludePathCtrl, "");
	AddParam(pRightPanel, "Exclude Path:", theExcludePathCtrl, true, 
		theExcludeParamSizer);

	wxSizer *theExcludeCmdSizer = new wxGridSizer( 1, 0, 4, 4 );
	pRightSizer->Add(theExcludeCmdSizer, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	theExcludeAddBtn = new wxButton(pRightPanel, ID_BackupLoc_ExcludeAddButton, 
		"Add", wxDefaultPosition);
	theExcludeAddBtn->Disable();
	theExcludeCmdSizer->Add(theExcludeAddBtn, 1, wxGROW, 0);
	
	theExcludeEditBtn = new wxButton(pRightPanel, ID_BackupLoc_ExcludeEditButton, 
		"Edit", wxDefaultPosition);
	theExcludeEditBtn->Disable();
	theExcludeCmdSizer->Add(theExcludeEditBtn, 1, wxGROW, 0);

	theExcludeRemoveBtn = new wxButton(pRightPanel, ID_BackupLoc_ExcludeRemoveButton, 
		"Remove", wxDefaultPosition);
	theExcludeRemoveBtn->Disable();
	theExcludeCmdSizer->Add(theExcludeRemoveBtn, 1, wxGROW, 0);

	SetSizer( topSizer );
	topSizer->SetSizeHints( this );
	
	theSelectedLocation = 0;
	NotifyChange();
}

void BackupLocationsPanel::OnBackupLocationClick(wxListEvent &event) {
	theSelectedLocation = event.GetIndex();
	Location* pLoc = mpConfig->GetLocations()[theSelectedLocation];
	theLocationNameCtrl->SetValue(pLoc->GetName());
	theLocationPathCtrl->SetValue(pLoc->GetPath());
	mpConfig->NotifyListeners();
}

void BackupLocationsPanel::PopulateLocationList() {
	Location* pSelectedLoc = NULL;
	{
		long selected = theLocationList->GetNextItem(-1, wxLIST_NEXT_ALL,
			wxLIST_STATE_SELECTED);
		if (selected != -1)
			pSelectedLoc = (Location*)(theLocationList->GetItemData(selected));
	}
	
	theLocationList->DeleteAllItems();
	const std::vector<Location*>& rLocs = mpConfig->GetLocations();
	
	for (size_t i = 0; i < rLocs.size(); i++) {
		Location* pLoc = rLocs[i];
		MyExcludeList& rExclude = pLoc->GetExcludeList();
		
		std::string excludeString;
		const std::vector<MyExcludeEntry*>& rEntries = 
			rExclude.GetEntries();
		
		for (size_t count = 0; count < rEntries.size(); 
			count++) 
		{
			excludeString.append(rEntries[count]->ToString());
			if (count != rEntries.size() - 1)
				excludeString.append(", ");
		}
		
		long item = theLocationList->InsertItem(i, pLoc->GetName());
		theLocationList->SetItem(item, 1, pLoc->GetPath());
		theLocationList->SetItem(item, 2, excludeString.c_str());
		theLocationList->SetItemData(item, (long)pLoc);
		
		if (pLoc == pSelectedLoc) {
			mpConfig->RemoveListener(this);
			theLocationList->SetItemState(item, wxLIST_STATE_SELECTED,
				wxLIST_STATE_SELECTED);
			mpConfig->AddListener(this);
		}
	}

	for (int i = 0; i < theLocationList->GetColumnCount(); i++) 
    	theLocationList->SetColumnWidth( i, wxLIST_AUTOSIZE );
}

void BackupLocationsPanel::PopulateExclusionList() {
	if (theSelectedLocation >= mpConfig->GetLocations().size())
		return;
	
	theExclusionList->DeleteAllItems();

	Location* pLoc = mpConfig->GetLocations()[theSelectedLocation];
	const std::vector<MyExcludeEntry*>& rEntries = 
		pLoc->GetExcludeList().GetEntries();
	
	for (size_t index = 0; index < rEntries.size(); index++) {
		MyExcludeEntry *pEntry = rEntries[index];
		theExclusionList->InsertItem(index, pEntry->ToStringType().c_str());
		theExclusionList->SetItem(index, 1, pEntry->GetValue().c_str());
	}
	
	for (int i = 0; i < theExclusionList->GetColumnCount(); i++) 
    	theExclusionList->SetColumnWidth( i, wxLIST_AUTOSIZE );
}

long BackupLocationsPanel::GetSelectedExcludeIndex() {
	return theExclusionList->GetNextItem(
		-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
}

MyExcludeEntry* BackupLocationsPanel::GetExcludeEntry() {
	Location* pLoc = mpConfig->GetLocations()[theSelectedLocation];
	const std::vector<MyExcludeEntry*>& rEntries = 
		pLoc->GetExcludeList().GetEntries();
	return rEntries[GetSelectedExcludeIndex()];
}

void BackupLocationsPanel::OnLocationChange(wxCommandEvent &event) {
	UpdateLocationCtrlEnabledState();
}

void BackupLocationsPanel::OnLocationExcludeClick(wxListEvent &event) {
	MyExcludeEntry *pEntry = GetExcludeEntry();

	for (int i = 0; i < theExcludeTypeCtrl->GetCount(); i++) {
		if (theExcludeTypeCtrl->GetString(i).IsSameAs(
			pEntry->ToStringType().c_str(), TRUE))
		{
			theExcludeTypeCtrl->SetSelection(i);
			break;
		}
	}
	
	theExcludePathCtrl->SetValue(pEntry->GetValue().c_str());
	theExcludeRemoveBtn->Enable();
}

void BackupLocationsPanel::OnExcludeTypeChoice(wxCommandEvent &event) {
	UpdateExcludeCtrlEnabledState();
}

void BackupLocationsPanel::UpdateLocationCtrlEnabledState() {
	wxTextCtrl* 	pNameCtrl 	= theLocationNameCtrl;
	wxTextCtrl*		pPathCtrl 	= theLocationPathCtrl;
	const std::vector<Location*>& rLocs = mpConfig->GetLocations();

	// Enable the Add and Edit buttons if the current values don't match any
	// existing entry, and the Remove button if they do match an existing entry

	theLocationAddBtn   ->Disable();
	theLocationEditBtn  ->Disable();
	
	bool matchExistingEntry = FALSE;
	
	for (size_t i = 0; i < rLocs.size(); i++) {
		Location* pLoc = rLocs[i];
		
		if (pNameCtrl->GetValue().IsSameAs(pLoc->GetName()) &&
			pPathCtrl->GetValue().IsSameAs(pLoc->GetPath()))
		{
			matchExistingEntry = TRUE;
		} 
		else 
		{
			theLocationAddBtn ->Enable();
			theLocationEditBtn->Enable();	
			break;
		} 
	}

	if (matchExistingEntry) {
		theLocationRemoveBtn->Enable();
	} else {
		theLocationRemoveBtn->Disable();
	}
}

void BackupLocationsPanel::UpdateExcludeCtrlEnabledState() {
	if (theSelectedLocation >= mpConfig->GetLocations().size())
		return;

	wxChoice* 		pChoice 	= theExcludeTypeCtrl;
	wxTextCtrl*		pTextCtrl 	= theExcludePathCtrl;
	Location* 		pLoc 		= mpConfig->GetLocations()[theSelectedLocation];
	MyExcludeList& 	rList		= pLoc->GetExcludeList();
	std::vector<MyExcludeEntry*> rEntries = rList.GetEntries();
	
	// Enable the Add and Edit buttons if the current values don't match any
	// existing entry, and the Remove button if they do match an existing entry
	
	theExcludeAddBtn->Enable();
	theExcludeEditBtn->Enable();
	theExcludeRemoveBtn->Disable();
	
	for (size_t i = 0; i < rEntries.size(); i++) {
		MyExcludeEntry* pEntry = rEntries[i];
		if (pChoice->GetStringSelection().IsSameAs(
				pEntry->ToStringType().c_str()) && 
			pTextCtrl->GetValue().IsSameAs(
				pEntry->GetValue().c_str()))
		{
			theExcludeAddBtn->Disable();
			theExcludeEditBtn->Disable();
			theExcludeRemoveBtn->Enable();
			break;
		}
	}
}

void BackupLocationsPanel::OnExcludePathChange(wxCommandEvent &event) {
	UpdateExcludeCtrlEnabledState();
}

const MyExcludeType* BackupLocationsPanel::GetExcludeTypeByName(
	const std::string& name) 
{
	const MyExcludeType* pType = NULL;

	for (size_t i = 0; i < sizeof(theExcludeTypes) / sizeof(MyExcludeType); 
		i++) 
	{
		const MyExcludeType* t = &(theExcludeTypes[i]);
		if (name.compare(t->ToString()) == 0)
		{
			pType = t;
			break;
		}
	}
		
	return pType;
}

void BackupLocationsPanel::OnExcludeCmdClick(wxCommandEvent &event) {
	Location* pLoc = mpConfig->GetLocations()[theSelectedLocation];
	MyExcludeList& 	rList = pLoc->GetExcludeList();
	
	std::string theTypeName = theExcludeTypeCtrl->GetStringSelection().c_str();
	const MyExcludeType* pType = GetExcludeTypeByName(theTypeName);
	assert(pType != NULL);
	
	int sourceId = event.GetId();
	long selIndex = GetSelectedExcludeIndex();
	
	if (sourceId == ID_BackupLoc_ExcludeAddButton ||
		sourceId == ID_BackupLoc_ExcludeEditButton) 
	{
		std::string theNewPath = theExcludePathCtrl->GetValue().c_str();
		MyExcludeEntry *newEntry = new MyExcludeEntry(pType, theNewPath);
		
		if (sourceId == ID_BackupLoc_ExcludeAddButton) {
			rList.AddEntry(newEntry);
		} else if (sourceId == ID_BackupLoc_ExcludeEditButton) {
			rList.ReplaceEntry(selIndex, newEntry);
		}
	}
	else if (sourceId == ID_BackupLoc_ExcludeRemoveButton)
	{
		rList.RemoveEntry(selIndex);
	}

	mpConfig->NotifyListeners();
}

void BackupLocationsPanel::OnLocationCmdClick(wxCommandEvent &event)
{
	int sourceId = event.GetId();
	Location* pCurrentLoc = mpConfig->GetLocations()[theSelectedLocation];
	
	if (sourceId == ID_Backup_LocationsAddButton) 
	{
		Location* pNewLocation = new Location(theLocationNameCtrl->GetValue(),
			theLocationPathCtrl->GetValue());
		MyExcludeList* pExcludeList = new MyExcludeList(
			pCurrentLoc->GetExcludeList());
		pNewLocation->SetExcludeList(pExcludeList);
		mpConfig->AddLocation(pNewLocation);
	}
	else if (sourceId == ID_Backup_LocationsEditButton)
	{
		pCurrentLoc->SetName(theLocationNameCtrl->GetValue().c_str());
		pCurrentLoc->SetPath(theLocationPathCtrl->GetValue().c_str());
	}
	else if (sourceId == ID_Backup_LocationsDelButton)
	{
		mpConfig->RemoveLocation(theSelectedLocation);
	}
	
	mpConfig->NotifyListeners();
}

void BackupLocationsPanel::NotifyChange()
{
	PopulateLocationList();
	PopulateExclusionList();
	UpdateExcludeCtrlEnabledState();
}

void BackupLocationsPanel::OnTreeNodeExpand(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	BackupTreeNode *node = 
		(BackupTreeNode *)(mpTree->GetItemData(item));
	node->AddChildren();
	mpTree->UpdateExcludedStateIcon(node, false, false);
}

void BackupLocationsPanel::OnTreeNodeCollapse(wxTreeEvent& event)
{

}

void BackupLocationsPanel::OnTreeNodeActivate(wxTreeEvent& event)
{
	BackupTreeNode* pTreeNode = 
		(BackupTreeNode*)( mpTree->GetItemData(event.GetItem()) );

	Location*       pLocation   = pTreeNode->GetLocation();
	MyExcludeEntry* pExcludedBy = pTreeNode->GetExcludedBy();
	MyExcludeEntry* pIncludedBy = pTreeNode->GetIncludedBy();
	MyExcludeList*  pList;
	if (pLocation) 
		pList = &(pLocation->GetExcludeList());
	bool updateParents  = FALSE;
	bool updateChildren = FALSE;
	bool updateLocation = FALSE;
	
	if (pIncludedBy != NULL)
	{
		// Tricky case. User wants to remove the AlwaysInclude
		// from this item. Unless the item is itself the subject
		// of the AlwaysInclude directive, we can't easily
		// remove it without affecting other items. The best we 
		// can do is to warn the user, ask them whether to
		// remove it.

		bool doDelete = FALSE;
		bool warnUser = TRUE;
		
		if (pIncludedBy->GetMatch() == EM_EXACT)
		{
			wxFileName fn(pIncludedBy->GetValue().c_str());
			if (fn.SameAs(pTreeNode->GetFileName()))
			{
				doDelete = TRUE;
				warnUser = FALSE;
				updateChildren = TRUE;
			}
		}
		
		if (warnUser) 
		{
			wxString msg;
			msg.Printf(
				"To exclude this item, you will have to exclude "
				"all of %s. Do you really want to do this?",
				pIncludedBy->GetValue().c_str());
			int result = wxMessageBox(msg, "Boxi Warning", 
				wxYES_NO | wxICON_EXCLAMATION);
			if (result == wxYES)
				doDelete = TRUE;
		}
		
		if (doDelete) {
			if (pIncludedBy->GetMatch() != EM_EXACT)
				updateLocation = TRUE;
			else
			{
				updateParents  = TRUE;
				updateChildren = TRUE;
			}
			pList->RemoveEntry(pIncludedBy);
		}			
	} 
	else if	(pExcludedBy != NULL)
	{
		// Node is excluded, and user wants to include it.
		// If the selected node is the value of an exact matching
		// Exclude(File|Dir) entry, we can delete that entry.
		// Otherwise, we will have to add an AlwaysInclude entry for it.
		
		bool handled = FALSE;
		
		if (pExcludedBy->GetMatch() == EM_EXACT)
		{
			wxFileName fn(pExcludedBy->GetValue().c_str());
			if (fn.SameAs(pTreeNode->GetFileName()))
			{
				updateChildren = TRUE;
				pList->RemoveEntry(pExcludedBy);
				handled = TRUE;
			}
		}
		
		if (!handled)
		{
			MyExcludeEntry* pNewEntry = new MyExcludeEntry(
				&theExcludeTypes[
					pTreeNode->IsDirectory() ? ETI_ALWAYS_INCLUDE_DIR
					: ETI_ALWAYS_INCLUDE_FILE],
				pTreeNode->GetFullPath().c_str());
			pList->AddEntry(pNewEntry);
			updateChildren = TRUE;
		}
	}
	else if (pLocation != NULL)
	{
		// user wants to exclude a location, or a file not already 
		// excluded within that location. Which is it? For a location
		// root, we prompt the user and delete the whole location.
		// For a subdirectory or file, we add an Exclude directive.
		bool handled = FALSE;

		wxFileName fn(pLocation->GetPath());
		if (fn.SameAs(pTreeNode->GetFileName()))
		{
			wxString msg;
			msg.Printf("Are you sure you want to delete the location "
				"%s from the server, and remove its configuration?",
				fn.GetFullPath().c_str());
			int result = wxMessageBox(msg, "Boxi Warning", 
				wxYES_NO | wxICON_EXCLAMATION);
			if (result == wxYES)
			{
				mpConfig->RemoveLocation(pLocation);
				handled = TRUE;
				updateChildren = TRUE;
			}
		}
		
		if (!handled) 
		{
			MyExcludeEntry* pNewEntry = new MyExcludeEntry(
				&theExcludeTypes[
					pTreeNode->IsDirectory() ? ETI_EXCLUDE_DIR
					: ETI_EXCLUDE_FILE],
				pTreeNode->GetFullPath().c_str());
			pList->AddEntry(pNewEntry);
			updateChildren = TRUE;
		}
	}
	else
	{
		// outside of any existing location. create a new one.
		wxFileName path(pTreeNode->GetFileName());
		wxString newLocName = path.GetName();
		const std::vector<Location*>& rLocs = mpConfig->GetLocations();
		bool foundUnique = FALSE;
		int counter = 1;
		
		while (!foundUnique)
		{
			foundUnique = TRUE;
			
			for (std::vector<Location*>::const_iterator i = rLocs.begin();
				i != rLocs.end(); i++)
			{
				if (newLocName.IsSameAs((*i)->GetName().c_str()))
				{
					foundUnique = FALSE;
					break;
				}
			}
			
			if (foundUnique) break;
				
			// generate a new filename, and try again
			newLocName.Printf("%s_%d", path.GetName().c_str(), counter);
		}
		
		Location* pNewLoc = new Location(newLocName, pTreeNode->GetFullPath());
		mpConfig->AddLocation(pNewLoc);
		updateChildren = TRUE;
	}
	
	NotifyChange();
	
	if (updateLocation) 
	{
		BackupTreeNode* pRoot = pTreeNode;
		while (
			pRoot->GetParentNode() != NULL && 
			pRoot->GetParentNode()->GetLocation() != NULL)
		{
			pRoot = pRoot->GetParentNode();
		}
		mpTree->UpdateExcludedStateIcon(pRoot, FALSE, TRUE);
	}
	else
		mpTree->UpdateExcludedStateIcon(pTreeNode, updateParents, 
			updateChildren);
}
