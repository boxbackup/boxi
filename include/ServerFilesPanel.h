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

#include <sys/types.h>

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <wx/listimpl.cpp>
#include <wx/datetime.h>

#define NDEBUG
#include "Box.h"
#include "BackupStoreConstants.h"
#include "BackupStoreDirectory.h"
#include "BackupStoreException.h"
#undef NDEBUG

#include "ClientConfig.h"
#include "ServerConnection.h"

class ServerSettings {
	public:
	bool				mViewOldFiles;
	bool				mViewDeletedFiles;
};

class ServerFileVersion 
{
	public:
	typedef std::vector<ServerFileVersion*> Vector;
	typedef Vector::iterator Iterator;
	
	private:
	int64_t            mBoxFileId;
	bool               mIsDirectory;
	bool               mIsDeleted;
	ClientConfig*      mpConfig;
	int16_t            mFlags;
	int64_t            mSizeInBlocks;
	bool               mHasAttributes;
	wxDateTime         mDateTime;
	bool               mCached;
	
	public:
	ServerFileVersion() 
	{
		mBoxFileId   = BackupProtocolClientListDirectory::RootDirectory;
		mIsDirectory = TRUE;
		mIsDeleted   = FALSE;
		mFlags       = 0;
		mSizeInBlocks = 0;
		mHasAttributes = FALSE;
	}
	ServerFileVersion(BackupStoreDirectory::Entry* pDirEntry);

	const wxDateTime& GetDateTime()   const { return mDateTime; }
	int64_t           GetBoxFileId()  const { return mBoxFileId; }
	int64_t           GetSizeBlocks() const { return mSizeInBlocks; }
	bool IsDirectory()                const { return mIsDirectory; }
	bool IsDeleted()                  const { return mIsDeleted; }
	void SetDeleted(bool value = true) { mIsDeleted   = value; }
	private:
};

class ServerCacheNode 
{
	public:
	typedef std::vector<ServerCacheNode*> Vector;
	typedef Vector::iterator Iterator;

	private:
	wxString                  mFileName;
	wxString                  mFullPath;
	ServerFileVersion::Vector mVersions;
	ServerFileVersion*        mpMostRecent;
	ServerCacheNode::Vector   mChildren;
	ServerCacheNode*          mpParentNode;
	bool                      mCached;
	ServerConnection*         mpServerConnection;
	wxMutex                   mMutex;
	
	public:
	ServerCacheNode(ServerConnection* pServerConnection) 
	{
		ServerFileVersion* pDummyRootVersion = 
			new ServerFileVersion();
		mVersions.push_back(pDummyRootVersion);
		mFileName    = wxT("");
		mFullPath    = wxT("/");
		mpParentNode = NULL;
		mpMostRecent = NULL;
		mCached      = FALSE;
		mpServerConnection = pServerConnection;
	}
	ServerCacheNode(ServerCacheNode* pParent, 
		BackupStoreDirectory::Entry* pDirEntry)
	{ 
		mFileName    = GetDecryptedName(pDirEntry);
		mFullPath.Printf(wxT("%s/%s"), 
			pParent->GetFileName().c_str(), 
			mFileName.c_str());
		mpParentNode = pParent;
		mpMostRecent = NULL;
		mCached      = FALSE;
		mpServerConnection = pParent->mpServerConnection;
	}
	~ServerCacheNode() { 
		wxMutexLocker lock(mMutex);
		FreeVersions(); 
		FreeChildren(); 
	}

	wxString GetDecryptedName(BackupStoreDirectory::Entry* pDirEntry)
	{
		BackupStoreFilenameClear clear(pDirEntry->GetName());
		wxString name(clear.GetClearFilename().c_str(), wxConvLibc);
		return name;
	}
		
	bool               IsRoot()           const { 
		return mFileName.CompareTo(wxT("")); 
	}
	const wxString&    GetFileName()      const { return mFileName; }
	const wxString&    GetFullPath()      const { return mFullPath; }
	ServerCacheNode*   GetParent()        const { return mpParentNode; }
	const ServerFileVersion::Vector& GetVersions();
	const ServerCacheNode::Vector&   GetChildren();
	wxMutex&           GetLock() { return mMutex; }
	
	ServerFileVersion* GetMostRecent()
	{
		wxMutexLocker lock(mMutex);
		
		if (mpMostRecent) 
			return mpMostRecent;
		
		for (ServerFileVersion::Vector::const_iterator i = mVersions.begin(); 
			i != mVersions.end(); i++)
		{
			if (mpMostRecent == NULL ||
				mpMostRecent->GetDateTime().IsEarlierThan((*i)->GetDateTime()))
			{
				mpMostRecent = *i;
			}
		}
		
		return mpMostRecent;
	}
	
	private:
	// Only call these with a lock held!
	void FreeVersions() 
	{
		for (ServerFileVersion::Iterator i = mVersions.begin(); 
			i != mVersions.end(); i++)
		{
			delete *i;
		}
		mVersions.clear();
	}		
	void FreeChildren() 
	{
		for (ServerCacheNode::Iterator i = mChildren.begin(); 
			i != mChildren.end(); i++)
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
	ServerCache(ServerConnection* pServerConnection) 
	: mRoot(pServerConnection)
	{ }
	
	ServerCacheNode* GetRoot() { return &mRoot; }
};

class RestoreSpecEntry
{
	private:
	ServerCacheNode* mpNode;
	bool mInclude;

