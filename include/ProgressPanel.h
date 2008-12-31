/***************************************************************************
 *            ProgressPanel.h
 *
 *  Sun Oct  1 22:44:02 2006
 *  Copyright 2006-2008 Chris Wilson
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
 
#ifndef _PROGRESSPANEL_H
#define _PROGRESSPANEL_H

#include <wx/log.h>
#include <wx/panel.h>

class wxButton;
class wxGauge;
class wxListBox;
class wxStaticText;
class wxString;
class wxTextCtrl;

class ProgressPanel : public wxPanel
{
	public:
	ProgressPanel
	(
		wxWindow* parent, 
		wxWindowID id = -1,
		const wxString& name = wxT("Progress Panel")
	);

	class ExclusionOracle
	{
		public:
		virtual bool IsExcludedFile(const std::string& rFileName) = 0;
		virtual bool IsExcludedDir (const std::string& rDirName)  = 0;
	};
	
	protected:
	wxListBox* mpErrorList;
	wxGauge*   mpProgressGauge;

	int GetNumFilesTotal() { return mNumFilesCounted; }

	private:
	friend class TestBackup;
	friend class TestRestore;
	friend class TestCompare;
	int GetProgressMax();
	int GetProgressPos();
	wxString GetNumFilesTotalString();
	wxString GetNumBytesTotalString();
	wxString GetNumFilesDoneString();
	wxString GetNumBytesDoneString();
	wxString GetNumFilesRemainingString();
	wxString GetNumBytesRemainingString();
	
	wxStaticText* mpSummaryText;
	wxStaticText* mpCurrentText;

	wxTextCtrl* mpNumFilesDone;
	wxTextCtrl* mpNumFilesRemaining;
	wxTextCtrl* mpNumFilesTotal;
	wxTextCtrl* mpNumBytesDone;
	wxTextCtrl* mpNumBytesRemaining;
	wxTextCtrl* mpNumBytesTotal;
	wxButton*   mpStopCloseButton;

	size_t  mNumFilesCounted;
	int64_t mNumBytesCounted;

	size_t  mNumFilesDone;
	int64_t mNumBytesDone;
	
	wxString FormatNumBytes(int64_t bytes);

	protected:	
	void NotifyMoreFilesCounted(size_t numAdditionalFiles, 
		int64_t numAdditionalBytes);

	void NotifyMoreFilesDone(size_t numAdditionalFiles, 
		int64_t numAdditionalBytes);

	void ReportFatalError(message_t messageId, wxString msg);
	
	void ResetCounters();
	void SetSummaryText    (const wxString& rText);
	void SetCurrentText    (const wxString& rText);
	void SetStopButtonLabel(const wxString& rLabel);

	virtual bool IsStopRequested() = 0;
		
	void CountLocalFiles(ExclusionOracle& rExclusionOracle,
		const std::string &rLocalPath);
	
	void NotifyCountDirectory(const std::string& rLocalPath)
	{
		wxString msg;
		msg.Printf(wxT("Counting files in directory '%s'"), 
			wxString(rLocalPath.c_str(), wxConvLibc).c_str());
		SetCurrentText(msg);
		Layout();
		wxYield();
	}	
};

class LogToListBox : public wxLog
{
	private:
	wxListBox* mpTarget;
	wxLog* mpOldTarget;
	
	public:
	LogToListBox(wxListBox* pTarget)
	: wxLog(), mpTarget(pTarget), mpOldTarget(wxLog::GetActiveTarget()) 
	{
		wxLog::SetActiveTarget(this);		
	}
	virtual ~LogToListBox() { wxLog::SetActiveTarget(mpOldTarget); }
	
	virtual void DoLog(wxLogLevel level, const wxChar *msg, 
		time_t timestamp);
};

#endif /* _PROGRESSPANEL_H */
