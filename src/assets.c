#include <assets.h>
#include <ecs.h>
#include <SDL2/SDL.h>
#include <mem.h>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define WINDOWS
#endif

/* Use stb_image for zlib decompression */
/* Put the implementation here */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(sz) (globalAlloc(sz))
#define STBI_REALLOC(p, newsz) (globalRealloc(p, newsz))
#define STBI_FREE(p) (globalDealloc(p))
#define STBI_MAX_DIMENSIONS 16384
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#define STBI_ONLY_JPEG
#include "stb_image.h"

size_t ichLoadFile(const char **fileData, void **userData, const char *fileName) {
	struct Asset *a = assetOpen(fileName);
	if (!a)
		return 0;
	*userData = a;
	*fileData = a->buffer;
	return a->bufferSize;
}
void ichFreeFile(void *userData) {
	struct Asset *a = userData;
	assetClose(a);
}

size_t assetRead(struct Asset *a, void *buf, size_t sz) {
	return SDL_RWread(a->rwOps, buf, 1, sz);
}


void assetSeek(struct Asset *a, long offset, int whence) {
	SDL_RWseek(a->rwOps, offset, whence);
}

void assetClose(struct Asset *a) {
	SDL_RWclose(a->rwOps);
	globalDealloc(a->buffer);
	globalDealloc(a);
}


struct LoadedArchive {
	SDL_RWops* f;
	struct AssetArchiveHeader ah;
};

static struct LoadedArchive archives[MAX_ARCHIVES];

void assetArchive(int slot, const char *archive) {
	struct LoadedArchive* ar = &archives[slot];
	if (ar->f) {
		SDL_RWclose(ar->f);
		ar->f = NULL;
	}
	if (archive) {
		char buf[256];
		snprintf(buf, 256, "%s%s", gameDir, archive);
		ar->f = SDL_RWFromFile(buf, "r");
		if (!ar->f)
			return;
		SDL_RWread(ar->f, &ar->ah, 1, sizeof(ar->ah));
		if (memcmp(&ar->ah.sig, "RI_0", 4))
			fail("Asset archive has incorrect signature\n");
	}
}


static bool tryLoadArchive(void** data, size_t* dataSize, int slot, const char *file) {
	SDL_RWops *ar = archives[slot].f;
	if (!ar) {
		return false;
	}
	struct AssetArchiveHeader *ah = &archives[slot].ah;
	struct AssetArchiveFileEntry afe;
	SDL_RWseek(ar, ah->fileEntryOffset, RW_SEEK_SET);

	/* Find the correct entry */
	bool found = false;
	for (unsigned int i = 0; i < ah->numberOfFiles; i++) {
		int64_t rd = SDL_RWread(ar, &afe, 1, sizeof(afe));
		if (rd != sizeof(afe))
			fail("Asset archive has incorrect number of files\n");

		if (!strcmp(&afe.name[0], file)) {
			found = true;
			break;
		}
	}
	if (!found) {
		return false;
	}

	/* Read it */
	void* compressedData = stackAlloc(afe.compressedSize);
	SDL_RWseek(ar, afe.offset, RW_SEEK_SET);
	if (SDL_RWread(ar, compressedData, 1, afe.compressedSize) != afe.compressedSize) {
		fail("Assets: failed to read compressed data\n");
	}

	/* Decompress it */
	int uncompressedSize = 0;
	void* uncompressedData = stbi_zlib_decode_malloc_guesssize(compressedData, afe.compressedSize, afe.uncompressedSize, &uncompressedSize);
	if (!uncompressedData) {
		fail("Assets: Failed to decompress %s\n", file);
	}
	if ((unsigned int)uncompressedSize != afe.uncompressedSize) {
		logNorm("Asset warning: Uncompressed size (%d) of %s is not what was expected (%d)\n", uncompressedSize, file, afe.uncompressedSize);
	}
	stackDealloc(afe.compressedSize);
	
	*dataSize = uncompressedSize;
	*data = uncompressedData;
	return true;
}
static bool tryLoadFile(void** data, size_t* dataSize, const char* file) {
	/* Get full file name */
	char* buf = stackAlloc(1024);
	snprintf(buf, 1024, "%sdat/%s", gameDir, file);

	/* Replace forward slashes with backwards on windows */
#if defined(_WIN32) || defined(_WIN64)
	char* c = buf;
	while (*c) {
		if (*c == '/')
			*c = '\\';
		c++;
	}
#endif

	/* Open file */
	SDL_RWops* f = SDL_RWFromFile(buf, "rb");
	stackDealloc(1024);
	if (!f) {
		return false;
	}

	/* Get file size */
	size_t fileSz = SDL_RWsize(f);
	if (fileSz > UINT32_MAX) {
		SDL_RWclose(f);
		logNorm("Cannot load %s: file too big\n", file);
		return false;
	}

	/* Read it into memory */
	void* dat = globalAlloc(fileSz);
	SDL_RWseek(f, 0, RW_SEEK_SET);
	if (SDL_RWread(f, dat, fileSz, 1) != 1) {
		SDL_RWclose(f);
		logNorm("Cannot read %s\n", file);
		globalDealloc(dat);
		return false;
	}
	SDL_RWclose(f);

	*data = dat;
	*dataSize = fileSz;
	return true;
}

