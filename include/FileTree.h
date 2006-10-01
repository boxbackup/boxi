/***************************************************************************
 *            FileTree.h
 *
 *  Mon Jan  2 12:33:06 2006
 *  Copyright 2006 Chris Wilson
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
 
#ifndef _FILETREE_H
#define _FILETREE_H

#include <wx/image.h>
#include <wx/imaglist.h>
#include <wx/mstream.h>
#include <wx/treectrl.h>

#include "StaticImage.h"

class FileImageList : public wxImageList
{
	private:
	int mEmptyImageId;
	int mPartialImageId;
	int mCheckedImageId;
	int mCheckedGreyImageId;
	int mCrossedImageId;
	int mCrossedGreyImageId;
	int mAlwaysImageId;
	int mAlwaysGreyImageId;
	
	int AddImage(const struct StaticImage & rImageData)
	{
		wxMemoryInputStream byteStream(rImageData.data, rImageData.size);
		wxImage img(byteStream, wxBITMAP_TYPE_PNG);
		return Add(img);
	}

	public:
	FileImageList()
	:	wxImageList(16, 16, true)
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

	int GetEmptyImageId()       { return mEmptyImageId;       }
	int GetPartialImageId()     { return mPartialImageId;     }
	int GetCheckedImageId()     { return mCheckedImageId;     }
	int GetCheckedGreyImageId() { return mCheckedGreyImageId; }
	int GetCrossedImageId()     { return mCrossedImageId;     }
	int GetCrossedGreyImageId() { return mCrossedGreyImageId; }
	int GetAlwaysImageId()      { return mAlwaysImageId;      }
	int GetAlwaysGreyImageId()  { return mAlwaysGreyImageId;  }
};

class FileNode : public wxTreeItemData 
{
	private:
	FileNode* mpParentNode;
	
	public:
	FileNode() 
	: mpParentNode(NULL)
	{ }

	FileNode(FileNode* pParent)
	: mpParentNode(pParent)
	{ }

	FileNode* GetParentNode() const { return mpParentNode; }
	virtual const wxString& GetFileName() const = 0;
	virtual const wxString& GetFullPath() const = 0;
	
	bool AddChildren(wxTreeCtrl* pTreeCtrl, bool recurse);
	virtual int UpdateState(FileImageList& rImageList, bool updateParents) = 0;
	
	private:
	virtual bool _AddChildrenSlow(wxTreeCtrl* pTreeCtrl, bool recurse) = 0;
};

class FileTree : public wxTreeCtrl 
{
	public:
	FileTree(
		wxWindow* pParent, 
		wxWindowID id,
		FileNode* pRootNode,
		const wxString& rRootLabel
		);
	void UpdateStateIcon(FileNode* pNode, bool updateParents, 
		bool updateChildren);

	private:
	FileImageList mImages;

	virtual int OnCompareItems(const wxTreeItemId& item1, 
		const wxTreeItemId& item2) = 0;
	void OnTreeNodeExpand(wxTreeEvent& event);
	
	DECLARE_EVENT_TABLE()
};

class LocalFileTree;
	
class LocalFileNode : public FileNode 
{
	private:
	wxString mFileName;
	wxString mFullPath;
	bool     mIsRoot;
	bool     mIsDirectory;
	
	public:
	LocalFileNode(const wxString& path);
	LocalFileNode(LocalFileNode* pParent, const wxString& path);

	virtual LocalFileNode* CreateChildNode(LocalFileNode* pParent, 
		const wxString& rPath);
	
	virtual const wxString& GetFileName() const { return mFileName; }
	virtual const wxString& GetFullPath() const { return mFullPath; }

	bool IsDirectory() const { return mIsDirectory; }
	bool IsRoot()      const { return mIsRoot; }

	// void SetDirectory(bool value = true) { mIsDirectory = value; }
	virtual int UpdateState(FileImageList& rImageList, bool updateParents);

	private:
	virtual bool _AddChildrenSlow(LocalFileTree* pTreeCtrl, bool recurse);
	virtual bool _AddChildrenSlow(wxTreeCtrl*    pTreeCtrl, bool recurse)
	{
		return _AddChildrenSlow((LocalFileTree*)pTreeCtrl, recurse);
	}
};

class LocalFileTree : public FileTree
{
	public:
	LocalFileTree
	(
		wxWindow* pParent, 
		wxWindowID id,
		LocalFileNode* pRootNode,
		const wxString& rRootLabel
	);
	
	private:
	virtual int OnCompareItems(const wxTreeItemId& item1, 
		const wxTreeItemId& item2);
};

#endif /* _FILETREE_H */
