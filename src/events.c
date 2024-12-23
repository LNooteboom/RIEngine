#include <events.h>
#include "system_events.h"

#include <SDL2/SDL.h>
#include <stddef.h>

#include <main.h>
#include <mem.h>
#include <audio.h>
#include <string.h>
#include <assets.h>
#include <gfx/draw.h>

#define LOAD_FRAMES 4

/* Public vars */
float deltaTime;
float fps;
float frameTimeMs;
struct UpdateTiming updateTiming;
float gameSpeed = 1;

const char *sceneName;

bool eventBlockUpdates;


struct EvCallback {
	union {
		void (*input)(void *arg, struct Event *ev);
		void (*update)(void *arg);
	};
	void *arg;
	int typeMask;
};

struct DrawUpdate {
	int priority;
	void (*draw)(void *arg);
	void *arg;
};

static struct Vector updateLists[NROF_UPDATES];
static struct Vector inputHandlers;

static struct Vector drawUpdates;

static bool doSceneSwitch;
static int sceneSwitchTime;
static char newSceneName[32];
void sceneStart(const char *name);
void sceneEnd(void);
static void doSwitchScene(void);

static uint64_t tickInterval;

static void (*loadStart)(void *arg, int prefade);
static void (*loadEnd)(void *arg);
static bool (*loadUpdate)(void *arg);
static void *loadArg;
static int loadFrames;
static uint64_t loadTickAccum;

extern void physicsUpdate(void);

static void notify(struct Vector *vec, struct Event *ev, bool update) {
	for (unsigned int i = 0; i < vecCount(vec); i++) {
		struct EvCallback *ec = vecAt(vec, i);
		if (update) {
			ec->update(ec->arg);
		} else if (ec->typeMask & ev->type) {
			ec->input(ec->arg, ev);
		}
	}
}


void eventInit(void) {
	for (int i = 0; i < NROF_UPDATES; i++) {
		vecCreate(&updateLists[i], sizeof(struct EvCallback));
	}
	vecCreate(&inputHandlers, sizeof(struct EvCallback));
	vecCreate(&drawUpdates, sizeof(struct DrawUpdate));

	loadFrames = LOAD_FRAMES;
	tickInterval = SDL_GetPerformanceFrequency() / 60;

	/* Load game controller mappings file, if exists */
	struct Asset *a = assetUserOpen("gamepads.txt", false);
	if (a) {
		logNorm("Loading gamepad mappings...\n");
		int err = SDL_GameControllerAddMappingsFromRW(a->rwOps, 0);
		if (err < 0)
			logNorm("Failed to load gamepad mappings: %s\n", SDL_GetError());
		else
			logNorm("%d mappings loaded\n", err);
		assetClose(a);
	}

	
}
void eventFini(void) {
	
}

static void eventDrawUpdate(void) {
	for (unsigned int i = 0; i < vecCount(&drawUpdates); i++) {
		struct DrawUpdate *upd = vecAt(&drawUpdates, i);
		if (upd->draw) {
			drawReset();
			upd->draw(upd->arg);
		}
	}
}

static void eventMainUpdate(void) {
	doSwitchScene();

	/* Update */
	uint64_t start = SDL_GetPerformanceCounter();
	uint64_t phys = start, physEngine = start, norm = start, late = start;
	if (!eventBlockUpdates) {
		notify(&updateLists[UPDATE_PHYS], NULL, true);
		phys = SDL_GetPerformanceCounter();
		physicsUpdate();
		physEngine = SDL_GetPerformanceCounter();
		notify(&updateLists[UPDATE_NORM], NULL, true);
		norm = SDL_GetPerformanceCounter();
		notify(&updateLists[UPDATE_LATE], NULL, true);
		late = SDL_GetPerformanceCounter();
	}

	notify(&updateLists[UPDATE_UI], NULL, true);
	uint64_t ui = SDL_GetPerformanceCounter();

	/* Do not block draw */
	eventDrawUpdate();
	uint64_t draw = SDL_GetPerformanceCounter();

//#ifndef RELEASE
	float freq = SDL_GetPerformanceFrequency() / 1000.0f;
	updateTiming.phys = (phys - start) / freq;
	updateTiming.physEngine = (physEngine - phys) / freq;
	updateTiming.norm = (norm - physEngine) / freq;
	updateTiming.late = (late - norm) / freq;
	updateTiming.ui = (ui - late) / freq;
	updateTiming.draw = (draw - ui) / freq;
	updateTiming.total = (draw - start) / freq;
//#endif
}
	

static void doEvent(SDL_Event *sdlEv) {
	struct Event ev;
	if (sdlEv->type == SDL_QUIT) {
		quit = true;
		return;
	}
	if (inputHandleEvent(&ev, sdlEv)) {
		notify(&inputHandlers, &ev, false);
	}
}

