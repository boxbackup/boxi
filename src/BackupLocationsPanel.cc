/***************************************************************************
 *            BackupLocationsPanel.cc
 *
 *  Tue Mar  1 00:26:30 2005
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
 */

#include <wx/dynarray.h>
#include <wx/filename.h>
// #include <wx/mstream.h>

#include "BackupLocationsPanel.h"
#include "FileTree.h"
#include "MainFrame.h"
#include "BoxiApp.h"

class BackupTreeNode : public LocalFileNode 
{
	private:
	Location*     mpLocation;
	const MyExcludeEntry* mpExcludedBy;
	const MyExcludeEntry* mpIncludedBy;
	ClientConfig* mpConfig;
	ExcludedState mExcludedState;
	int           mIconId;
	
	public:
	BackupTreeNode(ClientConfig* pConfig,   const wxString& path);
	BackupTreeNode(BackupTreeNode* pParent, const wxString& path);
	virtual LocalFileNode* CreateChildNode(LocalFileNode* pParent, 
		const wxString& rPath);

	Location*               GetLocation()   const { return mpLocation; }
	ClientConfig*           GetConfig()     const { return mpConfig; }
	const MyExcludeEntry*   GetExcludedBy() const { return mpExcludedBy; }
	const MyExcludeEntry*   GetIncludedBy() const { return mpIncludedBy; }

	virtual int UpdateState(FileImageList& rImageList, bool updateParents);
};

BackupTreeNode::BackupTreeNode(ClientConfig* pConfig, const wxString& path)
: LocalFileNode  (path),
  mpLocation     (NULL),
  mpExcludedBy   (NULL),
  mpIncludedBy   (NULL),
  mpConfig       (pConfig),
  mExcludedState (EST_UNKNOWN),
  mIconId        (-1)
{ }

BackupTreeNode::BackupTreeNode(BackupTreeNode* pParent, const wxString& path) 
: LocalFileNode  (pParent, path),
  mpLocation     (pParent->GetLocation()),
  mpExcludedBy   (pParent->GetExcludedBy()),
  mpIncludedBy   (pParent->GetIncludedBy()),
  mpConfig       (pParent->GetConfig()),
  mExcludedState (EST_UNKNOWN),
  mIconId        (-1)
{ }

LocalFileNode* BackupTreeNode::CreateChildNode(LocalFileNode* pParent, 
	const wxString& rPath)
{
	return new BackupTreeNode((BackupTreeNode *)pParent, rPath);
}

