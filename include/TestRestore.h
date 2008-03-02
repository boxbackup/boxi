/***************************************************************************
 *            TestRestore.h
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
 
#ifndef _TESTRESTORE_H
#define _TESTRESTORE_H

#include "FileTree.h"
#include "TestWithServer.h"

class RestorePanel;
class RestoreProgressPanel;

class TestRestore : public TestWithServer
{
	public:
	static CppUnit::Test *suite();
	virtual void RunTest();
	
	private:
	RestorePanel* mpRestorePanel;
	RestoreProgressPanel* mpRestoreProgressPanel;
	wxListBox* mpRestoreErrorList;
	wxFileName mTest2ZipFile, mTest3ZipFile, mRestoreDest;
	wxFileName mSub23, df9834_dsfRestored, df9834_dsfOriginal;
	wxFileName testdataRestored;
	wxTreeItemId sub23id, dhsfdss, mRootId, dfsfd, bfdlink_h, a_out_h, 
		mLocationId;
	FileImageList mImages;
	wxTreeCtrl* mpRestoreTree;

	void TestRestoreWholeDir();
	void TestRestoreSpec();
	void TestDoubleIncludes();
	void TestDoubleAndConflictingIncludes();
	void TestIncludeInsideExclude();
	void TestRestoreServerRoot();
	void TestOldAndDeletedFilesNotRestored();
	void TestRestoreToDate();
	void CleanUp();
};

#endif /* _TESTRESTORE_H */
