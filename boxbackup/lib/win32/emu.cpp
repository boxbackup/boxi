// Box Backup Win32 native port by Nick Knight

// Need at least 0x0500 to use GetFileSizeEx on Cygwin/MinGW
#define WINVER 0x0500

#include "Box.h"

#ifdef WIN32

// #include "emu.h"

#include <windows.h>
#include <fcntl.h>
// #include <atlenc.h>
#include <unistd.h>

#include <string>
#include <list>

//our implimentation for a timer
//based on a simple thread which sleeps for a
//period of time
static bool gFinishTimer;
static CRITICAL_SECTION gLock;

typedef struct 
{
	int countDown;
	int interval;
}
tTimer;

std::list<tTimer> gTimerList;
static void (__cdecl *gTimerFunc) (int) = NULL;

int setitimer(int type , struct itimerval *timeout, int)
{
	if ( SIGVTALRM == type || ITIMER_VIRTUAL == type )
	{
		EnterCriticalSection(&gLock);
		// we only need seconds for the mo!
		if (timeout->it_value.tv_sec  == 0 && 
		    timeout->it_value.tv_usec == 0)
		{
			gTimerList.clear();
		}
		else
		{
			tTimer ourTimer;
			ourTimer.countDown = timeout->it_value.tv_sec;
			ourTimer.interval  = timeout->it_interval.tv_sec;
			gTimerList.push_back(ourTimer);
		}
		LeaveCriticalSection(&gLock);
	}
	
	// indicate success
	return 0;
}

static unsigned int WINAPI RunTimer(LPVOID lpParameter)
{
	gFinishTimer = false;

	while (!gFinishTimer)
	{
		std::list<tTimer>::iterator it;
		EnterCriticalSection(&gLock);

		for (it = gTimerList.begin(); it != gTimerList.end(); it++)
		{
			tTimer& rTimer(*it);

			rTimer.countDown --;
			if (rTimer.countDown == 0)
			{
				if (gTimerFunc != NULL)
				{
					gTimerFunc(0);
				}
				if (rTimer.interval)
				{
					rTimer.countDown = rTimer.interval;
				}
				else
				{
					// mark for deletion
					rTimer.countDown = -1;
				}
			}
		}

		for (it = gTimerList.begin(); it != gTimerList.end(); it++)
		{
			tTimer& rTimer(*it);

			if (rTimer.countDown == -1)
			{
				gTimerList.erase(it);
				//if we don't do this the search is on a corrupt list
				it = gTimerList.begin();
			}
		}

		LeaveCriticalSection(&gLock);
		// we only need to have a 1 second resolution
		Sleep(1000);
	}

	return 0;
}

int SetTimerHandler(void (__cdecl *func ) (int))
{
	gTimerFunc = func;
	return 0;
}

void InitTimer(void)
{
	InitializeCriticalSection(&gLock);

	// create our thread
	HANDLE ourThread = (HANDLE)_beginthreadex(NULL, 0, RunTimer, 0, 
		CREATE_SUSPENDED, NULL);
	SetThreadPriority(ourThread, THREAD_PRIORITY_LOWEST);
	ResumeThread(ourThread);
}

void FiniTimer(void)
{
	gFinishTimer = true;
	EnterCriticalSection(&gLock);
	DeleteCriticalSection(&gLock);
}

//Our constants we need to keep track of
//globals
struct passwd gTempPasswd;

