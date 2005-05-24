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

#include <wx/wx.h>

#include "Configuration.h"

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

class MyExcludeType {
	public:
	MyExcludeType(ExcludeSense sense, ExcludeFileDir fileDir, 
		ExcludeMatch match) 
	{
		this->mSense 		= sense;
		this->mFileOrDir 	= fileDir;
		this->mMatch 		= match;
	}

	public:
	
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

const MyExcludeType theExcludeTypes[] = {
	MyExcludeType(ES_EXCLUDE, 		EFD_DIR, 	EM_EXACT),
	MyExcludeType(ES_EXCLUDE, 		EFD_DIR, 	EM_REGEX),
	MyExcludeType(ES_EXCLUDE, 		EFD_FILE, 	EM_EXACT),
	MyExcludeType(ES_EXCLUDE, 		EFD_FILE, 	EM_REGEX),
	MyExcludeType(ES_ALWAYSINCLUDE, EFD_DIR, 	EM_EXACT),
	MyExcludeType(ES_ALWAYSINCLUDE, EFD_DIR, 	EM_REGEX),
	MyExcludeType(ES_ALWAYSINCLUDE, EFD_FILE, 	EM_EXACT),
	MyExcludeType(ES_ALWAYSINCLUDE, EFD_FILE, 	EM_REGEX),
};

class MyExcludeEntry {
	public:
	MyExcludeEntry(const MyExcludeType* type, const std::string& value) 
	{
		this->mType  = type;
		this->mValue = value;
	}
	
	public:
	
	ExcludeSense   GetSense()   const { return mType->GetSense(); }
	ExcludeFileDir GetFileDir() const { return mType->GetFileDir(); }
	ExcludeMatch   GetMatch()   const { return mType->GetMatch(); }

	const std::string GetSenseString()   const { return mType->GetSenseString(); }
	const std::string GetFileDirString() const { return mType->GetFileDirString(); }
	const std::string GetMatchString()   const { return mType->GetMatchString(); }
	
	const std::string& GetValue() const { return mValue; }

	const std::string ToStringType() const { return mType->ToString(); }

	std::string ToString() {
		std::string buffer;
		buffer.append(ToStringType());
		buffer.append(" = ");
		buffer.append(mValue);
		return buffer;
	}

	bool IsSameAs(MyExcludeEntry& other) const
	{
		if (other.GetSense()   != GetSense())      return FALSE;
		if (other.GetFileDir() != GetFileDir())    return FALSE;
		if (other.GetMatch()   != GetMatch())      return FALSE;
		if (other.GetValue().compare(mValue) != 0) return FALSE;
		return TRUE;
	}
	
	private:
	const MyExcludeType* mType;
	std::string mValue;
};

class MyExcludeList {
	public:
	MyExcludeList(const Configuration& conf);
	const std::vector<MyExcludeEntry*>& GetEntries() const { return mEntries; }
	void AddEntry(MyExcludeEntry* newEntry);
	void ReplaceEntry(int index, MyExcludeEntry* newValues);
	void RemoveEntry(int index);

	bool IsSameAs(MyExcludeList& other) {
		const std::vector<MyExcludeEntry*>& otherEntries = other.GetEntries();
		if (otherEntries.size() != mEntries.size()) return FALSE;
		for (size_t i = 0; i < mEntries.size(); i++) {
			if (! mEntries[i]->IsSameAs( *(otherEntries[i]) ))
				return FALSE;
		}
		return TRUE;
	}
	
	private:
	std::vector<MyExcludeEntry*> mEntries;
	void addConfigList(const Configuration& conf, const std::string& keyName, 
		const MyExcludeType& type);
	void addSeparatedList(const std::string& entries, 
		const MyExcludeType& type);
};

class Location {
	public:
	Location(wxString Name, wxString Path) { 
		this->mName = Name;
		this->mPath = Path;
	}
	const wxString& GetName() const { return mName; }
	const wxString& GetPath() const { return mPath; }
	MyExcludeList& GetExcludeList() { return *mpExcluded; }
	void SetExcludeList(MyExcludeList *list) { mpExcluded = list; }
	void SetName(const std::string& name) { this->mName = name.c_str(); }
	void SetPath(const std::string& path) { this->mPath = path.c_str(); }

	bool IsSameAs(Location& rOther)
	{
		if (! rOther.mName.IsSameAs(mName) ) return false;
		if (! rOther.mPath.IsSameAs(mPath) ) return false;
		return mpExcluded->IsSameAs(*(rOther.mpExcluded));
	}
	
	private:
	wxString mName;
	wxString mPath;
	MyExcludeList* mpExcluded;
};

#endif /* _LOCATION_H */
