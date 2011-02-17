/***************************************************************************
 *            RestoreProgressPanel.cc
 *
 *  Sun Sep 3 21:02:39 2006
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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Contains software developed by Ben Summers.
 * YOU MUST NOT REMOVE THIS ATTRIBUTION!
 */

#include "SandBox.h"

#include <errno.h>
#include <stdio.h>

#include <wx/statbox.h>
#include <wx/listbox.h>
#include <wx/button.h>
#include <wx/gauge.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>

#include "BackupClientContext.h"
#include "BackupClientDirectoryRecord.h"
#include "BackupClientCryptoKeys.h"
#include "BackupClientInodeToIDMap.h"
#include "FileModificationTime.h"
#include "MemBlockStream.h"
#include "BackupStoreConstants.h"
#include "BackupStoreException.h"
#include "Utils.h"

#include "main.h"
#include "RestoreFilesPanel.h"
#include "RestoreProgressPanel.h"
#include "ServerConnection.h"

//DECLARE_EVENT_TYPE(myEVT_CLIENT_NOTIFY, -1)
//DEFINE_EVENT_TYPE(myEVT_CLIENT_NOTIFY)

BEGIN_EVENT_TABLE(RestoreProgressPanel, wxPanel)
EVT_BUTTON(wxID_CANCEL, RestoreProgressPanel::OnStopCloseClicked)
END_EVENT_TABLE()

RestoreProgressPanel::RestoreProgressPanel
(
	ClientConfig*     pConfig,
	ServerConnection* pConnection,
	wxWindow*         pParent
)
: ProgressPanel(pParent, ID_Restore_Progress_Panel, _("Restore Progress Panel")),
  mpConfig(pConfig),
  mpConnection(pConnection),
  mRestoreRunning(false),
  mRestoreStopRequested(false)
{
}

wxFileName MakeLocalPath(wxFileName& base, ServerCacheNode* pTargetNode)
{
	wxString remainingPath = pTargetNode->GetFullPath();
	
	if (!remainingPath.StartsWith(_("/")))
	{
		return wxFileName();
	}
	remainingPath = remainingPath.Mid(1);

	wxString currentPath;
	wxFileName outName(base.GetFullPath(), wxEmptyString);
	
	for (int index = remainingPath.Find('/'); index != -1; 
		index = remainingPath.Find('/'))
	{
		if (index <= 0)
		{
			return wxFileName();
		}
		
		wxString pathComponent = remainingPath.Mid(0, index);
		outName.AppendDir(pathComponent);
		if (!outName.DirExists() && !wxMkdir(outName.GetFullPath()))
		{
			return wxFileName();
		}
		
		currentPath.Append(_("/"));
		currentPath.Append(pathComponent);
		remainingPath = remainingPath.Mid(index + 1);
		
		// Find the server cache node corresponding to the
		// directory we just created, and restore its attributes.
		ServerCacheNode* pCurrentNode = NULL;
		
		for (pCurrentNode = pTargetNode; 
			pCurrentNode->GetParent() != pCurrentNode && 
			!(pCurrentNode->GetFullPath().IsSameAs(currentPath));
			pCurrentNode = pCurrentNode->GetParent())
		{ ; }

		// make sure that we found one		
		if (!(pCurrentNode->GetFullPath().IsSameAs(currentPath)))
		{
			return wxFileName();
		}
		
		// retrieve most recent attributes, and apply them
		ServerFileVersion* pVersion = pCurrentNode->GetMostRecent();
		if (!pVersion)
		{
			return wxFileName();
		}

		if (pVersion->HasAttributes())
		{
			wxCharBuffer namebuf = outName.GetFullPath().mb_str();
			pVersion->GetAttributes().WriteAttributes(namebuf.data());
		}
	}
		
	outName.SetFullName(remainingPath);	
	if (!outName.IsOk())
	{
		return wxFileName();
	}
	
	return outName;	
}

