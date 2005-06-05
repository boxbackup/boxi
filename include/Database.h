/***************************************************************************
 *            Database.h
 *
 *  Thu May 26 19:38:33 2005
 *  Copyright  2005  Chris Wilson
 *  Email boxi_Database.h@qwirx.com
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
 
#ifndef _DATABASE_H
#define _DATABASE_H

#include <vector>

#include <wx/wx.h>

#include <libxml/xmlreader.h>

class Entry {
	public:
	typedef wxString Name;
	typedef enum Type {
		T_UNKNOWN = 0,
		T_FILE,
		T_DIR,
	} Type;

	typedef int Permission;
	typedef size_t Size;
	typedef wxString Checksum;
	typedef wxString Modtime;

	private:
	Name       mName;
	Type       mType;
	Permission mPermission;
	size_t     mSize;
	Checksum   mChecksum;
	Modtime    mModtime;
	bool       mAscii;
	
	public:
	Entry(
		const wxString&   lrName, 
		const Type&       lrType, 
		const Permission& lrPermission, 
		const Size&       lrSize,
		const Checksum&   lrChecksum,
		const Modtime&    lrModtime,
		const bool&       lrAscii) 
	: mName      (lrName),
	  mType      (lrType),
	  mPermission(lrPermission),
	  mSize      (lrSize),
	  mChecksum  (lrChecksum),
	  mModtime   (lrModtime),
	  mAscii     (lrAscii)
	{ }
	
	Entry(const Entry& lrOther)
	: mName      (lrOther.mName),
	  mType      (lrOther.mType),
	  mPermission(lrOther.mPermission),
	  mSize      (lrOther.mSize),
	  mChecksum  (lrOther.mChecksum),
	  mModtime   (lrOther.mModtime),
	  mAscii     (lrOther.mAscii)
	{ }
	
	const Name&       GetName()       const { return mName; }
	const Type&       GetType()       const { return mType; }
	const Permission& GetPermission() const { return mPermission; }
	const Size&       GetSize()       const { return mSize; }
	const Checksum&   GetChecksum()   const { return mChecksum; }
	const Modtime&    GetModtime()    const { return mModtime; }
	const bool&       IsAscii()       const { return mAscii; }
};
	
class Database {
	private:
	wxString mFileName;
	std::vector<Entry> mEntries;
	bool mInItem, mInAscii;
	
	public:
	Database() { mInItem = FALSE; }
	Database(const wxString& lrFileName)
	{ 
		mInItem = FALSE;
		mInAscii = FALSE;
		Load(lrFileName);
	}

	void Load(const wxString& lrFileName);
	const std::vector<Entry>& GetEntries() { return mEntries; }
	
	private:
	Entry::Name       mEntryName;
	Entry::Type       mEntryType;
	Entry::Permission mEntryPermission;
	Entry::Size       mEntrySize;
	Entry::Checksum   mEntryChecksum;
	Entry::Modtime    mEntryModtime;
	bool              mEntryAscii;
	
	void ProcessNode        (xmlTextReaderPtr reader);
	void HandleItem         (xmlTextReaderPtr reader);
	void HandleFilename     (xmlTextReaderPtr reader);
	void HandleTypeDirectory(xmlTextReaderPtr reader);
	void HandleTypeFile     (xmlTextReaderPtr reader);
	void HandleProtection   (xmlTextReaderPtr reader);
	void HandleSize         (xmlTextReaderPtr reader);
	void HandleModtime      (xmlTextReaderPtr reader);
	void HandleAscii        (xmlTextReaderPtr reader);
	void HandleTrue         (xmlTextReaderPtr reader);
	void HandleFalse        (xmlTextReaderPtr reader);
};

#endif /* _DATABASE_H */
