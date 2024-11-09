#include <events.h>
#include <basics.h>
#ifdef JOLT
#include "jolt.h"
#endif

void physicsInit(void) {
#ifdef JOLT
	joltInit();
#endif
}

void physicsFini(void) {
#ifdef JOLT
	joltFini();
#endif
}

void physicsUpdate(void) {
#ifdef JOLT
	joltUpdate();
#endif
}
