/***************************************************************************
 *            RestoreProgressPanel.h
 *
 *  Sun Sep 3 21:02:39 2006
 *  Copyright 2006 Chris Wilson
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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#ifndef _RESTORE_PROGRESS_PANEL_H
#define _RESTORE_PROGRESS_PANEL_H

#include <wx/wx.h>
#include <wx/thread.h>

#include "SandBox.h"

#define NDEBUG
#include "TLSContext.h"
#include "BackupClientContext.h"
#include "BackupClientDirectoryRecord.h"
#include "BackupDaemon.h"
#include "BackupStoreException.h"
#undef NDEBUG

#include "BoxiApp.h"
#include "ClientConfig.h"
#include "ClientConnection.h"

class ServerCacheNode;
	
/** 
 * RestoreProgressPanel
 * Shows status of a running backup and allows user to pause or cancel it.
 */

class ServerConnection;
class RestoreSpec;

class RestoreProgressPanel 
: public wxPanel
{
	public:
	RestoreProgressPanel(
		ClientConfig*     pConfig,
		ServerConnection* pConnection,
		wxWindow* parent, wxWindowID id = -1,
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL, 
		const wxString& name = wxT("Restore Progress Panel"));

	void StartRestore(const RestoreSpec& rSpec, wxFileName dest);

	wxString GetNumFilesTotalString() { return mpNumFilesTotal->GetValue(); }
	wxString GetNumBytesTotalString() { return mpNumBytesTotal->GetValue(); }
	wxString GetNumFilesDoneString() { return mpNumFilesDone->GetValue(); }
	wxString GetNumBytesDoneString() { return mpNumBytesDone->GetValue(); }
	wxString GetNumFilesRemainingString() { return mpNumFilesRemaining->GetValue(); }
	wxString GetNumBytesRemainingString() { return mpNumBytesRemaining->GetValue(); }
	int GetProgressMax() { return mpProgressGauge->GetRange(); }
	int GetProgressPos() { return mpProgressGauge->GetValue(); }

	private:
	ClientConfig*     mpConfig;
	ServerConnection* mpConnection;
	TLSContext        mTlsContext;
	wxListBox*        mpErrorList;
	wxStaticText*     mpSummaryText;
	wxStaticText*     mpCurrentText;
	wxTextCtrl*       mpNumFilesDone;
	wxTextCtrl*       mpNumFilesRemaining;
	wxTextCtrl*       mpNumFilesTotal;
	wxTextCtrl*       mpNumBytesDone;
	wxTextCtrl*       mpNumBytesRemaining;
	wxTextCtrl*       mpNumBytesTotal;
	wxButton*         mpStopCloseButton;
	wxGauge*          mpProgressGauge;
	bool              mStorageLimitExceeded;
	bool              mRestoreRunning;
	bool              mRestoreStopRequested;

	size_t  mNumFilesCounted;
	int64_t mNumBytesCounted;

	size_t  mNumFilesDownloaded;
	int64_t mNumBytesDownloaded;
	
	void RestoreProgressPanel::CountDirectory(BackupClientContext& rContext,
		const std::string &rLocalPath);
	void NotifyCountDirectory(const std::string& rLocalPath)
	{
		wxString msg;
		msg.Printf(wxT("Counting files in directory '%s'"), 
			wxString(rLocalPath.c_str(), wxConvLibc).c_str());
		mpCurrentText->SetLabel(msg);
		Layout();
		wxYield();
	}
	
	wxString FormatNumBytes(int64_t bytes)
	{
		wxString units = wxT("B");
		
		if (bytes > 1024)
		{
			bytes >>= 10;
			units = wxT("kB");
		}
		
		if (bytes > 1024)
		{
			bytes >>= 10;
			units = wxT("MB");
		}

		if (bytes > 1024)
		{
			bytes >>= 10;
			units = wxT("GB");
		}

		wxString str;		
		str.Printf(wxT("%lld %s"), bytes, units.c_str());
		return str;
	}
	
	void NotifyMoreFilesCounted(size_t numAdditionalFiles, 
		int64_t numAdditionalBytes)
	{
		mNumFilesCounted += numAdditionalFiles;
		mNumBytesCounted += numAdditionalBytes;

		wxString str;
		str.Printf(wxT("%d"), mNumFilesCounted);
		mpNumFilesTotal->SetValue(str);
		mpNumBytesTotal->SetValue(FormatNumBytes(mNumBytesCounted));
	}

	void NotifyMoreFilesRestored(size_t numAdditionalFiles, 
		int64_t numAdditionalBytes)
	{
		mNumFilesDownloaded += numAdditionalFiles;
		mNumBytesDownloaded += numAdditionalBytes;

		wxString str;
		str.Printf(wxT("%d"), mNumFilesDownloaded);
		mpNumFilesDone->SetValue(str);
		mpNumBytesDone->SetValue(FormatNumBytes(mNumBytesDownloaded));
		
		int64_t numFilesRemaining = mNumFilesCounted - mNumFilesDownloaded;
		int64_t numBytesRemaining = mNumBytesCounted - mNumBytesDownloaded;

		str.Printf(wxT("%d"), numFilesRemaining);
		mpNumFilesRemaining->SetValue(str);
		mpNumBytesRemaining->SetValue(FormatNumBytes(numBytesRemaining));
		
		mpProgressGauge->SetValue(mNumFilesDownloaded);
		wxYield();
	}

	void ReportRestoreFatalError(message_t messageId, wxString msg)
	{
		wxGetApp().ShowMessageBox(messageId, msg, wxT("Boxi Error"), 
			wxOK | wxICON_ERROR, this);
		mpErrorList->Append(msg);
	}

	virtual void OnStopCloseClicked(wxCommandEvent& event) 
	{ 
		if (mRestoreRunning)
		{
			mRestoreStopRequested = TRUE;
		}
		else
		{
			Hide();
		}
	}
	
	void CountFilesRecursive(const RestoreSpec& rSpec, 
		ServerCacheNode* pNode, int blockSize);
	bool RestoreFilesRecursive(const RestoreSpec& rSpec, 
		ServerCacheNode* pNode,	int64_t parentId, wxFileName localName, 
		int blockSize);
	wxFileName MakeLocalPath(wxFileName base, wxString serverPath);

	DECLARE_EVENT_TABLE()
};

#endif /* _RESTORE_PROGRESS_PANEL_H */
