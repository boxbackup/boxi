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
 
#include "BackupLocationsPanel.h"

BEGIN_EVENT_TABLE(BackupLocationsPanel, wxPanel)
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
	theConfig = config;
	theConfig->AddListener(this);
	
	wxBoxSizer *topSizer = new wxBoxSizer( wxVERTICAL );
	
	theLocationList = new wxListCtrl(
		this, ID_Backup_LocationsList,
		wxDefaultPosition, wxDefaultSize, 
		wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_VRULES | wxLC_HRULES);
	topSizer->Add(theLocationList, 1, 
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
	topSizer->Add(theLocationCmdSizer, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	theLocationAddBtn = new wxButton(this, ID_Backup_LocationsAddButton, "Add", 
		wxDefaultPosition);
	theLocationCmdSizer->Add(theLocationAddBtn, 1, wxGROW, 0);
	theLocationAddBtn->Disable();
	
	theLocationEditBtn = new wxButton(this, ID_Backup_LocationsEditButton, "Edit", 
		wxDefaultPosition);
	theLocationCmdSizer->Add(theLocationEditBtn, 1, wxGROW, 0);
	theLocationEditBtn->Disable();
	
	theLocationRemoveBtn = new wxButton(this, ID_Backup_LocationsDelButton, "Remove", 
		wxDefaultPosition);
	theLocationCmdSizer->Add(theLocationRemoveBtn, 1, wxGROW, 0);
	theLocationRemoveBtn->Disable();

	wxGridSizer *theLocParamSizer = new wxGridSizer(2, 4, 4);
	topSizer->Add(theLocParamSizer, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	theLocationNameCtrl = new wxTextCtrl(this, ID_Backup_LocationNameCtrl, "");
	AddParam(this, "Location Name:", theLocationNameCtrl, true, theLocParamSizer);
		
	theLocationPathCtrl = new wxTextCtrl(this, ID_Backup_LocationPathCtrl, "");
	AddParam(this, "Location Path:", theLocationPathCtrl, true, theLocParamSizer);
		
	theExclusionList = new wxListCtrl(
		this, ID_BackupLoc_ExcludeList,
		wxDefaultPosition, wxDefaultSize, 
		wxLC_REPORT | wxLC_VRULES | wxLC_HRULES);
	topSizer->Add(theExclusionList, 1, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	itemCol.m_text = _T("Type");
	theExclusionList->InsertColumn(0, itemCol);
	itemCol.m_text = _T("Path or File");
	theExclusionList->InsertColumn(1, itemCol);

	for (int i = 0; i < theExclusionList->GetColumnCount(); i++) 
    	theExclusionList->SetColumnWidth( i, wxLIST_AUTOSIZE_USEHEADER );
	
	wxGridSizer *theExcludeParamSizer = new wxGridSizer(2, 4, 4);
	topSizer->Add(theExcludeParamSizer, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	wxString excludeTypes[sizeof(theExcludeTypes) / sizeof(theExcludeTypes[0])];
	for (size_t i = 0; i < sizeof(theExcludeTypes) / sizeof(theExcludeTypes[0]); i++) {
		excludeTypes[i] = wxString(theExcludeTypes[i].ToString().c_str());
	}
	
	theExcludeTypeCtrl = new wxChoice(this, ID_BackupLoc_ExcludeTypeList, 
		wxDefaultPosition, wxDefaultSize, 8, excludeTypes);
	AddParam(this, "Exclude Type:", theExcludeTypeCtrl, true, 
		theExcludeParamSizer);
		
	theExcludePathCtrl = new wxTextCtrl(this, ID_BackupLoc_ExcludePathCtrl, "");
	AddParam(this, "Exclude Path:", theExcludePathCtrl, true, 
		theExcludeParamSizer);

	wxSizer *theExcludeCmdSizer = new wxGridSizer( 1, 0, 4, 4 );
	topSizer->Add(theExcludeCmdSizer, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	theExcludeAddBtn = new wxButton(this, ID_BackupLoc_ExcludeAddButton, 
		"Add", wxDefaultPosition);
	theExcludeAddBtn->Disable();
	theExcludeCmdSizer->Add(theExcludeAddBtn, 1, wxGROW, 0);
	
	theExcludeEditBtn = new wxButton(this, ID_BackupLoc_ExcludeEditButton, 
		"Edit", wxDefaultPosition);
	theExcludeEditBtn->Disable();
	theExcludeCmdSizer->Add(theExcludeEditBtn, 1, wxGROW, 0);

	theExcludeRemoveBtn = new wxButton(this, ID_BackupLoc_ExcludeRemoveButton, 
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
	Location* pLoc = theConfig->GetLocations()[theSelectedLocation];
	theLocationNameCtrl->SetValue(pLoc->GetName());
	theLocationPathCtrl->SetValue(pLoc->GetPath());
	theConfig->NotifyListeners();
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
	const std::vector<Location*>& rLocs = theConfig->GetLocations();
	
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
			theConfig->RemoveListener(this);
			theLocationList->SetItemState(item, wxLIST_STATE_SELECTED,
				wxLIST_STATE_SELECTED);
			theConfig->AddListener(this);
		}
	}

	for (int i = 0; i < theLocationList->GetColumnCount(); i++) 
    	theLocationList->SetColumnWidth( i, wxLIST_AUTOSIZE );
}

void BackupLocationsPanel::PopulateExclusionList() {
	if (theSelectedLocation >= theConfig->GetLocations().size())
		return;
	
	theExclusionList->DeleteAllItems();

	Location* pLoc = theConfig->GetLocations()[theSelectedLocation];
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
	Location* pLoc = theConfig->GetLocations()[theSelectedLocation];
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
	const std::vector<Location*>& rLocs = theConfig->GetLocations();

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
	if (theSelectedLocation >= theConfig->GetLocations().size())
		return;

	wxChoice* 		pChoice 	= theExcludeTypeCtrl;
	wxTextCtrl*		pTextCtrl 	= theExcludePathCtrl;
	Location* 		pLoc 		= theConfig->GetLocations()[theSelectedLocation];
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
	Location* pLoc = theConfig->GetLocations()[theSelectedLocation];
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

	theConfig->NotifyListeners();
}

void BackupLocationsPanel::OnLocationCmdClick(wxCommandEvent &event)
{
	int sourceId = event.GetId();
	Location* pCurrentLoc = theConfig->GetLocations()[theSelectedLocation];
	
	if (sourceId == ID_Backup_LocationsAddButton) 
	{
		Location* pNewLocation = new Location(theLocationNameCtrl->GetValue(),
			theLocationPathCtrl->GetValue());
		MyExcludeList* pExcludeList = new MyExcludeList(
			pCurrentLoc->GetExcludeList());
		pNewLocation->SetExcludeList(pExcludeList);
		theConfig->AddLocation(pNewLocation);
	}
	else if (sourceId == ID_Backup_LocationsEditButton)
	{
		pCurrentLoc->SetName(theLocationNameCtrl->GetValue().c_str());
		pCurrentLoc->SetPath(theLocationPathCtrl->GetValue().c_str());
	}
	else if (sourceId == ID_Backup_LocationsDelButton)
	{
		theConfig->RemoveLocation(theSelectedLocation);
	}
	
	theConfig->NotifyListeners();
}

void BackupLocationsPanel::NotifyChange()
{
	PopulateLocationList();
	PopulateExclusionList();
	UpdateExcludeCtrlEnabledState();
}
