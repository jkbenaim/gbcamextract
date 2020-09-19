#ifndef _MAPFILE_H_
#define _MAPFILE_H_

#ifdef __MINGW32__
#include <windows.h>
#endif
#include <stdbool.h>
#include <stdint.h>

struct MappedFile_s {
	void *data;
	uint64_t size;
#ifdef __MINGW32__
	HANDLE _hFile;
	HANDLE _hMapping;
#else
	int _fd;
#endif
};

struct MappedFile_s MappedFile_Create(char *filename, size_t size);
struct MappedFile_s MappedFile_Open(char *filename, bool writable);
void MappedFile_Close(struct MappedFile_s m);

/* _MAPFILE_H_ */
#endif