void RestoreProgressPanel::StartRestore(const RestoreSpec& rSpec,
	wxFileName& rDest)
{
	RestoreSpec spec(rSpec);
	LogToListBox logTo(mpErrorList);
	
	mpErrorList->Clear();

	wxString errorMsg;
	if (!mpConnection->InitTlsContext(mTlsContext, errorMsg))
	{
		wxString msg;
		msg.Printf(_("Error: cannot start restore: %s"), errorMsg.c_str());
		ReportFatalError(BM_RESTORE_FAILED_CANNOT_INIT_ENCRYPTION, msg);
		return;
	}
	
	std::string storeHost;
	mpConfig->StoreHostname.GetInto(storeHost);

	if (storeHost.length() == 0) 
	{
		ReportFatalError(BM_RESTORE_FAILED_NO_STORE_HOSTNAME,
			_("Error: cannot start restore: "
			"You have not configured the Store Hostname!"));
		return;
	}

	std::string keysFile;
	mpConfig->KeysFile.GetInto(keysFile);

	if (keysFile.length() == 0) 
	{
		ReportFatalError(BM_RESTORE_FAILED_NO_KEYS_FILE,
			_("Error: cannot start restore: "
			"you have not configured the Keys File"));
		return;
	}

	int acctNo;
	if (!mpConfig->AccountNumber.GetInto(acctNo))
	{
		ReportFatalError(BM_RESTORE_FAILED_NO_ACCOUNT_NUMBER,
			_("Error: cannot start restore: "
			"you have not configured the Account Number"));
		return;
	}
	
	BackupClientCryptoKeys_Setup(keysFile.c_str());

	{
		if (!wxMkdir(rDest.GetFullPath()))
		{
			ReportFatalError(BM_RESTORE_FAILED_TO_CREATE_OBJECT,
				_("Error: cannot start restore: "
				"cannot create the destination directory"));
			return;
		}
	}
	
	ResetCounters();
	
	mRestoreRunning = true;
	mRestoreStopRequested = false;
	SetSummaryText(_("Starting Restore"));
	SetStopButtonLabel(_("Stop Restore"));

	Layout();
	wxYield();
	
	try 
	{
		SetCurrentText(_("Connecting to server"));
		mpConnection->Connect(false);

		SetSummaryText(_("Checking account details"));
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
		
		SetSummaryText(_("Counting Files to Restore"));
		SetCurrentText(wxT(""));

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
				ServerCacheNode* pNode = &(i->GetNode());
				CountFilesRecursive(spec, pNode, pNode, 
					blockSize);
			}
		}
		
		mpProgressGauge->SetRange(GetNumFilesTotal());
		mpProgressGauge->SetValue(0);
		mpProgressGauge->Show();

		SetSummaryText(_("Restoring files"));
		wxYield();
		
		// Entries may have been changed by removing duplicates
		// in CountFilesRecursive. Reload the list.
		entries = spec.GetEntries();
		
		bool succeeded = false;
		
		// Go through the records again, this time syncing them
		for (RestoreSpecEntry::Iterator i = entries.begin();
			i != entries.end(); i++)
		{
			if (i->IsInclude())
			{
				ServerCacheNode& rNode(i->GetNode());
				wxFileName restoreDest = MakeLocalPath(rDest, 
					&rNode);
				
				if (restoreDest.IsOk())
				{
					ServerCacheNode* pParentNode =
						rNode.GetParent();
					int64_t parentId = -1; // invalid
					
					if (pParentNode == NULL)
					{
						// restoring the root directory.
						// parentId is ignored for 
						// directories, which is good
						// because we don't have one!
					}
					else
					{
						parentId = pParentNode->GetMostRecent()->GetBoxFileId();
					}
					
					succeeded = RestoreFilesRecursive(spec, 
						&rNode, parentId, restoreDest, 
						blockSize);
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
			SetSummaryText(_("Restore Interrupted"));
			ReportFatalError(BM_BACKUP_FAILED_INTERRUPTED,
				_("Restore interrupted by user"));
		}
		else if (!succeeded)
		{
			SetSummaryText(_("Restore Failed"));
			mpErrorList->Append(_("Restore Failed"));
		}
		else
		{
			SetSummaryText(_("Restore Finished"));
			mpErrorList->Append(_("Restore Finished"));
		}
		
		mpProgressGauge->Hide();
	}
	catch (ConnectionException& e) 
	{
		SetSummaryText(_("Restore Failed"));
		wxString msg;
		msg.Printf(_("Error: cannot start restore: "
			"Failed to connect to server: %s"),
			wxString(e.what(), wxConvBoxi).c_str());
		ReportFatalError(BM_BACKUP_FAILED_CONNECT_FAILED, msg);
	}
	catch (std::exception& e) 
	{
		SetSummaryText(_("Restore Failed"));
		wxString msg;
		msg.Printf(_("Error: failed to finish restore: %s"),
			wxString(e.what(), wxConvBoxi).c_str());
		ReportFatalError(BM_BACKUP_FAILED_UNKNOWN_ERROR, msg);
	}
	catch (...)
	{
		SetSummaryText(_("Restore Failed"));
		ReportFatalError(BM_BACKUP_FAILED_UNKNOWN_ERROR,
			_("Error: failed to finish restore: unknown error"));
	}	

	SetSummaryText(_("Restore Finished"));
	SetCurrentText(_("Idle (nothing to do)"));
	mRestoreRunning = false;
	mRestoreStopRequested = false;
	SetStopButtonLabel(_("Close"));
}

ServerFileVersion* RestoreProgressPanel::GetVersionToRestore
	(ServerCacheNode* pFile, const RestoreSpec& rSpec)
{
	if (!rSpec.GetRestoreToDateEnabled())
	{
		// easy case first
		return pFile->GetMostRecent();
	}
	
	wxDateTime epoch = rSpec.GetRestoreToDate();
	ServerFileVersion::SafeVector& rVersions = pFile->GetVersions();
	ServerFileVersion* pVersion = NULL;

	for (ServerFileVersion::Iterator i = rVersions.begin();
		i != rVersions.end(); i++)
	{
		if (i->GetDateTime().IsLaterThan(epoch))
		{
			continue;
		}
		
		if (pVersion != NULL && i->GetDateTime().IsEarlierThan(
			pVersion->GetDateTime()))
		{
			continue;
		}
		
		pVersion = &(*i);
	}
	
	return pVersion;
}

