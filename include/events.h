#ifndef EVENTS_H
#define EVENTS_H

#include <SDL2/SDL_keycode.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EVENT_KEY		1
#define EVENT_MOUSEBTN	2
#define EVENT_START_SCENE 4
#define EVENT_END_SCENE 8
#define EVENT_MOUSEWHEEL 16
#define EVENT_GAMEPAD_BTN 32

#define UPDATE_PHYS		0
#define UPDATE_NORM		1
#define UPDATE_LATE		2
#define UPDATE_UI		3
#define NROF_UPDATES	4

enum GamepadAxis {
	GP_AXIS_L_X,
	GP_AXIS_L_Y,
	GP_AXIS_R_X,
	GP_AXIS_R_Y,
	GP_AXIS_TRIG_L,
	GP_AXIS_TRIG_R
};
enum GamepadButton {
	GP_BUTTON_INVALID = -1,
	GP_BUTTON_A,
	GP_BUTTON_B,
	GP_BUTTON_X,
	GP_BUTTON_Y,
	GP_BUTTON_SHOULDER_L,
	GP_BUTTON_SHOULDER_R,
	GP_BUTTON_DPAD_UP,
	GP_BUTTON_DPAD_DOWN,
	GP_BUTTON_DPAD_LEFT,
	GP_BUTTON_DPAD_RIGHT,
	GP_BUTTON_STICK_L,
	GP_BUTTON_STICK_R,
	GP_BUTTON_START,
	GP_BUTTON_SELECT,
};

/**
 * type: one of EVENT_KEY, EVENT_MOUSEBTN etc
 * param:
 *   keyboard: SDLK_*
 *   mouse: button
 * param2:
 *   keyboard & mouse: 1 if pressed, 0 if released
 */
struct Event {
	unsigned int type;
	int param;
	int param2;
};


struct UpdateTiming {
	float phys;
	float physEngine;
	float norm;
	float late;
	float ui;
	float draw;
	float total;
};

extern float deltaTime;
extern float fps;
extern float frameTimeMs;
extern struct UpdateTiming updateTiming;
extern bool eventBlockUpdates;
extern float gameSpeed;

extern bool gamepadConnected;

extern const char *sceneName;

void getMousePos(float *x, float *y);

float getGamepadAxis(enum GamepadAxis ax);


/**
 * Add an update function
 * @param type UPDATE_PHYS, UPDATE_NORM etc
 * @param callback The function to call
 * @param arg Argument to give to the function
 */
void addUpdate(int type, void (*callback)(void *arg), void *arg);

/**
 * Remove an update function
 */
void removeUpdate(int type, void (*callback)(void *arg));


void addDrawUpdate(int priority, void (*callback)(void *arg), void *arg);
void removeDrawUpdate(int priority);

/**
 * Add an input handler
 * @param evMask bitmask of desired events
 */
void addInputHandler(int evMask, void (*callback)(void *arg, struct Event *ev), void *arg);

/**
 * Remove an input handler
 */
void removeInputHandler(void (*callback)(void *arg, struct Event *ev));

/*
 * Update function returns true if a draw update is needed
 */
void loadFuncSet(void (*start)(void *arg, int prefade), void (*end)(void *arg), bool (*update)(void *arg), void *arg);
void loadUpdatePoll(void);

void loadingDelay(int frames);


/**
 * Get the time in seconds
 */
unsigned int getTime(void);

void switchScene(const char *newScene);
void switchSceneDelayed(const char *newScene, int delay);

#ifdef __cplusplus
} // extern "C"
#endif

#endif