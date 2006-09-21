/***************************************************************************
 *            RestorePanel.h
 *
 *  2005-12-31
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
 
#ifndef _RESTORE_PANEL_H
#define _RESTORE_PANEL_H

//#include <wx/wx.h>
//#include <wx/thread.h>

// #include "ClientConfig.h"
// #include "ClientInfoPanel.h"
#include "FunctionPanel.h"
//#include "ClientConnection.h"
#include "RestoreFilesPanel.h"

class RestoreProgressPanel;
class DirSelButton;
class wxDatePickerCtrl;
class wxSpinCtrl;

/** 
 * RestorePanel
 * Allows the user to configure and start an interactive restore
 */

class RestorePanel : public FunctionPanel, RestoreSpecChangeListener
{
	public:
	RestorePanel
	(
		ClientConfig*     pConfig,
		ClientInfoPanel*  pClientConfigPanel,
		MainFrame*        pMainFrame,
		ServerConnection* pServerConnection,
		wxWindow*         pParent
	);
	
	void AddToNotebook(wxNotebook* pNotebook);
	virtual void OnRestoreSpecChange();
	
	private:
	friend class TestRestore;	
	// for use in unit tests only!
	RestoreSpec& GetRestoreSpec() { return mpFilesPanel->GetRestoreSpec(); }
	
	RestoreProgressPanel* mpProgressPanel;
	RestoreFilesPanel* mpFilesPanel;
	// wxRadioButton* mpOldLocRadio;
	// wxRadioButton* mpNewLocRadio;
	wxTextCtrl*    mpNewLocText;
	DirSelButton*  mpNewLocButton;
	wxCheckBox*    mpToDateCheckBox;
	wxDatePickerCtrl* mpDatePicker;
	wxSpinCtrl*    mpHourSpin;
	wxSpinCtrl*    mpMinSpin;
	// wxCheckBox*    mpRestoreDirsCheck;

	virtual void Update();
	virtual void OnClickSourceButton(wxCommandEvent& rEvent);
	virtual void OnClickStartButton (wxCommandEvent& rEvent);
	void OnCheckBoxClick(wxCommandEvent& rEvent);
	void OnClickToDateCheckBox(wxCommandEvent& rEvent);
	void UpdateEnabledState();
	
	DECLARE_EVENT_TABLE()
};

#endif /* _RESTORE_PANEL_H */