	public:
	RestoreSpecEntry(ServerCacheNode* pNode, bool lInclude)
	{
		mpNode = pNode;
		mInclude = lInclude;
	}
	
	ServerCacheNode* GetNode() const { return mpNode; }
	bool IsInclude() const { return mInclude; }
	bool IsSameAs(const RestoreSpecEntry& rOther) const
	{
		return mpNode == rOther.mpNode &&
			mInclude == rOther.mInclude;
	}
};

class RestoreSpec
{
	public:
	typedef std::vector<RestoreSpecEntry> Vector;
	typedef Vector::iterator Iterator;
		
	private:
	Vector mEntries;
	
	public:
	RestoreSpec() { }
	const Vector& GetEntries() const { return mEntries; }
	void Add   (const RestoreSpecEntry& rNewEntry);
	void Remove(const RestoreSpecEntry& rOldEntry);
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
		const wxString& name = wxT("RestoreTreeCtrl")
	);
	void UpdateStateIcon(RestoreTreeNode* pNode, 
		bool updateParents, bool updateChildren);
	void SetRoot(RestoreTreeNode* pRootNode);
	
	private:
	int OnCompareItems(const wxTreeItemId& item1, 
			const wxTreeItemId& item2);

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
	wxTreeItemId       mTreeId;
	ServerCacheNode*   mpCacheNode;
	RestoreTreeNode*   mpParentNode;
	ServerSettings*    mpServerSettings;
	RestoreTreeCtrl*   mpTreeCtrl;
	const RestoreSpec& mrRestoreSpec;
	const RestoreSpecEntry* mpMatchingEntry;
	bool               mIncluded;
	
	public:
	RestoreTreeNode(RestoreTreeCtrl* pTreeCtrl, ServerCacheNode* pCacheNode,
		ServerSettings* pServerSettings, const RestoreSpec& rRestoreSpec) 
	: mrRestoreSpec(rRestoreSpec)
	{ 
		mpCacheNode      = pCacheNode;
		mpParentNode     = NULL;
		mpTreeCtrl       = pTreeCtrl;
		mpServerSettings = pServerSettings;
		mpMatchingEntry  = NULL;
		mIncluded        = FALSE;
	}
	RestoreTreeNode(RestoreTreeNode* pParent, ServerCacheNode* pCacheNode)
	: mrRestoreSpec(pParent->mrRestoreSpec)
	{ 
		mpCacheNode      = pCacheNode;
		mpParentNode     = pParent;
		mpTreeCtrl       = pParent->mpTreeCtrl;
		mpServerSettings = pParent->mpServerSettings;
		mpMatchingEntry  = NULL;
		mIncluded        = FALSE;
	}

	// bool ShowChildren(wxListCtrl *targetList);

	wxTreeItemId      GetTreeId()     const { return mTreeId; }
	RestoreTreeNode*  GetParentNode() const { return mpParentNode; }
	ServerCacheNode*  GetCacheNode()  const { return mpCacheNode; }
	const wxString&   GetFileName()   const { return mpCacheNode->GetFileName(); }
	const wxString&   GetFullPath()   const { return mpCacheNode->GetFullPath(); }
	const bool IsDirectory() const 
	{ 
		return mpCacheNode->GetMostRecent()->IsDirectory();
	}
	const bool IsDeleted() const 
	{ 
		return mpCacheNode->GetMostRecent()->IsDirectory();
	}
	const RestoreSpecEntry* GetMatchingEntry() const { return mpMatchingEntry; }
	void SetTreeId   (wxTreeItemId id)   { mTreeId = id; }
	
	bool AddChildren(bool recurse);
	void UpdateState(bool updateParents);
	
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
		const wxString& name 	= wxT("RestorePanel"));
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
	wxTextCtrl*			mpCountFilesBox;
	wxTextCtrl*			mpCountBytesBox;
	int					mListSortColumn;
	bool		  		mListSortReverse;
	RestoreTreeNode* 	mpTreeRoot;
	RestoreTreeNode* 	mpGlobalSelection;
	ServerSettings		mServerSettings;
	ServerConnection*   mpServerConnection;
	BackupProtocolClientAccountUsage* mpUsage;
	ServerCache*        mpCache;
	RestoreSpec         mRestoreSpec;
	std::vector<ServerCacheNode*> mCountFilesStack;
	uint64_t            mCountedFiles, mCountedBytes;
	
	void OnTreeNodeExpand  (wxTreeEvent& event);
	void OnTreeNodeSelect  (wxTreeEvent& event);
	void OnTreeNodeActivate(wxTreeEvent& event);
	void OnFileRestore     (wxCommandEvent& event);
	void OnFileDelete      (wxCommandEvent& event);
	void OnIdle            (wxIdleEvent& event);
	void GetUsageInfo();
	void StartCountingFiles();
	void UpdateFileCount();

	DECLARE_EVENT_TABLE()
};

extern const char * ErrorString(Protocol* proto);
extern const char * ErrorString(int type, int subtype);

#endif /* _SERVERFILESPANEL_H */
