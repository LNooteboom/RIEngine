#ifndef TTF_H
#define TTF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum Font {
	FONT_NOTO_SERIF,
	N_FONTS
};

#define TTF_FLAG_BOLD 1
#define TTF_FLAG_ITALIC 2
#define TTF_FLAG_UNDERLINE 4
#define TTF_FLAG_STRIKETHROUGH 8
/* Outline mask: 0xF0 */

struct Texture *ttfLoad(const char *text, enum Font font, int size, int flags);
int ttfWidth(uint32_t ch); /* Uses settings from last ttfLoad */

#ifdef __cplusplus
} // extern "C"
#endif

#endif