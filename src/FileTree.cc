/***************************************************************************
 *            FileTree.cc
 *
 *  Mon Jan  2 12:33:42 2006
 *  Copyright  2006  Chris Wilson
 *  chris-boxisource@qwirx.com
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

#include "FileTree.h"

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
			UpdateStateIcon(pChildNode, FALSE, TRUE);
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
	
	if (pNode->AddChildren(this, FALSE))
	{
		UpdateStateIcon(pNode, FALSE, TRUE);
	}
	else
	{
		event.Veto();
	}
}
