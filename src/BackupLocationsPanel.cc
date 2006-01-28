/***************************************************************************
 *            BackupLocationsPanel.cc
 *
 *  Tue Mar  1 00:26:30 2005
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

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/splitter.h>
#include <wx/treectrl.h>

#include "BackupLocationsPanel.h"
#include "FileTree.h"
#include "MainFrame.h"

class BackupTreeNode : public FileNode 
{
	private:
	wxString        mFileName;
	wxString        mFullPath;
	bool            mIsDirectory;
	Location*       mpLocation;
	const MyExcludeEntry* mpExcludedBy;
	const MyExcludeEntry* mpIncludedBy;
	ClientConfig*   mpConfig;
	ExcludedState   mExcludedState;
	
	public:
	int             mIconId;
	
	BackupTreeNode(ClientConfig* pConfig, const wxString& path)
	: FileNode()
	{ 
		mFullPath    = path;
		mFileName    = wxFileName(path).GetFullName();
		mIsDirectory = wxFileName::DirExists(mFullPath);
		mpLocation   = NULL;
		mpExcludedBy = NULL;
		mpIncludedBy = NULL;
		mpConfig     = pConfig;
		mIconId      = -1;
		mExcludedState = EST_UNKNOWN;
	}

	BackupTreeNode(BackupTreeNode* pParent, const wxString& path) 
	: FileNode(pParent)
	{ 
		mFullPath    = path;
		mFileName    = wxFileName(path).GetFullName();
		mIsDirectory = wxFileName::DirExists(mFullPath);
		mpLocation   = pParent->GetLocation();
		mpExcludedBy = pParent->GetExcludedBy();
		mpIncludedBy = pParent->GetIncludedBy();
		mpConfig     = pParent->GetConfig();
		mIconId      = -1;
		mExcludedState = EST_UNKNOWN;
	}
	
	virtual const wxString& GetFileName()   const { return mFileName; }
	virtual const wxString& GetFullPath()   const { return mFullPath; }
	bool                    IsDirectory()   const { return mIsDirectory; }
	Location*               GetLocation()   const { return mpLocation; }
	ClientConfig*           GetConfig()     const { return mpConfig; }
	const MyExcludeEntry*   GetExcludedBy() const { return mpExcludedBy; }
	const MyExcludeEntry*   GetIncludedBy() const { return mpIncludedBy; }

	void SetDirectory(bool value = true) { mIsDirectory = value; }
	virtual int UpdateState(FileImageList& rImageList, bool updateParents);

	private:
	virtual bool _AddChildrenSlow(wxTreeCtrl* pTreeCtrl, bool recurse);
};

bool BackupTreeNode::_AddChildrenSlow(wxTreeCtrl* pTreeCtrl, bool recursive) 
{
	wxDir dir(mFullPath);
	
	if ( !dir.IsOpened() )
    {
        // wxDir would already show an error message explaining the 
		// exact reason of the failure, and the user can always click [+]
		// another time :-)
        return FALSE;
    }

	// delete any existing children of the parent
	pTreeCtrl->DeleteChildren(GetId());
	
	wxString theCurrentFileName;
	
	bool doContinue = dir.GetFirst(&theCurrentFileName);
	while (doContinue)
	{
		wxFileName fileNameObject = wxFileName(mFullPath, theCurrentFileName);

		// add to the tree
		BackupTreeNode *pNewNode = new BackupTreeNode(this,
			fileNameObject.GetFullPath());
		
		wxTreeItemId newId = pTreeCtrl->AppendItem(GetId(),
			theCurrentFileName, -1, -1, pNewNode);
		
		pNewNode->SetId(newId);

		if (pNewNode->IsDirectory())
		{
			if (recursive) 
			{
				if (!pNewNode->_AddChildrenSlow(pTreeCtrl, FALSE))
				{
					return FALSE;
				}
			} 
			else 
			{
				pTreeCtrl->SetItemHasChildren(pNewNode->GetId(), TRUE);
			}
		}

		doContinue = dir.GetNext(&theCurrentFileName);
    }
	
	// sort out the kids
	pTreeCtrl->SortChildren(GetId());
	
	return TRUE;
}

int BackupTreeNode::UpdateState(FileImageList& rImageList, bool updateParents) 
{
	BackupTreeNode* pParentNode = (BackupTreeNode*)GetParentNode();
	
	if (updateParents && pParentNode != NULL)
	{
		pParentNode->UpdateState(rImageList, TRUE);
	}
	
	// by default, inherit our include/exclude state
	// from our parent node, if we have one
	ExcludedState ParentState = EST_UNKNOWN;
	
	if (pParentNode) 
	{
		mpLocation   = pParentNode->mpLocation;
		mpExcludedBy = pParentNode->mpExcludedBy;
		mpIncludedBy = pParentNode->mpIncludedBy;
		ParentState  = pParentNode->mExcludedState;
	} 
	else 
	{
		mpLocation   = NULL;
		mpExcludedBy = NULL;
		mpIncludedBy = NULL;
	}
		
	const std::vector<Location>& rLocations = mpConfig->GetLocations();

	if (!mpLocation) 
	{
		// determine whether or not this node's path
		// is inside a backup location.
	
		for (std::vector<Location>::const_iterator pLoc = rLocations.begin();
			pLoc != rLocations.end(); pLoc++)
		{
			const wxString& rPath = pLoc->GetPath();
			// std::cout << "Compare " << mFullPath 
			//	<< " against " << rPath << "\n";
			if (mFullPath.CompareTo(rPath) == 0) 
			{
				// std::cout << "Found location: " << pLoc->GetName() << "\n";
				mpLocation = mpConfig->GetLocation(*pLoc);
				break;
			}
		}
	}
	
	if (mpLocation && !(mFullPath.StartsWith(mpLocation->GetPath())))
	{
		wxMessageBox(
			wxT("Full path does not start with location path!"),
			wxT("Boxi Error"), wxOK | wxICON_ERROR, NULL);
	}
	
	/*	
	wxLogDebug(wxT("Checking %s against exclude list for %s"),
		mFullPath.c_str(), mpLocation->GetPath().c_str());
	*/
		
	// if this node doesn't belong to a location, then by definition
	// our parent node doesn't have one either, so the inherited
	// values are fine, leave them alone.

	if (mpLocation)
	{
		mExcludedState = mpLocation->GetExcludedState(mFullPath, mIsDirectory,
			&mpExcludedBy, &mpIncludedBy, ParentState);
	}

	int iconId = rImageList.GetEmptyImageId();
	
	if (mpIncludedBy != NULL)
	{
		if (pParentNode && mpIncludedBy == pParentNode->mpIncludedBy)
		{
			iconId = rImageList.GetAlwaysGreyImageId();
		}
		else
		{
			iconId = rImageList.GetAlwaysImageId();
		}
	}
	else if (mpExcludedBy != NULL)
	{
		if (pParentNode && mpExcludedBy == pParentNode->mpExcludedBy)
		{
			iconId = rImageList.GetCrossedGreyImageId();
		}
		else
		{
			iconId = rImageList.GetCrossedImageId();
		}
	}
	else if (mpLocation != NULL)
	{
		if (pParentNode && mpLocation == pParentNode->mpLocation)
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
		// this node is not included in any location, but we need to
		// check whether our path is a prefix to any configured location,
		// to decide whether to display a blank or a partially included icon.
		
		const std::vector<Location>& rLocations = mpConfig->GetLocations();
		wxFileName thisNodePath(mFullPath);
		
		for (std::vector<Location>::const_iterator pLoc = rLocations.begin();
			pLoc != rLocations.end(); pLoc++)
		{
			wxFileName locPath(pLoc->GetPath(), wxT(""));
			
			while(1)
			{
				if (locPath.GetDirCount() == 0)
				{
					break;
				}
				locPath.RemoveLastDir();
				
				wxString fn1 = locPath.GetFullPath();
				wxString fn2 = thisNodePath.GetFullPath();
				
				while (fn1.Length() > 1 && 
					wxFileName::IsPathSeparator(fn1.GetChar(fn1.Length() - 1)))
				{
					fn1.Truncate(fn1.Length() - 1);
				}

				while (fn2.Length() > 1 && 
					wxFileName::IsPathSeparator(fn2.GetChar(fn2.Length() - 1)))
				{
					fn2.Truncate(fn2.Length() - 1);
				}
				
				bool match = fn1.IsSameAs(fn2);
				
				if (match)
				{
					iconId = rImageList.GetPartialImageId();
					break;
				}
			}
		}
	}

	return iconId;
}

