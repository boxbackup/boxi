/***************************************************************************
 *            BackupFilesPanel.h
 *
 *  Tue Mar  1 00:24:11 2005
 *  Copyright 2005-2008 Chris Wilson
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
 
#ifndef _BACKUPFILESPANEL_H
#define _BACKUPFILESPANEL_H

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <wx/datetime.h>
#include <wx/imaglist.h>
 
#include "ClientConfig.h"
#include "ServerConnection.h"

/**
  * LocalServerSettings 
  * Contains shared information about whether the user has asked us
  * to connect to the server on demand. Accessible from and shared
  * between LocalFileTreeNode and BackupFilesPanel.
  */

class LocalServerSettings {
	public:
	bool mConnectOnDemand;
};

/**
  * BackupFilesPanel
  * Shows the files on the local machine (Local Files tab in the GUI)
  * and their corresponding status on the server.
  */

class BackupFilesPanel : public wxPanel {
	public:
	BackupFilesPanel(
		ClientConfig *config,
		ServerConnection* pConnection,
		wxWindow* parent, wxWindowID id = -1,
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL, 
		const wxString& name = "Files Panel");
	~BackupFilesPanel();
	wxTreeCtrl*   GetFileTree()       { return theBackupFileTree; }
	wxTreeItemId& GetTreeRootItemId() { return theTreeRootItem; }
	
	private:
	ClientConfig*   theConfig;
	LocalServerSettings* mpServerSettings;
	ServerConnection* mpServerConnection;
	wxSizer*        mpMainSizer;
	wxTreeCtrl*     theBackupFileTree;
	wxTreeItemId    theTreeRootItem;
	// wxImage         mUncheckedImage;
	// wxImage         mCheckedImage;
	wxImageList     mTreeImages;
	wxSizer*        mpBottomInfoSizer;
	wxStaticText*   mpFullPathLabel;
	wxStaticText*   mpLocationLabel;
	wxStaticText*   mpExcludedByLabel;
	wxStaticText*   mpIncludedByLabel;
	wxStaticText*   mpExcludeResultLabel;
	wxCheckBox*     mpServerConnectCheckbox;
	wxStaticText*   mpLocalVersionLabel;
	wxStaticText*   mpRemoteVersionLabel;
	wxStaticText*   mpServerStatus;
	
	int mCheckedImageId;
	int mCrossedImageId;
	int mUnknownImageId;
	int mWaitingImageId;
	int mEqualImageId;
	int mNotEqualImageId;
	int mSameTimeImageId;
	int mOldFileImageId;
	int mExclamImageId;
	int mLinkedImageId;
	int mFolderImageId;
	int mPartialImageId;
	int mAlienImageId;

	void OnTreeNodeExpand    (wxTreeEvent& event);
	void OnTreeNodeCollapse  (wxTreeEvent& event);
	void OnTreeNodeActivated (wxTreeEvent& event);
	void OnTreeNodeSelect    (wxTreeEvent& event);
	void OnServerConnectClick(wxCommandEvent& event);
	
	void UpdateExcludedState(wxTreeItemId &rNodeId);
	void UpdateExcludedStatePrivate(wxTreeItemId &rNodeId);
	
	class MyConfigChangeListener : public ConfigChangeListener {
		public:
		MyConfigChangeListener(BackupFilesPanel* pPanel) { mpPanel = pPanel; }
		void NotifyChange();
		private:
		BackupFilesPanel* mpPanel;
	} * mpConfigListener;
	
	DECLARE_EVENT_TABLE()
};

#endif /* _BACKUPFILESPANEL_H */
