/***************************************************************************
 *            CompareFilesPanel.h
 *
 *  Sat Sep 30 19:41:08 2006
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
 
#ifndef _COMPAREFILESPANEL_H
#define _COMPAREFILESPANEL_H

#include <wx/panel.h>

class ClientConfig;
class CompareSpec;
class MainFrame;
class RestoreTreeCtrl;
class RestoreTreeNode;
class ServerConnection;	

class CompareFilesPanel : public wxPanel 
{
	public:
	CompareFilesPanel
	(
		ClientConfig*     pConfig,
		ServerConnection* pServerConnection,
		MainFrame*        pMainFrame,
		wxWindow*         pParent,
		wxPanel*          pPanelToShowOnClose
	);
	
	private:
	friend class ComparePanel;
	/*	
	CompareSpec& GetCompareSpec() { return mCompareSpec; }

	ClientConfig*       mpConfig;
	RestoreTreeCtrl*    mpTreeCtrl;
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
	void OnTreeNodeSelect  (wxTreeEvent& event);
	void OnTreeNodeActivate(wxTreeEvent& event);
	void OnCloseButtonClick(wxCommandEvent& rEvent);
	*/
	
	DECLARE_EVENT_TABLE()
};

#endif /* _COMPAREFILESPANEL_H */
