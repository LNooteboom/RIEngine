#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define DEFINE_MAIN \
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) { \
	return runEngine(); \
}
#else
#define DEFINE_MAIN \
int main(int argc, char** argv) { \
	(void)argc; \
	(void)argv; \
	runEngine(); \
	return 0; \
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct EngineSettings {
	/* Resolution */
	int resW, resH;

	/* Audio volume */
	int bgmVol, sfxVol;

	/* Game Info */
	const char *gameTitle;
	const char *gameVersion;

	/* Draw priorities */
	int draw3DStart;
	int draw3DNoCull;
	int drawPhysDebug;
	int draw3DOverlay;
	int draw2DLowRes;
	int draw2DHiRes;
	int drawRttEnd;
};

extern const struct EngineSettings defaultEngineSettings;
extern struct EngineSettings *engineSettings;

/* Game info */
extern const char *gameDir;
extern const char *gameUserDir;
extern const char *assetDir;

/* Set to true to end the game */
extern bool quit;

/* DEFINE THESE IN YOUR GAME */
extern const char* gameDirName;
struct EngineSettings *gameGetEngineSettings(void);
void *gameInit(void);
void gameFini(void *arg);

int runEngine(void);

void showError(const char* title, const char* message);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
