// --------------------------------------------------------------------------
//
// File
//		Name:    BackupClientInodeToIDMap.h
//		Purpose: Map of inode numbers to file IDs on the store
//		Created: 11/11/03
//
// --------------------------------------------------------------------------

#ifndef BACKUPCLIENTINODETOIDMAP_H
#define BACKUPCLIENTINODETOIDMAP__H

#include <sys/types.h>

#include <map>
#include <utility>

// Use in memory implementation if there isn't access to the Berkely DB on this platform
#ifndef HAVE_DB
	#define BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
#endif

// avoid having to include the DB files when not necessary
#ifndef BACKIPCLIENTINODETOIDMAP_IMPLEMENTATION
#ifdef BERKELY_V4
	class Db;
#else
	class DB;
#endif
#endif

// --------------------------------------------------------------------------
//
// Class
//		Name:    BackupClientInodeToIDMap
//		Purpose: Map of inode numbers to file IDs on the store
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
class BackupClientInodeToIDMap
{
public:
	BackupClientInodeToIDMap();
	~BackupClientInodeToIDMap();
private:
	BackupClientInodeToIDMap(const BackupClientInodeToIDMap &rToCopy);	// not allowed
public:

	void Open(const char *Filename, bool ReadOnly, bool CreateNew);
	void OpenEmpty();

	void AddToMap(InodeRefType InodeRef, int64_t ObjectID, int64_t InDirectory);
	bool Lookup(InodeRefType InodeRef, int64_t &rObjectIDOut, int64_t &rInDirectoryOut) const;

	void Close();

private:
#ifdef BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
	std::map<InodeRefType, std::pair<int64_t, int64_t> > mMap;
#else
	bool mReadOnly;
	bool mEmpty;
#ifdef BERKELY_V4
	Db *dbp;	// c++ style implimentation
#else
	DB *dbp;	// C style interface, use notation from documentation
#endif // BERKELY_V4
#endif // BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
};

#endif // BACKUPCLIENTINODETOIDMAP__H


