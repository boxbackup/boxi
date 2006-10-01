/***************************************************************************
 *            RestoreProgressPanel.h
 *
 *  Sun Sep 3 21:02:39 2006
 *  Copyright 2006 Chris Wilson
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
 
#ifndef _RESTORE_PROGRESS_PANEL_H
#define _RESTORE_PROGRESS_PANEL_H

#include <wx/wx.h>
#include <wx/thread.h>

#include "SandBox.h"

#define NDEBUG
#include "TLSContext.h"
#include "BackupClientContext.h"
#include "BackupClientDirectoryRecord.h"
#include "BackupDaemon.h"
#include "BackupStoreException.h"
#undef NDEBUG

#include "BoxiApp.h"
#include "ClientConfig.h"
#include "ClientConnection.h"
#include "ProgressPanel.h"

class ServerCacheNode;
class ServerConnection;
class RestoreSpec;

class RestoreProgressPanel : public ProgressPanel
{
	public:
	RestoreProgressPanel
	(
		ClientConfig*     pConfig,
		ServerConnection* pConnection,
		wxWindow*         pParent
	);

	void StartRestore(const RestoreSpec& rSpec, wxFileName dest);

	private:
	ClientConfig*     mpConfig;
	ServerConnection* mpConnection;
	TLSContext        mTlsContext;
	
	bool mRestoreRunning;
	bool mRestoreStopRequested;

	void CountDirectory(BackupClientContext& rContext,
		const std::string &rLocalPath);
	
	virtual void OnStopCloseClicked(wxCommandEvent& event) 
	{ 
		if (mRestoreRunning)
		{
			mRestoreStopRequested = TRUE;
		}
		else
		{
			Hide();
		}
	}
	
	ServerFileVersion* GetVersionToRestore(ServerCacheNode* pFile,
		const RestoreSpec& rSpec);
	void CountFilesRecursive(RestoreSpec& rSpec, 
		ServerCacheNode* pRootNode, 
		ServerCacheNode* pCurrentNode, int blockSize);
	bool RestoreFilesRecursive(const RestoreSpec& rSpec, 
		ServerCacheNode* pNode, int64_t parentId, 
		wxFileName localName, int blockSize);
	wxFileName MakeLocalPath(wxFileName base, ServerCacheNode* pNode);

	friend class TestRestore;
	int GetConnectionIndex() { return mpConnection->GetConnectionIndex(); }

	DECLARE_EVENT_TABLE()
};

#endif /* _RESTORE_PROGRESS_PANEL_H */
