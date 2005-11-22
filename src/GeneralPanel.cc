/***************************************************************************
 *            GeneralPanel.cc
 *
 *  Mon Apr  4 20:36:25 2005
 *  Copyright  2005  Chris Wilson
 *  Email <boxi_GeneralPanel.cc@qwirx.com>
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
 *
 * Contains software developed by Ben Summers.
 * YOU MUST NOT REMOVE THIS ATTRIBUTION!
 */

#include "main.h"
#include "GeneralPanel.h"

BEGIN_EVENT_TABLE(GeneralPanel, wxPanel)
//EVT_BUTTON(ID_Backup_Panel_Start_Button, GeneralPanel::OnClickStartButton)
END_EVENT_TABLE()

GeneralPanel::GeneralPanel(
	wxWindow* parent, 
	wxWindowID id,
	const wxPoint& pos, 
	const wxSize& size,
	long style, 
	const wxString& name)
	: wxPanel(parent, id, pos, size, style, name)
{
	wxSizer* pMainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(pMainSizer);

	/* backup */
	
	wxStaticBoxSizer* pBackupBox = new wxStaticBoxSizer(wxHORIZONTAL,
		this, wxT("Backup"));
	pMainSizer->Add(pBackupBox, 0, wxGROW | wxALL, 8);
	
	wxStaticText* pBackupText = new wxStaticText(this, wxID_ANY, 
		wxT("Copy files from this computer to the backup server"));
	pBackupBox->Add(pBackupText, 1, wxALIGN_CENTER_VERTICAL | wxALL, 8);
	
	wxButton* pBackupButton = new wxButton(this, wxID_ANY, wxT("&Backup"));
	pBackupBox->Add(pBackupButton, 0, 
		wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 8);

	/* restore */
	
	wxStaticBoxSizer* pRestoreBox = new wxStaticBoxSizer(wxHORIZONTAL,
		this, wxT("Restore"));
	pMainSizer->Add(pRestoreBox, 0, wxGROW | wxALL, 8);
	
	wxStaticText* pRestoreText = new wxStaticText(this, wxID_ANY, 
		wxT("Copy files from the backup server to this computer"));
	pRestoreBox->Add(pRestoreText, 1, wxALIGN_CENTER_VERTICAL | wxALL, 8);
	
	wxButton* pRestoreButton = new wxButton(this, wxID_ANY, wxT("&Restore"));
	pRestoreBox->Add(pRestoreButton, 0, 
		wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 8);

	/* compare */
	
	wxStaticBoxSizer* pCompareBox = new wxStaticBoxSizer(wxHORIZONTAL,
		this, wxT("Compare"));
	pMainSizer->Add(pCompareBox, 0, wxGROW | wxALL, 8);
	
	wxStaticText* pCompareText = new wxStaticText(this, wxID_ANY, 
		wxT("Compare files on the backup server with this computer"));
	pCompareBox->Add(pCompareText, 1, wxALIGN_CENTER_VERTICAL | wxALL, 8);
	
	wxButton* pCompareButton = new wxButton(this, wxID_ANY, wxT("&Compare"));
	pCompareBox->Add(pCompareButton, 0, 
		wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 8);

}
