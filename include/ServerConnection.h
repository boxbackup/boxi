/***************************************************************************
 *            ServerConnection.h
 *
 *  Mon Mar 28 20:51:58 2005
 *  Copyright 2005 Chris Wilson
 *  Email <boxi_ServerConnection.h@qwirx.com>
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
 
#ifndef _SERVERCONNECTION_H
#define _SERVERCONNECTION_H

#define NDEBUG
#include "SSLLib.h"
#include "Socket.h"
#include "SocketStreamTLS.h"
#include "TLSContext.h"
#include "autogen_BackupProtocolClient.h"
#include "BackupStoreDirectory.h"
#undef NDEBUG

#include "ClientConfig.h"

enum RestoreState {
	RS_UNKNOWN = 0,
	RS_START_FILE,
	RS_FINISH_FILE,
};

typedef void (ProgressCallbackFunc)(RestoreState State, 
	std::string& rFileName, void* userData);


class ServerConnection {
	public:
	ServerConnection(ClientConfig* pConfig);
	~ServerConnection();
	
	bool Connect(bool Writable);
	void Disconnect();
	bool IsConnected() { return mIsConnected; }
	
	int Restore(int64_t DirectoryID, 
		const char *LocalDirectoryName, 
		ProgressCallbackFunc *pProgressCallback, 
		void* pProgressUserData, 
		bool RestoreDeleted, 
		bool UndeleteAfterRestoreDeleted, 
		bool Resume);
	
	bool GetFile(
		int64_t parentDirectoryId,
		int64_t theFileId,
		const char * destFileName);

	bool UndeleteDirectory(int64_t theDirectoryId);
	bool DeleteDirectory(int64_t theDirectoryId);
	
	BackupProtocolClientAccountUsage* GetAccountUsage();
	
	bool ListDirectory(int64_t theDirectoryId, int16_t excludeFlags, 
		BackupStoreDirectory& rDirectoryObject);

	private:
	wxString mErrorMessage;
	void HandleException(const wxString& when, BoxException& e);

	public:
	const char * ErrorString() 
	{
		int type, subtype;
		mpConnection->GetLastError(type, subtype);
		return ErrorString(type, subtype);
	}
	
	const wxString& GetErrorMessage() { return mErrorMessage; }
	
	const char * ErrorString(int type, int subtype);

	private:
	bool                    mIsConnected;
	bool					mIsWritable;
	SocketStreamTLS*        mpSocket;
	BackupProtocolClient*   mpConnection;
	ClientConfig*           mpConfig;
	
	bool Connect2(bool Writable);
};

#endif /* _SERVERCONNECTION_H */
