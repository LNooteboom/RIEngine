#ifndef SYSTEM_EVENTS_H
#define SYSTEM_EVENTS_H

#include <events.h>
#include <stdint.h>

/**
 * Initialize the event system
 */
void eventInit(void);

void eventFini(void);

/**
 * Gets the next event and puts it in ev
 */
void eventLoop(void);

/**
 * Send the 'Start scene' event to subscribers
 */
void eventStartScene(void);

/**
 * Send the 'End scene' event to subscribers
 */
void eventEndScene(void);

void inputInit(void);

void inputFini(void);

bool inputHandleEvent(struct Event *event, void *sdlEvent);

#endif
