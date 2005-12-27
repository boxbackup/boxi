/***************************************************************************
 *            ClientInfoPanel.h
 *
 *  Tue Mar  1 00:16:48 2005
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
 
#ifndef _CLIENTINFOPANEL_H
#define _CLIENTINFOPANEL_H

#include <wx/checkbox.h>

#include "ClientConfig.h"
#include "ParamPanel.h"
#include "Property.h"

class ClientInfoPanel : public wxPanel, public ConfigChangeListener {
	public:
	ClientInfoPanel(ClientConfig *config,
		wxWindow* parent, 
		wxWindowID id = -1,
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL, 
		const wxString& name = wxT("ClientInfoPanel"));
	~ClientInfoPanel() { }
	void Reload();
	
	private:
	ClientConfig*  mpConfig;
	TickCrossIcon* mpConfigStateBitmap;
	wxStaticText*  mpConfigStateLabel;
	
#define STR_PROP(name)                 BoundStringCtrl* mp ## name ## Ctrl;
#define STR_PROP_SUBCONF(name, parent) BoundStringCtrl* mp ## name ## Ctrl;
#define INT_PROP(name)                 BoundIntCtrl*    mp ## name ## Ctrl;
#define BOOL_PROP(name)                BoundBoolCtrl*   mp ## name ## Ctrl;
ALL_PROPS
#undef BOOL_PROP
#undef INT_PROP
#undef STR_PROP_SUBCONF
#undef STR_PROP
	
	void OnClickCloseButton(wxCommandEvent& rEvent);
	void NotifyChange();
	bool CheckConfig(wxString& rMsg);
	void UpdateConfigState();
	
	DECLARE_EVENT_TABLE()
};

#endif /* _CLIENTINFOPANEL_H */
