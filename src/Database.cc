/***************************************************************************
 *            Database.cc
 *
 *  Thu May 26 19:38:57 2005
 *  Copyright  2005  Chris Wilson
 *  Email boxi_Database.cc@qwirx.com
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
 
#include "Database.h"

void Database::Load(const wxString& lrFileName) {
	mFileName = lrFileName;
	mEntries.clear();
	
	if (mFileName.Length() == 0)
		return;
	
	xmlTextReaderPtr reader;
	int ret;
	
	reader = xmlNewTextReaderFilename(mFileName.c_str());
	if (reader == NULL) {
		wxLogError("Unable to open database file %s",
			mFileName.c_str());
		return;
	}
	
	ret = xmlTextReaderRead(reader);
	while (ret == 1) {
		ProcessNode(reader);
		ret = xmlTextReaderRead(reader);
	}
	xmlFreeTextReader(reader);
	
	if (ret != 0) {
		wxLogError("Unable to open database file %s",
			mFileName.c_str());
	}
}

void Database::ProcessNode(xmlTextReaderPtr reader) 
{
	wxString name( (const char *)xmlTextReaderConstName(reader) );
	bool isStart = (xmlTextReaderNodeType(reader) == 1);
	
	if (name.IsSameAs("item")) 
	{
		HandleItem(reader);
	}
	else if (name.IsSameAs("filename") && isStart) 
	{
		HandleFilename(reader);
	}
	else if (name.IsSameAs("type-file") && isStart) 
	{
		HandleTypeFile(reader);
	}
	else if (name.IsSameAs("type-directory") && isStart) 
	{
		HandleTypeDirectory(reader);
	}
	else if (name.IsSameAs("protection") && isStart) 
	{
		HandleProtection(reader);
	}
	else if (name.IsSameAs("size") && isStart) 
	{
		HandleSize(reader);
	}
	else if (name.IsSameAs("modtime") && isStart) 
	{
		HandleModtime(reader);
	}
	else if (name.IsSameAs("ascii")) 
	{
		HandleAscii(reader);
	}
	else if (name.IsSameAs("true")) 
	{
		HandleTrue(reader);
	}
	else if (name.IsSameAs("false")) 
	{
		HandleFalse(reader);
	}
}

void Database::HandleItem(xmlTextReaderPtr reader)
{
	if (xmlTextReaderNodeType(reader) == 1) 
	{
		// start element
		if (mInItem) {
			wxLogError("Nested items");
			return;
		}
		mInItem = TRUE;
		mEntryName = "";
		mEntryType = Entry::T_UNKNOWN;
		mEntryPermission = 0;
		mEntrySize = 0;
		mEntryChecksum = "";
		mEntryAscii = FALSE;
	}
	else if (xmlTextReaderNodeType(reader) == 15)
	{
		// end element
		if (!mInItem) {
			wxLogError("End item without starting");
			return;
		}
		mInItem = FALSE;
		Entry lNewEntry(
			mEntryName, mEntryType, mEntryPermission,
			mEntrySize, mEntryChecksum, mEntryModtime, mEntryAscii);
		mEntries.push_back(lNewEntry);
	}
}

const wxString GetValue(xmlTextReaderPtr reader) 
{
	// move from the element to its value, which follows
	int ret = xmlTextReaderRead(reader);

	assert(ret == 1);
	assert(xmlTextReaderNodeType(reader) == 3);

	wxString value( 
		(const char *) xmlTextReaderConstValue(reader) );
	
	return value;
}
	
void Database::HandleFilename (xmlTextReaderPtr reader)
{
	if (!mInItem) {
		wxLogError("filename outside of item");
		return;
	}
	
	mEntryName = GetValue(reader);
}

void Database::HandleTypeFile(xmlTextReaderPtr reader)
{
	if (!mInItem) {
		wxLogError("type-file outside of item");
		return;
	}
	mEntryType = Entry::T_FILE;
}

void Database::HandleTypeDirectory(xmlTextReaderPtr reader)
{
	// start element
	if (!mInItem) {
		wxLogError("type-directory outside of item");
		return;
	}
	mEntryType = Entry::T_DIR;
}

void Database::HandleProtection(xmlTextReaderPtr reader)
{
	if (!mInItem) {
		wxLogError("type-directory outside of item");
		return;
	}

	wxString prot(GetValue(reader));

	long lPermission;
	if (!prot.ToLong(&lPermission, 8)) {
		wxLogError("bad octal permission: %s", prot.c_str());
		return;
	}
	mEntryPermission = lPermission;
}
	
void Database::HandleSize(xmlTextReaderPtr reader)
{
	if (!mInItem) {
		wxLogError("size outside of item");
		return;
	}

	wxString lSizeString(GetValue(reader));

	char* endptr;
	mEntrySize = ::strtoull(lSizeString.c_str(), &endptr, 10);
	if (*endptr) {
		wxLogError("bad size: %s", lSizeString.c_str());
		return;
	}
}

void Database::HandleModtime(xmlTextReaderPtr reader)
{
	if (!mInItem) {
		wxLogError("modtime outside of item");
		return;
	}
	mEntryModtime = GetValue(reader);
}

void Database::HandleAscii(xmlTextReaderPtr reader)
{
	if (xmlTextReaderNodeType(reader) == 1) 
	{
		if (!mInItem) {
			wxLogError("modtime outside of item");
			return;
		}
		if (mInAscii) {
			wxLogError("nested ascii elements");
			return;
		}
		mInAscii = TRUE;
	}
	else if (xmlTextReaderNodeType(reader) == 15) 
	{
		if (!mInItem) {
			wxLogError("</ascii> outside of <item>");
			return;
		}
		if (!mInAscii) {
			wxLogError("</ascii> without <ascii>");
			return;
		}
		mInAscii = FALSE;
	}
}
	
void Database::HandleTrue(xmlTextReaderPtr reader)
{
	if (!mInItem) {
		wxLogError("true outside of item");
		return;
	}
	if (!mInAscii) {
		wxLogError("true outside of ascii");
		return;
	}
	mEntryAscii = TRUE;
}

void Database::HandleFalse(xmlTextReaderPtr reader)
{
	if (!mInItem) {
		wxLogError("false outside of item");
		return;
	}
	if (!mInAscii) {
		wxLogError("false outside of ascii");
		return;
	}
	mEntryAscii = FALSE;
}
