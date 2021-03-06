/***************************************************************************
 *            ProgressPanel.cc
 *
 *  Sun Oct  1 22:44:26 2006
 *  Copyright 2006-2009 Chris Wilson
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

#include "SandBox.h"

#include <errno.h>

#include <wx/button.h>
#include <wx/gauge.h>
#include <wx/intl.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "BackupStoreException.h"

#include "main.h"
#include "BoxiApp.h"
#include "ProgressPanel.h"

ProgressPanel::ProgressPanel
(
	wxWindow* parent, 
	wxWindowID id,
	const wxString& name
)
: wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, name)
{
	wxSizer* pMainSizer = new wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer* pSummaryBox = new wxStaticBoxSizer(wxVERTICAL,
		this, _("Summary"));
	pMainSizer->Add(pSummaryBox, 0, wxGROW | wxALL, 8);
	
	wxSizer* pSummarySizer = new wxGridSizer(1, 2, 0, 4);
	pSummaryBox->Add(pSummarySizer, 0, wxGROW | wxALL, 4);
	
	mpSummaryText = new wxStaticText(this, wxID_ANY, 
		_("Restore not started yet"));
	pSummarySizer->Add(mpSummaryText, 0, wxALIGN_CENTER_VERTICAL, 0);
	
	mpProgressGauge = new wxGauge(this, wxID_ANY, 100);
	mpProgressGauge->SetValue(0);
	pSummarySizer->Add(mpProgressGauge, 0, wxALIGN_CENTER_VERTICAL | wxGROW, 0);
	mpProgressGauge->Hide();
	
	wxStaticBoxSizer* pCurrentBox = new wxStaticBoxSizer(wxVERTICAL,
		this, _("Current Action"));
	pMainSizer->Add(pCurrentBox, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpCurrentText = new wxStaticText(this, wxID_ANY, 
		_("Idle (nothing to do)"), wxDefaultPosition, wxDefaultSize,
		wxST_NO_AUTORESIZE);
	pCurrentBox->Add(mpCurrentText, 0, wxGROW | wxALL, 4);
	
	wxStaticBoxSizer* pErrorsBox = new wxStaticBoxSizer(wxVERTICAL,
		this, _("Errors"));
	pMainSizer->Add(pErrorsBox, 1, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	mpErrorList = new wxListBox(this, ID_BackupProgress_ErrorList,
		wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_HSCROLL);
	pErrorsBox->Add(mpErrorList, 1, wxGROW | wxALL, 4);

	wxStaticBoxSizer* pStatsBox = new wxStaticBoxSizer(wxVERTICAL,
		this, _("Statistics"));
	pMainSizer->Add(pStatsBox, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	wxFlexGridSizer* pStatsGrid = new wxFlexGridSizer(4, 4, 4);
	pStatsBox->Add(pStatsGrid, 1, wxGROW | wxALL, 4);

	pStatsGrid->AddGrowableCol(0);
	pStatsGrid->AddGrowableCol(1);
	pStatsGrid->AddGrowableCol(2);
	pStatsGrid->AddGrowableCol(3);

	pStatsGrid->AddSpacer(1);
	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, _("Elapsed")));
	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, _("Remaining")));
	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, _("Total")));

	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, _("Files")));

	mpNumFilesDone = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumFilesDone, 1, wxGROW, 0);
	
	mpNumFilesRemaining = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumFilesRemaining, 1, wxGROW, 0);
	
	mpNumFilesTotal = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumFilesTotal, 1, wxGROW, 0);

	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, _("Bytes")));

	mpNumBytesDone = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumBytesDone, 1, wxGROW, 0);
	
	mpNumBytesRemaining = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumBytesRemaining, 1, wxGROW, 0);
	
	mpNumBytesTotal = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumBytesTotal, 1, wxGROW, 0);

	wxSizer* pButtonSizer = new wxBoxSizer(wxHORIZONTAL);
	pMainSizer->Add(pButtonSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpStopCloseButton = new wxButton(this, wxID_CANCEL, _("Close"));
	pButtonSizer->Add(mpStopCloseButton, 0, wxGROW, 0);

	mNumFilesCounted = 0;
	mNumBytesCounted = 0;

	SetSizer(pMainSizer);
}

void ProgressPanel::NotifyMoreFilesCounted(size_t numAdditionalFiles, 
	int64_t numAdditionalBytes)
{
	mNumFilesCounted += numAdditionalFiles;
	mNumBytesCounted += numAdditionalBytes;

	wxString str;
	str.Printf(_("%d"), mNumFilesCounted);
	mpNumFilesTotal->SetValue(str);
	mpNumBytesTotal->SetValue(FormatNumBytes(mNumBytesCounted));
	wxYield();
}

void ProgressPanel::NotifyMoreFilesDone(size_t numAdditionalFiles, 
	int64_t numAdditionalBytes)
{
	mNumFilesDone += numAdditionalFiles;
	mNumBytesDone += numAdditionalBytes;

	wxString str;
	str.Printf(_("%" wxLongLongFmtSpec "d"), (int64_t)mNumFilesDone);
	mpNumFilesDone->SetValue(str);
	mpNumBytesDone->SetValue(FormatNumBytes(mNumBytesDone));
	
	int64_t numFilesRemaining = mNumFilesCounted - mNumFilesDone;
	int64_t numBytesRemaining = mNumBytesCounted - mNumBytesDone;

	str.Printf(_("%" wxLongLongFmtSpec "d"), numFilesRemaining);
	mpNumFilesRemaining->SetValue(str);
	mpNumBytesRemaining->SetValue(FormatNumBytes(numBytesRemaining));
	
	mpProgressGauge->SetValue(mNumFilesDone);
	wxYield();
}

wxString ProgressPanel::FormatNumBytes(int64_t bytes)
{
	wxString units = _("B");
	
	if (bytes > 1024)
	{
		bytes >>= 10;
		units = _("kB");
	}
	
	if (bytes > 1024)
	{
		bytes >>= 10;
		units = _("MB");
	}

	if (bytes > 1024)
	{
		bytes >>= 10;
		units = _("GB");
	}

	wxString str;		
	str.Printf(_("%" wxLongLongFmtSpec "d %s"), bytes, units.c_str());
	return str;
}

void ProgressPanel::ReportFatalError(message_t messageId, wxString msg)
{
	wxGetApp().ShowMessageBox(messageId, msg, _("Boxi Error"), 
		wxOK | wxICON_ERROR, this);
	mpErrorList->Append(msg);
}

void LogToListBox::DoLog(wxLogLevel level, const wxChar *msg, 
	time_t timestamp)
{
	wxString msgOut;
	
	switch(level)
	{
		case wxLOG_FatalError:
			msgOut = _("Fatal: "); break;
		case wxLOG_Error:
			msgOut = _("Error: "); break;
		case wxLOG_Warning:
			msgOut = _("Warning: "); break;
		case wxLOG_Message:
			msgOut = _("Message: "); break;
		case wxLOG_Status: 
			msgOut = _("Status: "); break;
		case wxLOG_Info:
			msgOut = _("Info: "); break;
		case wxLOG_Debug:
			msgOut = _("Debug: "); break;
		case wxLOG_Trace:
			msgOut = _("Trace: "); break;
		default:
			msgOut.Printf(_("Unknown (level %d): "), level);
	}
	
	msgOut.Append(msg);
	mpTarget->Append(msgOut);
}

int ProgressPanel::GetProgressMax() { return mpProgressGauge->GetRange(); }
int ProgressPanel::GetProgressPos() { return mpProgressGauge->GetValue(); }

wxString ProgressPanel::GetNumFilesTotalString()
{ return mpNumFilesTotal->GetValue(); }

wxString ProgressPanel::GetNumBytesTotalString()
{ return mpNumBytesTotal->GetValue(); }

wxString ProgressPanel::GetNumFilesDoneString()
{ return mpNumFilesDone->GetValue(); }

wxString ProgressPanel::GetNumBytesDoneString()
{ return mpNumBytesDone->GetValue(); }

wxString ProgressPanel::GetNumFilesRemainingString()
{ return mpNumFilesRemaining->GetValue(); }

wxString ProgressPanel::GetNumBytesRemainingString()
{ return mpNumBytesRemaining->GetValue(); }

void ProgressPanel::ResetCounters()
{
	mNumFilesCounted = 0;
	mNumBytesCounted = 0;
	mNumFilesDone = 0;
	mNumBytesDone = 0;
	
	mpNumFilesTotal    ->SetValue(wxT(""));
	mpNumBytesTotal    ->SetValue(wxT(""));
	mpNumFilesDone     ->SetValue(wxT(""));
	mpNumBytesDone     ->SetValue(wxT(""));
	mpNumFilesRemaining->SetValue(wxT(""));
	mpNumBytesRemaining->SetValue(wxT(""));
}

void ProgressPanel::SetSummaryText(const wxString& rText)
{
	mpSummaryText->SetLabel(rText);
}

void ProgressPanel::SetCurrentText(const wxString& rText)
{
	mpCurrentText->SetLabel(rText);
	// the following attempt to make the label wrap doesn't compile
	// on wx 2.6.x and doesn't work anyway
	/*
	wxSize size = mpCurrentText->GetSize();
	mpCurrentText->Wrap(size.GetWidth());
	size = mpCurrentText->GetSize();
	// set minimum and maximum size to laid-out size
	mpCurrentText->SetSizeHints(size, size);
	// resize everything else to take account of new label size
	*/
	Layout();
}

