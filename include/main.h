/***************************************************************************
 *            main.h
 *
 *  Sun Feb 27 21:27:17 2005
 *  Copyright 2004-06 Chris Wilson
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
	ID_Main_Frame = wxID_HIGHEST + 1,
	ID_Test_Frame,

	ID_File_New,
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

	ID_General_Setup_Wizard_Button,
	ID_General_Setup_Advanced_Button,
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
	
	ID_Function_Source_Button,
	ID_Function_Dest_Button,
	ID_Function_Start_Button,
	
	ID_Setup_Wizard_Frame,
	ID_Setup_Wizard_Store_Hostname_Ctrl,
	ID_Setup_Wizard_Account_Number_Ctrl,
	ID_Setup_Wizard_New_File_Radio,
	ID_Setup_Wizard_Existing_File_Radio,
	ID_Setup_Wizard_File_Name_Text,
};

void AddParam(wxPanel* panel, const wxChar* label, wxWindow* editor, 
	bool growToFit, wxSizer *sizer);

#endif /* _MAIN_H */
