/***************************************************************************
 *            TestBackupConfig.cc
 *
 *  Wed May 10 23:10:16 2006
 *  Copyright 2006 Chris Wilson
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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <openssl/ssl.h>

#include <wx/button.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/treectrl.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <cppunit/TestSuite.h>
#include <cppunit/extensions/HelperMacros.h>

#define TLS_CLASS_IMPLEMENTATION_CPP

#include "SandBox.h"
#include "Box.h"
#include "BackupClientRestore.h"
#include "BackupStoreDaemon.h"
#include "RaidFileController.h"
#include "BackupStoreAccountDatabase.h"
#include "BackupStoreAccounts.h"
#include "BackupContext.h"
#include "autogen_BackupProtocolServer.h"
#include "BackupStoreConfigVerify.h"
#include "BackupQueries.h"
#include "BackupDaemonConfigVerify.h"
#include "BackupClientCryptoKeys.h"
#include "BackupStoreConstants.h"
#include "BackupStoreInfo.h"
#include "StoreStructure.h"
#include "NamedLock.h"

#include "main.h"
#include "BoxiApp.h"
#include "ClientConfig.h"
#include "MainFrame.h"
#include "SetupWizard.h"
#include "SslConfig.h"
#include "TestBackupConfig.h"
#include "FileTree.h"

#include "ServerConnection.h"

#undef TLS_CLASS_IMPLEMENTATION_CPP

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestBackupConfig, "WxGuiTest");

CppUnit::Test *TestBackupConfig::suite()
{
	CppUnit::TestSuite *suiteOfTests =
		new CppUnit::TestSuite("TestBackupConfig");
	suiteOfTests->addTest(
		new CppUnit::TestCaller<TestBackupConfig>(
			"TestBackupConfig",
			&TestBackupConfig::RunTest));
	return suiteOfTests;
}

wxFileName MakeAbsolutePath(wxFileName base, wxString relativePath)
{
	wxFileName outName(base.GetFullPath(), wxEmptyString);
	
	for (int index = relativePath.Find('/'); index != -1; 
		index = relativePath.Find('/'))
	{
		CPPUNIT_ASSERT(index > 0);
		outName.AppendDir(relativePath.Mid(0, index));
		relativePath = relativePath.Mid(index + 1);
	}
		
	outName.SetFullName(relativePath);	
	CPPUNIT_ASSERT(outName.IsOk());

	return outName;	
}

wxFileName MakeAbsolutePath(wxString baseDir, wxString relativePath)
{
	return MakeAbsolutePath(wxFileName(baseDir), relativePath);
}

wxFileName MakeAbsolutePath(wxString relativePath)
{
	return MakeAbsolutePath(wxFileName::GetCwd(), relativePath);
}

void DeleteRecursive(const wxFileName& rPath)
{
	if (rPath.FileExists())
	{
		CPPUNIT_ASSERT(wxRemoveFile(rPath.GetFullPath()));
	}
	else if (rPath.DirExists())
	{
		wxDir dir(rPath.GetFullPath());
		CPPUNIT_ASSERT(dir.IsOpened());
		
		wxString file;
		if (dir.GetFirst(&file))
		{
			do
			{
				wxFileName filename(rPath.GetFullPath(), file);
				DeleteRecursive(filename);
			}
			while (dir.GetNext(&file));
		}
		
		CPPUNIT_ASSERT(wxRmdir(rPath.GetFullPath()));
	}
}

void TestBackupConfig::RunTest()
{
	// create a working directory
	wxFileName tempDir;
	tempDir.AssignTempFileName(wxT("boxi-tempdir-"));
	CPPUNIT_ASSERT(wxRemoveFile(tempDir.GetFullPath()));
	CPPUNIT_ASSERT(wxMkdir     (tempDir.GetFullPath(), 0700));
	CPPUNIT_ASSERT(tempDir.DirExists());
	
	MainFrame* pMainFrame = GetMainFrame();
	CPPUNIT_ASSERT(pMainFrame);
	
	mpConfig = pMainFrame->GetConfig();
	CPPUNIT_ASSERT(mpConfig);
	
	pMainFrame->DoFileOpen(MakeAbsolutePath(
		_("../test/config/bbackupd.conf")).GetFullPath());
	wxString msg;
	bool isOk = mpConfig->Check(msg);

	if (!isOk)
	{
		wxCharBuffer buf = msg.mb_str(wxConvLibc);
		CPPUNIT_ASSERT_MESSAGE(buf.data(), isOk);
	}
		
	wxPanel* pBackupPanel = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_Backup_Panel), wxPanel
	);
	CPPUNIT_ASSERT(pBackupPanel);

	CPPUNIT_ASSERT(!pBackupPanel->IsShown());	
	ClickButtonWaitEvent(ID_Main_Frame, ID_General_Backup_Button);
	CPPUNIT_ASSERT(pBackupPanel->IsShown());

	wxPanel* pBackupFilesPanel = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_Backup_Files_Panel), wxPanel
	);
	CPPUNIT_ASSERT(pBackupFilesPanel);
	
	CPPUNIT_ASSERT(!pBackupFilesPanel->IsShown());	
	ClickButtonWaitEvent(ID_Main_Frame, ID_Function_Source_Button);
	CPPUNIT_ASSERT(pBackupFilesPanel->IsShown());

	wxTreeCtrl* pTree = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_Backup_Locations_Tree), wxTreeCtrl
	);
	CPPUNIT_ASSERT(pTree);

	wxTreeItemId rootId = pTree->GetRootItem();
	wxString rootLabel  = pTree->GetItemText(rootId);
	CPPUNIT_ASSERT_EQUAL((std::string)"/ (local root)", 
		(std::string)rootLabel.mb_str(wxConvLibc).data());
	
	FileImageList images;
	CPPUNIT_ASSERT_EQUAL(images.GetEmptyImageId(), 
		pTree->GetItemImage(rootId));
		
	wxListBox* pLocationsListBox = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_Backup_LocationsList), wxListBox
	);
	CPPUNIT_ASSERT(pLocationsListBox);
	CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox->GetCount());
	
	wxChoice* pExcludeLocsListBox = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_BackupLoc_ExcludeLocList), wxChoice
	);
	CPPUNIT_ASSERT(pExcludeLocsListBox);
	CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetCount());
	
	wxPanel* pLocationsPanel = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_BackupLoc_List_Panel), wxPanel
	);
	CPPUNIT_ASSERT(pLocationsPanel);
	
	wxTextCtrl* pLocationNameCtrl = wxDynamicCast
	(
		pLocationsPanel->FindWindow(ID_Backup_LocationNameCtrl), wxTextCtrl
	);
	CPPUNIT_ASSERT(pLocationNameCtrl);
	CPPUNIT_ASSERT_EQUAL((wxString)wxEmptyString, 
		pLocationNameCtrl->GetValue());

	wxTextCtrl* pLocationPathCtrl = wxDynamicCast
	(
		pLocationsPanel->FindWindow(ID_Backup_LocationPathCtrl), wxTextCtrl
	);
	CPPUNIT_ASSERT(pLocationPathCtrl);
	CPPUNIT_ASSERT_EQUAL((wxString)wxEmptyString, 
		pLocationPathCtrl->GetValue());	

	wxButton* pLocationAddButton = wxDynamicCast
	(
		pLocationsPanel->FindWindow(ID_Backup_LocationsAddButton), wxButton
	);
	CPPUNIT_ASSERT(pLocationAddButton);

	wxButton* pLocationEditButton = wxDynamicCast
	(
		pLocationsPanel->FindWindow(ID_Backup_LocationsEditButton), wxButton
	);
	CPPUNIT_ASSERT(pLocationEditButton);

	wxButton* pLocationDelButton = wxDynamicCast
	(
		pLocationsPanel->FindWindow(ID_Backup_LocationsDelButton), wxButton
	);
	CPPUNIT_ASSERT(pLocationDelButton);

	wxPanel* pExcludePanel = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_BackupLoc_Excludes_Panel), wxPanel
	);
	CPPUNIT_ASSERT(pExcludePanel);
	
	wxListBox* pExcludeListBox = wxDynamicCast
	(
		pExcludePanel->FindWindow(ID_Backup_LocationsList), wxListBox
	);
	CPPUNIT_ASSERT(pExcludeListBox);
	CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetCount());

	wxChoice* pExcludeTypeList = wxDynamicCast
	(
		pExcludePanel->FindWindow(ID_BackupLoc_ExcludeTypeList), wxChoice
	);
	CPPUNIT_ASSERT(pExcludeTypeList);
	CPPUNIT_ASSERT_EQUAL(8, pExcludeTypeList->GetCount());

	wxTextCtrl* pExcludePathCtrl = wxDynamicCast
	(
		pExcludePanel->FindWindow(ID_BackupLoc_ExcludePathCtrl), wxTextCtrl
	);
	CPPUNIT_ASSERT(pExcludePathCtrl);
	CPPUNIT_ASSERT_EQUAL((wxString)wxEmptyString, 
		pExcludePathCtrl->GetValue());

	wxButton* pExcludeAddButton = wxDynamicCast
	(
		pExcludePanel->FindWindow(ID_Backup_LocationsAddButton), wxButton
	);
	CPPUNIT_ASSERT(pExcludeAddButton);

	wxButton* pExcludeEditButton = wxDynamicCast
	(
		pExcludePanel->FindWindow(ID_Backup_LocationsEditButton), wxButton
	);
	CPPUNIT_ASSERT(pExcludeEditButton);

	wxButton* pExcludeDelButton = wxDynamicCast
	(
		pExcludePanel->FindWindow(ID_Backup_LocationsDelButton), wxButton
	);
	CPPUNIT_ASSERT(pExcludeDelButton);

	// test that creating a location using the tree adds entries 
	// to the locations and excludes list boxes
	ActivateTreeItemWaitEvent(pTree, rootId);
	CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/ -> root"), pLocationsListBox  ->GetString(0));
	CPPUNIT_ASSERT_EQUAL((wxString)_("/ -> root"), pExcludeLocsListBox->GetString(0));

	MessageBoxSetResponse(BM_BACKUP_FILES_DELETE_LOCATION_QUESTION, wxNO);
	ActivateTreeItemWaitEvent(pTree, rootId);
	MessageBoxCheckFired();
	CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetCount());

	MessageBoxSetResponse(BM_BACKUP_FILES_DELETE_LOCATION_QUESTION, wxYES);
	ActivateTreeItemWaitEvent(pTree, rootId);
	MessageBoxCheckFired();
	CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetCount());

	wxFileName testDataDir(tempDir.GetFullPath(), _("testdata"));
	CPPUNIT_ASSERT(testDataDir.Mkdir(0700));
	
	// will be changed in the block below
	wxTreeItemId testDataDirItem = rootId;

	{
		wxFileName testDepth1Dir(testDataDir.GetFullPath(), _("depth1"));
		CPPUNIT_ASSERT(testDepth1Dir.Mkdir(0700));
		wxFileName testDepth2Dir(testDepth1Dir.GetFullPath(), _("depth2"));
		CPPUNIT_ASSERT(testDepth2Dir.Mkdir(0700));
		wxFileName testDepth3Dir(testDepth2Dir.GetFullPath(), _("depth3"));
		CPPUNIT_ASSERT(testDepth3Dir.Mkdir(0700));
		wxFileName testDepth4Dir(testDepth3Dir.GetFullPath(), _("depth4"));
		CPPUNIT_ASSERT(testDepth4Dir.Mkdir(0700));
		wxFileName testDepth5Dir(testDepth4Dir.GetFullPath(), _("depth5"));
		CPPUNIT_ASSERT(testDepth5Dir.Mkdir(0700));
		wxFileName testDepth6Dir(testDepth5Dir.GetFullPath(), _("depth6"));
		CPPUNIT_ASSERT(testDepth6Dir.Mkdir(0700));
		
		wxArrayString testDataDirPathDirs = testDataDir.GetDirs();
		testDataDirPathDirs.Add(testDataDir.GetName());
		
		for (size_t i = 0; i < testDataDirPathDirs.Count(); i++)
		{
			wxString dirName = testDataDirPathDirs.Item(i);
			
			if (!pTree->IsExpanded(testDataDirItem))
			{
				pTree->Expand(testDataDirItem);
			}
			
			bool found = false;
			wxTreeItemIdValue cookie;
			wxTreeItemId child;
			
			for (child = pTree->GetFirstChild(testDataDirItem, cookie);
				child.IsOk(); child = pTree->GetNextChild(testDataDirItem, cookie))
			{
				if (pTree->GetItemText(child).IsSameAs(dirName))
				{
					testDataDirItem = child;
					found = true;
					break;
				}
			}
			
			CPPUNIT_ASSERT_MESSAGE(dirName.mb_str(wxConvLibc).data(), found);
		}
		
		wxTreeItemIdValue cookie;
		
		pTree->Expand(testDataDirItem);
		CPPUNIT_ASSERT_EQUAL((size_t)1, 
			pTree->GetChildrenCount(testDataDirItem, false));
		wxTreeItemId depth1 = pTree->GetFirstChild(testDataDirItem, cookie);
		CPPUNIT_ASSERT(depth1.IsOk());
		
		pTree->Expand(depth1);
		CPPUNIT_ASSERT_EQUAL((size_t)1, 
			pTree->GetChildrenCount(depth1, false));
		wxTreeItemId depth2 = pTree->GetFirstChild(depth1, cookie);
		CPPUNIT_ASSERT(depth2.IsOk());
		
		pTree->Expand(depth2);
		CPPUNIT_ASSERT_EQUAL((size_t)1, 
			pTree->GetChildrenCount(depth2, false));
		wxTreeItemId depth3 = pTree->GetFirstChild(depth2, cookie);
		CPPUNIT_ASSERT(depth3.IsOk());
		
		pTree->Expand(depth3);
		CPPUNIT_ASSERT_EQUAL((size_t)1, 
			pTree->GetChildrenCount(depth3, false));
		wxTreeItemId depth4 = pTree->GetFirstChild(depth3, cookie);		
		CPPUNIT_ASSERT(depth4.IsOk());

		pTree->Expand(depth4);
		CPPUNIT_ASSERT_EQUAL((size_t)1, 
			pTree->GetChildrenCount(depth4, false));
		wxTreeItemId depth5 = pTree->GetFirstChild(depth4, cookie);		
		CPPUNIT_ASSERT(depth5.IsOk());

		pTree->Expand(depth5);
		CPPUNIT_ASSERT_EQUAL((size_t)1, 
			pTree->GetChildrenCount(depth5, false));
		wxTreeItemId depth6 = pTree->GetFirstChild(depth5, cookie);		
		CPPUNIT_ASSERT(depth6.IsOk());

		{
			// activate the testdata dir node, check that it
			// and its children are shown as included in the tree,
			// and all parent nodes are shown as partially included
			ActivateTreeItemWaitEvent(pTree, testDataDirItem);

			CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(), 
				pTree->GetItemImage(testDataDirItem));
	
			for (wxTreeItemId nodeId = pTree->GetItemParent(testDataDirItem);
				nodeId.IsOk(); nodeId = pTree->GetItemParent(nodeId))
			{
				CPPUNIT_ASSERT_EQUAL(images.GetPartialImageId(), 
					pTree->GetItemImage(nodeId));
			}
			
			for (wxTreeItemId nodeId = depth6; 
				!(nodeId == testDataDirItem); 
				nodeId = pTree->GetItemParent(nodeId))
			{
				CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(), 
					pTree->GetItemImage(nodeId));
			}
			
			// check that the entry details are shown in the
			// Locations panel, and the entry is selected
			CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox  ->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox  ->GetSelection());
			CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());
			wxString expectedTitle = testDataDir.GetFullPath();
			expectedTitle.Append(_(" -> testdata"));
			CPPUNIT_ASSERT_EQUAL(expectedTitle, 
				pLocationsListBox  ->GetString(0));
			CPPUNIT_ASSERT_EQUAL(expectedTitle,
				pExcludeLocsListBox->GetString(0));
			CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL((wxString)_("testdata"), 
				pLocationNameCtrl->GetValue());
			CPPUNIT_ASSERT_EQUAL(testDataDir.GetFullPath(),
				pLocationPathCtrl->GetValue());
		}
		
		{
			// activate a node at depth2, check that it and
			// its children are Excluded
			ActivateTreeItemWaitEvent(pTree, depth2);
			CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox  ->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox  ->GetSelection());
			CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());
			wxString expectedTitle = testDataDir.GetFullPath();
			expectedTitle.Append(_(" -> testdata (ExcludeDir = "));
			expectedTitle.Append(testDepth2Dir.GetFullPath());
			expectedTitle.Append(_(")"));
			CPPUNIT_ASSERT_EQUAL(expectedTitle, 
				pLocationsListBox  ->GetString(0));
			CPPUNIT_ASSERT_EQUAL(expectedTitle,
				pExcludeLocsListBox->GetString(0));
			CPPUNIT_ASSERT_EQUAL(1, pExcludeListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(), 
				pTree->GetItemImage(testDataDirItem));
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(), 
				pTree->GetItemImage(depth1));
			CPPUNIT_ASSERT_EQUAL(images.GetCrossedImageId(), 
				pTree->GetItemImage(depth2));
			for (wxTreeItemId nodeId = depth6; !(nodeId == depth2); 
				nodeId = pTree->GetItemParent(nodeId))
			{
				CPPUNIT_ASSERT_EQUAL(images.GetCrossedGreyImageId(), 
					pTree->GetItemImage(nodeId));
			}
		}

		{
			// activate node at depth 4, check that it and its
			// children are AlwaysIncluded
			ActivateTreeItemWaitEvent(pTree, depth4);
			CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox  ->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox  ->GetSelection());
			CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());
			wxString expectedTitle = testDataDir.GetFullPath();
			expectedTitle.Append(_(" -> testdata (ExcludeDir = "));
			expectedTitle.Append(testDepth2Dir.GetFullPath());
			expectedTitle.Append(_(", AlwaysIncludeDir = "));
			expectedTitle.Append(testDepth4Dir.GetFullPath());
			expectedTitle.Append(_(")"));
			CPPUNIT_ASSERT_EQUAL(expectedTitle, 
				pLocationsListBox  ->GetString(0));
			CPPUNIT_ASSERT_EQUAL(expectedTitle,
				pExcludeLocsListBox->GetString(0));
			CPPUNIT_ASSERT_EQUAL(2, pExcludeListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(), 
				pTree->GetItemImage(testDataDirItem));
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(), 
				pTree->GetItemImage(depth1));
			CPPUNIT_ASSERT_EQUAL(images.GetCrossedImageId(), 
				pTree->GetItemImage(depth2));
			CPPUNIT_ASSERT_EQUAL(images.GetCrossedGreyImageId(), 
				pTree->GetItemImage(depth3));
			CPPUNIT_ASSERT_EQUAL(images.GetAlwaysImageId(), 
				pTree->GetItemImage(depth4));
			CPPUNIT_ASSERT_EQUAL(images.GetAlwaysGreyImageId(), 
				pTree->GetItemImage(depth5));
			CPPUNIT_ASSERT_EQUAL(images.GetAlwaysGreyImageId(), 
				pTree->GetItemImage(depth6));
		}

		{
			// activate node at depth 2, check that it is no longer
			// excluded, but children at depth 4 and below are still
			// AlwaysIncluded
			ActivateTreeItemWaitEvent(pTree, depth2);
			CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox  ->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox  ->GetSelection());
			CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());
			wxString expectedTitle = testDataDir.GetFullPath();
			expectedTitle.Append(_(" -> testdata (AlwaysIncludeDir = "));
			expectedTitle.Append(testDepth4Dir.GetFullPath());
			expectedTitle.Append(_(")"));
			CPPUNIT_ASSERT_EQUAL(expectedTitle, 
				pLocationsListBox  ->GetString(0));
			CPPUNIT_ASSERT_EQUAL(expectedTitle,
				pExcludeLocsListBox->GetString(0));
			CPPUNIT_ASSERT_EQUAL(1, pExcludeListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(), 
				pTree->GetItemImage(testDataDirItem));
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(), 
				pTree->GetItemImage(depth1));
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(), 
				pTree->GetItemImage(depth2));
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(), 
				pTree->GetItemImage(depth3));
			CPPUNIT_ASSERT_EQUAL(images.GetAlwaysImageId(), 
				pTree->GetItemImage(depth4));
			CPPUNIT_ASSERT_EQUAL(images.GetAlwaysGreyImageId(), 
				pTree->GetItemImage(depth5));
			CPPUNIT_ASSERT_EQUAL(images.GetAlwaysGreyImageId(), 
				pTree->GetItemImage(depth6));
		}
		
		{
			// activate node at depth 4, check that the 
			// AlwaysInclude entry is removed.
			ActivateTreeItemWaitEvent(pTree, depth4);
			CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox  ->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox  ->GetSelection());
			CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());
			wxString expectedTitle = testDataDir.GetFullPath();
			expectedTitle.Append(_(" -> testdata"));
			CPPUNIT_ASSERT_EQUAL(expectedTitle, 
				pLocationsListBox  ->GetString(0));
			CPPUNIT_ASSERT_EQUAL(expectedTitle,
				pExcludeLocsListBox->GetString(0));
			CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(), 
				pTree->GetItemImage(testDataDirItem));
	
			for (wxTreeItemId nodeId = depth6; 
				!(nodeId == testDataDirItem); 
				nodeId = pTree->GetItemParent(nodeId))
			{
				CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(), 
					pTree->GetItemImage(nodeId));
			}
		}

		{
			// activate node at depth 4 again, check that an
			// Exclude entry is added this time.
			ActivateTreeItemWaitEvent(pTree, depth4);
			CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox  ->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox  ->GetSelection());
			CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());

			wxString expectedTitle = testDataDir.GetFullPath();
			expectedTitle.Append(_(" -> testdata (ExcludeDir = "));
			expectedTitle.Append(testDepth4Dir.GetFullPath());
			expectedTitle.Append(_(")"));
			CPPUNIT_ASSERT_EQUAL(expectedTitle, 
				pLocationsListBox  ->GetString(0));
			CPPUNIT_ASSERT_EQUAL(expectedTitle,
				pExcludeLocsListBox->GetString(0));

			CPPUNIT_ASSERT_EQUAL(1, pExcludeListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());

			for (wxTreeItemId nodeId = depth6; !(nodeId == depth4); 
				nodeId = pTree->GetItemParent(nodeId))
			{
				CPPUNIT_ASSERT_EQUAL(images.GetCrossedGreyImageId(), 
					pTree->GetItemImage(nodeId));
			}
			
			CPPUNIT_ASSERT_EQUAL(images.GetCrossedImageId(), 
				pTree->GetItemImage(depth4));
			
			for (wxTreeItemId nodeId = depth3; !(nodeId == testDataDirItem); 
				nodeId = pTree->GetItemParent(nodeId))
			{
				CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(), 
					pTree->GetItemImage(nodeId));
			}
			
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(), 
				pTree->GetItemImage(testDataDirItem));
		}

		{
			// activate node at depth 2 again, check that an
			// Exclude entry is added again, and that the node
			// at depth 4 still shows as excluded
			ActivateTreeItemWaitEvent(pTree, depth2);
			CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox  ->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox  ->GetSelection());
			CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());

			wxString expectedTitle = testDataDir.GetFullPath();
			expectedTitle.Append(_(" -> testdata (ExcludeDir = "));
			expectedTitle.Append(testDepth4Dir.GetFullPath());
			expectedTitle.Append(_(", ExcludeDir = "));
			expectedTitle.Append(testDepth2Dir.GetFullPath());
			expectedTitle.Append(_(")"));
			CPPUNIT_ASSERT_EQUAL(expectedTitle, 
				pLocationsListBox  ->GetString(0));
			CPPUNIT_ASSERT_EQUAL(expectedTitle,
				pExcludeLocsListBox->GetString(0));

			CPPUNIT_ASSERT_EQUAL(2, pExcludeListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());

			CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(), 
				pTree->GetItemImage(testDataDirItem));
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(), 
				pTree->GetItemImage(depth1));
			CPPUNIT_ASSERT_EQUAL(images.GetCrossedImageId(), 
				pTree->GetItemImage(depth2));
			CPPUNIT_ASSERT_EQUAL(images.GetCrossedGreyImageId(), 
				pTree->GetItemImage(depth3));
			CPPUNIT_ASSERT_EQUAL(images.GetCrossedImageId(), 
				pTree->GetItemImage(depth4));
			CPPUNIT_ASSERT_EQUAL(images.GetCrossedGreyImageId(), 
				pTree->GetItemImage(depth5));
			CPPUNIT_ASSERT_EQUAL(images.GetCrossedGreyImageId(), 
				pTree->GetItemImage(depth6));
		}
		
		{
			// remove Exclude and Location entries, check that
			// all is reset.
			ActivateTreeItemWaitEvent(pTree, depth4);
			ActivateTreeItemWaitEvent(pTree, depth2);
			
			MessageBoxSetResponse(BM_BACKUP_FILES_DELETE_LOCATION_QUESTION, wxYES);
			ActivateTreeItemWaitEvent(pTree, testDataDirItem);
			MessageBoxCheckFired();
			
			CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox  ->GetCount());
			CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, 
				pLocationsListBox  ->GetSelection());
			CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetCount());
			CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, 
				pExcludeLocsListBox->GetSelection());
		}
		
		CPPUNIT_ASSERT(testDepth6Dir.Rmdir());
		CPPUNIT_ASSERT(testDepth5Dir.Rmdir());
		CPPUNIT_ASSERT(testDepth4Dir.Rmdir());
		CPPUNIT_ASSERT(testDepth3Dir.Rmdir());
		CPPUNIT_ASSERT(testDepth2Dir.Rmdir());
		CPPUNIT_ASSERT(testDepth1Dir.Rmdir());
	}
	
	{
		// add two locations using the Locations panel,
		// check that the one just added is selected,
		// and text controls are populated correctly.
		SetTextCtrlValue(pLocationNameCtrl, _("tmp"));
		SetTextCtrlValue(pLocationPathCtrl, _("/tmp"));
		ClickButtonWaitEvent(pLocationAddButton);
		CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("tmp"),  
			pLocationNameCtrl->GetValue());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp"), 
			pLocationPathCtrl->GetValue());
		
		SetTextCtrlValue(pLocationNameCtrl, _("etc"));
		SetTextCtrlValue(pLocationPathCtrl, _("/etc"));
		ClickButtonWaitEvent(pLocationAddButton);
		CPPUNIT_ASSERT_EQUAL(2, pLocationsListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("etc"),  
			pLocationNameCtrl->GetValue());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/etc"), 
			pLocationPathCtrl->GetValue());
	}

	{
		// add an exclude entry using the Exclusions panel,
		// check that the one just added is selected,
		// and text controls are populated correctly.
		
		CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeTypeList->GetSelection());

		SetTextCtrlValue(pExcludePathCtrl, _("/tmp/foo"));
		ClickButtonWaitEvent(pExcludeAddButton);
		
		CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(1, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/foo"), 
			pExcludePathCtrl->GetValue());
	}
	
	{
		// add another exclude entry using the Exclusions panel,
		// check that the one just added is selected,
		// and text controls are populated correctly.
		
		SetSelection(pExcludeTypeList, 1);
		SetTextCtrlValue(pExcludePathCtrl, _("/tmp/bar"));
		ClickButtonWaitEvent(pExcludeAddButton);

		CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(2, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(1, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(1, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/bar"), 
			pExcludePathCtrl->GetValue());
	}

	{
		// change selected exclude entry, check that
		// controls are populated correctly
		SetSelection(pExcludeListBox, 0);
		CPPUNIT_ASSERT_EQUAL(0, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/foo"), 
			pExcludePathCtrl->GetValue());
		
		SetSelection(pExcludeListBox, 1);
		CPPUNIT_ASSERT_EQUAL(1, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/bar"), 
			pExcludePathCtrl->GetValue());
	}

	{
		// edit exclude entries, check that
		// controls are populated correctly
		SetSelection(pExcludeListBox, 0);
		CPPUNIT_ASSERT_EQUAL(0, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/foo"), 
			pExcludePathCtrl->GetValue());
		SetSelection(pExcludeTypeList, 2);
		SetTextCtrlValue(pExcludePathCtrl, _("/tmp/baz"));
		ClickButtonWaitEvent(pExcludeEditButton);
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());
		
		SetSelection(pExcludeListBox, 1);
		CPPUNIT_ASSERT_EQUAL(1, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/bar"), 
			pExcludePathCtrl->GetValue());
		SetSelection(pExcludeTypeList, 3);
		SetTextCtrlValue(pExcludePathCtrl, _("/tmp/whee"));
		ClickButtonWaitEvent(pExcludeEditButton);
		CPPUNIT_ASSERT_EQUAL(1, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(3, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/whee"), 
			pExcludePathCtrl->GetValue());

		SetSelection(pExcludeListBox, 0);
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());

		SetSelection(pExcludeListBox, 1);
		CPPUNIT_ASSERT_EQUAL(3, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/whee"), 
			pExcludePathCtrl->GetValue());
	}

	{
		// check that adding a new entry based on an
		// existing entry works correctly
		
		SetSelection(pExcludeListBox, 0);
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());
		
		// change path and add new entry
		SetTextCtrlValue(pExcludePathCtrl, _("/tmp/foo"));
		ClickButtonWaitEvent(pExcludeAddButton);
		CPPUNIT_ASSERT_EQUAL(3, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(2, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/foo"), 
			pExcludePathCtrl->GetValue());
		
		// original entry unchanged
		SetSelection(pExcludeListBox, 0);
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());
			
		// change type and add new entry
		SetSelection(pExcludeTypeList, 1);
		ClickButtonWaitEvent(pExcludeAddButton);
		CPPUNIT_ASSERT_EQUAL(4, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(3, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(1, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());

		// original entry unchanged
		SetSelection(pExcludeListBox, 0);
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());
	}

	{
		// add an exclude entry to the second location,
		// check that the exclude entries for the two locations
		// are not conflated
		
		CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(4, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());

		SetSelection(pExcludeLocsListBox, 1);
		CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, pExcludeListBox->GetSelection());
		
		SetSelection(pExcludeTypeList, 4);
		SetTextCtrlValue(pExcludePathCtrl, _("fubar"));
		ClickButtonWaitEvent(pExcludeAddButton);
		CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(1, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
		
		SetSelection(pExcludeLocsListBox, 0);
		CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(4, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());
		
		SetSelection(pExcludeLocsListBox, 1);
		CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(1, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(4, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("fubar"), 
			pExcludePathCtrl->GetValue());

		ClickButtonWaitEvent(pExcludeDelButton);
		CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, pExcludeListBox->GetSelection());

		SetSelection(pExcludeLocsListBox, 0);
		CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(4, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());
	}
	
	{
		// check that removing entries works correctly
		SetSelection(pExcludeListBox, 3);
		CPPUNIT_ASSERT_EQUAL(4, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(3, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(1, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());
		ClickButtonWaitEvent(pExcludeDelButton);
		CPPUNIT_ASSERT_EQUAL(3, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());
		
		SetSelection(pExcludeListBox, 2);
		CPPUNIT_ASSERT_EQUAL(3, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(2, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/foo"), 
			pExcludePathCtrl->GetValue());
		ClickButtonWaitEvent(pExcludeDelButton);
		CPPUNIT_ASSERT_EQUAL(2, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());

		SetSelection(pExcludeListBox, 1);
		CPPUNIT_ASSERT_EQUAL(2, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(1, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(3, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/whee"), 
			pExcludePathCtrl->GetValue());
		ClickButtonWaitEvent(pExcludeDelButton);
		CPPUNIT_ASSERT_EQUAL(1, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());

		SetSelection(pExcludeListBox, 0);
		CPPUNIT_ASSERT_EQUAL(1, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(2, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
			pExcludePathCtrl->GetValue());
		ClickButtonWaitEvent(pExcludeDelButton);
		CPPUNIT_ASSERT_EQUAL(0, pExcludeListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, pExcludeListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL(0, pExcludeTypeList->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)wxEmptyString, 
			pExcludePathCtrl->GetValue());
	}
	
	{
		// check that adding a new location using the
		// Locations panel works correctly
		
		CPPUNIT_ASSERT_EQUAL(2, pLocationsListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox->GetSelection());
		SetSelection(pLocationsListBox, 0);
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp"),
			pLocationPathCtrl->GetValue());
		CPPUNIT_ASSERT_EQUAL((wxString)_("tmp"),
			pLocationNameCtrl->GetValue());
		
		SetTextCtrlValue(pLocationNameCtrl, _("foobar"));
		SetTextCtrlValue(pLocationPathCtrl, _("whee"));
		ClickButtonWaitEvent(pLocationAddButton);

		CPPUNIT_ASSERT_EQUAL(3, pLocationsListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(2, pLocationsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("whee"),
			pLocationPathCtrl->GetValue());
		CPPUNIT_ASSERT_EQUAL((wxString)_("foobar"),
			pLocationNameCtrl->GetValue());			
	}
	
	{
		// check that switching locations works correctly,
		// and changes the selected location in the
		// Excludes panel (doesn't work yet)
		SetSelection(pLocationsListBox, 0);
		CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp"),
			pLocationPathCtrl->GetValue());
		CPPUNIT_ASSERT_EQUAL((wxString)_("tmp"),
			pLocationNameCtrl->GetValue());
		// CPPUNIT_ASSERT_EQUAL(0, pExcludeLocsListBox->GetSelection());
		
		SetSelection(pLocationsListBox, 1);
		CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/etc"),
			pLocationPathCtrl->GetValue());
		CPPUNIT_ASSERT_EQUAL((wxString)_("etc"),
			pLocationNameCtrl->GetValue());
		// CPPUNIT_ASSERT_EQUAL(1, pExcludeLocsListBox->GetSelection());

		SetSelection(pLocationsListBox, 2);
		CPPUNIT_ASSERT_EQUAL(2, pLocationsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("whee"),
			pLocationPathCtrl->GetValue());
		CPPUNIT_ASSERT_EQUAL((wxString)_("foobar"),
			pLocationNameCtrl->GetValue());
		// CPPUNIT_ASSERT_EQUAL(2, pExcludeLocsListBox->GetSelection());
	}
	
	{
		// check that removing locations works correctly
		SetSelection(pLocationsListBox, 0);
		CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp"),
			pLocationPathCtrl->GetValue());
		CPPUNIT_ASSERT_EQUAL((wxString)_("tmp"),
			pLocationNameCtrl->GetValue());

		ClickButtonWaitEvent(pLocationDelButton);
		CPPUNIT_ASSERT_EQUAL(2, pLocationsListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/etc"),
			pLocationPathCtrl->GetValue());
		CPPUNIT_ASSERT_EQUAL((wxString)_("etc"),
			pLocationNameCtrl->GetValue());

		SetSelection(pLocationsListBox, 1);
		CPPUNIT_ASSERT_EQUAL((wxString)_("whee"),
			pLocationPathCtrl->GetValue());
		CPPUNIT_ASSERT_EQUAL((wxString)_("foobar"),
			pLocationNameCtrl->GetValue());

		ClickButtonWaitEvent(pLocationDelButton);
		CPPUNIT_ASSERT_EQUAL(1, pLocationsListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/etc"),
			pLocationPathCtrl->GetValue());
		CPPUNIT_ASSERT_EQUAL((wxString)_("etc"),
			pLocationNameCtrl->GetValue());

		ClickButtonWaitEvent(pLocationDelButton);
		CPPUNIT_ASSERT_EQUAL(0, pLocationsListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, 
			pLocationsListBox->GetSelection());
		CPPUNIT_ASSERT_EQUAL((wxString)wxEmptyString,
			pLocationPathCtrl->GetValue());
		CPPUNIT_ASSERT_EQUAL((wxString)wxEmptyString,
			pLocationNameCtrl->GetValue());				
	}			
	
	// more complex exclusion tests
	/*
		Config:
		ExcludeFile = testfiles/TestDir1/excluded_1
		ExcludeFile = testfiles/TestDir1/excluded_2
		ExcludeFilesRegex = \.excludethis$
		ExcludeFilesRegex = EXCLUDE
		AlwaysIncludeFile = testfiles/TestDir1/dont.excludethis
		ExcludeDir = testfiles/TestDir1/exclude_dir
		ExcludeDir = testfiles/TestDir1/exclude_dir_2
		ExcludeDirsRegex = not_this_dir
		AlwaysIncludeDirsRegex = ALWAYSINCLUDE
				
		Type	Name					Excluded
		----	----					--------
		Dir	TestDir1/exclude_dir			Yes
		Dir	TestDir1/exclude_dir_2			Yes
		Dir	TestDir1/sub23				No
		Dir	TestDir1/sub23/xx_not_this_dir_22	Yes
		File	TestDir1/sub23/somefile.excludethis	Yes
		File	TestDir1/xx_not_this_dir_22		No
		File	TestDir1/excluded_1			Yes
		File	TestDir1/excluded_2			Yes
		File	TestDir1/zEXCLUDEu			Yes
		File	TestDir1/dont.excludethis		No
		Dir	TestDir1/xx_not_this_dir_ALWAYSINCLUDE	No
		
		// under TestDir1:
		TEST_THAT(!SearchDir(dir, "excluded_1"));
		TEST_THAT(!SearchDir(dir, "excluded_2"));
		TEST_THAT(!SearchDir(dir, "exclude_dir"));
		TEST_THAT(!SearchDir(dir, "exclude_dir_2"));
		// xx_not_this_dir_22 should not be excluded by
		// ExcludeDirsRegex, because it's a file
		TEST_THAT(SearchDir (dir, "xx_not_this_dir_22"));
		TEST_THAT(!SearchDir(dir, "zEXCLUDEu"));
		TEST_THAT(SearchDir (dir, "dont.excludethis"));
		TEST_THAT(SearchDir (dir, "xx_not_this_dir_ALWAYSINCLUDE"));
		
		// under TestDir1/sub23:
		TEST_THAT(!SearchDir(dir, "xx_not_this_dir_22"));
		TEST_THAT(!SearchDir(dir, "somefile.excludethis"));

	*/
	
	wxFileName configFile(tempDir.GetFullPath(), _("bbackupd.conf"));

	{
		CPPUNIT_ASSERT_EQUAL((size_t)0, mpConfig->GetLocations().size());
		
		Location* pNewLoc = NULL;
		
		{
			Location testDirLoc(_("testdata"), testDataDir.GetFullPath(),
				mpConfig);
			mpConfig->AddLocation(testDirLoc);
			CPPUNIT_ASSERT_EQUAL((size_t)1, 
				mpConfig->GetLocations().size());
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(), 
				pTree->GetItemImage(testDataDirItem));
			pNewLoc = mpConfig->GetLocation(testDirLoc);
			CPPUNIT_ASSERT(pNewLoc);
		}
		
		MyExcludeList& rExcludes = pNewLoc->GetExcludeList();

		#define CREATE_FILE(dir, name) \
		wxFileName dir ## _ ## name(dir.GetFullPath(), _(#name)); \
		{ wxFile f; CPPUNIT_ASSERT(f.Create(dir ## _ ## name.GetFullPath())); }
		
		#define CREATE_DIR(dir, name) \
		wxFileName dir ## _ ## name(dir.GetFullPath(), _(#name)); \
		CPPUNIT_ASSERT(dir ## _ ## name.Mkdir(0700));
		
		CREATE_DIR( testDataDir, exclude_dir);
		CREATE_DIR( testDataDir, exclude_dir_2);
		CREATE_DIR( testDataDir, sub23);
		CREATE_DIR( testDataDir_sub23, xx_not_this_dir_22);
		CREATE_FILE(testDataDir_sub23, somefile_excludethis);
		CREATE_FILE(testDataDir, xx_not_this_dir_22);
		CREATE_FILE(testDataDir, excluded_1);
		CREATE_FILE(testDataDir, excluded_2);
		CREATE_FILE(testDataDir, zEXCLUDEu);
		CREATE_FILE(testDataDir, dont_excludethis);
		CREATE_DIR( testDataDir, xx_not_this_dir_ALWAYSINCLUDE);
		
		#undef CREATE_DIR
		#undef CREATE_FILE

		#define ADD_ENTRY(type, path) \
		rExcludes.AddEntry \
		( \
			MyExcludeEntry \
			( \
				theExcludeTypes[type], path \
			) \
		)
		
		ADD_ENTRY(ETI_EXCLUDE_FILE, testDataDir_excluded_1.GetFullPath());
		ADD_ENTRY(ETI_EXCLUDE_FILE, testDataDir_excluded_2.GetFullPath());
		ADD_ENTRY(ETI_EXCLUDE_FILES_REGEX, _("_excludethis$"));
		ADD_ENTRY(ETI_EXCLUDE_FILES_REGEX, _("EXCLUDE"));
		ADD_ENTRY(ETI_ALWAYS_INCLUDE_FILE, testDataDir_dont_excludethis.GetFullPath());
		ADD_ENTRY(ETI_EXCLUDE_DIR, testDataDir_exclude_dir.GetFullPath());
		ADD_ENTRY(ETI_EXCLUDE_DIR, testDataDir_exclude_dir_2.GetFullPath());
		ADD_ENTRY(ETI_EXCLUDE_DIRS_REGEX, _("not_this_dir"));
		ADD_ENTRY(ETI_ALWAYS_INCLUDE_DIRS_REGEX, _("ALWAYSINCLUDE"));
		
		#undef ADD_ENTRY

		mpConfig->Save(configFile.GetFullPath());
		CPPUNIT_ASSERT(configFile.FileExists());

		pTree->Collapse(testDataDirItem);
		pTree->Expand(testDataDirItem);

		wxTreeItemIdValue cookie1;
		wxTreeItemId dir1 = testDataDirItem;
		wxTreeItemId item;
		
		#define CHECK_ITEM(name, image) \
		CPPUNIT_ASSERT(item.IsOk()); \
		CPPUNIT_ASSERT_EQUAL((wxString)_(name), \
			pTree->GetItemText(item)); \
		CPPUNIT_ASSERT_EQUAL(images.Get ## image ## ImageId(), \
			pTree->GetItemImage(item))
		
		item = pTree->GetFirstChild(dir1, cookie1);
		CHECK_ITEM("exclude_dir", Crossed);

		item = pTree->GetNextChild(dir1, cookie1);
		CHECK_ITEM("exclude_dir_2", Crossed);

		item = pTree->GetNextChild(dir1, cookie1);
		CHECK_ITEM("sub23", CheckedGrey);

		// under sub23:
		{
			wxTreeItemIdValue cookie2;
			wxTreeItemId dir2 = item;
			pTree->Expand(dir2);

			item = pTree->GetFirstChild(dir2, cookie2);
			CHECK_ITEM("xx_not_this_dir_22", Crossed);

			item = pTree->GetNextChild(dir2, cookie2);
			CHECK_ITEM("somefile_excludethis", Crossed);
			
			item = pTree->GetNextChild(dir2, cookie2);
			CPPUNIT_ASSERT(!item.IsOk());
		}

		item = pTree->GetNextChild(dir1, cookie1);
		CHECK_ITEM("xx_not_this_dir_ALWAYSINCLUDE", Always);

		item = pTree->GetNextChild(dir1, cookie1);
		CHECK_ITEM("dont_excludethis", Always);

		item = pTree->GetNextChild(dir1, cookie1);
		CHECK_ITEM("excluded_1", Crossed);

		item = pTree->GetNextChild(dir1, cookie1);
		CHECK_ITEM("excluded_2", Crossed);

		// xx_not_this_dir_22 should not be excluded by
		// ExcludeDirsRegex, because it's a file
		item = pTree->GetNextChild(dir1, cookie1);
		CHECK_ITEM("xx_not_this_dir_22", CheckedGrey);

		item = pTree->GetNextChild(dir1, cookie1);
		CHECK_ITEM("zEXCLUDEu", Crossed);

		item = pTree->GetNextChild(dir1, cookie1);
		CPPUNIT_ASSERT(!item.IsOk());
		
		#undef CHECK_ITEM
		
		#define DELETE_FILE(dir, name) \
		CPPUNIT_ASSERT(wxRemoveFile(dir ## _ ## name.GetFullPath()))
		
		#define DELETE_DIR(dir, name) \
		CPPUNIT_ASSERT(dir ## _ ## name.Rmdir())

		DELETE_DIR( testDataDir_sub23, xx_not_this_dir_22);
		DELETE_FILE(testDataDir_sub23, somefile_excludethis);
		DELETE_FILE(testDataDir, xx_not_this_dir_22);
		DELETE_FILE(testDataDir, excluded_1);
		DELETE_FILE(testDataDir, excluded_2);
		DELETE_FILE(testDataDir, zEXCLUDEu);
		DELETE_FILE(testDataDir, dont_excludethis);
		DELETE_DIR( testDataDir, xx_not_this_dir_ALWAYSINCLUDE);
		DELETE_DIR( testDataDir, sub23);
		DELETE_DIR( testDataDir, exclude_dir);
		DELETE_DIR( testDataDir, exclude_dir_2);
		
		#undef DELETE_DIR
		#undef DELETE_FILE
		
		CPPUNIT_ASSERT_EQUAL((size_t)1, mpConfig->GetLocations().size());
		mpConfig->RemoveLocation(*pNewLoc);
		CPPUNIT_ASSERT_EQUAL((size_t)0, mpConfig->GetLocations().size());
	}

	{
		wxButton* pBackupFilesCloseButton = wxDynamicCast
		(
			pBackupFilesPanel->FindWindow(wxID_CANCEL), wxButton
		);
		CPPUNIT_ASSERT(pBackupFilesCloseButton);
	
		ClickButtonWaitEvent(pBackupFilesCloseButton);
		CPPUNIT_ASSERT(!pBackupFilesPanel->IsShown());
	}

	{
		wxButton* pBackupCloseButton = wxDynamicCast
		(
			pBackupPanel->FindWindow(wxID_CANCEL), wxButton
		);
		CPPUNIT_ASSERT(pBackupCloseButton);
	
		ClickButtonWaitEvent(pBackupCloseButton);
		CPPUNIT_ASSERT(!pBackupPanel->IsShown());
	}
		
	// clean up
	DeleteRecursive(testDataDir);
	CPPUNIT_ASSERT(wxRemoveFile(configFile.GetFullPath()));		
	CPPUNIT_ASSERT(tempDir.Rmdir());
}