void ProgressPanel::SetStopButtonLabel(const wxString& rLabel)
{
	mpStopCloseButton->SetLabel(rLabel);
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    ProgressPanel::CountLocalFiles(
//			 ExclusionOracle& rExclusionOracle,
//			 const std::string &rLocalPath)
//		Purpose: Recursively count files in a local directory
//			 and subdirectories.
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------
void ProgressPanel::CountLocalFiles(ExclusionOracle& rExclusionOracle,
	const std::string &rLocalPath)
{
	NotifyCountDirectory(rLocalPath);
	
	// Signal received by daemon?
	if(IsStopRequested())
	{
		// Yes. Stop now.
		THROW_EXCEPTION(BackupStoreException, SignalReceived)
	}
	
	// Read directory entries, counting number and size of files,
	// and recursing into directories.
	size_t  filesCounted = 0;
	int64_t bytesCounted = 0;
	
	// read the contents...
	DIR *dirHandle = 0;
	try
	{
		dirHandle = ::opendir(rLocalPath.c_str());
		if(dirHandle == 0)
		{
			// Ignore this directory for now.
			return;
		}
		
		struct dirent *en = 0;
		EMU_STRUCT_STAT st;
		std::string filename;
		while((en = ::readdir(dirHandle)) != 0)
		{
			// Don't need to use LinuxWorkaround_FinishDirentStruct(en, rLocalPath.c_str());
			// on Linux, as a stat is performed to get all this info

			if(en->d_name[0] == '.' && 
				(en->d_name[1] == '\0' || (en->d_name[1] == '.' && en->d_name[2] == '\0')))
			{
				// ignore, it's . or ..
				continue;
			}

			// Stat file to get info
			filename = rLocalPath + DIRECTORY_SEPARATOR + en->d_name;
			if(EMU_LSTAT(filename.c_str(), &st) != 0)
			{
				/*
				rParams.GetProgressNotifier().NotifyFileStatFailed(
					(BackupClientDirectoryRecord*)NULL, 
					filename, strerror(errno));
				THROW_EXCEPTION(CommonException, OSFileError)
				*/
				wxString msg;
				msg.Printf(_("Error counting files in '%s': %s"),
					wxString(filename.c_str(), wxConvBoxi).c_str(),
					wxString(strerror(errno),  wxConvBoxi).c_str());
				mpErrorList->Append(msg);
			}

			int type = st.st_mode & S_IFMT;
			if(type == S_IFREG || type == S_IFLNK)
			{
				// File or symbolic link

				// Exclude it?
				if (rExclusionOracle.IsExcludedFile(filename))
				{
					// Next item!
					continue;
				}

				filesCounted++;
				bytesCounted += st.st_size;
			}
			else if(type == S_IFDIR)
			{
				// Directory
				
				// Exclude it?
				if (rExclusionOracle.IsExcludedDir(filename))
				{
					// Next item!
					continue;
				}
				
				CountLocalFiles(rExclusionOracle, filename);
			}
			else
			{
				continue;
			}
		}

		if(::closedir(dirHandle) != 0)
		{
			THROW_EXCEPTION(CommonException, OSFileError)
		}
		dirHandle = 0;
		
	}
	catch(...)
	{
		if(dirHandle != 0)
		{
			::closedir(dirHandle);
		}
		throw;
	}

	NotifyMoreFilesCounted(filesCounted, bytesCounted);
}
