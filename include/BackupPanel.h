/***************************************************************************
 *            BackupPanel.h
 *
 *  Mon Apr  4 20:35:39 2005
 *  Copyright 2005-2006 Chris Wilson
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
 
#ifndef _BACKUP_PANEL_H
#define _BACKUP_PANEL_H

//#include <wx/wx.h>
//#include <wx/thread.h>

// #include "ClientConfig.h"
// #include "ClientInfoPanel.h"
#include "FunctionPanel.h"
//#include "ClientConnection.h"

class BackupLocationsPanel;
class BackupProgressPanel;
	
class wxNotebook;
class wxStaticText;

/** 
 * BackupPanel
 * Allows the user to configure and start an interactive backup
 */

class BackupPanel : public FunctionPanel 
{
	public:
	BackupPanel
	(
		ClientConfig*    pConfig,
		ClientInfoPanel* pClientConfigPanel,
		MainFrame*       pMainFrame,
		wxWindow*        pParent
	);
	
	void AddToNotebook(wxNotebook* pNotebook);
	
	private:
	BackupProgressPanel*  mpProgressPanel;
	BackupLocationsPanel* mpLocationsPanel;
	
	wxStaticText* mpDestLabel;
	wxSizer*      mpDestCtrlSizer;
	wxButton*     mpDestEditButton;

	virtual void Update();
	virtual void OnClickSourceButton(wxCommandEvent& rEvent);
	virtual void OnClickDestButton  (wxCommandEvent& rEvent);
	virtual void OnClickStartButton (wxCommandEvent& rEvent);
};

#endif /* _BACKUP_PANEL_H */
