/***************************************************************************
 *            TestFrame.h
 *
 *  Sun Jan 22 22:35:37 2006
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
 
#ifndef _TESTFRAME_H
#define _TESTFRAME_H

#include <vector>

// #include <wx/wx.h>

#include <cppunit/Asserter.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCase.h>
#include <cppunit/extensions/TestSetUp.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include "main.h"
#include "BoxiApp.h"
#include "ClientConfig.h"
#include "WxGuiTestHelper.h"
#include "swWxGuiTestEventSimulationHelper.h"

class MainFrame;
class TestCase;
class wxDatePickerCtrl;
class wxDateTime;

class TestFrame : public wxFrame
{
	public:
	TestFrame(const wxString PathToExecutable);
	void WaitForIdle();
	void WaitForEvent(wxCommandEvent& rEvent);
	void TestThreadFinished();
	wxMutex&   GetEventMutex()  { return mMutex; }
	MainFrame* GetMainFrame()   { return mpMainFrame; }
	static bool IsTestsInProgress() 
	{ 
		return mTestsInProgress; 
	}
	
	private:
	wxString       mPathToExecutable;
	wxMutex        mMutex;
	static bool    mTestsInProgress;
	bool           mTestThreadIsWaiting;
	wxCondition    mMainThreadIsIdle;
	wxCondition    mEventHandlerFinished;
	MainFrame*     mpMainFrame;
	
	typedef std::vector<TestCase*> tTestList;
	tTestList      mTests;
	tTestList::iterator mipCurrentTest;
	
	void OnIdle(wxIdleEvent& rEvent);
	void OnCreateWindowCommand(wxCommandEvent& rEvent);
	void OnTestFinishedEvent  (wxCommandEvent& rEvent);
	void MainDoClickButton    (wxCommandEvent& rEvent);
	void MainDoClickRadio     (wxCommandEvent& rEvent);
	
	DECLARE_EVENT_TABLE()
};

class TestSetUpDecorator : public CppUnit::TestSetUp
{
	public:
	TestSetUpDecorator(CppUnit::Test *pTest) : TestSetUp(pTest) { }
	virtual void RunAsDecorator() = 0;
	virtual ~TestSetUpDecorator() { }
};

#define CPPUNIT_ASSERT_AT(condition, line) \
	CppUnit::Asserter::failIf(!(condition), \
		CppUnit::Message("assertion failed", "Expression: " #condition), \
		line);

#define CPPUNIT_ASSERT_MESSAGE_AT(message, condition, line) \
	CppUnit::Asserter::failIf(!condition, \
		CppUnit::Message(message, "Expression: " #condition), \
		(line));

#define CPPUNIT_ASSERT_EQUAL_AT(expected, actual, line)          \
  ( CPPUNIT_NS::assertEquals( (expected),              \
                              (actual),                \
                              (line),    \
                              "" ) )

namespace CppUnit
{
	// specialization for assertions of equality on wxStrings
	template<>
	struct assertion_traits<wxString>   
	{
		static bool equal(const wxString& x, const wxString& y)
		{
			return x.IsSameAs(y);
		}
		
		static std::string toString(const wxString& x)
		{
			wxCharBuffer buf = x.mb_str(wxConvLibc);
			std::string ret = buf.data();
			return ret;
			// std::string text = '"' + x.mb_str(wxConvLibc).data() + '"';    
			// return text;
			// adds quote around the string to see whitespace
			// std::ostringstream ost;
			// ost << text;
			// return ost.str();
		}
	};
}

class wxLogAsserter : public wxLog
{
	private:
	wxLog* mpOldTarget;
	
	public:
	wxLogAsserter();
	virtual ~wxLogAsserter();
	virtual void DoLog(wxLogLevel level, const wxChar *msg, time_t timestamp);
};

#define MessageBoxSetResponse(messageId, response) \
	wxGetApp().ExpectMessageBox(messageId, response, CPPUNIT_SOURCELINE())

#define MessageBoxCheckFired() CPPUNIT_ASSERT(wxGetApp().ShowedMessageBox())

class GuiTestBase : public CppUnit::TestCase
{
	public: 
	GuiTestBase();
	virtual ~GuiTestBase() { }
	virtual void RunTest() = 0;
		
	protected:
	void ClickButtonWaitIdle  (wxWindow* pWindow);
	void ClickButtonWaitEvent (wxWindowID ParentID, wxWindowID ButtonID);
	void ClickButtonWaitEvent (wxWindowID ButtonID);
	void ClickButtonWaitEvent (wxButton* pButton);
	void ClickRadioWaitEvent  (wxWindow* pButton);
	void CloseWindow          (wxWindow* pWindow);
	void CloseWindowWaitClosed(wxWindow* pWindow);
	void ClickMenuItem(int id, wxWindow *frame)
	{
		WxGuiTestEventSimulationHelper::SelectMenuItem(id, frame);
	}
	void ActivateTreeItemWaitEvent(wxTreeCtrl* pTree, wxTreeItemId& rItem);
	void ExpandTreeItemWaitEvent  (wxTreeCtrl* pTree, wxTreeItemId& rItem);
	void CollapseTreeItemWaitEvent(wxTreeCtrl* pTree, wxTreeItemId& rItem);
	void SetTextCtrlValue(wxTextCtrl* pTextCtrl, const wxString& rValue);
	void SetSpinCtrlValue(wxSpinCtrl* pTextCtrl, int newValue);
	void SetDatePickerValue(wxDatePickerCtrl* pPicker, 
		const wxDateTime& rNewValue);
	void SetSelection(wxListBox* pListCtrl,   int value);
	void SetSelection(wxChoice*  pChoiceCtrl, int value);
	void CheckBoxWaitEvent(wxCheckBox* pCheckBox, bool newValue = true);
	
	MainFrame* GetMainFrame();
	wxWindow*  FindWindow(wxWindowID id) 
	{ 
		return wxWindow::FindWindowById(id); 
	}
	wxTextCtrl* GetTextCtrl  (wxWindow* pWindow, wxWindowID id);
	
	private:
	wxCommandEvent GetButtonClickEvent(wxWindow* pWindow);

	public:	
	void tearDown()
	{
		wxGetApp().ResetMessageBox();
		
		for (size_t i = 0; i < wxTopLevelWindows.GetCount(); i++)
		{
			wxWindow* pOpenWindow = 
				wxTopLevelWindows.Item(i)->GetData();
			CloseWindow(pOpenWindow);
		}
		
		while (wxTopLevelWindows.GetCount() > 0)
		{
			CPPUNIT_ASSERT_EQUAL(0, WxGuiTestHelper::FlushEventQueue());
		}
	}	
};

class TestWithConfig : public GuiTestBase
{
	protected:
	ClientConfig* mpConfig;
	
	public:
	TestWithConfig() : GuiTestBase(), mpConfig(NULL) { }
	virtual void tearDown()
	{
		if (mpConfig) mpConfig->Reset();
		this->GuiTestBase::tearDown();
	}
};

#endif /* _TESTFRAME_H */
