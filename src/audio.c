#include <audio.h>
#include <assets.h>
#include <stdio.h>
#include <events.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#define FREQUENCY 22050

static int sfxVol;
static int musVol;

struct Asset *sfxAssets[N_AUDIO_CHANNELS];
Mix_Chunk *sfxChunks[N_AUDIO_CHANNELS];

static struct Asset* musicAsset;
static Mix_Music* music;


/* Music */

void audioStartMusic(const char *name) {
	if (musicAsset) {
		audioStopMusic();
	}

	musicAsset = assetOpen(name);
	if (!musicAsset) {
		logNorm("Cannot find music %s\n", name);
		return;
	}
	music = Mix_LoadMUS_RW(musicAsset->rwOps, 0);
	if (!music) {
		logNorm("Failed to load music %s: %s\n", name, Mix_GetError());
		return;
	}

	Mix_PlayMusic(music, -1);

	loadUpdatePoll();
}

void audioStopMusic(void) {
	Mix_HaltMusic();
	if (music) {
		Mix_FreeMusic(music);
		music = NULL;
		assetClose(musicAsset);
		musicAsset = NULL;
	}
}

void audioPauseMusic(bool paused) {
	if (paused) {
		Mix_PauseMusic();
	} else {
		Mix_ResumeMusic();
	}
}

void audioSetMusicVolume(int volume) {
	musVol = volume;
	Mix_VolumeMusic(volume);
}

int audioGetMusicVolume(void) {
	return musVol;
}


/* SFX */

void audioSetSFXVolume(int volume) {
	sfxVol = volume;
	Mix_Volume(-1, volume);
}

int audioGetSFXVolume(void) {
	return sfxVol;
}

void audioPlaySFX(int channel, int volume, int panning) {
	if (sfxChunks[channel]) {
		int v = volume / 2 * sfxVol / 128;
		Mix_Volume(channel, v);
		if (panning < 0) {
			Mix_SetPanning(channel, 254, 254 + panning);
		} else {
			Mix_SetPanning(channel, 254 - panning, 254);
		}
		Mix_PlayChannel(channel, sfxChunks[channel], 0);
	}
}

void audioStopSFX(int channel) {
	Mix_FadeOutChannel(channel, 100);
}

void audioLoadSFX(int channel, const char *name) {
	struct Asset *a = assetOpen(name);
	if (!a) {
		logNorm("Could not find SFX: %s\n", name);
		return;
	}
	Mix_Chunk *ch = Mix_LoadWAV_RW(a->rwOps, 0);
	if (!ch) {
		logNorm("Could not load WAV %s: %s\n", name, Mix_GetError());
		assetClose(a);
		return;
	}
	Mix_VolumeChunk(ch, 64);
	sfxAssets[channel] = a;
	sfxChunks[channel] = ch;

	loadUpdatePoll();
}

void audioUnloadSFX(int channel) {
	Mix_HaltChannel(channel);
	if (sfxChunks[channel]) {
		Mix_FreeChunk(sfxChunks[channel]);
		sfxChunks[channel] = NULL;
	}
	struct Asset *a = sfxAssets[channel];
	if (a) {
		assetClose(a);
		sfxAssets[channel] = NULL;
	}
}
void audioUnloadAllSFX(void) {
	for (int i = 0; i < N_AUDIO_CHANNELS; i++) {
		audioUnloadSFX(i);
	}
}

/* Init and Fini */

void audioInit(void) {
	Mix_Init(MIX_INIT_OGG);

	int bufSize = 512;
	int error = Mix_OpenAudioDevice(FREQUENCY, AUDIO_S16SYS, 2, bufSize, NULL, 0);
	if (error == -1) {
		logNorm("Failed to open audio: %s\n", Mix_GetError());
		return;
	}

	sfxVol = engineSettings->sfxVol;
	musVol = engineSettings->bgmVol;

	int gotFreq, gotChannels;
	Uint16 gotFmt;
	Mix_QuerySpec(&gotFreq, &gotFmt, &gotChannels);

	Mix_AllocateChannels(N_AUDIO_CHANNELS);
}

void audioFini(void) {
	Mix_CloseAudio();
	Mix_Quit();
}
