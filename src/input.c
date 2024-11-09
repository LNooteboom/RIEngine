#include <input.h>
#include "system_events.h"

#include <SDL2/SDL.h>
#include <assets.h>
#include <gfx/draw.h>

bool gamepadConnected;

static SDL_GameController *gamepad;

void getMousePos(float *x, float *y) {
	int i, j;
	SDL_GetMouseState(&i, &j);
	*x = (float)i * winW / realWinW;
	*y = (float)j * winH / realWinH;
	/**x = ((float)i / winW * 2) - 1.0;
	*y = -( ((float)j / winH * 2) - 1.0 );*/
}

void setMouseRelative(bool rel) {
	SDL_SetRelativeMouseMode(rel);
	SDL_GetRelativeMouseState(NULL, NULL);
}

void getMouseRelativeMove(int *x, int *y) {
	SDL_GetRelativeMouseState(x, y);
}

static const SDL_GameControllerAxis gamepadAxisMap[] = {
	SDL_CONTROLLER_AXIS_LEFTX,
	SDL_CONTROLLER_AXIS_LEFTY,
	SDL_CONTROLLER_AXIS_RIGHTX,
	SDL_CONTROLLER_AXIS_RIGHTY,
	SDL_CONTROLLER_AXIS_TRIGGERLEFT,
	SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
};
#define GAMEPAD_BUTTON_MAP_MAX (SDL_CONTROLLER_BUTTON_DPAD_RIGHT + 1)
static const enum GamepadButton gamepadButtonMap[GAMEPAD_BUTTON_MAP_MAX] = {
	GP_BUTTON_A, GP_BUTTON_B, GP_BUTTON_X, GP_BUTTON_Y,
	GP_BUTTON_INVALID, GP_BUTTON_SELECT, GP_BUTTON_START,
	GP_BUTTON_STICK_L, GP_BUTTON_STICK_R,
	GP_BUTTON_SHOULDER_L, GP_BUTTON_SHOULDER_R,
	GP_BUTTON_DPAD_UP, GP_BUTTON_DPAD_DOWN,
	GP_BUTTON_DPAD_LEFT, GP_BUTTON_DPAD_RIGHT
};

float getGamepadAxis(enum GamepadAxis ax) {
	if (gamepadConnected) {
		SDL_GameControllerAxis sa = gamepadAxisMap[ax];
		int16_t v = SDL_GameControllerGetAxis(gamepad, sa);
		return v >= 0 ? v / (float)INT16_MAX : v / (float)(-INT16_MIN);
	} else {
		return 0;
	}
}

bool inputHandleEvent(struct Event *ev, void *sdlEvent) {
	SDL_Event *sdlEv = sdlEvent;
	switch (sdlEv->type) {
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		ev->type = EVENT_KEY;
		ev->param = sdlEv->key.keysym.sym;
		ev->param2 = (sdlEv->key.state == SDL_PRESSED) ? 1 : 0;
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		ev->type = EVENT_MOUSEBTN;
		ev->param = sdlEv->button.button;
		ev->param2 = (sdlEv->button.state == SDL_PRESSED) ? 1 : 0;
		break;
	case SDL_MOUSEWHEEL:
		ev->type = EVENT_MOUSEWHEEL;
		ev->param = sdlEv->wheel.y;
		ev->param2 = sdlEv->wheel.x;
		break;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
	{
		ev->type = EVENT_GAMEPAD_BTN;
		SDL_GameControllerButton b = sdlEv->cbutton.button;
		if (b >= 0 && b < GAMEPAD_BUTTON_MAP_MAX) {
			enum GamepadButton gb = gamepadButtonMap[sdlEv->cbutton.button];
			ev->param = gb;
			ev->param2 = sdlEv->cbutton.state == SDL_PRESSED ? 1 : 0;
		} else {
			return false;
		}
		break;
	}
	default:
		return false;
	}
#ifndef RELEASE
	if (ev->type == EVENT_KEY && !ev->param2 && ev->param == SDLK_BACKSPACE) {
		switchScene(sceneName);
	}
#endif
	return true;
}

void inputInit(void) {
	int js = SDL_NumJoysticks();
	int gamepads = 0;
	int gp = -1;
	for (int i = 0; i < js; i++) {
		if (SDL_IsGameController(i)) {
			if (gp == -1)
				gp = i;
			gamepads++;
		}
	}

	logNorm("%d gamepads connected\n", gamepads);
	if (gp != -1) {
		gamepad = SDL_GameControllerOpen(gp);
		if (!gamepad) {
			logNorm("Cannot open gamepad %d: %s\n", SDL_GetError());
		} else {
			logNorm("Using gamepad: %s\n", SDL_GameControllerName(gamepad));
			gamepadConnected = true;
		}
	}
}

void inputFini(void) {
	if (gamepad) {
		SDL_GameControllerClose(gamepad);
		gamepad = NULL;
	}
}