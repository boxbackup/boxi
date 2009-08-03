/***************************************************************************
 *            TestBackupConfig.cc
 *
 *  Wed May 10 23:10:16 2006
 *  Copyright 2006-2008 Chris Wilson
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

#include "SandBox.h"

#include <wx/button.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/treectrl.h>
#include <wx/volume.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <cppunit/TestSuite.h>
#include <cppunit/extensions/HelperMacros.h>

#include "Box.h"

#include "main.h"
#include "BoxiApp.h"
#include "ClientConfig.h"
#include "MainFrame.h"
#include "TestBackupConfig.h"

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
	wxFileName asDir(rPath.GetFullPath(), wxT(""));
	
	if (asDir.DirExists())
	{
		{
			wxDir dir(rPath.GetFullPath());
			CPPUNIT_ASSERT(dir.IsOpened());
			
			wxString file;
			if (dir.GetFirst(&file))
			{
				do
				{
					wxFileName filename(
						rPath.GetFullPath(), file);
					DeleteRecursive(filename);
				}
				while (dir.GetNext(&file));
			}
		}

		// Make sure wxDir is out of scope, so that the
		// directory is closed, otherwise we can't delete
		// it on Win32.
		
		wxCharBuffer buf = rPath.GetFullPath().mb_str(wxConvBoxi);
		CPPUNIT_ASSERT_MESSAGE(buf.data(), wxRmdir(rPath.GetFullPath()));
	}
	else
	#ifdef WIN32
		if (rPath.FileExists()) // no symlinks on windows
	#else
 		// rPath.FileExists() doesn't catch symlinks
	#endif
	{
		
		CPPUNIT_ASSERT(wxRemoveFile(rPath.GetFullPath()));
	}
}

void TestBackupConfig::RunTest()
{
	mapImages.reset(new FileImageList());

	wxLogNull logSuppress();

	// create a working directory
	mTempDir.AssignTempFileName(wxT("boxi-tempdir-"));

	// On Windows, the temporary file name may use short names, 
	// which breaks access via the tree control. So fix it now.
	// Also ensures that the wxFileName refers to a directory,
	// not a file.
	mTempDir = wxFileName(mTempDir.GetLongPath(), wxT(""));

	CPPUNIT_ASSERT(wxRemoveFile(mTempDir.GetPath()));
	CPPUNIT_ASSERT(mTempDir.Mkdir(0700));
	CPPUNIT_ASSERT(mTempDir.DirExists());
	
	MainFrame* pMainFrame = GetMainFrame();
	CPPUNIT_ASSERT(pMainFrame);
	
	mpConfig = pMainFrame->GetConfig();
	CPPUNIT_ASSERT(mpConfig);
	
	pMainFrame->DoFileOpen(MakeAbsolutePath(
		_("../test/config/bbackupd.conf")).GetFullPath());
	{
		const BoxiLocation::List& rLocs = mpConfig->GetLocations();
		CPPUNIT_ASSERT_EQUAL((size_t)0, rLocs.size());
	}
	
	wxString msg;
	bool isOk = mpConfig->Check(msg);

	if (!isOk)
	{
		wxCharBuffer buf = msg.mb_str(wxConvBoxi);
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

	mpTree = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_Backup_Locations_Tree), wxTreeCtrl
	);
	CPPUNIT_ASSERT(mpTree);

	wxTreeItemId rootId = mpTree->GetRootItem();
	wxString rootLabel  = mpTree->GetItemText(rootId);

	#ifdef WIN32
	CPPUNIT_ASSERT_EQUAL(wxString(_("My Computer")), rootLabel);
	#else
	CPPUNIT_ASSERT_EQUAL(wxString(_("/ (local root)")), rootLabel);
	#endif
	
	CPPUNIT_ASSERT_EQUAL(mapImages->GetEmptyImageId(), 
		mpTree->GetItemImage(rootId));
		
	mpLocationsListBox = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_Backup_LocationsList), wxListBox
	);
	CPPUNIT_ASSERT(mpLocationsListBox);
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox->GetCount());
	
	mpExcludeLocsListBox = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_BackupLoc_ExcludeLocList), wxChoice
	);
	CPPUNIT_ASSERT(mpExcludeLocsListBox);
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetCount());
	
	wxPanel* pLocationsPanel = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_BackupLoc_List_Panel), wxPanel
	);
	CPPUNIT_ASSERT(pLocationsPanel);
	
	mpLocationNameCtrl = wxDynamicCast
	(
		pLocationsPanel->FindWindow(ID_Backup_LocationNameCtrl), wxTextCtrl
	);
	CPPUNIT_ASSERT(mpLocationNameCtrl);
	CPPUNIT_ASSERT_EQUAL((wxString)wxEmptyString, 
		mpLocationNameCtrl->GetValue());

	mpLocationPathCtrl = wxDynamicCast
	(
		pLocationsPanel->FindWindow(ID_Backup_LocationPathCtrl), wxTextCtrl
	);
	CPPUNIT_ASSERT(mpLocationPathCtrl);
	CPPUNIT_ASSERT_EQUAL((wxString)wxEmptyString, 
		mpLocationPathCtrl->GetValue());	

	mpLocationAddButton = wxDynamicCast
	(
		pLocationsPanel->FindWindow(ID_Backup_LocationsAddButton), wxButton
	);
	CPPUNIT_ASSERT(mpLocationAddButton);

	mpLocationEditButton = wxDynamicCast
	(
		pLocationsPanel->FindWindow(ID_Backup_LocationsEditButton), wxButton
	);
	CPPUNIT_ASSERT(mpLocationEditButton);

	mpLocationDelButton = wxDynamicCast
	(
		pLocationsPanel->FindWindow(ID_Backup_LocationsDelButton), wxButton
	);
	CPPUNIT_ASSERT(mpLocationDelButton);

	wxPanel* pExcludePanel = wxDynamicCast
	(
		pMainFrame->FindWindow(ID_BackupLoc_Excludes_Panel), wxPanel
	);
	CPPUNIT_ASSERT(pExcludePanel);
	
	mpExcludeListBox = wxDynamicCast
	(
		pExcludePanel->FindWindow(ID_Backup_LocationsList), wxListBox
	);
	CPPUNIT_ASSERT(mpExcludeListBox);
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetCount());

	mpExcludeTypeList = wxDynamicCast
	(
		pExcludePanel->FindWindow(ID_BackupLoc_ExcludeTypeList), wxChoice
	);
	CPPUNIT_ASSERT(mpExcludeTypeList);
	CPPUNIT_ASSERT_EQUAL(8, mpExcludeTypeList->GetCount());

	mpExcludePathCtrl = wxDynamicCast
	(
		pExcludePanel->FindWindow(ID_BackupLoc_ExcludePathCtrl), wxTextCtrl
	);
	CPPUNIT_ASSERT(mpExcludePathCtrl);
	CPPUNIT_ASSERT_EQUAL((wxString)wxEmptyString, 
		mpExcludePathCtrl->GetValue());

	mpExcludeAddButton = wxDynamicCast
	(
		pExcludePanel->FindWindow(ID_Backup_LocationsAddButton), wxButton
	);
	CPPUNIT_ASSERT(mpExcludeAddButton);

	mpExcludeEditButton = wxDynamicCast
	(
		pExcludePanel->FindWindow(ID_Backup_LocationsEditButton), wxButton
	);
	CPPUNIT_ASSERT(mpExcludeEditButton);

	mpExcludeDelButton = wxDynamicCast
	(
		pExcludePanel->FindWindow(ID_Backup_LocationsDelButton), wxButton
	);
	CPPUNIT_ASSERT(mpExcludeDelButton);

	// test that creating a location using the tree adds entries 
	// to the locations and excludes list boxes (except on Win32,
	// where it represents My Computer and can't be selected)

	ActivateTreeItemWaitEvent(mpTree, rootId);

	#ifdef WIN32
		CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox  ->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetCount());
	#else
		CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox  ->GetCount());
		CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());
		CPPUNIT_ASSERT_EQUAL((wxString)_("/ -> root"), 
			mpLocationsListBox  ->GetString(0));
		CPPUNIT_ASSERT_EQUAL((wxString)_("/ -> root"), 
			mpExcludeLocsListBox->GetString(0));

		MessageBoxSetResponse(BM_BACKUP_FILES_DELETE_LOCATION_QUESTION,
			wxNO);
		ActivateTreeItemWaitEvent(mpTree, rootId);
		MessageBoxCheckFired();
		CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox  ->GetCount());
		CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());

		MessageBoxSetResponse(BM_BACKUP_FILES_DELETE_LOCATION_QUESTION, 
			wxYES);
		ActivateTreeItemWaitEvent(mpTree, rootId);
		MessageBoxCheckFired();
		CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox  ->GetCount());
		CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetCount());
	#endif

	mTestDataDir = wxFileName(mTempDir.GetFullPath(), _("testdata"));
	mTestDataDir = wxFileName(mTestDataDir.GetFullPath(), _(""));
	wxCharBuffer buf = mTestDataDir.GetFullPath().mb_str();
	CPPUNIT_ASSERT_MESSAGE(buf.data(), mTestDataDir.Mkdir(0700));
	
	// will be changed in the block below
	mTestDataDirItem = rootId;

	mTestDepth1Dir = wxFileName(mTestDataDir.GetFullPath(),	_("depth1"));
	mTestDepth1Dir = wxFileName(mTestDepth1Dir.GetFullPath(), _(""));
	CPPUNIT_ASSERT(mTestDepth1Dir.Mkdir(0700));
	
	mTestAnotherDir = wxFileName(mTestDataDir.GetFullPath(), _("another"));
	mTestAnotherDir = wxFileName(mTestAnotherDir.GetFullPath(), _(""));
	CPPUNIT_ASSERT(mTestAnotherDir.Mkdir(0700));

	mTestFile1 = wxFileName(mTestDepth1Dir.GetFullPath(), _("file1"));
	CPPUNIT_ASSERT(wxFile().Create(mTestFile1.GetFullPath()));
	
	mTestDir1 = wxFileName(mTestDepth1Dir.GetFullPath(), _("dir1"));
	mTestDir1 = wxFileName(mTestDir1.GetFullPath(), _(""));
	CPPUNIT_ASSERT(mTestDir1.Mkdir(0700));
	
	mTestDepth2Dir = wxFileName(mTestDepth1Dir.GetFullPath(), _("depth2"));
	mTestDepth2Dir = wxFileName(mTestDepth2Dir.GetFullPath(), _(""));
	CPPUNIT_ASSERT(mTestDepth2Dir.Mkdir(0700));

	mTestFile2 = wxFileName(mTestDepth2Dir.GetFullPath(), _("file2"));
	CPPUNIT_ASSERT(wxFile().Create(mTestFile2.GetFullPath()));
	mTestDir2 = wxFileName(mTestDepth2Dir.GetFullPath(), _("dir2"));
	mTestDir2 = wxFileName(mTestDir2.GetFullPath(), _(""));
	CPPUNIT_ASSERT(mTestDir2.Mkdir(0700));
	mTestDepth3Dir = wxFileName(mTestDepth2Dir.GetFullPath(), _("depth3"));
	mTestDepth3Dir = wxFileName(mTestDepth3Dir.GetFullPath(), _(""));
	CPPUNIT_ASSERT(mTestDepth3Dir.Mkdir(0700));

	mTestDepth4Dir = wxFileName(mTestDepth3Dir.GetFullPath(), _("depth4"));
	mTestDepth4Dir = wxFileName(mTestDepth4Dir.GetFullPath(), _(""));
	CPPUNIT_ASSERT(mTestDepth4Dir.Mkdir(0700));

	mTestDepth5Dir = wxFileName(mTestDepth4Dir.GetFullPath(), _("depth5"));
	mTestDepth5Dir = wxFileName(mTestDepth5Dir.GetFullPath(), _(""));
	CPPUNIT_ASSERT(mTestDepth5Dir.Mkdir(0700));

	mTestFile3 = wxFileName(mTestDepth5Dir.GetFullPath(), _("file3"));
	CPPUNIT_ASSERT(wxFile().Create(mTestFile3.GetFullPath()));
	mTestDir3 = wxFileName(mTestDepth5Dir.GetFullPath(), _("dir3"));
	mTestDir3 = wxFileName(mTestDir3.GetFullPath(), _(""));
	CPPUNIT_ASSERT(mTestDir3.Mkdir(0700));
	mTestDepth6Dir = wxFileName(mTestDepth5Dir.GetFullPath(), _("depth6"));
	mTestDepth6Dir = wxFileName(mTestDepth6Dir.GetFullPath(), _(""));
	CPPUNIT_ASSERT(mTestDepth6Dir.Mkdir(0700));

	mTestFile4 = wxFileName(mTestDepth6Dir.GetFullPath(), _("file4"));
	CPPUNIT_ASSERT(wxFile().Create(mTestFile4.GetFullPath()));
	mTestDir4 = wxFileName(mTestDepth6Dir.GetFullPath(), _("dir4"));
	mTestDir4 = wxFileName(mTestDir4.GetFullPath(), _(""));
	CPPUNIT_ASSERT(mTestDir4.Mkdir(0700));

	mTestFile5 = wxFileName(mTestDir4.GetFullPath(), _("file5"));
	CPPUNIT_ASSERT(wxFile().Create(mTestFile5.GetFullPath()));
	mTestDir5 = wxFileName(mTestDir4.GetFullPath(), _("dir5"));
	mTestDir5 = wxFileName(mTestDir5.GetFullPath(), _(""));
	CPPUNIT_ASSERT(mTestDir5.Mkdir(0700));

	mTestFile6 = wxFileName(mTestDir3.GetFullPath(), _("file6"));
	CPPUNIT_ASSERT(wxFile().Create(mTestFile6.GetFullPath()));
	mTestDir6 = wxFileName(mTestDir3.GetFullPath(), _("dir6"));
	mTestDir6 = wxFileName(mTestDir6.GetFullPath(), _(""));
	CPPUNIT_ASSERT(mTestDir6.Mkdir(0700));

	wxArrayString testDataDirPathDirs = mTestDataDir.GetDirs();
	// testDataDirPathDirs.Add(mTestDataDir.GetName()); // empty

	#ifdef WIN32
	if (! mTestDataDir.GetVolume().IsSameAs(wxEmptyString))
	{
		wxString volshort = mTestDataDir.GetVolume();
		volshort.Append(_(":\\"));
		wxFSVolumeBase volume(volshort);
		wxCharBuffer buf = volshort.mb_str();
		CPPUNIT_ASSERT_MESSAGE(buf.data(), volume.IsOk());
		testDataDirPathDirs.Insert(volume.GetDisplayName(), 0);
	}
	#endif
	
	for (size_t i = 0; i < testDataDirPathDirs.Count(); i++)
	{
		wxString dirName = testDataDirPathDirs.Item(i);
		
		if (!mpTree->IsExpanded(mTestDataDirItem))
		{
			mpTree->Expand(mTestDataDirItem);
		}
		
		bool found = false;
		wxTreeItemIdValue cookie;
		wxTreeItemId child;
		
		for (child = mpTree->GetFirstChild(mTestDataDirItem, cookie);
			child.IsOk(); child = mpTree->GetNextChild(mTestDataDirItem, cookie))
		{
			if (mpTree->GetItemText(child).IsSameAs(dirName))
			{
				mTestDataDirItem = child;
				found = true;
				break;
			}
		}

		wxCharBuffer buf = dirName.mb_str();			
		CPPUNIT_ASSERT_MESSAGE(buf.data(), found);
	}
	
	wxTreeItemIdValue cookie;
	
	mpTree->Expand(mTestDataDirItem);
	CPPUNIT_ASSERT_EQUAL((size_t)2, 
		mpTree->GetChildrenCount(mTestDataDirItem, false));
	mAnother = mpTree->GetFirstChild(mTestDataDirItem, cookie);
	CPPUNIT_ASSERT(mAnother.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("another")), 
		mpTree->GetItemText(mAnother));
	mDepth1 = mpTree->GetNextChild(mTestDataDirItem, cookie);
	CPPUNIT_ASSERT(mDepth1.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("depth1")), 
		mpTree->GetItemText(mDepth1));
	
	mpTree->Expand(mDepth1);
	CPPUNIT_ASSERT_EQUAL((size_t)3, 
		mpTree->GetChildrenCount(mDepth1, false));
	mDepth2 = mpTree->GetFirstChild(mDepth1, cookie);
	CPPUNIT_ASSERT(mDepth2.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("depth2")), 
		mpTree->GetItemText(mDepth2));
	mDir1 = mpTree->GetNextChild(mDepth1, cookie);
	CPPUNIT_ASSERT(mDir1.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("dir1")), 
		mpTree->GetItemText(mDir1));
	mFile1 = mpTree->GetNextChild(mDepth1, cookie);
	CPPUNIT_ASSERT(mFile1.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("file1")), 
		mpTree->GetItemText(mFile1));
	
	mpTree->Expand(mDepth2);
	CPPUNIT_ASSERT_EQUAL((size_t)3, 
		mpTree->GetChildrenCount(mDepth2, false));
	mDepth3 = mpTree->GetFirstChild(mDepth2, cookie);
	CPPUNIT_ASSERT(mDepth3.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("depth3")), 
		mpTree->GetItemText(mDepth3));
	mDir2 = mpTree->GetNextChild(mDepth2, cookie);
	CPPUNIT_ASSERT(mDir2.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("dir2")), 
		mpTree->GetItemText(mDir2));
	mFile2 = mpTree->GetNextChild(mDepth2, cookie);
	CPPUNIT_ASSERT(mFile2.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("file2")), 
		mpTree->GetItemText(mFile2));
	
	mpTree->Expand(mDepth3);
	CPPUNIT_ASSERT_EQUAL((size_t)1, 
		mpTree->GetChildrenCount(mDepth3, false));
	mDepth4 = mpTree->GetFirstChild(mDepth3, cookie);		
	CPPUNIT_ASSERT(mDepth4.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("depth4")), 
		mpTree->GetItemText(mDepth4));

	mpTree->Expand(mDepth4);
	CPPUNIT_ASSERT_EQUAL((size_t)1, 
		mpTree->GetChildrenCount(mDepth4, false));
	mDepth5 = mpTree->GetFirstChild(mDepth4, cookie);		
	CPPUNIT_ASSERT(mDepth5.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("depth5")), 
		mpTree->GetItemText(mDepth5));

	mpTree->Expand(mDepth5);
	CPPUNIT_ASSERT_EQUAL((size_t)3, 
		mpTree->GetChildrenCount(mDepth5, false));
	mDepth6 = mpTree->GetFirstChild(mDepth5, cookie);		
	CPPUNIT_ASSERT(mDepth6.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("depth6")), 
		mpTree->GetItemText(mDepth6));
	mDir3 = mpTree->GetNextChild(mDepth5, cookie);
	CPPUNIT_ASSERT(mDir3.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("dir3")), 
		mpTree->GetItemText(mDir3));
	mFile3 = mpTree->GetNextChild(mDepth5, cookie);
	CPPUNIT_ASSERT(mFile3.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("file3")), 
		mpTree->GetItemText(mFile3));

	mpTree->Expand(mDepth6);
	CPPUNIT_ASSERT_EQUAL((size_t)2, 
		mpTree->GetChildrenCount(mDepth6, false));
	mDir4 = mpTree->GetFirstChild(mDepth6, cookie);
	CPPUNIT_ASSERT(mDir4.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("dir4")), 
		mpTree->GetItemText(mDir4));
	mFile4 = mpTree->GetNextChild(mDepth6, cookie);
	CPPUNIT_ASSERT(mFile4.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("file4")), 
		mpTree->GetItemText(mFile4));

	mpTree->Expand(mDir4);
	CPPUNIT_ASSERT_EQUAL((size_t)2, 
		mpTree->GetChildrenCount(mDir4, false));
	mDir5 = mpTree->GetFirstChild(mDir4, cookie);
	CPPUNIT_ASSERT(mDir5.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("dir5")), 
		mpTree->GetItemText(mDir5));
	mFile5 = mpTree->GetNextChild(mDir4, cookie);
	CPPUNIT_ASSERT(mFile5.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("file5")), 
		mpTree->GetItemText(mFile5));

	mpTree->Expand(mDir3);
	CPPUNIT_ASSERT_EQUAL((size_t)2, 
		mpTree->GetChildrenCount(mDir3, false));
	mDir6 = mpTree->GetFirstChild(mDir3, cookie);
	CPPUNIT_ASSERT(mDir6.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("dir6")), 
		mpTree->GetItemText(mDir6));
	mFile6 = mpTree->GetNextChild(mDir3, cookie);
	CPPUNIT_ASSERT(mFile6.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("file6")), 
		mpTree->GetItemText(mFile6));

	TestAddAndRemoveSimpleLocationInTree();		
	TestAddLocationInTree();		
	TestSimpleExcludeInTree();		
	TestDeepIncludePattern();
	TestAlwaysIncludeFileDeepInTree();
	TestAddAlwaysIncludeInTree();		
	TestExcludeUnderAlwaysIncludePromptsToRemove();
	TestRemoveMultipleAlwaysIncludesInTree();
	TestAlwaysIncludeDirWithSubdirsDeepInTree();
	TestRemoveExcludeInTree();		
	TestRemoveAlwaysIncludeInTree();
	TestAddExcludeInIncludedNode();
	TestAddHigherExcludeInTree();		
	TestRemoveEntriesFromTree();
	
	CPPUNIT_ASSERT(wxRemoveFile(mTestFile6.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mTestFile5.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mTestFile4.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mTestFile3.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mTestFile2.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mTestFile1.GetFullPath()));
	CPPUNIT_ASSERT(mTestDir6.Rmdir());
	CPPUNIT_ASSERT(mTestDir5.Rmdir());
	CPPUNIT_ASSERT(mTestDir4.Rmdir());
	CPPUNIT_ASSERT(mTestDir3.Rmdir());
	CPPUNIT_ASSERT(mTestDir2.Rmdir());
	CPPUNIT_ASSERT(mTestDir1.Rmdir());
	CPPUNIT_ASSERT_MESSAGE_WX(mTestDepth6Dir.GetFullPath(),
		mTestDepth6Dir.Rmdir());
	CPPUNIT_ASSERT_MESSAGE_WX(mTestDepth5Dir.GetFullPath(),
		mTestDepth5Dir.Rmdir());
	CPPUNIT_ASSERT_MESSAGE_WX(mTestDepth4Dir.GetFullPath(),
		mTestDepth4Dir.Rmdir());
	CPPUNIT_ASSERT_MESSAGE_WX(mTestDepth3Dir.GetFullPath(),
		mTestDepth3Dir.Rmdir());
	CPPUNIT_ASSERT_MESSAGE_WX(mTestDepth2Dir.GetFullPath(),
		mTestDepth2Dir.Rmdir());
	CPPUNIT_ASSERT_MESSAGE_WX(mTestDepth1Dir.GetFullPath(),
		mTestDepth1Dir.Rmdir());
	
	TestAddTwoLocations();
	TestAddExcludeEntry();	
	TestAddAnotherExcludeEntry();
	TestSelectExcludeEntry();
	TestEditExcludeEntries();
	TestAddExcludeBasedOnExisting();
	TestAddExcludeEntryToSecondLoc();	
	TestRemoveExcludeEntries();	
	TestAddLocationOnLocationsPanel();	
	TestSwitchLocations();	
	TestRemoveLocation();	
	TestComplexExcludes();
	
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
	DeleteRecursive(mTestDataDir);
	CPPUNIT_ASSERT_MESSAGE_WX(mTempDir.GetPath(), mTempDir.Rmdir());
}

void TestBackupConfig::TestAddAndRemoveSimpleLocationInTree()
{
	ActivateTreeItemWaitEvent(mpTree, mTestDataDirItem);

	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedImageId(), 
		mpTree->GetItemImage(mTestDataDirItem));

	const BoxiLocation::List& rLocations = 
		mpConfig->GetLocations();
	CPPUNIT_ASSERT_EQUAL((size_t)1, rLocations.size());
	
	BoxiLocation* pTestDataLocation = mpConfig->GetLocation(
		*(rLocations.begin()));
	CPPUNIT_ASSERT(pTestDataLocation);
	
	BoxiExcludeList& rExcludeList = 
		pTestDataLocation->GetExcludeList();

	const BoxiExcludeEntry::List& rEntries = 
		rExcludeList.GetEntries();
	CPPUNIT_ASSERT_EQUAL((size_t)0, rEntries.size());

	// Check that excluding all files but one does not mark the
	// location node as excluded, and other nodes are correctly 
	// marked. Reconfigure to exclude all files but one
	
	rExcludeList.AddEntry
	(
		BoxiExcludeEntry
		(
			theExcludeTypes[ETI_EXCLUDE_FILES_REGEX], 
			wxString(_(".*"))
		)
	);
	
	rExcludeList.AddEntry
	(
		BoxiExcludeEntry
		(
			theExcludeTypes[ETI_EXCLUDE_DIRS_REGEX], 
			wxString(_(".*"))
		)
	);
	
	rExcludeList.AddEntry
	(
		BoxiExcludeEntry
		(
			theExcludeTypes[ETI_ALWAYS_INCLUDE_DIR],
			mTestDepth1Dir.GetPath()
		)
	);
	
	CPPUNIT_ASSERT_EQUAL((size_t)3, rEntries.size());
	
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedImageId(), 
		mpTree->GetItemImage(mTestDataDirItem));
		
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mAnother));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth1));

	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDir1));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mFile1));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDepth2));
		
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDir2));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mFile2));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDepth3));

	// Now add another entry to AlwaysInclude mDepth2
	// by regex. Make sure that its children are still
	// excluded by the Exclude* = .* directives.

	wxString path = wxT("^");
	path.Append(mTestDepth1Dir.GetPath());
	path.Append(wxT("/de...2$"));
	
	rExcludeList.AddEntry
	(
		BoxiExcludeEntry
		(
			theExcludeTypes[ETI_ALWAYS_INCLUDE_DIRS_REGEX],
			path
		)
	);

	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDir1));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mFile1));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth2));

	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDir2));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mFile2));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDepth3));

	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDepth4));

	// Include a file and a dir using the wrong kinds of
	// rules. Check that they don't show up as included.
	
	rExcludeList.AddEntry
	(
		BoxiExcludeEntry
		(
			theExcludeTypes[ETI_ALWAYS_INCLUDE_DIR],
			mTestFile1.GetFullPath()
		)
	);

	rExcludeList.AddEntry
	(
		BoxiExcludeEntry
		(
			theExcludeTypes[ETI_ALWAYS_INCLUDE_FILE],
			mTestDir1.GetFullPath()
		)
	);

	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDir1));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mFile1));

	// Include a file inside an excluded directory. Check
	// that it doesn't show up as included, because it will
	// never be scanned until its parents are included.

	rExcludeList.AddEntry
	(
		BoxiExcludeEntry
		(
			theExcludeTypes[ETI_ALWAYS_INCLUDE_DIR],
			mTestDir3.GetFullPath()
		)
	);

	rExcludeList.AddEntry
	(
		BoxiExcludeEntry
		(
			theExcludeTypes[ETI_ALWAYS_INCLUDE_FILE],
			mTestFile3.GetFullPath()
		)
	);

	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDir3));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mFile3));

	// Remove the location by clicking in the tree.

	MessageBoxSetResponse(
		BM_BACKUP_FILES_DELETE_LOCATION_QUESTION, 
		wxYES);
	ActivateTreeItemWaitEvent(mpTree, mTestDataDirItem);
	MessageBoxCheckFired();

	CPPUNIT_ASSERT_EQUAL(mapImages->GetEmptyImageId(), 
		mpTree->GetItemImage(mTestDataDirItem));
		
	CPPUNIT_ASSERT_EQUAL((size_t)0, rLocations.size());
}

void TestBackupConfig::TestAddLocationInTree()
{
	// activate the testdata dir node, check that it
	// and its children are shown as included in the tree,
	// and all parent nodes are shown as partially included
	ActivateTreeItemWaitEvent(mpTree, mTestDataDirItem);

	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedImageId(), 
		mpTree->GetItemImage(mTestDataDirItem));

	for (wxTreeItemId nodeId = mpTree->GetItemParent(mTestDataDirItem);
		nodeId.IsOk(); nodeId = mpTree->GetItemParent(nodeId))
	{
		CPPUNIT_ASSERT_EQUAL(mapImages->GetPartialImageId(), 
			mpTree->GetItemImage(nodeId));
	}
	
	for (wxTreeItemId nodeId = mDepth6; 
		!(nodeId == mTestDataDirItem); 
		nodeId = mpTree->GetItemParent(nodeId))
	{
		CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedGreyImageId(), 
			mpTree->GetItemImage(nodeId));
	}
	
	// check that the entry details are shown in the
	// Locations panel, and the entry is selected
	CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox  ->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	wxString expectedTitle = mTestDataDir.GetPath();
	expectedTitle.Append(_(" -> testdata"));
	CPPUNIT_ASSERT_EQUAL(expectedTitle, 
		mpLocationsListBox  ->GetString(0));
	CPPUNIT_ASSERT_EQUAL(expectedTitle,
		mpExcludeLocsListBox->GetString(0));
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL((wxString)_("testdata"), 
		mpLocationNameCtrl->GetValue());
	CPPUNIT_ASSERT_EQUAL(mTestDataDir.GetPath(),
		mpLocationPathCtrl->GetValue());
}

void TestBackupConfig::TestSimpleExcludeInTree()
{
	// activate a node at depth 2, check that it and
	// its children are Excluded
	ActivateTreeItemWaitEvent(mpTree, mDepth2);
	CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox  ->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	wxString expectedTitle = mTestDataDir.GetPath();
	expectedTitle.Append(_(" -> testdata (ExcludeDir = "));
	expectedTitle.Append(mTestDepth2Dir.GetPath());
	expectedTitle.Append(_(")"));
	CPPUNIT_ASSERT_EQUAL(expectedTitle, 
		mpLocationsListBox  ->GetString(0));
	CPPUNIT_ASSERT_EQUAL(expectedTitle,
		mpExcludeLocsListBox->GetString(0));
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedImageId(), 
		mpTree->GetItemImage(mTestDataDirItem));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedGreyImageId(), 
		mpTree->GetItemImage(mDepth1));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDepth2));
	for (wxTreeItemId nodeId = mDepth6; !(nodeId == mDepth2); 
		nodeId = mpTree->GetItemParent(nodeId))
	{
		CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
			mpTree->GetItemImage(nodeId));
	}
}

void TestBackupConfig::TestDeepIncludePattern()
{
	// check that the right sequence of excludes and
	// alwaysincludes has the desired effect, and then
	// remove them again.

	const BoxiLocation::List& rLocations = 
		mpConfig->GetLocations();
	CPPUNIT_ASSERT_EQUAL((size_t)1, rLocations.size());
	
	BoxiLocation* pTestDataLocation = mpConfig->GetLocation(
		*(rLocations.begin()));
	CPPUNIT_ASSERT(pTestDataLocation);
	
	BoxiExcludeList& rExcludeList = 
		pTestDataLocation->GetExcludeList();
	CPPUNIT_ASSERT_EQUAL((size_t)1, 
		rExcludeList.GetEntries().size());

	BoxiExcludeEntry t0(ET_EXCLUDE_DIR,
		mTestDepth2Dir.GetPath());
	rExcludeList.RemoveEntry(t0);

	BoxiExcludeEntry t1(ET_EXCLUDE_DIRS_REGEX,
		wxString(wxT("^"))
		.Append(mTestDepth2Dir.GetPath())
		.Append(wxT("/")));
	rExcludeList.AddEntry(t1);

	BoxiExcludeEntry t2(ET_EXCLUDE_FILES_REGEX,
		wxString(wxT("^"))
		.Append(mTestDepth2Dir.GetPath())
		.Append(wxT("/")));
	rExcludeList.AddEntry(t2);

	BoxiExcludeEntry t3(ET_ALWAYS_INCLUDE_DIR,
		mTestDepth3Dir.GetPath());
	rExcludeList.AddEntry(t3);

	BoxiExcludeEntry t4(ET_ALWAYS_INCLUDE_DIR,
		mTestDepth4Dir.GetPath());
	rExcludeList.AddEntry(t4);

	BoxiExcludeEntry t5(ET_ALWAYS_INCLUDE_DIR,
		mTestDepth5Dir.GetPath());
	rExcludeList.AddEntry(t5);

	BoxiExcludeEntry t6(ET_ALWAYS_INCLUDE_DIR,
		mTestDepth6Dir.GetPath());
	rExcludeList.AddEntry(t6);

	BoxiExcludeEntry t7(ET_ALWAYS_INCLUDE_DIRS_REGEX,
		wxString(wxT("^"))
		.Append(mTestDepth6Dir.GetPath())
		.Append(wxT("/")));
	rExcludeList.AddEntry(t7);

	BoxiExcludeEntry t8(ET_ALWAYS_INCLUDE_FILES_REGEX,
		wxString(wxT("^"))
		.Append(mTestDepth6Dir.GetPath())
		.Append(wxT("/")));
	rExcludeList.AddEntry(t8);

	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedImageId(), 
		mpTree->GetItemImage(mTestDataDirItem));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedGreyImageId(), 
		mpTree->GetItemImage(mAnother));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedGreyImageId(), 
		mpTree->GetItemImage(mDepth1));

	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedGreyImageId(), 
		mpTree->GetItemImage(mDir1));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedGreyImageId(), 
		mpTree->GetItemImage(mFile1));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedGreyImageId(), 
		mpTree->GetItemImage(mDepth2));

	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDir2));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mFile2));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth3));

	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth4));
		
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth5));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDir3));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mFile3));
		
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth6));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDir4));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mFile4));

	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDir5));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mFile5));

	rExcludeList.AddEntry(t0);
	rExcludeList.RemoveEntry(t1);
	rExcludeList.RemoveEntry(t2);
	rExcludeList.RemoveEntry(t3);
	rExcludeList.RemoveEntry(t4);
	rExcludeList.RemoveEntry(t5);
	rExcludeList.RemoveEntry(t6);
	rExcludeList.RemoveEntry(t7);
	rExcludeList.RemoveEntry(t8);
}			

void TestBackupConfig::TestAlwaysIncludeFileDeepInTree()
{
	// activate node for mFile3 at depth 5, check that 
	// it and its parents are AlwaysIncluded
	ActivateTreeItemWaitEvent(mpTree, mFile3);
	
	CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox  ->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(8, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
	
	wxString expectedTitle = mTestDataDir.GetPath();
	expectedTitle.Append(wxT(" -> testdata ("));
	expectedTitle.Append(wxT("ExcludeDir = "));
	expectedTitle.Append(mTestDepth2Dir.GetPath());
	expectedTitle.Append(wxT(", ExcludeDirsRegex = ^"));
	expectedTitle.Append(mTestDepth2Dir.GetPath());
	expectedTitle.Append(wxT("/, ExcludeFilesRegex = ^"));
	expectedTitle.Append(mTestDepth2Dir.GetPath());
	expectedTitle.Append(wxT("/, AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth2Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth3Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth5Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeFile = "));
	expectedTitle.Append(mTestFile3.GetFullPath());
	expectedTitle.Append(wxT(")"));
	
	CPPUNIT_ASSERT_EQUAL(expectedTitle, 
		mpLocationsListBox  ->GetString(0));			
	CPPUNIT_ASSERT_EQUAL(expectedTitle,
		mpExcludeLocsListBox->GetString(0));
		
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedImageId(), 
		mpTree->GetItemImage(mTestDataDirItem));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedGreyImageId(), 
		mpTree->GetItemImage(mDepth1));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth2));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth3));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth4));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth5));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mFile3));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDir3));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDepth6));
	// mFile4 is grey because its parent is excluded,
	// even though it matches an Exclude entry itself.
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mFile4));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDir4));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mFile5));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDir5));

	// deactivate mFile3 again, check that its alwaysinclude
	// is removed, but not its parents
	ActivateTreeItemWaitEvent(mpTree, mFile3);
	
	expectedTitle = mTestDataDir.GetPath();
	expectedTitle.Append(wxT(" -> testdata ("));
	expectedTitle.Append(wxT("ExcludeDir = "));
	expectedTitle.Append(mTestDepth2Dir.GetPath());
	expectedTitle.Append(wxT(", ExcludeDirsRegex = ^"));
	expectedTitle.Append(mTestDepth2Dir.GetPath());
	expectedTitle.Append(wxT("/, ExcludeFilesRegex = ^"));
	expectedTitle.Append(mTestDepth2Dir.GetPath());
	expectedTitle.Append(wxT("/, AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth2Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth3Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth5Dir.GetPath());
	expectedTitle.Append(wxT(")"));
	
	CPPUNIT_ASSERT_EQUAL(expectedTitle, 
		mpLocationsListBox  ->GetString(0));			
	CPPUNIT_ASSERT_EQUAL(expectedTitle,
		mpExcludeLocsListBox->GetString(0));
		
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedImageId(), 
		mpTree->GetItemImage(mTestDataDirItem));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedGreyImageId(), 
		mpTree->GetItemImage(mDepth1));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth2));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth3));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth4));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDepth5));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mFile3));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDir3));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDepth6));
	// mFile4 is grey because its parent is excluded,
	// even though it matches an Exclude entry itself.
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mFile4));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDir4));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mFile5));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDir5));
}

// Remaining tests are in TestBackupConfig2.cc
