/***************************************************************************
 *            RestoreProgressPanel.cc
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
 *
 * Contains software developed by Ben Summers.
 * YOU MUST NOT REMOVE THIS ATTRIBUTION!
 */

#include <errno.h>
#include <mntent.h>
#include <stdio.h>
#include <regex.h>

#include <wx/statbox.h>
#include <wx/listbox.h>
#include <wx/button.h>
#include <wx/gauge.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>

#include "SandBox.h"

#define NDEBUG
#define EXCLUDELIST_IMPLEMENTATION_REGEX_T_DEFINED
#include "BackupClientContext.h"
#include "BackupClientDirectoryRecord.h"
#include "BackupClientCryptoKeys.h"
#include "BackupClientInodeToIDMap.h"
#include "FileModificationTime.h"
#include "MemBlockStream.h"
#include "BackupStoreConstants.h"
#include "BackupStoreException.h"
#include "Utils.h"
#undef EXCLUDELIST_IMPLEMENTATION_REGEX_T_DEFINED
#undef NDEBUG

#include "main.h"
#include "RestoreFilesPanel.h"
#include "RestoreProgressPanel.h"
#include "ServerConnection.h"

//DECLARE_EVENT_TYPE(myEVT_CLIENT_NOTIFY, -1)
//DEFINE_EVENT_TYPE(myEVT_CLIENT_NOTIFY)

BEGIN_EVENT_TABLE(RestoreProgressPanel, wxPanel)
EVT_BUTTON(wxID_CANCEL, RestoreProgressPanel::OnStopCloseClicked)
END_EVENT_TABLE()

RestoreProgressPanel::RestoreProgressPanel(
	ClientConfig*     pConfig,
	ServerConnection* pConnection,
	wxWindow* parent, 
	wxWindowID id,
	const wxPoint& pos, 
	const wxSize& size,
	long style, 
	const wxString& name)
	: wxPanel(parent, id, pos, size, style, name),
	  mpConfig(pConfig),
	  mpConnection(pConnection),
	  mRestoreRunning(false)
{
	wxSizer* pMainSizer = new wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer* pSummaryBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Summary"));
	pMainSizer->Add(pSummaryBox, 0, wxGROW | wxALL, 8);
	
	wxSizer* pSummarySizer = new wxGridSizer(1, 2, 0, 4);
	pSummaryBox->Add(pSummarySizer, 0, wxGROW | wxALL, 4);
	
	mpSummaryText = new wxStaticText(this, wxID_ANY, 
		wxT("Restore not started yet"));
	pSummarySizer->Add(mpSummaryText, 0, wxALIGN_CENTER_VERTICAL, 0);
	
	mpProgressGauge = new wxGauge(this, wxID_ANY, 100);
	mpProgressGauge->SetValue(0);
	pSummarySizer->Add(mpProgressGauge, 0, wxALIGN_CENTER_VERTICAL | wxGROW, 0);
	mpProgressGauge->Hide();
	
	wxStaticBoxSizer* pCurrentBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Current Action"));
	pMainSizer->Add(pCurrentBox, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpCurrentText = new wxStaticText(this, wxID_ANY, 
		wxT("Idle (nothing to do)"));
	pCurrentBox->Add(mpCurrentText, 0, wxGROW | wxALL, 4);
	
	wxStaticBoxSizer* pErrorsBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Errors"));
	pMainSizer->Add(pErrorsBox, 1, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	mpErrorList = new wxListBox(this, ID_BackupProgress_ErrorList);
	pErrorsBox->Add(mpErrorList, 1, wxGROW | wxALL, 4);

	wxStaticBoxSizer* pStatsBox = new wxStaticBoxSizer(wxVERTICAL,
		this, wxT("Statistics"));
	pMainSizer->Add(pStatsBox, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	wxFlexGridSizer* pStatsGrid = new wxFlexGridSizer(4, 4, 4);
	pStatsBox->Add(pStatsGrid, 1, wxGROW | wxALL, 4);

	pStatsGrid->AddGrowableCol(0);
	pStatsGrid->AddGrowableCol(1);
	pStatsGrid->AddGrowableCol(2);
	pStatsGrid->AddGrowableCol(3);

	pStatsGrid->AddSpacer(1);
	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, wxT("Elapsed")));
	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, wxT("Remaining")));
	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, wxT("Total")));

	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, wxT("Files")));

	mpNumFilesDone = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumFilesDone, 1, wxGROW, 0);
	
	mpNumFilesRemaining = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumFilesRemaining, 1, wxGROW, 0);
	
	mpNumFilesTotal = new wxTextCtrl(this, wxID_ANY, wxT(""), 
		wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	pStatsGrid->Add(mpNumFilesTotal, 1, wxGROW, 0);

	pStatsGrid->Add(new wxStaticText(this, wxID_ANY, wxT("Bytes")));

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

	mpStopCloseButton = new wxButton(this, wxID_CANCEL, wxT("Close"));
	pButtonSizer->Add(mpStopCloseButton, 0, wxGROW, 0);

	mRestoreRunning = false;
	mRestoreStopRequested = false;
	mNumFilesCounted = 0;
	mNumBytesCounted = 0;

	SetSizer(pMainSizer);
}

