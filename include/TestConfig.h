/***************************************************************************
 *            TestConfig.h
 *
 *  Sat Aug 12 14:35:33 2006
 *  Copyright  2006  Chris Wilson
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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#ifndef _TESTCONFIG_H
#define _TESTCONFIG_H

#include "TestFrame.h"

class TestConfig : public TestWithConfig
{
	public:
	TestConfig() { }
	virtual void RunTest();
	static CppUnit::Test *suite();

	private:
	MainFrame* mpMainFrame;
	wxFileName mConfigFileName;
	void NewConfig();
	void LoadConfig();
	void AssertDefaultConfig();
	void AssertConfigAsExpected();
	void ClearConfig();
	void AssertClean();
	void AssertDirty();
};

#endif /* _TESTCONFIG_H */
