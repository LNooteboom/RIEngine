#include <audio.h>
#include <assets.h>
#include <stdio.h>
#include <events.h>
#include <SDL2/SDL.h>
#include <vec.h>

#define FREQUENCY 44100
#define MAX_SFX_SOURCES 64

#define m128 __m128
#define m128i __m128i

struct AudioChannel {
	bool play;
	uint32_t sample, length;
	float left, right;

	SDL_AudioFormat fmt;
	uint8_t *data; /* The audio samples */
};

static struct AudioChannel sfxChannels[N_AUDIO_CHANNELS];

static struct Asset* musicAsset;
struct AudioChannel musicChannel;

static int sfxVol;
static int musVol;

static SDL_AudioSpec audioSpec;
static SDL_AudioDeviceID audioDeviceID;
static float *audioAccum;
static uint32_t audioAccumSamples;

static uint32_t addChannelSamplesS16(struct AudioChannel *chan, float masterVolume) {
	/* Evil floating point hacking here, beware of dragons */
	int16_t *src = (int16_t *)chan->data + chan->sample;
	uint32_t srcLen = (chan->length - chan->sample) & ~1; /* Make sure it's an even number */
	uint32_t s = 0;

	float vLeft = masterVolume * chan->left;
	float vRight = masterVolume * chan->right;
	m128 volume = _mm_set_ps(vLeft, vRight, vLeft, vRight);

	m128i makeUnsigned = _mm_set1_epi16(0x8000);
	m128i magic = _mm_set1_epi16(0x4380); /* Exponent part of float */
	m128 magic2 = _mm_set1_ps(257);

	//for (int i = 0; i < audioAccumSamples / 8 && i < srcLen / 8; i++) {
	//	m128i sdat = _mm_loadu_si128((m128i *)(src + s));

	//	sdat = _mm_xor_si128(sdat, makeUnsigned); /* Wrap to unsigned short */
	//	m128i lo = _mm_unpacklo_epi16(sdat, magic); /* unpack and turn it into float */
	//	m128i hi = _mm_unpackhi_epi16(sdat, magic);
	//	m128 lof = _mm_sub_ps(_mm_castsi128_ps(lo), magic2); /* subtract bias value */
	//	m128 hif = _mm_sub_ps(_mm_castsi128_ps(hi), magic2);

	//	/* Multiply the volume */
	//	lof = _mm_mul_ps(lof, volume);
	//	hif = _mm_mul_ps(hif, volume);

	//	/* Add to accumulator */
	//	_mm_store_ps(audioAccum + s    , _mm_add_ps(_mm_load_ps(audioAccum + s    ), lof));
	//	_mm_store_ps(audioAccum + s + 4, _mm_add_ps(_mm_load_ps(audioAccum + s + 4), hif));

	//	s += 8;
	//}

	while (s < audioAccumSamples && s < srcLen) {
		audioAccum[s] += src[s] / (float)0x8000 * vLeft;
		s++;
		audioAccum[s] += src[s] / (float)0x8000 * vRight;
		s++;
	}
	
	return s;
}

static uint32_t addChannelSamplesF32(struct AudioChannel *chan, float masterVolume) {
	/* Much simpler version of the above */
	float *src = (int16_t *)chan->data + chan->sample;
	uint32_t srcLen = (chan->length - chan->sample) & ~1; /* Make sure it's an even number */
	uint32_t s = 0;

	float vLeft = masterVolume * chan->left;
	float vRight = masterVolume * chan->right;
	m128 volume = _mm_set_ps(vLeft, vRight, vLeft, vRight);

	for (int i = 0; i < audioAccumSamples / 4 && i < srcLen / 4; i++) {
		/* Multiply src sample by volume, then add to accumulator */
		m128 v = _mm_mul_ps(_mm_loadu_ps(src + s), volume);
		_mm_store_ps(audioAccum + s, _mm_add_ps(_mm_load_ps(audioAccum + s), v));

		s += 4;
	}

	while (s < audioAccumSamples && s < srcLen) {
		audioAccum[s] += src[s] * vLeft;
		s++;
		audioAccum[s] += src[s] * vRight;
		s++;
	}

	return s;
}

static void audioCallback(void *userData, Uint8 *finalStream, int streamLength) {
	memset(audioAccum, 0, audioSpec.samples * 8);
	float sfxV = sfxVol / 255.0f;

	for (int curChannel = 0; curChannel < N_AUDIO_CHANNELS; curChannel++) {
		struct AudioChannel *chan = &sfxChannels[curChannel];
		if (!chan->play)
			continue;

		uint32_t s = 0;
		if (chan->fmt == AUDIO_S16SYS) {
			s = addChannelSamplesS16(chan, sfxV);
		} else {
			assert(false);
		}

		chan->sample += s;
		if (chan->sample >= chan->length)
			chan->play = false;
	}
	
	m128 vMin = _mm_set1_ps(-1);
	m128 vMax = _mm_set1_ps(1);
	if (audioSpec.format == AUDIO_F32) {
		for (int i = 0; i < streamLength / sizeof(float); i += 4) {
			m128 v = _mm_load_ps(audioAccum + i);
			v = _mm_max_ps(v, vMin);
			v = _mm_min_ps(v, vMax);
			_mm_store_ps(((float *)finalStream) + i, v);
		}
	} else if (audioSpec.format == AUDIO_S16SYS) {
		/* convert to shorts */
		m128 mul = _mm_set1_ps(0x8000);
		for (int i = 0; i < streamLength / sizeof(float); i += 8) {
			m128 lo = _mm_mul_ps(_mm_load_ps(audioAccum + i), mul);
			m128 hi = _mm_mul_ps(_mm_load_ps(audioAccum + i + 4), mul);
			m128i v = _mm_packs_epi32(_mm_cvtps_epi32(lo), _mm_cvtps_epi32(hi));
			_mm_store_si128((m128i *)(((float *)finalStream) + i), v);
		}
	}
}


