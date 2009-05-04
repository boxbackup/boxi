/***************************************************************************
 *            CompareProgressPanel.h
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
 */
 
#ifndef _COMPARE_PROGRESS_PANEL_H
#define _COMPARE_PROGRESS_PANEL_H

#include <wx/wx.h>
#include <wx/thread.h>

#include "BoxBackupCompareParams.h"

#include "ProgressPanel.h"

class wxFileName;

class ClientConfig;
class ServerConnection;
class ServerCacheNode;
class BoxiCompareParams;

class CompareProgressPanel : public ProgressPanel
{
	public:
	CompareProgressPanel
	(
		ClientConfig*     pConfig,
		ServerConnection* pConnection,
		wxWindow*         pParent
	);

	void StartCompare(const BoxiCompareParams& rParams);

	private:
	ClientConfig*     mpConfig;
	ServerConnection* mpConnection;
	
	bool mCompareRunning;
	bool mCompareStopRequested;

	virtual bool IsStopRequested() { return mCompareStopRequested; }

	/*
	void CountDirectory(const BoxiCompareParams& rParams,
		const std::string &rLocalPath);
	*/
	
	virtual void OnStopCloseClicked(wxCommandEvent& event) 
	{ 
		if (mCompareRunning)
		{
			mCompareStopRequested = TRUE;
		}
		else
		{
			Hide();
		}
	}
	
	void CountFilesRecursive(const BoxiCompareParams& rParams, 
		ServerCacheNode* pRootNode, 
		ServerCacheNode* pCurrentNode, int blockSize);
	bool CompareFilesRecursive(const BoxiCompareParams& rParams,
		ServerCacheNode* pNode, int64_t parentId, 
		wxFileName& rLocalName, int blockSize);
	// wxFileName MakeLocalPath(wxFileName& rBase, ServerCacheNode* pNode);

	/*
	friend class TestRestore;
	int GetConnectionIndex() { return mpConnection->GetConnectionIndex(); }
	*/
	
	void NotifyComparing(const wxString& rLocalPath, bool IsFile)
	{
		wxString msg;
		msg.Printf(wxT("Comparing %s '%s'"),
			IsFile ? _("file") : _("directory"), rLocalPath.c_str());
		SetCurrentText(msg);
		Layout();
		wxYield();
	}
	
	public:

	virtual void NotifyLocalDirMissing(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		wxString msg;
		msg.Printf(_("Local directory '%s' does not exist, "
			"but remote directory does."),
			wxString(rLocalPath.c_str(), wxConvLibc).c_str());
		mpErrorList->Append(msg);
		// mDifferences ++;
	}
	
	virtual void NotifyLocalDirAccessFailed(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		wxString msg;
		msg.Printf(_("Failed to access local directory '%s': %s"),
			wxString(rLocalPath.c_str(), wxConvLibc).c_str(),
			wxString(GetNativeErrorMessage().c_str(),
				wxConvLibc).c_str());
		mpErrorList->Append(msg);
		// mUncheckedFiles ++;
	}

	virtual void NotifyStoreDirMissingAttributes(
		const std::string& rLocalPath, const std::string& rRemotePath)
	{
		wxString msg;
		msg.Printf(_("Store directory '%s' doesn't have attributes."),
			wxString(rRemotePath.c_str(), wxConvLibc).c_str());
		mpErrorList->Append(msg);
	}

	virtual void NotifyRemoteFileMissing(const std::string& rLocalPath,
		const std::string& rRemotePath,	bool modifiedAfterLastSync)
	{
		wxString msg;
		msg.Printf(_("Local file '%s' exists, but remote file '%s' "
			"does not."),
			wxString(rLocalPath.c_str(), wxConvLibc).c_str(),
			wxString(rRemotePath.c_str(), wxConvLibc).c_str());
		
		// mDifferences ++;
		
		if(modifiedAfterLastSync)
		{
			// mDifferencesExplainedByModTime ++;
			msg += _(" (modified since the last backup)");
		}

		mpErrorList->Append(msg);
	}

	virtual void NotifyLocalFileMissing(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		wxString msg;
		msg.Printf(_("Remote file '%s' exists, but local file '%s' "
			"does not."),
			wxString(rRemotePath.c_str(), wxConvLibc).c_str(),
			wxString(rLocalPath.c_str(), wxConvLibc).c_str());
		mpErrorList->Append(msg);
		// mDifferences ++;
	}

	virtual void NotifyExcludedFileNotDeleted(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		wxString msg;
		msg.Printf(_("Local file '%s' is excluded, but remote file "
			"'%s' still exists."),
			wxString(rLocalPath.c_str(), wxConvLibc).c_str(),
			wxString(rRemotePath.c_str(), wxConvLibc).c_str());
		mpErrorList->Append(msg);
		// mDifferences ++;
	}
	
