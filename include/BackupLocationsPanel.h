/***************************************************************************
 *            BackupLocationsPanel.h
 *
 *  Tue Mar  1 00:26:35 2005
 *  Copyright  2005  Chris Wilson
 *  anjuta@qwirx.com
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
 
#ifndef _BACKUPLOCATIONSPANEL_H
#define _BACKUPLOCATIONSPANEL_H

#include <wx/wx.h>
#include <wx/listctrl.h>

#include "ClientConfig.h"

class BackupLocationsPanel : public wxPanel, public ConfigChangeListener {
	public:
	BackupLocationsPanel(ClientConfig *config,
		wxWindow* parent, wxWindowID id = -1,
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL, 
		const wxString& name = "panel");
	
	private:
	ClientConfig	*theConfig;
	wxListCtrl 		*theLocationList;
	wxButton 		*theLocationAddBtn;
	wxButton 		*theLocationEditBtn;
	wxButton 		*theLocationRemoveBtn;
	wxTextCtrl		*theLocationNameCtrl;
	wxTextCtrl		*theLocationPathCtrl;
	wxListCtrl 		*theExclusionList;
	wxChoice		*theExcludeTypeCtrl;
	wxTextCtrl		*theExcludePathCtrl;
	wxButton 		*theExcludeAddBtn;
	wxButton 		*theExcludeEditBtn;
	wxButton 		*theExcludeRemoveBtn;
	unsigned int theSelectedLocation;
	
	void OnBackupLocationClick (wxListEvent &event);
	void OnLocationExcludeClick(wxListEvent &event);
	void OnLocationCmdClick (wxCommandEvent &event);
	void OnLocationChange   (wxCommandEvent &event);
	void OnExcludeTypeChoice(wxCommandEvent &event);
	void OnExcludePathChange(wxCommandEvent &event);
	void OnExcludeCmdClick  (wxCommandEvent &event);
	void UpdateLocationCtrlEnabledState();
	void UpdateExcludeCtrlEnabledState();
	void PopulateLocationList();
	void PopulateExclusionList();
	void NotifyChange();
	
	long GetSelectedExcludeIndex();
	MyExcludeEntry* GetExcludeEntry();
	const MyExcludeType* GetExcludeTypeByName(const std::string& name);

	DECLARE_EVENT_TABLE()
};

#endif /* _BACKUPLOCATIONSPANEL_H */
