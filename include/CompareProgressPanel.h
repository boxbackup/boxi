/***************************************************************************
 *            CompareProgressPanel.h
 *
 *  Sun Sep 3 21:02:39 2006
 *  Copyright 2006-2008 Chris Wilson
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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#ifndef _COMPARE_PROGRESS_PANEL_H
#define _COMPARE_PROGRESS_PANEL_H

#include <wx/wx.h>
#include <wx/thread.h>

#include "SandBox.h"

#include "ProgressPanel.h"

class wxFileName;

class ClientConfig;
class ServerConnection;
class ServerCacheNode;
class BoxiCompareParams;

class CompareProgressPanel : public ProgressPanel
{
	public:
	CompareProgressPanel
	(
		ClientConfig*     pConfig,
		ServerConnection* pConnection,
		wxWindow*         pParent
	);

	void StartCompare(const BoxiCompareParams& rParams);

	private:
	ClientConfig*     mpConfig;
	ServerConnection* mpConnection;
	
	bool mCompareRunning;
	bool mCompareStopRequested;

	/*
	void CountDirectory(const BoxiCompareParams& rParams,
		const std::string &rLocalPath);
	*/
	
	virtual void OnStopCloseClicked(wxCommandEvent& event) 
	{ 
		if (mCompareRunning)
		{
			mCompareStopRequested = TRUE;
		}
		else
		{
			Hide();
		}
	}
	
	void CountFilesRecursive(const BoxiCompareParams& rParams, 
		ServerCacheNode* pRootNode, 
		ServerCacheNode* pCurrentNode, int blockSize);
	bool CompareFilesRecursive(const BoxiCompareParams& rParams,
		ServerCacheNode* pNode, int64_t parentId, 
		wxFileName& rLocalName, int blockSize);
	// wxFileName MakeLocalPath(wxFileName& rBase, ServerCacheNode* pNode);

	/*
	friend class TestRestore;
	int GetConnectionIndex() { return mpConnection->GetConnectionIndex(); }
	*/
	
	DECLARE_EVENT_TABLE()
};

#endif /* _COMPARE_PROGRESS_PANEL_H */
