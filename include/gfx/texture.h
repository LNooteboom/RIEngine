#ifndef GFX_TEXTURE_H
#define GFX_TEXTURE_H

#include <stddef.h>
#include <stdbool.h>

#define TEXTURE_SRGB 1
#define TEXTURE_POINT 2
#define TEXTURE_MIPMAP 4
#define TEXTURE_HIBIT 8

#ifdef __cplusplus
extern "C" {
#endif

struct Texture {
	unsigned int w, h;

	union {
		unsigned int glTexture;
		void *d3dTexture;
	};
	void *d3dResourceView;

	int refs;
	int flags;
};

bool loadPixels(int *w, int *h, int *channels, unsigned char **pixels, const char *fileName, int flags);
void deletePixels(unsigned char *pixels);

struct Texture *loadTexture2(const char *fileName, int flags);
struct Texture *loadTextureCube(const char *name);
struct Texture *loadTextureFromPixels(int w, int h, unsigned char *pixels, int flags);

#define loadTexture(fn) loadTexture2(fn, 0)
#define loadTexture3D(fn) loadTexture2(fn, TEXTURE_SRGB)

void deleteTexture(struct Texture *texture);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
