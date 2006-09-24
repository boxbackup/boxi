// Box Backup Win32 native port by Nick Knight

// Need at least 0x0500 to use GetFileSizeEx on Cygwin/MinGW
#define WINVER 0x0500

#include "emu.h"

#ifdef WIN32

#include <assert.h>
#include <fcntl.h>
#include <windows.h>

#ifdef __MINGW32__
	#include <unistd.h>
#endif
#include <process.h>

#include <string>
#include <list>

// message resource definitions for syslog()

#include "messages.h"

// our implementation for a timer, based on a 
// simple thread which sleeps for a period of time

static bool gFinishTimer;
static CRITICAL_SECTION gLock;

typedef struct 
{
	int countDown;
	int interval;
}
Timer_t;

std::list<Timer_t> gTimerList;
static void (__cdecl *gTimerFunc) (int) = NULL;

int setitimer(int type, struct itimerval *timeout, void *arg)
{
	if (ITIMER_VIRTUAL == type)
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
			Timer_t ourTimer;
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
		std::list<Timer_t>::iterator it;
		EnterCriticalSection(&gLock);

		for (it = gTimerList.begin(); it != gTimerList.end(); it++)
		{
			Timer_t& rTimer(*it);

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
			Timer_t& rTimer(*it);

			if (rTimer.countDown == -1)
			{
				gTimerList.erase(it);
				
				// the iterator is now invalid, so restart search
				it = gTimerList.begin();

				// if the list is now empty, don't try to increment 
				// the iterator again
				if (it == gTimerList.end()) break;
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
		printf( "Cannot open process token: error %d\n", 
			(int)GetLastError() );
		return false;
	}

	//let's build the token privilege struct - 
	//first, look up the LUID for the backup privilege

	if( !LookupPrivilegeValue( NULL, //this system
		SE_BACKUP_NAME, //the name of the privilege
		&( token_priv.Privileges[0].Luid )) ) //result
	{
		printf( "Cannot lookup backup privilege: error %d\n", 
			(int)GetLastError( ) );
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
		printf( "Could not enable backup privileges: error %d\n", 
			(int)GetLastError( ) );
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
//		Name:    ConvertToWideString
//		Purpose: Converts a string from specified codepage to 
//			 a wide string (WCHAR*). Returns a buffer which 
//			 MUST be freed by the caller with delete[].
//			 In case of fire, logs the error and returns NULL.
//		Created: 4th February 2006
//
// --------------------------------------------------------------------------
WCHAR* ConvertToWideString(const char* pString, unsigned int codepage)
{
	int len = MultiByteToWideChar
	(
		codepage, // source code page
		0,        // character-type options
		pString,  // string to map
		-1,       // number of bytes in string - auto detect
		NULL,     // wide-character buffer
		0         // size of buffer - work out 
		          //   how much space we need
	);

	if (len == 0)
	{
		::syslog(LOG_WARNING, 
			"Failed to convert string to wide string: "
			"error %d", GetLastError());
		errno = EINVAL;
		return NULL;
	}

	WCHAR* buffer = new WCHAR[len];

	if (buffer == NULL)
	{
		::syslog(LOG_WARNING, 
			"Failed to convert string to wide string: "
			"out of memory");
		errno = ENOMEM;
		return NULL;
	}

	len = MultiByteToWideChar
	(
		codepage, // source code page
		0,        // character-type options
		pString,  // string to map
		-1,       // number of bytes in string - auto detect
		buffer,   // wide-character buffer
		len       // size of buffer
	);

	if (len == 0)
	{
		::syslog(LOG_WARNING, 
			"Failed to convert string to wide string: "
			"error %i", GetLastError());
		errno = EACCES;
		delete [] buffer;
		return NULL;
	}

	return buffer;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    ConvertUtf8ToWideString
//		Purpose: Converts a string from UTF-8 to a wide string.
//			 Returns a buffer which MUST be freed by the caller 
//			 with delete[].
//			 In case of fire, logs the error and returns NULL.
//		Created: 4th February 2006
//
// --------------------------------------------------------------------------
WCHAR* ConvertUtf8ToWideString(const char* pString)
{
	return ConvertToWideString(pString, CP_UTF8);
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    ConvertFromWideString
//		Purpose: Converts a wide string to a narrow string in the
//			 specified code page. Returns a buffer which MUST 
//			 be freed by the caller with delete[].
//			 In case of fire, logs the error and returns NULL.
//		Created: 4th February 2006
//
// --------------------------------------------------------------------------
char* ConvertFromWideString(const WCHAR* pString, unsigned int codepage)
{
	int len = WideCharToMultiByte
	(
		codepage, // destination code page
		0,        // character-type options
		pString,  // string to map
		-1,       // number of bytes in string - auto detect
		NULL,     // output buffer
		0,        // size of buffer - work out 
		          //   how much space we need
		NULL,     // replace unknown chars with system default
		NULL      // don't tell us when that happened
	);

	if (len == 0)
	{
		::syslog(LOG_WARNING, 
			"Failed to convert wide string to narrow: "
			"error %d", GetLastError());
		errno = EINVAL;
		return NULL;
	}

	char* buffer = new char[len];

	if (buffer == NULL)
	{
		::syslog(LOG_WARNING, 
			"Failed to convert wide string to narrow: "
			"out of memory");
		errno = ENOMEM;
		return NULL;
	}

	len = WideCharToMultiByte
	(
		codepage, // source code page
		0,        // character-type options
		pString,  // string to map
		-1,       // number of bytes in string - auto detect
		buffer,   // output buffer
		len,      // size of buffer
		NULL,     // replace unknown chars with system default
		NULL      // don't tell us when that happened
	);

	if (len == 0)
	{
		::syslog(LOG_WARNING, 
			"Failed to convert wide string to narrow: "
			"error %i", GetLastError());
		errno = EACCES;
		delete [] buffer;
		return NULL;
	}

	return buffer;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    ConvertUtf8ToConsole
//		Purpose: Converts a string from UTF-8 to the console 
//			 code page. On success, replaces contents of rDest 
//			 and returns true. In case of fire, logs the error 
//			 and returns false.
//		Created: 4th February 2006
//
// --------------------------------------------------------------------------
bool ConvertUtf8ToConsole(const char* pString, std::string& rDest)
{
	WCHAR* pWide = ConvertUtf8ToWideString(pString);
	if (pWide == NULL)
	{
		return false;
	}

	char* pConsole = ConvertFromWideString(pWide, GetConsoleOutputCP());
	delete [] pWide;

	if (!pConsole)
	{
		return false;
	}

	rDest = pConsole;
	delete [] pConsole;

	return true;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    ConvertConsoleToUtf8
//		Purpose: Converts a string from the console code page
//			 to UTF-8. On success, replaces contents of rDest
//			 and returns true. In case of fire, logs the error 
//			 and returns false.
//		Created: 4th February 2006
//
// --------------------------------------------------------------------------
bool ConvertConsoleToUtf8(const char* pString, std::string& rDest)
{
	WCHAR* pWide = ConvertToWideString(pString, GetConsoleCP());
	if (pWide == NULL)
	{
		return false;
	}

	char* pConsole = ConvertFromWideString(pWide, CP_UTF8);
	delete [] pWide;

	if (!pConsole)
	{
		return false;
	}

	rDest = pConsole;
	delete [] pConsole;

	return true;
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    ConvertPathToAbsoluteUnicode
//		Purpose: Converts relative paths to absolute (with unicode marker)
//		Created: 4th February 2006
//
// --------------------------------------------------------------------------
std::string ConvertPathToAbsoluteUnicode(const char *pFileName)
{
	std::string filename;
	for (int i = 0; pFileName[i] != 0; i++)
	{
		if (pFileName[i] == '/')
		{
			filename += '\\';
		}
		else
		{
			filename += pFileName[i];
		}
	}

	std::string tmpStr("\\\\?\\");
	
	// Is the path relative or absolute?
	// Absolute paths on Windows are always a drive letter
	// followed by ':'
	
	if (filename[1] != ':')
	{
		// Must be relative. We need to get the 
		// current directory to make it absolute.
		
		char wd[PATH_MAX];
		if (::getcwd(wd, PATH_MAX) == 0)
		{
			::syslog(LOG_WARNING, 
				"Failed to open '%s': path too long", 
				pFileName);
			errno = ENAMETOOLONG;
			tmpStr = "";
			return tmpStr;
		}
		
		tmpStr += wd;
		if (tmpStr[tmpStr.length()] != '\\')
		{
			tmpStr += '\\';
		}
	}
	
	tmpStr += filename;
	return tmpStr;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    openfile
//		Purpose: replacement for any open calls - handles unicode 
//			filenames - supplied in utf8
//		Created: 25th October 2004
//
// --------------------------------------------------------------------------
HANDLE openfile(const char *pFileName, int flags, int mode)
{
	std::string AbsPathWithUnicode = 
		ConvertPathToAbsoluteUnicode(pFileName);
	
	if (AbsPathWithUnicode.size() == 0)
	{
		// error already logged by ConvertPathToAbsoluteUnicode()
		return INVALID_HANDLE_VALUE;
	}
	
	WCHAR* pBuffer = ConvertUtf8ToWideString(AbsPathWithUnicode.c_str());
	// We are responsible for freeing pBuffer
	
	if (pBuffer == NULL)
	{
		// error already logged by ConvertUtf8ToWideString()
		return INVALID_HANDLE_VALUE;
	}

	// flags could be O_WRONLY | O_CREAT | O_RDONLY
	DWORD createDisposition = OPEN_EXISTING;
	DWORD shareMode = FILE_SHARE_READ;
	DWORD accessRights = FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | FILE_READ_EA;

	if (flags & O_WRONLY)
	{
		accessRights = FILE_WRITE_DATA;
		shareMode = FILE_SHARE_WRITE;
	}
	else if (flags & (O_RDWR | O_CREAT))
	{
		accessRights |= FILE_WRITE_ATTRIBUTES 
			| FILE_WRITE_DATA | FILE_WRITE_EA;
		shareMode |= FILE_SHARE_WRITE;
	}

	if (flags & O_CREAT)
	{
		createDisposition = OPEN_ALWAYS;
	}
	if (flags & O_TRUNC)
	{
		createDisposition = CREATE_ALWAYS;
	}
	if (flags & O_EXCL)
	{
		shareMode = 0;
	}

	HANDLE hdir = CreateFileW(pBuffer, 
		accessRights, 
		shareMode, 
		NULL, 
		createDisposition, 
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL);
	
	delete [] pBuffer;

	if (hdir == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_INVALID_NAME)
		{
			::syslog(LOG_WARNING, "Failed to open file '%s': "
				"invalid file name", pFileName);
		}
		else
		{
			::syslog(LOG_WARNING, "Failed to open file '%s': "
				"error %i", pFileName, GetLastError());
		}
		return INVALID_HANDLE_VALUE;
	}

	return hdir;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    emu_fstat
//		Purpose: replacement for fstat supply a windows handle
//		Created: 25th October 2004
//
// --------------------------------------------------------------------------
int emu_fstat(HANDLE hdir, struct stat * st)
{
	if (hdir == INVALID_HANDLE_VALUE)
	{
		::syslog(LOG_ERR, "Error: invalid file handle in emu_fstat()");
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

	if (INVALID_FILE_ATTRIBUTES == fi.dwFileAttributes)
	{
		::syslog(LOG_WARNING, "Failed to get file attributes: "
			"error %d", GetLastError());
		errno = EACCES;
		return -1;
	}

	memset(st, 0, sizeof(*st));

	// This is how we get our INODE (equivalent) information
	ULARGE_INTEGER conv;
	conv.HighPart = fi.nFileIndexHigh;
	conv.LowPart = fi.nFileIndexLow;
	st->st_ino = (_ino_t)conv.QuadPart;

	// get the time information
	st->st_ctime = ConvertFileTimeToTime_t(&fi.ftCreationTime);
	st->st_atime = ConvertFileTimeToTime_t(&fi.ftLastAccessTime);
	st->st_mtime = ConvertFileTimeToTime_t(&fi.ftLastWriteTime);

	if (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		st->st_size = 0;
	}
	else
	{
		// size of the file
		LARGE_INTEGER st_size;
		memset(&st_size, 0, sizeof(st_size));

		if (!GetFileSizeEx(hdir, &st_size))
		{
			::syslog(LOG_WARNING, "Failed to get file size: "
				"error %d", GetLastError());
			errno = EACCES;
			return -1;
		}

		conv.HighPart = st_size.HighPart;
		conv.LowPart = st_size.LowPart;
		st->st_size = (_off_t)conv.QuadPart;
	}

	// at the mo
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_nlink = 1;

	// the mode of the file
	// mode zero will make it impossible to restore on Unix
	// (no access to anybody, including the owner).
	// we'll fake a sensible mode:
	// all objects get user read (0400)
	// if it's a directory it gets user execute (0100)
	// if it's not read-only it gets user write (0200)
	st->st_mode = S_IREAD;

	if (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		st->st_mode |= S_IFDIR | S_IEXEC;
	}
	else
	{
		st->st_mode |= S_IFREG;
	}

	if (!(fi.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
	{
		st->st_mode |= S_IWRITE;
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
HANDLE OpenFileByNameUtf8(const char* pFileName, DWORD flags)
{
	std::string AbsPathWithUnicode = 
		ConvertPathToAbsoluteUnicode(pFileName);
	
	if (AbsPathWithUnicode.size() == 0)
	{
		// error already logged by ConvertPathToAbsoluteUnicode()
		return NULL;
	}
	
	WCHAR* pBuffer = ConvertUtf8ToWideString(AbsPathWithUnicode.c_str());
	// We are responsible for freeing pBuffer
	
	if (pBuffer == NULL)
	{
		// error already logged by ConvertUtf8ToWideString()
		return NULL;
	}

	HANDLE handle = CreateFileW(pBuffer, 
		flags,
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
		handle = CreateFileW(pBuffer, 
			READ_CONTROL,
			FILE_SHARE_READ, 
			NULL, 
			OPEN_EXISTING, 
			FILE_FLAG_BACKUP_SEMANTICS,
			NULL);
	}

	delete [] pBuffer;

	if (handle == INVALID_HANDLE_VALUE)
	{
		DWORD err = GetLastError();

		if (err == ERROR_FILE_NOT_FOUND ||
			err == ERROR_PATH_NOT_FOUND)
		{
			errno = ENOENT;
		}
		else
		{
			if (err == ERROR_ACCESS_DENIED)
			{
				::syslog(LOG_WARNING, "Failed to open '%s': "
					"access denied", pFileName);
			}
			else
			{
				::syslog(LOG_WARNING, "Failed to open '%s': "
					"error %d", pFileName, err);
			}
			errno = EACCES;
		}

		return NULL;
	}

	return handle;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    emu_stat 
//		Purpose: replacement for the lstat and stat functions, 
//			works with unicode filenames supplied in utf8 format
//		Created: 25th October 2004
//
// --------------------------------------------------------------------------
int emu_stat(const char * pName, struct stat * st)
{
	HANDLE handle = OpenFileByNameUtf8(pName, 
		FILE_READ_ATTRIBUTES | FILE_READ_EA);

	if (handle == NULL)
	{
		// errno already set and error logged by OpenFileByNameUtf8()
		return -1;
	}

	int retVal = emu_fstat(handle, st);
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
	HANDLE handle = OpenFileByNameUtf8(pName,
		FILE_READ_ATTRIBUTES | FILE_READ_EA);

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
	s->f_mntonname[0] = '\\';

	CloseHandle(handle);   // close the handle

	return 0;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    emu_utimes
//		Purpose: replacement for the POSIX utimes() function,
//			works with unicode filenames supplied in utf8 format,
//			sets creation time instead of last access time.
//		Created: 25th July 2006
//
// --------------------------------------------------------------------------
int emu_utimes(const char * pName, const struct timeval times[])
{
	FILETIME creationTime;
	if (!ConvertTime_tToFileTime(times[0].tv_sec, &creationTime))
	{
		errno = EINVAL;
		return -1;
	}

	FILETIME modificationTime;
	if (!ConvertTime_tToFileTime(times[1].tv_sec, &modificationTime))
	{
		errno = EINVAL;
		return -1;
	}

	HANDLE handle = OpenFileByNameUtf8(pName, FILE_WRITE_ATTRIBUTES);

	if (handle == NULL)
	{
		// errno already set and error logged by OpenFileByNameUtf8()
		return -1;
	}

	if (!SetFileTime(handle, &creationTime, NULL, &modificationTime))
	{
		::syslog(LOG_ERR, "Failed to set times on '%s': error %d",
			pName, GetLastError());
		CloseHandle(handle);
		return 1;
	}

	CloseHandle(handle);
	return 0;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    emu_chmod
//		Purpose: replacement for the POSIX chmod function,
//			works with unicode filenames supplied in utf8 format
//		Created: 26th July 2006
//
// --------------------------------------------------------------------------
int emu_chmod(const char * pName, mode_t mode)
{
	std::string AbsPathWithUnicode = 
		ConvertPathToAbsoluteUnicode(pName);
	
	if (AbsPathWithUnicode.size() == 0)
	{
		// error already logged by ConvertPathToAbsoluteUnicode()
		return -1;
	}
	
	WCHAR* pBuffer = ConvertUtf8ToWideString(AbsPathWithUnicode.c_str());
	// We are responsible for freeing pBuffer
	
	if (pBuffer == NULL)
	{
		// error already logged by ConvertUtf8ToWideString()
		free(pBuffer);
		return -1;
	}

	DWORD attribs = GetFileAttributesW(pBuffer);
	if (attribs == INVALID_FILE_ATTRIBUTES)
	{
		::syslog(LOG_ERR, "Failed to get file attributes of '%s': "
			"error %d", pName, GetLastError());
		errno = EACCES;
		free(pBuffer);
		return -1;
	}

	if (mode & S_IWRITE)
	{
		attribs &= ~FILE_ATTRIBUTE_READONLY;
	}
	else
	{
		attribs |= FILE_ATTRIBUTE_READONLY;
	}

	if (!SetFileAttributesW(pBuffer, attribs))
	{
		::syslog(LOG_ERR, "Failed to set file attributes of '%s': "
			"error %d", pName, GetLastError());
		errno = EACCES;
		free(pBuffer);
		return -1;
	}

	free(pBuffer);
	return 0;
}


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
	if (!name || !name[0])
	{
		errno = EINVAL;
		return NULL;
	}
	
	std::string dirName(name);

	//append a '\' win32 findfirst is sensitive to this
	if ( dirName[dirName.size()] != '\\' || dirName[dirName.size()] != '/' )
	{
		dirName += '\\';
	}

	// what is the search string? - everything
	dirName += '*';

	DIR *pDir = new DIR;
	if (pDir == NULL)
	{
		errno = ENOMEM;
		return NULL;
	}

	pDir->name = ConvertUtf8ToWideString(dirName.c_str());
	// We are responsible for freeing dir->name
	
	if (pDir->name == NULL)
	{
		delete pDir;
		return NULL;
	}

	pDir->fd = _wfindfirst((const wchar_t*)pDir->name, &(pDir->info));

	if (pDir->fd == -1)
	{
		delete [] pDir->name;
		delete pDir;
		return NULL;
	}
		
	pDir->result.d_name = 0;
	return pDir;
}

// this kinda makes it not thread friendly!
// but I don't think it needs to be.
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
				den = &dp->result;
				std::wstring input(dp->info.name);
				memset(tempbuff, 0, sizeof(tempbuff));
				WideCharToMultiByte(CP_UTF8, 0, dp->info.name, 
					-1, &tempbuff[0], sizeof (tempbuff), 
					NULL, NULL);
				//den->d_name = (char *)dp->info.name;
				den->d_name = &tempbuff[0];
				if (dp->info.attrib & FILE_ATTRIBUTE_DIRECTORY)
				{
					den->d_type = S_IFDIR;
				}
				else
				{
					den->d_type = S_IFREG;
				}
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

		FD_ZERO(&readfd);
		FD_ZERO(&writefd);

		// struct pollfd *ufdsTmp = ufds;

		timeval timOut;
		timeval *tmpptr; 

		if (timeout == INFTIM)
			tmpptr = NULL;
		else
			tmpptr = &timOut;

		timOut.tv_sec  = timeout / 1000;
		timOut.tv_usec = (timeout * 1000) % 1000000;

		for (unsigned long i = 0; i < nfds; i++)
		{
			struct pollfd* ufd = &(ufds[i]);

			if (ufd->events & POLLIN)
			{
				FD_SET(ufd->fd, &readfd);
			}

			if (ufd->events & POLLOUT)
			{
				FD_SET(ufd->fd, &writefd);
			}

			if (ufd->events & ~(POLLIN | POLLOUT))
			{
				printf("Unsupported poll bits %d",
					ufd->events);
				return -1;
			}
		}	

		int nready = select(0, &readfd, &writefd, 0, tmpptr);

		if (nready == SOCKET_ERROR)
		{
			// int errval = WSAGetLastError();

			struct pollfd* pufd = ufds;
			for (unsigned long i = 0; i < nfds; i++)
			{
				pufd->revents = POLLERR;
				pufd++;
			}
			return (-1);
		}
		else if (nready > 0)
		{
			for (unsigned long i = 0; i < nfds; i++)
			{
				struct pollfd *ufd = &(ufds[i]);

				if (FD_ISSET(ufd->fd, &readfd))
				{
					ufd->revents |= POLLIN;
				}

				if (FD_ISSET(ufd->fd, &writefd))
				{
					ufd->revents |= POLLOUT;
				}
			}
		}

		return nready;
	}
	catch (...)
	{
		printf("Caught poll");
	}

	return -1;
}

// copied from MSDN: http://msdn.microsoft.com/library/default.asp?url=/library/en-us/eventlog/base/adding_a_source_to_the_registry.asp

BOOL AddEventSource
(
	LPTSTR pszSrcName, // event source name
	DWORD  dwNum       // number of categories
)
{
	// Work out the executable file name, to register ourselves
	// as the event source

	char cmd[MAX_PATH];
	if (GetModuleFileName(NULL, cmd, sizeof(cmd)-1) == 0)
	{
		::syslog(LOG_ERR, "Failed to get the program file name: "
			"error %d", GetLastError());
		return FALSE;
	}
	cmd[sizeof(cmd)-1] = 0;
 	std::string exepath(cmd);

	// Create the event source as a subkey of the log. 

	std::string regkey("SYSTEM\\CurrentControlSet\\Services\\EventLog\\"
		"Application\\");
	regkey += pszSrcName; 
 
	HKEY hk;
	DWORD dwDisp;

	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, regkey.c_str(), 
			 0, NULL, REG_OPTION_NON_VOLATILE,
			 KEY_WRITE, NULL, &hk, &dwDisp)) 
	{
		::syslog(LOG_ERR, "Failed to create the registry key: "
			"error %d", GetLastError()); 
		return FALSE;
	}

	// Set the name of the message file. 
 
	if (RegSetValueEx(hk,                // subkey handle 
			 "EventMessageFile", // value name 
			 0,                  // must be zero 
			 REG_EXPAND_SZ,      // value type 
			 (LPBYTE) exepath.c_str(),  // pointer to value data 
			 (DWORD) (exepath.size()))) // data size
	{
		::syslog(LOG_ERR, "Failed to set the event message file: "
			"error %d", GetLastError()); 
		RegCloseKey(hk); 
		return FALSE;
	}
 
	// Set the supported event types. 
 
	DWORD dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | 
		  EVENTLOG_INFORMATION_TYPE; 
 
	if (RegSetValueEx(hk,               // subkey handle 
			  "TypesSupported", // value name 
			  0,                // must be zero 
			  REG_DWORD,        // value type 
			  (LPBYTE) &dwData, // pointer to value data 
			  sizeof(DWORD)))   // length of value data 
	{
		::syslog(LOG_ERR, "Failed to set the supported types: "
			"error %d", GetLastError()); 
		RegCloseKey(hk); 
		return FALSE;
	}
 
	// Set the category message file and number of categories.

	if (RegSetValueEx(hk,                        // subkey handle 
			  "CategoryMessageFile",     // value name 
			  0,                         // must be zero 
			  REG_EXPAND_SZ,             // value type 
			  (LPBYTE) exepath.c_str(),  // pointer to value data 
			  (DWORD) (exepath.size()))) // data size
	{
		::syslog(LOG_ERR, "Failed to set the category message file: "
			"error %d", GetLastError());
		RegCloseKey(hk); 
		return FALSE;
	}
 
	if (RegSetValueEx(hk,              // subkey handle 
			  "CategoryCount", // value name 
			  0,               // must be zero 
			  REG_DWORD,       // value type 
			  (LPBYTE) &dwNum, // pointer to value data 
			  sizeof(DWORD)))  // length of value data 
	{
		::syslog(LOG_ERR, "Failed to set the category count: "
			"error %d", GetLastError());
		RegCloseKey(hk); 
		return FALSE;
	}

	RegCloseKey(hk); 
	return TRUE;
}

static HANDLE gSyslogH = 0;
static bool sHaveWarnedEventLogFull = false;

void openlog(const char * daemonName, int, int)
{
	// register a default event source, so that we can
	// log errors with the process of adding or registering our own.
	gSyslogH = RegisterEventSource(
		NULL,        // uses local computer 
		daemonName); // source name
	if (gSyslogH == NULL) 
	{
	}

	if (!AddEventSource("Box Backup", 0))
	{
		::syslog(LOG_ERR, "Failed to add our own event source");
		return;
	}

	HANDLE newSyslogH = RegisterEventSource(NULL, "Box Backup");
	if (newSyslogH == NULL)
	{
		::syslog(LOG_ERR, "Failed to register our own event source: "
			"error %d", GetLastError());
		return;
	}

	DeregisterEventSource(gSyslogH);
	gSyslogH = newSyslogH;
}

void closelog(void)
{
	DeregisterEventSource(gSyslogH); 
}

void syslog(int loglevel, const char *frmt, ...)
{
	WORD errinfo;
	char buffer[1024];
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

	// taken from MSDN
	int sixfourpos;
	while ( (sixfourpos = (int)sixfour.find("%ll")) != -1 )
	{
		// maintain portability - change the 64 bit formater...
		std::string temp = sixfour.substr(0,sixfourpos);
		temp += "%I64";
		temp += sixfour.substr(sixfourpos+3, sixfour.length());
		sixfour = temp;
	}

	// printf("parsed string is:%s\r\n", sixfour.c_str());

	va_list args;
	va_start(args, frmt);

	int len = vsnprintf(buffer, sizeof(buffer)-1, sixfour.c_str(), args);
	assert(len >= 0);
	if (len < 0) 
	{
		printf("%s\r\n", buffer);
		fflush(stdout);
		return;
	}
	
	assert((size_t)len < sizeof(buffer));
	buffer[sizeof(buffer)-1] = 0;

	va_end(args);

	LPCSTR strings[] = { buffer, NULL };

	if (gSyslogH == 0)
	{
		printf("%s\r\n", buffer);
		fflush(stdout);
		return;
	}

	if (!ReportEvent(gSyslogH, // event log handle 
		errinfo,               // event type 
		0,                     // category zero 
		MSG_ERR,	       // event identifier - 
		                       // we will call them all the same
		NULL,                  // no user security identifier 
		1,                     // one substitution string 
		0,                     // no data 
		strings,               // pointer to string array 
		NULL))                 // pointer to data 

	{
		DWORD err = GetLastError();
		if (err == ERROR_LOG_FILE_FULL)
		{
			if (!sHaveWarnedEventLogFull)
			{
				printf("Unable to send message to Event Log "
					"(Event Log is full):\r\n");
				fflush(stdout);
				sHaveWarnedEventLogFull = TRUE;
			}
		}
		else
		{
			printf("Unable to send message to Event Log: "
				"error %i:\r\n", (int)err);
			fflush(stdout);
		}
	}
	else
	{
		sHaveWarnedEventLogFull = false;
	}

	printf("%s\r\n", buffer);
	fflush(stdout);
}

int emu_chdir(const char* pDirName)
{
	std::string AbsPathWithUnicode = 
		ConvertPathToAbsoluteUnicode(pDirName);

	if (AbsPathWithUnicode.size() == 0)
	{
		// error already logged by ConvertPathToAbsoluteUnicode()
		return -1;
	}

	WCHAR* pBuffer = ConvertUtf8ToWideString(AbsPathWithUnicode.c_str());
	if (!pBuffer) return -1;
	int result = SetCurrentDirectoryW(pBuffer);
	delete [] pBuffer;
	if (result != 0) return 0;
	errno = EACCES;
	return -1;
}

char* emu_getcwd(char* pBuffer, int BufSize)
{
	DWORD len = GetCurrentDirectoryW(0, NULL);
	if (len == 0)
	{
		errno = EINVAL;
		return NULL;
	}

	if ((int)len > BufSize)
	{
		errno = ENAMETOOLONG;
		return NULL;
	}

	WCHAR* pWide = new WCHAR [len];
	if (!pWide)
	{
		errno = ENOMEM;
		return NULL;
	}

	DWORD result = GetCurrentDirectoryW(len, pWide);
	if (result <= 0 || result >= len)
	{
		errno = EACCES;
		return NULL;
	}

	char* pUtf8 = ConvertFromWideString(pWide, CP_UTF8);
	delete [] pWide;

	if (!pUtf8)
	{
		return NULL;
	}

	strncpy(pBuffer, pUtf8, BufSize - 1);
	pBuffer[BufSize - 1] = 0;
	delete [] pUtf8;

	return pBuffer;
}

int emu_mkdir(const char* pPathName)
{
	std::string AbsPathWithUnicode = 
		ConvertPathToAbsoluteUnicode(pPathName);

	if (AbsPathWithUnicode.size() == 0)
	{
		// error already logged by ConvertPathToAbsoluteUnicode()
		return -1;
	}

	WCHAR* pBuffer = ConvertUtf8ToWideString(AbsPathWithUnicode.c_str());
	if (!pBuffer)
	{
		return -1;
	}

	BOOL result = CreateDirectoryW(pBuffer, NULL);
	delete [] pBuffer;

	if (!result)
	{
		errno = EACCES;
		return -1;
	}

	return 0;
}

int emu_unlink(const char* pFileName)
{
	std::string AbsPathWithUnicode = 
		ConvertPathToAbsoluteUnicode(pFileName);

	if (AbsPathWithUnicode.size() == 0)
	{
		// error already logged by ConvertPathToAbsoluteUnicode()
		return -1;
	}

	WCHAR* pBuffer = ConvertUtf8ToWideString(AbsPathWithUnicode.c_str());
	if (!pBuffer)
	{
		return -1;
	}

	BOOL result = DeleteFileW(pBuffer);
	DWORD err = GetLastError();
	delete [] pBuffer;

	if (!result)
	{
		if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
		{
			errno = ENOENT;
		}
		else if (err == ERROR_SHARING_VIOLATION)
		{
			errno = EBUSY;
		}
		else if (err == ERROR_ACCESS_DENIED)
		{
			errno = EACCES;
		}
		else
		{
			::syslog(LOG_WARNING, "Failed to delete file "
				"'%s': error %d", pFileName, (int)err);
			errno = ENOSYS;
		}
		return -1;
	}

	return 0;
}

int console_read(char* pBuffer, size_t BufferSize)
{
	HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);

	if (hConsole == INVALID_HANDLE_VALUE)
	{
		::fprintf(stderr, "Failed to get a handle on standard input: "
			"error %d\n", GetLastError());
		return -1;
	}

	size_t WideSize = BufferSize / 5;
	WCHAR* pWideBuffer = new WCHAR [WideSize];

	if (!pWideBuffer)
	{
		::perror("Failed to allocate wide character buffer");
		return -1;
	}

	DWORD numCharsRead = 0;

	if (!ReadConsoleW(
			hConsole,
			pWideBuffer,
			WideSize, // will not be null terminated by ReadConsole
			&numCharsRead,
			NULL // reserved
		)) 
	{
		::fprintf(stderr, "Failed to read from console: error %d\n",
			GetLastError());
		return -1;
	}

	pWideBuffer[numCharsRead] = 0;

	char* pUtf8 = ConvertFromWideString(pWideBuffer, GetConsoleCP());
	strncpy(pBuffer, pUtf8, BufferSize);
	delete [] pUtf8;

	return strlen(pBuffer);
}

int readv (int filedes, const struct iovec *vector, size_t count)
{
	int bytes = 0;
	
	for (size_t i = 0; i < count; i++)
	{
		int result = read(filedes, vector[i].iov_base, 
			vector[i].iov_len);
		if (result < 0)
		{
			return result;
		}
		bytes += result;
	}

	return bytes;
}

int writev(int filedes, const struct iovec *vector, size_t count)
{
	int bytes = 0;
	
	for (size_t i = 0; i < count; i++)
	{
		int result = write(filedes, vector[i].iov_base, 
			vector[i].iov_len);
		if (result < 0)
		{
			return result;
		}
		bytes += result;
	}

	return bytes;
}

// need this for conversions
time_t ConvertFileTimeToTime_t(FILETIME *fileTime)
{
	SYSTEMTIME stUTC;
	struct tm timeinfo;

	// Convert the last-write time to local time.
	FileTimeToSystemTime(fileTime, &stUTC);

	memset(&timeinfo, 0, sizeof(timeinfo));	
	timeinfo.tm_sec = stUTC.wSecond;
	timeinfo.tm_min = stUTC.wMinute;
	timeinfo.tm_hour = stUTC.wHour;
	timeinfo.tm_mday = stUTC.wDay;
	timeinfo.tm_wday = stUTC.wDayOfWeek;
	timeinfo.tm_mon = stUTC.wMonth - 1;
	// timeinfo.tm_yday = ...;
	timeinfo.tm_year = stUTC.wYear - 1900;

	time_t retVal = mktime(&timeinfo) - _timezone;
	return retVal;
}

bool ConvertTime_tToFileTime(const time_t from, FILETIME *pTo)
{
	time_t adjusted = from + _timezone;
	struct tm *time_breakdown = gmtime(&adjusted);
	if (time_breakdown == NULL)
	{
		::syslog(LOG_ERR, "Error: failed to convert time format: "
			"%d is not a valid time\n", from);
		return false;
	}

	SYSTEMTIME stUTC;
	stUTC.wSecond       = time_breakdown->tm_sec;
	stUTC.wMinute       = time_breakdown->tm_min;
	stUTC.wHour         = time_breakdown->tm_hour;
	stUTC.wDay          = time_breakdown->tm_mday;
	stUTC.wDayOfWeek    = time_breakdown->tm_wday;
	stUTC.wMonth        = time_breakdown->tm_mon  + 1;
	stUTC.wYear         = time_breakdown->tm_year + 1900;
	stUTC.wMilliseconds = 0;

	// Convert the last-write time to local time.
	if (!SystemTimeToFileTime(&stUTC, pTo))
	{
		syslog(LOG_ERR, "Failed to convert between time formats: "
			"error %d", GetLastError());
		return false;
	}

	return true;
}


#endif // WIN32
