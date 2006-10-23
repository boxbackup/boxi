/***************************************************************************
 *            CompareFilesPanel.cc
 *
 *  Sat Sep 30 19:41:49 2006
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

#include "main.h"
#include "CompareFilesPanel.h"

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
	MainFrame*        pMainFrame,
	wxWindow*         pParent,
	wxPanel*          pPanelToShowOnClose
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
