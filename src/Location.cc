/***************************************************************************
 *            Location.cc
 *
 *  Mon Feb 28 23:38:45 2005
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

#include <set>
#include <string>
#include <regex.h>

#include <wx/filename.h>

#define NDEBUG
#include "Utils.h"
#undef NDEBUG

#include "Location.h"

MyExcludeList::MyExcludeList(const Configuration& conf) 
{
	for (size_t i = 0; i < sizeof(theExcludeTypes) / sizeof(MyExcludeType); i++)
	{
		const MyExcludeType& t = theExcludeTypes[i];
		addConfigList(conf, t.ToString(), t);
	}
}	

inline void MyExcludeList::addConfigList(const Configuration& conf, 
	const std::string& keyName, const MyExcludeType& type)
{
	if (!conf.KeyExists(keyName.c_str())) return;
	std::string value = conf.GetKeyValue(keyName.c_str());
	addSeparatedList(value, type);
}

inline void MyExcludeList::addSeparatedList(const std::string& entries, 
	const MyExcludeType& type)
{
	std::vector<std::string> temp;
	SplitString(entries, Configuration::MultiValueSeparator, temp);
	for (std::vector<std::string>::const_iterator i = 
		temp.begin(); i != temp.end(); i++)
	{
		std::string temp = *i;
		mEntries.push_back(new MyExcludeEntry(&type, temp));
	}
}

void MyExcludeList::AddEntry(MyExcludeEntry* newEntry) {
	mEntries.push_back(newEntry);
}

void MyExcludeList::ReplaceEntry(int index, MyExcludeEntry* newEntry) {
	mEntries[index] = newEntry;
}

void MyExcludeList::RemoveEntry(int target) 
{
	std::vector<MyExcludeEntry*>::iterator current = mEntries.begin();
	int i;
	for (i = 0; i < target; i++) 
	{
		current++;
	}
	if (i == target)
		mEntries.erase(current);
}

void MyExcludeList::RemoveEntry(MyExcludeEntry* oldEntry) 
{
	std::vector<MyExcludeEntry*>::iterator current = mEntries.begin();
	for ( ; current != mEntries.end() && *current != oldEntry; current++) 
		{ }
		
	if (*current == oldEntry)
		mEntries.erase(current);
}

bool Location::IsExcluded(const wxString& rLocalFileName, bool mIsDirectory, 
	MyExcludeEntry** ppExcludedBy, MyExcludeEntry** ppIncludedBy)
{
	ExcludedState state = GetExcludedState(rLocalFileName, mIsDirectory,
		ppExcludedBy, ppIncludedBy);
	return (state == EST_UNKNOWN || state == EST_NOLOC 
		||  state == EST_EXCLUDED);
}
	
ExcludedState Location::GetExcludedState(const wxString& rLocalFileName, 
	bool mIsDirectory, MyExcludeEntry** ppExcludedBy, 
	MyExcludeEntry** ppIncludedBy, ExcludedState ParentState)
{
	wxLogDebug(wxT(" checking whether %s is excluded..."), 
		rLocalFileName.c_str());
	
	const std::vector<MyExcludeEntry*>& rExcludeList =
		mpExcluded->GetEntries();

	// inherit default state from parent
	ExcludedState isExcluded = EST_UNKNOWN;
	
	if (ParentState != EST_UNKNOWN)
	{
		isExcluded = ParentState;
	}
	else
	{
		wxFileName fn(rLocalFileName);
		wxFileName locroot(GetPath());
		wxFileName root(fn.GetPath());
		root.SetPath(fn.GetPathSeparator());

		*ppExcludedBy = NULL;
		*ppIncludedBy = NULL;
		
		if (fn == locroot)
		{
			isExcluded = EST_INCLUDED;
		}
		else if (fn == root)
		{
			isExcluded = EST_NOLOC;
		}
		else
		{
			isExcluded = GetExcludedState(fn.GetPath(), TRUE,
				ppExcludedBy, ppIncludedBy);
		}
	}
	
	if (isExcluded == EST_ALWAYSINCLUDED ||
		isExcluded == EST_UNKNOWN ||
		isExcluded == EST_NOLOC)
	{
		return isExcluded;
	}

	assert(isExcluded == EST_INCLUDED || isExcluded == EST_EXCLUDED);
	
	// on pass 1, remove Excluded files
	// on pass 2, re-add AlwaysInclude files
	
	for (int pass = 1; pass <= 2; pass++) 
	{
		// wxLogDebug(wxT(" pass %d"), pass);

		if (pass == 2 && isExcluded != EST_EXCLUDED) 
		{
			// not excluded, so don't bother checking the AlwaysInclude entries
			continue;
		}
		
		for (size_t i = 0; i < rExcludeList.size(); i++) 
		{
			MyExcludeEntry* pExclude = rExcludeList[i];
			ExcludeMatch match = pExclude->GetMatch();
			std::string  value = pExclude->GetValue();
			wxString value2(value.c_str(), wxConvLibc);
			bool matched = false;

			{
				std::string name = pExclude->ToString();
				wxString name2(name.c_str(), wxConvLibc);
				wxLogDebug(wxT("  checking against %s"),
					name2.c_str());
			}

			ExcludeSense sense = pExclude->GetSense();
			
			if (pass == 1 && sense != ES_EXCLUDE) 
			{
				wxLogDebug(wxT("   not an Exclude entry"));
				continue;
			}
			
			if (pass == 2 && sense != ES_ALWAYSINCLUDE) 
			{
				wxLogDebug(wxT("   not an AlwaysInclude entry"));
				continue;
			}
			
			ExcludeFileDir fileOrDir = pExclude->GetFileDir();
			
			if (fileOrDir == EFD_FILE && mIsDirectory) 
			{
				wxLogDebug(wxT("   doesn't match directories"));
				continue;
			}
			
			if (fileOrDir == EFD_DIR && !mIsDirectory) 
			{
				wxLogDebug(wxT("   doesn't match files"));
				continue;
			}
			
			if (match == EM_EXACT) 
			{
				if (rLocalFileName == value2) 
				{
					wxLogDebug(wxT("    exact match"));
					matched = true;
				}
			} 
			else if (match == EM_REGEX) 
			{
				std::auto_ptr<regex_t> apr = 
					std::auto_ptr<regex_t>(new regex_t);
				if (::regcomp(apr.get(), value.c_str(),
					REG_EXTENDED | REG_NOSUB) != 0) 
				{
					wxLogError(wxT("Regular expression compile failed (%s)"),
						value2.c_str());
				}
				else
				{
					wxCharBuffer buf = rLocalFileName.mb_str(wxConvLibc);
					int result = regexec(apr.get(), buf.data(), 0, 0, 0);
					matched = (result == 0);
				}
			}
			
			if (!matched) 
			{
				wxLogDebug(wxT("   no match."));
				continue;
			}

			wxLogDebug(wxT("   matched!"));
			
			if (sense == ES_EXCLUDE)
			{
				isExcluded = EST_EXCLUDED;
				if (ppExcludedBy)
					*ppExcludedBy = pExclude;
			} 
			else if (sense == ES_ALWAYSINCLUDE)
			{
				isExcluded = EST_ALWAYSINCLUDED;
				if (ppIncludedBy)
					*ppIncludedBy = pExclude;
			}
		}
	}
	
	return isExcluded;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    GetBoxExcludeList()
//		Purpose: Return a Box Backup-compatible ExcludeList.
//		Created: 28/1/04
//
// --------------------------------------------------------------------------
ExcludeList* Location::GetBoxExcludeList(bool listDirs)
{
	// Create the exclude list
	ExcludeList *pExclude = new ExcludeList;
	ExcludeList *pInclude = new ExcludeList;
	pExclude->SetAlwaysIncludeList(pInclude);
	
	typedef const std::vector<MyExcludeEntry*> tMyExcludeEntries;
	tMyExcludeEntries& rEntries = mpExcluded->GetEntries();
	
	try
	{
		for (tMyExcludeEntries::const_iterator i = rEntries.begin();
			i != rEntries.end(); i++)
		{
			const MyExcludeEntry* pEntry = *i;
			
			if (listDirs && (pEntry->GetFileDir() == EFD_FILE))
				continue;
			
			if (!listDirs && (pEntry->GetFileDir() == EFD_DIR))
				continue;
			
			ExcludeList* pList = NULL;
			if (pEntry->GetSense() == ES_EXCLUDE)
			{
				pList = pExclude;
			}
			else if (pEntry->GetSense() == ES_ALWAYSINCLUDE)
			{
				pList = pInclude;
			}
			
			if (pEntry->GetMatch() == EM_EXACT)
			{
				pList->AddDefiniteEntries(pEntry->GetValue());
			}
			else if (pEntry->GetMatch() == EM_REGEX)
			{
				pList->AddRegexEntries(pEntry->GetValue());
			}
		}
	}
	catch(...)
	{
		// Clean up
		delete pExclude;
		delete pInclude;
		throw;
	}

	return pExclude;
}
