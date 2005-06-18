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

class ServerCacheNode 
{
	public:
	typedef std::vector<ServerCacheNode*> Vector;
	typedef Vector::iterator Iterator;

	private:
	int64_t           mBoxFileId;
	wxString          mFileName;
	wxString          mFullPath;
	ServerCacheNode*  mpParentNode;
	bool              mIsDirectory;
	bool              mIsDeleted;
	ClientConfig*     mpConfig;
	int16_t           mFlags;
	int64_t           mSizeInBlocks;
	bool              mHasAttributes;
	wxDateTime        mDateTime;
	ServerConnection* mpServerConnection;
	Vector            mChildren;
	bool              mCached;
	
	public:
	ServerCacheNode(ClientConfig* pConfig,
		ServerConnection* pServerConnection) 
	{ 
		mBoxFileId   = BackupProtocolClientListDirectory::RootDirectory;
		mFileName    = "";
		mFullPath    = "/";
		mpParentNode = NULL;
		mIsDirectory = TRUE;
		mIsDeleted   = FALSE;
		mpConfig     = pConfig;
		mFlags       = 0;
		mSizeInBlocks = 0;
		mHasAttributes = FALSE;
		mpServerConnection = pServerConnection;
		mCached = FALSE;
	}
	ServerCacheNode(ServerCacheNode* pParent, 
		BackupStoreDirectory::Entry* pDirEntry)
	{ 
		BackupStoreFilenameClear clear(pDirEntry->GetName());

		mBoxFileId   = pDirEntry->GetObjectID();
		mFileName    = clear.GetClearFilename().c_str();	
		mFullPath.Printf("%s/%s", pParent->GetFileName().c_str(), 
			mFileName.c_str());
		mpParentNode = pParent;
		mFlags       = pDirEntry->GetFlags();
		mIsDirectory = mFlags & BackupStoreDirectory::Entry::Flags_Dir;
		mIsDeleted   = mFlags & BackupStoreDirectory::Entry::Flags_Deleted;
		mpConfig     = pParent->mpConfig;
		mSizeInBlocks = 0;
		mHasAttributes = FALSE;
		mpServerConnection = pParent->mpServerConnection;
		mCached = FALSE;
	}
	~ServerCacheNode() { FreeChildren(); }

	const wxString&   GetFileName()   const { return mFileName; }
	const wxString&   GetFullPath()   const { return mFullPath; }
	ServerCacheNode*  GetParentNode() const { return mpParentNode; }
	int64_t           GetBoxFileId()  const { return mBoxFileId; }
	bool IsDirectory()                const { return mIsDirectory; }
	bool IsDeleted()                  const { return mIsDeleted; }
	void SetDeleted(bool value = true) { mIsDeleted   = value; }

	const Vector& GetChildren();
	
	private:
	void FreeChildren() 
	{
		for (Iterator i = mChildren.begin(); i != mChildren.end(); i++)
		{
			delete *i;
		}
		mChildren.clear();
	}		
};

class ServerCache
{
	private:
	ServerCacheNode mRoot;
	
	public:
	ServerCache(ClientConfig* pConfig, ServerConnection* pServerConnection) 
	: mRoot(pConfig, pServerConnection)
	{ }
	
	ServerCacheNode* GetRoot() { return &mRoot; }
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
	wxTreeItemId      mTreeId;
	ServerCacheNode*  mpCacheNode;
	RestoreTreeNode*  mpParentNode;
	ServerSettings*   mpServerSettings;
	RestoreTreeCtrl*  mpTreeCtrl;
	
	public:
	RestoreTreeNode(RestoreTreeCtrl* pTreeCtrl, ServerCacheNode* pCacheNode,
		ServerSettings* pServerSettings) 
	{ 
		mpCacheNode      = pCacheNode;
		mpParentNode     = NULL;
		mpTreeCtrl       = pTreeCtrl;
		mpServerSettings = pServerSettings;
	}
	RestoreTreeNode(RestoreTreeNode* pParent, ServerCacheNode* pCacheNode)
	{ 
		mpCacheNode      = pCacheNode;
		mpParentNode     = pParent;
		mpTreeCtrl       = pParent->mpTreeCtrl;
		mpServerSettings = pParent->mpServerSettings;
	}

	// bool ShowChildren(wxListCtrl *targetList);

	wxTreeItemId      GetTreeId()     const { return mTreeId; }
	RestoreTreeNode*  GetParentNode() const { return mpParentNode; }
	const wxString&   GetFileName()   const { return mpCacheNode->GetFileName(); }
	const wxString&   GetFullPath()   const { return mpCacheNode->GetFullPath(); }
	int64_t           GetBoxFileId()  const { return mpCacheNode->GetBoxFileId(); }
	bool IsDirectory()                const { return mpCacheNode->IsDirectory(); }
	bool IsDeleted()                  const { return mpCacheNode->IsDeleted(); }
	void SetTreeId   (wxTreeItemId id)   { mTreeId = id; }
	void SetDeleted  (bool value = true) { mpCacheNode->SetDeleted(value); }
	
	bool AddChildren(bool recurse);

	private:
	bool _AddChildrenSlow(bool recurse);
};

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
		const wxString& name 	= "RestorePanel");
	~RestorePanel() { delete mpCache; }
	
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
	ServerCache*        mpCache;
	
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
