/***************************************************************************
 *            TestCompare.cc
 *
 *  Sun Sep 24 14:45:40 2006
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

#include <sys/time.h> // for utimes()
#include <utime.h> // for utime()

#include <openssl/ssl.h>

#include <wx/button.h>
#include <wx/datectrl.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/spinctrl.h>
#include <wx/treectrl.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <cppunit/TestSuite.h>
#include <cppunit/extensions/HelperMacros.h>

#define TLS_CLASS_IMPLEMENTATION_CPP

#include "SandBox.h"
#include "Box.h"
#include "BackupClientRestore.h"
#include "RaidFileController.h"
#include "BackupDaemonConfigVerify.h"
#include "BackupClientCryptoKeys.h"
#include "BackupStoreConstants.h"
#include "BackupStoreException.h"

#include "main.h"
#include "BoxiApp.h"
#include "ClientConfig.h"
#include "ComparePanel.h"
#include "FileTree.h"
#include "TestBackup.h"
#include "TestBackupConfig.h"
#include "TestCompare.h"
#include "MainFrame.h"
#include "Restore.h"
#include "RestoreFilesPanel.h"
#include "RestoreProgressPanel.h"
#include "ServerConnection.h"

#undef TLS_CLASS_IMPLEMENTATION_CPP

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestCompare, "WxGuiTest");

CppUnit::Test *TestCompare::suite()
{
	CppUnit::TestSuite *suiteOfTests =
		new CppUnit::TestSuite("TestCompare");
	suiteOfTests->addTest(
		new CppUnit::TestCaller<TestCompare>(
			"TestCompare",
			&TestCompare::RunTest));
	return suiteOfTests;
}

static wxTreeItemId GetItemIdFromPath(wxTreeCtrl* pTreeCtrl, wxTreeItemId root, 
	wxString path)
{
	wxTreeItemId none;
	
	wxString pathComponent;
	int indexOfSlash = path.Find(_("/"));
	if (indexOfSlash == -1)
	{
		pathComponent = path;
		path = wxEmptyString;
	}
	else
	{
		CPPUNIT_ASSERT(indexOfSlash > 0);
		pathComponent = path.Mid(0, indexOfSlash);
		path = path.Mid(indexOfSlash + 1);
	}
	CPPUNIT_ASSERT(pathComponent.Length() > 0);
	
	pTreeCtrl->Expand(root);
	
	wxTreeItemIdValue cookie;
	for (wxTreeItemId entry = pTreeCtrl->GetFirstChild(root, cookie);
		entry.IsOk(); entry = pTreeCtrl->GetNextChild(root, cookie))
	{
		if (pTreeCtrl->GetItemText(entry).IsSameAs(pathComponent))
		{
			if (path.Length() == 0)
			{
				// this is it
				return entry;
			}
			else
			{
				// it's below this item
				return GetItemIdFromPath(pTreeCtrl, entry,
					path);
			}
		}
	}
	
	return none;
}

void TestCompare::RunTest()
{
	wxLogAsserter logToCppUnit();
	
	CPPUNIT_ASSERT(!mpBackupPanel->IsShown());	
	ClickButtonWaitEvent(ID_Main_Frame, ID_General_Backup_Button);
	CPPUNIT_ASSERT(mpBackupPanel->IsShown());
	
	// makes tests faster
	mpConfig->MinimumFileAge.Set(0);

	ComparePanel* pComparePanel = wxDynamicCast
	(
		mpMainFrame->FindWindow(ID_Compare_Panel), ComparePanel
	);
	CPPUNIT_ASSERT(pComparePanel);

	CPPUNIT_ASSERT(!pComparePanel->IsShown());	
	ClickButtonWaitEvent(ID_Main_Frame, ID_General_Compare_Button);
	CPPUNIT_ASSERT(pComparePanel->IsShown());

	wxRadioButton* pCompareAllRadio = wxDynamicCast
	(
		pComparePanel->FindWindow(ID_Compare_Panel_All_Locs_Radio), 
		wxRadioButton
	);
	CPPUNIT_ASSERT(pCompareAllRadio);
	CPPUNIT_ASSERT(pCompareAllRadio->GetValue());

	wxRadioButton* pCompareOneRadio = wxDynamicCast
	(
		pComparePanel->FindWindow(ID_Compare_Panel_One_Loc_Radio), 
		wxRadioButton
	);
	CPPUNIT_ASSERT(pCompareOneRadio);
	CPPUNIT_ASSERT(!pCompareOneRadio->GetValue());

	wxChoice* pCompareOneLocChoice = wxDynamicCast
	(
		pComparePanel->FindWindow(ID_Compare_Panel_One_Loc_Choice), 
		wxChoice
	);
	CPPUNIT_ASSERT(pCompareOneLocChoice);
	CPPUNIT_ASSERT(!pCompareOneLocChoice->IsEnabled());
	CPPUNIT_ASSERT_EQUAL(1, pCompareOneLocChoice->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, pCompareOneLocChoice->GetSelection());
	
	wxString testLabel;
	testLabel.Printf(wxT("%s -> %s"), mpTestDataLocation->GetName().c_str(),
		mpTestDataLocation->GetPath().c_str());

	wxRadioButton* pCompareDirRadio = wxDynamicCast
	(
		pComparePanel->FindWindow(ID_Compare_Panel_Dir_Radio), 
		wxRadioButton
	);
	CPPUNIT_ASSERT(pCompareDirRadio);
	CPPUNIT_ASSERT(!pCompareDirRadio->GetValue());

	ClickRadioWaitEvent(pCompareOneRadio);
	CPPUNIT_ASSERT(pCompareOneLocChoice->IsEnabled());
	ClickRadioWaitEvent(pCompareAllRadio);
	CPPUNIT_ASSERT(!pCompareOneLocChoice->IsEnabled());
	ClickRadioWaitEvent(pCompareOneRadio);
	CPPUNIT_ASSERT(pCompareOneLocChoice->IsEnabled());

	CPPUNIT_ASSERT_EQUAL(1, pCompareOneLocChoice->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, pCompareOneLocChoice->GetSelection());
	
	RemoveDefaultLocation();
	CPPUNIT_ASSERT_EQUAL(0, pCompareOneLocChoice->GetCount());
	CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, pCompareOneLocChoice->GetSelection());
	
	SetupDefaultLocation();
	CPPUNIT_ASSERT_EQUAL(1, pCompareOneLocChoice->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, pCompareOneLocChoice->GetSelection());
	
	wxTextCtrl* pCompareDirLocalText = wxDynamicCast
	(
		pComparePanel->FindWindow(ID_Compare_Panel_Dir_Local_Path_Text), 
		wxTextCtrl
	);
	CPPUNIT_ASSERT(pCompareDirLocalText);
	CPPUNIT_ASSERT(!pCompareDirLocalText->IsEnabled());

	wxButton* pCompareDirLocalButton = wxDynamicCast
	(
		pComparePanel->FindWindow(ID_Compare_Panel_Dir_Local_Path_Button), 
		wxButton
	);
	CPPUNIT_ASSERT(pCompareDirLocalButton);
	CPPUNIT_ASSERT(!pCompareDirLocalButton->IsEnabled());

	wxTextCtrl* pCompareDirRemoteText = wxDynamicCast
	(
		pComparePanel->FindWindow(ID_Compare_Panel_Dir_Remote_Path_Text), 
		wxTextCtrl
	);
	CPPUNIT_ASSERT(pCompareDirRemoteText);
	CPPUNIT_ASSERT(!pCompareDirRemoteText->IsEnabled());

	wxButton* pCompareDirRemoteButton = wxDynamicCast
	(
		pComparePanel->FindWindow(ID_Compare_Panel_Dir_Remote_Path_Button), 
		wxButton
	);
	CPPUNIT_ASSERT(pCompareDirRemoteButton);
	CPPUNIT_ASSERT(!pCompareDirRemoteButton->IsEnabled());

	ClickRadioWaitEvent(pCompareDirRadio);
	CPPUNIT_ASSERT(pCompareDirLocalText->IsEnabled());
	CPPUNIT_ASSERT(pCompareDirLocalButton->IsEnabled());
	CPPUNIT_ASSERT(pCompareDirRemoteText->IsEnabled());
	CPPUNIT_ASSERT(pCompareDirRemoteButton->IsEnabled());
	CPPUNIT_ASSERT(!pCompareOneLocChoice->IsEnabled());

	/*
	wxListBox* pLocsList = wxDynamicCast
	(
		pComparePanel->FindWindow(ID_Function_Source_List), wxListBox
	);
	CPPUNIT_ASSERT(pLocsList);
	CPPUNIT_ASSERT_EQUAL(0, pLocsList->GetCount());
	
	wxPanel* pCompareFilesPanel = wxDynamicCast
	(
		mpMainFrame->FindWindow(ID_Compare_Files_Panel), wxPanel
	);
	CPPUNIT_ASSERT(pCompareFilesPanel);

	CPPUNIT_ASSERT(!pCompareFilesPanel->IsShown());	
	ClickButtonWaitEvent(ID_Compare_Panel, ID_Function_Source_Button);
	CPPUNIT_ASSERT(pCompareFilesPanel->IsShown());
	
	{
		wxFileName testSpaceZipFile(_("../test/data/spacetest1.zip"));
		CPPUNIT_ASSERT(testSpaceZipFile.FileExists());
		Unzip(testSpaceZipFile, mTestDataDir, true);
	}

	{
		wxFileName testBaseZipFile(MakeAbsolutePath(
			_("../test/data/test_base.zip")).GetFullPath());
		CPPUNIT_ASSERT(testBaseZipFile.FileExists());
		Unzip(testBaseZipFile, mTestDataDir, true);
	}

	wxFileName test2ZipFile(MakeAbsolutePath(_("../test/data/test2.zip")));
	{
		CPPUNIT_ASSERT(test2ZipFile.FileExists());
		Unzip(test2ZipFile, mTestDataDir, true);
	}

	{
		// Add some files and directories which are marked as excluded
		wxFileName testExcludeZipFile(MakeAbsolutePath(
			_("../test/data/testexclude.zip")));
		CPPUNIT_ASSERT(testExcludeZipFile.FileExists());
		Unzip(testExcludeZipFile, mTestDataDir, true);
	}
	
	wxFileName test3ZipFile(MakeAbsolutePath(_("../test/data/test3.zip")));
	{
		// Add some more files and modify others
		CPPUNIT_ASSERT(test3ZipFile.FileExists());
		Unzip(test3ZipFile, mTestDataDir, true);
	}

	wxTreeCtrl* pCompareTree = wxDynamicCast
	(
		pCompareFilesPanel->FindWindow(ID_Server_File_Tree), 
		wxTreeCtrl
	);
	CPPUNIT_ASSERT(pCompareTree);
	*/
	
	/*	
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(3, 4);
	
	wxTreeItemId rootId = pRestoreTree->GetRootItem();
	CPPUNIT_ASSERT(rootId.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("/ (server root)")),
		pRestoreTree->GetItemText(rootId));
	pRestoreTree->Expand(rootId);
	CPPUNIT_ASSERT_EQUAL((size_t)1, pRestoreTree->GetChildrenCount(rootId, 
		false));
		
	wxTreeItemIdValue cookie;
	wxTreeItemId loc = pRestoreTree->GetFirstChild(rootId, cookie);
	CPPUNIT_ASSERT(loc.IsOk());
	CPPUNIT_ASSERT_EQUAL(wxString(_("testdata")),
		pRestoreTree->GetItemText(loc));
	pRestoreTree->Expand(loc);
	
	FileImageList images;
	
	wxChar* labels[] =
	{
		wxT("anotehr"),
		wxT("dir23"),
		wxT("spacetest"),
		wxT("sub23"),
		wxT("x1"),
		wxT("xx_not_this_dir_ALWAYSINCLUDE"),
		wxT("apropos"),
		wxT("chsh"),
		wxT("df324"),
		wxT("df9834.dsf"),
		wxT("dont.excludethis"),
		wxT("f1.dat"),
		wxT("f45.df"),
		wxT("symlink1"),
		wxT("symlink3"),
		wxT("xx_not_this_dir_22"),
		NULL
	};
	wxChar** pLabel = labels;
	
	// select a single file to restore
	for (wxTreeItemId entry = pRestoreTree->GetFirstChild(loc, cookie);
		entry.IsOk(); entry = pRestoreTree->GetNextChild(loc, cookie))
	{
		wxString label = pRestoreTree->GetItemText(entry);
		wxString expected = *pLabel;
		wxCharBuffer buf = expected.mb_str();
		CPPUNIT_ASSERT_EQUAL(expected, label);
		CPPUNIT_ASSERT_EQUAL_MESSAGE(buf.data(), 
			images.GetEmptyImageId(),
			pRestoreTree->GetItemImage(entry));
		CPPUNIT_ASSERT_MESSAGE(buf.data(),
			pRestoreTree->GetItemTextColour(entry) ==
			wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
		
		pLabel++;
		
		if (label.IsSameAs(_("df9834.dsf")))
		{
			ActivateTreeItemWaitEvent(pRestoreTree, entry);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(buf.data(),
				images.GetCheckedImageId(),
				pRestoreTree->GetItemImage(entry));
		}
	}
	
	if (*pLabel != NULL)
	{
		wxCharBuffer buf = wxString(*pLabel).mb_str();
		CPPUNIT_ASSERT_MESSAGE(buf.data(), *pLabel == NULL);
	}

	// check that the restore spec contains what we expect
	{
		RestoreSpec& rRestoreSpec(pRestorePanel->GetRestoreSpec());
		const RestoreSpecEntry::Vector entries = rRestoreSpec.GetEntries();
		CPPUNIT_ASSERT_EQUAL((size_t)1, entries.size());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/df9834.dsf")),
			entries[0].GetNode().GetFullPath());
		CPPUNIT_ASSERT(entries[0].IsInclude());
	}

	CPPUNIT_ASSERT_EQUAL(images.GetPartialImageId(),
		pRestoreTree->GetItemImage(loc));
	CPPUNIT_ASSERT_EQUAL(images.GetPartialImageId(),
		pRestoreTree->GetItemImage(rootId));
	
	CPPUNIT_ASSERT_EQUAL(1, pLocsList->GetCount());
	CPPUNIT_ASSERT_EQUAL(wxString(_("+ /testdata/df9834.dsf")),
		pLocsList->GetString(0));
	
	RestoreProgressPanel* pRestoreProgressPanel = wxDynamicCast
	(
		mpMainFrame->FindWindow(ID_Restore_Progress_Panel), RestoreProgressPanel
	);
	CPPUNIT_ASSERT(pRestoreProgressPanel);

	wxRadioButton* pNewLocRadio = wxDynamicCast
	(
		pRestorePanel->FindWindow(ID_Restore_Panel_New_Location_Radio), 
		wxRadioButton
	);
	CPPUNIT_ASSERT(!pNewLocRadio);

	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown());
	MessageBoxSetResponse(BM_RESTORE_FAILED_INVALID_DESTINATION_PATH, wxOK);
	ClickButtonWaitEvent(ID_Restore_Panel, ID_Function_Start_Button);
	MessageBoxCheckFired();
	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown());

	wxTextCtrl* pNewLocText = wxDynamicCast
	(
		pRestorePanel->FindWindow(ID_Restore_Panel_New_Location_Text), 
		wxTextCtrl
	);
	CPPUNIT_ASSERT(pNewLocText);
	wxFileName restoreDest(mBaseDir.GetFullPath(), _("restore"));
	SetTextCtrlValue(pNewLocText, restoreDest.GetFullPath());

	wxListBox* pRestoreErrorList = wxDynamicCast
	(
		pRestoreProgressPanel->FindWindow(ID_BackupProgress_ErrorList), 
		wxListBox
	);
	CPPUNIT_ASSERT(pRestoreErrorList);
	CPPUNIT_ASSERT_EQUAL(0, pRestoreErrorList->GetCount());

	#define CHECK_RESTORE_OK(files, bytes) \
	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown()); \
	ClickButtonWaitEvent(ID_Restore_Panel, ID_Function_Start_Button); \
	CPPUNIT_ASSERT(pRestoreProgressPanel->IsShown()); \
	CPPUNIT_ASSERT(pRestoreErrorList->GetCount() >= 1); \
	CPPUNIT_ASSERT_EQUAL(wxString(_("Restore Finished")), \
		pRestoreErrorList->GetString(0)); \
	CPPUNIT_ASSERT_EQUAL(1, pRestoreErrorList->GetCount()); \
	ClickButtonWaitEvent(ID_Restore_Progress_Panel, wxID_CANCEL); \
	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown()); \
	mpMainFrame->GetConnection()->Disconnect(); \
	CPPUNIT_ASSERT_EQUAL(wxString(_(#files)), \
		pRestoreProgressPanel->GetNumFilesTotalString()); \
	CPPUNIT_ASSERT_EQUAL(wxString(_(bytes)), \
		pRestoreProgressPanel->GetNumBytesTotalString()); \
	CPPUNIT_ASSERT_EQUAL(wxString(_(#files)), \
		pRestoreProgressPanel->GetNumFilesDoneString()); \
	CPPUNIT_ASSERT_EQUAL(wxString(_(bytes)), \
		pRestoreProgressPanel->GetNumBytesDoneString()); \
	CPPUNIT_ASSERT_EQUAL(wxString(_("0")), \
		pRestoreProgressPanel->GetNumFilesRemainingString()); \
	CPPUNIT_ASSERT_EQUAL(wxString(_("0 B")), \
		pRestoreProgressPanel->GetNumBytesRemainingString()); \
	CPPUNIT_ASSERT_EQUAL(files, pRestoreProgressPanel->GetProgressPos()); \
	CPPUNIT_ASSERT_EQUAL(files, pRestoreProgressPanel->GetProgressMax());
	
	CHECK_RESTORE_OK(1, "18 kB");
	
	CPPUNIT_ASSERT(restoreDest.DirExists());
	wxFileName testdataRestored = MakeAbsolutePath(restoreDest, 
		_("testdata"));
	CPPUNIT_ASSERT(testdataRestored.DirExists());
	wxFileName df9834_dsfRestored = MakeAbsolutePath(testdataRestored, 
		_("df9834.dsf"));
	CPPUNIT_ASSERT(df9834_dsfRestored.FileExists());
	wxFileName df9834_dsfOriginal = MakeAbsolutePath(mTestDataDir, 
		_("df9834.dsf"));
	CompareFiles(df9834_dsfOriginal, df9834_dsfRestored);
	CPPUNIT_ASSERT(wxRemoveFile(df9834_dsfRestored.GetFullPath()));
	CPPUNIT_ASSERT(wxRmdir(testdataRestored.GetFullPath()));
	
	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown());
	MessageBoxSetResponse(BM_RESTORE_FAILED_OBJECT_ALREADY_EXISTS, wxOK);
	ClickButtonWaitEvent(ID_Restore_Panel, ID_Function_Start_Button);
	MessageBoxCheckFired();
	CPPUNIT_ASSERT(!pRestoreProgressPanel->IsShown());
	
	CPPUNIT_ASSERT(wxRmdir(restoreDest.GetFullPath()));
	
	// now select a whole directory to restore
	// leave the single file selected as well
	for (wxTreeItemId entry = pRestoreTree->GetFirstChild(loc, cookie);
		entry.IsOk(); entry = pRestoreTree->GetNextChild(loc, cookie))
	{
		if (pRestoreTree->GetItemText(entry).IsSameAs(_("sub23")))
		{
			ActivateTreeItemWaitEvent(pRestoreTree, entry);
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
				pRestoreTree->GetItemImage(entry));
		}
		else if (pRestoreTree->GetItemText(entry).IsSameAs(_("df9834.dsf")))
		{
			CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
				pRestoreTree->GetItemImage(entry));
		}
		else
		{
			CPPUNIT_ASSERT_EQUAL(images.GetEmptyImageId(),
				pRestoreTree->GetItemImage(entry));
		}
	}
	
	CHECK_RESTORE_OK(13, "124 kB");
	
	CPPUNIT_ASSERT(restoreDest.DirExists());
	wxFileName sub23 = MakeAbsolutePath(restoreDest, _("testdata/sub23"));
	CPPUNIT_ASSERT(sub23.DirExists());
	CPPUNIT_ASSERT(df9834_dsfRestored.FileExists());
	CompareFiles(df9834_dsfOriginal, df9834_dsfRestored);
	CompareExpectNoDifferences(rClientConfig, mTlsContext, _("testdata/sub23"),
		sub23);
	DeleteRecursive(sub23);
	CPPUNIT_ASSERT(wxRemoveFile(df9834_dsfRestored.GetFullPath()));
	CPPUNIT_ASSERT(wxRmdir(testdataRestored.GetFullPath()));
	CPPUNIT_ASSERT(wxRmdir(restoreDest.GetFullPath()));
	
	// check that the restore spec contains what we expect
	{
		RestoreSpec& rRestoreSpec(pRestorePanel->GetRestoreSpec());
		const RestoreSpecEntry::Vector entries = rRestoreSpec.GetEntries();
		CPPUNIT_ASSERT_EQUAL((size_t)2, entries.size());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/df9834.dsf")),
			entries[0].GetNode().GetFullPath());
		CPPUNIT_ASSERT(entries[0].IsInclude());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23")),
			entries[1].GetNode().GetFullPath());
		CPPUNIT_ASSERT(entries[1].IsInclude());

		// and remove the first one		
		rRestoreSpec.Remove(entries[0]);
	}
	
	wxTreeItemId sub23id = GetItemIdFromPath(pRestoreTree, loc, 
		_("sub23"));
	CPPUNIT_ASSERT(sub23id.IsOk());
	CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
		pRestoreTree->GetItemImage(sub23id));
	
	wxTreeItemId dhsfdss = GetItemIdFromPath(pRestoreTree, loc, 
		_("sub23/dhsfdss"));
	CPPUNIT_ASSERT(dhsfdss.IsOk());
	CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(),
		pRestoreTree->GetItemImage(dhsfdss));

	// create a weird configuration, with an item included under
	// another included item (i.e. double included)
	{		
		ActivateTreeItemWaitEvent(pRestoreTree, sub23id);
		CPPUNIT_ASSERT_EQUAL(images.GetEmptyImageId(),
			pRestoreTree->GetItemImage(sub23id));
		CPPUNIT_ASSERT_EQUAL(images.GetEmptyImageId(),
			pRestoreTree->GetItemImage(dhsfdss));
		
		ActivateTreeItemWaitEvent(pRestoreTree, dhsfdss);
		CPPUNIT_ASSERT_EQUAL(images.GetPartialImageId(),
			pRestoreTree->GetItemImage(sub23id));
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
			pRestoreTree->GetItemImage(dhsfdss));

		ActivateTreeItemWaitEvent(pRestoreTree, sub23id);
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
			pRestoreTree->GetItemImage(sub23id));
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(),
			pRestoreTree->GetItemImage(dhsfdss));
	}

	{
		RestoreSpec& rRestoreSpec(pRestorePanel->GetRestoreSpec());
		const RestoreSpecEntry::Vector entries = rRestoreSpec.GetEntries();
		CPPUNIT_ASSERT_EQUAL((size_t)2, entries.size());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23")),
			entries[0].GetNode().GetFullPath());
		CPPUNIT_ASSERT(entries[0].IsInclude());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23/dhsfdss")),
			entries[1].GetNode().GetFullPath());
		CPPUNIT_ASSERT(entries[1].IsInclude());
	}
	
	// check that restore works as expected
	CHECK_RESTORE_OK(12, "106 kB");
	
	CPPUNIT_ASSERT(restoreDest.DirExists());
	CPPUNIT_ASSERT(sub23.DirExists());
	CPPUNIT_ASSERT(!df9834_dsfRestored.FileExists());
	CompareExpectNoDifferences(rClientConfig, mTlsContext, _("testdata/sub23"),
		sub23);
	DeleteRecursive(sub23);
	CPPUNIT_ASSERT(wxRmdir(testdataRestored.GetFullPath()));
	CPPUNIT_ASSERT(wxRmdir(restoreDest.GetFullPath()));

	// create a weirder configuration, with double include and the
	// included item also excluded. The include should be ignored,
	// and the exclude used.
	{
		ActivateTreeItemWaitEvent(pRestoreTree, dhsfdss);
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
			pRestoreTree->GetItemImage(sub23id));
		CPPUNIT_ASSERT_EQUAL(images.GetCrossedImageId(),
			pRestoreTree->GetItemImage(dhsfdss));
	}

	{
		RestoreSpec& rRestoreSpec(pRestorePanel->GetRestoreSpec());
		const RestoreSpecEntry::Vector entries = rRestoreSpec.GetEntries();
		CPPUNIT_ASSERT_EQUAL((size_t)3, entries.size());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23")),
			entries[0].GetNode().GetFullPath());
		CPPUNIT_ASSERT(entries[0].IsInclude());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23/dhsfdss")),
			entries[1].GetNode().GetFullPath());
		CPPUNIT_ASSERT(entries[1].IsInclude());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23/dhsfdss")),
			entries[2].GetNode().GetFullPath());
		CPPUNIT_ASSERT(!(entries[2].IsInclude()));
	}
	
	// check that restore works as expected
	CHECK_RESTORE_OK(8, "76 kB");
	
	CPPUNIT_ASSERT(restoreDest.DirExists());
	CPPUNIT_ASSERT(sub23.DirExists());
	CPPUNIT_ASSERT(!df9834_dsfRestored.FileExists());
	CompareExpectDifferences(rClientConfig, mTlsContext, _("testdata/sub23"),
		sub23, 1, 0);
	DeleteRecursive(sub23);
	CPPUNIT_ASSERT(wxRmdir(testdataRestored.GetFullPath()));
	CPPUNIT_ASSERT(wxRmdir(restoreDest.GetFullPath()));

	wxTreeItemId bfdlink_h = GetItemIdFromPath(pRestoreTree, dhsfdss, 
		_("bfdlink.h"));
	CPPUNIT_ASSERT(bfdlink_h.IsOk());

	wxTreeItemId dfsfd = GetItemIdFromPath(pRestoreTree, dhsfdss, 
		_("dfsfd"));
	CPPUNIT_ASSERT(dfsfd.IsOk());

	wxTreeItemId a_out_h = GetItemIdFromPath(pRestoreTree, dfsfd, 
		_("a.out.h"));
	CPPUNIT_ASSERT(a_out_h.IsOk());

	// include a file and a dir inside the excluded dir, check that
	// both are restored properly.
	{
		CPPUNIT_ASSERT_EQUAL(images.GetCrossedGreyImageId(),
			pRestoreTree->GetItemImage(bfdlink_h));
	
		ActivateTreeItemWaitEvent(pRestoreTree, bfdlink_h);
		CPPUNIT_ASSERT_EQUAL(images.GetCrossedImageId(),
			pRestoreTree->GetItemImage(dhsfdss));
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
			pRestoreTree->GetItemImage(bfdlink_h));

		CPPUNIT_ASSERT_EQUAL(images.GetCrossedGreyImageId(),
			pRestoreTree->GetItemImage(dfsfd));
	
		ActivateTreeItemWaitEvent(pRestoreTree, dfsfd);
		CPPUNIT_ASSERT_EQUAL(images.GetCrossedImageId(),
			pRestoreTree->GetItemImage(dhsfdss));
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
			pRestoreTree->GetItemImage(dfsfd));

		CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(),
			pRestoreTree->GetItemImage(a_out_h));
	}

	{
		RestoreSpec& rRestoreSpec(pRestorePanel->GetRestoreSpec());
		const RestoreSpecEntry::Vector entries = rRestoreSpec.GetEntries();
		CPPUNIT_ASSERT_EQUAL((size_t)5, entries.size());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23")),
			entries[0].GetNode().GetFullPath());
		CPPUNIT_ASSERT(entries[0].IsInclude());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23/dhsfdss")),
			entries[1].GetNode().GetFullPath());
		CPPUNIT_ASSERT(entries[1].IsInclude());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/testdata/sub23/dhsfdss")),
			entries[2].GetNode().GetFullPath());
		CPPUNIT_ASSERT(!(entries[2].IsInclude()));
		CPPUNIT_ASSERT_EQUAL(
			wxString(_("/testdata/sub23/dhsfdss/bfdlink.h")),
			entries[3].GetNode().GetFullPath());
		CPPUNIT_ASSERT(entries[3].IsInclude());
		CPPUNIT_ASSERT_EQUAL(
			wxString(_("/testdata/sub23/dhsfdss/dfsfd")),
			entries[4].GetNode().GetFullPath());
		CPPUNIT_ASSERT(entries[4].IsInclude());
	}
	
	// check that restore works as expected
	CHECK_RESTORE_OK(10, "96 kB");
	
	CPPUNIT_ASSERT(restoreDest.DirExists());
	CPPUNIT_ASSERT(sub23.DirExists());
	CompareExpectDifferences(rClientConfig, mTlsContext, _("testdata/sub23"),
		sub23, 4, 0);
	DeleteRecursive(sub23);
	CPPUNIT_ASSERT(wxRmdir(testdataRestored.GetFullPath()));
	CPPUNIT_ASSERT(wxRmdir(restoreDest.GetFullPath()));

	// clear all entries, select the server root
	{
		CPPUNIT_ASSERT_EQUAL(images.GetCrossedImageId(),
			pRestoreTree->GetItemImage(dhsfdss));
		ActivateTreeItemWaitEvent(pRestoreTree, dhsfdss);
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedGreyImageId(),
			pRestoreTree->GetItemImage(dhsfdss));

		CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
			pRestoreTree->GetItemImage(sub23id));		
		ActivateTreeItemWaitEvent(pRestoreTree, sub23id);
		CPPUNIT_ASSERT_EQUAL(images.GetPartialImageId(),
			pRestoreTree->GetItemImage(sub23id));

		CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
			pRestoreTree->GetItemImage(dhsfdss));
		ActivateTreeItemWaitEvent(pRestoreTree, dhsfdss);
		CPPUNIT_ASSERT_EQUAL(images.GetPartialImageId(),
			pRestoreTree->GetItemImage(dhsfdss));
		
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
			pRestoreTree->GetItemImage(bfdlink_h));
		ActivateTreeItemWaitEvent(pRestoreTree, bfdlink_h);
		CPPUNIT_ASSERT_EQUAL(images.GetEmptyImageId(),
			pRestoreTree->GetItemImage(bfdlink_h));
	
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
			pRestoreTree->GetItemImage(dfsfd));
		ActivateTreeItemWaitEvent(pRestoreTree, dfsfd);
		CPPUNIT_ASSERT_EQUAL(images.GetEmptyImageId(),
			pRestoreTree->GetItemImage(dfsfd));
	
		CPPUNIT_ASSERT_EQUAL(images.GetEmptyImageId(),
			pRestoreTree->GetItemImage(dhsfdss));

		CPPUNIT_ASSERT_EQUAL(images.GetEmptyImageId(),
			pRestoreTree->GetItemImage(rootId));
		ActivateTreeItemWaitEvent(pRestoreTree, rootId);
		CPPUNIT_ASSERT_EQUAL(images.GetCheckedImageId(),
			pRestoreTree->GetItemImage(rootId));
	}

	RestoreSpec& rRestoreSpec(pRestorePanel->GetRestoreSpec());
	CPPUNIT_ASSERT(!rRestoreSpec.GetRestoreToDateEnabled());

	{
		const RestoreSpecEntry::Vector entries = rRestoreSpec.GetEntries();
		CPPUNIT_ASSERT_EQUAL((size_t)1, entries.size());
		CPPUNIT_ASSERT_EQUAL(wxString(_("/")),
			entries[0].GetNode().GetFullPath());
		CPPUNIT_ASSERT(entries[0].IsInclude());
	}
	
	// check that restore works as expected
	CHECK_RESTORE_OK(32, "262 kB");
	
	CPPUNIT_ASSERT(restoreDest.DirExists());
	CPPUNIT_ASSERT(testdataRestored.DirExists());
	CPPUNIT_ASSERT(sub23.DirExists());
	CompareExpectNoDifferences(rClientConfig, mTlsContext, _("testdata"),
		testdataRestored);
	DeleteRecursive(testdataRestored);
	CPPUNIT_ASSERT(wxRmdir(restoreDest.GetFullPath()));
	
	// check restoring to a specified date
	wxCheckBox* pToDateCheckBox = wxDynamicCast
	(
		pRestorePanel->FindWindow(ID_Restore_Panel_To_Date_Checkbox), 
		wxCheckBox
	);
	CPPUNIT_ASSERT(pToDateCheckBox);
	CPPUNIT_ASSERT(!pToDateCheckBox->GetValue());
	
	wxDatePickerCtrl* pDatePicker = wxDynamicCast
	(
		pRestorePanel->FindWindow(ID_Restore_Panel_Date_Picker), 
		wxDatePickerCtrl
	);
	CPPUNIT_ASSERT(pDatePicker);
	
	wxSpinCtrl* pHourSpin = wxDynamicCast
	(
		pRestorePanel->FindWindow(ID_Restore_Panel_Hour_Spin), 
		wxSpinCtrl
	);
	CPPUNIT_ASSERT(pHourSpin);

	wxSpinCtrl* pMinSpin = wxDynamicCast
	(
		pRestorePanel->FindWindow(ID_Restore_Panel_Min_Spin), 
		wxSpinCtrl
	);
	CPPUNIT_ASSERT(pMinSpin);

	CPPUNIT_ASSERT(!pDatePicker->IsEnabled());
	CPPUNIT_ASSERT(!pHourSpin->IsEnabled());
	CPPUNIT_ASSERT(!pMinSpin->IsEnabled());

	// check that connection index is being incremented with each connection
	CPPUNIT_ASSERT_EQUAL(6, pRestoreProgressPanel->GetConnectionIndex());

	// check that old and deleted files are not restored, and that 
	// node cache is being invalidated when connection index changes
	// (otherwise the restore will think that deleted files are not deleted)
	DeleteRecursive(mTestDataDir);
	CPPUNIT_ASSERT(wxMkdir(mTestDataDir.GetFullPath()));
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);

	Unzip(test2ZipFile, mTestDataDir, true);
	CHECK_COMPARE_LOC_FAILS(2, 0, 0, 0);
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);
	CHECK_RESTORE_OK(11, "86 kB");
	DeleteRecursive(restoreDest);
	CPPUNIT_ASSERT_EQUAL(7, pRestoreProgressPanel->GetConnectionIndex());

	Unzip(test3ZipFile, mTestDataDir, true);
	CHECK_COMPARE_LOC_FAILS(12, 0, 0, 0);
	CHECK_BACKUP_OK();
	CHECK_COMPARE_LOC_OK(0, 0);
	CHECK_RESTORE_OK(17, "160 kB");
	DeleteRecursive(restoreDest);
	CPPUNIT_ASSERT_EQUAL(8, pRestoreProgressPanel->GetConnectionIndex());

	wxDateTime now = wxDateTime::Now();
	CheckBoxWaitEvent(pToDateCheckBox);
	CPPUNIT_ASSERT(pDatePicker->IsEnabled());
	CPPUNIT_ASSERT(pHourSpin->IsEnabled());
	CPPUNIT_ASSERT(pMinSpin->IsEnabled());
	
	{
		wxDateTime picked = pDatePicker->GetValue();
		CPPUNIT_ASSERT(picked.IsValid());
		CPPUNIT_ASSERT_EQUAL(now.GetYear(),   picked.GetYear());
		CPPUNIT_ASSERT_EQUAL(now.GetMonth(),  picked.GetMonth());
		CPPUNIT_ASSERT_EQUAL(now.GetDay(),    picked.GetDay());
		CPPUNIT_ASSERT_EQUAL((int)now.GetHour(),   pHourSpin->GetValue());
		CPPUNIT_ASSERT_EQUAL((int)now.GetMinute(), pMinSpin ->GetValue());
		CPPUNIT_ASSERT(picked.IsSameDate(now));
	}

	{
		CPPUNIT_ASSERT(rRestoreSpec.GetRestoreToDateEnabled());
		wxDateTime picked = rRestoreSpec.GetRestoreToDate();
		CPPUNIT_ASSERT(picked.IsValid());
		CPPUNIT_ASSERT_EQUAL(now.GetYear(),   picked.GetYear());
		CPPUNIT_ASSERT_EQUAL(now.GetMonth(),  picked.GetMonth());
		CPPUNIT_ASSERT_EQUAL(now.GetDay(),    picked.GetDay());
		CPPUNIT_ASSERT_EQUAL(now.GetHour(),   picked.GetHour());
		CPPUNIT_ASSERT_EQUAL(now.GetMinute(), picked.GetMinute());
		now.SetSecond(0);
		now.SetMillisecond(0);
		CPPUNIT_ASSERT(picked.IsEqualTo(now));
	}
	
	// Restore to a date just later than all the files in test2,
	// but before all the files in test3. This date is calculated
	// automatically as half-way between the latest file in test2,
	// and the earliest one in test3.

	std::auto_ptr<wxZipEntry> beforeEntry = FindZipEntry(test2ZipFile,
		_("sub23/dhsfdss/bfdlink.h"));
	CPPUNIT_ASSERT(beforeEntry.get());
	wxDateTime before = beforeEntry->GetDateTime();

	std::auto_ptr<wxZipEntry> afterEntry = FindZipEntry(test3ZipFile,
		_("apropos"));
	CPPUNIT_ASSERT(afterEntry.get());
	wxDateTime after = afterEntry->GetDateTime();
	CPPUNIT_ASSERT(after.IsLaterThan(before));
	
	wxTimeSpan diff = after.Subtract(before);
	CPPUNIT_ASSERT(diff.GetSeconds() > 2);
	wxTimeSpan smalldiff = wxTimeSpan::Seconds(diff.GetSeconds().ToLong() / 2);
	wxDateTime epoch = before;
	epoch.Add(smalldiff);
	CPPUNIT_ASSERT(epoch.IsLaterThan(before));
	CPPUNIT_ASSERT(epoch.IsEarlierThan(after));

	SetDatePickerValue(pDatePicker, epoch);
	SetSpinCtrlValue(pHourSpin, epoch.GetHour());
	SetSpinCtrlValue(pMinSpin,  epoch.GetMinute());

	{
		CPPUNIT_ASSERT(rRestoreSpec.GetRestoreToDateEnabled());
		wxDateTime picked = rRestoreSpec.GetRestoreToDate();
		CPPUNIT_ASSERT(picked.IsValid());
		CPPUNIT_ASSERT_EQUAL(epoch.GetYear(),   picked.GetYear());
		CPPUNIT_ASSERT_EQUAL(epoch.GetMonth(),  picked.GetMonth());
		CPPUNIT_ASSERT_EQUAL(epoch.GetDay(),    picked.GetDay());
		CPPUNIT_ASSERT_EQUAL(epoch.GetHour(),   picked.GetHour());
		CPPUNIT_ASSERT_EQUAL(epoch.GetMinute(), picked.GetMinute());
		epoch.SetSecond(0);
		epoch.SetMillisecond(0);
		CPPUNIT_ASSERT(picked.IsEqualTo(epoch));
	}

	CHECK_RESTORE_OK(10, "62 kB");

	// Create a directory to hold the set of files that we expect to
	// be restored. Because we can't set the seconds of epoch,
	// we can't get exactly the files in test2 with no files from test3.
	//
	// We use 2003-03-29 19:49:00, and one file from test2, 
	// sub23/dhsfdss/bfdlink.h, is later and will be excluded.
	//
	// Also, Box does not store timestamps for directories, so we
	// will have restored one directory, sub23/ping!, which should
	// not be here (but not its contents, sub23/ping!/apply).
	// 
	// So there should be 2 differences to start with.
	
	wxFileName expectedRestoreDir(mBaseDir.GetFullPath(), _("expected"));
	CPPUNIT_ASSERT(expectedRestoreDir.Mkdir(0700));
	wxFileName expectedTestDataDir(expectedRestoreDir.GetFullPath(), 
		_("testdata"));
	CPPUNIT_ASSERT(expectedTestDataDir.Mkdir(0700));
	Unzip(test2ZipFile, expectedTestDataDir, true);
	
	CompareDirs(expectedRestoreDir, restoreDest, 2);
	
	// Now fix those differences.
	{
		wxFileName removeMe = MakeAbsolutePath(expectedRestoreDir,
			_("testdata/sub23/dhsfdss/bfdlink.h"));
		CPPUNIT_ASSERT(removeMe.FileExists());
		CPPUNIT_ASSERT(wxRemoveFile(removeMe.GetFullPath()));
		CPPUNIT_ASSERT(!removeMe.FileExists());
	}
	{
		wxFileName createMe = MakeAbsolutePath(expectedRestoreDir,
			_("testdata/sub23/ping!"));
		CPPUNIT_ASSERT(!createMe.DirExists());
		CPPUNIT_ASSERT(wxMkdir(createMe.GetFullPath()));
	}
	
	// Now there should be no differences.
	CompareDirs(expectedRestoreDir, restoreDest, 0);
	
	DeleteRecursive(restoreDest);
	DeleteRecursive(expectedRestoreDir);

	DeleteRecursive(mTestDataDir);
	CPPUNIT_ASSERT(mStoreConfigFileName.FileExists());
	CPPUNIT_ASSERT(wxRemoveFile(mStoreConfigFileName.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mAccountsFile.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mRaidConfigFile.GetFullPath()));
	CPPUNIT_ASSERT(wxRemoveFile(mClientConfigFile.GetFullPath()));		
	CPPUNIT_ASSERT(mConfDir.Rmdir());
	DeleteRecursive(mStoreDir);
	CPPUNIT_ASSERT(mBaseDir.Rmdir());
	*/
}
