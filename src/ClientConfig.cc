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
 *
 *  Includes (small) portions of OpenSSL by Eric Young (eay@cryptsoft.com) and
 *  Tim Hudson (tjh@cryptsoft.com), under their source redistribution license.
 */

#include <sys/types.h>

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/socket.h>

#include <openssl/err.h>
#include <openssl/pem.h>

#include "SandBox.h"

#define NDEBUG
#include "Box.h"
#include "BackupDaemonConfigVerify.h"
#undef NDEBUG

#include "main.h"
#include "ClientConfig.h"
#include "SslConfig.h"

#define INIT_PROP(name, value) name(#name, value, this)
#define INIT_PROP_EMPTY(name)  name(#name, this)
#define INIT_PROPS_DEFAULTS \
	INIT_PROP_EMPTY(CertRequestFile), \
	INIT_PROP_EMPTY(CertificateFile), \
	INIT_PROP_EMPTY(PrivateKeyFile), \
	INIT_PROP(DataDirectory,   "/var/bbackupd"), \
	INIT_PROP(NotifyScript,    "/etc/box/bbackupd/NotifySysadmin.sh"), \
	INIT_PROP(TrustedCAsFile,  "/etc/box/bbackupd/serverCA.pem"), \
	INIT_PROP_EMPTY(KeysFile), \
	INIT_PROP_EMPTY(StoreHostname), \
	INIT_PROP(SyncAllowScript, ""), \
	INIT_PROP(CommandSocket,   "/var/run/bbackupd.sock"), \
	INIT_PROP(PidFile,         "/var/run/bbackupd.pid"), \
	INIT_PROP_EMPTY(AccountNumber), \
	INIT_PROP(UpdateStoreInterval, 3600), \
	INIT_PROP(MinimumFileAge,  21600), \
	INIT_PROP(MaxUploadWait,   86400), \
	INIT_PROP(FileTrackingSizeThreshold, 65535), \
	INIT_PROP(DiffingUploadSizeThreshold, 8192), \
	INIT_PROP(MaximumDiffingTime, 20), \
	INIT_PROP(MaxFileTimeInFuture, 0), \
	INIT_PROP(ExtendedLogging, true)
	
ClientConfig::ClientConfig()
: INIT_PROPS_DEFAULTS
{
	SetClean();	
}

ClientConfig::ClientConfig(const wxString& rConfigFileName) 
: INIT_PROPS_DEFAULTS
{
	Load(rConfigFileName);
}
#undef INIT_PROPS_DEFAULTS
#undef INIT_PROP_EMPTY
#undef INIT_PROP

std::vector<Location> ClientConfig::GetConfigurationLocations(
	const Configuration& conf)
{
	const Configuration& rLocations = 
		conf.GetSubConfiguration("BackupLocations");
	std::vector<Location> locs;

	for(std::list<std::pair<std::string, Configuration> >::const_iterator i = 
		rLocations.mSubConfigurations.begin();
		i != rLocations.mSubConfigurations.end(); 
		i++)
	{
		std::string name = i->first;
		std::string path = i->second.GetKeyValue("Path");
		
		Location loc(
			wxString(name.c_str(), wxConvLibc), 
			wxString(path.c_str(), wxConvLibc), NULL);

		MyExcludeList ex(i->second, NULL);
		loc.SetExcludeList(ex);
		
		locs.push_back(loc);
	}
	
	return locs;
}

