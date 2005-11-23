/***************************************************************************
 *            BackupPanel.h
 *
 *  Mon Apr  4 20:35:39 2005
 *  Copyright  2005  Chris Wilson
 *  Email <boxi_BackupPanel.h@qwirx.com>
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
 
#ifndef _BACKUP_PANEL_H
#define _BACKUP_PANEL_H

#include <wx/wx.h>
#include <wx/thread.h>

#include "ClientConfig.h"
#include "ClientInfoPanel.h"
#include "ClientConnection.h"
#include "BackupProgressPanel.h"

/** 
 * BackupPanel
 * Allows the user to configure and start an interactive backup
 */

class BackupPanel 
: public wxPanel, public ConfigChangeListener 
{
	public:
	BackupPanel(
		ClientConfig *pConfig,
		BackupProgressPanel* pProgressPanel,
		MainFrame* pMainFrame,
		ClientInfoPanel* pClientConfigPanel,
		wxWindow* parent, wxWindowID id = -1,
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL, 
		const wxString& name = wxT("Backup Panel"));

	private:
	ClientConfig* mpConfig;
	ClientInfoPanel* mpClientConfigPanel;
	BackupProgressPanel* mpProgressPanel;
	wxListBox*    mpSourceList;
	wxStaticText* mpDestLabel;
	MainFrame*    mpMainFrame;
	
	void NotifyChange();
	void Update();
	void OnClickLocationsButton(wxCommandEvent& rEvent);
	void OnClickServerButton   (wxCommandEvent& rEvent);
	void OnClickStartButton    (wxCommandEvent& rEvent);
	void OnClickCloseButton    (wxCommandEvent& rEvent);
	
	DECLARE_EVENT_TABLE()
};

#endif /* _BACKUP_PANEL_H */
