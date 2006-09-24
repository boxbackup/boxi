/***************************************************************************
 *            FunctionPanel.cc
 *
 *  Fri Dec 30 15:38:52 2005
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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 

#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/listbox.h>
#include <wx/button.h>

#include "main.h"
#include "FunctionPanel.h"

BEGIN_EVENT_TABLE(FunctionPanel, wxPanel)
	EVT_BUTTON(ID_Function_Source_Button, FunctionPanel::OnClickSourceButton)
	EVT_BUTTON(ID_Function_Start_Button,  FunctionPanel::OnClickStartButton)
	EVT_BUTTON(wxID_CANCEL,               FunctionPanel::OnClickCloseButton)
END_EVENT_TABLE()

FunctionPanel::FunctionPanel(
		const wxString&  rName,
		ClientConfig*    pConfig,
		ClientInfoPanel* pClientConfigPanel,
		MainFrame*       pMainFrame,
		wxWindow*        pParent,
		wxWindowID       WindowId)
: wxPanel(pParent, WindowId, wxDefaultPosition, wxDefaultSize, 
	wxTAB_TRAVERSAL, rName),
  mpConfig(pConfig),
  mpClientConfigPanel(pClientConfigPanel),
  mpMainFrame(pMainFrame)
{
	mpMainSizer = new wxBoxSizer(wxVERTICAL);

	mpSourceBox = new wxStaticBoxSizer(wxVERTICAL, this, wxT(""));
	mpMainSizer->Add(mpSourceBox, 1, wxGROW | wxALL, 8);
	
	mpSourceList = new wxListBox(this, ID_Function_Source_List);
	mpSourceBox->Add(mpSourceList, 1, wxGROW | wxALL, 8);
	
	mpSourceCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	mpSourceBox->Add(mpSourceCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpSourceEditButton = new wxButton(this, 
		ID_Function_Source_Button, wxT(""));
	mpSourceCtrlSizer->Add(mpSourceEditButton, 0, wxGROW, 0);
	
	mpDestBox = new wxStaticBoxSizer(wxVERTICAL, 
		this, wxT(""));
	mpMainSizer->Add(mpDestBox, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	wxSizer* pActionCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	mpMainSizer->Add(pActionCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpStartButton = new wxButton(this, ID_Function_Start_Button, wxT(""));
	pActionCtrlSizer->Add(mpStartButton, 0, wxGROW, 0);

	wxButton* pCloseButton = new wxButton(this, 
		wxID_CANCEL, wxT("Close"));
	pActionCtrlSizer->Add(pCloseButton, 0, wxGROW | wxLEFT, 8);

	SetSizer( mpMainSizer );
}

void FunctionPanel::OnClickCloseButton(wxCommandEvent& rEvent)
{
	Hide();
}