void eventLoop(void) {
	SDL_Event sdlEv;
	float tfps = 60; /* Target fps */
	int vsync = 0;
	drawSetVsync(vsync);
	
	deltaTime = 1.0f / tfps;

	uint64_t freq = SDL_GetPerformanceFrequency();
	tickInterval = freq / tfps;
	uint64_t accum = SDL_GetPerformanceCounter();

	/* FPS Counter */
	uint64_t fCntStart = accum;
	int fCnt = tfps;
	uint64_t fTime = 0;

	while (!quit) {
		/* Update FPS */
		if (--fCnt == 0) {
			fCnt = tfps;
			uint64_t t = SDL_GetPerformanceCounter();
			fps = (float)freq / (t - fCntStart) * fCnt;
			fCntStart = t;

			frameTimeMs = fTime / tfps / freq * 1000.0f;
			fTime = 0;
		}
		uint64_t start = SDL_GetPerformanceCounter();

		while (SDL_PollEvent(&sdlEv)) {
			doEvent(&sdlEv);
		}
		eventMainUpdate();

		if (loadFrames) {
			loadFrames--;
			if (!loadFrames && loadEnd) {
				loadEnd(loadArg);
			}
		}
		
		uint64_t ticks = SDL_GetPerformanceCounter();
		fTime += ticks - start;
		uint64_t next = accum + tickInterval;
		if (ticks < next) {
			uint64_t ms = (next - ticks) * 1000 / freq;
			if (ms > 2ULL) {
				SDL_Delay(ms - 2ULL);
			}
			while (SDL_GetPerformanceCounter() < next) {}
			accum += tickInterval;
		}
		else {
			accum = ticks;
		}
	}
}


void addUpdate(int type, void (*callback)(void *arg), void *arg) {
	struct EvCallback *ec = vecInsert(&updateLists[type], -1);
	ec->update = callback;
	ec->arg = arg;
}

void removeUpdate(int type, void (*callback)(void *arg)) {
	for (unsigned int i = 0; i < vecCount(&updateLists[type]); i++) {
		struct EvCallback *ec = vecAt(&updateLists[type], i);
		if (ec->update == callback) {
			vecDelete(&updateLists[type], i);
			break;
		}
	}
}

void addDrawUpdate(int priority, void (*callback)(void *arg), void *arg) {
	unsigned int i;
	for (i = 0; i < vecCount(&drawUpdates); i++) {
		struct DrawUpdate *upd = vecAt(&drawUpdates, i);
		if (upd->priority > priority) {
			break;
		}
	}

	struct DrawUpdate *newUpdate = vecInsert(&drawUpdates, i);
	newUpdate->priority = priority;
	newUpdate->draw = callback;
	newUpdate->arg = arg;
}
void removeDrawUpdate(int priority) {
	for (unsigned int i = 0; i < vecCount(&drawUpdates); i++) {
		struct DrawUpdate *upd = vecAt(&drawUpdates, i);
		if (upd->priority == priority) {
			vecDelete(&drawUpdates, i);
			break;
		}
	}
}

void addInputHandler(int evMask, void (*callback)(void *arg, struct Event *ev), void *arg) {
	struct EvCallback *ec = vecInsert(&inputHandlers, -1);
	ec->input = callback;
	ec->arg = arg;
	ec->typeMask = evMask;
}

void removeInputHandler(void (*callback)(void *arg, struct Event *ev)) {
	for (unsigned int i = 0; i < vecCount(&inputHandlers); i++) {
		struct EvCallback *ec = vecAt(&inputHandlers, i);
		if (ec->input == callback) {
			vecDelete(&inputHandlers, i);
			break;
		}
	}
}

void loadFuncSet(void (*start)(void *arg, int prefade), void (*end)(void *arg), bool (*update)(void *arg), void *arg) {
	loadStart = start;
	loadEnd = end;
	loadUpdate = update;
	loadArg = arg;
}
void loadUpdatePoll(void) {
	if (!loadFrames)
		return;
	uint64_t ticks = SDL_GetPerformanceCounter();
	if (ticks >= loadTickAccum + tickInterval || ticks < tickInterval) {
		SDL_Event sdlEv;
		while (SDL_PollEvent(&sdlEv)) {
			if (sdlEv.type == SDL_QUIT) {
				quit = true;
			}
		}
		bool draw = false;
		if (loadUpdate)
			draw = loadUpdate(loadArg);
		if (draw)
			eventDrawUpdate();
		loadTickAccum = SDL_GetPerformanceCounter();
	} else {
		SDL_GetPerformanceCounter();
	}
}

void loadingDelay(int frames) {
	for (int i = 0; i < frames; i++) {
		SDL_Delay(10);
		loadUpdatePoll();
	}
}

void switchSceneDelayed(const char *newScene, int delay) {
	strncpy(newSceneName, newScene, 32);
	doSceneSwitch = true;
	sceneSwitchTime = delay;

	loadTickAccum = SDL_GetPerformanceCounter();
	if (loadStart)
		loadStart(loadArg, delay);
}
void switchScene(const char *newScene) {
	switchSceneDelayed(newScene, 0);
}


static void doSwitchScene(void) {
	struct Event ev;
	if (!doSceneSwitch)
		return;
	if (sceneSwitchTime) {
		sceneSwitchTime--;
		return;
	}

	loadFrames = LOAD_FRAMES;

	if (sceneName) {
		/* End this scene */
		ev.type = EVENT_END_SCENE;
		notify(&inputHandlers, &ev, false);
		componentListEndScene();
	}

	/* Reset camera */
	camX = camY = 0;
	cam3DFov = DEG2RAD(60);
	cam3DLook(0, 0, 0, 0, 0, -1, 0, 1, 0);
	gameSpeed = 1;

	audioStopMusic();

	/* Start new scene */
	sceneName = newSceneName;
	ev.type = EVENT_START_SCENE;
	notify(&inputHandlers, &ev, false);

	doSceneSwitch = false;
}



unsigned int getTime(void) {
	return SDL_GetTicks() / 1000;
}
