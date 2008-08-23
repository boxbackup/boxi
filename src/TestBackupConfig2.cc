/***************************************************************************
 *            TestBackupConfig2.cc
 *
 *  Sun Nov 25 2007
 *  Copyright 2007-2008 Chris Wilson
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

// #include <openssl/bio.h>

// class SSL;
// class SSL_CTX;

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

// #define TLS_CLASS_IMPLEMENTATION_CPP

#include "SandBox.h"
#include "Box.h"
// #include "BackupClientRestore.h"
// #include "BackupStoreDaemon.h"
// #include "RaidFileController.h"
// #include "BackupStoreAccountDatabase.h"
// #include "BackupStoreAccounts.h"
// #include "BackupContext.h"
// #include "autogen_BackupProtocolServer.h"
// #include "BackupStoreConfigVerify.h"
// #include "BackupQueries.h"
// #include "BackupDaemonConfigVerify.h"
// #include "BackupClientCryptoKeys.h"
// #include "BackupStoreConstants.h"
// #include "BackupStoreInfo.h"
// #include "StoreStructure.h"
// #include "NamedLock.h"

#include "main.h"
#include "BoxiApp.h"
#include "ClientConfig.h"
#include "MainFrame.h"
// #include "SetupWizard.h"
// #include "SslConfig.h"
#include "TestBackupConfig.h"
// #include "ServerConnection.h"
// #undef TLS_CLASS_IMPLEMENTATION_CPP

void TestBackupConfig::TestAddAlwaysIncludeInTree()
{
	// activate node for mDir3 at depth 5, check that 
	// it and its parents and children are AlwaysIncluded
	ActivateTreeItemWaitEvent(mpTree, mDir3);
	
	CPPUNIT_ASSERT_EQUAL(1,  mpLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(0,  mpLocationsListBox  ->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1,  mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0,  mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(10, mpExcludeListBox    ->GetCount());
	CPPUNIT_ASSERT_EQUAL(0,  mpExcludeListBox    ->GetSelection());
	
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
	expectedTitle.Append(wxT(", AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDir3.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDirsRegex = ^"));
	expectedTitle.Append(mTestDir3.GetPath());
	expectedTitle.Append(wxT("/, AlwaysIncludeFilesRegex = ^"));
	expectedTitle.Append(mTestDir3.GetPath());
	expectedTitle.Append(wxT("/)"));
	
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
		mpTree->GetItemImage(mDir3));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mFile3));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDepth6));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDir4));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mFile4));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDir5));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mFile5));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDir6));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mFile6));
}

void TestBackupConfig::TestExcludeUnderAlwaysIncludePromptsToRemove()
{
	// activate node mDir6 under mDir3, check for the expected
	// warning message that excluding from AlwaysInclude
	// is not possible, say no, check that the AlwaysInclude
	// entry is not removed.
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(10, mpExcludeListBox->GetCount());
	MessageBoxSetResponse(BM_EXCLUDE_MUST_REMOVE_ALWAYSINCLUDE,
		wxNO);
	ActivateTreeItemWaitEvent(mpTree, mDir6);
	MessageBoxCheckFired();
	CPPUNIT_ASSERT_EQUAL(10, mpExcludeListBox->GetCount());

	// activate node mDir6 again, check for the expected
	// warning message that excluding from AlwaysInclude
	// is not possible, say yes, check that the 
	// AlwaysInclude entry is is removed.
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(10, mpExcludeListBox->GetCount());
	MessageBoxSetResponse(BM_EXCLUDE_MUST_REMOVE_ALWAYSINCLUDE,
		wxYES);
	ActivateTreeItemWaitEvent(mpTree, mDir6);
	MessageBoxCheckFired();
	CPPUNIT_ASSERT_EQUAL(9, mpExcludeListBox->GetCount());

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
	expectedTitle.Append(wxT(", AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDir3.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeFilesRegex = ^"));
	expectedTitle.Append(mTestDir3.GetPath());
	expectedTitle.Append(wxT("/)"));
	
	CPPUNIT_ASSERT_EQUAL(expectedTitle, 
		mpLocationsListBox  ->GetString(0));
	CPPUNIT_ASSERT_EQUAL(expectedTitle,
		mpExcludeLocsListBox->GetString(0));
		
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mDir3));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDir6));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetAlwaysImageId(), 
		mpTree->GetItemImage(mFile6));
}

void TestBackupConfig::TestRemoveMultipleAlwaysIncludesInTree()
{
	// clear out our AlwaysIncludes one at a time
	
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(9, mpExcludeListBox->GetCount());

	// this one is complex, so it requires confirmation
	MessageBoxSetResponse(BM_EXCLUDE_MUST_REMOVE_ALWAYSINCLUDE,
		wxYES);
	ActivateTreeItemWaitEvent(mpTree, mFile6);
	MessageBoxCheckFired();

	ActivateTreeItemWaitEvent(mpTree, mDir3);
	ActivateTreeItemWaitEvent(mpTree, mDepth5);
	ActivateTreeItemWaitEvent(mpTree, mDepth4);
	ActivateTreeItemWaitEvent(mpTree, mDepth3);
	ActivateTreeItemWaitEvent(mpTree, mDepth2);

	wxString expectedTitle = mTestDataDir.GetPath();
	expectedTitle.Append(wxT(" -> testdata ("));
	expectedTitle.Append(wxT("ExcludeDir = "));
	expectedTitle.Append(mTestDepth2Dir.GetPath());
	expectedTitle.Append(wxT(")"));
	
	CPPUNIT_ASSERT_EQUAL(expectedTitle, 
		mpLocationsListBox  ->GetString(0));
	CPPUNIT_ASSERT_EQUAL(expectedTitle,
		mpExcludeLocsListBox->GetString(0));

	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeListBox->GetCount());
}

void TestBackupConfig::TestAlwaysIncludeDirWithSubdirsDeepInTree()
{
	// now activate node at mDepth4
	
	CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox  ->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeListBox    ->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox    ->GetSelection());

	ActivateTreeItemWaitEvent(mpTree, mDepth4);

	wxString expectedTitle = mTestDataDir.GetPath();
	expectedTitle.Append(_(" -> testdata (ExcludeDir = "));
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
	expectedTitle.Append(wxT(", AlwaysIncludeDirsRegex = ^"));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(wxT("/, AlwaysIncludeFilesRegex = ^"));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(_("/)"));

	CPPUNIT_ASSERT_EQUAL(expectedTitle, 
		mpLocationsListBox  ->GetString(0));
	CPPUNIT_ASSERT_EQUAL(expectedTitle,
		mpExcludeLocsListBox->GetString(0));

	CPPUNIT_ASSERT_EQUAL(8, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
}

void TestBackupConfig::TestRemoveExcludeInTree()
{
	// Activate node depth2 to remove the AlwaysInclude
	
	CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox  ->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(8, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());

	ActivateTreeItemWaitEvent(mpTree, mDepth2);

	wxString expectedTitle = mTestDataDir.GetPath();
	expectedTitle.Append(_(" -> testdata (ExcludeDir = "));
	expectedTitle.Append(mTestDepth2Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth3Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDirsRegex = ^"));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(wxT("/, AlwaysIncludeFilesRegex = ^"));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(_("/)"));

	CPPUNIT_ASSERT_EQUAL(expectedTitle, 
		mpLocationsListBox  ->GetString(0));
	CPPUNIT_ASSERT_EQUAL(expectedTitle,
		mpExcludeLocsListBox->GetString(0));

	// Activate it again to remove the exclude, and check that the
	// children at depth 4 and below are still AlwaysIncluded

	ActivateTreeItemWaitEvent(mpTree, mDepth2);

	expectedTitle = mTestDataDir.GetPath();
	expectedTitle.Append(_(" -> testdata (AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth3Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDirsRegex = ^"));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(wxT("/, AlwaysIncludeFilesRegex = ^"));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(_("/)"));

	CPPUNIT_ASSERT_EQUAL(expectedTitle, 
		mpLocationsListBox  ->GetString(0));
	CPPUNIT_ASSERT_EQUAL(expectedTitle,
		mpExcludeLocsListBox->GetString(0));
		
	CPPUNIT_ASSERT_EQUAL(4, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
}

void TestBackupConfig::TestRemoveAlwaysIncludeInTree()
{
	// activate node at depth 4, check that the 
	// AlwaysInclude entry is removed.

	CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox  ->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(4, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());

	ActivateTreeItemWaitEvent(mpTree, mDepth4);
	
	wxString expectedTitle = mTestDataDir.GetPath();
	expectedTitle.Append(_(" -> testdata (AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth3Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDirsRegex = ^"));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(wxT("/, AlwaysIncludeFilesRegex = ^"));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(_("/)"));

	CPPUNIT_ASSERT_EQUAL(expectedTitle, 
		mpLocationsListBox  ->GetString(0));
	CPPUNIT_ASSERT_EQUAL(expectedTitle,
		mpExcludeLocsListBox->GetString(0));

	CPPUNIT_ASSERT_EQUAL(3, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
}

void TestBackupConfig::TestAddExcludeInIncludedNode()
{
	// activate node at depth 4 again, check that an
	// Exclude entry is added this time.

	CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox  ->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(3, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());

	ActivateTreeItemWaitEvent(mpTree, mDepth4);

	wxString expectedTitle = mTestDataDir.GetPath();
	expectedTitle.Append(_(" -> testdata (AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth3Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDirsRegex = ^"));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(wxT("/, AlwaysIncludeFilesRegex = ^"));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(wxT("/, ExcludeDir = "));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(_(")"));
	
	CPPUNIT_ASSERT_EQUAL(expectedTitle, 
		mpLocationsListBox  ->GetString(0));
	CPPUNIT_ASSERT_EQUAL(expectedTitle,
		mpExcludeLocsListBox->GetString(0));

	CPPUNIT_ASSERT_EQUAL(4, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
}

void TestBackupConfig::TestAddHigherExcludeInTree()
{
	// activate node at depth 2 again, check that an
	// Exclude entry is added again, and that the node
	// at depth 4 still shows as excluded
	ActivateTreeItemWaitEvent(mpTree, mDepth2);
	CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox  ->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());

	wxString expectedTitle = mTestDataDir.GetPath();
	expectedTitle.Append(_(" -> testdata (AlwaysIncludeDir = "));
	expectedTitle.Append(mTestDepth3Dir.GetPath());
	expectedTitle.Append(wxT(", AlwaysIncludeDirsRegex = ^"));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(wxT("/, AlwaysIncludeFilesRegex = ^"));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(wxT("/, ExcludeDir = "));
	expectedTitle.Append(mTestDepth4Dir.GetPath());
	expectedTitle.Append(_(", ExcludeDir = "));
	expectedTitle.Append(mTestDepth2Dir.GetPath());
	expectedTitle.Append(_(")"));
	CPPUNIT_ASSERT_EQUAL(expectedTitle, 
		mpLocationsListBox  ->GetString(0));
	CPPUNIT_ASSERT_EQUAL(expectedTitle,
		mpExcludeLocsListBox->GetString(0));

	CPPUNIT_ASSERT_EQUAL(5, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());

	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedImageId(), 
		mpTree->GetItemImage(mTestDataDirItem));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedGreyImageId(), 
		mpTree->GetItemImage(mDepth1));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedImageId(), 
		mpTree->GetItemImage(mDepth2));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDepth3));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDepth4));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDepth5));
	CPPUNIT_ASSERT_EQUAL(mapImages->GetCrossedGreyImageId(), 
		mpTree->GetItemImage(mDepth6));
}

void TestBackupConfig::TestRemoveEntriesFromTree()
{
	// remove Exclude and Location entries, check that
	// all is reset.
	ActivateTreeItemWaitEvent(mpTree, mDepth4);
	ActivateTreeItemWaitEvent(mpTree, mDepth3);
	ActivateTreeItemWaitEvent(mpTree, mDepth2);
	
	MessageBoxSetResponse(BM_BACKUP_FILES_DELETE_LOCATION_QUESTION, wxYES);
	ActivateTreeItemWaitEvent(mpTree, mTestDataDirItem);
	MessageBoxCheckFired();
	
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox  ->GetCount());
	CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, 
		mpLocationsListBox  ->GetSelection());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, 
		mpExcludeLocsListBox->GetSelection());
}

void TestBackupConfig::TestAddTwoLocations()
{
	// add two locations using the Locations panel,
	// check that the one just added is selected,
	// and text controls are populated correctly.
	SetTextCtrlValue(mpLocationNameCtrl, _("tmp"));
	SetTextCtrlValue(mpLocationPathCtrl, _("/tmp"));
	ClickButtonWaitEvent(mpLocationAddButton);
	CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("tmp"),  
		mpLocationNameCtrl->GetValue());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp"), 
		mpLocationPathCtrl->GetValue());
	
	SetTextCtrlValue(mpLocationNameCtrl, _("etc"));
	SetTextCtrlValue(mpLocationPathCtrl, _("/etc"));
	ClickButtonWaitEvent(mpLocationAddButton);
	CPPUNIT_ASSERT_EQUAL(2, mpLocationsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("etc"),  
		mpLocationNameCtrl->GetValue());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/etc"), 
		mpLocationPathCtrl->GetValue());
}

void TestBackupConfig::TestAddExcludeEntry()
{
	// add an exclude entry using the Exclusions panel,
	// check that the one just added is selected,
	// and text controls are populated correctly.
	
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeTypeList->GetSelection());

	SetTextCtrlValue(mpExcludePathCtrl, _("/tmp/foo"));
	ClickButtonWaitEvent(mpExcludeAddButton);
	
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/foo"), 
		mpExcludePathCtrl->GetValue());
}

void TestBackupConfig::TestAddAnotherExcludeEntry()
{
	// add another exclude entry using the Exclusions panel,
	// check that the one just added is selected,
	// and text controls are populated correctly.
	
	SetSelection(mpExcludeTypeList, 1);
	SetTextCtrlValue(mpExcludePathCtrl, _("/tmp/bar"));
	ClickButtonWaitEvent(mpExcludeAddButton);

	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/bar"), 
		mpExcludePathCtrl->GetValue());
}

void TestBackupConfig::TestSelectExcludeEntry()
{
	// change selected exclude entry, check that
	// controls are populated correctly
	SetSelection(mpExcludeListBox, 0);
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/foo"), 
		mpExcludePathCtrl->GetValue());
	
	SetSelection(mpExcludeListBox, 1);
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/bar"), 
		mpExcludePathCtrl->GetValue());
}

void TestBackupConfig::TestEditExcludeEntries()
{
	// edit exclude entries, check that
	// controls are populated correctly
	SetSelection(mpExcludeListBox, 0);
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/foo"), 
		mpExcludePathCtrl->GetValue());
	SetSelection(mpExcludeTypeList, 2);
	SetTextCtrlValue(mpExcludePathCtrl, _("/tmp/baz"));
	ClickButtonWaitEvent(mpExcludeEditButton);
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());
	
	SetSelection(mpExcludeListBox, 1);
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/bar"), 
		mpExcludePathCtrl->GetValue());
	SetSelection(mpExcludeTypeList, 3);
	SetTextCtrlValue(mpExcludePathCtrl, _("/tmp/whee"));
	ClickButtonWaitEvent(mpExcludeEditButton);
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(3, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/whee"), 
		mpExcludePathCtrl->GetValue());

	SetSelection(mpExcludeListBox, 0);
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());

	SetSelection(mpExcludeListBox, 1);
	CPPUNIT_ASSERT_EQUAL(3, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/whee"), 
		mpExcludePathCtrl->GetValue());
}

void TestBackupConfig::TestAddExcludeBasedOnExisting()
{
	// check that adding a new entry based on an
	// existing entry works correctly
	
	SetSelection(mpExcludeListBox, 0);
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());
	
	// change path and add new entry
	SetTextCtrlValue(mpExcludePathCtrl, _("/tmp/foo"));
	ClickButtonWaitEvent(mpExcludeAddButton);
	CPPUNIT_ASSERT_EQUAL(3, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/foo"), 
		mpExcludePathCtrl->GetValue());
	
	// original entry unchanged
	SetSelection(mpExcludeListBox, 0);
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());
		
	// change type and add new entry
	SetSelection(mpExcludeTypeList, 1);
	ClickButtonWaitEvent(mpExcludeAddButton);
	CPPUNIT_ASSERT_EQUAL(4, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(3, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());

	// original entry unchanged
	SetSelection(mpExcludeListBox, 0);
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());
}

void TestBackupConfig::TestAddExcludeEntryToSecondLoc()
{
	// add an exclude entry to the second location,
	// check that the exclude entries for the two locations
	// are not conflated
	
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(4, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());

	SetSelection(mpExcludeLocsListBox, 1);
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, mpExcludeListBox->GetSelection());
	
	SetSelection(mpExcludeTypeList, 4);
	SetTextCtrlValue(mpExcludePathCtrl, _("fubar"));
	ClickButtonWaitEvent(mpExcludeAddButton);
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
	
	SetSelection(mpExcludeLocsListBox, 0);
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(4, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());
	
	SetSelection(mpExcludeLocsListBox, 1);
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(4, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("fubar"), 
		mpExcludePathCtrl->GetValue());

	ClickButtonWaitEvent(mpExcludeDelButton);
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, mpExcludeListBox->GetSelection());

	SetSelection(mpExcludeLocsListBox, 0);
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(4, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());
}

void TestBackupConfig::TestRemoveExcludeEntries()
{
	// check that removing entries works correctly
	SetSelection(mpExcludeListBox, 3);
	CPPUNIT_ASSERT_EQUAL(4, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(3, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());
	ClickButtonWaitEvent(mpExcludeDelButton);
	CPPUNIT_ASSERT_EQUAL(3, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());
	
	SetSelection(mpExcludeListBox, 2);
	CPPUNIT_ASSERT_EQUAL(3, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/foo"), 
		mpExcludePathCtrl->GetValue());
	ClickButtonWaitEvent(mpExcludeDelButton);
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());

	SetSelection(mpExcludeListBox, 1);
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(3, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/whee"), 
		mpExcludePathCtrl->GetValue());
	ClickButtonWaitEvent(mpExcludeDelButton);
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());

	SetSelection(mpExcludeListBox, 0);
	CPPUNIT_ASSERT_EQUAL(1, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(2, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp/baz"), 
		mpExcludePathCtrl->GetValue());
	ClickButtonWaitEvent(mpExcludeDelButton);
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, mpExcludeListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL(0, mpExcludeTypeList->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)wxEmptyString, 
		mpExcludePathCtrl->GetValue());
}

void TestBackupConfig::TestAddLocationOnLocationsPanel()
{
	// check that adding a new location using the
	// Locations panel works correctly
	
	CPPUNIT_ASSERT_EQUAL(2, mpLocationsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox->GetSelection());
	SetSelection(mpLocationsListBox, 0);
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp"),
		mpLocationPathCtrl->GetValue());
	CPPUNIT_ASSERT_EQUAL((wxString)_("tmp"),
		mpLocationNameCtrl->GetValue());
	
	SetTextCtrlValue(mpLocationNameCtrl, _("foobar"));
	SetTextCtrlValue(mpLocationPathCtrl, _("whee"));
	ClickButtonWaitEvent(mpLocationAddButton);

	CPPUNIT_ASSERT_EQUAL(3, mpLocationsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(2, mpLocationsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("whee"),
		mpLocationPathCtrl->GetValue());
	CPPUNIT_ASSERT_EQUAL((wxString)_("foobar"),
		mpLocationNameCtrl->GetValue());			
}

void TestBackupConfig::TestSwitchLocations()
{
	// check that switching locations works correctly,
	// and changes the selected location in the
	// Excludes panel (doesn't work yet)
	SetSelection(mpLocationsListBox, 0);
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp"),
		mpLocationPathCtrl->GetValue());
	CPPUNIT_ASSERT_EQUAL((wxString)_("tmp"),
		mpLocationNameCtrl->GetValue());
	// CPPUNIT_ASSERT_EQUAL(0, mpExcludeLocsListBox->GetSelection());
	
	SetSelection(mpLocationsListBox, 1);
	CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/etc"),
		mpLocationPathCtrl->GetValue());
	CPPUNIT_ASSERT_EQUAL((wxString)_("etc"),
		mpLocationNameCtrl->GetValue());
	// CPPUNIT_ASSERT_EQUAL(1, mpExcludeLocsListBox->GetSelection());

	SetSelection(mpLocationsListBox, 2);
	CPPUNIT_ASSERT_EQUAL(2, mpLocationsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("whee"),
		mpLocationPathCtrl->GetValue());
	CPPUNIT_ASSERT_EQUAL((wxString)_("foobar"),
		mpLocationNameCtrl->GetValue());
	// CPPUNIT_ASSERT_EQUAL(2, mpExcludeLocsListBox->GetSelection());
}

void TestBackupConfig::TestRemoveLocation()
{
	// check that removing locations works correctly
	SetSelection(mpLocationsListBox, 0);
	CPPUNIT_ASSERT_EQUAL((wxString)_("/tmp"),
		mpLocationPathCtrl->GetValue());
	CPPUNIT_ASSERT_EQUAL((wxString)_("tmp"),
		mpLocationNameCtrl->GetValue());

	ClickButtonWaitEvent(mpLocationDelButton);
	CPPUNIT_ASSERT_EQUAL(2, mpLocationsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/etc"),
		mpLocationPathCtrl->GetValue());
	CPPUNIT_ASSERT_EQUAL((wxString)_("etc"),
		mpLocationNameCtrl->GetValue());

	SetSelection(mpLocationsListBox, 1);
	CPPUNIT_ASSERT_EQUAL((wxString)_("whee"),
		mpLocationPathCtrl->GetValue());
	CPPUNIT_ASSERT_EQUAL((wxString)_("foobar"),
		mpLocationNameCtrl->GetValue());

	ClickButtonWaitEvent(mpLocationDelButton);
	CPPUNIT_ASSERT_EQUAL(1, mpLocationsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)_("/etc"),
		mpLocationPathCtrl->GetValue());
	CPPUNIT_ASSERT_EQUAL((wxString)_("etc"),
		mpLocationNameCtrl->GetValue());

	ClickButtonWaitEvent(mpLocationDelButton);
	CPPUNIT_ASSERT_EQUAL(0, mpLocationsListBox->GetCount());
	CPPUNIT_ASSERT_EQUAL(wxNOT_FOUND, 
		mpLocationsListBox->GetSelection());
	CPPUNIT_ASSERT_EQUAL((wxString)wxEmptyString,
		mpLocationPathCtrl->GetValue());
	CPPUNIT_ASSERT_EQUAL((wxString)wxEmptyString,
		mpLocationNameCtrl->GetValue());				
}			

void TestBackupConfig::TestComplexExcludes()
{
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
	
	wxFileName configFile(mTempDir.GetFullPath(), _("bbackupd.conf"));
	BoxiLocation mTestDirLoc(_("testdata"),
		mTestDataDir.GetPath(),
		mpConfig);	

	{
		CPPUNIT_ASSERT_EQUAL((size_t)0, mpConfig->GetLocations().size());
		
		mpConfig->AddLocation(mTestDirLoc);
		CPPUNIT_ASSERT_EQUAL((size_t)1, 
			mpConfig->GetLocations().size());
		CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedImageId(), 
			mpTree->GetItemImage(mTestDataDirItem));
		BoxiLocation* pNewLoc = mpConfig->GetLocation(mTestDirLoc);
		CPPUNIT_ASSERT(pNewLoc);
		
		BoxiExcludeList& rExcludes = pNewLoc->GetExcludeList();

		#define ADD_ENTRY(type, path) \
		rExcludes.AddEntry \
		( \
			BoxiExcludeEntry \
			( \
				theExcludeTypes[type], path \
			) \
		)
		
		ADD_ENTRY(ETI_EXCLUDE_FILES_REGEX, wxString(_(".*")));
		ADD_ENTRY(ETI_EXCLUDE_DIRS_REGEX,  wxString(_(".*")));
		
		#undef ADD_ENTRY

		CPPUNIT_ASSERT(!configFile.FileExists());
		mpConfig->Save(configFile.GetFullPath());
		CPPUNIT_ASSERT(configFile.FileExists());

		#define CHECK_ITEM(name, image) \
		CPPUNIT_ASSERT(item.IsOk()); \
		CPPUNIT_ASSERT_EQUAL((wxString)_(name), \
			mpTree->GetItemText(item)); \
		CPPUNIT_ASSERT_EQUAL(mapImages->Get ## image ## ImageId(), \
			mpTree->GetItemImage(item))
		
		wxTreeItemId item = mTestDataDirItem;
		CHECK_ITEM("testdata", Checked);

		CPPUNIT_ASSERT_EQUAL((size_t)1, mpConfig->GetLocations().size());
		mpConfig->RemoveLocation(*pNewLoc);
		CPPUNIT_ASSERT_EQUAL((size_t)0, mpConfig->GetLocations().size());
	}

	{
		CPPUNIT_ASSERT_EQUAL((size_t)0, mpConfig->GetLocations().size());
		
		mpConfig->AddLocation(mTestDirLoc);
		CPPUNIT_ASSERT_EQUAL((size_t)1, 
			mpConfig->GetLocations().size());
		CPPUNIT_ASSERT_EQUAL(mapImages->GetCheckedImageId(), 
			mpTree->GetItemImage(mTestDataDirItem));
		BoxiLocation* pNewLoc = mpConfig->GetLocation(mTestDirLoc);
		CPPUNIT_ASSERT(pNewLoc);
		
		BoxiExcludeList& rExcludes = pNewLoc->GetExcludeList();

		#define CREATE_FILE(dir, name) \
		wxFileName dir ## _ ## name(dir.GetFullPath(), _(#name)); \
		{ wxFile f; CPPUNIT_ASSERT(f.Create(dir ## _ ## name.GetFullPath())); }
		
		#define CREATE_DIR(dir, name) \
		wxFileName dir ## _ ## name(dir.GetFullPath(), _("")); \
		dir ## _ ## name.AppendDir(_(#name)); \
		CPPUNIT_ASSERT(dir ## _ ## name.Mkdir(0700));
		
		CREATE_DIR( mTestDataDir, exclude_dir);
		CREATE_DIR( mTestDataDir, exclude_dir_2);
		CREATE_DIR( mTestDataDir, sub23);
		CREATE_DIR( mTestDataDir_sub23, xx_not_this_dir_22);
		CREATE_FILE(mTestDataDir_sub23, somefile_excludethis);
		CREATE_FILE(mTestDataDir, xx_not_this_dir_22);
		CREATE_FILE(mTestDataDir, excluded_1);
		CREATE_FILE(mTestDataDir, excluded_2);
		CREATE_FILE(mTestDataDir, zEXCLUDEu);
		CREATE_FILE(mTestDataDir, dont_excludethis);
		CREATE_DIR( mTestDataDir, xx_not_this_dir_ALWAYSINCLUDE);
		
		#undef CREATE_DIR
		#undef CREATE_FILE

		#define ADD_ENTRY(type, path) \
		rExcludes.AddEntry \
		( \
			BoxiExcludeEntry \
			( \
				theExcludeTypes[type], path \
			) \
		)
		#define ADD_ENTRY_DIR(type, pathvar) \
		ADD_ENTRY(type, pathvar.GetPath());

		#define ADD_ENTRY_FILE(type, pathvar) \
		ADD_ENTRY(type, pathvar.GetFullPath());

		ADD_ENTRY_FILE(ETI_EXCLUDE_FILE, mTestDataDir_excluded_1);
		ADD_ENTRY_FILE(ETI_EXCLUDE_FILE, mTestDataDir_excluded_2);
		ADD_ENTRY(ETI_EXCLUDE_FILES_REGEX, wxString(_("_excludethis$")));
		ADD_ENTRY(ETI_EXCLUDE_FILES_REGEX, wxString(_("EXCLUDE")));
		ADD_ENTRY_FILE(ETI_ALWAYS_INCLUDE_FILE, mTestDataDir_dont_excludethis);
		ADD_ENTRY_DIR(ETI_EXCLUDE_DIR, mTestDataDir_exclude_dir);
		ADD_ENTRY_DIR(ETI_EXCLUDE_DIR, mTestDataDir_exclude_dir_2);
		ADD_ENTRY(ETI_EXCLUDE_DIRS_REGEX, wxString(_("not_this_dir")));
		ADD_ENTRY(ETI_ALWAYS_INCLUDE_DIRS_REGEX, wxString(_("ALWAYSINCLUDE")));
		
		#undef ADD_ENTRY_FILE
		#undef ADD_ENTRY_DIR
		#undef ADD_ENTRY

		mpConfig->Save(configFile.GetFullPath());
		CPPUNIT_ASSERT(configFile.FileExists());

		mpTree->Collapse(mTestDataDirItem);
		mpTree->Expand(mTestDataDirItem);

		wxTreeItemIdValue cookie1;
		wxTreeItemId mDir1 = mTestDataDirItem;
		wxTreeItemId item;
		
		#define CHECK_ITEM(name, image) \
		CPPUNIT_ASSERT(item.IsOk()); \
		CPPUNIT_ASSERT_EQUAL((wxString)_(name), \
			mpTree->GetItemText(item)); \
		CPPUNIT_ASSERT_EQUAL(mapImages->Get ## image ## ImageId(), \
			mpTree->GetItemImage(item))
		
		item = mpTree->GetFirstChild(mDir1, cookie1);
		CHECK_ITEM("another", CheckedGrey);

		item = mpTree->GetNextChild(mDir1, cookie1);
		CHECK_ITEM("exclude_dir", Crossed);

		item = mpTree->GetNextChild(mDir1, cookie1);
		CHECK_ITEM("exclude_dir_2", Crossed);

		item = mpTree->GetNextChild(mDir1, cookie1);
		CHECK_ITEM("sub23", CheckedGrey);

		// under sub23:
		{
			wxTreeItemIdValue cookie2;
			wxTreeItemId mDir2 = item;
			mpTree->Expand(mDir2);

			item = mpTree->GetFirstChild(mDir2, cookie2);
			CHECK_ITEM("xx_not_this_dir_22", Crossed);

			item = mpTree->GetNextChild(mDir2, cookie2);
			CHECK_ITEM("somefile_excludethis", Crossed);
			
			item = mpTree->GetNextChild(mDir2, cookie2);
			CPPUNIT_ASSERT(!item.IsOk());
		}

		item = mpTree->GetNextChild(mDir1, cookie1);
		CHECK_ITEM("xx_not_this_dir_ALWAYSINCLUDE", Always);

		item = mpTree->GetNextChild(mDir1, cookie1);
		CHECK_ITEM("dont_excludethis", Always);

		item = mpTree->GetNextChild(mDir1, cookie1);
		CHECK_ITEM("excluded_1", Crossed);

		item = mpTree->GetNextChild(mDir1, cookie1);
		CHECK_ITEM("excluded_2", Crossed);

		// xx_not_this_dir_22 should not be excluded by
		// ExcludeDirsRegex, because it's a file
		item = mpTree->GetNextChild(mDir1, cookie1);
		CHECK_ITEM("xx_not_this_dir_22", CheckedGrey);

		item = mpTree->GetNextChild(mDir1, cookie1);
		CHECK_ITEM("zEXCLUDEu", Crossed);

		item = mpTree->GetNextChild(mDir1, cookie1);
		CPPUNIT_ASSERT(!item.IsOk());
		
		#undef CHECK_ITEM
		
		#define DELETE_FILE(dir, name) \
		CPPUNIT_ASSERT(wxRemoveFile(dir ## _ ## name.GetFullPath()))
		
		#define DELETE_DIR(dir, name) \
		CPPUNIT_ASSERT(dir ## _ ## name.Rmdir())

		DELETE_DIR( mTestDataDir_sub23, xx_not_this_dir_22);
		DELETE_FILE(mTestDataDir_sub23, somefile_excludethis);
		DELETE_FILE(mTestDataDir, xx_not_this_dir_22);
		DELETE_FILE(mTestDataDir, excluded_1);
		DELETE_FILE(mTestDataDir, excluded_2);
		DELETE_FILE(mTestDataDir, zEXCLUDEu);
		DELETE_FILE(mTestDataDir, dont_excludethis);
		DELETE_DIR( mTestDataDir, xx_not_this_dir_ALWAYSINCLUDE);
		DELETE_DIR( mTestDataDir, sub23);
		DELETE_DIR( mTestDataDir, exclude_dir);
		DELETE_DIR( mTestDataDir, exclude_dir_2);
		
		#undef DELETE_DIR
		#undef DELETE_FILE
		
		CPPUNIT_ASSERT_EQUAL((size_t)1, mpConfig->GetLocations().size());
		mpConfig->RemoveLocation(*pNewLoc);
		CPPUNIT_ASSERT_EQUAL((size_t)0, mpConfig->GetLocations().size());
	}

	CPPUNIT_ASSERT(wxRemoveFile(configFile.GetFullPath()));		
	CPPUNIT_ASSERT(!configFile.FileExists());
}