/* Music */

void audioStartMusic(const char *name) {
	
	loadUpdatePoll();
}

void audioStopMusic(void) {
	
}

void audioPauseMusic(bool paused) {
	
}

void audioSetMusicVolume(int volume) {
	musVol = volume;
}

int audioGetMusicVolume(void) {
	return musVol;
}


/* SFX */

void audioSetSFXVolume(int volume) {
	SDL_LockAudioDevice(audioDeviceID);
	sfxVol = volume;
	SDL_UnlockAudioDevice(audioDeviceID);
}

int audioGetSFXVolume(void) {
	return sfxVol;
}

void audioPlaySFX(int channel, int volume, int panning) {
	struct AudioChannel *chan = &sfxChannels[channel];
	if (!chan->data)
		return;

	SDL_LockAudioDevice(audioDeviceID);
	chan->left = chan->right = volume / 255.0f;
	if (panning < 0) {
		chan->right *= 1.0f - (panning / -127.0f);
	} else if (panning > 0) {
		chan->left *= 1.0f - (panning / 127.0f);
	}
	chan->sample = 0;
	chan->play = true;
	SDL_UnlockAudioDevice(audioDeviceID);
}

void audioStopSFX(int channel) {
}

void audioLoadSFX(int channel, const char *name) {
	struct Asset *a = assetOpen(name);
	if (!a) {
		logError("Could not find SFX: %s\n", name);
		return;
	}

	struct AudioChannel *chan = &sfxChannels[channel];
	struct SDL_AudioSpec spec;
	Uint8 *data;
	Uint32 len;
	if (SDL_LoadWAV_RW(a->rwOps, 0, &spec, &data, &len) == NULL) {
		logError("Failed to load SFX: %s, %s\n", name, SDL_GetError());
		chan->data = NULL;
		goto close_asset;
	}

	if (spec.format != AUDIO_S16SYS || spec.channels > 2 ||
		(spec.freq != FREQUENCY && spec.freq != FREQUENCY / 2)) {
		logError("Failed to load SFX: %s, invalid audio format (must be signed 16-bit, mono or stereo, 44.1 or 22.05kHz)\n", name);
		goto free_wav;
	}

	uint32_t frames = len / sizeof(int16_t) / spec.channels;
	chan->length = frames * 2;
	if (spec.freq == FREQUENCY / 2)
		chan->length *= 2;

	chan->data = globalAlloc(chan->length * sizeof(int16_t));
	int16_t *dst = (int16_t *)chan->data;

	for (uint32_t i = 0; i < frames; i++) {
		int32_t l, r;
		if (spec.channels == 2) {
			l = ((int16_t *)data)[i * 2];
			r = ((int16_t *)data)[i * 2 + 1];
		} else {
			l = r = ((int16_t *)data)[i];
		}
		*(dst++) = l;
		*(dst++) = r;

		if (spec.freq == FREQUENCY / 2) {
			if (false && i != frames - 1) {
				if (spec.channels == 2) {
					l = (l + ((int16_t *)data)[i * 2 + 2]) / 2;
					r = (r + ((int16_t *)data)[i * 2 + 3]) / 2;
				} else {
					l = r = (l + ((int16_t *)data)[i + 1]) / 2;
				}
			}
			*(dst++) = l;
			*(dst++) = r;
		}
	}
	
	chan->fmt = AUDIO_S16SYS;

	free_wav:
	SDL_FreeWAV(data);
	close_asset:
	assetClose(a);

	loadUpdatePoll();
}

void audioUnloadSFX(int channel) {
	struct AudioChannel *chan = &sfxChannels[channel];
	SDL_LockAudioDevice(audioDeviceID);
	chan->play = false;
	SDL_UnlockAudioDevice(audioDeviceID);

	globalDealloc(chan->data);
	chan->data = NULL;
}
void audioUnloadAllSFX(void) {
	for (int i = 0; i < N_AUDIO_CHANNELS; i++) {
		audioUnloadSFX(i);
	}
}

/* Init and Fini */

void audioInit(void) {
	sfxVol = engineSettings->sfxVol;
	musVol = engineSettings->bgmVol;

	SDL_AudioSpec desired = { 0 };
	desired.freq = FREQUENCY;
	desired.samples = 512;
	desired.channels = 2;
	desired.format = AUDIO_F32;
	desired.callback = audioCallback;
	audioDeviceID = SDL_OpenAudioDevice(NULL, 0, &desired, &audioSpec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
	if (audioDeviceID < 0) {
		fail("Failed to open audio device: %s\n", SDL_GetError());
	}

	audioAccumSamples = audioSpec.samples * 2; /* stereo */
	audioAccum = globalAlloc(audioAccumSamples * sizeof(float));

	/* Unpause */
	SDL_PauseAudioDevice(audioDeviceID, 0);
}

void audioFini(void) {
}
