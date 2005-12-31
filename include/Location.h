/***************************************************************************
 *            Location.h
 *
 *  Mon Feb 28 23:38:27 2005
 *  Copyright  2005  Chris Wilson
 *  anjuta@qwirx.com
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
 
#ifndef _LOCATION_H
#define _LOCATION_H

#include <regex.h>

#include <wx/wx.h>

#define NDEBUG
#include <BoxConfig.h>
#define EXCLUDELIST_IMPLEMENTATION_REGEX_T_DEFINED
#include "Configuration.h"
#include "ExcludeList.h"
#undef EXCLUDELIST_IMPLEMENTATION_REGEX_T_DEFINED
#undef NDEBUG

enum ExcludeSense {
	ES_UNKNOWN = 0,
	ES_EXCLUDE,
	ES_ALWAYSINCLUDE,
};

enum ExcludeFileDir {
	EFD_UNKNOWN = 0,
	EFD_FILE,
	EFD_DIR,
};

enum ExcludeMatch {
	EM_UNKNOWN = 0,
	EM_EXACT,
	EM_REGEX,
};

enum ExcludedState {
	EST_UNKNOWN = 0,
	EST_NOLOC,
	EST_INCLUDED,
	EST_EXCLUDED,
	EST_ALWAYSINCLUDED,
};

class MyExcludeType {
	private:

	public:
	MyExcludeType(ExcludeSense sense, ExcludeFileDir fileDir, 
		ExcludeMatch match) 
	: mSense(sense),
	  mFileOrDir(fileDir),
	  mMatch(match)
	{ }

	MyExcludeType(const MyExcludeType& rToCopy)
	: mSense(rToCopy.mSense),
	  mFileOrDir(rToCopy.mFileOrDir),
	  mMatch(rToCopy.mMatch)
	{ }
	
	ExcludeSense GetSense() const { return mSense; }
	const std::string GetSenseString() const {
		std::string temp;
		switch (mSense) {
		case ES_EXCLUDE: 		temp = "Exclude"; 		break;
		case ES_ALWAYSINCLUDE: 	temp = "AlwaysInclude"; break;
		default: 				temp = "Unknown"; 		break;
		}
		return temp;
	}

	ExcludeFileDir GetFileDir() const { return mFileOrDir; }
	const std::string GetFileDirString() const
	{
		std::string temp;
		switch (mFileOrDir) {
		case EFD_FILE: 	temp = "File"; 		break;
		case EFD_DIR: 	temp = "Dir";		break;
		default: 		temp = "Unknown"; 	break;
		}
		return temp;
	}
	
	ExcludeMatch GetMatch() const { return mMatch; }
	const std::string GetMatchString() const
	{
		std::string temp;
		switch (mMatch) {
		case EM_EXACT: 	temp = ""; 			break;
		case EM_REGEX: 	temp = "sRegex";	break;
		default: 		temp = "Unknown"; 	break;
		}
		return temp;
	}
	
	public:
	std::string ToString() const {
		std::string buffer = "";
		buffer.append(GetSenseString());
		buffer.append(GetFileDirString());
		buffer.append(GetMatchString());
		return buffer;
	}
	
	private:
	ExcludeSense	mSense;
	ExcludeFileDir	mFileOrDir;
	ExcludeMatch	mMatch;
};

#define numExcludeTypes 8
extern MyExcludeType theExcludeTypes [];

enum ExcludeTypeIndex {
	ETI_EXCLUDE_DIR = 0,
	ETI_EXCLUDE_DIRS_REGEX,
	ETI_EXCLUDE_FILE,
	ETI_EXCLUDE_FILES_REGEX,
	ETI_ALWAYS_INCLUDE_DIR,
	ETI_ALWAYS_INCLUDE_DIRS_REGEX,
	ETI_ALWAYS_INCLUDE_FILE,
	ETI_ALWAYS_INCLUDE_FILES_REGEX,
};

class Location;

class MyExcludeEntry 
{
	private:
	MyExcludeType* mpType;
	std::string mValue;

	public:
	MyExcludeEntry(MyExcludeType& rType, const std::string& rValue) 
	: mpType(&rType),
	  mValue(rValue)
	{ }

	MyExcludeEntry(MyExcludeType& rType, const wxString& rValue) 
	: mpType(&rType),
	  mValue(wxCharBuffer(rValue.mb_str(wxConvLibc)).data())
	{ }

	MyExcludeEntry(const MyExcludeEntry& rToCopy)
	: mpType(rToCopy.mpType),
	  mValue(rToCopy.mValue)
	{ }
	
	ExcludeSense   GetSense()   const { return mpType->GetSense(); }
	ExcludeFileDir GetFileDir() const { return mpType->GetFileDir(); }
	ExcludeMatch   GetMatch()   const { return mpType->GetMatch(); }

	const std::string GetSenseString()   const { return mpType->GetSenseString(); }
	const std::string GetFileDirString() const { return mpType->GetFileDirString(); }
	const std::string GetMatchString()   const { return mpType->GetMatchString(); }
	
	MyExcludeType*       GetType()  const { return mpType; }
	const std::string&   GetValue() const { return mValue; }
	
	wxString GetValueString() const 
	{ 
		return wxString(mValue.c_str(), wxConvLibc); 
	}
	
	const std::string ToStringType() const { return mpType->ToString(); }

	std::string ToString() const
	{
		std::string buffer;
		buffer.append(ToStringType());
		buffer.append(" = ");
		buffer.append(mValue);
		return buffer;
	}

	bool IsSameAs(const MyExcludeEntry& other) const
	{
		if (other.GetSense()   != GetSense())      return FALSE;
		if (other.GetFileDir() != GetFileDir())    return FALSE;
		if (other.GetMatch()   != GetMatch())      return FALSE;
		if (other.GetValue().compare(mValue) != 0) return FALSE;
		return TRUE;
	}	
};

class MyExcludeList;

class LocationChangeListener 
{
	public:
	virtual void OnLocationChange   (Location*      pLocation)    = 0;
	virtual void OnExcludeListChange(MyExcludeList* pExcludeList) = 0;
	virtual ~LocationChangeListener() {}
};

class MyExcludeList 
{
	private:
	std::vector<MyExcludeEntry> mEntries;
	LocationChangeListener* mpListener;

	public:
	MyExcludeList(LocationChangeListener* pListener) 
	: mpListener(pListener) { };
	
	MyExcludeList(const Configuration& conf, LocationChangeListener* pListener);
	
	MyExcludeList(const MyExcludeList& rToCopy)
	: mpListener(rToCopy.mpListener)
	{
		for (size_t i = 0; i < rToCopy.mEntries.size(); i++) 
		{
			MyExcludeEntry e(rToCopy.mEntries[i]);
			mEntries.push_back(e);
		}
	}
	void CopyFrom(const MyExcludeList& rToCopy)
	{
		mEntries.clear();
		for (size_t i = 0; i < rToCopy.mEntries.size(); i++) 
		{
			MyExcludeEntry e(rToCopy.mEntries[i]);
			mEntries.push_back(e);
		}
		if (mpListener)
		{
			mpListener->OnExcludeListChange(this);
		}
	}
	
	const std::vector<MyExcludeEntry>& GetEntries() const { return mEntries; }
	void AddEntry(const MyExcludeEntry& rNewEntry);
	void ReplaceEntry(const MyExcludeEntry& rOldEntry, const MyExcludeEntry& rNewEntry);
	// void RemoveEntry(int index);
	void RemoveEntry(const MyExcludeEntry& rOldEntry);
	MyExcludeEntry* UnConstEntry(const MyExcludeEntry& rEntry);
	bool IsSameAs(const MyExcludeList& rOther) const
	{
		const std::vector<MyExcludeEntry>& rOtherEntries = rOther.GetEntries();
		if (rOtherEntries.size() != mEntries.size()) return FALSE;
		
		for (size_t i = 0; i < mEntries.size(); i++) 
		{
			if (! mEntries[i].IsSameAs(rOtherEntries[i]))
			{
				return FALSE;
			}
		}
		
		return TRUE;
	}
	
	void addConfigList(const Configuration& conf, const std::string& keyName, 
		MyExcludeType& type);
	void addSeparatedList(const std::string& entries, MyExcludeType& type);
	void SetListener(LocationChangeListener* pListener)
	{ mpListener = pListener; }
};

class Location
{
	private:
	wxString mName;
	wxString mPath;
	MyExcludeList mExcluded;
	LocationChangeListener* mpListener;
	
	public:
	Location(const wxString& rName, const wxString& rPath, 
		LocationChangeListener* pListener)
	: mName(rName),
	  mPath(rPath),
	  mExcluded(pListener),
	  mpListener(pListener)
	{ }
	
	Location(const Location& rToCopy) 
	: mName(rToCopy.mName),
	  mPath(rToCopy.mPath),
	  mExcluded(rToCopy.mExcluded),
	  mpListener(rToCopy.mpListener)
	{ }
	
	const wxString& GetName() const { return mName; }
	const wxString& GetPath() const { return mPath; }
	MyExcludeList& GetExcludeList() { return mExcluded; }
	const MyExcludeList& GetExcludeList() const { return mExcluded; }
	void SetExcludeList(const MyExcludeList& rSourceList) 
	{ 
		mExcluded.CopyFrom(rSourceList);
		if (mpListener)
		{
			mpListener->OnLocationChange(this);
		}
	}
	void SetName(const wxString& rName) 
	{
		this->mName = rName;
		if (mpListener)
		{
			mpListener->OnLocationChange(this);
		}
	}
	void SetPath(const wxString& rPath) 
	{
		this->mPath = rPath;
		if (mpListener)
		{
			mpListener->OnLocationChange(this);
		}
	}

	bool IsSameAs(const Location& rOther) const
	{
		if (! rOther.mName.IsSameAs(mName) ) return false;
		if (! rOther.mPath.IsSameAs(mPath) ) return false;
		return mExcluded.IsSameAs(rOther.mExcluded);
	}
	
	bool IsExcluded(const wxString& rLocalFileName, bool mIsDirectory, 
		const MyExcludeEntry** ppExcludedBy, 
		const MyExcludeEntry** ppIncludedBy);
	ExcludedState GetExcludedState(const wxString& rLocalFileName, 
		bool mIsDirectory, 
		const MyExcludeEntry** ppExcludedBy, 
		const MyExcludeEntry** ppIncludedBy,
		ExcludedState ParentState = EST_UNKNOWN);
	
	ExcludeList* GetBoxExcludeList(bool listDirs) const;
	
	void SetListener(LocationChangeListener* pListener)
	{ mpListener = pListener; mExcluded.SetListener(pListener); }

};

#endif /* _LOCATION_H */
