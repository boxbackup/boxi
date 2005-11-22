/***************************************************************************
 *            GeneralPanel.h
 *
 *  Mon Nov 21, 2005
 *  Copyright  2005  Chris Wilson
 *  Email <boxi_GeneralPanel.h@qwirx.com>
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
#include <wx/thread.h>

#include "ClientConfig.h"
#include "ClientConnection.h"
#include "BackupProgressPanel.h"

/** 
 * GeneralPanel
 * Allows the user to configure and start an interactive backup
 */

class GeneralPanel : public wxPanel
{
	public:
	GeneralPanel(
		wxWindow* parent, wxWindowID id = wxID_ANY,
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL, 
		const wxString& name = wxT("General Panel"));

	private:
	DECLARE_EVENT_TABLE()
};

#endif /* _GENERAL_PANEL_H */