	virtual void NotifyDownloadFailed(const std::string& rLocalPath,
		const std::string& rRemotePath, int64_t NumBytes,
		BoxException& rException)
	{
		wxString msg;
		msg.Printf(_("Failed to download remote file '%s': "
			"%s (%d/%d)"),
			wxString(rRemotePath.c_str(), wxConvLibc).c_str(),
			wxString(rException.what(), wxConvLibc).c_str(),
			rException.GetType(), rException.GetSubType());
		mpErrorList->Append(msg);
		// mUncheckedFiles ++;
	}

	virtual void NotifyDownloadFailed(const std::string& rLocalPath,
		const std::string& rRemotePath, int64_t NumBytes,
		std::exception& rException)
	{
		wxString msg;
		msg.Printf(_("Failed to download remote file '%s': %s"),
			wxString(rRemotePath.c_str(), wxConvLibc).c_str(),
			wxString(rException.what(), wxConvLibc).c_str());
		mpErrorList->Append(msg);
		// mUncheckedFiles ++;
	}

	virtual void NotifyDownloadFailed(const std::string& rLocalPath,
		const std::string& rRemotePath, int64_t NumBytes)
	{
		wxString msg;
		msg.Printf(_("Failed to download remote file '%s'"),
			wxString(rRemotePath.c_str(), wxConvLibc).c_str());
		mpErrorList->Append(msg);
		// mUncheckedFiles ++;
	}

	virtual void NotifyExcludedFile(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		// mExcludedFiles ++;
	}

	virtual void NotifyExcludedDir(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		// mExcludedDirs ++;
	}

	virtual void NotifyDirComparing(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		NotifyComparing(wxString(rLocalPath.c_str(), wxConvLibc), false);
	}
	
	virtual void NotifyDirCompared(const std::string& rLocalPath,
		const std::string& rRemotePath,	bool HasDifferentAttributes,
		bool modifiedAfterLastSync)
	{
		if (HasDifferentAttributes)
		{
			wxString msg;
			
			msg.Printf(_("Local directory '%s' has different "
				"attributes to store directory '%s'."),
				wxString(rLocalPath.c_str(), wxConvLibc).c_str(),
				wxString(rRemotePath.c_str(), wxConvLibc).c_str());
			// mDifferences ++;
			
			if(modifiedAfterLastSync)
			{
				msg += _(" (modified since the last backup)");
				// mDifferencesExplainedByModTime ++;
			}
			
			mpErrorList->Append(msg);
		}
	}
	
	virtual void NotifyFileComparing(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		NotifyComparing(wxString(rLocalPath.c_str(), wxConvLibc), true);
	}
	
	virtual void NotifyFileCompared(const std::string& rLocalPath,
		const std::string& rRemotePath, int64_t NumBytes,
		bool HasDifferentAttributes, bool HasDifferentContents,
		bool modifiedAfterLastSync, bool newAttributesApplied)
	{
		if (HasDifferentAttributes)
		{
			wxString msg;
			
			msg.Printf(_("Local file '%s' has different attributes "
				"to store file '%s'."),
				wxString(rLocalPath.c_str(), wxConvLibc).c_str(),
				wxString(rRemotePath.c_str(), wxConvLibc).c_str());
			// mDifferences ++;
			
			if(modifiedAfterLastSync)
			{
				msg += _(" (modified since the last backup)");
				// mDifferencesExplainedByModTime ++;
			}
			else if(newAttributesApplied)
			{
				msg += _(" (new attributes applied)");
			}
			
			mpErrorList->Append(msg);
		}

		if (HasDifferentContents)
		{
			wxString msg;
			
			msg.Printf(_("Local file '%s' has different contents "
				"to store file '%s'."),
				wxString(rLocalPath.c_str(), wxConvLibc).c_str(),
				wxString(rRemotePath.c_str(), wxConvLibc).c_str());
			// mDifferences ++;
			
			if(modifiedAfterLastSync)
			{
				// mDifferencesExplainedByModTime ++;
				msg += _(" (modified since the last backup)");
			}

			mpErrorList->Append(msg);
		}
		
		NotifyMoreFilesDone(1, NumBytes);
	}

	virtual void NotifyDirComparing(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		wxString message;
		message.Printf(_("Comparing %s"), 
			wxString(rLocalPath.c_str(), wxConvLibc).c_str());
		SetCurrentText(message);
	}

	virtual void NotifyFileComparing(const std::string& rLocalPath,
		const std::string& rRemotePath)
	{
		wxString message;
		message.Printf(_("Comparing %s"), 
			wxString(rLocalPath.c_str(), wxConvLibc).c_str());
		SetCurrentText(message);
	}
		
	DECLARE_EVENT_TABLE()
};

#endif /* _COMPARE_PROGRESS_PANEL_H */
