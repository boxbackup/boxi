// --------------------------------------------------------------------------
//
// File
//		Name:    MemLeakFinder.cpp
//		Purpose: Memory leak finder
//		Created: 12/1/04
//
// --------------------------------------------------------------------------


#ifndef NDEBUG

#include "Box.h"

#undef malloc
#undef realloc
#undef free

#include <map>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <set>

bool memleakfinder_global_enable = false;

typedef struct
{
	size_t size;
	const char *file;
	int line;
} MallocBlockInfo;

typedef struct
{
	size_t size;
	const char *file;
	int line;
	bool array;
} ObjectInfo;

namespace
{
	static std::map<void *, MallocBlockInfo> sMallocBlocks;
	static std::map<void *, ObjectInfo> sObjectBlocks;
	
	static bool sTrackMallocInSection = false;
	static std::set<void *> sSectionMallocBlocks;
	static bool sTrackObjectsInSection = false;
	static std::map<void *, ObjectInfo> sSectionObjectBlocks;
	
	static std::set<void *> sNotLeaks;

	void *sNotLeaksPre[1024];
	int sNotLeaksPreNum = 0;
}

void memleakfinder_malloc_add_block(void *b, size_t size, const char *file, int line)
{
	if(b != 0)
	{
		MallocBlockInfo i;
		i.size = size;
		i.file = file;
		i.line = line;
		sMallocBlocks[b] = i;
		
		if(sTrackMallocInSection)
		{
			sSectionMallocBlocks.insert(b);
		}
	}
}


void *memleakfinder_malloc(size_t size, const char *file, int line)
{
	void *b = ::malloc(size);
	if(!memleakfinder_global_enable) return b;

	memleakfinder_malloc_add_block(b, size, file, line);

	//TRACE4("malloc(), %d, %s, %d, %08x\n", size, file, line, b);
	return b;
}

void *memleakfinder_realloc(void *ptr, size_t size)
{
	if(!memleakfinder_global_enable)
	{
		return ::realloc(ptr, size);
	}

	// Check it's been allocated
	std::map<void *, MallocBlockInfo>::iterator i(sMallocBlocks.find(ptr));
	if(ptr && i == sMallocBlocks.end())
	{
		TRACE1("Block %x realloc(), but not in list. Error? Or allocated in startup static objects?\n", ptr);
	}

	void *b = ::realloc(ptr, size);

	if(ptr && i!=sMallocBlocks.end())
	{
		// Worked?
		if(b != 0)
		{
			// Update map
			MallocBlockInfo inf = i->second;
			inf.size = size;
			sMallocBlocks.erase(i);
			sMallocBlocks[b] = inf;

			if(sTrackMallocInSection)
			{
				std::set<void *>::iterator si(sSectionMallocBlocks.find(ptr));
				if(si != sSectionMallocBlocks.end()) sSectionMallocBlocks.erase(si);
				sSectionMallocBlocks.insert(b);
			}
		}
	}
	else
	{
		memleakfinder_malloc_add_block(b, size, "FOUND-IN-REALLOC", 0);
	}

	//TRACE3("realloc(), %d, %08x->%08x\n", size, ptr, b);
	return b;
}

void memleakfinder_free(void *ptr)
{
	if(memleakfinder_global_enable)
	{
		// Check it's been allocated
		std::map<void *, MallocBlockInfo>::iterator i(sMallocBlocks.find(ptr));
		if(i != sMallocBlocks.end())
		{
			sMallocBlocks.erase(i);
		}
		else
		{
			TRACE1("Block %x freed, but not known. Error? Or allocated in startup static allocation?\n", ptr);
		}

		if(sTrackMallocInSection)
		{
			std::set<void *>::iterator si(sSectionMallocBlocks.find(ptr));
			if(si != sSectionMallocBlocks.end()) sSectionMallocBlocks.erase(si);
		}
	}

	//TRACE1("free(), %08x\n", ptr);
	::free(ptr);
}


void memleakfinder_notaleak_insert_pre()
{
	if(!memleakfinder_global_enable) return;
	for(int l = 0; l < sNotLeaksPreNum; l++)
	{
		sNotLeaks.insert(sNotLeaksPre[l]);
	}
	sNotLeaksPreNum = 0;
}

bool is_leak(void *ptr)
{
	memleakfinder_notaleak_insert_pre();
	return sNotLeaks.find(ptr) == sNotLeaks.end();
}

void memleakfinder_notaleak(void *ptr)
{
	memleakfinder_notaleak_insert_pre();
	if(memleakfinder_global_enable)
	{
		sNotLeaks.insert(ptr);
	}
	else
	{
		sNotLeaksPre[sNotLeaksPreNum++] = ptr;
	}
/*	{
		std::map<void *, MallocBlockInfo>::iterator i(sMallocBlocks.find(ptr));
		if(i != sMallocBlocks.end()) sMallocBlocks.erase(i);
	}
	{
		std::set<void *>::iterator si(sSectionMallocBlocks.find(ptr));
		if(si != sSectionMallocBlocks.end()) sSectionMallocBlocks.erase(si);
	}
	{
		std::map<void *, ObjectInfo>::iterator i(sObjectBlocks.find(ptr));
		if(i != sObjectBlocks.end()) sObjectBlocks.erase(i);
	}*/
}



// start monitoring a section of code
void memleakfinder_startsectionmonitor()
{
	sTrackMallocInSection = true;
	sSectionMallocBlocks.clear();
	sTrackObjectsInSection = true;
	sSectionObjectBlocks.clear();
}

