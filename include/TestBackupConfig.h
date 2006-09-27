/***************************************************************************
 *            TestBackupConfig.h
 *
 *  Sat Aug 12 14:01:01 2006
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
 
#ifndef _TESTBACKUPCONFIG_H
#define _TESTBACKUPCONFIG_H

#include <wx/filename.h>

#include "TestFrame.h"

class TestBackupConfig : public TestWithConfig
{
	public:
	TestBackupConfig() { }
	virtual void RunTest();
	static CppUnit::Test *suite();
};

wxFileName MakeAbsolutePath(wxFileName base, wxString relativePath);
wxFileName MakeAbsolutePath(wxString baseDir, wxString relativePath);
wxFileName MakeAbsolutePath(wxString relativePath);
void DeleteRecursive(const wxFileName& rPath);

#endif /* _TESTBACKUPCONFIG_H */
