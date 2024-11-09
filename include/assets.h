#ifndef ASSETS_H
#define ASSETS_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include <ecs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASSET_SET	0
#define ASSET_CUR	1
#define ASSET_END	2

struct AssetArchiveHeader {
	char sig[4];
	unsigned int numberOfFiles;
	uint64_t fileEntryOffset;
};

struct AssetArchiveFileEntry { /* 64 bytes */
	char name[36];
	uint32_t adler32;
	uint64_t offset;
	uint64_t compressedSize;
	uint64_t uncompressedSize;
};


struct Asset {
	void *rwOps;
	void *buffer;
	size_t bufferSize;
};

struct FoundFile {
	char name[256];

	int flags;

	int8_t changedMinute;
	int8_t changedHour;
	int8_t changedDay;
	int8_t changedMonth;
	int16_t changedYear;

	void *priv;
};

#define MAX_ARCHIVES 4
void assetArchive(int slot, const char *archive);

struct Asset *assetOpen(const char *file);
size_t assetRead(struct Asset* a, void* buf, size_t sz);
void assetSeek(struct Asset* a, long offset, int whence);
void assetClose(struct Asset* a);

/* Load file in user dir */
struct Asset *assetUserOpen(const char *file, bool write);
/* Write to a user asset */
size_t assetUserWrite(struct Asset *a, const void *buf, size_t sz);

bool assetDirFirst(struct FoundFile *ff, const char *dir);
bool assetDirNext(struct FoundFile *ff);
void assetDirClose(struct FoundFile *ff);

/* Logging / Error handling */
void logNorm(const char *fmt, ...);
#ifndef RELEASE
#define logDebug(...) logNorm(__VA_ARGS__)
#else
#define logDebug(...)
#endif
void fail(const char *fmt, ...);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
