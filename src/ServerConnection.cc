/***************************************************************************
 *            ServerConnection.cc
 *
 *  Mon Mar 28 20:56:20 2005
 *  Copyright  2005 Chris Wilson
 *  Email <boxi_ServerConnection.c@qwirx.com>
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
 
#include <wx/wx.h>

#include "Box.h"
#include "BackupClientCryptoKeys.h"
#include "BackupStoreConstants.h"
#include "BackupStoreFile.h"
#include "BoxException.h"
#include "BoxPortsAndFiles.h"

#include "ServerConnection.h"

ServerConnection::ServerConnection(ClientConfig* pConfig)
{
	mpConfig = pConfig;
	mpSocket = NULL;
	mpConnection = NULL;
	mIsConnected = FALSE;
	mIsWritable = FALSE;
}

ServerConnection::~ServerConnection()
{
	if (mIsConnected) Disconnect();
}

void ServerConnection::HandleException(const char * when, BoxException& e)
{
	wxString msg = when;
	msg.append(": ");
	
	int type = 0, subtype = 0;
	
	if (mpConnection != NULL)
		mpConnection->GetLastError(type, subtype);
	
	if (type == 1000) {
		msg.append(ErrorString(type, subtype)); 
	} else {
		msg.Append(e.what());
	}
	
	wxMessageBox(msg, "Boxi Error", wxOK | wxICON_ERROR, NULL);

	if (e.GetType()    == ConnectionException::ExceptionType &&
		e.GetSubType() == ConnectionException::TLSReadFailed) 
	{
		Disconnect();
	}
}

bool ServerConnection::Connect(bool Writable) 
{
	if (mIsConnected && mIsWritable == Writable) return TRUE;
	
	if (mIsConnected) Disconnect();
	
	bool result;

	try {
		result = Connect2(Writable);
	} catch (BoxException &e) {
		HandleException("Error connecting to server", e);
		return FALSE;
	}

	if (result) {
		mIsConnected = TRUE;
		mIsWritable = Writable;
	} else {
		mIsConnected = FALSE;
		mIsWritable  = FALSE;
		wxString msg = "Error connecting to server: ";
		msg.Append(GetErrorMessage());
		wxLogDebug(msg);
	}
	
	return result;
}

bool ServerConnection::Connect2(bool Writable) {
	std::string certFile;
	mpConfig->CertificateFile.GetInto(certFile);

	if (certFile.length() == 0) {
		mErrorMessage = "You have not configured the Certificate File!";
		return FALSE;
	}

	std::string keysFile;
	mpConfig->KeysFile.GetInto(keysFile);

	if (keysFile.length() == 0) {
		mErrorMessage = "You have not configured the Keys File!";
		return FALSE;
	}

	std::string privKeyFile;
	mpConfig->PrivateKeyFile.GetInto(privKeyFile);

	if (privKeyFile.length() == 0) {
		mErrorMessage = "You have not configured the Private Key File!";
		return FALSE;
	}

	std::string caFile;
	mpConfig->TrustedCAsFile.GetInto(caFile);

	if (caFile.length() == 0) {
		mErrorMessage = "You have not configured the Trusted CAs File!";
		return FALSE;
	}

	std::string storeHost;
	mpConfig->StoreHostname.GetInto(storeHost);

	if (storeHost.length() == 0) {
		mErrorMessage = "You have not configured the Store Hostname!";
		return FALSE;
	}

	TLSContext tlsContext;

	try {	
		tlsContext.Initialise(false /* as client */, certFile.c_str(),
			privKeyFile.c_str(), caFile.c_str());
	} catch (BoxException &e) {
		mErrorMessage = "There is something wrong with your Certificate "
			"File, Private Key File, or Trusted CAs File. (";
		mErrorMessage.Append(e.what());
		mErrorMessage.Append(")");
		return FALSE;
	}
	
	// Initialise keys
	BackupClientCryptoKeys_Setup(keysFile.c_str());

	// 2. Connect to server
	mpSocket = new SocketStreamTLS();
	int storePort = BOX_PORT_BBSTORED; 
	mpSocket->Open(tlsContext, Socket::TypeINET, storeHost.c_str(), storePort);
	
	// 3. Make a protocol, and handshake
	mpConnection = new BackupProtocolClient(*mpSocket);
	mpConnection->Handshake();
	
	// Check the version of the server
	{
		std::auto_ptr<BackupProtocolClientVersion> serverVersion(
			mpConnection->QueryVersion(BACKUP_STORE_SERVER_VERSION));
		
		if (serverVersion->GetVersion() != BACKUP_STORE_SERVER_VERSION)
		{
			mErrorMessage.Printf("Wrong server version: "
				"expected %d but found %d", BACKUP_STORE_SERVER_VERSION, 
				serverVersion->GetVersion());
			return FALSE;
		}
	}

	// Login -- if this fails, the Protocol will exception
	int acctNo;
	mpConfig->AccountNumber.GetInto(acctNo);
	mpConnection->QueryLogin(acctNo, 
		Writable ? 0 : BackupProtocolClientLogin::Flags_ReadOnly);

	return TRUE;
}

