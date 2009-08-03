/***************************************************************************
 *            Location.h
 *
 *  Mon Feb 28 23:38:27 2005
 *  Copyright 2005-2008 Chris Wilson
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
 
#ifndef _LOCATION_H
#define _LOCATION_H

#include "BoxConfig.h"
#include "Configuration.h"
#include "ExcludeList.h"

#include "main.h"

enum ExcludeSense 
{
	ES_UNKNOWN = 0,
	ES_EXCLUDE,
	ES_ALWAYSINCLUDE,
};

enum ExcludeFileDir 
{
	EFD_UNKNOWN = 0,
	EFD_FILE,
	EFD_DIR,
};

enum ExcludeMatch 
{
	EM_UNKNOWN = 0,
	EM_EXACT,
	EM_REGEX,
};

enum ExcludedState 
{
	EST_UNKNOWN = 0,
	EST_NOLOC,
	EST_INCLUDED,
	EST_EXCLUDED,
	EST_ALWAYSINCLUDED,
};

class BoxiExcludeType 
{
	private:

	public:
	BoxiExcludeType(ExcludeSense sense, ExcludeFileDir fileDir, 
		ExcludeMatch match) 
	: mSense(sense),
	  mFileOrDir(fileDir),
	  mMatch(match)
	{ }

	BoxiExcludeType(const BoxiExcludeType& rToCopy)
	: mSense(rToCopy.mSense),
	  mFileOrDir(rToCopy.mFileOrDir),
	  mMatch(rToCopy.mMatch)
	{ }
	
	ExcludeSense GetSense() const { return mSense; }
	const std::string GetSenseString() const {
		std::string temp;
		switch (mSense) {
		case ES_EXCLUDE:       temp = "Exclude";       break;
		case ES_ALWAYSINCLUDE: temp = "AlwaysInclude"; break;
		default:               temp = "Unknown";       break;
		}
		return temp;
	}

	ExcludeFileDir GetFileDir() const { return mFileOrDir; }
	const std::string GetFileDirString() const
	{
		std::string temp;
		switch (mFileOrDir) 
		{
			case EFD_FILE:  temp = "File";    break;
			case EFD_DIR:   temp = "Dir";     break;
			default:        temp = "Unknown"; break;
		}
		return temp;
	}
	
	ExcludeMatch GetMatch() const { return mMatch; }
	const std::string GetMatchString() const
	{
		std::string temp;
		switch (mMatch) 
		{
			case EM_EXACT: temp = "";        break;
			case EM_REGEX: temp = "sRegex";	 break;
			default:       temp = "Unknown"; break;
		}
		return temp;
	}
	
	public:
	std::string ToString() const 
	{
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
extern BoxiExcludeType theExcludeTypes [];

enum ExcludeTypeIndex 
{
	ETI_EXCLUDE_DIR = 0,
	ETI_EXCLUDE_DIRS_REGEX,
	ETI_EXCLUDE_FILE,
	ETI_EXCLUDE_FILES_REGEX,
	ETI_ALWAYS_INCLUDE_DIR,
	ETI_ALWAYS_INCLUDE_DIRS_REGEX,
	ETI_ALWAYS_INCLUDE_FILE,
	ETI_ALWAYS_INCLUDE_FILES_REGEX,
};

#define ET_EXCLUDE_DIR                theExcludeTypes[ETI_EXCLUDE_DIR]
#define ET_EXCLUDE_DIRS_REGEX         theExcludeTypes[ETI_EXCLUDE_DIRS_REGEX]
#define ET_EXCLUDE_FILE               theExcludeTypes[ETI_EXCLUDE_FILE]
#define ET_EXCLUDE_FILES_REGEX        theExcludeTypes[ETI_EXCLUDE_FILES_REGEX]
#define ET_ALWAYS_INCLUDE_DIR         theExcludeTypes[ETI_ALWAYS_INCLUDE_DIR]
#define ET_ALWAYS_INCLUDE_DIRS_REGEX  theExcludeTypes[ETI_ALWAYS_INCLUDE_DIRS_REGEX]
#define ET_ALWAYS_INCLUDE_FILE        theExcludeTypes[ETI_ALWAYS_INCLUDE_FILE]
#define ET_ALWAYS_INCLUDE_FILES_REGEX theExcludeTypes[ETI_ALWAYS_INCLUDE_FILES_REGEX]

class BoxiLocation;

class BoxiExcludeEntry 
{
	public:
	typedef std::list<BoxiExcludeEntry> List;
	typedef List::iterator            Iterator;
	typedef List::const_iterator      ConstIterator;

	private:
	BoxiExcludeType* mpType;
	std::string mValue;

	public:
	BoxiExcludeEntry(BoxiExcludeType& rType, const std::string& rValue) 
	: mpType(&rType),
	  mValue(rValue)
	{ }

	BoxiExcludeEntry(BoxiExcludeType& rType, const wxString& rValue) 
	: mpType(&rType),
	  mValue(wxCharBuffer(rValue.mb_str(wxConvBoxi)).data())
	{ }

	BoxiExcludeEntry(const BoxiExcludeEntry& rToCopy)
	: mpType(rToCopy.mpType),
	  mValue(rToCopy.mValue)
	{ }
	
	ExcludeSense   GetSense()   const { return mpType->GetSense(); }
	ExcludeFileDir GetFileDir() const { return mpType->GetFileDir(); }
	ExcludeMatch   GetMatch()   const { return mpType->GetMatch(); }

	const std::string GetSenseString()   const { return mpType->GetSenseString(); }
	const std::string GetFileDirString() const { return mpType->GetFileDirString(); }
	const std::string GetMatchString()   const { return mpType->GetMatchString(); }
	
	BoxiExcludeType*   GetType()  const { return mpType; }
	const std::string& GetValue() const { return mValue; }
	
	wxString GetValueString() const 
	{ 
		return wxString(mValue.c_str(), wxConvBoxi); 
	}
	
	const std::string GetTypeName() const { return mpType->ToString(); }

	std::string ToString() const
	{
		std::string buffer;
		buffer.append(GetTypeName());
		buffer.append(" = ");
		buffer.append(mValue);
		return buffer;
	}

	bool IsSameAs(const BoxiExcludeEntry& other) const
	{
		if (other.GetSense()   != GetSense())      return FALSE;
		if (other.GetFileDir() != GetFileDir())    return FALSE;
		if (other.GetMatch()   != GetMatch())      return FALSE;
		if (other.GetValue().compare(mValue) != 0) return FALSE;
		return TRUE;
	}	
};

class BoxiExcludeList;

class LocationChangeListener 
{
	public:
	virtual void OnLocationChange   (BoxiLocation*    pLocation)    = 0;
	virtual void OnExcludeListChange(BoxiExcludeList* pExcludeList) = 0;
	virtual ~LocationChangeListener() {}
};

class BoxiExcludeList 
{
	private:
	BoxiExcludeEntry::List mEntries;
	LocationChangeListener* mpListener;

	public:
	BoxiExcludeList(LocationChangeListener* pListener) 
	: mpListener(pListener) { };
	
	BoxiExcludeList(const Configuration& conf,
		LocationChangeListener* pListener);
	
	BoxiExcludeList(const BoxiExcludeList& rToCopy)
	: mpListener(rToCopy.mpListener)
	{
		mEntries = rToCopy.mEntries;
	}
	void CopyFrom(const BoxiExcludeList& rToCopy)
	{
		mEntries = rToCopy.mEntries;

		if (mpListener)
		{
			mpListener->OnExcludeListChange(this);
		}
	}
	
	const BoxiExcludeEntry::List& GetEntries() const { return mEntries; }
	void AddEntry(const BoxiExcludeEntry& rNewEntry);
	void InsertEntry(int index, const BoxiExcludeEntry& rNewEntry);
	void ReplaceEntry(const BoxiExcludeEntry& rOldEntry, 
		const BoxiExcludeEntry& rNewEntry);
	// void RemoveEntry(int index);
	void RemoveEntry(const BoxiExcludeEntry& rOldEntry);
	BoxiExcludeEntry* UnConstEntry(const BoxiExcludeEntry& rEntry);
	bool Contains(const BoxiExcludeEntry& rEntry)
	{
		return (UnConstEntry(rEntry) != NULL);
	}
	bool IsSameAs(const BoxiExcludeList& rOther) const
	{
		const BoxiExcludeEntry::List& rOtherEntries = 
			rOther.GetEntries();

		BoxiExcludeEntry::ConstIterator myPos, otherPos;

		for 
		(
			myPos     = mEntries.begin(), 
			otherPos  = rOtherEntries.begin();
			myPos    != mEntries.end() &&
			otherPos != rOtherEntries.end();
			myPos++, otherPos++
		)
		{
			if (!(myPos->IsSameAs(*otherPos)))
			{
				return false;
			}
		}

		if (myPos != mEntries.end() || otherPos != rOtherEntries.end())
		{
			return false;
		}
		
		return true;
	}
	
	void SetListener(LocationChangeListener* pListener)
	{ mpListener = pListener; }
	
	private:
	void _AddConfigList(const Configuration& conf, const std::string& keyName,
		BoxiExcludeType& rType);
	void _AddSeparatedList(const std::string& entries, BoxiExcludeType& rType);
};

class BoxiLocation
{
	public:
	typedef std::list<BoxiLocation>  List;
	typedef List::iterator       Iterator;
	typedef List::const_iterator ConstIterator;

	private:
	wxString mName;
	wxString mPath;
	BoxiExcludeList mExcluded;
	LocationChangeListener* mpListener;
	
	public:
	BoxiLocation(const wxString& rName, const wxString& rPath, 
		LocationChangeListener* pListener)
	: mName(rName),
	  mPath(rPath),
	  mExcluded(pListener),
	  mpListener(pListener)
	{ }
	
	BoxiLocation(const BoxiLocation& rToCopy) 
	: mName(rToCopy.mName),
	  mPath(rToCopy.mPath),
	  mExcluded(rToCopy.mExcluded),
	  mpListener(rToCopy.mpListener)
	{ }
	
	const wxString& GetName() const { return mName; }
	const wxString& GetPath() const { return mPath; }
	BoxiExcludeList& GetExcludeList() { return mExcluded; }
	const BoxiExcludeList& GetExcludeList() const { return mExcluded; }
	void SetExcludeList(const BoxiExcludeList& rSourceList) 
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

	bool IsSameAs(const BoxiLocation& rOther) const
	{
		if (! rOther.mName.IsSameAs(mName) ) return false;
		if (! rOther.mPath.IsSameAs(mPath) ) return false;
		return mExcluded.IsSameAs(rOther.mExcluded);
	}

	/*	
	bool IsExcluded(const wxString& rLocalFileName, bool mIsDirectory, 
		const BoxiExcludeEntry** ppExcludedBy, 
		const BoxiExcludeEntry** ppIncludedBy);
	*/
	
	ExcludedState GetExcludedState(const wxString& rLocalFileName, 
		bool mIsDirectory, 
		const BoxiExcludeEntry** ppExcludedBy, 
		const BoxiExcludeEntry** ppIncludedBy,
		bool* pMatched);
	
	ExcludeList* GetBoxExcludeList(bool listDirs) const;
	
	void SetListener(LocationChangeListener* pListener)
	{ mpListener = pListener; mExcluded.SetListener(pListener); }

};

#endif /* _LOCATION_H */
