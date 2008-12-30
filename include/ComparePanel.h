/***************************************************************************
 *            ComparePanel.h
 *
 *  Tue Mar  1 00:24:11 2005
 *  Copyright 2005-2008 Chris Wilson
 *  Email chris-boxisource@qwirx.com
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

#include <wx/filename.h>

#include "FunctionPanel.h"

class CompareProgressPanel;
class ServerConnection;

class wxChoice;
class wxFileName;
class wxGenericDirCtrl;
class wxNotebook;
class wxRadioButton;
class wxTreeCtrl;

class BoxiCompareParams
{
	public:
	BoxiCompareParams() { }
	
	private:
	BoxiCompareParams(const BoxiCompareParams& rToCopy) { /* forbidden */ }
	BoxiCompareParams& operator=(const BoxiCompareParams& rToCopy)
	{ return *this; /* forbidden */ }
};

class ComparePanel : public wxPanel, public ConfigChangeListener
{
	public:
	ComparePanel
	(
		ClientConfig*     pConfig,
		MainFrame*        pMainFrame,
		ServerConnection* pServerConnection,
		wxWindow*         pParent
	);
	
	void AddToNotebook(wxNotebook* pNotebook);
	
	private:	
	ServerConnection* mpServerConnection;
	ClientConfig*     mpConfig;
	MainFrame*        mpMainFrame;
	
	wxRadioButton* mpAllLocsRadio;
	wxRadioButton* mpOneLocRadio;
	wxRadioButton* mpDirRadio;
	
	wxChoice* mpOneLocChoice;
	
	wxGenericDirCtrl* mpDirLocalTree;
	wxTreeCtrl*       mpDirRemoteTree;
	
	/*CompareFilesPanel* mpFilesPanel;*/
	CompareProgressPanel* mpProgressPanel;

	void Update();
	void OnClickStartButton(wxCommandEvent& rEvent);
	void OnClickCloseButton(wxCommandEvent& rEvent);
	void OnClickRadioButton(wxCommandEvent& rEvent);
	void UpdateEnabledState();

	virtual void NotifyChange(); // ConfigChangeListener
		
	DECLARE_EVENT_TABLE()
};

#endif /* _COMPAREPANEL_H */
