/***************************************************************************
 *            BackupLocationsPanel.h
 *
 *  Tue Mar  1 00:26:35 2005
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
 
#ifndef _BACKUPLOCATIONSPANEL_H
#define _BACKUPLOCATIONSPANEL_H

#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>

#include "ClientConfig.h"

class BackupTreeNode;

class BackupTreeCtrl : public wxTreeCtrl {
	public:
	BackupTreeCtrl(
		ClientConfig* pConfig,
		wxWindow* parent, 
		wxWindowID id, 
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize, 
		long style = wxTR_HAS_BUTTONS, 
		const wxValidator& validator = wxDefaultValidator, 
		const wxString& name = wxT("BackupTreeCtrl")
	);
	void UpdateExcludedStateIcon(BackupTreeNode* pNode, 
		bool updateParents, bool updateChildren);
	BackupTreeNode* GetRootNode() { return mpRootNode; }
	
	private:
	int OnCompareItems(const wxTreeItemId& item1, const wxTreeItemId& item2);

	BackupTreeNode *mpRootNode;

	wxImageList mImages;
	int mEmptyImageId;
	int mPartialImageId;
	int mCheckedImageId;
	int mCheckedGreyImageId;
	int mCrossedImageId;
	int mCrossedGreyImageId;
	int mAlwaysImageId;
	int mAlwaysGreyImageId;

};

class BackupTreeNode : public wxTreeItemData {
	private:
	wxTreeItemId    mTreeId;
	wxFileName      mFileName;
	wxString        mFullPath;
	BackupTreeNode* mpParentNode;
	BackupTreeCtrl* mpTreeCtrl;
	bool            mIsDirectory;
	Location*       mpLocation;
	const MyExcludeEntry* mpExcludedBy;
	const MyExcludeEntry* mpIncludedBy;
	ClientConfig*   mpConfig;
	ExcludedState   mExcludedState;
	
	public:
	int             mIconId;
	
	BackupTreeNode(BackupTreeCtrl* pTreeCtrl, ClientConfig* pConfig, 
		const wxString& path
	) { 
		mpParentNode = NULL;
		mpTreeCtrl   = pTreeCtrl;
		mFullPath    = path;
		mFileName    = wxFileName(path);
		mIsDirectory = wxFileName::DirExists(mFullPath);
		mpLocation   = NULL;
		mpExcludedBy = NULL;
		mpIncludedBy = NULL;
		mpConfig     = pConfig;
		mIconId      = -1;
		mExcludedState = EST_UNKNOWN;
	}
	BackupTreeNode(BackupTreeNode* pParent, const wxString& path) { 
		mpParentNode = pParent;
		mpTreeCtrl   = pParent->GetTreeCtrl(); 
		mFullPath    = path;
		mFileName    = wxFileName(path);
		mIsDirectory = wxFileName::DirExists(mFullPath);
		mpLocation   = pParent->GetLocation();
		mpExcludedBy = pParent->GetExcludedBy();
		mpIncludedBy = pParent->GetIncludedBy();
		mpConfig     = pParent->GetConfig();
		mIconId      = -1;
		mExcludedState = EST_UNKNOWN;
	}
	
	wxTreeItemId      GetTreeId()   { return mTreeId; }
	const wxFileName& GetFileName() { return mFileName; }
	const wxString&   GetFullPath() { return mFullPath; }
	bool IsDirectory() { return mIsDirectory; }
	void SetTreeId   (wxTreeItemId id)   { mTreeId = id; }
	void SetDirectory(bool value = true) { mIsDirectory = value; }

	void AddChildren();
	void UpdateExcludedState(bool updateParents);
	BackupTreeNode* GetParentNode() { return mpParentNode; }
	Location*       GetLocation()   { return mpLocation; }
	ClientConfig*   GetConfig()     { return mpConfig; }

	const MyExcludeEntry* GetExcludedBy() { return mpExcludedBy; }
	const MyExcludeEntry* GetIncludedBy() { return mpIncludedBy; }

	private:
	BackupTreeCtrl* GetTreeCtrl() { return mpTreeCtrl; }
	void _AddChildrenSlow(bool recurse);
};

class LocationsPanel;

class BackupLocationsPanel : public wxPanel, public ConfigChangeListener {
	public:
	BackupLocationsPanel(ClientConfig *config,
		wxWindow* parent, wxWindowID id = -1,
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL, 
		const wxString& name = wxT("BackupLocationsPanel"));
	
	private:
	ClientConfig*   mpConfig;
	BackupTreeCtrl* mpTree;
	// LocationsPanel* mpLocationsPanel;
	/*
	wxListBox*      mpLocationPanelLocList;
	wxButton 		*theLocationAddBtn;
	wxButton 		*theLocationEditBtn;
	wxButton 		*theLocationRemoveBtn;
	wxTextCtrl		*theLocationNameCtrl;
	wxTextCtrl		*theLocationPathCtrl;
	wxListCtrl 		*theExclusionList;
	wxChoice		*theExcludeTypeCtrl;
	wxTextCtrl		*theExcludePathCtrl;
	wxButton 		*theExcludeAddBtn;
	wxButton 		*theExcludeEditBtn;
	wxButton 		*theExcludeRemoveBtn;
	unsigned int theSelectedLocation;
	
	void OnBackupLocationClick (wxListEvent&    rEvent);
	void OnLocationExcludeClick(wxListEvent&    rEvent);
	void OnLocationCmdClick    (wxCommandEvent& rEvent);
	void OnLocationChange      (wxCommandEvent& rEvent);
	void OnExcludeTypeChoice   (wxCommandEvent& rEvent);
	void OnExcludePathChange   (wxCommandEvent& rEvent);
	void OnExcludeCmdClick     (wxCommandEvent& rEvent);
	*/

	void OnClickCloseButton    (wxCommandEvent& rEvent);
	
	void OnTreeNodeExpand      (wxTreeEvent&    rEvent);
	void OnTreeNodeCollapse    (wxTreeEvent&    rEvent);
	void OnTreeNodeActivate    (wxTreeEvent&    rEvent);

	void NotifyChange(); // ConfigChangeListener interface
	void UpdateTreeOnConfigChange();
	
	/*
	void UpdateLocationCtrlEnabledState();
	void UpdateExcludeCtrlEnabledState();
	void PopulateLocationList();
	void PopulateExclusionList();
	
	void UpdateAdvancedTabOnConfigChange();
	
	long GetSelectedExcludeIndex();
	MyExcludeEntry* GetExcludeEntry();
	const MyExcludeType* GetExcludeTypeByName(const std::string& name);
	*/
	
	DECLARE_EVENT_TABLE()
};

#endif /* _BACKUPLOCATIONSPANEL_H */
