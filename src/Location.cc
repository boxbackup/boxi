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

#include "Utils.h"

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

void MyExcludeList::RemoveEntry(int target) {
	std::vector<MyExcludeEntry*>::iterator current = mEntries.begin();
	int i;
	for (i = 0; i < target; i++) {
		current++;
	}
	if (i == target)
		mEntries.erase(current);
}

void MyExcludeList::RemoveEntry(MyExcludeEntry* oldEntry) {
	std::vector<MyExcludeEntry*>::iterator current = mEntries.begin();
	for ( ; current != mEntries.end() && *current != oldEntry; current++) 
		{ }
		
	if (*current == oldEntry)
		mEntries.erase(current);
}
