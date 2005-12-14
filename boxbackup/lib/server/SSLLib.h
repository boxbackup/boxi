// --------------------------------------------------------------------------
//
// File
//		Name:    SSLLib.h
//		Purpose: Utility functions for dealing with the OpenSSL library
//		Created: 2003/08/06
//
// --------------------------------------------------------------------------

#ifndef SSLLIB__H
#define SSLLIB__H

#ifndef NDEBUG
	extern bool SSLLib__TraceErrors;
	#define SET_DEBUG_SSLLIB_TRACE_ERRORS {SSLLib__TraceErrors = true;}
#else
	#define SET_DEBUG_SSLLIB_TRACE_ERRORS
#endif


// --------------------------------------------------------------------------
//
// Namespace
//		Name:    SSLLib
//		Purpose: Utility functions for dealing with the OpenSSL library
//		Created: 2003/08/06
//
// --------------------------------------------------------------------------
namespace SSLLib
{
	void Initialise();
	void LogError(const char *ErrorDuringAction);
};

#endif // SSLLIB__H

