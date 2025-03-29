#include <gfx/ttf.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <assets.h>
#include <gfx/texture.h>
#include "gfx.h"

static TTF_Font *fonts[TTF_MAX_FONTS];
static int curFont;
struct Asset *fontAssets[TTF_MAX_FONTS];

int ttfLoadFont(int idx, const char* file) {
	fontAssets[idx] = assetOpen(file);
	if (!fontAssets[idx])
		return -1;
	
	fonts[idx] = TTF_OpenFontRW(fontAssets[idx]->rwOps, 0, 32);
	return fonts[idx] ? 0 : -1;
}

void ttfDeleteFont(int idx) {
	if (fonts[idx]) {
		TTF_CloseFont(fonts[idx]);
		fonts[idx] = NULL;
	}
	if (fontAssets[idx]) {
		assetClose(fontAssets[idx]);
		fontAssets[idx] = NULL;
	}
}

struct Texture *ttfLoad(const char *text, enum Font font, int size, int flags) {
	SDL_Color col = {
		.r = 0xff, .g = 0xff, .b = 0xff, .a = 0xff
	};
	TTF_Font *f = fonts[font];
	curFont = font;
	TTF_SetFontSize(f, size);

	int style = 0;
	if (flags & TTF_FLAG_BOLD)
		style |= TTF_STYLE_BOLD;
	if (flags & TTF_FLAG_ITALIC)
		style |= TTF_STYLE_ITALIC;
	if (flags & TTF_FLAG_UNDERLINE)
		style |= TTF_STYLE_UNDERLINE;
	if (flags & TTF_FLAG_STRIKETHROUGH)
		style |= TTF_STYLE_STRIKETHROUGH;
	TTF_SetFontStyle(f, style);

	TTF_SetFontOutline(f, (flags & 0xF0) >> 4);

	SDL_Surface *surf = TTF_RenderUTF8_Blended(f, text, col);
	if (!surf) {
		logNorm("Failed to render text %s\n");
		return NULL;
	}
	SDL_LockSurface(surf);
	struct Texture *tex = loadTextureFromPixels(surf->pitch / 4, surf->h, surf->pixels, 0);
	SDL_UnlockSurface(surf);
	SDL_FreeSurface(surf);
	return tex;
}

int ttfWidth(uint32_t ch) {
	int w;
	TTF_GlyphMetrics32(fonts[curFont], ch, NULL, NULL, NULL, NULL, &w);
	return w;
}

void ttfInit(void) {
	int err = TTF_Init();
	if (err < 0) {
		logNorm("Couldn't initialize TTF: %s\n", SDL_GetError());
		return;
	}

	ttfLoadFont(TTF_FONT_DEFAULT, TTF_FONT_DEFAULT_FILE);
}
void ttfFini(void) {
	for (int i = 0; i < TTF_MAX_FONTS; i++) {
		ttfDeleteFont(i);
	}
}