#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define N_AUDIO_CHANNELS	64

void audioSetSFXVolume(int volume);
int audioGetSFXVolume(void);

void audioSetMusicVolume(int volume);
int audioGetMusicVolume(void);

/**
 * Start the music
 */
void audioStartMusic(const char *name);

/**
 * Stop the music
 */
void audioStopMusic(void);

/**
 * Pause the music
 */
void audioPauseMusic(bool paused);

/**
 * Play a sound effect
 */
void audioPlaySFX(int channel, int volume, int panning);

/**
 * Stop playing sound effect
 */
void audioStopSFX(int channel);

/**
 * Load a sound effect
 */
void audioLoadSFX(int channel, const char *name);

/**
 * Unload a sound effect
 */
void audioUnloadSFX(int channel);

/*
* Unload all sfx
*/
void audioUnloadAllSFX(void);



/* Control functions */

/**
 * Start the audio system
 */
void audioInit(void);

/**
 * Stop the audio system
 */
void audioFini(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
