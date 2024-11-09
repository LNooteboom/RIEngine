#ifndef INPUT_H
#define INPUT_H

#include <events.h>

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

extern bool gamepadConnected;

void getMousePos(float *x, float *y);

void setMouseRelative(bool rel);

void getMouseRelativeMove(int *x, int *y);

float getGamepadAxis(enum GamepadAxis ax);

#endif