int BackupTreeNode::UpdateState(FileImageList& rImageList, bool updateParents) 
{
	int iconId = LocalFileNode::UpdateState(rImageList, updateParents);

	// by default, inherit our include/exclude state
	// from our parent node, if we have one
	ExcludedState ParentState = EST_UNKNOWN;
	
	BackupTreeNode* pParentNode = (BackupTreeNode*)GetParentNode();
	
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
		
	const Location::List& rLocations = mpConfig->GetLocations();

	if (!mpLocation) 
	{
		// determine whether or not this node's path
		// is inside a backup location.
	
		for (Location::ConstIterator pLoc = 
			rLocations.begin();
			pLoc != rLocations.end(); pLoc++)
		{
			const wxString& rPath = pLoc->GetPath();
			// std::cout << "Compare " << mFullPath 
			//	<< " against " << rPath << "\n";
			if (GetFullPath().CompareTo(rPath) == 0) 
			{
				// std::cout << "Found location: " << pLoc->GetName() << "\n";
				mpLocation = mpConfig->GetLocation(*pLoc);
				break;
			}
		}
	}
	
	if (mpLocation && !(GetFullPath().StartsWith(mpLocation->GetPath())))
	{
		wxGetApp().ShowMessageBox(BM_INTERNAL_PATH_MISMATCH,
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
		mExcludedState = mpLocation->GetExcludedState(GetFullPath(), 
			IsDirectory(), &mpExcludedBy, &mpIncludedBy, ParentState);
	}

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
		
		const Location::List& rLocations = mpConfig->GetLocations();
		wxFileName thisNodePath(GetFullPath());
		bool found = false;
		
		for (Location::ConstIterator pLoc = 
			rLocations.begin();
			pLoc != rLocations.end() && !found; pLoc++)
		{
			#ifdef WIN32
			if (IsRoot())
			{
				// root node is always included if there is
				// at least one location, even though its
				// empty path (for My Computer) does not 
				// match in the algorithm below.
				found = true;
			}
			#endif
				
			wxFileName locPath(pLoc->GetPath(), wxT(""));
			
			while(locPath.GetDirCount() > 0 && !found)
			{
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
				
				found = fn1.IsSameAs(fn2);
			}
		}
				
		if (found)
		{
			iconId = rImageList.GetPartialImageId();
		}
	}

	return iconId;
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
	EditorPanel(wxWindow* pParent, ClientConfig *pConfig, wxWindowID id);
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

EditorPanel::EditorPanel(wxWindow* pParent, ClientConfig *pConfig, wxWindowID id)
: wxPanel(pParent, id),
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
	const Location::List& rLocs = mpConfig->GetLocations();
	
	for (Location::ConstIterator pLoc = rLocs.begin();
		pLoc != rLocs.end(); pLoc++)
	{
		const MyExcludeList& rExclude = pLoc->GetExcludeList();
		
		wxString locString;
		locString.Printf(wxT("%s -> %s"), 
			pLoc->GetPath().c_str(), pLoc->GetName().c_str());
		
		const MyExcludeEntry::List& rExcludes = rExclude.GetEntries();
		
		if (rExcludes.size() > 0)
		{
			locString.Append(wxT(" ("));

			int size = rExcludes.size();
			for (MyExcludeEntry::ConstIterator pExclude = rExcludes.begin();
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
		
		if (&(*pLoc) == pSelectedLoc) 
		{
			pTargetList->SetSelection(newIndex);
		}
	}
	
	if (pTargetList->GetSelection() == wxNOT_FOUND && 
		pTargetList->GetCount() > 0)
	{
		pTargetList->SetSelection(0);
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
: EditorPanel(pParent, pConfig, ID_BackupLoc_List_Panel)
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
	PopulateControls();
}

void LocationsPanel::UpdateEnabledState() 
{
	const Location::List& rLocs = mpConfig->GetLocations();

	// Enable the Add and Edit buttons if the current values 
	// don't match any existing entry, and the Remove button 
	// if they do match an existing entry

	bool matchExistingEntry = FALSE;
	
	for (Location::ConstIterator pLocation = rLocs.begin();
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
		wxGetApp().ShowMessageBox(BM_INTERNAL_LOCATION_DOES_NOT_EXIST,
			wxT("Editing nonexistant location!"), wxT("Boxi Error"), 
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
		wxGetApp().ShowMessageBox(BM_INTERNAL_LOCATION_DOES_NOT_EXIST,
			wxT("Removing nonexistant location!"), wxT("Boxi Error"), 
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
			PopulateControls();
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
	const MyExcludeEntry::List* GetSelectedLocEntries();
	
	DECLARE_EVENT_TABLE()
};

ExclusionsPanel::ExclusionsPanel(wxWindow* pParent, ClientConfig *pConfig)
: EditorPanel(pParent, pConfig, ID_BackupLoc_Excludes_Panel)
{
	wxStaticBoxSizer* pLocationListBox = new wxStaticBoxSizer(wxVERTICAL, 
		this, wxT("&Locations"));
	mpTopSizer->Insert(0, pLocationListBox, 0, 
		wxGROW | wxTOP | wxLEFT | wxRIGHT, 8);
	
	mpLocationList = new wxChoice(this, ID_BackupLoc_ExcludeLocList);
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
	mpTypeList->SetSelection(0);
		
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
	
const MyExcludeEntry::List* ExclusionsPanel::GetSelectedLocEntries()
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
	const MyExcludeEntry::List& rEntries = rExcludeList.GetEntries();
	
	for (MyExcludeEntry::ConstIterator pEntry = rEntries.begin();
		pEntry != rEntries.end(); pEntry++)
	{
		wxString exString(pEntry->ToString().c_str(), wxConvLibc);
		int newIndex = mpList->Append(exString);
		mpList->SetClientData(newIndex, rExcludeList.UnConstEntry(*pEntry));
		
		if (&(*pEntry) == pSelectedEntry)
		{
			mpList->SetSelection(newIndex);
		}
	}
	
	if (mpList->GetSelection() == wxNOT_FOUND &&
		mpList->GetCount() > 0)
	{
		mpList->SetSelection(0);
	}

	PopulateControls();
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
	
	const MyExcludeEntry::List* pEntries = GetSelectedLocEntries();
	if (pEntries) 
	{
		for (MyExcludeEntry::ConstIterator pEntry = pEntries->begin();
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
	EVT_CHOICE(ID_BackupLoc_ExcludeLocList,
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
	
	mpTypeList->SetSelection(0);
	mpValueText->SetValue(wxT(""));

	if (selected != wxNOT_FOUND)
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
		wxGetApp().ShowMessageBox(BM_EXCLUDE_NO_TYPE_SELECTED,
			wxT("No type selected!"), wxT("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	MyExcludeEntry newEntry = CreateNewEntry();
	
	Location* pLocation = GetSelectedLocation();
	MyExcludeList& rList = pLocation->GetExcludeList();	
	rList.AddEntry(newEntry);
	// calls listeners, updates list and tree automatically
	
	SelectExclusion(newEntry);
}

void ExclusionsPanel::OnClickButtonEdit(wxCommandEvent &event)
{
	long selected = mpLocationList->GetSelection();
	if (selected == wxNOT_FOUND)
	{
		wxGetApp().ShowMessageBox(BM_INTERNAL_LOCATION_DOES_NOT_EXIST,
			wxT("Editing nonexistant location!"), wxT("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	MyExcludeEntry* pOldEntry = GetSelectedEntry();
	if (!pOldEntry)
	{
		wxGetApp().ShowMessageBox(BM_INTERNAL_EXCLUDE_ENTRY_NOT_SELECTED,
			wxT("No exclude entry selected!"), wxT("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	if (mpTypeList->GetSelection() == wxNOT_FOUND)
	{
		wxGetApp().ShowMessageBox(BM_EXCLUDE_NO_TYPE_SELECTED,
			wxT("No type selected!"), wxT("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	MyExcludeEntry newEntry = CreateNewEntry();
	
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
		wxGetApp().ShowMessageBox(BM_INTERNAL_LOCATION_DOES_NOT_EXIST,
			wxT("Removing nonexistant location!"), wxT("Boxi Error"), 
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
			PopulateControls();
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

#ifdef WIN32
	mpRootNode = new BackupTreeNode(pConfig, wxEmptyString);
	wxString rootLabel = wxT("My Computer");
#else
	mpRootNode = new BackupTreeNode(pConfig, wxT("/"));
	wxString rootLabel = wxT("/ (local root)");
#endif
	
	mpTree = new LocalFileTree(pBasicPanel, ID_Backup_Locations_Tree, 
		mpRootNode, rootLabel);
	pBasicPanelSizer->Add(mpTree, 1, wxGROW | wxALL, 8);
	
	LocationsPanel* pLocationsPanel = new LocationsPanel(pNotebook, mpConfig);
	pNotebook->AddPage(pLocationsPanel, wxT("Locations"));

	ExclusionsPanel* pExclusionsPanel = new ExclusionsPanel(pNotebook, mpConfig);
	pNotebook->AddPage(pExclusionsPanel, wxT("Exclusions"));

	wxSizer* pActionCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	pTopSizer->Add(pActionCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	wxButton* pCloseButton = new wxButton(this, wxID_CANCEL, wxT("Close"));
	pActionCtrlSizer->Add(pCloseButton, 0, wxGROW | wxLEFT, 8);
}

void BackupLocationsPanel::NotifyChange()
{
	UpdateTreeOnConfigChange();
}

void BackupLocationsPanel::UpdateTreeOnConfigChange()
{
	mpTree->UpdateStateIcon(mpRootNode, FALSE, TRUE);
}

void BackupLocationsPanel::OnTreeNodeActivate(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	BackupTreeNode* pTreeNode = (BackupTreeNode *)(mpTree->GetItemData(item));

	#ifdef WIN32
	if (pTreeNode->IsRoot()) return;
	#endif

	Location*             pLocation   = pTreeNode->GetLocation();
	const MyExcludeEntry* pExcludedBy = pTreeNode->GetExcludedBy();
	const MyExcludeEntry* pIncludedBy = pTreeNode->GetIncludedBy();
	MyExcludeList*        pList       = NULL;

	if (pLocation) 
	{
		pList = &(pLocation->GetExcludeList());
	}

	BackupTreeNode* pUpdateFrom = NULL;

	// we will need the location root in several places,
	// any time an exclude entry is added or removed,
	// because all mpExcludedBy/IncludedBy pointers 
	// will be invalidated and must be recalculated.
	BackupTreeNode* pLocationRoot = NULL;
	for (pLocationRoot = pTreeNode; pLocationRoot != NULL;)
	{
		BackupTreeNode* pParentNode = 
			(BackupTreeNode*)pLocationRoot->GetParentNode();
		if (pParentNode == NULL ||
			pParentNode->GetLocation() != pLocation)
		{
			break;
		}
		pLocationRoot = pParentNode;
	}

	wxASSERT(!(pLocation && !pLocationRoot));
	wxASSERT(!(pLocation && !pList));
	wxASSERT(!(pIncludedBy && !pList));
	wxASSERT(!(pExcludedBy && !pList));
	
	// avoid updating whole tree
	mpConfig->RemoveListener(this);
	
	if (pIncludedBy != NULL)
	{
		// Tricky case. User wants to exclude an AlwaysIncluded item.
		// We can't override it with anything, and unless this item is 
		// the only match of the AlwaysInclude directive, we can't 
		// remove it without affecting other items. The best we can do is 
		// to warn the user, ask them whether to remove it.

		bool doDelete = false;
		bool warnUser = true;
		
		if (pIncludedBy->GetMatch() == EM_EXACT)
		{
			wxFileName fn(wxString(
				pIncludedBy->GetValue().c_str(), wxConvLibc));
			if (fn.SameAs(pTreeNode->GetFullPath()))
			{
				doDelete = true;
				warnUser = false;
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
			
			int result = wxGetApp().ShowMessageBox
			(
				BM_EXCLUDE_MUST_REMOVE_ALWAYSINCLUDE,
				msg, wxT("Boxi Warning"), 
				wxYES_NO | wxICON_EXCLAMATION, NULL
			);

			if (result == wxYES)
			{
				doDelete = true;
			}
		}
		
		if (doDelete) 
		{
			if (pList)
			{
				pList->RemoveEntry(*pIncludedBy);
				pUpdateFrom = pLocationRoot;
			}
		}			
	} 
	else if	(pExcludedBy != NULL)
	{
		// Node is excluded, and user wants to include it.
		// If the selected node is the value of an exact matching
		// Exclude(File|Dir) entry, we can delete that entry.
		// Otherwise, we will have to add an AlwaysInclude entry for it.
		
		bool handled = false;
		
		if (pExcludedBy->GetMatch() == EM_EXACT)
		{
			wxFileName fn(wxString(
				pExcludedBy->GetValue().c_str(), wxConvLibc));
			if (fn.SameAs(pTreeNode->GetFullPath()))
			{
				if (pList)
				{
					pList->RemoveEntry(*pExcludedBy);
				}
				handled = true;
			}
		}
		
		if (!handled)
		{
			MyExcludeEntry newEntry(
				theExcludeTypes[
					pTreeNode->IsDirectory() 
					? ETI_ALWAYS_INCLUDE_DIR
					: ETI_ALWAYS_INCLUDE_FILE],
				pTreeNode->GetFullPath());
			
			if (pList)
			{
				pList->AddEntry(newEntry);
			}
		}

		pUpdateFrom = pLocationRoot;
	}
	else if (pLocation != NULL)
	{
		// User wants to exclude a location, or a file not already 
		// excluded within that location. Which is it? For a location
		// root, we prompt the user and delete the whole location.
		// For a subdirectory or file, we add an Exclude directive.
		bool handled = false;

		wxFileName fn(pLocation->GetPath());
		if (fn.SameAs(pTreeNode->GetFullPath()))
		{
			wxString msg;
			msg.Printf(wxT(
				"Are you sure you want to delete "
				"the location %s from the server, "
				"and remove its configuration?"),
				fn.GetFullPath().c_str());

			int result = wxGetApp().ShowMessageBox(
				BM_BACKUP_FILES_DELETE_LOCATION_QUESTION, msg, 
				wxT("Boxi Warning"), 
				wxYES_NO | wxICON_EXCLAMATION, this);

			if (result == wxYES)
			{
				mpConfig->RemoveLocation(*pLocation);
				pUpdateFrom = mpRootNode;
			}
			handled = true;
		}
		
		if (!handled) 
		{
			MyExcludeEntry newEntry(
				theExcludeTypes[
					pTreeNode->IsDirectory() 
					? ETI_EXCLUDE_DIR
					: ETI_EXCLUDE_FILE],
				pTreeNode->GetFullPath());

			if (pList)
			{
				pList->AddEntry(newEntry);
				pUpdateFrom = pLocationRoot;
			}
		}
	}
	else
	{
		// outside of any existing location. create a new one.
		wxFileName path(pTreeNode->GetFileName());
		
		wxString newLocName = path.GetName();
		if (newLocName.IsSameAs(wxEmptyString))
		{
			newLocName = _("root");
		}
		
		const Location::List& rLocs = mpConfig->GetLocations();
		bool foundUnique = false;
		int counter = 1;
		
		while (!foundUnique)
		{
			foundUnique = true;
			
			for (Location::ConstIterator i = rLocs.begin();
				i != rLocs.end(); i++)
			{
				if (newLocName.IsSameAs(i->GetName().c_str()))
				{
					foundUnique = false;
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
	}
	
	mpConfig->AddListener(this);
	
	if (pUpdateFrom)
	{
		mpTree->UpdateStateIcon(pUpdateFrom, false, true);
	}
	
	// doesn't work? - FIXME
	// event.Veto();
	event.Skip();
}

void BackupLocationsPanel::OnClickCloseButton(wxCommandEvent& rEvent)
{
	Hide();
	mpMainFrame->ShowPanel(mpPanelToShowOnClose);
}
