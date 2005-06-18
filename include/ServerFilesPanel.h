/***************************************************************************
 *            ServerFilesPanel.h
 *
 *  Mon Feb 28 22:48:43 2005
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
 
#ifndef _SERVERFILESPANEL_H
#define _SERVERFILESPANEL_H

#include <stdint.h>

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <wx/listimpl.cpp>
#include <wx/datetime.h>

#include "Box.h"
#include "BackupStoreConstants.h"
#include "BackupStoreDirectory.h"
#include "BackupStoreException.h"

#include "ClientConfig.h"
#include "ServerConnection.h"

class ServerSettings {
	public:
	bool				mViewOldFiles;
	bool				mViewDeletedFiles;
};	

class RestoreTreeNode;

class RestoreTreeCtrl : public wxTreeCtrl {
	public:
	RestoreTreeCtrl(
		wxWindow* parent, 
		wxWindowID id, 
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize, 
		long style = wxTR_HAS_BUTTONS, 
		const wxValidator& validator = wxDefaultValidator, 
		const wxString& name = "RestoreTreeCtrl"
	);
	void UpdateExcludedStateIcon(RestoreTreeNode* pNode, 
		bool updateParents, bool updateChildren);
	void SetRoot(RestoreTreeNode* pRootNode);
	
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

class RestoreTreeNode : public wxTreeItemData {
	private:
	int64_t           mBoxFileId;
	wxTreeItemId      mTreeId;
	wxString          mFileName;
	wxString          mFullPath;
	RestoreTreeNode*  mpParentNode;
	RestoreTreeCtrl*  mpTreeCtrl;
	bool              mIsDirectory;
	bool              mIsDeleted;
	ClientConfig*     mpConfig;
	int16_t           mFlags;
	int64_t           mSizeInBlocks;
	bool              mHasAttributes;
	wxDateTime        mDateTime;
	ServerSettings*   mpServerSettings;
	ServerConnection* mpServerConnection;
	BackupProtocolClientAccountUsage* mpUsage;
	
	public:
	RestoreTreeNode(RestoreTreeCtrl* pTreeCtrl, ClientConfig* pConfig,
		ServerSettings* pServerSettings, ServerConnection* pServerConnection) 
	{ 
		mBoxFileId   = BackupProtocolClientListDirectory::RootDirectory;
		mFileName    = "";
		mFullPath    = "/";
		mpParentNode = NULL;
		mpTreeCtrl   = pTreeCtrl;
		mIsDirectory = TRUE;
		mpConfig     = pConfig;
		mFileName    = "";
		mpUsage      = NULL;
		mpServerSettings   = pServerSettings;
		mpServerConnection = pServerConnection;
	}
	RestoreTreeNode(RestoreTreeNode* pParent, int64_t lBoxFileId,
		const wxString& lrFileName, bool lIsDirectory) 
	{ 
		mBoxFileId   = lBoxFileId;
		mFileName    = lrFileName;
		wxString lFullPath;
		lFullPath.Printf("%s/%s", pParent->GetFileName().c_str(), 
			lrFileName.c_str());
		mFullPath    = lFullPath;
		mpParentNode = pParent;
		mpTreeCtrl   = pParent->mpTreeCtrl;
		mIsDirectory = lIsDirectory;
		mpConfig     = pParent->mpConfig;
		mpUsage      = NULL;
		mpServerSettings   = pParent->mpServerSettings;
		mpServerConnection = pParent->mpServerConnection;
	}

	// bool ShowChildren(wxListCtrl *targetList);

	wxTreeItemId      GetTreeId()     const { return mTreeId; }
	const wxString&   GetFileName()   const { return mFileName; }
	const wxString&   GetFullPath()   const { return mFullPath; }
	RestoreTreeNode*  GetParentNode() const { return mpParentNode; }
	int64_t           GetBoxFileId()  const { return mBoxFileId; }
	bool IsDirectory()                const { return mIsDirectory; }
	bool IsDeleted()                  const { return mIsDeleted; }
	void SetTreeId   (wxTreeItemId id)   { mTreeId = id; }
	void SetDirectory(bool value = true) { mIsDirectory = value; }
	void SetDeleted  (bool value = true) { mIsDeleted   = value; }
	
	bool AddChildren(bool recurse);

	private:
	bool _AddChildrenSlow(bool recurse);
};

// declare our list class: this macro declares and partly implements 
// DirEntryPtrList class (which derives from wxListBase)
WX_DECLARE_LIST(RestoreTreeNode, DirEntryPtrList);

class RestorePanel : public wxPanel {
	public:
	RestorePanel(
		ClientConfig*   config,
		ServerConnection* pServerConnection,
		wxWindow* 		parent, 
		wxStatusBar* 	pStatusBar,
		wxWindowID 		id 		= -1,
		const wxPoint& 	pos 	= wxDefaultPosition, 
		const wxSize& 	size 	= wxDefaultSize,
		long 			style 	= wxTAB_TRAVERSAL, 
		const wxString& name 	= "panel");
	
	void RestoreProgress(RestoreState State, std::string& rFileName);
	int  GetListSortColumn () { return mListSortColumn; }
	bool GetListSortReverse() { return mListSortReverse; }
	bool GetViewOldFlag    () { return mServerSettings.mViewOldFiles; }
	bool GetViewDeletedFlag() { return mServerSettings.mViewDeletedFiles; }
	void SetViewOldFlag    (bool NewValue);
	void SetViewDeletedFlag(bool NewValue);
	// void RefreshItem(RestoreTreeNode* pItem);
	
	private:
	ClientConfig*		mpConfig;
	RestoreTreeCtrl*    mpTreeCtrl;
	// wxListView*		theServerFileList;
	wxButton*			mpDeleteButton;
	wxStatusBar*		mpStatusBar;
	int					mRestoreCounter;	
	wxImageList			mImageList;
	int					mListSortColumn;
	bool		  		mListSortReverse;
	RestoreTreeNode* 	mpTreeRoot;
	RestoreTreeNode* 	mpGlobalSelection;
	ServerSettings		mServerSettings;
	ServerConnection*   mpServerConnection;
	BackupProtocolClientAccountUsage* mpUsage;
	
	void OnTreeNodeExpand	(wxTreeEvent& event);
	void OnTreeNodeSelect	(wxTreeEvent& event);
	void OnListColumnClick  (wxListEvent& event);
	void OnListItemActivate (wxListEvent& event);
	void OnFileRestore		(wxCommandEvent& event);
	void OnFileDelete		(wxCommandEvent& event);
	void GetUsageInfo();
	
	DECLARE_EVENT_TABLE()
};

extern const char * ErrorString(Protocol* proto);
extern const char * ErrorString(int type, int subtype);

#endif /* _SERVERFILESPANEL_H */
