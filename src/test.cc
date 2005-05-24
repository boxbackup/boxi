#include <stddef.h>
#include <stdint.h>

#include "Box.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>

#ifndef PLATFORM_READLINE_NOT_SUPPORTED
	#ifdef PLATFORM_LINUX
		#include "../../local/_linux_readline.h"
	#else
		#include <readline/readline.h>
		#include <readline/history.h>
	#endif
#endif

#include "MainHelper.h"
#include "BoxPortsAndFiles.h"
#include "BackupDaemonConfigVerify.h"
#include "SocketStreamTLS.h"
#include "Socket.h"
#include "TLSContext.h"
#include "SSLLib.h"
#include "BackupStoreConstants.h"
#include "BackupStoreDirectory.h"
#include "BackupStoreException.h"
#include "autogen_BackupProtocolClient.h"
#include "BackupClientCryptoKeys.h"
#include "BoxTimeToText.h"

int main(int argc, char **argv)
{
	const char *configFilename = 
		"/home/chris/project/boxi/test/config/bbackupd.conf";
	
	std::string errs;
	std::auto_ptr<Configuration> config(Configuration::LoadAndVerify(
		configFilename, &BackupDaemonConfigVerify, errs));
	
	if(config.get() == 0 || !errs.empty())
	{
		printf("Invalid configuration file:\n%s", errs.c_str());
		return 1;
	}

	// Easier coding
	const Configuration &conf(*config);
	
	// Setup and connect
	// 1. TLS context
	SSLLib::Initialise();
	// Read in the certificates creating a TLS context
	TLSContext tlsContext;
	std::string certFile(conf.GetKeyValue("CertificateFile"));
	std::string keyFile(conf.GetKeyValue("PrivateKeyFile"));
	std::string caFile(conf.GetKeyValue("TrustedCAsFile"));
	tlsContext.Initialise(false /* as client */, certFile.c_str(),
		keyFile.c_str(), caFile.c_str());
	
	// Initialise keys
	BackupClientCryptoKeys_Setup(conf.GetKeyValue("KeysFile").c_str());

	// 2. Connect to server
	SocketStreamTLS socket;
	socket.Open(tlsContext, Socket::TypeINET, conf.GetKeyValue("StoreHostname").c_str(), BOX_PORT_BBSTORED);
	
	// 3. Make a protocol, and handshake
	BackupProtocolClient connection(socket);
	connection.Handshake();
	
	// Check the version of the server
	{
		std::auto_ptr<BackupProtocolClientVersion> serverVersion(
			connection.QueryVersion(BACKUP_STORE_SERVER_VERSION));

		if (serverVersion->GetVersion() != BACKUP_STORE_SERVER_VERSION)
		{
			THROW_EXCEPTION(BackupStoreException, WrongServerVersion)
		}
	}

	// Login -- if this fails, the Protocol will exception
	connection.QueryLogin(conf.GetKeyValueInt("AccountNumber"), 
		BackupProtocolClientLogin::Flags_ReadOnly);

	int64_t rootDir = BackupProtocolClientListDirectory::RootDirectory;
	
	// Generate exclude flags
	int16_t excludeFlags = BackupProtocolClientListDirectory::Flags_EXCLUDE_NOTHING;

	// Do communication
	connection.QueryListDirectory(
			rootDir,
			BackupProtocolClientListDirectory::Flags_INCLUDE_EVERYTHING,	// both files and directories
			excludeFlags,
			true /* want attributes */);

	// Retrieve the directory from the stream following
	BackupStoreDirectory dir;
	std::auto_ptr<IOStream> dirstream(connection.ReceiveStream());
	dir.ReadFromStream(*dirstream, connection.GetTimeout());

	// Then... display everything
	BackupStoreDirectory::Iterator i(dir);
	BackupStoreDirectory::Entry *en = 0;
	while((en = i.Next()) != 0)
	{
		// Display this entry
		BackupStoreFilenameClear clear(en->GetName());
		std::string line;
		
		// Object ID?
		if(1)
		{
			// add object ID to line
			char oid[32];
			sprintf(oid, "%08llx ", en->GetObjectID());
			line += oid;
		}
		
		// Flags?
		if(1)
		{
			static const char *flags = BACKUPSTOREDIRECTORY_ENTRY_FLAGS_DISPLAY_NAMES;
			char displayflags[8];
			// make sure f is big enough
			ASSERT(sizeof(displayflags) >= sizeof(BACKUPSTOREDIRECTORY_ENTRY_FLAGS_DISPLAY_NAMES) + 3);
			// Insert flags
			char *f = displayflags;
			const char *t = flags;
			int16_t en_flags = en->GetFlags();
			while(*t != 0)
			{
				*f = ((en_flags&1) == 0)?'-':*t;
				en_flags >>= 1;
				f++;
				t++;
			}
			// attributes flags
			*(f++) = (en->HasAttributes())?'a':'-';
			// terminate
			*(f++) = ' ';
			*(f++) = '\0';
			line += displayflags;
			if(en_flags != 0)
			{
				line += "[ERROR: Entry has additional flags set] ";
			}
		}
		
		if(1)
		{
			// Show times...
			line += BoxTimeToISO8601String(en->GetModificationTime());
			line += ' ';
		}
		
		if(1)
		{
			char hash[64];
			::sprintf(hash, "%016llx ", en->GetAttributesHash());
			line += hash;
		}
		
		if(1)
		{
			char num[32];
			sprintf(num, "%05lld ", en->GetSizeInBlocks());
			line += num;
		}
		
		// add name
		/*
		if(!FirstLevel)
		{
			line += rListRoot;
			line += '/';
		}
		*/
		line += clear.GetClearFilename().c_str();
		
		if(!en->GetName().IsEncrypted())
		{
			line += "[FILENAME NOT ENCRYPTED]";
		}
		
		// print line
		printf("%s\n", line.c_str());
		
		// Directory?
		if((en->GetFlags() & BackupStoreDirectory::Entry::Flags_Dir) != 0)
		{
			// Recurse?
			/*
			if(0)
			{
				std::string subroot(rListRoot);
				if(!FirstLevel) subroot += '/';
				subroot += clear.GetClearFilename();
				List(en->GetObjectID(), subroot, opts, false);
			}
			*/
		}
	}

    return 0;
}