struct Asset *assetOpen(const char *file) {
	logDebug("Load %s\n", file);
	bool found = false;
	void* data;
	size_t dataSize;
	found = tryLoadFile(&data, &dataSize, file);
	for (int i = 0; i < MAX_ARCHIVES && !found; i++) {
		found = tryLoadArchive(&data, &dataSize, i, file);
	}
	if (!found) {
		logNorm("Assets: Entry %s not found\n", file);
		return NULL;
	}

	struct Asset *a = globalAlloc(sizeof(*a));
	a->buffer = data;
	a->bufferSize = dataSize;
	a->rwOps = SDL_RWFromConstMem(a->buffer, (int)a->bufferSize);

	return a;
}

struct Asset *assetUserOpen(const char *file, bool write) {
	char buf[256];
	snprintf(buf, 256, "%s%s", gameUserDir, file);
	buf[255] = 0;
	SDL_RWops *rw = SDL_RWFromFile(buf, write ? "w" : "r");
	if (!rw) {
		return NULL;
	}
	struct Asset *a = globalAlloc(sizeof(*a));
	a->rwOps = rw;
	return a;

}
size_t assetUserWrite(struct Asset *a, const void *buf, size_t sz) {
	return SDL_RWwrite(a->rwOps, buf, 1, sz);
}

#ifdef WINDOWS
static void setFF(struct FoundFile *ff, WIN32_FIND_DATAA *fData) {
	ff->flags = 0;
	if (fData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		ff->flags |= 1;
	if (fData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
		ff->flags |= 2;

	strncpy(ff->name, fData->cFileName, 256);
	SYSTEMTIME utcTime;
	SYSTEMTIME localTime;
	if (FileTimeToSystemTime(&fData->ftLastWriteTime, &utcTime) && SystemTimeToTzSpecificLocalTime(NULL, &utcTime, &localTime)) {
		ff->changedMinute = localTime.wMinute;
		ff->changedHour = localTime.wHour;
		ff->changedDay = localTime.wDay;
		ff->changedMonth = localTime.wMonth;
		ff->changedYear = localTime.wYear;
	} else {
		ff->changedYear = 0;
	}
}
#endif
bool assetDirFirst(struct FoundFile *ff, const char *dir) {
	char *buf = stackAlloc(1024);
	
	bool ret = false;

#ifdef WINDOWS
	snprintf(buf, 1024, "%sdat/%s/*", gameDir, dir);
	char *c = buf;
	while (*c) {
		if (*c == '/')
			*c = '\\';
		c++;
	}

	WIN32_FIND_DATAA fData;
	HANDLE handle = FindFirstFileA(buf, &fData);
	ff->priv = handle;
	if (handle != INVALID_HANDLE_VALUE) {
		setFF(ff, &fData);
		ret = true;
		
	}
#else
	// TODO
#endif

	stackDealloc(1024);
	return ret;
}
bool assetDirNext(struct FoundFile *ff) {
#ifdef WINDOWS
	WIN32_FIND_DATAA fData;
	bool next = true;
	bool found = FindNextFileA(ff->priv, &fData);
	if (!found)
		return false;
	setFF(ff, &fData);
	return true;
#endif
}
void assetDirClose(struct FoundFile *ff) {
#ifdef WINDOWS
	if (ff->priv != INVALID_HANDLE_VALUE)
		FindClose(ff->priv);
#endif
}


static struct Asset *logFile;
void assetFini(void);
void fail(const char *fmt, ...) {
	char buf[256];
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(buf, 256, fmt, args);
	va_end(args);

	if (logFile && len > 0) {
		assetUserWrite(logFile, buf, len);
	}

	showError("An error occured", len > 0? buf : "Unknown error");
	assetFini();
	abort();
}
void logNorm(const char *fmt, ...) {
	if (!logFile) {
		return;
	}

	char buf[256];
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(buf, 256, fmt, args);
	va_end(args);
	if (len > 0) {
		assetUserWrite(logFile, buf, len);
#ifndef RELEASE
#ifdef WINDOWS
		OutputDebugStringA(buf);
#else
		fputs(buf, stderr);
#endif
#endif
	}
}

void assetInit(void) {
	logFile = assetUserOpen("log.txt", "w");
}

void assetFini(void) {
	if (logFile)
		assetClose(logFile);
}
