/***************************************************************************
 *            FileTree.cc
 *
 *  Mon Jan  2 12:33:42 2006
 *  Copyright  2006  Chris Wilson
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

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/string.h>
#include <wx/volume.h>

#include "FileTree.h"

FileImageList::FileImageList()
: wxImageList(16, 16, true)
{
	mEmptyImageId        = AddImage(empty16_png);
	mPartialImageId      = AddImage(partialtick16_png);
	mCheckedImageId      = AddImage(tick16_png);
	mCheckedGreyImageId  = AddImage(tickgrey16_png);
	mCrossedImageId      = AddImage(cross16_png);
	mCrossedGreyImageId  = AddImage(crossgrey16_png);
	mAlwaysImageId       = AddImage(plus16_png);
	mAlwaysGreyImageId   = AddImage(plusgrey16_png);
}

IMPLEMENT_DYNAMIC_CLASS(FileTree, wxTreeCtrl)

bool FileNode::AddChildren(wxTreeCtrl* pTreeCtrl, bool recurse) 
{
	pTreeCtrl->SetCursor(*wxHOURGLASS_CURSOR);
	wxSafeYield();
	
	bool result;
	
	try 
	{
		result = _AddChildrenSlow(pTreeCtrl, recurse);
	} 
	catch (...) 
	{
		pTreeCtrl->SetCursor(*wxSTANDARD_CURSOR);
		throw;
	}
	
	pTreeCtrl->SetCursor(*wxSTANDARD_CURSOR);
	return result;
}

FileTree::FileTree
(
	wxWindow* pParent, 
	wxWindowID id,
	FileNode* pRootNode,
	const wxString& rRootLabel
)
:	wxTreeCtrl(pParent, id, wxDefaultPosition, wxDefaultSize, 
		wxTR_DEFAULT_STYLE | wxTR_HAS_BUTTONS | wxSUNKEN_BORDER, 
		wxDefaultValidator, wxT("FileTree"))
{
	SetImageList(&mImages);

	wxTreeItemId lRootId = AddRoot(rRootLabel, -1, -1, pRootNode);
	pRootNode->SetId(lRootId);
	SetItemHasChildren(lRootId, TRUE);
	UpdateStateIcon(pRootNode, false, false);
}

void FileTree::UpdateStateIcon(FileNode* pNode, bool updateParents, 
	bool updateChildren) 
{	
	int iconId = pNode->UpdateState(mImages, updateParents);
	SetItemImage(pNode->GetId(), iconId, wxTreeItemIcon_Normal);

	if (updateParents && pNode->GetParentNode() != NULL)
	{
		UpdateStateIcon(pNode->GetParentNode(), TRUE, FALSE);
	}
	
	if (updateChildren)
	{
		wxTreeItemId thisId = pNode->GetId();
		wxTreeItemIdValue cookie;
		
		for (wxTreeItemId childId = GetFirstChild(thisId, cookie);
			childId.IsOk(); childId = GetNextChild(thisId, cookie))
		{
			FileNode* pChildNode = (FileNode*)GetItemData(childId);
			UpdateStateIcon(pChildNode, false, true);
		}
	}
}

BEGIN_EVENT_TABLE(FileTree, wxTreeCtrl)
	EVT_TREE_ITEM_EXPANDING(wxID_ANY, FileTree::OnTreeNodeExpand)
END_EVENT_TABLE()

void FileTree::OnTreeNodeExpand(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	FileNode *pNode = (FileNode *)GetItemData(item);
	
	if (pNode->AddChildren(this, false))
	{
		UpdateStateIcon(pNode, false, true);
	}
	else
	{
		event.Veto();
	}
}

LocalFileNode::LocalFileNode(const wxString& path)
: FileNode     (),
  mFileName    (wxFileName(path).GetFullName()),
  mFullPath    (path),
  mIsRoot      (true),
  mIsDirectory (wxFileName::DirExists(mFullPath))
{ }

LocalFileNode::LocalFileNode(LocalFileNode* pParent, const wxString& path) 
: FileNode     (pParent),
  mFileName    (wxFileName(path).GetFullName()),
  mFullPath    (path),
  mIsRoot      (false),
  mIsDirectory (wxFileName::DirExists(mFullPath))
{ 
	if (path.Length() == 3 && path.Mid(1, 2).IsSameAs(wxT(":\\")))
	{
		mIsDirectory = true;
	}
	else
	{
		mIsDirectory = wxFileName::DirExists(mFullPath);
	}
}

LocalFileNode* LocalFileNode::CreateChildNode(LocalFileNode* pParent, 
	const wxString& rPath)
{
	return new LocalFileNode(pParent, rPath);
}

WX_DECLARE_OBJARRAY(wxFileName, FileNameArray);
#include <wx/arrimpl.cpp>
WX_DEFINE_OBJARRAY(FileNameArray);

bool LocalFileNode::_AddChildrenSlow(LocalFileTree* pTreeCtrl, bool recursive) 
{
	// delete any existing children of the parent
	pTreeCtrl->DeleteChildren(GetId());

	FileNameArray entries;

#ifdef WIN32
	// root node is My Computer, contains a list of drives
	if (GetParentNode() == NULL && mFullPath.IsSameAs(wxEmptyString))
	{
		wxArrayString volumes = wxFSVolumeBase::GetVolumes();
		for (size_t i = 0; i < volumes.GetCount(); i++)
		{
			wxFileName fn(volumes.Item(i));
			entries.Add(fn);
		}
	}
	else
	{
#endif // WIN32
		wxDir dir(mFullPath);
		
		if (!dir.IsOpened())
		{
			// wxDir would already show an error message explaining the 
			// exact reason of the failure, and the user can always click 
			// [+] another time :-)
			return false;
		}

		wxString theCurrentFileName;
		
		bool doContinue = dir.GetFirst(&theCurrentFileName);
		while (doContinue)
		{
			wxFileName fn = wxFileName(mFullPath, 
				theCurrentFileName);
			entries.Add(fn);
			doContinue = dir.GetNext(&theCurrentFileName);
		}
#ifdef WIN32
	}
#endif

	for (size_t i = 0; i < entries.GetCount(); i++)
	{
		wxFileName fileNameObject = entries.Item(i);

		// add to the tree
		LocalFileNode *pNewNode = CreateChildNode(this,
			fileNameObject.GetFullPath());
		
		wxString label = fileNameObject.GetFullName();

		#ifdef WIN32
		if (label.IsSameAs(wxEmptyString))
		{
			wxFSVolumeBase vol(fileNameObject.GetFullPath());
			label = vol.GetDisplayName();
		}
		#endif

		wxTreeItemId newId = pTreeCtrl->AppendItem(GetId(),
			label, -1, -1, pNewNode);
		
		pNewNode->SetId(newId);

		if (pNewNode->IsDirectory())
		{
			if (recursive) 
			{
				if (!pNewNode->_AddChildrenSlow(pTreeCtrl, 
					false))
				{
					return false;
				}
			} 
			else 
			{
				pTreeCtrl->SetItemHasChildren(pNewNode->GetId(),
					true);
			}
		}
	}
	
	// sort out the kids
	pTreeCtrl->SortChildren(GetId());
	
	return true;
}

int LocalFileNode::UpdateState(FileImageList& rImageList, bool updateParents) 
{
	LocalFileNode* pParentNode = (LocalFileNode*)GetParentNode();
	
	if (updateParents && pParentNode != NULL)
	{
		pParentNode->UpdateState(rImageList, true);
	}
	
	return rImageList.GetEmptyImageId();
}

LocalFileTree::LocalFileTree
(
	wxWindow* pParent, 
	wxWindowID id,
	LocalFileNode* pRootNode,
	const wxString& rRootLabel
)
: FileTree(pParent, id, pRootNode, rRootLabel)
{ }

int LocalFileTree::OnCompareItems
(
	const wxTreeItemId& item1, 
	const wxTreeItemId& item2
)
{
	LocalFileNode *pNode1 = (LocalFileNode*)GetItemData(item1);
	LocalFileNode *pNode2 = (LocalFileNode*)GetItemData(item2);

	{
		bool dir1 = pNode1->IsDirectory();
		bool dir2 = pNode2->IsDirectory();
		
		if ( dir1 && !dir2 )
		{
			return -1;
		}
		
		if ( dir2 && !dir1 ) 
		{
			return 1;
		}
	}
	
	wxString name1 = pNode1->GetFullPath();
	wxString name2 = pNode2->GetFullPath();
	
	return name1.CompareTo(name2);
}