void ServerConnection::Disconnect() {
	if (mpConnection != NULL) {
		try {
			mpConnection->QueryFinished();
		} catch (BoxException &e) {
			// ignore exceptions - may be disconnected from server
		}
		delete mpConnection;
		mpConnection = NULL;
	}
	
	if (mpSocket != NULL) {
		try {
			mpSocket->Close();
		} catch (BoxException &e) {
			// ignore exceptions - may be disconnected from server
		}
		delete mpSocket;
		mpSocket = NULL;
	}
	
	mIsConnected = FALSE;
}

bool ServerConnection::GetFile(
	int64_t parentDirectoryId,
	int64_t theFileId,
	const char * destFileName)
{
	if (!Connect(FALSE)) return FALSE;
	
	try 
	{
		mpConnection->QueryGetFile(parentDirectoryId, theFileId);
	
		// Stream containing encoded file
		std::auto_ptr<IOStream> objectStream(mpConnection->ReceiveStream());
		
		// Decode it
		BackupStoreFile::DecodeFile(*objectStream, destFileName, 
			mpConnection->GetTimeout());
		
		return TRUE;
	}
	catch (BoxException& e) 
	{
		HandleException("Error retrieving file from server", e);
		return FALSE;
	}	
}

bool ServerConnection::ListDirectory(
	int64_t theDirectoryId, 
	int16_t excludeFlags, 
	BackupStoreDirectory& rDirectoryObject) 
{
	if (!Connect(FALSE)) return FALSE;

	try {
		mpConnection->QueryListDirectory(
			theDirectoryId,
			// both files and directories:
			BackupProtocolClientListDirectory::Flags_INCLUDE_EVERYTHING,
			excludeFlags,
			// want attributes:
			TRUE);

		// Retrieve the directory from the stream following
		std::auto_ptr<IOStream> dirstream(mpConnection->ReceiveStream());
		
		rDirectoryObject.ReadFromStream(*dirstream, mpConnection->GetTimeout());	
		
		return TRUE;
	} 
	catch (BoxException& e) 
	{
		HandleException("Error listing directory on server", e);
		return FALSE;
	}
}

BackupProtocolClientAccountUsage* ServerConnection::GetAccountUsage() {
	if (!Connect(FALSE)) return FALSE;

	try {
		std::auto_ptr<BackupProtocolClientAccountUsage> Usage =
			mpConnection->QueryGetAccountUsage();
		return new BackupProtocolClientAccountUsage(*Usage);
	} catch (BoxException &e) {
		HandleException("Error getting account information from server", e);
		return NULL;
	}
}

bool ServerConnection::UndeleteDirectory(int64_t theDirectoryId) {
	if (!Connect(TRUE)) return FALSE;

	try {
		mpConnection->QueryUndeleteDirectory(theDirectoryId);
		return TRUE;
	} catch (BoxException &e) {
		HandleException("Error undeleting directory on server", e);
		return FALSE;
	}
}

bool ServerConnection::DeleteDirectory(int64_t theDirectoryId) {
	if (!Connect(TRUE)) return FALSE;

	try {
		mpConnection->QueryDeleteDirectory(theDirectoryId);
		return TRUE;
	} catch (BoxException &e) {
		HandleException("Error deleting directory on server", e);
		return FALSE;
	}
}

const char * ServerConnection::ErrorString(int type, int subtype) {
	if (type == 1000) {
		switch (subtype) {
		case BackupProtocolClientError::Err_WrongVersion:
			return "Wrong version";
		case BackupProtocolClientError::Err_NotInRightProtocolPhase:	
			return "Wrong protocol phase";
		case BackupProtocolClientError::Err_BadLogin:					
			return "Bad login";
		case BackupProtocolClientError::Err_CannotLockStoreForWriting:	
			return "Cannot lock store for writing";
		case BackupProtocolClientError::Err_SessionReadOnly:			
			return "Session is read-only";
		case BackupProtocolClientError::Err_FileDoesNotVerify:			
			return "File verification failed";
		case BackupProtocolClientError::Err_DoesNotExist:				
			return "Object does not exist";
		case BackupProtocolClientError::Err_DirectoryAlreadyExists:	
			return "Directory already exists";
		case BackupProtocolClientError::Err_CannotDeleteRoot:			
			return "Cannot delete root directory";
		case BackupProtocolClientError::Err_TargetNameExists:			
			return "Target name exists";
		case BackupProtocolClientError::Err_StorageLimitExceeded:		
			return "Storage limit exceeded";
		case BackupProtocolClientError::Err_DiffFromFileDoesNotExist:	
			return "Diff From File does not exist";
		case BackupProtocolClientError::Err_DoesNotExistInDirectory:	
			return "Does not exist in directory";
		case BackupProtocolClientError::Err_PatchConsistencyError:		
			return "Patch consistency error";
		default:
			mErrorMessage.Printf("Unknown protocol error: %d/%d", 
				type, subtype);
			return mErrorMessage.c_str();
		}
	} else {
		mErrorMessage.Printf("Unknown error: %d/%d", type, subtype);
		return mErrorMessage.c_str();
	}
}
