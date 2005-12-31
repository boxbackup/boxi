/***************************************************************************
 *            FunctionPanel.h
 *
 *  Fri Dec 30 15:39:30 2005
 *  Copyright  2005  Chris Wilson
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
 
#ifndef _FUNCTIONPANEL_H
#define _FUNCTIONPANEL_H

#include <wx/wx.h>

#include "ClientConfig.h"
class ClientInfoPanel;
class MainFrame;

class FunctionPanel 
: public wxPanel, public ConfigChangeListener 
{
	public:
	FunctionPanel(
		const wxString&  rName,
		ClientConfig*    pConfig,
		ClientInfoPanel* pClientConfigPanel,
		MainFrame*       pMainFrame,
		wxWindow*        pParent);

	protected:
	ClientConfig*     mpConfig;
	ClientInfoPanel*  mpClientConfigPanel;
	MainFrame*        mpMainFrame;
	wxSizer*          mpMainSizer;
	wxStaticBoxSizer* mpSourceBox;
	wxListBox*        mpSourceList;
	wxSizer*          mpSourceCtrlSizer;
	wxButton*         mpSourceEditButton;
	wxStaticBoxSizer* mpDestBox;
	wxStaticText*     mpDestLabel;
	wxSizer*          mpDestCtrlSizer;
	wxButton*         mpDestEditButton;
	wxButton*         mpStartButton;
	
	void NotifyChange() { Update(); }
	virtual void Update() = 0;
	virtual void OnClickSourceButton(wxCommandEvent& rEvent) = 0;
	virtual void OnClickDestButton  (wxCommandEvent& rEvent) = 0;
	virtual void OnClickStartButton (wxCommandEvent& rEvent) = 0;
	virtual void OnClickCloseButton (wxCommandEvent& rEvent);
	
	DECLARE_EVENT_TABLE()
};

#endif /* _FUNCTIONPANEL_H */
