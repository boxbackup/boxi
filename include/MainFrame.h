/***************************************************************************
 *            MainFrame.h
 *
 *  Sun Jan 22 22:35:37 2006
 *  Copyright  2006  Chris Wilson
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
 
#ifndef _MAINFRAME_H
#define _MAINFRAME_H

#include <wx/wx.h>

#include "ClientConfig.h"

class ClientConfig;
class ServerConnection;
class ClientInfoPanel;
class wxMenu;
class wxNotebook;
class wxStatusBar;
class TestInterface;

class MainFrame : public wxFrame, public ConfigChangeListener 
{
	public:
	MainFrame
	(
		const wxString* pConfigFileName,
		const wxString& rBoxiExecutablePath,
		const wxPoint& pos, const wxSize& size, 
		long style = wxDEFAULT_FRAME_STYLE
	);
	void ShowPanel (wxPanel* pPanel);
	bool IsTopPanel(wxPanel* pPanel);
	ClientConfig* GetConfig() { return mpConfig; }
	
	private:
	void OnFileNew	  (wxCommandEvent& event);
	void OnFileOpen	  (wxCommandEvent& event);
	void OnFileSave	  (wxCommandEvent& event);
	void OnFileSaveAs (wxCommandEvent& event);
	void OnFileQuit	  (wxCommandEvent& event);
	void OnFileDir	  (wxCommandEvent& event);
	void OnViewOld    (wxCommandEvent& event);
	void OnViewDeleted(wxCommandEvent& event);
	void OnHelpAbout  (wxCommandEvent& event);
	void OnSize       (wxSizeEvent&	   event);
	void OnClose      (wxCloseEvent&   event);

	public:
	// don't want to expose this, but need to unit test it.
	void DoFileOpen   (const wxString& path);
	
	private:
	void DoFileNew    ();
	void DoFileSave   ();
	void DoFileSaveAs ();
	void DoFileSaveAs2();
	
	// implement ConfigChangeListener
	void NotifyChange();
	void UpdateTitle();
	
	wxStatusBar*      mpStatusBar;
	// wxString       mConfigFileName;
	ClientConfig*     mpConfig;
	ServerConnection* mpServerConnection;
	wxNotebook*       mpTopNotebook;
	ClientInfoPanel*  mpClientConfigPanel;
	wxMenu*           mpViewMenu;
	TestInterface*    mpTestInterface;

	DECLARE_EVENT_TABLE()
};	

#endif /* _MAINFRAME_H */