class wxLogListBox : public wxLog
{
	private:
	wxListBox* mpTarget;
	wxLog* mpOldTarget;
	
	public:
	wxLogListBox(wxListBox* pTarget)
	: wxLog(), mpTarget(pTarget), mpOldTarget(wxLog::GetActiveTarget()) 
	{
		wxLog::SetActiveTarget(this);		
	}
	virtual ~wxLogListBox() { wxLog::SetActiveTarget(mpOldTarget); }
	
	virtual void DoLog(wxLogLevel level, const wxChar *msg, time_t timestamp)
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
};

wxFileName RestoreProgressPanel::MakeLocalPath(wxFileName base, wxString serverPath)
{
	wxLogListBox logTo(mpErrorList);
	
	if (!serverPath.StartsWith(_("/")))
	{
		return wxFileName();
	}
	serverPath = serverPath.Mid(1);
	
	wxFileName outName(base.GetFullPath(), wxEmptyString);
	
	for (int index = serverPath.Find('/'); index != -1; 
		index = serverPath.Find('/'))
	{
		if (index <= 0)
		{
			return wxFileName();
		}
		
		outName.AppendDir(serverPath.Mid(0, index));
		if (!outName.DirExists() && !wxMkdir(outName.GetFullPath()))
		{
			return wxFileName();
		}
		
		serverPath = serverPath.Mid(index + 1);
	}
		
	outName.SetFullName(serverPath);	
	if (!outName.IsOk())
	{
		return wxFileName();
	}
	
	return outName;	
}

