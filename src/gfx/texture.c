#include <main.h>
#include <gfx/texture.h>
#include <mem.h>
#include <assets.h>
#include <events.h>
#include <string.h>

#include "../stb_image.h"

struct TextureFileHeader {
	char signature[4];
	unsigned int w, h;
};

static const char unexpectedEOFMsg[] = "Failed to read %s: Unexpected end of file encountered\n";

static unsigned char *lTex(size_t *bufSz, struct TextureFileHeader *header, struct Asset *a, const char *fileName) {
	/* Load header */
	size_t rd = assetRead(a, header, sizeof(*header));
	if (rd != sizeof(*header)) {
		logNorm(unexpectedEOFMsg, fileName);
		return NULL;
	}

	/* Check signature */
	if (memcmp("TEX0", &header->signature[0], 4)) {
		logNorm("Texture file is corrupted: %s\n", fileName);
		return NULL;
	}

	/* Load the image data */
	size_t size = header->w * header->h * 4; /* 32 bpp */
	unsigned char *buf = globalAlloc(size);
	rd = assetRead(a, buf, size);
	if (rd != size) {
		logNorm(unexpectedEOFMsg, fileName);
		globalDealloc(buf);
		return NULL;
	}

	*bufSz = size;
	return buf;
}
bool loadPixels(int *w, int *h, int *channels, unsigned char **pixels, const char *fileName, int flags) {
	struct Asset *a = assetOpen(fileName);
	if (!a) {
		logNorm("Texture file %s does not exist\n", fileName);
		return false;
	}

	int fnlen = (int)strlen(fileName);
	bool isTex = !memcmp(".tex", &fileName[fnlen - 4], 4);

	int x, y, chan;
	unsigned char *buf;
	size_t texSize;
	if (isTex) {
		struct TextureFileHeader hdr;
		buf = lTex(&texSize, &hdr, a, fileName);
		x = hdr.w;
		y = hdr.h;
		chan = 4;
	} else {
		stbi_set_flip_vertically_on_load(0); /* Dont flip */
		if (flags & TEXTURE_HIBIT) {
			buf = (unsigned char*)stbi_load_16_from_memory(a->buffer, (int)a->bufferSize, &x, &y, &chan, *channels);
		} else {
			buf = stbi_load_from_memory(a->buffer, (int)a->bufferSize, &x, &y, &chan, *channels);
		}

		/* Set fully transparent pixels to black */
		if (chan == STBI_rgb_alpha) {
			for (int i = 0; i < x * y; i++) {
				unsigned char *px = &buf[i * 4];
				if (px[3] == 0) {
					*((uint32_t *)px) = 0;
				}
			}
		}
	}

	if (!buf) {
		logNorm("Failed to load image %s\n", fileName);
		assetClose(a);
		return false;
	}
	assetClose(a);
	a = NULL;

	*w = x;
	*h = y;
	*channels = chan;
	*pixels = buf;

	return true;
}
void deletePixels(unsigned char *pixels) {
	globalDealloc(pixels);
}

struct Texture *loadTexture2(const char *fileName, int flags) {
	unsigned char *pixels;
	int w, h, chan = 4;
	bool found = loadPixels(&w, &h, &chan, &pixels, fileName, flags);
	if (!found)
		return NULL;

	struct Texture *ret = loadTextureFromPixels(w, h, pixels, flags);

	deletePixels(pixels);

	loadUpdatePoll();

	return ret;
}