void RestoreProgressPanel::CountFilesRecursive
(
	RestoreSpec& rSpec, ServerCacheNode* pRootNode, 
	ServerCacheNode* pCurrentNode, int blockSize
)
{
	// stop if requested
	if (mRestoreStopRequested) return;
		
	ServerFileVersion* pVersion = GetVersionToRestore(pCurrentNode, rSpec);
	if (!pVersion)
	{
		// no version available, cannot restore
		return;
	}
	
	RestoreSpecEntry::Vector specs = rSpec.GetEntries();

	for (RestoreSpecEntry::Iterator i = specs.begin();
		i != specs.end(); i++)
	{
		if (&(i->GetNode()) == pCurrentNode)
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
	
	if (pVersion->IsDeleted())
	{
		// ignore deleted files for now. fixme.
		return;
	}
	
	if (pVersion->IsDirectory())
	{
		wxString message;
		message.Printf(_("Counting files in %s"), 
			pCurrentNode->GetFullPath().c_str());
		SetCurrentText(message);
		wxYield();

		ServerCacheNode::SafeVector* pChildren = 
			pCurrentNode->GetChildren();
		wxASSERT(pChildren);
		
		if (!pChildren)
		{
			return;
		}
		
		for (ServerCacheNode::Iterator i = pChildren->begin();
			i != pChildren->end(); i++)
		{
			CountFilesRecursive(rSpec, pRootNode, &(*i), 
				blockSize);
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
	wxFileName& rLocalName, int blockSize
)
{
	// stop if requested
	if (mRestoreStopRequested) return false;
		
	ServerFileVersion* pVersion = GetVersionToRestore(pNode, rSpec);
	
	RestoreSpecEntry::Vector specs = rSpec.GetEntries();
	for (RestoreSpecEntry::Iterator i = specs.begin();
		i != specs.end(); i++)
	{
		if (&(i->GetNode()) != pNode)
		{
			// no match
			continue;
		}
		
		if (! i->IsInclude())
		{
			// excluded, stop here.
			return true;
		}
		
		// Must be included. If the user explicitly included a file
		// and we can't find it, then we must report an error.
		// Otherwise, silently ignore it (!).

		if (!pVersion)
		{
			// no version available, cannot restore this file
			wxString msg;
			msg.Printf(_("Failed to restore '%s': not found on server"),
				pNode->GetFullPath().c_str());
			mpErrorList->Append(msg);
			return false;
		}
	}
	
	if (!pVersion)
	{
		// no version available, cannot restore this file
		return true;
	}

	// The restore root directory always exists, because we just created it.
	// In the special case of restoring the root, don't complain about it.
	if (!pNode->IsRoot())
	{
		if (rLocalName.FileExists() ||
			wxDirExists(rLocalName.GetFullPath()))
		{
			wxString msg;
			msg.Printf(_("Error: failed to finish restore: "
				"object already exists: '%s'"), 
				rLocalName.GetFullPath().c_str());
			ReportFatalError(
				BM_RESTORE_FAILED_OBJECT_ALREADY_EXISTS, msg);
			return false;
		}
	}
	
	wxCharBuffer namebuf = rLocalName.GetFullPath().mb_str(wxConvBoxi);
		
	if (pVersion->IsDeleted())
	{
		// ignore deleted files for now. fixme.
		return true;
	}
	
	if (pVersion->IsDirectory())
	{
		wxString message;
		message.Printf(_("Reading %s"), pNode->GetFullPath().c_str());
		SetCurrentText(message);

		// And don't try to create the root a second time.
		if (!pNode->IsRoot())
		{
			if (!wxMkdir(rLocalName.GetFullPath()))
			{
				wxString msg;
				msg.Printf(_("Error: failed to finish restore: "
					"cannot create directory: '%s'"), 
					rLocalName.GetFullPath().c_str());
				ReportFatalError(
					BM_RESTORE_FAILED_TO_CREATE_OBJECT, msg);
				return false;
			}

			if (pVersion->HasAttributes())
			{
				pVersion->GetAttributes().WriteAttributes(namebuf.data());
			}
		}
		
		ServerCacheNode::SafeVector* pChildren = pNode->GetChildren();
		wxASSERT(pChildren);
		
		if (!pChildren)
		{
			return false;
		}
		
		for (ServerCacheNode::Iterator i = pChildren->begin();
			i != pChildren->end(); i++)
		{
			wxFileName childName(rLocalName.GetFullPath(),
				i->GetFileName());
			if (!RestoreFilesRecursive(rSpec, &(*i), 
				pVersion->GetBoxFileId(), childName, 
				blockSize))
			{
				return false;
			}
		}
	}
	else
	{		
		wxString message;
		message.Printf(_("Restoring %s"), pNode->GetFullPath().c_str());
		SetCurrentText(message);

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

		NotifyMoreFilesDone(1, pVersion->GetSizeBlocks() * blockSize);		
	}
	
	return true;
}