class BackupTreeCtrl : public FileTree
{
	public:
	BackupTreeCtrl
	(
		ClientConfig* pConfig,
		wxWindow* pParent, 
		wxWindowID id,
		BackupTreeNode* pRootNode,
		const wxString& rRootLabel
	)
	: FileTree(pParent, id, pRootNode, rRootLabel)
	{ }
	
	private:
	virtual int OnCompareItems(const wxTreeItemId& item1, 
		const wxTreeItemId& item2);
};

int BackupTreeCtrl::OnCompareItems
(
	const wxTreeItemId& item1, 
	const wxTreeItemId& item2
)
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
	
	wxString name1 = pNode1->GetFullPath();
	wxString name2 = pNode2->GetFullPath();
	
	return name1.CompareTo(name2);
}

class EditorPanel : public wxPanel, ConfigChangeListener
{
	protected:
	ClientConfig* mpConfig;
	wxBoxSizer*   mpTopSizer;
	wxListBox*    mpList;
	wxButton*     mpAddButton;
	wxButton*     mpEditButton;
	wxButton*     mpRemoveButton;
	wxStaticBoxSizer* mpListBoxSizer;
	wxStaticBoxSizer* mpDetailsBoxSizer;
	wxGridSizer*      mpDetailsParamSizer;
	
	public:
	EditorPanel(wxWindow* pParent, ClientConfig *pConfig);
	virtual ~EditorPanel() { mpConfig->RemoveListener(this); }
	void NotifyChange(); /* ConfigChangeListener interface */
	
	protected:
	virtual void PopulateList() = 0;
	virtual void UpdateEnabledState() = 0;
	virtual void OnSelectListItem   (wxCommandEvent& rEvent) = 0;
	virtual void OnClickButtonAdd   (wxCommandEvent& rEvent) = 0;
	virtual void OnClickButtonEdit  (wxCommandEvent& rEvent) = 0;
	virtual void OnClickButtonRemove(wxCommandEvent& rEvent) = 0;
	void PopulateLocationList(wxControlWithItems* pTargetList);
	
	DECLARE_EVENT_TABLE()
};

