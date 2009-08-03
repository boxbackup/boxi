/***************************************************************************
 *            RestoreFilesPanel.h
 *
 *  Mon Feb 28 22:48:43 2005
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
 
#ifndef RESTOREFILESPANEL_H
#define RESTOREFILESPANEL_H

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
#include "BackupClientFileAttributes.h"
#undef NDEBUG

#include "ClientConfig.h"
#include "ServerConnection.h"
#include "FileTree.h"

class ServerSettings {
	public:
	bool				mViewOldFiles;
	bool				mViewDeletedFiles;
};

class ServerFileVersion 
{
	public:
	typedef std::vector<ServerFileVersion> Vector;
	typedef Vector::iterator Iterator;
	typedef Vector::const_iterator ConstIterator;

	class SafeVector
	{
		private:
		std::vector<ServerFileVersion>& mrRealVector;
		public:
		SafeVector(std::vector<ServerFileVersion>& rRealVector)
		: mrRealVector(rRealVector) { }
		ServerFileVersion::Iterator begin() { return mrRealVector.begin(); }
		ServerFileVersion::Iterator end()   { return mrRealVector.end(); }
	};		
	
	private:
	int64_t       mBoxFileId;
	bool          mIsDirectory;
	bool          mIsDeleted;
	ClientConfig* mpConfig;
	int16_t       mFlags;
	int64_t       mSizeInBlocks;
	// bool       mHasAttributes;
	wxDateTime    mDateTime;
	bool          mCached;
	std::auto_ptr<BackupClientFileAttributes> mapAttributes;
	
	public:
	ServerFileVersion() 
	{
		mBoxFileId    = BackupProtocolClientListDirectory::RootDirectory;
		mIsDirectory  = true;
		mIsDeleted    = false;
		mFlags        = 0;
		mSizeInBlocks = 0;
		mCached       = false;
		mDateTime     = wxDateTime((time_t)0);
	}
	ServerFileVersion(BackupStoreDirectory::Entry* pDirEntry);
	ServerFileVersion(const ServerFileVersion& rToCopy);
	ServerFileVersion& operator=(const ServerFileVersion& rToCopy);
	
	const wxDateTime& GetDateTime()   const { return mDateTime; }
	int64_t           GetBoxFileId()  const { return mBoxFileId; }
	int64_t           GetSizeBlocks() const { return mSizeInBlocks; }
	bool IsDirectory()                const { return mIsDirectory; }
	bool IsDeleted()                  const { return mIsDeleted; }
	void SetDeleted(bool value = true) { mIsDeleted   = value; }
	bool HasAttributes()              const { return mapAttributes.get(); }
	BackupClientFileAttributes GetAttributes() const 
	{
		return *mapAttributes; // silent clone		
	}

	void SetAttributes(const BackupClientFileAttributes& rAttribs)
	{
		mapAttributes.reset(new BackupClientFileAttributes(rAttribs));
	}
};

class ServerCacheNode 
{
	public:
	typedef std::vector<ServerCacheNode> Vector;
	typedef Vector::iterator       Iterator;
	typedef Vector::const_iterator ConstIterator;
	
	class SafeVector
	{
		private:
		std::vector<ServerCacheNode>& mrRealVector;
		public:
		SafeVector(std::vector<ServerCacheNode>& rRealVector)
		: mrRealVector(rRealVector) { }
		ServerCacheNode::Iterator begin() { return mrRealVector.begin(); }
		ServerCacheNode::Iterator end()   { return mrRealVector.end(); }
	};		

	private:
	wxString                  mFileName;
	wxString                  mFullPath;
	ServerFileVersion::Vector mVersions;
	ServerFileVersion*        mpMostRecent;
	ServerCacheNode::Vector   mChildren;
	ServerCacheNode*          mpParentNode;
	bool                      mCached;
	ServerConnection*         mpServerConnection;
	int                       mConnectionIndex;
	wxMutex                   mMutex;
	ServerCacheNode::SafeVector   mChildrenSafe;
	ServerFileVersion::SafeVector mVersionsSafe;
	
	public:
	ServerCacheNode(ServerConnection* pServerConnection) 
	: mFileName         (wxT("")),
	  mFullPath         (wxT("/")),
	  mpMostRecent      (NULL),
	  mpParentNode      (NULL),
	  mCached           (false),
	  mpServerConnection(pServerConnection),
	  mConnectionIndex  (pServerConnection->GetConnectionIndex()),
	  mChildrenSafe     (mChildren),
	  mVersionsSafe     (mVersions)
	{
		ServerFileVersion dummyRootVersion; 
		mVersions.push_back(dummyRootVersion);
	}
	
	ServerCacheNode
	(
		ServerCacheNode& rParent, 
		BackupStoreDirectory::Entry* pDirEntry
	)
	: mFileName         (GetDecryptedName(pDirEntry)),
	  mpMostRecent      (NULL),
	  mpParentNode      (&rParent),
	  mCached           (false),
	  mpServerConnection(rParent.mpServerConnection),
	  mConnectionIndex  (mpServerConnection->GetConnectionIndex()),
	  mChildrenSafe     (mChildren),
	  mVersionsSafe     (mVersions)
	{ 
		if (rParent.IsRoot())
		{
			mFullPath.Printf(wxT("/%s"), mFileName.c_str());
		}
		else
		{
			mFullPath.Printf(wxT("%s/%s"), 
				rParent.GetFullPath().c_str(), 
				mFileName.c_str());
		}
	}
	
	ServerCacheNode(const ServerCacheNode& rToCopy)
	: mFileName         (rToCopy.mFileName),
	  mFullPath         (rToCopy.mFullPath),
	  mVersions         (rToCopy.mVersions),
	  mpMostRecent      (rToCopy.mpMostRecent),
	  mChildren         (rToCopy.mChildren),
	  mpParentNode      (rToCopy.mpParentNode),
	  mCached           (rToCopy.mCached),
	  mpServerConnection(rToCopy.mpServerConnection),
	  mConnectionIndex  (rToCopy.mConnectionIndex),
	  mChildrenSafe     (mChildren),
	  mVersionsSafe     (mVersions)
	{ }
	
	ServerCacheNode& operator=(const ServerCacheNode& rToCopy)
	{
		mFileName          = rToCopy.mFileName;
	  	mFullPath          = rToCopy.mFullPath;
	  	mVersions          = rToCopy.mVersions;
	  	mpMostRecent       = rToCopy.mpMostRecent;
	  	mChildren          = rToCopy.mChildren;
	  	mpParentNode       = rToCopy.mpParentNode;
	  	mCached            = rToCopy.mCached;
	  	mpServerConnection = rToCopy.mpServerConnection;
	  	mConnectionIndex   = rToCopy.mConnectionIndex;
		return *this;
	}
	
	static wxString GetDecryptedName(BackupStoreDirectory::Entry* pDirEntry)
	{
		BackupStoreFilenameClear clear(pDirEntry->GetName());
		wxString name(clear.GetClearFilename().c_str(), wxConvBoxi);
		return name;
	}
		
	bool IsRoot() const { return (mpParentNode == NULL); }
	
	const wxString&    GetFileName()      const { return mFileName; }
	const wxString&    GetFullPath()      const { return mFullPath; }
	ServerCacheNode*   GetParent()        const { return mpParentNode; }
	ServerFileVersion::SafeVector& GetVersions() { return mVersionsSafe; }
	ServerFileVersion*             GetMostRecent();
	ServerCacheNode::SafeVector*   GetChildren();

	wxMutex&           GetLock() { return mMutex; }	
};

class ServerCache
{
	private:
	ServerCacheNode mRoot;
	
	public:
	ServerCache(ServerConnection* pServerConnection) 
	: mRoot(pServerConnection)
	{ }
	
	ServerCacheNode& GetRoot() { return mRoot; }
};

class RestoreSpecEntry
{
	public:
	typedef std::vector<RestoreSpecEntry> Vector;
	typedef Vector::iterator Iterator;
	typedef Vector::const_iterator ConstIterator;
		
	private:
	ServerCacheNode* mpNode;
	bool mInclude;

	public:
	RestoreSpecEntry(ServerCacheNode& rNode, bool lInclude)
	: mpNode(&rNode)
	{
		mInclude = lInclude;
	}
	
	ServerCacheNode& GetNode() const { return *mpNode; }
	bool IsInclude() const { return mInclude; }
	bool IsSameAs(const RestoreSpecEntry& rOther) const
	{
		return mpNode == rOther.mpNode &&
			mInclude == rOther.mInclude;
	}
};

class RestoreSpec
{
	private:
	RestoreSpecEntry::Vector mEntries;
	bool mRestoreToDateEnabled;
	wxDateTime mRestoreToDate;
	
	public:
	RestoreSpec() { }
	const RestoreSpecEntry::Vector& GetEntries() const { return mEntries; }
	void Add   (const RestoreSpecEntry& rNewEntry);
	void Remove(const RestoreSpecEntry& rOldEntry);
	void SetRestoreToDateEnabled(bool newValue) 
	{ mRestoreToDateEnabled = newValue; }
	void SetRestoreToDate(const wxDateTime& rNewValue)
	{ mRestoreToDate = rNewValue; }
	bool       GetRestoreToDateEnabled() const { return mRestoreToDateEnabled; }
	wxDateTime GetRestoreToDate       () const { return mRestoreToDate; }
};

class RestoreSpecChangeListener
{
	public:
	virtual void OnRestoreSpecChange() = 0;
	virtual ~RestoreSpecChangeListener() { }
};

class MainFrame;
class RestoreTreeCtrl;
class RestoreTreeNode;
	
class RestoreFilesPanel : public wxPanel 
{
	public:
	RestoreFilesPanel
	(
		ClientConfig*     pConfig,
		ServerConnection* pServerConnection,
		MainFrame*        pMainFrame,
		wxWindow*         pParent,
		RestoreSpecChangeListener* pListener,
		wxPanel*          pPanelToShowOnClose
	);
	
	void RestoreProgress(RestoreState State, std::string& rFileName);
	/*
	int  GetListSortColumn () { return mListSortColumn; }
	bool GetListSortReverse() { return mListSortReverse; }
	*/
	bool GetViewOldFlag    () { return mServerSettings.mViewOldFiles; }
	bool GetViewDeletedFlag() { return mServerSettings.mViewDeletedFiles; }
	void SetViewOldFlag    (bool NewValue);
	void SetViewDeletedFlag(bool NewValue);
	// void RefreshItem(RestoreTreeNode* pItem);
	
	private:
	friend class RestorePanel;
	RestoreSpec& GetRestoreSpec() { return mRestoreSpec; }
	
	ClientConfig*       mpConfig;
	RestoreTreeCtrl*    mpTreeCtrl;
	// wxListView*		theServerFileList;
	/*
	wxButton*			mpDeleteButton;
	wxStatusBar*		mpStatusBar;
	int					mRestoreCounter;
	wxTextCtrl*			mpCountFilesBox;
	wxTextCtrl*			mpCountBytesBox;
	int					mListSortColumn;
	bool		  		mListSortReverse;
	*/
	RestoreTreeNode*    mpTreeRoot;
	RestoreTreeNode*    mpGlobalSelection;
	ServerSettings      mServerSettings;
	ServerConnection*   mpServerConnection;
	BackupProtocolClientAccountUsage* mpUsage;
	ServerCache         mCache;
	RestoreSpec         mRestoreSpec;
	MainFrame*          mpMainFrame;
	wxPanel*            mpPanelToShowOnClose;
	RestoreSpecChangeListener* mpListener;
	/*
	std::vector<ServerCacheNode*> mCountFilesStack;
	uint64_t            mCountedFiles, mCountedBytes;
	*/
	void OnTreeNodeSelect  (wxTreeEvent& event);
	void OnTreeNodeActivate(wxTreeEvent& event);
	void OnCloseButtonClick(wxCommandEvent& rEvent);
	/*
	void OnFileRestore     (wxCommandEvent& event);
	void OnFileDelete      (wxCommandEvent& event);
	void OnIdle            (wxIdleEvent& event);
	
	void GetUsageInfo();
	void StartCountingFiles();
	void UpdateFileCount();
	*/
	
	DECLARE_EVENT_TABLE()
};

extern const char * ErrorString(Protocol* proto);
extern const char * ErrorString(int type, int subtype);

#endif /* RESTOREFILESPANEL_H */