bool EnableBackupRights( void )
{
	HANDLE hToken;
	TOKEN_PRIVILEGES token_priv;

	//open current process to adjust privileges
	if( !OpenProcessToken( GetCurrentProcess( ), 
		TOKEN_ADJUST_PRIVILEGES, 
		&hToken ))
	{
		printf( "Cannot open process token - err = %d\n", GetLastError( ) );
		return false;
	}

	//let's build the token privilege struct - 
	//first, look up the LUID for the backup privilege

	if( !LookupPrivilegeValue( NULL, //this system
		SE_BACKUP_NAME, //the name of the privilege
		&( token_priv.Privileges[0].Luid )) ) //result
	{
		printf( "Cannot lookup backup privilege - err = %d\n", GetLastError( ) );
		return false;
	}

	token_priv.PrivilegeCount = 1;
	token_priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	// now set the privilege
	// because we're going exit right after dumping the streams, there isn't 
	// any need to save current state

	if( !AdjustTokenPrivileges( hToken, //our process token
		false,  //we're not disabling everything
		&token_priv, //address of structure
		sizeof( token_priv ), //size of structure
		NULL, NULL )) //don't save current state
	{
		//this function is a little tricky - if we were adjusting
		//more than one privilege, it could return success but not
		//adjust them all - in the general case, you need to trap this
		printf( "Could not enable backup privileges - err = %d\n", GetLastError( ) );
		return false;

	}
	else
	{
		return true;
	}
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    openfile
//		Purpose: replacement for any open calls - handles unicode filenames - supplied in utf8
//		Created: 25th October 2004
//
// --------------------------------------------------------------------------
HANDLE openfile(const char *filename, int flags, int mode)
{
	try{

		wchar_t *buffer;
		std::string fileN(filename);

		std::string tmpStr("\\\\?\\");
		//is the path relative or otherwise
		if ( fileN[1] != ':' )
		{
			//we need to get the current directory
			char wd[PATH_MAX];
			if(::getcwd(wd, PATH_MAX) == 0)
			{
				return NULL;
			}
			tmpStr += wd;
			if (tmpStr[tmpStr.length()] != '\\')
			{
				tmpStr += '\\';
			}
		}
		tmpStr += filename;

		int strlen = MultiByteToWideChar(
			CP_UTF8,               // code page
			0,                     // character-type options
			tmpStr.c_str(),        // string to map
			(int)tmpStr.length(),       // number of bytes in string
			NULL,                  // wide-character buffer
			0                      // size of buffer - work out how much space we need
			);

		buffer = new wchar_t[strlen+1];
		if ( buffer == NULL )
		{
			return NULL;
		}

		strlen = MultiByteToWideChar(
			CP_UTF8,               // code page
			0,                     // character-type options
			tmpStr.c_str(),        // string to map
			(int)tmpStr.length(),       // number of bytes in string
			buffer,                // wide-character buffer
			strlen                 // size of buffer
			);

		if ( strlen == 0 )
		{
			delete [] buffer;
			return NULL;
		}

		buffer[strlen] = L'\0';

		//flags could be O_WRONLY | O_CREAT | O_RDONLY
		DWORD createDisposition = OPEN_EXISTING;
		DWORD shareMode = FILE_SHARE_READ;
		DWORD accessRights = FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | FILE_READ_EA;

		if ( flags & O_WRONLY )
		{
			createDisposition = OPEN_EXISTING;
			shareMode |= FILE_SHARE_READ ;//| FILE_SHARE_WRITE;
		}
		if ( flags & O_CREAT )
		{
			createDisposition = OPEN_ALWAYS;
			shareMode |= FILE_SHARE_READ ;//| FILE_SHARE_WRITE;
			accessRights |= FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA | FILE_WRITE_EA | FILE_ALL_ACCESS;
		}
		if ( flags & O_TRUNC )
		{
			createDisposition = OPEN_ALWAYS;
		}

		HANDLE hdir = CreateFileW(buffer, 
			accessRights, 
			shareMode, 
			NULL, 
			createDisposition, 
			FILE_FLAG_BACKUP_SEMANTICS,
			NULL);

		if ( hdir == INVALID_HANDLE_VALUE )
		{
			// DWORD err = GetLastError();
			// syslog(EVENTLOG_WARNING_TYPE, "Couldn't open file %s, err %i\n", filename, err);
			delete [] buffer;
			return NULL;
		}

		delete [] buffer;
		return hdir;

	}
	catch(...)
	{
		printf("Caught openfile:%s\r\n", filename);
	}
	return NULL;

}

// MinGW provides a getopt implementation
#ifndef __MINGW32__
//works with getopt
char *optarg;
//optind looks like an index into the string - how far we have moved along
int optind = 1;
char nextchar = -1;
#endif

// --------------------------------------------------------------------------
//
// Function
//		Name:    ourfstat
//		Purpose: replacement for fstat supply a windows handle
//		Created: 25th October 2004
//
// --------------------------------------------------------------------------
int ourfstat(HANDLE hdir, struct stat * st)
{
	ULARGE_INTEGER conv;

	if (hdir == INVALID_HANDLE_VALUE)
	{
		::syslog(LOG_ERR, "Error: invalid file handle in ourfstat()");
		errno = EBADF;
		return -1;
	}

	BY_HANDLE_FILE_INFORMATION fi;
	if (!GetFileInformationByHandle(hdir, &fi))
	{
		::syslog(LOG_WARNING, "Failed to read file information: "
			"error %d", GetLastError());
		errno = EACCES;
		return -1;
	}

	// This next example is how we get our INODE (equivalent) information
	conv.HighPart = fi.nFileIndexHigh;
	conv.LowPart = fi.nFileIndexLow;
	st->st_ino = conv.QuadPart;

	// get the time information
	st->st_ctime = ConvertFileTimeToTime_t(&fi.ftCreationTime);
	st->st_atime = ConvertFileTimeToTime_t(&fi.ftLastAccessTime);
	st->st_mtime = ConvertFileTimeToTime_t(&fi.ftLastWriteTime);

	// size of the file
	LARGE_INTEGER st_size;
	if (!GetFileSizeEx(hdir, &st_size))
	{
		::syslog(LOG_WARNING, "Failed to get file size: error %d",
			GetLastError());
		errno = EACCES;
		return -1;
	}

	conv.HighPart = st_size.HighPart;
	conv.LowPart = st_size.LowPart;
	st->st_size = conv.QuadPart;

	//the mode of the file
	st->st_mode = 0;
	//DWORD res = GetFileAttributes((LPCSTR)tmpStr.c_str());

	if (INVALID_FILE_ATTRIBUTES != fi.dwFileAttributes)
	{
		if (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			st->st_mode |= S_IFDIR;
		}
		else
		{
			st->st_mode |= S_IFREG;
		}
	}
	else
	{
		::syslog(LOG_WARNING, "Failed to get file attributes: "
			"error %d", GetLastError());
		errno = EACCES;
		return -1;
	}

	return 0;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    OpenFileByNameUtf8
//		Purpose: Converts filename to Unicode and returns 
//			a handle to it. In case of error, sets errno,
//			logs the error and returns NULL.
//		Created: 10th December 2004
//
// --------------------------------------------------------------------------
HANDLE OpenFileByNameUtf8(const char* pName)
{
	//some string thing - required by ms to indicate long/unicode filename
	std::string tmpStr("\\\\?\\");

	// is the path relative or otherwise
	std::string fileN(pName);
	if (fileN[1] != ':')
	{
		// we need to get the current directory
		char wd[PATH_MAX];
		if(::getcwd(wd, PATH_MAX) == 0)
		{
			::syslog(LOG_WARNING, 
				"Failed to open '%s': path too long", pName);
			errno = ENAMETOOLONG;
			return NULL;
		}

		tmpStr += wd;
		if (tmpStr[tmpStr.length()] != '\\')
		{
			tmpStr += '\\';
		}
	}

	tmpStr += fileN;

	int strlen = MultiByteToWideChar(
		CP_UTF8,               // code page
		0,                     // character-type options
		tmpStr.c_str(),        // string to map
		(int)tmpStr.length(),  // number of bytes in string
		NULL,                  // wide-character buffer
		0                      // size of buffer - work out 
		                       //   how much space we need
		);

	wchar_t* buffer = new wchar_t[strlen+1];

	if (buffer == NULL)
	{
		::syslog(LOG_WARNING, 
			"Failed to open '%s': out of memory", pName);
		errno = ENOMEM;
		return NULL;
	}

	strlen = MultiByteToWideChar(
		CP_UTF8,               // code page
		0,                     // character-type options
		tmpStr.c_str(),        // string to map
		(int)tmpStr.length(),  // number of bytes in string
		buffer,                // wide-character buffer
		strlen                 // size of buffer
		);

	if (strlen == 0)
	{
		::syslog(LOG_WARNING, 
			"Failed to open '%s': could not convert "
			"file name to Unicode", pName);
		errno = EACCES;
		delete [] buffer;
		return NULL;
	}

	buffer[strlen] = L'\0';

	HANDLE handle = CreateFileW(buffer, 
		FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | FILE_READ_EA, 
		FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, 
		NULL, 
		OPEN_EXISTING, 
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL);

	if (handle == INVALID_HANDLE_VALUE)
	{
		// if our open fails we should always be able to 
		// open in this mode - to get the inode information
		// at least one process must have the file open - 
		// in this case someone else does.
		handle = CreateFileW(buffer, 
			0, 
			FILE_SHARE_READ, 
			NULL, 
			OPEN_EXISTING, 
			FILE_FLAG_BACKUP_SEMANTICS,
			NULL);
	}

	delete [] buffer;

	if (handle == INVALID_HANDLE_VALUE)
	{
		DWORD err = GetLastError();

		if (err == ERROR_FILE_NOT_FOUND)
		{
			::syslog(LOG_WARNING, 
				"Failed to open '%s': file not found", pName);
			errno = ENOENT;
		}
		else
		{
			::syslog(LOG_WARNING, 
				"Failed to open '%s': error %d", pName);
			errno = EACCES;
		}

		return NULL;
	}

	return handle;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    ourstat 
//		Purpose: replacement for the lstat and stat functions, 
//			works with unicode filenames supplied in utf8 format
//		Created: 25th October 2004
//
// --------------------------------------------------------------------------
int ourstat(const char * pName, struct stat * st)
{
	// at the mo
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_nlink = 1;

	HANDLE handle = OpenFileByNameUtf8(pName);

	if (handle == NULL)
	{
		// errno already set and error logged by OpenFileByNameUtf8()
		return -1;
	}

	int retVal = ourfstat(handle, st);
	if (retVal != 0)
	{
		// error logged, but without filename
		::syslog(LOG_WARNING, "Failed to get file information "
			"for '%s'", pName);
	}

	// close the handle
	CloseHandle(handle);

	return retVal;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    statfs
//		Purpose: returns the mount point of where a file is located - 
//			in this case the volume serial number
//		Created: 25th October 2004
//
// --------------------------------------------------------------------------
int statfs(const char * pName, struct statfs * s)
{
	HANDLE handle = OpenFileByNameUtf8(pName);

	if (handle == NULL)
	{
		// errno already set and error logged by OpenFileByNameUtf8()
		return -1;
	}

	BY_HANDLE_FILE_INFORMATION fi;
	if (!GetFileInformationByHandle(handle, &fi))
	{
		::syslog(LOG_WARNING, "Failed to get file information "
			"for '%s': error %d", pName, GetLastError());
		CloseHandle(handle);
		errno = EACCES;
		return -1;
	}

	// convert volume serial number to a string
	_ui64toa(fi.dwVolumeSerialNumber, s->f_mntonname + 1, 16);

	// pseudo unix mount point
	s->f_mntonname[0] = DIRECTORY_SEPARATOR_ASCHAR;

	CloseHandle(handle);   // close the handle

	return 0;
}





// MinGW provides opendir(), etc. via <dirent.h>
// MSVC does not provide these, so emulation is needed

#ifndef __MINGW32__
// --------------------------------------------------------------------------
//
// Function
//		Name:    opendir
//		Purpose: replacement for unix function, uses win32 findfirst routines
//		Created: 25th October 2004
//
// --------------------------------------------------------------------------
DIR *opendir(const char *name)
{
	try
	{
		DIR *dir = 0;
		std::string dirName(name);

		//append a '\' win32 findfirst is sensitive to this
		if ( dirName[dirName.size()] != '\\' || dirName[dirName.size()] != '/' )
		{
			dirName += '\\';
		}

		//what is the search string? - everything
		dirName += '*';

		if(name && name[0])
		{
			if ( ( dir = new DIR ) != 0 )
			{
				//mbstowcs(dir->name, dirName.c_str(),100);
				//wcscpy((wchar_t*)dir->name, (const wchar_t*)dirName.c_str());
				//mbstowcs(dir->name, dirName.c_str(), dirName.size()+1);
				//wchar_t *buffer;

				int strlen = MultiByteToWideChar(
					CP_UTF8,               // code page
					0,                     // character-type options
					dirName.c_str(),       // string to map
					(int)dirName.length(), // number of bytes in string
					NULL,                  // wide-character buffer
					0                      // size of buffer - work out how much space we need
					);

				dir->name = new wchar_t[strlen+1];

				if (dir->name == NULL)
				{
					delete dir;
					dir   = 0;
					errno = ENOMEM;
					return NULL;
				}

				strlen = MultiByteToWideChar(
					CP_UTF8,               // code page
					0,                     // character-type options
					dirName.c_str(),        // string to map
					(int)dirName.length(),       // number of bytes in string
					dir->name,                // wide-character buffer
					strlen                 // size of buffer
					);

				if (strlen == 0)
				{
					delete dir->name;
					delete dir;
					dir   = 0;
					errno = ENOMEM;
					return NULL;
				}

				dir->name[strlen] = L'\0';

				
				dir->fd = _wfindfirst(
					(const wchar_t*)dir->name,
					&dir->info);

				if (dir->fd != -1)
				{
					dir->result.d_name = 0;
				}
				else // go back
				{
					delete [] dir->name;
					delete dir;
					dir = 0;
				}
			}
			else // backwards again
			{
				delete dir;
				dir   = 0;
				errno = ENOMEM;
			}
		}
		else
		{
			errno = EINVAL;
		}

		return dir;

	}
	catch(...)
	{
		printf("Caught opendir");
	}

	return NULL;
}

//this kinda makes it not thread friendly!
//but I don't think it needs to be.
char tempbuff[MAX_PATH];
// --------------------------------------------------------------------------
//
// Function
//		Name:    readdir
//		Purpose: as function above
//		Created: 25th October 2004
//
// --------------------------------------------------------------------------
struct dirent *readdir(DIR *dp)
{
	try
	{
		struct dirent *den = NULL;

		if (dp && dp->fd != -1)
		{
			if (!dp->result.d_name || 
				_wfindnext(dp->fd, &dp->info) != -1)
			{
				den         = &dp->result;
				std::wstring input(dp->info.name);
				memset(tempbuff, 0, sizeof(tempbuff));
				WideCharToMultiByte(CP_UTF8, 0, dp->info.name, 
					-1, &tempbuff[0], sizeof (tempbuff), 
					NULL, NULL);
				//den->d_name = (char *)dp->info.name;
				den->d_name = &tempbuff[0];
			}
		}
		else
		{
			errno = EBADF;
		}
		return den;
	}
	catch (...)
	{
		printf("Caught readdir");
	}
	return NULL;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    closedir
//		Purpose: as function above
//		Created: 25th October 2004
//
// --------------------------------------------------------------------------
int closedir(DIR *dp)
{
	try
	{
		int finres = -1;
		if (dp)
		{
			if(dp->fd != -1)
			{
				finres = _findclose(dp->fd);
			}

			delete [] dp->name;
			delete dp;
		}

		if (finres == -1) // errors go to EBADF 
		{
			errno = EBADF;
		}

		return finres;
	}
	catch (...)
	{
		printf("Caught closedir");
	}
	return -1;
}
#endif // !__MINGW32__

// --------------------------------------------------------------------------
//
// Function
//		Name:    poll
//		Purpose: a weak implimentation (just enough for box) 
//			of the unix poll for winsock2
//		Created: 25th October 2004
//
// --------------------------------------------------------------------------
int poll (struct pollfd *ufds, unsigned long nfds, int timeout)
{
	try
	{
		fd_set readfd;
		fd_set writefd;

		readfd.fd_count = 0;
		writefd.fd_count = 0;

		struct pollfd *ufdsTmp = ufds;

		timeval timOut;
		timeval *tmpptr; 

		if (timeout == INFTIM)
			tmpptr = NULL;
		else
			tmpptr = &timOut;

		timOut.tv_sec  = timeout / 1000;
		timOut.tv_usec = timeout * 1000;

		if (ufds->events & POLLIN)
		{
			for (unsigned long i = 0; i < nfds; i++)
			{
				readfd.fd_array[i] = ufdsTmp->fd;
				readfd.fd_count++;
			}
		}

		if (ufds->events & POLLOUT)
		{
			for (unsigned long i = 0; i < nfds; i++)
			{

				writefd.fd_array[i]=ufdsTmp->fd;
				writefd.fd_count++;
			}
		}	

		int noffds = select(0, &readfd, &writefd, 0, tmpptr);

		if (noffds == SOCKET_ERROR)
		{
			// int errval = WSAGetLastError();

			ufdsTmp = ufds;
			for (unsigned long i = 0; i < nfds; i++)
			{
				ufdsTmp->revents = POLLERR;
				ufdsTmp++;
			}
			return (-1);
		}

		return noffds;
	}
	catch (...)
	{
		printf("Caught poll");
	}

	return -1;
}

HANDLE gSyslogH = 0;

void syslog(int loglevel, const char *frmt, ...)
{
	DWORD errinfo;
	char* buffer;
	std::string sixfour(frmt);

	switch (loglevel)
	{
	case LOG_INFO:
		errinfo = EVENTLOG_INFORMATION_TYPE;
		break;
	case LOG_ERR:
		errinfo = EVENTLOG_ERROR_TYPE;
		break;
	case LOG_WARNING:
		errinfo = EVENTLOG_WARNING_TYPE;
		break;
	default:
		errinfo = EVENTLOG_WARNING_TYPE;
		break;
	}


	//taken from MSDN
	try
	{


		int sixfourpos;
		while ( ( sixfourpos = sixfour.find("%ll")) != -1 )
		{
			//maintain portability - change the 64 bit formater...
			std::string temp = sixfour.substr(0,sixfourpos);
			temp += "%I64";
			temp += sixfour.substr(sixfourpos+3, sixfour.length());
			sixfour = temp;
		}

		//printf("parsed string is:%s\r\n", sixfour.c_str());

		va_list args;
		va_start(args, frmt);

#ifdef __MINGW32__
		// no _vscprintf, use a fixed size buffer
		buffer = new char[1024];
		int len = 1023;
#else
		int len = _vscprintf( sixfour.c_str(), args );
		ASSERT(len > 0)

		len = len + 1;
		char* buffer = new char[len];
#endif

		ASSERT(buffer)
		memset(buffer, 0, len);

		int len2 = vsnprintf(buffer, len, sixfour.c_str(), args);
		ASSERT(len2 <= len);

		va_end(args);
	}
	catch (...)
	{
		printf("Caught syslog: %s", sixfour.c_str());
		return;
	}

	try
	{

		if (!ReportEvent(gSyslogH,    // event log handle 
			errinfo,              // event type 
			0,                    // category zero 
			MSG_ERR_EXIST,	      // event identifier - 
			                      // we will call them all the same
			NULL,                 // no user security identifier 
			1,                    // one substitution string 
			0,                    // no data 
			(LPCSTR*)&buffer,     // pointer to string array 
			NULL))                // pointer to data 

		{
			DWORD err = GetLastError();
			printf("Unable to send message to Event Log "
				"(error %i):\r\n", err);
		}

		printf("%s\r\n", buffer);

		if (buffer) delete [] buffer;
	}
	catch (...)
	{
		printf("Caught syslog ReportEvent");
	}
}

#endif // WIN32
