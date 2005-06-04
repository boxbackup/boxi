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
		const wxString& name = "BackupTreeCtrl"
	);
	void UpdateExcludedStateIcon(BackupTreeNode* pNode, bool updateParents);

	private:
	int OnCompareItems(const wxTreeItemId& item1, const wxTreeItemId& item2);

	wxImageList mImages;
	int mEmptyImageId;
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
	wxString        mFullPath;
	BackupTreeNode* mpParentNode;
	BackupTreeCtrl* mpTreeCtrl;
	bool            mIsDirectory;
	wxFileName      mFileName;
	Location*       mpLocation;
	MyExcludeEntry* mpExcludedBy;
	MyExcludeEntry* mpIncludedBy;
	ClientConfig*   mpConfig;
	/*
	LocalServerSettings* mpServerSettings;
	ServerConnection*   mpServerConnection;
	wxTreeCtrl*			mpTreeCtrl;
	LocalFileTreeNode*	mpRootNode;
	wxString			mFileName;
	wxString			mFullPath;
	wxString			mLocationString;
	wxString            mLocationPath;
	wxString			mExcludedByString;
	wxString			mIncludedByString;
	bool				mExcluded;
	int64_t				mBoxFileId;
	wxDateTime          mBoxFileLastModified;
	wxDateTime          mLocalFileLastModified;
	bool				mServerStateKnown;
	bool				mFoundOnServer;
	*/
	
	public:
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
	}
	// ~BackupTreeNode() { }
	
	wxTreeItemId      GetTreeId()   { return mTreeId; }
	const wxFileName& GetFileName() { return mFileName; }
	bool IsDirectory() { return mIsDirectory; }
	void SetTreeId   (wxTreeItemId id)   { mTreeId = id; }
	void SetDirectory(bool value = true) { mIsDirectory = value; }

	void AddChildren();
	void UpdateExcludedState(bool updateParents);
	BackupTreeNode* GetParentNode() { return mpParentNode; }
	Location*       GetLocation()   { return mpLocation; }
	MyExcludeEntry* GetExcludedBy() { return mpExcludedBy; }
	MyExcludeEntry* GetIncludedBy() { return mpIncludedBy; }
	ClientConfig*   GetConfig()     { return mpConfig; }

	/*
	void ShowChildren(wxListCtrl *targetList);
	void SetChecked(bool checked) { this->checked = checked; }
	bool IsChecked() { return this->checked; }
	void UpdateExcludedState(bool updateParents);
	bool GetBoxFileId();
	
	static const int64_t ID_UNKNOWN = -1;
	
	enum ServerState {
		SS_UNKNOWN = 0,
		SS_OUTSIDE,
		SS_EXCLUDED,
		SS_ALIEN,
		SS_MISSING,
		SS_OUTOFDATE,
		SS_TOONEW,
		SS_DIRECTORY,
		SS_PARTIAL,
		SS_UPTODATE,
	};

	ServerState mServerState;
	*/
	
	private:
	BackupTreeCtrl* GetTreeCtrl() { return mpTreeCtrl; }
	wxString GetFullPath() { return mFullPath; }
	void _AddChildrenSlow(bool recurse);
	// bool checked;
};

class BackupLocationsPanel : public wxPanel, public ConfigChangeListener {
	public:
	BackupLocationsPanel(ClientConfig *config,
		wxWindow* parent, wxWindowID id = -1,
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL, 
		const wxString& name = "panel");
	
	private:
	ClientConfig*   mpConfig;
	BackupTreeCtrl* mpTree;
	wxListCtrl 		*theLocationList;
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
	
	void OnBackupLocationClick (wxListEvent& event);
	void OnLocationExcludeClick(wxListEvent& event);
	void OnLocationCmdClick (wxCommandEvent& event);
	void OnLocationChange   (wxCommandEvent& event);
	void OnExcludeTypeChoice(wxCommandEvent& event);
	void OnExcludePathChange(wxCommandEvent& event);
	void OnExcludeCmdClick  (wxCommandEvent& event);
	void OnTreeNodeExpand      (wxTreeEvent& event);
	void OnTreeNodeCollapse    (wxTreeEvent& event);
	void UpdateLocationCtrlEnabledState();
	void UpdateExcludeCtrlEnabledState();
	void PopulateLocationList();
	void PopulateExclusionList();
	void NotifyChange();
	
	long GetSelectedExcludeIndex();
	MyExcludeEntry* GetExcludeEntry();
	const MyExcludeType* GetExcludeTypeByName(const std::string& name);

	DECLARE_EVENT_TABLE()
};

#endif /* _BACKUPLOCATIONSPANEL_H */
