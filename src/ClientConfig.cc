/***************************************************************************
 *            ClientConfig.cc
 *
 *  Mon Feb 28 22:52:32 2005
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

#include <wx/file.h>
#include <wx/filename.h>

#include "Box.h"
#include "BackupDaemonConfigVerify.h"

#include "main.h"
#include "ClientConfig.h"

#define INIT_PROP(name, value) name(#name, value, this)
ClientConfig::ClientConfig()
: 	INIT_PROP(CertificateFile, "/etc/box/bbackupd/999-cert.pem"),
	INIT_PROP(PrivateKeyFile,  "/etc/box/bbackupd/999-key.pem"),
	INIT_PROP(DataDirectory,   "/var/bbackupd"),
	INIT_PROP(NotifyScript,    "/etc/box/bbackupd/NotifySysadmin.sh"),
	INIT_PROP(TrustedCAsFile,  "/etc/box/bbackupd/serverCA.pem"),
	INIT_PROP(KeysFile,        "/etc/box/bbackupd/999-FileEncKeys.raw"),
	INIT_PROP(StoreHostname,   "my.server.host"),
	INIT_PROP(SyncAllowScript, ""),
	INIT_PROP(CommandSocket,   "/var/run/bbackupd.sock"),
	INIT_PROP(PidFile,         "/var/run/bbackupd.pid"),
	INIT_PROP(AccountNumber,   999),
	INIT_PROP(UpdateStoreInterval, 3600),
	INIT_PROP(MinimumFileAge,  21600),
	INIT_PROP(MaxUploadWait,   86400),
	INIT_PROP(FileTrackingSizeThreshold, 65535),
	INIT_PROP(DiffingUploadSizeThreshold, 8192),
	INIT_PROP(MaximumDiffingTime, 20),
	INIT_PROP(ExtendedLogging, true)
{
	SetClean();	
}

ClientConfig::ClientConfig(const wxString& rConfigFileName) 
: 	INIT_PROP(CertificateFile, "/etc/box/bbackupd/999-cert.pem"),
	INIT_PROP(PrivateKeyFile,  "/etc/box/bbackupd/999-key.pem"),
	INIT_PROP(DataDirectory,   "/var/bbackupd"),
	INIT_PROP(NotifyScript,    "/etc/box/bbackupd/NotifySysadmin.sh"),
	INIT_PROP(TrustedCAsFile,  "/etc/box/bbackupd/serverCA.pem"),
	INIT_PROP(KeysFile,        "/etc/box/bbackupd/999-FileEncKeys.raw"),
	INIT_PROP(StoreHostname,   "my.server.host"),
	INIT_PROP(SyncAllowScript, ""),
	INIT_PROP(CommandSocket,   "/var/run/bbackupd.sock"),
	INIT_PROP(PidFile,         "/var/run/bbackupd.pid"),
	INIT_PROP(AccountNumber,   999),
	INIT_PROP(UpdateStoreInterval, 3600),
	INIT_PROP(MinimumFileAge,  21600),
	INIT_PROP(MaxUploadWait,   86400),
	INIT_PROP(FileTrackingSizeThreshold, 65535),
	INIT_PROP(DiffingUploadSizeThreshold, 8192),
	INIT_PROP(MaximumDiffingTime, 20),
	INIT_PROP(ExtendedLogging, true)
{
	Load(rConfigFileName);
}
#undef INIT_PROP

void ClientConfig::Load(const wxString& rConfigFileName)
{
	std::string errs;
	
	mapConfig = Configuration::LoadAndVerify(
		rConfigFileName.c_str(), 
		&BackupDaemonConfigVerify, errs);
		
	this->mConfigFileName = rConfigFileName;
	
	if (mapConfig.get() == 0 || !errs.empty())
	{
		wxString *exception = new wxString();
		exception->Printf(
			"Invalid configuration file:\n%s", errs.c_str());
		throw exception;
	}

	#define STR_PROP(name) \
		name.SetFrom(mapConfig.get());
	#define STR_PROP_SUBCONF(name, subconf) \
		name.SetFrom( &(mapConfig->GetSubConfiguration(#subconf)) );
	#define INT_PROP(name) \
		name.SetFrom(mapConfig.get());
	#define BOOL_PROP(name) \
		name.SetFrom(mapConfig.get());
	ALL_PROPS
	#undef BOOL_PROP
	#undef INT_PROP
	#undef STR_PROP_SUBCONF
	#undef STR_PROP

	const Configuration& rLocations =
		mapConfig->GetSubConfiguration("BackupLocations");
	mLocations.clear();

	for(std::list<std::pair<std::string, Configuration> >::const_iterator i = 
		rLocations.mSubConfigurations.begin();
		i != rLocations.mSubConfigurations.end(); 
		i++)
	{
		std::string name = i->first;
		std::string path = i->second.GetKeyValue("Path");
		
		Location *pLoc = new Location(wxString(name.c_str()), 
			wxString(path.c_str()));

		MyExcludeList* pExcluded = new MyExcludeList(i->second);
		pLoc->SetExcludeList(pExcluded);
		
		mLocations.push_back(pLoc);
	}
	
	SetClean();
	NotifyListeners();
}	

void ClientConfig::AddLocation(Location* newLoc) {
	mLocations.push_back(newLoc);
}

void ClientConfig::ReplaceLocation(int index, Location* newLoc) {
	mLocations[index] = newLoc;
}

void ClientConfig::RemoveLocation(int target) {
	std::vector<Location*>::iterator current = mLocations.begin();
	int i;
	for (i = 0; i < target; i++) {
		current++;
	}
	if (i == target)
		mLocations.erase(current);
}

void ClientConfig::RemoveLocation(Location* pOldLocation) {
	std::vector<Location*>::iterator current = mLocations.begin();
	
	for ( ; current != mLocations.end() && *current != pOldLocation; current++)
		{ }
	
	if (*current == pOldLocation)
		mLocations.erase(current);
}

void ClientConfig::AddListener(ConfigChangeListener* pNewListener)
{
	for (std::vector<ConfigChangeListener*>::iterator i = mListeners.begin();
		i != mListeners.end(); i++)
	{
		if (*i == pNewListener)
			return;
	}
	mListeners.push_back(pNewListener);
}

void ClientConfig::RemoveListener(ConfigChangeListener* pOldListener)
{
	for (std::vector<ConfigChangeListener*>::iterator i = mListeners.begin();
		i != mListeners.end(); i++)
	{
		if (*i == pOldListener)
		{
			mListeners.erase(i);
			return;
		}
	}
}

void ClientConfig::NotifyListeners() 
{
	for (std::vector<ConfigChangeListener*>::iterator i = mListeners.begin();
		i != mListeners.end(); i++)
	{
		(*i)->NotifyChange();
	}
}

bool ClientConfig::Save() 
{
	return Save(mConfigFileName);
}

bool ClientConfig::Save(const wxString& rConfigFileName) 
{
	wxTempFile file(rConfigFileName);
	
	if (!file.IsOpened()) {
		return false;
	}
	
	file.Write(
		"# This Box Backup configuration file was written by Boxi\n"
		"# This file is automatically generated. Do not edit by hand!\n"
		"\n");
	
	wxString buffer;

	#define WRITE_STR_PROP(name) \
	if (const std::string * value = name.Get()) { \
		buffer.Printf(#name " = %s\n", value->c_str()); \
		file.Write(buffer); \
	}

	#define WRITE_INT_PROP_FMT(name, fmt) \
	if (const int * value = name.Get()) { \
		buffer.Printf(#name " = " fmt "\n", *value); \
		file.Write(buffer); \
	}
	
	#define WRITE_INT_PROP(name) \
	if (const int * value = name.Get()) { \
		buffer.Printf(#name " = %d\n", *value); \
		file.Write(buffer); \
	}

	#define WRITE_BOOL_PROP(name) \
	if (const bool * value = name.Get()) { \
		buffer.Printf(#name " = %s\n", *value ? "yes" : "no"); \
		file.Write(buffer); \
	}
	
	WRITE_STR_PROP(StoreHostname)
	WRITE_INT_PROP_FMT(AccountNumber, "0x%x")
	WRITE_STR_PROP(KeysFile)
	WRITE_STR_PROP(CertificateFile)
	WRITE_STR_PROP(PrivateKeyFile)
	WRITE_STR_PROP(TrustedCAsFile)
	WRITE_STR_PROP(DataDirectory)
	WRITE_STR_PROP(NotifyScript)
	WRITE_INT_PROP(UpdateStoreInterval)
	WRITE_INT_PROP(MinimumFileAge)
	WRITE_INT_PROP(MaxUploadWait)
	WRITE_INT_PROP(FileTrackingSizeThreshold)
	WRITE_INT_PROP(DiffingUploadSizeThreshold)
	WRITE_INT_PROP(MaximumDiffingTime)
	WRITE_BOOL_PROP(ExtendedLogging)
	WRITE_STR_PROP(SyncAllowScript)
	WRITE_STR_PROP(CommandSocket)

	#undef WRITE_STR_PROP
	#undef WRITE_INT_PROP
	#undef WRITE_INT_PROP_FMT
	#undef WRITE_BOOL_PROP
	
	if (PidFile.Get()) {
		std::string pidbuf;
		PidFile.GetInto(pidbuf);
		buffer.Printf("Server\n{\n\tPidFile = %s\n}\n", pidbuf.c_str());
		file.Write(buffer);
	}
	
	const std::vector<Location*>& rLocations = GetLocations();
	file.Write("BackupLocations\n{\n");
	
	for (size_t i = 0; i < rLocations.size(); i++) {
		Location* pLoc = rLocations[i];
		buffer.Printf("\t%s\n\t{\n\t\tPath = %s\n", pLoc->GetName().c_str(), 
			pLoc->GetPath().c_str());
		file.Write(buffer);
		
		const std::vector<MyExcludeEntry*>& rEntries = 
			pLoc->GetExcludeList().GetEntries();
		
		for (size_t l = 0; l < rEntries.size(); l++) {
			MyExcludeEntry* pEntry = rEntries[l];
			buffer.Printf("\t\t%s\n", pEntry->ToString().c_str());
			file.Write(buffer);
		}
		
		file.Write("\t}\n");
	}
	
	file.Write("}\n");
	file.Commit();
	
	// clean configuration
	Load(rConfigFileName);
	
	return TRUE;
}

void ClientConfig::SetClean() 
{
	#define PROP(name) name.SetClean();
	#define STR_PROP(name) PROP(name)
	#define STR_PROP_SUBCONF(name, conf) PROP(name)
	#define INT_PROP(name) PROP(name)
	#define BOOL_PROP(name) PROP(name)
	ALL_PROPS
	#undef BOOL_PROP
	#undef INT_PROP
	#undef STR_PROP
	#undef STR_PROP_SUBCONF
	#undef PROP
}

bool ClientConfig::IsClean() 
{
	#define PROP(name) if (!name.IsClean()) return FALSE;
	#define STR_PROP(name) PROP(name)
	#define STR_PROP_SUBCONF(name, conf) PROP(name)
	#define INT_PROP(name) PROP(name)
	#define BOOL_PROP(name) PROP(name)
	ALL_PROPS
	#undef BOOL_PROP
	#undef INT_PROP
	#undef STR_PROP
	#undef STR_PROP_SUBCONF
	#undef PROP

	std::vector<Location*> oldLocs;
	
	if (mapConfig.get())
	{
		const Configuration& rLocations = 
			mapConfig->GetSubConfiguration("BackupLocations");
	
		for(std::list<std::pair<std::string, Configuration> >::const_iterator i = 
			rLocations.mSubConfigurations.begin();
			i != rLocations.mSubConfigurations.end(); 
			i++)
		{
			std::string name = i->first;
			std::string path = i->second.GetKeyValue("Path");
			
			Location *pLoc = new Location(wxString(name.c_str()), 
				wxString(path.c_str()));
	
			MyExcludeList* pExcluded = new MyExcludeList(i->second);
			pLoc->SetExcludeList(pExcluded);
			
			oldLocs.push_back(pLoc);
		}
	}
	
	if (oldLocs.size() != mLocations.size())
		return FALSE;
	
	for (size_t i = 0; i < mLocations.size(); i++) {
		Location* pOld = oldLocs[i];
		Location* pNew = mLocations[i];
		if (!pNew->IsSameAs(*pOld))
			return FALSE;
	}
	
	return TRUE;
}

void ClientConfig::OnPropertyChange(Property* pProp)
{
	NotifyListeners();
}

bool PropSet(StringProperty& rProp, wxString& msg, const char* desc)
{
	if (rProp.Get())
		return TRUE;
	msg.Printf("The %s is not set", desc);
	return FALSE;
}

bool PropSet(IntProperty& rProp, wxString& msg, const char* desc)
{
	if (rProp.Get())
		return TRUE;
	msg.Printf("The %s is not set", desc);
	return FALSE;
}

bool PropSet(BoolProperty& rProp, wxString& msg, const char* desc)
{
	if (rProp.Get())
		return TRUE;
	msg.Printf("The %s is not set", desc);
	return FALSE;
}

bool PropPath(StringProperty& rProp, wxString& msg, bool dir, const char *desc)
{
	std::string path1;
	rProp.GetInto(path1);
	wxString path2 = path1.c_str();
	if (dir) 
	{
		if (wxFileName::FileExists(path2))
		{
			msg.Printf("The %s is not a directory (%s)",
				desc, path1.c_str());
			return FALSE;
		}
		if (!wxFileName::DirExists(path2))
		{
			msg.Printf("The %s does not exist (%s)",
				desc, path1.c_str());
			return FALSE;
		}
	} 
	else 
	{
		if (wxFileName::DirExists(path2))
		{
			msg.Printf("The %s is not a file (%s)",
				desc, path1.c_str());
			return FALSE;
		}
		if (!wxFileName::FileExists(path2))
		{
			msg.Printf("The %s does not exist (%s)",
				desc, path1.c_str());
			return FALSE;
		}
	}
	
	return TRUE;
}

bool PropWritable(StringProperty& rProp, wxString& msg, const char *desc)
{
	std::string path1;
	rProp.GetInto(path1);
	wxString path2 = path1.c_str();
	if (wxFileName::FileExists(path2))
	{
		if (wxFile::Access(path2, wxFile::write))
			return TRUE;
		msg.Printf("The %s cannot be written (%s)", desc, path1.c_str());
		return FALSE;
	}
	else
	{
		wxFileName fn(path2);
		fn.SetName("");
		fn.SetExt("");
		wxString parentPath = fn.GetFullPath();
		if (!wxFileName::DirExists(parentPath)) {
			msg.Printf("The %s parent directory does not exist (%s)",
				desc, parentPath.c_str());
			return FALSE;
		}
		if (wxFile::Access(parentPath, wxFile::write))
			return TRUE;
		msg.Printf("The %s parent directory cannot be written (%s)", 
			desc, parentPath.c_str());
		return FALSE;
	}

	return TRUE;
}	

bool ClientConfig::Check(wxString& rMsg)
{
	if (!PropSet(StoreHostname,    rMsg, "Store Host"))
		return FALSE;
	if (!PropSet(AccountNumber,    rMsg, "Account Number"))
		return FALSE;
	if (!PropPath(KeysFile,        rMsg, FALSE, "Keys File"))
		return FALSE;
	if (!PropPath(CertificateFile, rMsg, FALSE, "Certificate File"))
		return FALSE;
	if (!PropPath(PrivateKeyFile,  rMsg, FALSE, "Private Key File"))
		return FALSE;
	if (!PropPath(TrustedCAsFile,  rMsg, FALSE, "Trusted CAs File"))
		return FALSE;
	if (!PropPath(DataDirectory,   rMsg, TRUE, "Data Directory"))
		return FALSE;
	if (PropSet(NotifyScript, rMsg, ""))
	{
		if (!PropPath(NotifyScript, rMsg, FALSE, "Notify Script"))
			return FALSE;
	}
	if (!PropSet(UpdateStoreInterval, rMsg, "Update Store Interval"))
		return FALSE;
	if (!PropSet(MinimumFileAge, rMsg, "Minimum File Age"))
		return FALSE;
	if (!PropSet(MaxUploadWait, rMsg, "Maximum Upload Wait"))
		return FALSE;
	if (!PropSet(FileTrackingSizeThreshold, rMsg, 
		"File Tracking Size Threshold"))
		return FALSE;
	if (!PropSet(DiffingUploadSizeThreshold, rMsg, 
		"Diffing Upload Size Threshold"))
		return FALSE;
	if (!PropSet(MaximumDiffingTime, rMsg, "Maximum Diffing Time"))
		return FALSE;
	if (!PropSet(MaxUploadWait, rMsg, "Maximum Upload Wait"))
		return FALSE;
	if (PropSet(SyncAllowScript, rMsg, ""))
	{
		if (!PropPath(SyncAllowScript, rMsg, FALSE, 
			"Sync Allow Script"))
			return FALSE;
	}
	if (!PropWritable(CommandSocket, rMsg, "Command Socket"))
		return FALSE;
	if (!PropWritable(PidFile, rMsg, "Process ID File"))
		return FALSE;
	
	return TRUE;
}
