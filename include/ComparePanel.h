/***************************************************************************
 *            ComparePanel.h
 *
 *  Tue Mar  1 00:24:11 2005
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
 
#ifndef _COMPAREPANEL_H
#define _COMPAREPANEL_H

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <wx/datetime.h>
 
#include "ClientConfig.h"
#include "ServerConnection.h"

class ComparePanel : public wxPanel {
	public:
	ComparePanel(
		ClientConfig *config,
		ServerConnection* pConnection,
		wxWindow* parent, wxWindowID id = -1,
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL, 
		const wxString& name = "Compare");
	
	private:
	ClientConfig*     mpConfig;
	ServerConnection* mpServerConnection;
	wxListCtrl*       mpCompareList;
	wxTextCtrl*       mpCompareThreadStateCtrl;

	void UpdateCompareThreadStateCtrl();
	
	DECLARE_EVENT_TABLE()
};

#endif /* _COMPAREPANEL_H */
