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
//		Name:    SocketListen.h
//		Purpose: Stream based sockets for servers
//		Created: 2003/07/31
//
// --------------------------------------------------------------------------

#ifndef SOCKETLISTEN__H
#define SOCKETLISTEN__H

#include <errno.h>
#include <unistd.h>
#include <new>
#include <poll.h>
#include <memory>
#include <string>
#ifndef PLATFORM_KQUEUE_NOT_SUPPORTED
	#include <sys/event.h>
	#include <sys/time.h>
#endif

#include "Socket.h"
#include "ServerException.h"

#include "MemLeakFindOn.h"

// --------------------------------------------------------------------------
//
// Class
//		Name:    _NoSocketLocking
//		Purpose: Default locking class for SocketListen
//		Created: 2003/07/31
//
// --------------------------------------------------------------------------
class _NoSocketLocking
{
public:
	_NoSocketLocking(int sock)
	{
	}
	
	~_NoSocketLocking()
	{
	}
	
	bool HaveLock()
	{
		return true;
	}
	
private:
	_NoSocketLocking(const _NoSocketLocking &rToCopy)
	{
	}
};


// --------------------------------------------------------------------------
//
// Class
//		Name:    SocketListen
//		Purpose: 
//		Created: 2003/07/31
//
// --------------------------------------------------------------------------
template<typename SocketType, int ListenBacklog = 128, typename SocketLockingType = _NoSocketLocking, int MaxMultiListenSockets = 16>
class SocketListen
{
public:
	// Initialise
	SocketListen()
		: mSocketHandle(-1)
	{
	}
	// Close socket nicely
	~SocketListen()
	{
		Close();
	}
private:
	SocketListen(const SocketListen &rToCopy)
	{
	}
public:

	enum
	{
		MaxMultipleListenSockets = MaxMultiListenSockets
	};

	void Close()
	{
		if(mSocketHandle != -1)
		{
			if(::close(mSocketHandle) == -1)
			{
				THROW_EXCEPTION(ServerException, SocketCloseError)
			}
		}
		mSocketHandle = -1;
	}

	// --------------------------------------------------------------------------
	//
	// Function
	//		Name:    SocketListen::Listen(int, char*, int)
	//		Purpose: Initialises, starts the socket listening.
	//		Created: 2003/07/31
	//
	// --------------------------------------------------------------------------
	void Listen(int Type, const char *Name, int Port = 0)
	{
		if(mSocketHandle != -1) {THROW_EXCEPTION(ServerException, SocketAlreadyOpen)}
		
		// Setup parameters based on type, looking up names if required
		int sockDomain = 0;
		SocketAllAddr addr;
		int addrLen = 0;
		Socket::NameLookupToSockAddr(addr, sockDomain, Type, Name, Port, addrLen);
	
		// Create the socket
		mSocketHandle = ::socket(sockDomain, SOCK_STREAM, 0 /* let OS choose protocol */);
		if(mSocketHandle == -1)
		{
			THROW_EXCEPTION(ServerException, SocketOpenError)
		}
		
		// Set an option to allow reuse (useful for -HUP situations!)
		int option = true;
		if(::setsockopt(mSocketHandle, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1)
		{
			THROW_EXCEPTION(ServerException, SocketOpenError)
		}

		// Bind it to the right port, and start listening
		if(::bind(mSocketHandle, &addr.sa_generic, addrLen) == -1
			|| ::listen(mSocketHandle, ListenBacklog) == -1)
		{
			// Dispose of the socket
			::close(mSocketHandle);
			mSocketHandle = -1;
			THROW_EXCEPTION(ServerException, SocketBindError)
		}	
	}
	
	// --------------------------------------------------------------------------
	//
	// Function
	//		Name:    SocketListen::Accept(int)
	//		Purpose: Accepts a connection, returning a pointer to a class of
	//				 the specified type. May return a null pointer if a signal happens,
	//				 or there's a timeout. Timeout specified in milliseconds, defaults to infinite time.
	//		Created: 2003/07/31
	//
	// --------------------------------------------------------------------------
	std::auto_ptr<SocketType> Accept(int Timeout = INFTIM, std::string *pLogMsg = 0)
	{
		if(mSocketHandle == -1) {THROW_EXCEPTION(ServerException, BadSocketHandle)}
		
		// Do the accept, using the supplied locking type
		int sock;
		struct sockaddr addr;
		socklen_t addrlen = sizeof(addr);
		// BLOCK
		{
			SocketLockingType socklock(mSocketHandle);
			
			if(!socklock.HaveLock())
			{
				// Didn't get the lock for some reason. Wait a while, then
				// return nothing.
				::sleep(1);
				return std::auto_ptr<SocketType>();
			}
			
			// poll this socket
			struct pollfd p;
			p.fd = mSocketHandle;
			p.events = POLLIN;
			p.revents = 0;
			switch(::poll(&p, 1, Timeout))
			{
			case -1:
				// signal?
				if(errno == EINTR)
				{
					// return nothing
					return std::auto_ptr<SocketType>();
				}
				else
				{
					THROW_EXCEPTION(ServerException, SocketPollError)
				}
				break;
			case 0:	// timed out
				return std::auto_ptr<SocketType>();
				break;
			default:	// got some thing...
				// control flows on...
				break;
			}
			
			sock = ::accept(mSocketHandle, &addr, &addrlen);
		}
		// Got socket (or error), unlock (implcit in destruction)
		if(sock == -1)
		{
			THROW_EXCEPTION(ServerException, SocketAcceptError)
		}

		// Log it
		if(pLogMsg)
		{
			*pLogMsg = Socket::IncomingConnectionLogMessage(&addr, addrlen);
		}
		else
		{
			// Do logging ourselves
			Socket::LogIncomingConnection(&addr, addrlen);
		}

		return std::auto_ptr<SocketType>(new SocketType(sock));
	}
	
	// Functions to allow adding to WaitForEvent class, for efficient waiting
	// on multiple sockets.
#ifndef PLATFORM_KQUEUE_NOT_SUPPORTED
	// --------------------------------------------------------------------------
	//
	// Function
	//		Name:    SocketListen::FillInKEevent
	//		Purpose: Fills in a kevent structure for this socket
	//		Created: 9/3/04
	//
	// --------------------------------------------------------------------------
	void FillInKEvent(struct kevent &rEvent, int Flags = 0) const
	{
		EV_SET(&rEvent, mSocketHandle, EVFILT_READ, 0, 0, 0, (void*)this);
	}
#else
	// --------------------------------------------------------------------------
	//
	// Function
	//		Name:    SocketListen::FillInPoll
	//		Purpose: Fills in the data necessary for a poll operation
	//		Created: 9/3/04
	//
	// --------------------------------------------------------------------------
	void FillInPoll(int &fd, short &events, int Flags = 0) const
	{
		fd = mSocketHandle;
		events = POLLIN;
	}
#endif
	
private:
	int mSocketHandle;
};

#include "MemLeakFindOff.h"

#endif // SOCKETLISTEN__H

