/***************************************************************************
 *            ComparePanel.cc
 *
 *  Tue Mar  1 00:24:16 2005
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

// #include <wx/arrstr.h>
#include <wx/dir.h>
#include <wx/filename.h>

#include "ComparePanel.h"

BEGIN_EVENT_TABLE(ComparePanel, wxPanel)
END_EVENT_TABLE()

ComparePanel::ComparePanel(
	ClientConfig *pConfig,
	ServerConnection* pServerConnection,
	wxWindow* parent, 
	wxWindowID id,
	const wxPoint& pos, 
	const wxSize& size,
	long style, 
	const wxString& name)
	: wxPanel(parent, id, pos, size, style, name)
{
	mpConfig = pConfig;
	mpServerConnection = pServerConnection;
	
	wxSizer* pMainSizer = new wxBoxSizer( wxVERTICAL );
	
	wxSizer* pParamSizer = new wxGridSizer(2, 4, 4);
	pMainSizer->Add(pParamSizer, 0, wxGROW | wxALL, 8);
	
	mpCompareThreadStateCtrl = new wxTextCtrl(this, -1, wxT(""));
	::AddParam(this, wxT("Current state:"), mpCompareThreadStateCtrl,
		false, pParamSizer);

	wxSizer* pButtonSizer = new wxBoxSizer(wxHORIZONTAL);
	pMainSizer->Add(pButtonSizer, 0, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	wxButton* pCompareButton = new wxButton(this, ID_Compare_Button,
		wxT("Compare"));
	pButtonSizer->Add(pCompareButton);

	mpCompareList = new wxListCtrl(this, ID_Compare_List, wxDefaultPosition,
		wxDefaultSize, wxLC_REPORT | wxSUNKEN_BORDER);
	pMainSizer->Add(mpCompareList, 1, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	SetSizer( pMainSizer );
	pMainSizer->SetSizeHints( this );
		
	UpdateCompareThreadStateCtrl();
}

void ComparePanel::UpdateCompareThreadStateCtrl()
{
}
