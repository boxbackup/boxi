/***************************************************************************
 *            TestBackupConfig.h
 *
 *  Sat Aug 12 14:01:01 2006
 *  Copyright 2006-2007 Chris Wilson
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
 
#ifndef _TESTBACKUPCONFIG_H
#define _TESTBACKUPCONFIG_H

#include <wx/filename.h>

#include "TestFrame.h"
#include "FileTree.h"

class TestBackupConfig : public TestWithConfig
{
	public:
	TestBackupConfig() { }
	virtual void RunTest();
	static CppUnit::Test *suite();
	
	private:
	wxFileName  mTempDir;
	wxListBox*  mpLocationsListBox;
	wxTextCtrl* mpLocationNameCtrl;
	wxTextCtrl* mpLocationPathCtrl;
	wxButton*   mpLocationAddButton;
	wxButton*   mpLocationEditButton;
	wxButton*   mpLocationDelButton;
	wxChoice*   mpExcludeLocsListBox;
	wxChoice*   mpExcludeTypeList;
	wxListBox*  mpExcludeListBox;
	wxButton*   mpExcludeAddButton;
	wxButton*   mpExcludeEditButton;
	wxButton*   mpExcludeDelButton;
	wxTextCtrl* mpExcludePathCtrl;
	wxTreeCtrl* mpTree;
	FileImageList mImages;
	wxFileName mTestDataDir;
	wxTreeItemId mTestDataDirItem;
	wxFileName mTestDepth1Dir;
	wxFileName mTestAnotherDir;
	wxFileName mTestFile1;
	wxFileName mTestDir1;
	wxFileName mTestDepth2Dir;
	wxFileName mTestFile2;
	wxFileName mTestDir2;
	wxFileName mTestDepth3Dir;
	wxFileName mTestDepth4Dir;
	wxFileName mTestDepth5Dir;
	wxFileName mTestFile3;
	wxFileName mTestDir3;
	wxFileName mTestDepth6Dir;
	wxFileName mTestFile4;
	wxFileName mTestDir4;
	wxFileName mTestFile5;
	wxFileName mTestDir5;
	wxFileName mTestFile6;
	wxFileName mTestDir6;
	wxTreeItemId mDepth1, mDepth2, mDir1, mFile1, mDepth3, mDir2, mFile2,
		mDepth4, mDepth5, mDepth6, mDir3, mFile3, mDir4, mFile4, 
		mDir5, mFile5, mDir6, mFile6, mAnother;

	void TestAddAndRemoveSimpleLocationInTree();
	void TestAddLocationInTree();
	void TestSimpleExcludeInTree();
	void TestDeepIncludePattern();
	void TestAlwaysIncludeFileDeepInTree();
	void TestAddAlwaysIncludeInTree();
	void TestExcludeUnderAlwaysIncludePromptsToRemove();
	void TestRemoveMultipleAlwaysIncludesInTree();
	void TestAlwaysIncludeDirWithSubdirsDeepInTree();
	void TestRemoveExcludeInTree();
	void TestRemoveAlwaysIncludeInTree();
	void TestAddExcludeInIncludedNode();
	void TestAddHigherExcludeInTree();
	void TestRemoveEntriesFromTree();
	void TestAddTwoLocations();
	void TestAddExcludeEntry();
	void TestAddAnotherExcludeEntry();
	void TestSelectExcludeEntry();
	void TestEditExcludeEntries();
	void TestAddExcludeBasedOnExisting();
	void TestAddExcludeEntryToSecondLoc();
	void TestRemoveExcludeEntries();
	void TestAddLocationOnLocationsPanel();
	void TestSwitchLocations();
	void TestRemoveLocation();
	void TestComplexExcludes();
};

wxFileName MakeAbsolutePath(wxFileName base, wxString relativePath);
wxFileName MakeAbsolutePath(wxString baseDir, wxString relativePath);
wxFileName MakeAbsolutePath(wxString relativePath);
void DeleteRecursive(const wxFileName& rPath);

#endif /* _TESTBACKUPCONFIG_H */
