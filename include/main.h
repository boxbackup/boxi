/***************************************************************************
 *            main.h
 *
 *  Sun Feb 27 21:27:17 2005
 *  Copyright 2004-05  Chris Wilson
 *  boxi_main.h@qwirx.com
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
 
#ifndef _MAIN_H
#define _MAIN_H

// #include <stdlib.h>
// #include "dmalloc.h"

#include <wx/wx.h>

#define BOXI_VERSION "0.1.1"

// Define these four macros (STR_PROP, STR_PROP_SUBCONF, INT_PROP and BOOL_PROP)
// and then use the ALL_PROPS macro, to insert code for every configuration 
// property known (all those listed below).

#include "Property.h"

enum {
	ID_File_New = wxID_HIGHEST + 1,
	ID_File_Open,
	ID_File_Save,
	ID_File_SaveAs,
	ID_View_Old,
	ID_View_Deleted,
	
	ID_Top_Notebook,
	ID_Backup_Files_Panel,
	ID_Backup_Files_Tree,
	ID_Restore_Files_Panel,
	ID_Client_Panel,
	ID_Backup_Panel,
	ID_Backup_Progress_Panel,

	// IDs for the controls that manipulate properties	
	#define STR_PROP(name)	ID_ClientProp_ ## name,
	#define STR_PROP_SUBCONF(name, subconf) ID_ClientProp_ ## name,
	#define INT_PROP(name)  ID_ClientProp_ ## name,
	#define BOOL_PROP(name) ID_ClientProp_ ## name,
	ALL_PROPS
	#undef BOOL_PROP
	#undef INT_PROP
	#undef STR_PROP
	#undef STR_PROP_SUBCONF

	ID_General_Backup_Button,
	ID_General_Restore_Button,
	ID_General_Compare_Button,

	ID_Backup_Start_Button,
	ID_Backup_Locations_Button,
	ID_Backup_Config_Button,

	ID_Backup_Locations_Tree,
	ID_Backup_LocationsAddButton,
	ID_Backup_LocationsEditButton,
	ID_Backup_LocationsDelButton,
	ID_Backup_LocationsList,
	ID_Backup_LocationNameCtrl,
	ID_Backup_LocationPathCtrl,
	ID_BackupLoc_ExcludeTypeList,
	ID_BackupLoc_ExcludePathCtrl,
	ID_BackupLoc_ExcludeAddButton,
	ID_BackupLoc_ExcludeEditButton,
	ID_BackupLoc_ExcludeRemoveButton,
	ID_BackupLoc_ExcludeList,
	ID_Server_Splitter,
	ID_Server_File_Tree,
	ID_Server_File_List,
	ID_Server_File_RestoreButton,
	ID_Server_File_CompareButton,
	ID_Server_File_DeleteButton,
	
	ID_Local_ServerConnectCheckbox,
	
	ID_Daemon_Connect_Timer,
	ID_Daemon_Start,
	ID_Daemon_Stop,
	ID_Daemon_Kill,
	ID_Daemon_Restart,
	ID_Daemon_Reload,
	ID_Daemon_Sync,
	
	ID_Compare_List,
	ID_Compare_Button,
};

void AddParam(wxPanel* panel, const wxChar* label, wxWindow* editor, 
	bool growToFit, wxSizer *sizer);

class ConfigChangeListener {
	public:
	virtual ~ConfigChangeListener() { }
	virtual void NotifyChange() { }
};

class ClientConfig;
class ServerConnection;
class BackupFilesPanel;
class ClientInfoPanel;
class RestorePanel;

class MainFrame : public wxFrame, public ConfigChangeListener {
	public:
	MainFrame(
		const wxString* pConfigFileName,
		const wxString& rBoxiExecutablePath,
		const wxPoint& pos, const wxSize& size, 
		long style = wxDEFAULT_FRAME_STYLE);

	void ShowPanel(wxPanel* pPanel);
	
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
	void OnSize		  (wxSizeEvent&	   event);
	void OnClose      (wxCloseEvent&   event);

	void DoFileOpen   (const wxString& path);
	void DoFileNew    ();
	void DoFileSave   ();
	void DoFileSaveAs ();
	void DoFileSaveAs2();
	
	// implement ConfigChangeListener
	void NotifyChange();
	void UpdateTitle();
	
	wxStatusBar*        mpStatusBar;
	wxString            mConfigFileName;
	ClientConfig*       mpConfig;
	ServerConnection*   mpServerConnection;
	
	wxNotebook*         mpTopNotebook;
	BackupFilesPanel*   mpBackupFilesPanel;
	ClientInfoPanel*    mpClientConfigPanel;
	wxPanel*            mpLocationsPanel;
	RestorePanel*       mpRestorePanel;
	wxMenu*             mpViewMenu;
	
	DECLARE_EVENT_TABLE()
};	

#endif /* _MAIN_H */