void RestoreProgressPanel::StartRestore(const RestoreSpec& rSpec, wxFileName dest)
{
	RestoreSpec spec(rSpec);
	
	mpErrorList->Clear();

	wxString errorMsg;
	if (!mpConnection->InitTlsContext(mTlsContext, errorMsg))
	{
		wxString msg;
		msg.Printf(wxT("Error: cannot start restore: %s"), errorMsg.c_str());
		ReportRestoreFatalError(BM_BACKUP_FAILED_CANNOT_INIT_ENCRYPTION, msg);
		return;
	}
	
	std::string storeHost;
	mpConfig->StoreHostname.GetInto(storeHost);

	if (storeHost.length() == 0) 
	{
		ReportRestoreFatalError(BM_BACKUP_FAILED_NO_STORE_HOSTNAME,
			wxT("Error: cannot start restore: "
			"You have not configured the Store Hostname!"));
		return;
	}

	std::string keysFile;
	mpConfig->KeysFile.GetInto(keysFile);

	if (keysFile.length() == 0) 
	{
		ReportRestoreFatalError(BM_BACKUP_FAILED_NO_KEYS_FILE,
			wxT("Error: cannot start restore: "
			"you have not configured the Keys File"));
		return;
	}

	int acctNo;
	if (!mpConfig->AccountNumber.GetInto(acctNo))
	{
		ReportRestoreFatalError(BM_BACKUP_FAILED_NO_ACCOUNT_NUMBER,
			wxT("Error: cannot start restore: "
			"you have not configured the Account Number"));
		return;
	}
	
	BackupClientCryptoKeys_Setup(keysFile.c_str());

	{
		wxLogListBox logTo(mpErrorList);
		if (!wxMkdir(dest.GetFullPath()))
		{
			ReportRestoreFatalError(BM_RESTORE_FAILED_TO_CREATE_OBJECT,
				wxT("Error: cannot start restore: "
				"cannot create the destination directory"));
			return;
		}
	}
	
	mNumFilesCounted = 0;
	mNumBytesCounted = 0;
	mNumFilesDownloaded = 0;
	mNumBytesDownloaded = 0;
	
	mpSummaryText->SetLabel(wxT("Starting Restore"));
	mRestoreRunning = true;
	mRestoreStopRequested = false;
	mpStopCloseButton->SetLabel(wxT("Stop Restore"));
	mpNumFilesTotal->SetValue(wxT(""));
	mpNumBytesTotal->SetValue(wxT(""));
	mpNumFilesDone->SetValue(wxT(""));
	mpNumBytesDone->SetValue(wxT(""));
	mpNumFilesRemaining->SetValue(wxT(""));
	mpNumBytesRemaining->SetValue(wxT(""));
	Layout();
	wxYield();
	
	try 
	{
		// count files to restore
		std::auto_ptr<BackupProtocolClientAccountUsage> accountInfo = 
			mpConnection->GetAccountUsage();
		int blockSize = 0;
		if (accountInfo.get())
		{
			blockSize = accountInfo->GetBlockSize();
		}
		else
		{
			mpErrorList->Append(_("Warning: failed to get account "
				"information from server, file size will not "
				"be reported"));
		}
		
		RestoreSpecEntry::Vector entries = spec.GetEntries();
		for (RestoreSpecEntry::ConstIterator i = entries.begin();
			i != entries.end(); i++)
		{
			// Duplicate entries may be removed from the list
			// by CountFilesRecursive, so check whether the
			// current entry has been removed, and if so, skip it.
			RestoreSpecEntry::Vector newEntries = spec.GetEntries();
			bool foundEntry = false;
			
			for (RestoreSpecEntry::ConstIterator j = newEntries.begin();
				j != newEntries.end(); j++)
			{
				if (j->IsSameAs(*i))
				{
					foundEntry = true;
					break;
				}
			}
			
			if (!foundEntry)
			{
				// no longer in the list, skip it.
				continue;
			}
			
			if (i->IsInclude())
			{
				ServerCacheNode* pNode = i->GetNode();
				CountFilesRecursive(spec, pNode, pNode, 
					blockSize);
			}
		}
		
		mpProgressGauge->SetRange(mNumFilesCounted);
		mpProgressGauge->SetValue(0);
		mpProgressGauge->Show();

		mpSummaryText->SetLabel(_("Restoring files"));
		wxYield();
		
		// Entries may have been changed by removing duplicates
		// in CountFilesRecursive. Reload the list.
		entries = spec.GetEntries();
		
		bool succeeded = false;
		
		// Go through the records agains, this time syncing them
		for (RestoreSpecEntry::ConstIterator i = entries.begin();
			i != entries.end(); i++)
		{
			if (i->IsInclude())
			{
				ServerCacheNode* pNode = i->GetNode();
				wxFileName restoreDest = MakeLocalPath(dest, 
					pNode->GetFullPath());
				
				if (restoreDest.IsOk())
				{
					succeeded = RestoreFilesRecursive(spec, pNode, 
						pNode->GetParent()->GetMostRecent()->GetBoxFileId(),
						restoreDest, blockSize);
				}
				else
				{
					succeeded = false;
				}
				
			}
			
			if (!succeeded) break;
		}
		
		if (mRestoreStopRequested)
		{
			mpSummaryText->SetLabel(wxT("Restore Interrupted"));
			ReportRestoreFatalError(BM_BACKUP_FAILED_INTERRUPTED,
				wxT("Restore interrupted by user"));
			mpSummaryText->SetLabel(wxT("Restore Interrupted"));
		}
		else if (!succeeded)
		{
			mpSummaryText->SetLabel(wxT("Restore Failed"));
			mpErrorList  ->Append  (wxT("Restore Failed"));
		}
		else
		{
			mpSummaryText->SetLabel(wxT("Restore Finished"));
			mpErrorList  ->Append  (wxT("Restore Finished"));
		}
		
		mpProgressGauge->Hide();
	}
	catch (ConnectionException& e) 
	{
		mpSummaryText->SetLabel(wxT("Restore Failed"));
		wxString msg;
		msg.Printf(_("Error: cannot start restore: "
			"Failed to connect to server: %s"),
			wxString(e.what(), wxConvLibc).c_str());
		ReportRestoreFatalError(BM_BACKUP_FAILED_CONNECT_FAILED, msg);
	}
	catch (std::exception& e) 
	{
		mpSummaryText->SetLabel(_("Restore Failed"));
		wxString msg;
		msg.Printf(_("Error: failed to finish restore: %s"),
			wxString(e.what(), wxConvLibc).c_str());
		ReportRestoreFatalError(BM_BACKUP_FAILED_UNKNOWN_ERROR, msg);
	}
	catch (...)
	{
		mpSummaryText->SetLabel(wxT("Restore Failed"));
		ReportRestoreFatalError(BM_BACKUP_FAILED_UNKNOWN_ERROR,
			wxT("Error: failed to finish restore: unknown error"));
	}	

	mpCurrentText->SetLabel(wxT("Idle (nothing to do)"));
	mRestoreRunning = false;
	mRestoreStopRequested = false;
	mpStopCloseButton->SetLabel(wxT("Close"));
}

