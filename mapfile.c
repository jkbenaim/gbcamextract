#ifdef __MINGW32__
#include <windows.h>
#include <inttypes.h>
#include <stdio.h>
#include "mapfile.h"

struct MappedFile_s MappedFile_Create(char *filename, size_t size)
{
	__label__ out_error, out_ok;
	LPVOID p;
	BOOL rc;
	LARGE_INTEGER liSize;
	struct MappedFile_s m;
	DWORD dw;
	char *lpMsgBuf;

	m._hFile = CreateFile(
		filename,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (m._hFile == INVALID_HANDLE_VALUE) {
		goto out_error;
	}

	liSize.QuadPart = size;
	m.size = size;
	rc = SetFilePointerEx(
		m._hFile,
		liSize,
		NULL,
		FILE_BEGIN
	);
	if (rc == 0) {
		goto out_error;
	}

	rc = SetEndOfFile(m._hFile);
	if (rc == 0) {
		goto out_error;
	}

	m._hMapping = CreateFileMapping(
		m._hFile,
		NULL,
		PAGE_READWRITE,
		liSize.HighPart,
		liSize.LowPart,
		NULL
	);

	if (m._hMapping == INVALID_HANDLE_VALUE) {
		goto out_error;
	}

	p = MapViewOfFile(
		m._hMapping,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		0
	);

	if (p == NULL) {
		goto out_error;
	}

	m.data = (void *) p;
	goto out_ok;

out_error:
	dw = GetLastError();
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0,
		NULL
	);
	printf("error in MappedFile_Create: %s", lpMsgBuf);
	LocalFree(lpMsgBuf);
	m.data = NULL;
out_ok:
	return m;
}

struct MappedFile_s MappedFile_Open(char *filename, bool writable)
{
	__label__ out_error, out_ok;
	LPVOID p;
	BOOL rc;
	LARGE_INTEGER liSize;
	struct MappedFile_s m;

	m._hFile = CreateFile(
		filename,
		writable ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (m._hFile == INVALID_HANDLE_VALUE) {
		goto out_error;
	}

	rc = GetFileSizeEx(m._hFile, &liSize);
	if (rc == 0) {
		goto out_error;
	}
	m.size = (uint64_t) liSize.QuadPart;

	m._hMapping = CreateFileMapping(
		m._hFile,
		NULL,
		writable ? PAGE_READWRITE : PAGE_READONLY,
		0,
		m.size,
		NULL
	);

	if (m._hMapping == INVALID_HANDLE_VALUE) {
		goto out_error;
	}

	p = MapViewOfFile(
		m._hMapping,
		writable ? FILE_MAP_ALL_ACCESS : FILE_MAP_COPY,
		0,
		0,
		0
	);

	if (p == NULL) {
		goto out_error;
	}

	m.data = (void *) p;
	goto out_ok;

out_error:
	m.data = NULL;
out_ok:
	return m;
}

void MappedFile_Close(struct MappedFile_s m)
{
	FlushViewOfFile((LPCVOID) m.data, 0);
	UnmapViewOfFile((LPCVOID) m.data);
	CloseHandle(m._hMapping);
	CloseHandle(m._hFile);
}

/* __MINGW32__ */
#else
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "mapfile.h"

struct MappedFile_s MappedFile_Create(char *filename, size_t size)
{
	__label__ out_error, out_ok, out_close;
	struct MappedFile_s m;

	m._fd = open(filename, O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
	if (!m._fd) goto out_error;
	close(m._fd);
	if (truncate(filename, size) < 0) goto out_error;
	m._fd = open(filename, O_RDWR);
	if (!m._fd) goto out_error;

	m.data = mmap(
		NULL,
		size,
		PROT_READ|PROT_WRITE,
		MAP_SHARED,
		m._fd,
		0
	);
	if (m.data == NULL) {
		goto out_close;
	}

	memset(m.data, 0, size);
	m.size = size;

	goto out_ok;

out_close:
	close(m._fd);
out_error:
	m.size = 0;
	m.data = NULL;
out_ok:
	return m;
}

struct MappedFile_s MappedFile_Open(char *filename, bool writable)
{
	__label__ out_error, out_ok, out_close;
	struct MappedFile_s m;
	struct stat sb;

	if (stat(filename, &sb) == -1) {
		goto out_error;
	}
	m.size = sb.st_size;

	m._fd = open(filename, writable ? O_RDWR : O_RDONLY);
	if (!m._fd) {
		goto out_error;
	}

	m.data = mmap(
		NULL,
		sb.st_size,
		PROT_READ|PROT_WRITE,
		writable ? MAP_SHARED : MAP_PRIVATE,
		m._fd,
		0
	);
	if (m.data == NULL) {
		goto out_close;
	}

	goto out_ok;

out_close:
	close(m._fd);
out_error:
	m.data = NULL;
out_ok:
	return m;
}

void MappedFile_Close(struct MappedFile_s m)
{
	munmap(m.data, m.size);
	close(m._fd);
}

/* __MINGW32__ */
#endif
