#include <main.h>
#include <events.h>
#include <SDL2/SDL.h>
#include <ecs.h>
#include <basics.h>
#include <audio.h>
#include <assets.h>

#include "system_events.h"
#include "system_assets.h"
#include "system_init.h"

const struct EngineSettings defaultEngineSettings = {
	854, 480,
	64, 64,
	"Undefined Game",
	"0.01a",
	399, 498, 499, 1199, 1599, 3199, 3999
};
struct EngineSettings *engineSettings;

const char *gameDir;
const char *gameUserDir;

bool quit;
bool debugEnabled;
struct ConfigFile *configFile;

int runEngine(void) {
	if (SDL_Init(SDL_INIT_EVERYTHING))
		fail("SDL_Init failed\n");

	memInit();
	ecsInit();

	/* Get game folders */
	gameDir = SDL_GetBasePath();
	gameUserDir = SDL_GetPrefPath(NULL, "DreamingMemories");
	logDebug("Game directories: %s, %s\n", gameDir, gameUserDir);

	assetInit();

	engineSettings = gameGetEngineSettings();

	quit = false;

	/* Initialize event system */
	eventInit();
	inputInit();

	/* Load modules */
	basicsInit();
	ichigoHeapInit();
	assetArchive(0, "ri.dat");
	drawInit();
	physicsInit();
	audioInit();

	void *gameStruct = gameInit();

	/* Load default scene */
	switchScene("@menu");

	/* Event handler loop */
	eventLoop();

	/* Quit game */
	gameFini(gameStruct);

	/* Unload modules */
	audioFini();
	physicsFini();
	drawFini();
	ichigoHeapFini();
	basicsFini();

	inputFini();
	eventFini();

	assetArchive(0, NULL);
	assetFini();

	return 0;
}
