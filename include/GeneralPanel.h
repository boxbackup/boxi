/***************************************************************************
 *            GeneralPanel.h
 *
 *  Mon Nov 21, 2005
 *  Copyright  2005  Chris Wilson
 *  Email <chris-boxisource@qwirx.com>
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
 
#ifndef _GENERAL_PANEL_H
#define _GENERAL_PANEL_H

#include <wx/wx.h>

class BackupPanel;
class ClientConfig;
class ClientInfoPanel;
class MainFrame;
class RestorePanel;
class ServerConnection;
class SetupWizard;

/** 
 * GeneralPanel
 * Allows the user to configure and start an interactive backup
 */

class GeneralPanel : public wxPanel
{
	public:
	GeneralPanel
	(
		MainFrame* pMainFrame,
		BackupPanel* pBackupPanel,
		ClientInfoPanel* pConfigPanel,
		ClientConfig* pConfig,
		ServerConnection* pServerConnection,
		wxWindow* pParent
	);

	void AddToNotebook(wxNotebook* pNotebook);
		
	private:
	MainFrame*    mpMainFrame;
	BackupPanel*  mpBackupPanel;
	RestorePanel* mpRestorePanel;
	ClientInfoPanel* mpConfigPanel;
	ClientConfig* mpConfig;
	SetupWizard*  mpWizard;
	
	void OnBackupButtonClick(wxCommandEvent& event);
	void OnRestoreButtonClick(wxCommandEvent& event);
	void OnSetupWizardButtonClick(wxCommandEvent& event);
	void OnSetupAdvancedButtonClick(wxCommandEvent& event);
	void OnIdle(wxIdleEvent& event);
	
	DECLARE_EVENT_TABLE()
};

#endif /* _GENERAL_PANEL_H */
