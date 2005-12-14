// --------------------------------------------------------------------------
//
// File
//		Name:    Test.h
//		Purpose: Useful stuff for tests
//		Created: 2003/07/11
//
// --------------------------------------------------------------------------

#ifndef TEST__H
#define TEST__H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <stdio.h>
 
extern int failures;

#define TEST_FAIL_WITH_MESSAGE(msg) {failures++; printf("FAILURE: " msg " at " __FILE__ "(%d)\n", __LINE__);}
#define TEST_ABORT_WITH_MESSAGE(msg) {failures++; printf("FAILURE: " msg " at " __FILE__ "(%d)\n", __LINE__); return 1;}

#define TEST_THAT(condition) {if(!(condition)) TEST_FAIL_WITH_MESSAGE("Condition [" #condition "] failed")}
#define TEST_THAT_ABORTONFAIL(condition) {if(!(condition)) TEST_ABORT_WITH_MESSAGE("Condition [" #condition "] failed")}

// NOTE: The 0- bit it to allow this to work with stuff which has negative constants for flags (eg ConnectionException)
#define TEST_CHECK_THROWS(statement, excepttype, subtype)									\
	{																						\
		bool didthrow = false;																\
		try																					\
		{																					\
			statement;																		\
		}																					\
		catch(excepttype &e)																\
		{																					\
			if(e.GetSubType() != ((unsigned int)excepttype::subtype)						\
					&& e.GetSubType() != (unsigned int)(0-excepttype::subtype)) 			\
			{																				\
				throw;																		\
			}																				\
			didthrow = true;																\
		}																					\
		catch(...)																			\
		{																					\
			throw;																			\
		}																					\
		if(!didthrow)																		\
		{																					\
			TEST_FAIL_WITH_MESSAGE("Didn't throw exception " #excepttype "(" #subtype ")")	\
		}																					\
	}

inline bool TestFileExists(const char *Filename)
{
	struct stat st;
	return ::stat(Filename, &st) == 0 && (st.st_mode & S_IFDIR) == 0;
}

inline bool TestDirExists(const char *Filename)
{
	struct stat st;
	return ::stat(Filename, &st) == 0 && (st.st_mode & S_IFDIR) == S_IFDIR;
}

// -1 if doesn't exist
inline int TestGetFileSize(const char *Filename)
{
	struct stat st;
	if(::stat(Filename, &st) == 0)
	{
		return st.st_size;
	}
	return -1;
}

inline int LaunchServer(const char *CommandLine, const char *pidFile)
{
	if(::system(CommandLine) != 0)
	{
		printf("Server: %s\n", CommandLine);
		TEST_FAIL_WITH_MESSAGE("Couldn't start server");
		return -1;
	}
	// time for it to start up
	::sleep(1);
	
	// read pid file
	if(!TestFileExists(pidFile))
	{
		printf("Server: %s\n", CommandLine);
		TEST_FAIL_WITH_MESSAGE("Server didn't save PID file");	
		return -1;
	}
	
	FILE *f = fopen(pidFile, "r");
	int pid = -1;
	if(f == NULL || fscanf(f, "%d", &pid) != 1)
	{
		printf("Server: %s (pidfile %s)\n", CommandLine, pidFile);
		TEST_FAIL_WITH_MESSAGE("Couldn't read PID file");	
		return -1;
	}
	fclose(f);
	
	return pid;
}

#ifdef WIN32

#include "WinNamedPipeStream.h"
#include "IOStreamGetLine.h"
#include "BoxPortsAndFiles.h"

bool SendCommands(const std::string& rCmd)
{
	WinNamedPipeStream connection;

	try
	{
		connection.Connect(BOX_NAMED_PIPE_NAME);
	}
	catch(...)
	{
		printf("Failed to connect to daemon control socket.\n");
		return false;
	}

	// For receiving data
	IOStreamGetLine getLine(connection);
	
	// Wait for the configuration summary
	std::string configSummary;
	if(!getLine.GetLine(configSummary))
	{
		printf("Failed to receive configuration summary from daemon\n");
		return false;
	}

	// Was the connection rejected by the server?
	if(getLine.IsEOF())
	{
		printf("Server rejected the connection.\n");
		return false;
	}

	// Decode it
	int autoBackup, updateStoreInterval, minimumFileAge, maxUploadWait;
	if(::sscanf(configSummary.c_str(), "bbackupd: %d %d %d %d", 
			&autoBackup, &updateStoreInterval, 
			&minimumFileAge, &maxUploadWait) != 4)
	{
		printf("Config summary didn't decode\n");
		return false;
	}

	std::string cmds;
	bool expectResponse;

	if (rCmd != "")
	{
		cmds = rCmd;
		cmds += "\nquit\n";
		expectResponse = true;
	}
	else
	{
		cmds = "quit\n";
		expectResponse = false;
	}
	
	connection.Write(cmds.c_str(), cmds.size());
	
	// Read the response
	std::string line;
	bool statusOk = !expectResponse;

	while (expectResponse && !getLine.IsEOF() && getLine.GetLine(line))
	{
		// Is this an OK or error line?
		if (line == "ok")
		{
			statusOk = true;
		}
		else if (line == "error")
		{
			printf("ERROR (%s)\n", rCmd.c_str());
			break;
		}
		else
		{
			printf("WARNING: Unexpected response to command '%s': "
				"%s", rCmd.c_str(), line.c_str());
		}
	}
	
	return statusOk;
}

inline bool ServerIsAlive()
{
	return SendCommands("");
}

inline bool HUPServer(int pid)
{
	return SendCommands("reload");
}

inline bool KillServer(int pid)
{
	TEST_THAT(SendCommands("terminate"));
	::sleep(1);
	return !ServerIsAlive();
}

#else // !WIN32

inline bool ServerIsAlive(int pid)
{
	if(pid == 0) return false;
	return ::kill(pid, 0) != -1;
}

inline bool HUPServer(int pid)
{
	if(pid == 0) return false;
	return ::kill(pid, SIGHUP) != -1;
}

inline bool KillServer(int pid)
{
	if(pid == 0 || pid == -1) return false;
	bool KilledOK = ::kill(pid, SIGTERM) != -1;
	TEST_THAT(KilledOK);
	::sleep(1);
	return !ServerIsAlive(pid);
}

#endif // WIN32

inline void TestRemoteProcessMemLeaks(const char *filename)
{
#ifdef BOX_MEMORY_LEAK_TESTING
	// Does the file exist?
	if(!TestFileExists(filename))
	{
		++failures;
		printf("FAILURE: MemLeak report not available (file %s)\n", filename);
	}
	else
	{
		// Is it empty?
		if(TestGetFileSize(filename) > 0)
		{
			++failures;
			printf("FAILURE: Memory leaks found in other process (file %s)\n==========\n", filename);
			FILE *f = fopen(filename, "r");
			char line[512];
			while(::fgets(line, sizeof(line), f) != 0)
			{
				printf("%s", line);
			}
			fclose(f);
			printf("==========\n");
		}
		
		// Delete it
		::unlink(filename);
	}
#endif
}

#endif // TEST__H

