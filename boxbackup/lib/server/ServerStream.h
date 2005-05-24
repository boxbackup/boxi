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
//		Name:    ServerStream.h
//		Purpose: Stream based server daemons
//		Created: 2003/07/31
//
// --------------------------------------------------------------------------

#ifndef SERVERSTREAM__H
#define SERVERSTREAM__H

#include <syslog.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>

#include "Daemon.h"
#include "SocketListen.h"
#include "Utils.h"
#include "Configuration.h"
#include "WaitForEvent.h"

#include "MemLeakFindOn.h"

// --------------------------------------------------------------------------
//
// Class
//		Name:    ServerStream
//		Purpose: Stream based server daemon
//		Created: 2003/07/31
//
// --------------------------------------------------------------------------
template<typename StreamType, int Port, int ListenBacklog = 128, bool ForkToHandleRequests = true>
class ServerStream : public Daemon
{
public:
	ServerStream()
	{
	}
	~ServerStream()
	{
		DeleteSockets();
	}
private:
	ServerStream(const ServerStream &rToCopy)
	{
	}
public:

	virtual const char *DaemonName() const
	{
		return "generic-stream-server";
	}

	virtual void Run()
	{
		// Set process title as appropraite
		SetProcessTitle(ForkToHandleRequests?"server":"idle");
	
		// Handle exceptions and child task quitting gracefully.
		bool childExit = false;
		try
		{
			Run2(childExit);
		}
		catch(BoxException &e)
		{
			if(childExit)
			{
				::syslog(LOG_ERR, "in server child, exception %s (%d/%d) -- terminating child", e.what(), e.GetType(), e.GetSubType());
				_exit(1);
			}
			else throw;
		}
		catch(std::exception &e)
		{
			if(childExit)
			{
				::syslog(LOG_ERR, "in server child, exception %s -- terminating child", e.what());
				_exit(1);
			}
			else throw;
		}
		catch(...)
		{
			if(childExit)
			{
				::syslog(LOG_ERR, "in server child, unknown exception -- terminating child");
				_exit(1);
			}
			else throw;
		}

		// if it's a child fork, exit the process now
		if(childExit)
		{
			// Child task, dump leaks to trace, which we make sure is on
			#ifdef BOX_MEMORY_LEAK_TESTING
				#ifndef NDEBUG
					TRACE_TO_SYSLOG(true);
					TRACE_TO_STDOUT(true);
				#endif
				memleakfinder_traceblocksinsection();
			#endif

			// If this is a child quitting, exit now to stop bad things happening
			_exit(0);
		}
	}
	
