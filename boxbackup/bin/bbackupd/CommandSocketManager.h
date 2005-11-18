// distribution boxbackup-0.09
// 
//  
// Copyright (c) 2003, 2004
//      Ben Summers.  All rights reserved.
//  
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. All use of this software and associated advertising materials must 
//    display the following acknowledgement:
//        This product includes software developed by Ben Summers.
// 4. The names of the Authors may not be used to endorse or promote
//    products derived from this software without specific prior written
//    permission.
// 
// [Where legally impermissible the Authors do not disclaim liability for 
// direct physical injury or death caused solely by defects in the software 
// unless it is modified by a third party.]
// 
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//  
//  
//  
// --------------------------------------------------------------------------
//
// File
//		Name:    CommandSocketManager.h
//		Purpose: Interface for managing command socket and processing
//			 client commands
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------

#ifndef COMMANDSOCKETMANAGER__H
#define COMMANDSOCKETMANAGER__H

#include "BoxTime.h"
#include "Socket.h"
#include "SocketListen.h"
#include "SocketStream.h"
#include "Configuration.h"

typedef enum
{
	// Add stuff to this, make sure the textual equivalents 
	// in BackupDaemon::SetState() are changed too.
	State_Initialising = -1,
	State_Idle = 0,
	State_Connected = 1,
	State_Error = 2,
	State_StorageLimitExceeded = 3
} state_t;

class IOStreamGetLine;

class CommandListener
{
	public:
	virtual ~CommandListener() { }
	virtual void SetReloadConfigWanted() = 0;
	virtual void SetTerminateWanted() = 0;
	virtual void SetSyncRequested() = 0;
	virtual void SetSyncForced() = 0;
};

class CommandSocketManager
{
public:
	CommandSocketManager(
		const Configuration& rConf, 
		CommandListener* pListener,
		const char * pSocketName);
	~CommandSocketManager();
	void Wait(box_time_t RequiredDelay);
	void CloseConnection();
	void SendSyncStartOrFinish(bool SendStart);
	void SendStateUpdate(state_t newState);

private:
	CommandSocketManager(const CommandSocketManager &);	// no copying
	CommandSocketManager &operator=(const CommandSocketManager &);
	SocketListen<SocketStream, 1 /* listen backlog */> mListeningSocket;
	std::auto_ptr<SocketStream> mpConnectedSocket;
	IOStreamGetLine *mpGetLine;
	CommandListener* mpListener;
	Configuration mConf;
	state_t mState;
};

#endif // COMMANDSOCKETMANAGER__H