// trace all blocks allocated and still allocated since memleakfinder_startsectionmonitor() called
void memleakfinder_traceblocksinsection()
{
	std::set<void *>::iterator s(sSectionMallocBlocks.begin());
	for(; s != sSectionMallocBlocks.end(); ++s)
	{
		std::map<void *, MallocBlockInfo>::const_iterator i(sMallocBlocks.find(*s));
		if(i == sMallocBlocks.end())
		{
			TRACE0("Logical error in section block finding\n");
		}
		else
		{
			TRACE4("Block 0x%08lx size %d allocated at %s:%d\n", (long)i->first, i->second.size, i->second.file, i->second.line);
		}
	}
	for(std::map<void *, ObjectInfo>::const_iterator i(sSectionObjectBlocks.begin()); i != sSectionObjectBlocks.end(); ++i)
	{
		TRACE5("Object%s 0x%08lx size %d allocated at %s:%d\n", i->second.array?" []":"", (long)i->first, i->second.size, i->second.file, i->second.line);
	}
}

int memleakfinder_numleaks()
{
	int n = 0;
	
	for(std::map<void *, MallocBlockInfo>::const_iterator i(sMallocBlocks.begin()); i != sMallocBlocks.end(); ++i)
	{
		if(is_leak(i->first)) ++n;
	}
	
	for(std::map<void *, ObjectInfo>::const_iterator i(sObjectBlocks.begin()); i != sObjectBlocks.end(); ++i)
	{
		if(is_leak(i->first)) ++n;
	}

	return n;
}

void memleakfinder_reportleaks_file(FILE *file)
{
	for(std::map<void *, MallocBlockInfo>::const_iterator i(sMallocBlocks.begin()); i != sMallocBlocks.end(); ++i)
	{
		if(is_leak(i->first)) ::fprintf(file, "Block 0x%08lx size %d allocated at %s:%d\n", (long)i->first, i->second.size, i->second.file, i->second.line);
	}
	for(std::map<void *, ObjectInfo>::const_iterator i(sObjectBlocks.begin()); i != sObjectBlocks.end(); ++i)
	{
		if(is_leak(i->first)) ::fprintf(file, "Object%s 0x%08lx size %d allocated at %s:%d\n", i->second.array?" []":"", (long)i->first, i->second.size, i->second.file, i->second.line);
	}
}

void memleakfinder_reportleaks()
{
	// report to stdout
	memleakfinder_reportleaks_file(stdout);
}

void memleakfinder_reportleaks_appendfile(const char *filename, const char *markertext)
{
	FILE *file = ::fopen(filename, "a");
	if(file != 0)
	{
		if(memleakfinder_numleaks() > 0)
		{
			fprintf(file, "MEMORY LEAKS FROM PROCESS %d (%s)\n", getpid(), markertext);
			memleakfinder_reportleaks_file(file);
		}
	
		::fclose(file);
	}
	else
	{
		printf("WARNING: Couldn't open memory leak results file %s for appending\n", filename);
	}
}

static char atexit_filename[512];
static char atexit_markertext[512];
static bool atexit_registered = false;

void memleakfinder_atexit()
{
	memleakfinder_reportleaks_appendfile(atexit_filename, atexit_markertext);
}

void memleakfinder_setup_exit_report(const char *filename, const char *markertext)
{
	::strcpy(atexit_filename, filename);
	::strcpy(atexit_markertext, markertext);
	if(!atexit_registered)
	{
		atexit(memleakfinder_atexit);
		atexit_registered = true;
	}
}




void add_object_block(void *block, size_t size, const char *file, int line, bool array)
{
	if(!memleakfinder_global_enable) return;

	if(block != 0)
	{
		ObjectInfo i;
		i.size = size;
		i.file = file;
		i.line = line;
		i.array = array;
		sObjectBlocks[block] = i;
		
		if(sTrackObjectsInSection)
		{
			sSectionObjectBlocks[block] = i;
		}
	}
}

void remove_object_block(void *block)
{
	if(!memleakfinder_global_enable) return;

	std::map<void *, ObjectInfo>::iterator i(sObjectBlocks.find(block));
	if(i != sObjectBlocks.end())
	{
		sObjectBlocks.erase(i);
	}

	if(sTrackObjectsInSection)
	{
		std::map<void *, ObjectInfo>::iterator i(sSectionObjectBlocks.find(block));
		if(i != sSectionObjectBlocks.end())
		{
			sSectionObjectBlocks.erase(i);
		}
	}

	// If it's not in the list, just ignore it, as lots of stuff goes this way...
}

void *operator new(size_t size, const char *file, int line)
{
	void *r = ::malloc(size);
	add_object_block(r, size, file, line, false);
	//TRACE4("new(), %d, %s, %d, %08x\n", size, file, line, r);
	return r;
}

void *operator new[](size_t size, const char *file, int line)
{
	void *r = ::malloc(size);
	add_object_block(r, size, file, line, true);
	//TRACE4("new[](), %d, %s, %d, %08x\n", size, file, line, r);
	return r;
}

void operator delete[](void *ptr) throw ()
{
	::free(ptr);
	remove_object_block(ptr);
	//TRACE1("delete[]() called, %08x\n", ptr);
}

void operator delete(void *ptr) throw ()
{
	::free(ptr);
	remove_object_block(ptr);
	//TRACE1("delete() called, %08x\n", ptr);
}

#endif // NDEBUG
