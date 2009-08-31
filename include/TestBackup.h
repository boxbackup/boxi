/***************************************************************************
 *            TestBackup.h
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
 
#ifndef _TESTBACKUP_H
#define _TESTBACKUP_H

#include <wx/filename.h>

class TLSContext;
class Configuration;
class BackupStoreDirectory;

#include "BackupQueries.h"
#include "TestWithServer.h"

class TestBackup : public TestWithServer
{
	public:
	void RunTest();
	static CppUnit::Test *suite();
	
	private:
	void TestConfigChecks();
	void TestRenameTrackedOverDeleted();
	void TestVeryOldFiles();
	void TestModifyExisting();
	void TestExclusions();
	void TestReadErrors();
	void TestMinimumFileAge();
	void TestBackupOfAttributes();
	void TestAddMoreFiles();
	void TestRenameDir();
	void TestRestore();
	void CleanUp();
};

void Unzip(const wxFileName& rZipFile, const wxFileName& rDestDir,
	bool restoreTimes = false);

class wxZipEntry;
std::auto_ptr<wxZipEntry> FindZipEntry(const wxFileName& rZipFile, 
	const wxString& rFileName);

void CompareBackup(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, BackupQueries::CompareParams& rParams,
	const wxString& rRemoteDir, const wxFileName& rLocalPath);

void CompareExpectNoDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const wxString& rRemoteDir, 
	const wxFileName& rLocalPath);

void CompareExpectDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const wxString& rRemoteDir, 
	const wxFileName& rLocalPath, int numDiffs, int numDiffsModTime);

void CompareLocation(const Configuration& rConfig, 
	TLSContext& rTlsContext,
	const std::string& rLocationName, 
	BackupQueries::CompareParams& rParams);

void CompareLocationExpectNoDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const std::string& rLocationName,
	int excludedDirs, int excludedFiles);

void CompareLocationExpectDifferences(const Configuration& rClientConfig, 
	TLSContext& rTlsContext, const std::string& rLocationName, 
	int numDiffs, int numDiffsModTime, int numUnchecked, int excludedDirs,
	int excludedFiles);

int64_t SearchDir(BackupStoreDirectory& rDir, const std::string& rChildName);

#endif /* _TESTBACKUP_H */