void ClientConfig::Load(const wxString& rConfigFileName)
{
	std::string errs;

	wxCharBuffer buf = rConfigFileName.mb_str(wxConvLibc);	
	mapConfig = Configuration::LoadAndVerify(buf.data(),
		&BackupDaemonConfigVerify, errs);
		
	this->mConfigFileName = rConfigFileName;
	
	if (mapConfig.get() == 0 || !errs.empty())
	{
		wxString *exception = new wxString();
		exception->Printf(
			wxT("Invalid configuration file:\n%s"), 
			errs.c_str());
		throw exception;
	}

	#ifdef __CYGWIN__
	#define DIR_PROP(name) \
		name.Set(ConvertCygwinPathToWindows( \
			mapConfig->GetKeyValue(#name)));
	#define DIR_PROP_SUBCONF(name, subconf) \
		name.Set(ConvertCygwinPathToWindows( \
			mapConfig->GetSubConfiguration(#subconf)->GetKeyValue(#name)));
	#else /* ! __CYGWIN__ */
	#define DIR_PROP(name) \
		name.SetFrom(mapConfig.get());
	#define DIR_PROP_SUBCONF(name, subconf) \
		name.SetFrom(&(mapConfig->GetSubConfiguration(#subconf)));
	#endif /* __CYGWIN__ */
	
	#define STR_PROP(name) \
		name.SetFrom(mapConfig.get());
	#define INT_PROP(name) \
		name.SetFrom(mapConfig.get());
	#define BOOL_PROP(name) \
		name.SetFrom(mapConfig.get());

	DIR_PROP(CertificateFile)
	DIR_PROP(PrivateKeyFile)
	DIR_PROP(DataDirectory)
	DIR_PROP(NotifyScript)
	DIR_PROP(TrustedCAsFile)
	DIR_PROP(KeysFile)
	DIR_PROP(SyncAllowScript)
	DIR_PROP(CommandSocket)
	DIR_PROP_SUBCONF(PidFile, Server)
	STR_PROP(StoreHostname)
	INT_PROP(AccountNumber)
	INT_PROP(UpdateStoreInterval)
	INT_PROP(MinimumFileAge)
	INT_PROP(MaxUploadWait)
	INT_PROP(FileTrackingSizeThreshold)
	INT_PROP(DiffingUploadSizeThreshold)
	INT_PROP(MaximumDiffingTime)
	BOOL_PROP(ExtendedLogging)
	
	#undef BOOL_PROP
	#undef INT_PROP
	#undef STR_PROP
	#undef DIR_PROP_SUBCONF
	#undef DIR_PROP

	mLocations = GetConfigurationLocations(*mapConfig);
	
	for (std::vector<Location>::iterator i = mLocations.begin();
		i != mLocations.end(); i++)
	{
		i->SetListener(this);
	}
	
	SetClean();
	NotifyListeners();
}	

void ClientConfig::AddLocation(const Location& rNewLoc) 
{
	mLocations.push_back(rNewLoc);
	NotifyListeners();
}

void ClientConfig::ReplaceLocation(int index, const Location& rNewLoc) 
{
	mLocations[index] = rNewLoc;
	NotifyListeners();
}

void ClientConfig::RemoveLocation(int target) {
	std::vector<Location>::iterator current = mLocations.begin();
	int i;
	for (i = 0; i < target; i++) {
		current++;
	}
	if (i == target)
	{
		mLocations.erase(current);
		NotifyListeners();
	}
}

void ClientConfig::RemoveLocation(const Location& rOldLocation) {
	std::vector<Location>::iterator current;
	
	for (current = mLocations.begin(); 
		current != mLocations.end() && !current->IsSameAs(rOldLocation); 
		current++)
		{ }
	
	if (current->IsSameAs(rOldLocation))
	{
		mLocations.erase(current);
		NotifyListeners();
	}
}

Location* ClientConfig::GetLocation(const Location& rConstLocation)
{
	for (std::vector<Location>::iterator pLoc = mLocations.begin();
		pLoc != mLocations.end(); pLoc++)
	{
		if (pLoc->IsSameAs(rConstLocation))
		{
			return &(*pLoc);
		}
	}
	return NULL;
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

/*
wxString ClientConfig::ConvertCygwinPathToWindows(const char *cygPath)
{
	wxFileName fn(wxString(cygPath, wxConvLibc), wxPATH_UNIX);
	fn.MakeAbsolute();
	wxString fs = fn.GetFullPath();
	
	if (fs.StartsWith(wxT("/cygdrive/")))
	{
		fn.RemoveDir(0);
		fs = fn.GetFullPath();
		wxString drive = fs.Mid(1, 1);
		fn.RemoveDir(0);
		fn.SetVolume(drive);
	}
	
	return fn.GetFullPath();
}

wxString ClientConfig::ConvertWindowsPathToCygwin(const char *winPath)
{
	wxFileName fn(wxString(winPath, wxConvLibc));
	fn.MakeAbsolute();
	wxString cygfile;
	
	cygfile.Printf(wxT("/cygdrive/%s/%s/"), fn.GetVolume().c_str(),
		fn.GetPath(0, wxPATH_UNIX).c_str());
	
	if (fn.HasName())
	{
		cygfile.Append(fn.GetName());
		cygfile.Append(wxT("/"));
	}
	
	if (fn.HasExt())
	{
		cygfile.Append(wxT("."));
		cygfile.Append(fn.GetExt());
	}
	
	return cygfile;
}
*/

bool ClientConfig::Save(const wxString& rConfigFileName) 
{
	wxTempFile file(rConfigFileName);
	
	if (!file.IsOpened()) {
		return false;
	}
	
	file.Write(
		wxT(
		"# This Box Backup configuration file was written by Boxi\n"
		"# This file is automatically generated. Do not edit by hand!\n"
		"\n"));
	
	wxString buffer;

	#define WRITE_STR_PROP(name) \
	if (const std::string * value = name.Get()) { \
		buffer.Printf(wxT(#name " = %s\n"), \
			wxString(value->c_str(), wxConvLibc).c_str()); \
		file.Write(buffer); \
	}

	#ifdef __CYGWIN__
	#define WRITE_DIR_PROP(name) \
	if (const std::string * value = name.Get()) { \
		wxString cygpath = ConvertWindowsPathToCygwin(value.c_str()); \
		buffer.Printf(wxT(#name " = %s\n"), \
			wxString(cygpath.c_str(), wxConvLibc).c_str()); \
		file.Write(buffer); \
	}
	#else /* ! __CYGWIN__ */
	#define WRITE_DIR_PROP(name) \
	if (const std::string * value = name.Get()) { \
		buffer.Printf(wxT(#name " = %s\n"), \
			wxString(value->c_str(), wxConvLibc).c_str()); \
		file.Write(buffer); \
	}
	#endif

	#define WRITE_INT_PROP_FMT(name, fmt) \
	if (const int * value = name.Get()) { \
		buffer.Printf(wxT(#name " = " fmt "\n"), *value); \
		file.Write(buffer); \
	}
	
	#define WRITE_INT_PROP(name) \
	if (const int * value = name.Get()) { \
		buffer.Printf(wxT(#name " = %d\n"), *value); \
		file.Write(buffer); \
	}

	#define WRITE_BOOL_PROP(name) \
	if (const bool * value = name.Get()) { \
		buffer.Printf(wxT(#name " = %s\n"), \
			*value ? wxT("yes") : wxT("no")); \
		file.Write(buffer); \
	}
	
	WRITE_STR_PROP(StoreHostname)
	WRITE_INT_PROP_FMT(AccountNumber, "0x%x")
	WRITE_DIR_PROP(KeysFile)
	WRITE_DIR_PROP(CertificateFile)
	WRITE_DIR_PROP(PrivateKeyFile)
	WRITE_DIR_PROP(TrustedCAsFile)
	WRITE_DIR_PROP(DataDirectory)
	WRITE_DIR_PROP(NotifyScript)
	WRITE_INT_PROP(UpdateStoreInterval)
	WRITE_INT_PROP(MinimumFileAge)
	WRITE_INT_PROP(MaxUploadWait)
	WRITE_INT_PROP(FileTrackingSizeThreshold)
	WRITE_INT_PROP(DiffingUploadSizeThreshold)
	WRITE_INT_PROP(MaximumDiffingTime)
	WRITE_BOOL_PROP(ExtendedLogging)
	WRITE_DIR_PROP(SyncAllowScript)
	WRITE_DIR_PROP(CommandSocket)

	#undef WRITE_STR_PROP
	#undef WRITE_INT_PROP
	#undef WRITE_INT_PROP_FMT
	#undef WRITE_BOOL_PROP
	
	if (PidFile.Get()) {
		wxString pidbuf;
		PidFile.GetInto(pidbuf);
		buffer.Printf(wxT("Server\n{\n\tPidFile = %s\n}\n"), pidbuf.c_str());
		file.Write(buffer);
	}
	
	const std::vector<Location>& rLocations = GetLocations();
	file.Write(wxT("BackupLocations\n{\n"));
	
	for (std::vector<Location>::const_iterator pLocation = rLocations.begin();
		pLocation != rLocations.end(); pLocation++) 
	{
		buffer.Printf(wxT("\t%s\n\t{\n\t\tPath = %s\n"),
			pLocation->GetName().c_str(), 
			pLocation->GetPath().c_str());
		file.Write(buffer);
		
		const std::vector<MyExcludeEntry>& rEntries = 
			pLocation->GetExcludeList().GetEntries();
		
		for (std::vector<MyExcludeEntry>::const_iterator pEntry = rEntries.begin();
			pEntry != rEntries.end(); pEntry++) {
			buffer.Printf(wxT("\t\t%s\n"), 
				wxString(pEntry->ToString().c_str(), wxConvLibc).c_str());
			file.Write(buffer);
		}
		
		file.Write(wxT("\t}\n"));
	}
	
	file.Write(wxT("}\n"));
	file.Commit();
	
	// clean configuration
	Load(rConfigFileName);
	
	return true;
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
	#define PROP(name) if (!name.IsClean()) return false;
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

	std::vector<Location> oldLocs;
	
	if (mapConfig.get())
	{
		oldLocs = GetConfigurationLocations(*mapConfig);
	}
	
	if (oldLocs.size() != mLocations.size())
	{
		return false;
	}
	
	for (size_t i = 0; i < mLocations.size(); i++) 
	{
		Location pOld = oldLocs[i];
		Location pNew = mLocations[i];
		if (!pNew.IsSameAs(pOld))
		{
			return false;
		}
	}
	
	return true;
}

void ClientConfig::OnPropertyChange(Property* pProp)
{
	NotifyListeners();
}

void ClientConfig::OnLocationChange(Location* pLocation)
{
	NotifyListeners();
}

void ClientConfig::OnExcludeListChange(MyExcludeList* pExcludeList)
{
	NotifyListeners();
}

bool PropSet(StringProperty& rProp, wxString& msg, const char* desc)
{
	if (rProp.Get())
		return true;
	wxString desc2(desc, wxConvLibc);
	msg.Printf(wxT("The %s is not set"), desc2.c_str());
	return false;
}

bool PropSet(IntProperty& rProp, wxString& msg, const char* desc)
{
	if (rProp.Get())
		return true;
	wxString desc2(desc, wxConvLibc);
	msg.Printf(wxT("The %s is not set"), desc2.c_str());
	return false;
}

bool PropSet(BoolProperty& rProp, wxString& msg, const char* desc)
{
	if (rProp.Get())
		return true;
	wxString desc2(desc, wxConvLibc);
	msg.Printf(wxT("The %s is not set"), desc2.c_str());
	return false;
}

bool PropPath(StringProperty& rProp, wxString& msg, bool dir, const char *desc)
{
	std::string path1;
	rProp.GetInto(path1);
	wxString path2(path1.c_str(), wxConvLibc);
	wxString desc2(desc, wxConvLibc);

	if (dir) 
	{
		if (wxFileName::FileExists(path2))
		{
			msg.Printf(wxT("The %s is not a directory (%s)"),
				desc2.c_str(), path2.c_str());
			return false;
		}
		if (!wxFileName::DirExists(path2))
		{
			msg.Printf(wxT("The %s does not exist (%s)"),
				desc2.c_str(), path2.c_str());
			return false;
		}
	} 
	else 
	{
		if (wxFileName::DirExists(path2))
		{
			msg.Printf(wxT("The %s is not a file (%s)"),
				desc2.c_str(), path2.c_str());
			return false;
		}
		if (!wxFileName::FileExists(path2))
		{
			msg.Printf(wxT("The %s does not exist (%s)"),
				desc2.c_str(), path2.c_str());
			return false;
		}
	}
	
	return true;
}

bool PropWritable(StringProperty& rProp, wxString& msg, const char *desc)
{
	std::string path1;
	rProp.GetInto(path1);
	wxString path2(path1.c_str(), wxConvLibc);
	
	if (wxFileName::FileExists(path2))
	{
		if (wxFile::Access(path2, wxFile::write))
			return true;
		msg.Printf(wxT("The %s cannot be written (%s)"), 
			desc, path1.c_str());
		return false;
	}
	else
	{
		wxFileName fn(path2);
		fn.SetName(wxT(""));
		fn.SetExt(wxT(""));
		wxString parentPath = fn.GetFullPath();
		if (!wxFileName::DirExists(parentPath)) {
			msg.Printf(
				wxT("The %s parent directory "
				"does not exist (%s)"),
				desc, parentPath.c_str());
			return false;
		}
		if (wxFile::Access(parentPath, wxFile::write))
			return true;
		msg.Printf(wxT("The %s parent directory "
				"cannot be written (%s)"), 
			desc, parentPath.c_str());
		return false;
	}

	return true;
}	

bool ClientConfig::Check(wxString& rMsg)
{
	if (!PropSet(StoreHostname,    rMsg, "Store Host"))
		return false;
	if (!PropSet(AccountNumber,    rMsg, "Account Number"))
		return false;
	if (!PropPath(KeysFile,        rMsg, false, "Keys File"))
		return false;
	if (!PropPath(CertificateFile, rMsg, false, "Certificate File"))
		return false;
	if (!PropPath(PrivateKeyFile,  rMsg, false, "Private Key File"))
		return false;
	if (!PropPath(TrustedCAsFile,  rMsg, false, "Trusted CAs File"))
		return false;
	if (!PropPath(DataDirectory,   rMsg, true, "Data Directory"))
		return false;
	if (PropSet(NotifyScript, rMsg, ""))
	{
		if (!PropPath(NotifyScript, rMsg, false, "Notify Script"))
			return false;
	}
	if (!PropSet(UpdateStoreInterval, rMsg, "Update Store Interval"))
		return false;
	if (!PropSet(MinimumFileAge, rMsg, "Minimum File Age"))
		return false;
	if (!PropSet(MaxUploadWait, rMsg, "Maximum Upload Wait"))
		return false;
	if (!PropSet(FileTrackingSizeThreshold, rMsg, 
		"File Tracking Size Threshold"))
		return false;
	if (!PropSet(DiffingUploadSizeThreshold, rMsg, 
		"Diffing Upload Size Threshold"))
		return false;
	if (!PropSet(MaximumDiffingTime, rMsg, "Maximum Diffing Time"))
		return false;
	if (!PropSet(MaxUploadWait, rMsg, "Maximum Upload Wait"))
		return false;
	if (PropSet(SyncAllowScript, rMsg, ""))
	{
		if (!PropPath(SyncAllowScript, rMsg, false, 
			"Sync Allow Script"))
			return false;
	}
	if (!PropWritable(CommandSocket, rMsg, "Command Socket"))
		return false;
	if (!PropWritable(PidFile, rMsg, "Process ID File"))
		return false;

	if (!CheckAccountNumber(&rMsg))
		return false;
	if (!CheckStoreHostname(&rMsg))
		return false;
	if (!CheckPrivateKeyFile(&rMsg))
		return false;
	if (!CheckCertificateFile(&rMsg))
		return false;
	if (!CheckTrustedCAsFile(&rMsg))
		return false;
	if (!CheckCertificateAndTrustedCAsPublicKeys(&rMsg))
		return false;
	if (!CheckCryptoKeysFile(&rMsg))
		return false;
	
	return true;
}

bool ClientConfig::CheckAccountNumber(wxString* pMsgOut)
{
	int account;

	if (! AccountNumber.GetInto(account))
	{
		*pMsgOut = wxT("The account number is empty or not valid");
		return false;
	}
	
	if (account <= 0)
	{
		*pMsgOut = wxT("The account number is zero or negative");
		return false;
	}
	
	if (account > 0x7FFFFFFF)
	{
		*pMsgOut = wxT("The account number is too large");
		return false;
	}
	
	return true;
}

bool ClientConfig::CheckStoreHostname(wxString* pMsgOut)
{
	wxString hostname;
	if (!StoreHostname.GetInto(hostname))
	{
		*pMsgOut = wxT("The store hostname is not configured");
		return false;
	}
	
	wxIPV4address addr;
	if (!addr.Hostname(hostname))
	{
		*pMsgOut = wxT("The store hostname is not valid");
		return false;
	}

	return true;
}

wxString FormatSslError(const wxString& rMsgBase)
{
	unsigned long err = ERR_get_error();
	wxString sslErrorMsg(ERR_error_string(err, NULL), wxConvLibc);
	
	wxString msg = rMsgBase;
	
	const char* reason = ERR_reason_error_string(err);
	if (reason != NULL)
	{
		msg.Append(wxT(": "));
		msg.Append(wxString(reason, wxConvLibc));
	}
	
	return msg;
}	

bool CheckExistingKeyWithBio(const wxString& rKeyFileName, BIO* pKeyBio, 
	wxString* pMsgOut);

bool ClientConfig::CheckPrivateKeyFile(wxString* pMsgOut)
{	
	wxString name;
	if (!PrivateKeyFile.GetInto(name))
	{
		*pMsgOut = _("The private key filename is not configured");
		return false;
	}
	
	BIO* pKeyBio = BIO_new(BIO_s_file());
	if (pKeyBio == NULL)
	{
		*pMsgOut = FormatSslError(_("Failed to create an OpenSSL BIO"));
		return false;
	}
	
	bool isOk = CheckExistingKeyWithBio(name, pKeyBio, pMsgOut);

	BIO_free(pKeyBio);
	
	return isOk;
}

bool CheckExistingKeyWithData(char* pName, char* pHeader,
	unsigned char* pData, wxString* pMsgOut);

bool CheckExistingKeyWithBio(const wxString& rKeyFileName, BIO* pKeyBio, 
	wxString* pMsgOut)
{
	wxCharBuffer buf = rKeyFileName.mb_str(wxConvLibc);
	if (BIO_read_filename(pKeyBio, buf.data()) <= 0)
	{
		*pMsgOut = FormatSslError(_("Failed to read key file"));
		return false;
	}
	
	char *pName = NULL, *pHeader = NULL;
	unsigned char *pData = NULL;
	long len;
	
	for (;;)
	{
		if (!PEM_read_bio(pKeyBio, &pName, &pHeader, &pData, &len)) 
		{
			if (ERR_GET_REASON(ERR_peek_error()) == PEM_R_NO_START_LINE)
			{
				// ERR_add_error_data(2, "Expecting: ", PEM_STRING_EVP_PKEY);
				*pMsgOut = FormatSslError(_("Failed to read key file"));
				return false;
			}
		}
		
		if (strcmp(pName, PEM_STRING_RSA) == 0)
			break;
		
		OPENSSL_free(pName);
		OPENSSL_free(pHeader);
		OPENSSL_free(pData);
	}
	
	bool isOk = CheckExistingKeyWithData(pName, pHeader, pData, pMsgOut);
	
	OPENSSL_free(pName);
	OPENSSL_free(pHeader);
	OPENSSL_free(pData);
	
	return isOk;
}

bool CheckExistingKeyWithData(char* pName, char* pHeader,
	unsigned char* pData, wxString* pMsgOut)
{
	EVP_CIPHER_INFO cipher;
	
	if (!PEM_get_EVP_CIPHER_INFO(pHeader, &cipher))
	{
		*pMsgOut = FormatSslError(_("Failed to decipher key file"));
		return false;
	}
	
	if (cipher.cipher != NULL)
	{
		*pMsgOut = wxT("This private key is protected with a passphrase. "
			"It cannot be used with Box Backup.");
		return false;
	}
	
	return true;
}

static wxString gVerifyError;

static int VerifyCertificateCallback(int ok, X509_STORE_CTX *pContext)
{
	if (ok) return ok;
	
	wxString errorMessage;
	
	if (pContext->current_cert)
	{
		char buf[256];
		X509_NAME_oneline(
			X509_get_subject_name(pContext->current_cert), 
			buf, sizeof(buf));
		errorMessage = wxString(buf, wxConvLibc);
		errorMessage.Append(_(": "));
	}	
	
	errorMessage.Append(wxString(
		X509_verify_cert_error_string(pContext->error), wxConvLibc));
	
	gVerifyError = errorMessage;
	return ok;
}

template<typename T, void free_T(T*)>
class AutoFree
{
	private:
	T* mpT;
	
	public:
	AutoFree ()      : mpT(NULL) { }
	AutoFree (T* pT) : mpT(pT) { }
	~AutoFree() { if (mpT) free_T(mpT); }
	T* get()    { return mpT;  }

	AutoFree (AutoFree& rSource)
	{
		mpT = rSource.mpT;
		rSource.mpT = NULL;
	}		

	AutoFree operator=(AutoFree& rSource) 
	{ 
		if (mpT) free_T(mpT);
		mpT = rSource.mpT;
		rSource.mpT = NULL;
	}
	
	class ref
	{
		private:
		AutoFree<T, free_T>& mrRef;
		public:
		ref(AutoFree<T, free_T>& rRef) : mrRef(rRef) { }
		friend class AutoFree<T, free_T>;
	};
	
	AutoFree(ref rRef)
	{
		mpT = rRef.mrRef.mpT;
		rRef.mrRef.mpT = NULL;
	}
	
	operator ref() { return ref(*this); }
};

static AutoFree<X509, X509_free> LoadCertificate(wxString rCertificateFileName,
	wxString* pMsgOut)
{
	AutoFree<X509, X509_free> error(NULL);
	
	AutoFree<BIO, &BIO_free_all> apCertBio(BIO_new(BIO_s_file()));
	if (!apCertBio.get())
	{
		*pMsgOut = FormatSslError(_("Failed to create an OpenSSL BIO"));
		return error;
	}
	
	if (BIO_read_filename(apCertBio.get(), 
		rCertificateFileName.mb_str(wxConvLibc).data()) <= 0)
	{
		wxString msg;
		msg.Printf(_("Failed to set the certificate file name (%s)"),
			rCertificateFileName.c_str());
		*pMsgOut = FormatSslError(msg);
		return error;
	}
	
	AutoFree<X509, X509_free> apCertToVerify(
		PEM_read_bio_X509_AUX(apCertBio.get(), NULL, NULL, NULL));
	if (!apCertToVerify.get())
	{
		wxString msg;
		msg.Printf(_("Failed to read the certificate file (%s)"),
			rCertificateFileName.c_str());
		*pMsgOut = FormatSslError(msg);
		return error;
	}
	
	return apCertToVerify;
}

static bool VerifyCertificate
(
	const wxString& rCertificateFileName, 
	const wxString& rIssuerCertFileName, 
	wxString* pMsgOut
)
{
	AutoFree<X509_STORE, X509_STORE_free> apStore(X509_STORE_new());
	
	if (!apStore.get())
	{
		*pMsgOut = FormatSslError(
			_("Failed to create an OpenSSL certificate store"));
		return false;
	}
	
	X509_STORE_set_verify_cb_func(apStore.get(), VerifyCertificateCallback);
	
	X509_LOOKUP* pLookup = X509_STORE_add_lookup(apStore.get(), 
		X509_LOOKUP_file());
	if (!pLookup)
	{
		*pMsgOut = FormatSslError(
			_("Failed to create an OpenSSL certificate lookup"));
		return false;
	}
	
	if (!X509_LOOKUP_load_file(pLookup, 
		rIssuerCertFileName.mb_str(wxConvLibc).data(), 
		X509_FILETYPE_PEM))
	{
		wxString msg;
		msg.Printf(_("Failed to load the expected issuer's "
			"certificate file (%s)"), rIssuerCertFileName.c_str());
		*pMsgOut = FormatSslError(msg);
		return false;
	}
	
	AutoFree<X509, X509_free> apCertToVerify = 
		LoadCertificate(rCertificateFileName, pMsgOut);
	
	if (!apCertToVerify.get())
	{
		return false;
	}
	
	AutoFree<X509_STORE_CTX, X509_STORE_CTX_free> apContext(
		X509_STORE_CTX_new());
	if (!apContext.get())
	{
		*pMsgOut = FormatSslError(_("Failed to create an OpenSSL "
			"X.509 context"));
		return false;
	}
	
	if (!X509_STORE_CTX_init(apContext.get(), apStore.get(), 
		apCertToVerify.get(), NULL))
	{
		*pMsgOut = FormatSslError(_("Failed to initialise the OpenSSL "
			"X.509 context"));
		return false;
	}
	
	if (!X509_verify_cert(apContext.get()))
	{
		wxString msg;
		msg.Printf(_("Certificate problem: %s (%s)"),
			gVerifyError.c_str(), rCertificateFileName.c_str());
		*pMsgOut = msg;
		return false;
	}

	return true;
}

bool CheckCertWithBio(const wxString& rCertFileName, 
	const wxString& rPrivateKeyFileName, int accountNumber, 
	BIO* pCertBio, wxString* pMsgOut);

bool ClientConfig::CheckCertificateFile(wxString* pMsgOut)
{
	wxString certFileName;
	if (!CertificateFile.GetInto(certFileName))
	{
		*pMsgOut =_("The certificate file name is not configured");
		return  false;
	}
	
	int accountNumber;
	if (!AccountNumber.GetInto(accountNumber))
	{
		*pMsgOut = _("The account number is not configured, "
			"but it should be!");
		return false;
	}

	wxString keyFileName;
	if (!PrivateKeyFile.GetInto(keyFileName))
	{
		*pMsgOut = _("The private key file is not configured, "
			"but it should be!");
		return false;
	}
	
	BIO* pCertBio = BIO_new(BIO_s_file());
	if (!pCertBio)
	{
		*pMsgOut = FormatSslError(_("Failed to create an OpenSSL BIO "
			"to read the certificate"));
		return false;
	}
	
	bool isOk = CheckCertWithBio(certFileName, keyFileName, 
		accountNumber, pCertBio, pMsgOut);
	
	BIO_free(pCertBio);
	
	return isOk;
}

bool CheckCertSubject(const wxString& rPrivateKeyFileName, int accountNumber, 
	X509* pCert, wxString* pMsgOut);

bool CheckCertWithBio(const wxString& rCertFileName, 
	const wxString& rPrivateKeyFileName, int accountNumber, 
	BIO* pCertBio, wxString* pMsgOut)
{
	wxCharBuffer buf = rCertFileName.mb_str(wxConvLibc);
	if (BIO_read_filename(pCertBio, buf.data()) <= 0)
	{
		*pMsgOut = FormatSslError(_("Failed to set the BIO filename to the "
			"certificate file"));
		return false;
	}
	
	X509* pCert = PEM_read_bio_X509(pCertBio, NULL, NULL, NULL);
	if (!pCert)
	{
		*pMsgOut = FormatSslError(_("Failed to read the certificate file"));
		return false;			
	}
	
	bool isOk = CheckCertSubject(rPrivateKeyFileName, accountNumber, 
		pCert, pMsgOut);
	
	X509_free(pCert);
	
	return isOk;
}

bool EnsureSamePublicKeys(EVP_PKEY* pActualKey, EVP_PKEY* pExpectKey,
	wxString* pMsgOut);

bool CheckCertSubject(const wxString& rPrivateKeyFileName, int accountNumber, 
	X509* pCert, wxString* pMsgOut)
{
	wxString commonNameActual;
	if (!GetCommonName(X509_get_subject_name(pCert), &commonNameActual, pMsgOut))
	{
		return false;
	}

	wxString commonNameExpected;
	commonNameExpected.Printf(wxT("BACKUP-%d"), accountNumber);

	if (!commonNameExpected.IsSameAs(commonNameActual))
	{
		wxString msg;
		msg.Printf(wxT("The certificate does not match the "
			"account number. The common name of the certificate "
			"should be '%s', but is actually '%s'."),
			commonNameExpected.c_str(), commonNameActual.c_str());
		*pMsgOut = msg;
		return false;
	}

	EVP_PKEY* pSubjectPublicKey = X509_get_pubkey(pCert);
	if (!pSubjectPublicKey)
	{
		*pMsgOut = FormatSslError(_("Failed to extract the subject's public key"
			"from the certificate"));
		return false;
	}
	
	EVP_PKEY* pMyPublicKey = LoadKey(rPrivateKeyFileName, pMsgOut);
	if (!pMyPublicKey)
	{
		EVP_PKEY_free(pSubjectPublicKey);
		return false;
	}
	
	bool isOk = EnsureSamePublicKeys(pSubjectPublicKey, pMyPublicKey, pMsgOut);
	
	EVP_PKEY_free(pMyPublicKey);
	EVP_PKEY_free(pSubjectPublicKey);

	return isOk;
}

bool EnsureSamePublicKeysWithBios(EVP_PKEY* pActualKey, 
	EVP_PKEY* pExpectKey, BIO* pActualBio, BIO* pExpectBio, wxString* pMsgOut);

bool EnsureSamePublicKeys(EVP_PKEY* pActualKey, EVP_PKEY* pExpectKey,
	wxString* pMsgOut)
{	
	BIO* pActualBio = BIO_new(BIO_s_mem());
	if (!pActualBio)
	{
		*pMsgOut = FormatSslError(_("Failed to allocate BIO for key data"));
		return false;
	}

	BIO* pExpectBio = BIO_new(BIO_s_mem());
	if (!pExpectBio)
	{
		*pMsgOut = FormatSslError(_("Failed to allocate BIO for key data"));
		BIO_free(pActualBio);
		return false;
	}
	
	bool isOk = EnsureSamePublicKeysWithBios(pActualKey, pExpectKey,
		pActualBio, pExpectBio, pMsgOut);
	
	BIO_free(pExpectBio);
	BIO_free(pActualBio);
	
	return isOk;
}

bool EnsureSamePublicKeysWithBuffers(BIO* pActualBio, BIO* pExpectBio,
	unsigned char* pActBuffer, unsigned char* pExpBuffer, int size,
	wxString* pMsgOut);

bool EnsureSamePublicKeysWithBios(EVP_PKEY* pActualKey, 
	EVP_PKEY* pExpectKey, BIO* pActualBio, BIO* pExpectBio, wxString* pMsgOut)
{
	if (!PEM_write_bio_PUBKEY(pActualBio, pActualKey))
	{
		*pMsgOut = FormatSslError(_("Failed to write public key to memory buffer"));
		return false;
	}

	if (!PEM_write_bio_PUBKEY(pExpectBio, pExpectKey))
	{
		*pMsgOut = FormatSslError(_("Failed to write public key to memory buffer"));
		return false;
	}
	
	int actSize = BIO_get_mem_data(pActualBio, NULL);
	int expSize = BIO_get_mem_data(pExpectBio, NULL);
	
	if (expSize != actSize)
	{
		*pMsgOut = wxT("The certificate file does not match "
			"your private key.");
		return false;
	}

	unsigned char* pExpBuffer = new unsigned char [expSize];
	if (!pExpBuffer)
	{
		*pMsgOut = wxT("Failed to allocate space for private key data");
		return false;
	}
	
	unsigned char* pActBuffer = new unsigned char [actSize];
	if (!pActBuffer)
	{
		*pMsgOut = wxT("Failed to allocate space for private key data");
		delete [] pExpBuffer;
		return false;
	}
		
	bool isOk = EnsureSamePublicKeysWithBuffers(pActualBio, pExpectBio,
		pActBuffer, pExpBuffer, actSize, pMsgOut);
	
	delete [] pActBuffer;
	delete [] pExpBuffer;

	return isOk;
}
	
bool EnsureSamePublicKeysWithBuffers(BIO* pActualBio, BIO* pExpectBio,
	unsigned char* pActBuffer, unsigned char* pExpBuffer, int size,
	wxString* pMsgOut)
{
	if (BIO_read(pActualBio, pActBuffer, size) != size)
	{
		*pMsgOut = FormatSslError(_("Failed to read enough key data"));
		return false;
	}
	
	if (BIO_read(pExpectBio, pExpBuffer, size) != size)
	{
		*pMsgOut = FormatSslError(_("Failed to read enough key data"));
		return false;
	}
	
	if (memcmp(pActBuffer, pExpBuffer, size) != 0)
	{
		*pMsgOut = wxT("The certificate file does not match your private key.");
		return false;
	}

	return true;
}

bool ClientConfig::CheckTrustedCAsFile(wxString* pMsgOut)
{
	wxString certFileName;
	if (!TrustedCAsFile.GetInto(certFileName))
	{
		*pMsgOut = _("The Trusted CAs file is not configured");
		return false;
	}
	
	return VerifyCertificate(certFileName, certFileName, pMsgOut);
}

bool ClientConfig::CheckCertificateAndTrustedCAsPublicKeys(wxString* pMsgOut)
{
	wxString certFile;
	if (!CertificateFile.GetInto(certFile))
	{
		*pMsgOut = _("The Certificate File is not configured");
		return false;
	}
	
	AutoFree<X509, X509_free> apLocalCert = 
		LoadCertificate(certFile, pMsgOut);
	if (!apLocalCert.get()) return false;

	wxString caFile;
	if (!TrustedCAsFile.GetInto(caFile))
	{
		*pMsgOut = _("The Trusted CAs File is not configured");
		return false;
	}
	
	AutoFree<X509, X509_free> apTrustedCaCert = 
		LoadCertificate(caFile, pMsgOut);
	if (!apTrustedCaCert.get()) return false;
	
	AutoFree<EVP_PKEY, EVP_PKEY_free> apLocalKey(
		X509_get_pubkey(apLocalCert.get()));
	if (!apLocalKey.get())
	{
		*pMsgOut = FormatSslError(_("Failed to get public key"));
		return false;
	}

	AutoFree<EVP_PKEY, EVP_PKEY_free> apTrustedKey(
		X509_get_pubkey(apTrustedCaCert.get()));
	if (!apTrustedKey.get())
	{
		*pMsgOut = FormatSslError(_("Failed to get public key"));
		return false;
	}

	if (!X509_verify(apLocalCert.get(), apTrustedKey.get()))
	{
		*pMsgOut = _("Our certificate and the Trusted CA's certificate "
			"do not match");
		return false;
	}
	
	return true;
}

std::auto_ptr<SslConfig> SslConfig::Get(wxString* pMsgOut)
{
	wxString configDir(X509_get_default_cert_area(), wxConvLibc);
	wxString configFileName = wxT("openssl.cnf");
	wxFileName configFile(configDir, configFileName);
	configFileName = configFile.GetFullPath();
	
	std::auto_ptr<SslConfig> result, empty;
	
	CONF* pConfig = NCONF_new(NULL);
	if (!pConfig)
	{
		*pMsgOut = FormatSslError(_("Failed to create an OpenSSL configuration object"));
		return result;
	}
	
	result = std::auto_ptr<SslConfig>(new SslConfig(pConfig, configFileName));
	// now the object will always be freed automatically
	
	long errline;

	wxCharBuffer buf = configFileName.mb_str(wxConvLibc);
	if (!NCONF_load(pConfig, buf.data(), &errline))
	{
		wxString msg;
		msg.Printf(_("Failed to initialise OpenSSL. Perhaps the "
			"configuration file could not be found or read? %s"),
			configFileName.c_str());
		*pMsgOut = FormatSslError(msg);
		return empty;
	}

	OPENSSL_load_builtin_modules();
	
	if (CONF_modules_load(pConfig, NULL, 0) <= 0)
	{
		*pMsgOut = FormatSslError(_("Failed to configure OpenSSL"));
		return empty;
	}

	return result;
}

bool GetCommonName(X509_NAME* pX509SubjectName, wxString* pTarget, 
	wxString* pMsgOut)
{
	int commonNameNid = OBJ_txt2nid("commonName");
	if (commonNameNid == NID_undef)
	{
		*pMsgOut = FormatSslError(_("Failed to find NID of "
			"X.509 CommonName attribute"));
		return false;
	}
	
	char commonNameBuf[256];

	X509_NAME_get_text_by_NID(pX509SubjectName, commonNameNid, 
		commonNameBuf, sizeof(commonNameBuf)-1);
	commonNameBuf[sizeof(commonNameBuf)-1] = 0;

	// X509_NAME_free(pX509SubjectName);
	
	*pTarget = wxString(commonNameBuf, wxConvLibc);
	return true;
}

EVP_PKEY* LoadKey(const wxString& rKeyFileName, wxString* pMsgOut)
{
	BIO *pKeyBio = BIO_new(BIO_s_file());
	if (!pKeyBio)
	{
		*pMsgOut = FormatSslError(_("Failed to create an OpenSSL BIO "
			"to read the private key"));
		return NULL;
	}
	
	if (BIO_read_filename(pKeyBio, 
		rKeyFileName.mb_str(wxConvLibc).data()) <= 0)
	{
		wxString msg;
		msg.Printf(_("Failed to read the private key file (%s)"),
			rKeyFileName.c_str());
		*pMsgOut = FormatSslError(msg);
		BIO_free(pKeyBio);
		return NULL;
	}
	
	EVP_PKEY* pKey = PEM_read_bio_PrivateKey(pKeyBio, NULL, NULL, NULL);
	BIO_free(pKeyBio);
	
	if (!pKey)
	{
		wxString msg;
		msg.Printf(_("Failed to read the private key (%s)"),
			rKeyFileName.c_str());
		*pMsgOut = FormatSslError(msg);
		return NULL;
	}
	
	return pKey;
}

void FreeKey(EVP_PKEY* pKey)
{
	EVP_PKEY_free(pKey);
}

bool ClientConfig::CheckCryptoKeysFile(wxString* pMsgOut)
{
	wxString keysFileString;
	if (!KeysFile.GetInto(keysFileString))
	{
		*pMsgOut = _("The Keys File is not configured");
		return false;
	}
	
	if (wxFileName::DirExists(keysFileString))
	{
		*pMsgOut = _("The Keys File is a directory.");
		return false;
	}

	if (!wxFileName::FileExists(keysFileString))
	{
		*pMsgOut = _("The Keys File was not found.");
		return false;
	}

	if (!wxFile::Access(keysFileString, wxFile::read))
	{
		*pMsgOut = _("The Keys File is not readable.");
		return false;
	}

	wxFile keysFile(keysFileString);
	if (keysFile.Length() != 1024)
	{
		*pMsgOut = _("The specified encryption key file "
			"is not valid (should be 1024 bytes long)");
		return false;
	}

	return true;

}