void RestoreProgressPanel::CountFilesRecursive
(
	RestoreSpec& rSpec, ServerCacheNode* pRootNode, 
	ServerCacheNode* pCurrentNode, int blockSize
)
{
	// stop if requested
	if (mRestoreStopRequested) return;
		
	ServerFileVersion* pVersion = pCurrentNode->GetMostRecent();
	
	RestoreSpecEntry::Vector specs = rSpec.GetEntries();

	for (RestoreSpecEntry::Iterator i = specs.begin();
		i != specs.end(); i++)
	{
		if (i->GetNode() == pCurrentNode)
		{
			if (i->IsInclude() && pCurrentNode != pRootNode)
			{
				// Redundant entry in include list.
				// Remove it to prevent double restoring.
				rSpec.Remove(*i);
			}
			else if (!i->IsInclude())
			{
				// excluded, stop here.
				return;
			}
		}
	}
	
	if (pVersion->IsDirectory())
	{
		const ServerCacheNode::Vector* pChildren = 
			pCurrentNode->GetChildren();
		
		if (pChildren)
		{
			for (ServerCacheNode::ConstIterator i = pChildren->begin();
				i != pChildren->end(); i++)
			{
				CountFilesRecursive(rSpec, pRootNode, *i, 
					blockSize);
			}
		}
	}
	else
	{
		NotifyMoreFilesCounted(1, pVersion->GetSizeBlocks() * blockSize);
	}
}

bool RestoreProgressPanel::RestoreFilesRecursive
(
	const RestoreSpec& rSpec, ServerCacheNode* pNode, int64_t parentId,
	wxFileName localName, int blockSize
)
{
	// stop if requested
	if (mRestoreStopRequested) return false;
		
	ServerFileVersion* pVersion = pNode->GetMostRecent();
	
	RestoreSpecEntry::Vector specs = rSpec.GetEntries();
	for (RestoreSpecEntry::Iterator i = specs.begin();
		i != specs.end(); i++)
	{
		if (i->GetNode() == pNode && ! i->IsInclude())
		{
			// excluded, stop here.
			return true;
		}
	}
	
	if (localName.FileExists() || localName.DirExists())
	{
		wxString msg;
		msg.Printf(_("Error: failed to finish restore: "
			"object already exists: '%s'"), 
			localName.GetFullPath().c_str());
		ReportRestoreFatalError(
			BM_RESTORE_FAILED_OBJECT_ALREADY_EXISTS, msg);
		return false;
	}

	wxCharBuffer namebuf = localName.GetFullPath().mb_str(wxConvLibc);
		
	if (pVersion->IsDirectory())
	{
		if (!wxMkdir(localName.GetFullPath()))
		{
			wxString msg;
			msg.Printf(_("Error: failed to finish restore: "
				"cannot create directory: '%s'"), 
				localName.GetFullPath().c_str());
			ReportRestoreFatalError(
				BM_RESTORE_FAILED_TO_CREATE_OBJECT, msg);
			return false;
		}
		
		pVersion->GetAttributes().WriteAttributes(namebuf.data());
			 
		const ServerCacheNode::Vector* pChildren = pNode->GetChildren();
		if (pChildren)
		{
			for (ServerCacheNode::ConstIterator i = pChildren->begin();
				i != pChildren->end(); i++)
			{
				wxFileName childName(localName.GetFullPath(),
					(*i)->GetFileName());
				if (!RestoreFilesRecursive(rSpec, *i, 
					pVersion->GetBoxFileId(), childName, 
					blockSize))
				{
					return false;
				}
			}
		}
	}
	else
	{		
		if (!mpConnection->GetFile(parentId, pVersion->GetBoxFileId(), 
			namebuf.data()))
		{
			return false;
		}
		
		// Decode the file -- need to do different things depending on whether 
		// the directory entry has additional attributes
		if(pVersion->HasAttributes())
		{
			// Use these attributes
			pVersion->GetAttributes().WriteAttributes(namebuf.data());
		}

		NotifyMoreFilesRestored(1, pVersion->GetSizeBlocks() * blockSize);		
	}
	
	return true;
}