	virtual void Run2(bool &rChildExit)
	{
		try
		{
			// Wait object with a timeout of 10 seconds, which is a reasonable time to wait before
			// cleaning up finished child processes.
			WaitForEvent connectionWait(10000);
			
			// BLOCK
			{
				// Get the address we need to bind to
				// this-> in next line required to build under some gcc versions
				const Configuration &config(this->GetConfiguration());
				const Configuration &server(config.GetSubConfiguration("Server"));
				std::string addrs = server.GetKeyValue("ListenAddresses");
	
				// split up the list of addresses
				std::vector<std::string> addrlist;
				SplitString(addrs, ',', addrlist);
	
				for(unsigned int a = 0; a < addrlist.size(); ++a)
				{
					// split the address up into components
					std::vector<std::string> c;
					SplitString(addrlist[a], ':', c);
	
					// listen!
					SocketListen<StreamType, ListenBacklog> *psocket = new SocketListen<StreamType, ListenBacklog>;
					try
					{
						if(c[0] == "inet")
						{
							// Check arguments
							if(c.size() != 2 && c.size() != 3)
							{
								THROW_EXCEPTION(ServerException, ServerStreamBadListenAddrs)
							}
							
							// Which port?
							int port = Port;
							
							if(c.size() == 3)
							{
								// Convert to number
								port = ::atol(c[2].c_str());
								if(port <= 0 || port > ((64*1024)-1))
								{
									THROW_EXCEPTION(ServerException, ServerStreamBadListenAddrs)
								}
							}
							
							// Listen
							psocket->Listen(Socket::TypeINET, c[1].c_str(), port);
						}
						else if(c[0] == "unix")
						{
							// Check arguments size
							if(c.size() != 2)
							{
								THROW_EXCEPTION(ServerException, ServerStreamBadListenAddrs)
							}

							// unlink anything there
							::unlink(c[1].c_str());
							
							psocket->Listen(Socket::TypeUNIX, c[1].c_str());
						}
						else
						{
							delete psocket;
							THROW_EXCEPTION(ServerException, ServerStreamBadListenAddrs)
						}
						
						// Add to list of sockets
						mSockets.push_back(psocket);
					}
					catch(...)
					{
						delete psocket;
						throw;
					}

					// Add to the list of things to wait on
					connectionWait.Add(psocket);
				}
			}
	
			while(!StopRun())
			{
				// Wait for a connection, or timeout
				SocketListen<StreamType, ListenBacklog> *psocket
					= (SocketListen<StreamType, ListenBacklog> *)connectionWait.Wait();

				if(psocket)
				{
					// Get the incomming connection (with zero wait time)
					std::string logMessage;
					std::auto_ptr<StreamType> connection(psocket->Accept(0, &logMessage));

					// Was there one (there should be...)
					if(connection.get())
					{
						// Since this is a template parameter, the if() will be optimised out by the compiler
						if(ForkToHandleRequests)
						{
							pid_t pid = ::fork();
							switch(pid)
							{
							case -1:
								// Error!
								THROW_EXCEPTION(ServerException, ServerForkError)
								break;
								
							case 0:
								// Child process
								rChildExit = true;
								// Close listening sockets
								DeleteSockets();
								
								// Set up daemon
								EnterChild();
								SetProcessTitle("transaction");
								
								// Memory leak test the forked process
								#ifdef BOX_MEMORY_LEAK_TESTING
									memleakfinder_startsectionmonitor();
								#endif
								
								// The derived class does some server magic with the connection
								HandleConnection(*connection);
								// Since rChildExit == true, the forked process will call _exit() on return from this fn
								return;
			
							default:
								// parent daemon process
								break;
							}
							
							// Log it
							::syslog(LOG_INFO, "%s (handling in child %d)", logMessage.c_str(), pid);
						}
						else
						{
							// Just handle in this connection
							SetProcessTitle("handling");
							HandleConnection(*connection);
							SetProcessTitle("idle");										
						}
					}
				}
				
				// Clean up child processes (if forking daemon)
				if(ForkToHandleRequests)
				{
					int status = 0;
					int p = 0;
					do
					{
						if((p = ::waitpid(0 /* any child in process group */, &status, WNOHANG)) == -1
							&& errno != ECHILD && errno != EINTR)
						{
							THROW_EXCEPTION(ServerException, ServerWaitOnChildError)
						}
					} while(p > 0);
				}
			}
		}
		catch(...)
		{
			DeleteSockets();
			throw;
		}
		
		// Delete the sockets
		DeleteSockets();
	}

	virtual void HandleConnection(StreamType &rStream)
	{
		Connection(rStream);
	}

	virtual void Connection(StreamType &rStream) = 0;
	
protected:
	// For checking code in dervied classes -- use if you have an algorithm which
	// depends on the forking model in case someone changes it later.
	bool WillForkToHandleRequests()
	{
		return ForkToHandleRequests;
	}

private:
	// --------------------------------------------------------------------------
	//
	// Function
	//		Name:    ServerStream::DeleteSockets()
	//		Purpose: Delete sockets
	//		Created: 9/3/04
	//
	// --------------------------------------------------------------------------
	void DeleteSockets()
	{
		for(unsigned int l = 0; l < mSockets.size(); ++l)
		{
			if(mSockets[l])
			{
				mSockets[l]->Close();
				delete mSockets[l];
			}
			mSockets[l] = 0;
		}
		mSockets.clear();
	}

private:
	std::vector<SocketListen<StreamType, ListenBacklog> *> mSockets;
};

#define SERVERSTREAM_VERIFY_SERVER_KEYS(DEFAULT_ADDRESSES) \
											{"ListenAddresses", DEFAULT_ADDRESSES, 0, 0}, \
											DAEMON_VERIFY_SERVER_KEYS 

#include "MemLeakFindOff.h"

#endif // SERVERSTREAM__H



