/***************************************************************************
 *            ComparePanel.h
 *
 *  Tue Mar  1 00:24:11 2005
 *  Copyright 2005-2006 Chris Wilson
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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#ifndef _COMPAREPANEL_H
#define _COMPAREPANEL_H

#include <wx/filename.h>

#include "FunctionPanel.h"

class CompareFilesPanel;
class CompareProgressPanel;
class DirSelButton;
class ServerCacheNode;
class ServerConnection;

class wxChoice;
class wxFileName;
class wxNotebook;
class wxRadioButton;
class wxTextCtrl;

class CompareSpecEntry
{
	public:
	typedef std::list<CompareSpecEntry> List;
	typedef List::iterator Iterator;
	typedef List::const_iterator ConstIterator;
		
	private:
	wxFileName       mLocalFile;
	ServerCacheNode* mpRemoteFile;
	bool             mInclude;

	public:
	CompareSpecEntry(const wxFileName& rLocalFile, 
		ServerCacheNode* pRemoteFile, bool lInclude)
	: mLocalFile(rLocalFile),
	  mpRemoteFile(pRemoteFile),
	  mInclude(lInclude)
	{ }

	wxFileName       GetLocalFile()  const { return mLocalFile; }
	ServerCacheNode& GetRemoteFile() const { return *mpRemoteFile; }
	bool IsInclude() const { return mInclude; }
	bool IsSameAs(const CompareSpecEntry& rOther) const
	{
		return mLocalFile.SameAs(rOther.mLocalFile) && 
			mpRemoteFile == rOther.mpRemoteFile &&
			mInclude == rOther.mInclude;
	}
};

class CompareSpec
{
	private:
	CompareSpecEntry::List mEntries;
	bool mCompareWithOriginalLocations;
	wxFileName mCompareWithNewLocation;
	
	public:
	CompareSpec() { }
	const CompareSpecEntry::List& GetEntries() const { return mEntries; }
	void Add   (const CompareSpecEntry& rNewEntry);
	void Remove(const CompareSpecEntry& rOldEntry);
	
	void SetCompareWithOriginalLocations(bool newValue) 
	{ mCompareWithOriginalLocations = newValue; }
	void SetCompareWithNewLocation(const wxFileName& rNewValue)
	{ mCompareWithNewLocation = rNewValue; }
	
	bool       GetCompareWithOriginalLocations() const 
	{ return mCompareWithOriginalLocations; }
	wxFileName GetCompareWithNewLocation() const 
	{ return mCompareWithNewLocation; }
};

class CompareSpecChangeListener
{
	public:
	virtual void OnCompareSpecChange() = 0;
	virtual ~CompareSpecChangeListener() { }
};

class ComparePanel : public wxPanel, public ConfigChangeListener
{
	public:
	ComparePanel
	(
		ClientConfig*     pConfig,
		MainFrame*        pMainFrame,
		ServerConnection* pServerConnection,
		wxWindow*         pParent
	);
	
	void AddToNotebook(wxNotebook* pNotebook);
	
	private:	
	ServerConnection*     mpServerConnection;
	ClientConfig*         mpConfig;
	MainFrame*            mpMainFrame;
	
	wxRadioButton* mpAllLocsRadio;
	wxRadioButton* mpOneLocRadio;
	wxRadioButton* mpDirRadio;
	
	wxChoice* mpOneLocChoice;
	
	wxTextCtrl* mpDirLocalText;
	wxTextCtrl* mpDirRemoteText;
	wxButton*   mpDirLocalButton;
	wxButton*   mpDirRemoteButton;
	
	/*
	CompareProgressPanel* mpProgressPanel;
	CompareFilesPanel*    mpFilesPanel;
	wxRadioButton* mpOldLocRadio;
	wxRadioButton* mpNewLocRadio;
	wxTextCtrl*    mpNewLocText;
	DirSelButton*  mpNewLocButton;
	*/

	void Update();
	void OnClickStartButton(wxCommandEvent& rEvent);
	void OnClickCloseButton(wxCommandEvent& rEvent);
	void OnClickRadioButton(wxCommandEvent& rEvent);
	void UpdateEnabledState();

	virtual void NotifyChange(); // ConfigChangeListener
		
	DECLARE_EVENT_TABLE()
};

#endif /* _COMPAREPANEL_H */