EditorPanel::EditorPanel(wxWindow* pParent, ClientConfig *pConfig)
: wxPanel(pParent, wxID_ANY),
  mpConfig(pConfig)
{
	mpConfig->AddListener(this);

	mpTopSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mpTopSizer);
	
	mpListBoxSizer = new wxStaticBoxSizer(wxVERTICAL, this, wxT(""));
	mpTopSizer->Add(mpListBoxSizer, 1, wxGROW | wxALL, 8);
	
	mpList = new wxListBox(this, ID_Backup_LocationsList);
	mpListBoxSizer->Add(mpList, 1, wxGROW | wxALL, 8);
	
	mpDetailsBoxSizer = new wxStaticBoxSizer(wxVERTICAL, this, wxT(""));
	mpTopSizer->Add(mpDetailsBoxSizer, 0, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpDetailsParamSizer = new wxGridSizer(2, 8, 8);
	mpDetailsBoxSizer->Add(mpDetailsParamSizer, 0, wxGROW | wxALL, 8);

	wxSizer* pButtonSizer = new wxGridSizer( 1, 0, 8, 8 );
	mpDetailsBoxSizer->Add(pButtonSizer, 0, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpAddButton = new wxButton(this, ID_Backup_LocationsAddButton, wxT("Add"));
	pButtonSizer->Add(mpAddButton, 1, wxGROW, 0);
	mpAddButton->Disable();
	
	mpEditButton = new wxButton(this, ID_Backup_LocationsEditButton, wxT("Edit"));
	pButtonSizer->Add(mpEditButton, 1, wxGROW, 0);
	mpEditButton->Disable();
	
	mpRemoveButton = new wxButton(this, ID_Backup_LocationsDelButton, wxT("Remove"));
	pButtonSizer->Add(mpRemoveButton, 1, wxGROW, 0);
	mpRemoveButton->Disable();
}

void EditorPanel::NotifyChange()
{
	PopulateList();
	UpdateEnabledState();
}	

void EditorPanel::PopulateLocationList(wxControlWithItems* pTargetList)
{
	Location* pSelectedLoc = NULL;
	{
		long selected = pTargetList->GetSelection();
		if (selected != wxNOT_FOUND)
		{
			pSelectedLoc = (Location*)( pTargetList->GetClientData(selected) );
		}
	}
	
	pTargetList->Clear();
	const std::vector<Location>& rLocs = mpConfig->GetLocations();
	
	for (std::vector<Location>::const_iterator pLoc = rLocs.begin();
		pLoc != rLocs.end(); pLoc++)
	{
		const MyExcludeList& rExclude = pLoc->GetExcludeList();
		
		wxString locString;
		locString.Printf(wxT("%s -> %s"), 
			pLoc->GetPath().c_str(), pLoc->GetName().c_str());
		
		const std::vector<MyExcludeEntry>& rExcludes = rExclude.GetEntries();
		
		if (rExcludes.size() > 0)
		{
			locString.Append(wxT(" ("));

			int size = rExcludes.size();
			for (std::vector<MyExcludeEntry>::const_iterator pExclude = rExcludes.begin();
				pExclude != rExcludes.end(); pExclude++)
			{
				wxString entryDesc(pExclude->ToString().c_str(), wxConvLibc);
				locString.Append(entryDesc);
				
				if (--size)
				{
					locString.Append(wxT(", "));
				}
			}

			locString.Append(wxT(")"));
		}
			
		int newIndex = pTargetList->Append(locString);
		pTargetList->SetClientData(newIndex, mpConfig->GetLocation(*pLoc));
		
		if (pSelectedLoc && pLoc->IsSameAs(*pSelectedLoc)) 
		{
			pTargetList->SetSelection(newIndex);
		}
	}
}

BEGIN_EVENT_TABLE(EditorPanel, wxPanel)
	EVT_LISTBOX(ID_Backup_LocationsList, 
		EditorPanel::OnSelectListItem)
	EVT_BUTTON(ID_Backup_LocationsAddButton,  
		EditorPanel::OnClickButtonAdd)
	EVT_BUTTON(ID_Backup_LocationsEditButton, 
		EditorPanel::OnClickButtonEdit)
	EVT_BUTTON(ID_Backup_LocationsDelButton,  
		EditorPanel::OnClickButtonRemove)
END_EVENT_TABLE()

class LocationsPanel : public EditorPanel
{
	private:
	wxTextCtrl*   mpNameText;
	wxTextCtrl*   mpPathText;

	public:
	LocationsPanel(wxWindow* pParent, ClientConfig *pConfig);
	virtual void PopulateList();
	virtual void PopulateControls();
	virtual void UpdateEnabledState();
	virtual void OnSelectListItem       (wxCommandEvent& rEvent);
	virtual void OnClickButtonAdd       (wxCommandEvent& rEvent);
	virtual void OnClickButtonEdit      (wxCommandEvent& rEvent);
	virtual void OnClickButtonRemove    (wxCommandEvent& rEvent);
	virtual void OnChangeLocationDetails(wxCommandEvent& rEvent);
	
	private:
	void SelectLocation(const Location& rLocation);
	Location* GetSelectedLocation();
	
	DECLARE_EVENT_TABLE()
};

LocationsPanel::LocationsPanel(wxWindow* pParent, ClientConfig *pConfig)
: EditorPanel(pParent, pConfig)
{
	mpListBoxSizer   ->GetStaticBox()->SetLabel(wxT("&Locations"));
	mpDetailsBoxSizer->GetStaticBox()->SetLabel(wxT("&Selected or New Location"));

	mpNameText = new wxTextCtrl(this, ID_Backup_LocationNameCtrl, wxT(""));
	AddParam(this, wxT("Location &Name:"), mpNameText, true, mpDetailsParamSizer);
		
	mpPathText = new wxTextCtrl(this, ID_Backup_LocationPathCtrl, wxT(""));
	AddParam(this, wxT("Location &Path:"), mpPathText, true, mpDetailsParamSizer);

	NotifyChange();
}

void LocationsPanel::PopulateList()
{
	PopulateLocationList(mpList);
}

void LocationsPanel::UpdateEnabledState() 
{
	const std::vector<Location>& rLocs = mpConfig->GetLocations();

	// Enable the Add and Edit buttons if the current values 
	// don't match any existing entry, and the Remove button 
	// if they do match an existing entry

	bool matchExistingEntry = FALSE;
	
	for (std::vector<Location>::const_iterator pLocation = rLocs.begin();
		pLocation != rLocs.end(); pLocation++)
	{
		if (mpNameText->GetValue().IsSameAs(pLocation->GetName()) &&
			mpPathText->GetValue().IsSameAs(pLocation->GetPath()))
		{
			matchExistingEntry = TRUE;
			break;
		}
	}
	
	if (matchExistingEntry) 
	{
		mpAddButton->Disable();
	} 
	else 
	{
		mpAddButton->Enable();
	}
	
	if (mpList->GetSelection() == wxNOT_FOUND)
	{
		mpEditButton  ->Disable();	
		mpRemoveButton->Disable();
	}
	else if (matchExistingEntry)
	{
		mpEditButton  ->Disable();	
		mpRemoveButton->Enable();
	}
	else
	{
		mpEditButton  ->Enable();	
		mpRemoveButton->Disable();
	}		
}

BEGIN_EVENT_TABLE(LocationsPanel, EditorPanel)
	EVT_TEXT(ID_Backup_LocationNameCtrl, 
		LocationsPanel::OnChangeLocationDetails)
	EVT_TEXT(ID_Backup_LocationPathCtrl, 
		LocationsPanel::OnChangeLocationDetails)
END_EVENT_TABLE()

void LocationsPanel::OnSelectListItem(wxCommandEvent &event)
{
	PopulateControls();
}

Location* LocationsPanel::GetSelectedLocation()
{
	int selected = mpList->GetSelection();	
	if (selected == wxNOT_FOUND)
	{
		return NULL;
	}
	
	Location* pLocation = (Location*)( mpList->GetClientData(selected) );
	return pLocation;
}

void LocationsPanel::PopulateControls()
{
	Location* pLocation = GetSelectedLocation();

	if (pLocation)
	{
		mpNameText->SetValue(pLocation->GetName());
		mpPathText->SetValue(pLocation->GetPath());
	}
	else
	{
		mpNameText->SetValue(wxT(""));
		mpPathText->SetValue(wxT(""));
	}
	
	UpdateEnabledState();
}

void LocationsPanel::OnChangeLocationDetails(wxCommandEvent& rEvent)
{
	UpdateEnabledState();
}

void LocationsPanel::OnClickButtonAdd(wxCommandEvent &event)
{
	Location newLoc(
		mpNameText->GetValue(),
		mpPathText->GetValue(), mpConfig);

	Location* pOldLocation = GetSelectedLocation();
	if (pOldLocation)
	{
		newLoc.SetExcludeList(pOldLocation->GetExcludeList());
	}
	
	mpConfig->AddLocation(newLoc);
	SelectLocation(newLoc);
}

void LocationsPanel::OnClickButtonEdit(wxCommandEvent &event)
{
	Location* pLocation = GetSelectedLocation();
	if (!pLocation)
	{
		wxMessageBox(wxT("Editing nonexistant location!"), wxT("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}
	
	pLocation->SetName(mpNameText->GetValue());
	pLocation->SetPath(mpPathText->GetValue());
}

void LocationsPanel::OnClickButtonRemove(wxCommandEvent &event)
{
	Location* pLocation = GetSelectedLocation();
	if (!pLocation)
	{
		wxMessageBox(wxT("Removing nonexistant location!"), wxT("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}
	mpConfig->RemoveLocation(*pLocation);
	PopulateControls();
}

void LocationsPanel::SelectLocation(const Location& rLocation)
{
	for (int i = 0; i < mpList->GetCount(); i++)
	{
		Location* pLocation = (Location*)( mpList->GetClientData(i) );
		if (pLocation->IsSameAs(rLocation)) 
		{
			mpList->SetSelection(i);
			break;
		}
	}
}

class ExclusionsPanel : public EditorPanel
{
	private:
	wxChoice*   mpLocationList;
	wxChoice*   mpTypeList;
	wxTextCtrl* mpValueText;

	public:
	ExclusionsPanel(wxWindow* pParent, ClientConfig *pConfig);
	virtual void PopulateList();
	virtual void PopulateControls();
	virtual void UpdateEnabledState();
	virtual void OnSelectLocationItem  (wxCommandEvent& rEvent);
	virtual void OnSelectListItem      (wxCommandEvent& rEvent);
	virtual void OnClickButtonAdd      (wxCommandEvent& rEvent);
	virtual void OnClickButtonEdit     (wxCommandEvent& rEvent);
	virtual void OnClickButtonRemove   (wxCommandEvent& rEvent);
	virtual void OnChangeExcludeDetails(wxCommandEvent& rEvent);
	
	private:
	void SelectExclusion(const MyExcludeEntry& rEntry);
	Location* GetSelectedLocation();
	MyExcludeEntry* GetSelectedEntry();
	MyExcludeEntry  CreateNewEntry();
	const std::vector<MyExcludeEntry>* GetSelectedLocEntries();
	
	DECLARE_EVENT_TABLE()
};

ExclusionsPanel::ExclusionsPanel(wxWindow* pParent, ClientConfig *pConfig)
: EditorPanel(pParent, pConfig)
{
	wxStaticBoxSizer* pLocationListBox = new wxStaticBoxSizer(wxVERTICAL, 
		this, wxT("&Locations"));
	mpTopSizer->Insert(0, pLocationListBox, 0, 
		wxGROW | wxTOP | wxLEFT | wxRIGHT, 8);
	
	mpLocationList = new wxChoice(this, ID_Backup_LocationsList);
	pLocationListBox->Add(mpLocationList, 0, wxGROW | wxALL, 8);
	
	mpListBoxSizer   ->GetStaticBox()->SetLabel(wxT("&Exclusions"));
	mpDetailsBoxSizer->GetStaticBox()->SetLabel(wxT("&Selected or New Exclusion"));

	wxString excludeTypes[numExcludeTypes];
	
	for (size_t i = 0; i < numExcludeTypes; i++) 
	{
		excludeTypes[i] = wxString(
			theExcludeTypes[i].ToString().c_str(), 
			wxConvLibc);
	}

	mpTypeList = new wxChoice(this, ID_BackupLoc_ExcludeTypeList, 
		wxDefaultPosition, wxDefaultSize, 8, excludeTypes);

	for (size_t i = 0; i < numExcludeTypes; i++) 
	{
		mpTypeList->SetClientData(i, &(theExcludeTypes[i]));
	}
	
	AddParam(this, wxT("Exclude Type:"), mpTypeList, true, 
		mpDetailsParamSizer);
		
	mpValueText = new wxTextCtrl(this, ID_BackupLoc_ExcludePathCtrl, wxT(""));
	AddParam(this, wxT("Exclude Path:"), mpValueText, true, 
		mpDetailsParamSizer);

	NotifyChange();
}

Location* ExclusionsPanel::GetSelectedLocation()
{
	int selectedLocIndex = mpLocationList->GetSelection();
	if (selectedLocIndex == wxNOT_FOUND) return NULL;
	
	Location* mpLocation = (Location*)( 
		mpLocationList->GetClientData(selectedLocIndex) );
	return mpLocation;
}
	
const std::vector<MyExcludeEntry>* ExclusionsPanel::GetSelectedLocEntries()
{	
	Location* mpLocation = GetSelectedLocation();
	if (!mpLocation) return NULL;
		
	MyExcludeList& rExcludeList = mpLocation->GetExcludeList();
	return &rExcludeList.GetEntries();
}

MyExcludeEntry* ExclusionsPanel::GetSelectedEntry()
{
	int selected = mpList->GetSelection();
	if (selected == wxNOT_FOUND)
		return NULL;
	
	MyExcludeEntry* pEntry = (MyExcludeEntry*)( mpList->GetClientData(selected) );
	return pEntry;
}

void ExclusionsPanel::PopulateList()
{
	PopulateLocationList(mpLocationList);
	
	MyExcludeEntry* pSelectedEntry = GetSelectedEntry();	
	mpList->Clear();
	
	Location* pLocation = GetSelectedLocation();
	if (!pLocation) return;
	
	MyExcludeList& rExcludeList = pLocation->GetExcludeList();
	const std::vector<MyExcludeEntry>& rEntries = rExcludeList.GetEntries();
	
	for (std::vector<MyExcludeEntry>::const_iterator pEntry = rEntries.begin();
		pEntry != rEntries.end(); pEntry++)
	{
		wxString exString(pEntry->ToString().c_str(), wxConvLibc);
		int newIndex = mpList->Append(exString);
		mpList->SetClientData(newIndex, rExcludeList.UnConstEntry(*pEntry));
		
		if (pSelectedEntry && pEntry->IsSameAs(*pSelectedEntry)) 
		{
			mpList->SetSelection(newIndex);
		}
	}
}

void ExclusionsPanel::UpdateEnabledState() 
{
	if (mpLocationList->GetSelection() == wxNOT_FOUND)
	{
		mpList->Disable();
		mpTypeList->Disable();
		mpValueText->Disable();
		mpAddButton->Disable();
		mpEditButton->Disable();
		mpRemoveButton->Disable();
		return;
	}
	
	mpList->Enable();
	mpTypeList->Enable();
	mpValueText->Enable();
	
	// Enable the Add and Edit buttons if the current values 
	// don't match any existing entry, and the Remove button 
	// if they do match an existing entry

	bool matchExistingEntry = FALSE;
	
	const std::vector<MyExcludeEntry>* pEntries = GetSelectedLocEntries();
	if (pEntries) 
	{
		for (std::vector<MyExcludeEntry>::const_iterator pEntry = pEntries->begin();
			pEntry != pEntries->end(); pEntry++)
		{
			int selection = mpTypeList->GetSelection();
			if (selection != wxNOT_FOUND &&
				mpTypeList->GetClientData(selection) == pEntry->GetType() &&
				mpValueText->GetValue().IsSameAs(pEntry->GetValueString()))
			{
				matchExistingEntry = TRUE;
				break;
			}
		}
	}
	
	if (matchExistingEntry) 
	{
		mpAddButton->Disable();
	} 
	else 
	{
		mpAddButton->Enable();
	}
	
	if (mpList->GetSelection() == wxNOT_FOUND)
	{
		mpEditButton  ->Disable();	
		mpRemoveButton->Disable();
	}
	else if (matchExistingEntry)
	{
		mpEditButton  ->Disable();	
		mpRemoveButton->Enable();
	}
	else
	{
		mpEditButton  ->Enable();	
		mpRemoveButton->Disable();
	}		
}

BEGIN_EVENT_TABLE(ExclusionsPanel, EditorPanel)
	EVT_CHOICE(ID_Backup_LocationsList,
		ExclusionsPanel::OnSelectLocationItem)
	EVT_CHOICE(ID_BackupLoc_ExcludeTypeList, 
		ExclusionsPanel::OnChangeExcludeDetails)
	EVT_TEXT(ID_BackupLoc_ExcludePathCtrl, 
		ExclusionsPanel::OnChangeExcludeDetails)
END_EVENT_TABLE()

void ExclusionsPanel::OnSelectLocationItem(wxCommandEvent &event)
{
	PopulateList();
	UpdateEnabledState();
}

void ExclusionsPanel::OnSelectListItem(wxCommandEvent &event)
{
	PopulateControls();
}

void ExclusionsPanel::PopulateControls()
{
	int selected = mpList->GetSelection();
	
	if (selected == wxNOT_FOUND)
	{
		mpTypeList->SetSelection(wxNOT_FOUND);
		mpValueText->SetValue(wxT(""));
	}
	else
	{
		MyExcludeEntry* pEntry = (MyExcludeEntry*)( mpList->GetClientData(selected) );
		for (int i = 0; i < mpTypeList->GetCount(); i++)
		{
			if (mpTypeList->GetClientData(i) == pEntry->GetType())
			{
				mpTypeList->SetSelection(i);
				break;
			}
		}
		mpValueText->SetValue(pEntry->GetValueString());
	}
	
	UpdateEnabledState();
}

void ExclusionsPanel::OnChangeExcludeDetails(wxCommandEvent& rEvent)
{
	UpdateEnabledState();
}

MyExcludeEntry ExclusionsPanel::CreateNewEntry()
{
	MyExcludeType* pType = NULL;
	
	long selectedType = mpTypeList->GetSelection();
	if (selectedType != wxNOT_FOUND)
	{
		pType = (MyExcludeType*)( mpTypeList->GetClientData(selectedType) );
	}

	return MyExcludeEntry(*pType, mpValueText->GetValue());
}

void ExclusionsPanel::OnClickButtonAdd(wxCommandEvent &event)
{
	if (mpTypeList->GetSelection() == wxNOT_FOUND)
	{
		wxMessageBox(wxT("No type selected!"), wxT("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	MyExcludeEntry newEntry = CreateNewEntry();
	
	Location* pLocation = GetSelectedLocation();
	MyExcludeList& rList = pLocation->GetExcludeList();	
	rList.AddEntry(newEntry);
	// calls listener and updates list automatically
	
	SelectExclusion(newEntry);
}

void ExclusionsPanel::OnClickButtonEdit(wxCommandEvent &event)
{
	long selected = mpLocationList->GetSelection();
	if (selected == wxNOT_FOUND)
	{
		wxMessageBox(wxT("Editing nonexistant location!"), wxT("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	MyExcludeEntry* pOldEntry = GetSelectedEntry();
	if (!pOldEntry)
	{
		wxMessageBox(wxT("No exclude entry selected!"), wxT("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	if (mpTypeList->GetSelection() == wxNOT_FOUND)
	{
		wxMessageBox(wxT("No type selected!"), wxT("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	MyExcludeEntry   newEntry = CreateNewEntry();
	
	Location* pLocation = GetSelectedLocation();
	MyExcludeList& rList = pLocation->GetExcludeList();
	
	rList.ReplaceEntry(*pOldEntry, newEntry);
	SelectExclusion(newEntry);
}

void ExclusionsPanel::OnClickButtonRemove(wxCommandEvent &event)
{
	MyExcludeEntry* pOldEntry = GetSelectedEntry();
	if (!pOldEntry)
	{
		wxMessageBox(wxT("Removing nonexistant location!"), wxT("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	Location* pLocation = GetSelectedLocation();
	MyExcludeList& rList = pLocation->GetExcludeList();
	rList.RemoveEntry(*pOldEntry);
	PopulateControls();
}

void ExclusionsPanel::SelectExclusion(const MyExcludeEntry& rEntry)
{
	for (int i = 0; i < mpList->GetCount(); i++)
	{
		MyExcludeEntry* pEntry = (MyExcludeEntry*)( mpList->GetClientData(i) );
		if (pEntry->IsSameAs(rEntry)) 
		{
			mpList->SetSelection(i);
			break;
		}
	}
}

BEGIN_EVENT_TABLE(BackupLocationsPanel, wxPanel)
	EVT_TREE_ITEM_ACTIVATED(ID_Backup_Locations_Tree, 
		BackupLocationsPanel::OnTreeNodeActivate)
	EVT_BUTTON(wxID_CANCEL, BackupLocationsPanel::OnClickCloseButton)
END_EVENT_TABLE()

BackupLocationsPanel::BackupLocationsPanel
(
	ClientConfig* pConfig,
	wxWindow* pParent, 
	MainFrame* pMainFrame,
	wxPanel* pPanelToShowOnClose,
	wxWindowID id,
	const wxPoint& pos, 
	const wxSize& size,
	long style, 
	const wxString& name
)
:	wxPanel(pParent, id, pos, size, style, name),
	mpMainFrame(pMainFrame),
	mpPanelToShowOnClose(pPanelToShowOnClose)
{
	mpConfig = pConfig;
	mpConfig->AddListener(this);

	wxBoxSizer* pTopSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(pTopSizer);
	
	wxNotebook* pNotebook = new wxNotebook(this, wxID_ANY);
	pTopSizer->Add(pNotebook, 1, wxGROW | wxALL, 8);
	
	wxPanel* pBasicPanel = new wxPanel(pNotebook);
	pNotebook->AddPage(pBasicPanel, wxT("Basic"));
	
	wxBoxSizer* pBasicPanelSizer = new wxBoxSizer(wxVERTICAL);
	pBasicPanel->SetSizer(pBasicPanelSizer);

#if defined CYGWIN || defined WIN32
	mpRootNode = new BackupTreeNode(pConfig, wxT("c:\\"));
	wxString rootLabel = wxT("My Computer");
#else
	mpRootNode = new BackupTreeNode(pConfig, wxT("/"));
	wxString rootLabel = wxT("/ (local root)");
#endif
	
	mpTree = new BackupTreeCtrl(mpConfig, pBasicPanel, 
		ID_Backup_Locations_Tree, mpRootNode, rootLabel);
	pBasicPanelSizer->Add(mpTree, 1, wxGROW | wxALL, 8);
	
	wxPanel* pLocationsPanel = new LocationsPanel(pNotebook, mpConfig);
	pNotebook->AddPage(pLocationsPanel, wxT("Locations"));

	wxPanel* pExclusionsPanel = new ExclusionsPanel(pNotebook, mpConfig);
	pNotebook->AddPage(pExclusionsPanel, wxT("Exclusions"));

	/*
	theExclusionList = new wxListCtrl(
		pAdvancedPanel, ID_BackupLoc_ExcludeList,
		wxDefaultPosition, wxDefaultSize, 
		wxLC_REPORT | wxLC_VRULES | wxLC_HRULES);
	pAdvancedPanelSizer->Add(theExclusionList, 1, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	itemCol.m_text = _T("Type");
	theExclusionList->InsertColumn(0, itemCol);
	itemCol.m_text = _T("Path or File");
	theExclusionList->InsertColumn(1, itemCol);

	for (int i = 0; i < theExclusionList->GetColumnCount(); i++) 
    	theExclusionList->SetColumnWidth( i, wxLIST_AUTOSIZE_USEHEADER );
	
	wxGridSizer *theExcludeParamSizer = new wxGridSizer(2, 4, 4);
	pAdvancedPanelSizer->Add(theExcludeParamSizer, 0, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	#define numExcludeTypes (sizeof(theExcludeTypes) / sizeof(*theExcludeTypes))
	
	wxString excludeTypes[numExcludeTypes];
	
	for (size_t i = 0; i < numExcludeTypes; i++) {
		excludeTypes[i] = wxString(
			theExcludeTypes[i].ToString().c_str(), 
			wxConvLibc);
	}
	
	theExcludeTypeCtrl = new wxChoice(pAdvancedPanel, 
		ID_BackupLoc_ExcludeTypeList, 
		wxDefaultPosition, wxDefaultSize, 8, excludeTypes);
	AddParam(pAdvancedPanel, wxT("Exclude Type:"), theExcludeTypeCtrl, true, 
		theExcludeParamSizer);
		
	theExcludePathCtrl = new wxTextCtrl(pAdvancedPanel, 
		ID_BackupLoc_ExcludePathCtrl, wxT(""));
	AddParam(pAdvancedPanel, wxT("Exclude Path:"), theExcludePathCtrl, true, 
		theExcludeParamSizer);

	wxSizer *theExcludeCmdSizer = new wxGridSizer( 1, 0, 4, 4 );
	pAdvancedPanelSizer->Add(theExcludeCmdSizer, 0, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	theExcludeAddBtn = new wxButton(pAdvancedPanel, 
		ID_BackupLoc_ExcludeAddButton, 
		wxT("Add"), wxDefaultPosition);
	theExcludeAddBtn->Disable();
	theExcludeCmdSizer->Add(theExcludeAddBtn, 1, wxGROW, 0);
	
	theExcludeEditBtn = new wxButton(pAdvancedPanel, 
		ID_BackupLoc_ExcludeEditButton, 
		wxT("Edit"), wxDefaultPosition);
	theExcludeEditBtn->Disable();
	theExcludeCmdSizer->Add(theExcludeEditBtn, 1, wxGROW, 0);

	theExcludeRemoveBtn = new wxButton(pAdvancedPanel, 
		ID_BackupLoc_ExcludeRemoveButton, 
		wxT("Remove"), wxDefaultPosition);
	theExcludeRemoveBtn->Disable();
	theExcludeCmdSizer->Add(theExcludeRemoveBtn, 1, wxGROW, 0);
	
	theSelectedLocation = 0;
	NotifyChange();
	*/

	wxSizer* pActionCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	pTopSizer->Add(pActionCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	wxButton* pCloseButton = new wxButton(this, wxID_CANCEL, wxT("Close"));
	pActionCtrlSizer->Add(pCloseButton, 0, wxGROW | wxLEFT, 8);
}

/*
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
		long selected = theLocationList->GetNextItem(-1, 
			wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (selected != -1)
			pSelectedLoc = (Location*)
				(theLocationList->GetItemData(selected));
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
		theLocationList->SetItem(item, 2, 
			wxString(excludeString.c_str(), wxConvLibc));
		theLocationList->SetItemData(item, (long)pLoc);
		
		if (pLoc == pSelectedLoc) {
			mpConfig->RemoveListener(this);
			theLocationList->SetItemState(item, 
				wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
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
		wxString type (pEntry->ToStringType().c_str(), wxConvLibc);
		wxString value(pEntry->GetValue().c_str(),     wxConvLibc);
		theExclusionList->InsertItem(index, type);
		theExclusionList->SetItem(index, 1, value);
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
			wxString(pEntry->ToStringType().c_str(), wxConvLibc), 
			TRUE))
		{
			theExcludeTypeCtrl->SetSelection(i);
			break;
		}
	}
	
	wxString path(pEntry->GetValue().c_str(), wxConvLibc);
	theExcludePathCtrl->SetValue(path);
	theExcludeRemoveBtn->Enable();
}

void BackupLocationsPanel::OnExcludeTypeChoice(wxCommandEvent &event) {
	UpdateExcludeCtrlEnabledState();
}

void BackupLocationsPanel::UpdateLocationCtrlEnabledState() {
	wxTextCtrl* pNameCtrl = theLocationNameCtrl;
	wxTextCtrl* pPathCtrl = theLocationPathCtrl;
	const std::vector<Location*>& rLocs = mpConfig->GetLocations();

	// Enable the Add and Edit buttons if the current values 
	// don't match any existing entry, and the Remove button 
	// if they do match an existing entry

	theLocationAddBtn ->Disable();
	theLocationEditBtn->Disable();
	
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

	wxChoice* 	pChoice 	= theExcludeTypeCtrl;
	wxTextCtrl*	pTextCtrl 	= theExcludePathCtrl;
	Location* 	pLoc 		= mpConfig->GetLocations()[theSelectedLocation];
	MyExcludeList& 	rList		= pLoc->GetExcludeList();
	std::vector<MyExcludeEntry*> rEntries = rList.GetEntries();
	
	// Enable the Add and Edit buttons if the current values 
	// don't match any existing entry, and the Remove button 
	// if they do match an existing entry
	
	theExcludeAddBtn->Enable();
	theExcludeEditBtn->Enable();
	theExcludeRemoveBtn->Disable();
	
	for (size_t i = 0; i < rEntries.size(); i++) {
		MyExcludeEntry* pEntry = rEntries[i];
		wxString type (pEntry->ToStringType().c_str(), wxConvLibc);
		wxString value(pEntry->GetValue().c_str(), wxConvLibc);

		if (pChoice->GetStringSelection().IsSameAs(type) &&
			pTextCtrl->GetValue().IsSameAs(value))
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

	wxCharBuffer theTypeString = theExcludeTypeCtrl->GetStringSelection()
		.mb_str(wxConvLibc);	
	std::string theTypeName = theTypeString.data();

	const MyExcludeType* pType = GetExcludeTypeByName(theTypeName);
	assert(pType != NULL);
	
	int sourceId = event.GetId();
	long selIndex = GetSelectedExcludeIndex();
	
	if (sourceId == ID_BackupLoc_ExcludeAddButton ||
		sourceId == ID_BackupLoc_ExcludeEditButton) 
	{
		wxCharBuffer buf = 
			theExcludePathCtrl->GetValue().mb_str(wxConvLibc);
		std::string theNewPath = buf.data();
		MyExcludeEntry *newEntry = 
			new MyExcludeEntry(pType, theNewPath);
		
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
		Location* pNewLocation = new Location(
			theLocationNameCtrl->GetValue(),
			theLocationPathCtrl->GetValue());
		MyExcludeList* pExcludeList = new MyExcludeList(
			pCurrentLoc->GetExcludeList());
		pNewLocation->SetExcludeList(pExcludeList);
		mpConfig->AddLocation(pNewLocation);
	}
	else if (sourceId == ID_Backup_LocationsEditButton)
	{
		wxCharBuffer name = theLocationNameCtrl->GetValue()
			.mb_str(wxConvLibc);
		wxCharBuffer path = theLocationPathCtrl->GetValue()
			.mb_str(wxConvLibc);
		pCurrentLoc->SetName(name.data());
		pCurrentLoc->SetPath(path.data());
	}
	else if (sourceId == ID_Backup_LocationsDelButton)
	{
		mpConfig->RemoveLocation(theSelectedLocation);
	}
	
	mpConfig->NotifyListeners();
}
*/

void BackupLocationsPanel::NotifyChange()
{
	UpdateTreeOnConfigChange();
	// UpdateAdvancedTabOnConfigChange();	
}

void BackupLocationsPanel::UpdateTreeOnConfigChange()
{
	mpTree->UpdateStateIcon(mpRootNode, FALSE, TRUE);
}

/*
void BackupLocationsPanel::UpdateAdvancedTabOnConfigChange()
{
	PopulateLocationList();
	PopulateExclusionList();
	UpdateExcludeCtrlEnabledState();
}
*/

/*
void BackupLocationsPanel::OnTreeNodeExpand(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	BackupTreeNode *pNode = (BackupTreeNode *)(mpTree->GetItemData(item));
	pNode->AddChildren(mpTree, FALSE);
	mpTree->UpdateStateIcon(pNode, FALSE, FALSE);
}
*/

/*
void BackupLocationsPanel::OnTreeNodeCollapse(wxTreeEvent& event)
{

}
*/

void BackupLocationsPanel::OnTreeNodeActivate(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	BackupTreeNode* pTreeNode = (BackupTreeNode *)(mpTree->GetItemData(item));

	Location*       pLocation   = pTreeNode->GetLocation();
	const MyExcludeEntry* pExcludedBy = pTreeNode->GetExcludedBy();
	const MyExcludeEntry* pIncludedBy = pTreeNode->GetIncludedBy();
	MyExcludeList*  pList = NULL;

	if (pLocation) 
		pList = &(pLocation->GetExcludeList());

	BackupTreeNode* pUpdateFrom = NULL;
	bool updateChildren = FALSE;
	
	// avoid updating whole tree
	mpConfig->RemoveListener(this);
	
	if (pIncludedBy != NULL)
	{
		// Tricky case. User wants to exclude an AlwaysIncluded item.
		// We can't override it with anything, and unless this item is 
		// the only match of the AlwaysInclude directive, we can't 
		// remove it without affecting other items. The best we can do is 
		// to warn the user, ask them whether to remove it.

		bool doDelete = FALSE;
		bool warnUser = TRUE;
		
		if (pIncludedBy->GetMatch() == EM_EXACT)
		{
			wxFileName fn(wxString(
				pIncludedBy->GetValue().c_str(), wxConvLibc));
			if (fn.SameAs(pTreeNode->GetFullPath()))
			{
				doDelete = TRUE;
				warnUser = FALSE;
				updateChildren = TRUE;
			}
		}
		
		if (warnUser) 
		{
			wxString PathToExclude(pIncludedBy->GetValue().c_str(), 
				wxConvLibc);
			
			wxString msg;
			msg.Printf(wxT(
				"To exclude this item, you will have to "
				"exclude all of %s. Do you really want to "
				"do this?"),
				PathToExclude.c_str());
			
			int result = wxMessageBox(msg, wxT("Boxi Warning"), 
				wxYES_NO | wxICON_EXCLAMATION);
			if (result == wxYES)
				doDelete = TRUE;
		}
		
		if (doDelete) 
		{
			pUpdateFrom = pTreeNode;
			
			if (pIncludedBy->GetMatch() != EM_EXACT)
			{
				for (BackupTreeNode* pParentNode = 
						(BackupTreeNode*)( pTreeNode->GetParentNode() );
					pParentNode != NULL && 
					pParentNode->GetLocation() == pLocation;
					pParentNode = 
						(BackupTreeNode*)( pParentNode->GetParentNode()) )
				{
					pUpdateFrom = pParentNode;
				}
			}
			else
			{
				for (BackupTreeNode* pParentNode = 
						(BackupTreeNode*)( pTreeNode->GetParentNode() );
					pParentNode != NULL && 
					pParentNode->GetExcludedBy() == pExcludedBy;
					pParentNode = 
						(BackupTreeNode*)( pParentNode->GetParentNode()) )
				{
					pUpdateFrom = pParentNode;
				}
			}

			updateChildren = TRUE;
			
			if (pList)
			{
				pList->RemoveEntry(*pIncludedBy);
			}
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
			wxFileName fn(wxString(
				pExcludedBy->GetValue().c_str(), wxConvLibc));
			if (fn.SameAs(pTreeNode->GetFullPath()))
			{
				updateChildren = TRUE;
				if (pList)
					pList->RemoveEntry(*pExcludedBy);
				handled = TRUE;
			}
		}
		
		if (!handled)
		{
			wxCharBuffer buf = pTreeNode->GetFullPath().mb_str(wxConvLibc);
			MyExcludeEntry newEntry(
				theExcludeTypes[
					pTreeNode->IsDirectory() 
					? ETI_ALWAYS_INCLUDE_DIR
					: ETI_ALWAYS_INCLUDE_FILE],
				buf.data());
			if (pList)
				pList->AddEntry(newEntry);
			updateChildren = TRUE;
		}

		pUpdateFrom = pTreeNode;
	}
	else if (pLocation != NULL)
	{
		// user wants to exclude a location, or a file not already 
		// excluded within that location. Which is it? For a location
		// root, we prompt the user and delete the whole location.
		// For a subdirectory or file, we add an Exclude directive.
		bool handled = FALSE;

		wxFileName fn(pLocation->GetPath());
		if (fn.SameAs(pTreeNode->GetFullPath()))
		{
			wxString msg;
			msg.Printf(wxT(
				"Are you sure you want to delete "
				"the location %s from the server, "
				"and remove its configuration?"),
				fn.GetFullPath().c_str());

			int result = wxMessageBox(msg, wxT("Boxi Warning"), 
				wxYES_NO | wxICON_EXCLAMATION);

			if (result == wxYES)
			{
				mpConfig->RemoveLocation(*pLocation);
				pUpdateFrom = mpRootNode;
				updateChildren = TRUE;
			}
			handled = TRUE;
		}
		
		if (!handled) 
		{
			wxCharBuffer buf = pTreeNode->GetFullPath()
				.mb_str(wxConvLibc);

			MyExcludeEntry newEntry(
				theExcludeTypes[
					pTreeNode->IsDirectory() 
					? ETI_EXCLUDE_DIR
					: ETI_EXCLUDE_FILE],
				buf.data());

			if (pList)
				pList->AddEntry(newEntry);

			pUpdateFrom = pTreeNode;
			updateChildren = TRUE;
		}
	}
	else
	{
		// outside of any existing location. create a new one.
		wxFileName path(pTreeNode->GetFileName());
		wxString newLocName = path.GetName();
		const std::vector<Location>& rLocs = mpConfig->GetLocations();
		bool foundUnique = FALSE;
		int counter = 1;
		
		while (!foundUnique)
		{
			foundUnique = TRUE;
			
			for (std::vector<Location>::const_iterator i = rLocs.begin();
				i != rLocs.end(); i++)
			{
				if (newLocName.IsSameAs(i->GetName().c_str()))
				{
					foundUnique = FALSE;
					break;
				}
			}
			
			if (foundUnique) break;
				
			// generate a new filename, and try again
			newLocName.Printf(wxT("%s_%d"), 
				path.GetName().c_str(), counter++);
		}
		
		Location newLoc(newLocName, pTreeNode->GetFullPath(), mpConfig);
		mpConfig->AddLocation(newLoc);
		pUpdateFrom = mpRootNode;
		updateChildren = TRUE;
	}
	
	mpConfig->AddListener(this);
	
	if (pUpdateFrom)
		mpTree->UpdateStateIcon(pUpdateFrom, FALSE, updateChildren);
	
	// doesn't work? - FIXME
	// event.Veto();
}

void BackupLocationsPanel::OnClickCloseButton(wxCommandEvent& rEvent)
{
	Hide();
	mpMainFrame->ShowPanel(mpPanelToShowOnClose);
}
