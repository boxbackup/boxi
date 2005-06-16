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

class RestoreTreeNode : public wxTreeItemData {
	public:
	int64_t 				boxFileID;
	wxTreeItemId 			treeId;
	ClientConfig*			theConfig;
	wxTreeCtrl*				theTree;
	RestoreTreeNode*		theServerNode;
	RestoreTreeNode*		theParentNode;
	wxString				mFileName;
	bool					isDirectory;
	bool					isDeleted;
	int16_t 				mFlags;
	int64_t					mSizeInBlocks;
	bool					mHasAttributes;
	wxDateTime				mDateTime;
	ServerSettings*			mpServerSettings;
	ServerConnection*       mpServerConnection;
	
	RestoreTreeNode();
	~RestoreTreeNode();
	bool ShowChildren(wxListCtrl *targetList);

	private:
	bool ScanChildrenAndList(wxListCtrl *targetList, bool recurse);
	BackupProtocolClientAccountUsage* mpUsage;
};

// declare our list class: this macro declares and partly implements 
// DirEntryPtrList class (which derives from wxListBase)
WX_DECLARE_LIST(RestoreTreeNode, DirEntryPtrList);

class ServerFilesPanel : public wxPanel {
	public:
	ServerFilesPanel(
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
	void SetViewOldFlag    (bool NewValue);
	void SetViewDeletedFlag(bool NewValue);
	bool GetViewOldFlag    () { return mServerSettings.mViewOldFiles; }
	bool GetViewDeletedFlag() { return mServerSettings.mViewDeletedFiles; }
	// void RefreshItem(RestoreTreeNode* pItem);
	
	private:
	ClientConfig*		mpConfig;
	wxTreeCtrl*			theServerFileTree;
	wxListView*			theServerFileList;
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
