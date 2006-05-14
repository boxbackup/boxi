/***************************************************************************
 *            BoxiApp.h
 *
 *  Wed May 10 20:50:08 2006
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
 
#ifndef _BOXIAPP_H
#define _BOXIAPP_H

#include <wx/app.h>
#include <wx/filedlg.h>

#include <cppunit/SourceLine.h>

#include "main.h"

class TestSetUpDecorator;

class BoxiApp : public wxApp
{
  	public:
	// constructor
	BoxiApp() 
	: wxApp(), 
	  mpTestRunner(NULL), 
	  mExpectingFileDialog(false),
	  mTesting(false)
	{ }
	virtual ~BoxiApp() { }
	
	// called by wx, not strictly event handlers as they are
	// not installed in the event handler table.
	virtual bool OnInit();
	virtual int  OnRun ();
	
	// event handlers
	void OnIdle(wxIdleEvent& rEvent);
	
	// cppunit testing interface
	void SetTestRunner(TestSetUpDecorator* pTestRunner)
	{
		mpTestRunner = pTestRunner;
	}
	bool IsTestMode() { return mTesting; }
	
	int  ShowFileDialog  (wxFileDialog& rDialog);
	void ExpectFileDialog(const wxString& rPathToReturn);
	bool ShowedFileDialog() { return !mExpectingFileDialog; }
	
	int ShowMessageBox(message_t messageId, const wxString& message,
		const wxString& caption, long style, wxWindow *parent);
	void ExpectMessageBox(message_t messageId, int response, 
		const CppUnit::SourceLine& rLine);
	bool ShowedMessageBox() { return mExpectedMessageId == BM_UNKNOWN; }
	void ResetMessageBox()
	{
		mExpectedMessageId = BM_UNKNOWN;
		mExpectedMessageResponse = -1;
	}
	
	bool     HaveConfigFile() { return mHaveConfigFile; }
	wxString GetConfigFile () { return mConfigFileName; }
	
	private:
	TestSetUpDecorator* mpTestRunner;
	wxString  mExpectedFileDialogResult;
	bool      mExpectingFileDialog;
	bool      mTesting;
	message_t mExpectedMessageId;
	int       mExpectedMessageResponse;
	bool      mHaveConfigFile;
	wxString  mConfigFileName;
	
	DECLARE_EVENT_TABLE()
};

BoxiApp& wxGetApp();

#endif // _BOXIAPP_H
