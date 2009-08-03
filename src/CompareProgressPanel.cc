/***************************************************************************
 *            CompareProgressPanel.cc
 *
 *  Sun Dec 28 2008
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

#include "BackupQueries.h"
#include "BackupStoreException.h"
#include "TLSContext.h"
#include "BoxBackupCompareParams.h"

#include "main.h"
#include "CompareProgressPanel.h"
#include "ServerConnection.h"

#include "BoxiApp.h"
// #include "ClientConfig.h"
// #include "ClientConnection.h"

//DECLARE_EVENT_TYPE(myEVT_CLIENT_NOTIFY, -1)
//DEFINE_EVENT_TYPE(myEVT_CLIENT_NOTIFY)

class BoxiCompareParamsShim : public BoxBackupCompareParams,
	public ProgressPanel::ExclusionOracle
{
	private:
	CompareProgressPanel* mpProgress;
	
	public:
	BoxiCompareParamsShim(CompareProgressPanel* pProgress,
		bool QuickCompare, bool IgnoreExcludes, bool IgnoreAttributes,
		box_time_t LatestFileUploadTime)
	: BoxBackupCompareParams(QuickCompare, IgnoreExcludes, IgnoreAttributes,
		LatestFileUploadTime),
	  mpProgress(pProgress)
	{ }
	
	/*
	BoxBackupCompareParams interface implementation, forwarding
	events to CompareProgressPanel
	*/
	
	virtual void NotifyLocalDirMissing(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		mpProgress->NotifyLocalDirMissing(rLocalPath, rRemotePath);
	}
	
	virtual void NotifyLocalDirAccessFailed(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		mpProgress->NotifyLocalDirAccessFailed(rLocalPath, rRemotePath);
	}

	virtual void NotifyStoreDirMissingAttributes(
		const std::string& rLocalPath, const std::string& rRemotePath)
	{
		mpProgress->NotifyStoreDirMissingAttributes(rLocalPath,
			rRemotePath);
	}

	virtual void NotifyRemoteFileMissing(const std::string& rLocalPath,
		const std::string& rRemotePath,	bool modifiedAfterLastSync)
	{
		mpProgress->NotifyRemoteFileMissing(rLocalPath, rRemotePath,
			modifiedAfterLastSync);
	}

	virtual void NotifyLocalFileMissing(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		mpProgress->NotifyLocalFileMissing(rLocalPath, rRemotePath);
	}

	virtual void NotifyExcludedFileNotDeleted(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		mpProgress->NotifyExcludedFileNotDeleted(rLocalPath,
			rRemotePath);
	}
	
	virtual void NotifyDownloadFailed(const std::string& rLocalPath,
		const std::string& rRemotePath, int64_t NumBytes,
		BoxException& rException)
	{
		mpProgress->NotifyDownloadFailed(rLocalPath, rRemotePath,
			NumBytes, rException);
	}

	virtual void NotifyDownloadFailed(const std::string& rLocalPath,
		const std::string& rRemotePath, int64_t NumBytes,
		std::exception& rException)
	{
		mpProgress->NotifyDownloadFailed(rLocalPath, rRemotePath,
			NumBytes, rException);
	}

	virtual void NotifyDownloadFailed(const std::string& rLocalPath,
		const std::string& rRemotePath, int64_t NumBytes)
	{
		mpProgress->NotifyDownloadFailed(rLocalPath, rRemotePath,
			NumBytes);
	}

	virtual void NotifyExcludedFile(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{ mpProgress->NotifyExcludedFile(rLocalPath, rRemotePath); }

	virtual void NotifyExcludedDir(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{ mpProgress->NotifyExcludedDir(rLocalPath, rRemotePath); }

	virtual void NotifyDirComparing(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{ mpProgress->NotifyDirComparing(rLocalPath, rRemotePath); }

	virtual void NotifyDirCompared(const std::string& rLocalPath,
		const std::string& rRemotePath,	bool HasDifferentAttributes,
		bool modifiedAfterLastSync)
	{
		mpProgress->NotifyDirCompared(rLocalPath, rRemotePath,
			HasDifferentAttributes, modifiedAfterLastSync);
	}
	
	virtual void NotifyFileComparing(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{ mpProgress->NotifyFileComparing(rLocalPath, rRemotePath); }
	
	virtual void NotifyFileCompared(const std::string& rLocalPath,
		const std::string& rRemotePath, int64_t NumBytes,
		bool HasDifferentAttributes, bool HasDifferentContents,
		bool modifiedAfterLastSync, bool newAttributesApplied)
	{
		mpProgress->NotifyFileCompared(rLocalPath, rRemotePath,
			NumBytes, HasDifferentAttributes, HasDifferentContents,
			modifiedAfterLastSync, newAttributesApplied);
	}

	/* ProgressPanel::ExclusionOracle interface implementation */
	
	virtual bool IsExcludedFile(const std::string& rFileName)
	{
		return BoxBackupCompareParams::IsExcludedFile(rFileName);
	}
	
	virtual bool IsExcludedDir(const std::string& rDirName)
	{
		return BoxBackupCompareParams::IsExcludedDir(rDirName);
	}
};

BEGIN_EVENT_TABLE(CompareProgressPanel, wxPanel)
	EVT_BUTTON(wxID_CANCEL, CompareProgressPanel::OnStopCloseClicked)
END_EVENT_TABLE()

CompareProgressPanel::CompareProgressPanel
(
	ClientConfig*     pConfig,
	ServerConnection* pConnection,
	wxWindow*         pParent
)
: ProgressPanel(pParent, ID_Compare_Progress_Panel, _("Compare Progress Panel")),
  mpConfig(pConfig),
  mpConnection(pConnection),
  mCompareRunning(false),
  mCompareStopRequested(false)
{
}

wxFileName MakeLocalPath(wxFileName& base, ServerCacheNode* pTargetNode);

void CompareProgressPanel::StartCompare(const BoxiCompareParams& rParams)
{
	LogToListBox logTo(mpErrorList);
	mpErrorList->Clear();

	ResetCounters();
	
	mCompareRunning = true;
	mCompareStopRequested = false;
	SetSummaryText(_("Starting Compare"));
	SetStopButtonLabel(_("Stop Compare"));

	Layout();
	wxYield();
	
	try 
	{
		SetCurrentText(_("Connecting to server"));
		
		Configuration BoxConfig(mpConfig->GetBoxConfig());
		BackupProtocolClient* pClient =
			mpConnection->GetProtocolClient(false);
		if (!pClient)
		{
			THROW_EXCEPTION(ConnectionException, 
				SocketConnectError);
		}

		SetSummaryText(_("Counting Files to Compare"));
		SetCurrentText(_(""));
			
		BackupQueries queries(*pClient,	BoxConfig, false);
		
		BoxiCompareParamsShim BBParams(this, false, false, false,
			GetCurrentBoxTime() /* FIXME last backup time */);
		const Configuration& rLocations(
			BoxConfig.GetSubConfiguration("BackupLocations"));

		std::vector<std::string> locNames =
			rLocations.GetSubConfigurationNames();

		// Go through the records, counting files and bytes
		for(std::vector<std::string>::iterator
			pLocName  = locNames.begin();
			pLocName != locNames.end();
			pLocName++)
		{
			const Configuration& rLocation(
				rLocations.GetSubConfiguration(*pLocName));
			BBParams.LoadExcludeLists(rLocation);
			CountLocalFiles(BBParams, rLocation.GetKeyValue("Path"));
		}
		
		mpProgressGauge->SetRange(GetNumFilesTotal());
		mpProgressGauge->SetValue(0);
		mpProgressGauge->Show();

		SetSummaryText(_("Comparing files"));
		wxYield();

		for(std::vector<std::string>::iterator
			pLocName  = locNames.begin();
			pLocName != locNames.end();
			pLocName++)
		{
			queries.CompareLocation(*pLocName, BBParams);
		}

		SetSummaryText(_("Compare Finished"));
		mpErrorList->Append(_("Compare Finished"));

		mpProgressGauge->Hide();
	}
	catch (ConnectionException& e) 
	{
		SetSummaryText(_("Compare Failed"));
		wxString msg;
		msg.Printf(_("Error: cannot start compare: "
			"Failed to connect to server: %s"),
			wxString(e.what(), wxConvBoxi).c_str());
		ReportFatalError(BM_BACKUP_FAILED_CONNECT_FAILED, msg);
	}
	catch (std::exception& e) 
	{
		SetSummaryText(_("Compare Failed"));
		wxString msg;
		msg.Printf(_("Error: failed to finish compare: %s"),
			wxString(e.what(), wxConvBoxi).c_str());
		ReportFatalError(BM_BACKUP_FAILED_UNKNOWN_ERROR, msg);
	}
	catch (...)
	{
		SetSummaryText(_("Compare Failed"));
		ReportFatalError(BM_BACKUP_FAILED_UNKNOWN_ERROR,
			wxT("Error: failed to finish compare: unknown error"));
	}	
	
	SetSummaryText(_("Idle (nothing to do)"));
	mCompareRunning = false;
	mCompareStopRequested = false;
	SetStopButtonLabel(_("Close"));
}
