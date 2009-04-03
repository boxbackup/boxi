/***************************************************************************
 *            CompareFilesPanel.cc
 *
 *  Sat Sep 30 19:41:49 2006
 *  Copyright 2006-2008 Chris Wilson
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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "SandBox.h"

#include "main.h"
#include "CompareFilesPanel.h"
#include "FileTree.h"

class CompareParams
{
};

class CompareTreeNode : public LocalFileNode 
{
	private:
	CompareParams& mrParams;
	int mIconId;
	
	public:
	CompareTreeNode(CompareParams& rParams,   const wxString& path);
	CompareTreeNode(CompareTreeNode* pParent, const wxString& path);
	virtual LocalFileNode* CreateChildNode(LocalFileNode* pParent, 
		const wxString& rPath);

	virtual int UpdateState(FileImageList& rImageList, bool updateParents);
};

CompareTreeNode::CompareTreeNode(CompareParams& rParams, const wxString& path)
: LocalFileNode (path),
  mrParams      (rParams),
  mIconId       (-1)
{ }

CompareTreeNode::CompareTreeNode(CompareTreeNode* pParent, const wxString& path) 
: LocalFileNode (pParent, path),
  mrParams      (pParent->mrParams),
  mIconId       (-1)
{ }

LocalFileNode* CompareTreeNode::CreateChildNode(LocalFileNode* pParent, 
	const wxString& rPath)
{
	return new CompareTreeNode((CompareTreeNode *)pParent, rPath);
}

int CompareTreeNode::UpdateState(FileImageList& rImageList, bool updateParents) 
{
	int iconId = LocalFileNode::UpdateState(rImageList, updateParents);
	
	/*
	CompareTreeNode* pParentNode = (CompareTreeNode*)GetParentNode();
	
	// by default, inherit our include/exclude state
	// from our parent node, if we have one
	
	if (pParentNode) 
	{
		mpLocation     = pParentNode->mpLocation;
		mpExcludedBy   = pParentNode->mpExcludedBy;
		mpIncludedBy   = pParentNode->mpIncludedBy;
	} 
	else 
	{
		mpLocation   = NULL;
		mpExcludedBy = NULL;
		mpIncludedBy = NULL;
	}
		
	const BoxiLocation::List& rLocations = mpConfig->GetLocations();

	if (!mpLocation) 
	{
		// determine whether or not this node's path
		// is inside a backup location.
	
		for (BoxiLocation::ConstIterator
			pLoc  = rLocations.begin();
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
	
	bool matchedRule = false;
	
	if (!mpLocation)
	{
		// if this node doesn't belong to a location, then by definition
		// our parent node doesn't have one either, so the inherited
		// values are fine, leave them alone.
	}
	else if (mpExcludedBy && !mpIncludedBy)
	{
		// A node whose parent is excluded is also excluded,
		// since Box Backup will never scan it.
	}
	else
	{
		mpLocation->GetExcludedState(
			GetFullPath(), IsDirectory(), 
			&mpExcludedBy, &mpIncludedBy, &matchedRule);
	}

	// now decide which icon to use, based on the resulting state.
	
	if (mpIncludedBy != NULL)
	{
		if (matchedRule)
		{
			iconId = rImageList.GetAlwaysImageId();
		}
		else
		{
			iconId = rImageList.GetAlwaysGreyImageId();
		}
	}
	else if (mpExcludedBy != NULL)
	{
		if (matchedRule)
		{
			iconId = rImageList.GetCrossedImageId();
		}
		else
		{
			iconId = rImageList.GetCrossedGreyImageId();
		}
	}
	else if (mpLocation != NULL)
	{
		if (matchedRule)
		{
			iconId = rImageList.GetCheckedImageId();
		}
		else
		{
			iconId = rImageList.GetCheckedGreyImageId();
		}
	}
	else
	{
		// this node is not included in any location, but we need to
		// check whether our path is a prefix to any configured location,
		// to decide whether to display a blank or a partially included icon.
		
		const BoxiLocation::List& rLocations = mpConfig->GetLocations();
		wxFileName thisNodePath(GetFullPath());
		bool found = false;
		
		for (BoxiLocation::ConstIterator
			pLoc  = rLocations.begin();
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
	*/
	
	return iconId;
}

BEGIN_EVENT_TABLE(CompareFilesPanel, wxPanel)
/*
	EVT_TREE_SEL_CHANGING(ID_Server_File_Tree,
		RestoreFilesPanel::OnTreeNodeSelect)
	EVT_TREE_ITEM_ACTIVATED(ID_Server_File_Tree,
		RestoreFilesPanel::OnTreeNodeActivate)
	EVT_BUTTON(wxID_CANCEL, RestoreFilesPanel::OnCloseButtonClick)
	EVT_BUTTON(ID_Server_File_RestoreButton, 
		RestoreFilesPanel::OnFileRestore)
	EVT_BUTTON(ID_Server_File_DeleteButton, 
		RestoreFilesPanel::OnFileDelete)
	EVT_IDLE(RestoreFilesPanel::OnIdle)
*/
END_EVENT_TABLE()

CompareFilesPanel::CompareFilesPanel
(
	ClientConfig*     pConfig,
	ServerConnection* pServerConnection,
	/* MainFrame*        pMainFrame, */
	wxWindow*         pParent
	/* wxPanel*          pPanelToShowOnClose */
)
:	wxPanel(pParent, ID_Compare_Files_Panel, wxDefaultPosition, 
		wxDefaultSize, wxTAB_TRAVERSAL, wxT("CompareFilesPanel")) /*,
	mCache(pServerConnection),
	mpMainFrame(pMainFrame),
	mpPanelToShowOnClose(pPanelToShowOnClose),
	mpListener(pListener) */
{
	/*
	mpConfig = pConfig;
	// mpStatusBar = pMainFrame->GetStatusBar();
	mpServerConnection = pServerConnection;
	
	wxBoxSizer *topSizer = new wxBoxSizer( wxVERTICAL );
	SetSizer(topSizer);

	mpTreeRoot = new RestoreTreeNode(mCache.GetRoot(), 
		&mServerSettings, mRestoreSpec);

	mpTreeCtrl = new RestoreTreeCtrl(this, ID_Server_File_Tree,
		mpTreeRoot);
	topSizer->Add(mpTreeCtrl, 1, wxGROW | wxALL, 8);
		
	wxSizer* pActionCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	topSizer->Add(pActionCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	wxButton* pCloseButton = new wxButton(this, wxID_CANCEL, wxT("Close"));
	pActionCtrlSizer->Add(pCloseButton, 0, wxGROW | wxLEFT, 8);
	
	mServerSettings.mViewOldFiles     = FALSE;
	mServerSettings.mViewDeletedFiles = FALSE;
	*/
}
