#include <gfx/ttf.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <assets.h>
#include <gfx/texture.h>
#include "gfx.h"

static TTF_Font *fonts[N_FONTS];
static int curFont;
struct Asset *fontAssets[1];

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

	fontAssets[0] = assetOpen("ascii/NotoSerifJP-Regular.otf");
	if (fontAssets[0]) {
		fonts[0] = TTF_OpenFontRW(fontAssets[0]->rwOps, 0, 32);
	}

	//assetClose(notoSerif); Crashes when trying to render with closed asset
}
void ttfFini(void) {
	for (int i = 0; i < N_FONTS; i++) {
		TTF_CloseFont(fonts[i]);
	}
	assetClose(fontAssets[0]);
}