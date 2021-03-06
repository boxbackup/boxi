/***************************************************************************
 *            TestFrame.h
 *
 *  Sun Jan 22 22:35:37 2006
 *  Copyright 2006-2008 Chris Wilson
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

#include <wx/stackwalk.h>

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

class GuiStarter : public TestSetUpDecorator
{
	public:
	GuiStarter(CppUnit::Test *pTest) 
	: TestSetUpDecorator (pTest), mpResult(NULL) { }
	
	virtual void run(CppUnit::TestResult *pResult);
	
	virtual void RunAsDecorator();
	
	protected:
	virtual void setUp();
	virtual void tearDown() { }

	private:
	// Store result sent to run() for latter call from WxGuiTestApp::OnRun():
	CppUnit::TestResult *mpResult;
};

#define CPPUNIT_ASSERT_AT(condition, line) \
	CppUnit::Asserter::failIf(!(condition), \
		CppUnit::Message("assertion failed", "Expression: " #condition), \
		line);

#define CPPUNIT_ASSERT_MESSAGE_AT(message, condition, line) \
	CppUnit::Asserter::failIf(!condition, \
		CppUnit::Message(message, "Expression: " #condition), \
		(line));

#define CPPUNIT_ASSERT_MESSAGE_AT_FILE_LINE(message, condition, file, line) \
	CppUnit::Asserter::failIf(!condition, \
		CppUnit::Message(message, "Expression: " #condition), \
		CppUnit::SourceLine(file, line));

#define CPPUNIT_ASSERT_EQUAL_AT(expected, actual, line)          \
  ( CPPUNIT_NS::assertEquals( (expected),              \
                              (actual),                \
                              (line),    \
                              "" ) )

#define CPPUNIT_ASSERT_MESSAGE_WX(message, condition) \
	CPPUNIT_ASSERT_MESSAGE(wxCharBuffer(message.mb_str()).data(), condition)

#if wxUSE_STACKWALKER
#define BOXI_ASSERT(condition) \
{ \
	bool _value = condition; \
	wxString msgOut = _("Assertion failed: " #condition "\n"); \
	if (!_value) msgOut.Append(StackWalker::GetStackTrace()); \
	CPPUNIT_ASSERT_MESSAGE_WX(msgOut, _value); \
}

#define BOXI_ASSERT_EQUAL(expected, actual) \
{ \
	wxString msgOut = _("Assertion failed: " #expected " == " \
		#actual "\n"); \
	msgOut.Append(StackWalker::GetStackTrace()); \
	CPPUNIT_ASSERT_EQUAL_MESSAGE(wxCharBuffer(msgOut.mb_str()).data(), \
		expected, actual); \
}
#else // !wxUSE_STACKWALKER
#define BOXI_ASSERT(condition) \
{ \
	wxString msgOut = _("Assertion failed: " #condition "\n"); \
	CPPUNIT_ASSERT_MESSAGE_WX(msgOut, condition); \
}

#define BOXI_ASSERT_EQUAL(expected, actual) \
{ \
	wxString msgOut = _("Assertion failed: " #expected " == " \
		#actual "\n"); \
	CPPUNIT_ASSERT_EQUAL_MESSAGE(wxCharBuffer(msgOut.mb_str()).data(), \
		expected, actual); \
}
#endif // !wxUSE_STACKWALKER

/*
#define CPPUNIT_ASSERT_EQUAL_MESSAGE(expected, actual, message) \
  ( CPPUNIT_NS::assertEquals( (expected), \
                              (actual), \
                              CppUnit::SourceLine(), \
                              (message) ) )
*/

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
			wxCharBuffer buf = x.mb_str(wxConvBoxi);
			std::string ret = buf.data();
			return ret;
			// std::string text = '"' + x.mb_str(wxConvBoxi).data() + '"';    
			// return text;
			// adds quote around the string to see whitespace
			// std::ostringstream ost;
			// ost << text;
			// return ost.str();
		}
	};

	// specialisation for assertions of equality on signed and
	// unsigned integers (e.g. integer constants vs. GetCount())
	inline void assertEquals(int expected,
		unsigned int actual,
		SourceLine sourceLine,
		const std::string &message)
	{
		assertEquals((unsigned int)expected, actual,
			sourceLine, message);
	}
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
	virtual void runTest () { RunTest(); }
		
	protected:
	void ClickButtonWaitIdle  (wxWindow* pWindow);
	void ClickButtonWaitEvent (wxWindowID ParentID, wxWindowID ButtonID);
	void ClickButtonWaitEvent (wxWindowID ButtonID);
	void ClickButtonWaitEvent (wxButton* pButton);
	void ClickRadioWaitEvent  (wxWindow* pButton);
	void ClickRadioWaitEvent  (wxWindowID ButtonID);
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
	void SetValueAndDefocus(wxTextCtrl* pTextCtrl, const wxString& rValue);
	void SetValueDefocusCheck(wxTextCtrl* pTextCtrl, const wxString& rValue);
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
	std::auto_ptr<wxLogAsserter> mapAsserter;
	
	public:
	void setUp();
	void tearDown();
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

#if wxUSE_STACKWALKER
class StackWalker : public wxStackWalker
{
	private:
	wxString m_StackTrace;

	protected:
	void OnStackFrame(const wxStackFrame& frame);

	public:
	static wxString GetStackTrace();
	static wxString AppendTo(const wxString& rMessage);
};
#endif

#endif /* _TESTFRAME_H */
