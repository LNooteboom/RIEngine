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

	SDL_AudioSpec spec;
	uint8_t *data; /* The audio samples */
};

static int sfxVol;
static int musVol;

static struct AudioChannel *sfxChannels[N_AUDIO_CHANNELS];

static struct Asset* musicAsset;
struct AudioChannel *musicChannel;

static SDL_AudioSpec audioSpec;
static float *audioAccum;
static uint32_t audioAccumSamples;

static uint32_t addChannelSamplesS16(struct AudioChannel *chan, float masterVolume) {
	/* Evil floating point hacking here, beware of dragons */
	int16_t *src = (int16_t *)chan->data + chan->sample;
	uint32_t srcLen = chan->length - chan->sample;
	uint32_t s = 0;

	float vLeft = masterVolume * chan->left;
	float vRight = masterVolume * chan->right;
	m128 volume = _mm_set_ps(vLeft, vRight, vLeft, vRight);

	m128i makeUnsigned = _mm_set1_epi16(0x8000);
	m128i magic = _mm_set1_epi16(0x4380); /* Exponent part of float */
	m128 magic2 = _mm_set1_ps(257);

	for (int i = 0; i < audioAccumSamples / 8 && i < srcLen / 8; i++) {
		m128i sdat = _mm_loadu_si128((m128i *)src[s]);

		sdat = _mm_xor_si128(sdat, makeUnsigned); /* Wrap to unsigned short */
		m128i lo = _mm_unpacklo_epi16(sdat, magic); /* unpack and turn it into float */
		m128i hi = _mm_unpackhi_epi16(sdat, magic);
		m128 lof = _mm_sub_ps(_mm_castsi128_ps(lo), magic2); /* subtract bias value */
		m128 hif = _mm_sub_ps(_mm_castsi128_ps(lo), magic2);

		/* Multiply the volume */
		lof = _mm_mul_ps(lof, volume);
		hif = _mm_mul_ps(hif, volume);

		/* Add to accumulator */
		_mm_store_ps(audioAccum + s    , _mm_add_ps(_mm_load_ps(audioAccum + s    ), lof));
		_mm_store_ps(audioAccum + s + 4, _mm_add_ps(_mm_load_ps(audioAccum + s + 4), hif));

		s += 8;
	}

	while (s < audioAccumSamples && s < srcLen) {
		audioAccum[s] += src[s] / (float)0x8000 * vLeft;
		s++;
		audioAccum[s] += src[s] / (float)0x8000 * vRight;
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
		if (chan->spec.format == AUDIO_S16SYS) {
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
		for (int i = 0; i < streamLength / sizeof(float); i += 8) {
			m128 lo = _mm_load_ps(audioAccum + i);
			m128 hi = _mm_load_ps(audioAccum + i + 4);
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
	sfxVol = volume;
}

int audioGetSFXVolume(void) {
	return sfxVol;
}

void audioPlaySFX(int channel, int volume, int panning) {
	struct AudioChannel *chan = &sfxChannels[channel];
	if (!chan->data)
		return;

	chan->left = chan->right = volume / 255.0f;
	if (panning < 0) {
		chan->right *= 1.0f - (panning / -127.0f);
	} else if (panning > 0) {
		chan->left *= 1.0f - (panning / 127.0f);
	}
	chan->sample = 0;
	chan->play = true;
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
	Uint8 *data;
	Uint32 len;
	if (SDL_LoadWAV_RW(a->rwOps, 0, &chan->spec, &chan->data, &chan->length) == NULL) {
		logError("Failed to load SFX: %s, %s\n", name, SDL_GetError());
		chan->data = NULL;
	}

	assetClose(a);

	loadUpdatePoll();
}

void audioUnloadSFX(int channel) {
	struct AudioChannel *chan = &sfxChannels[channel];
	SDL_FreeWAV(chan->data);
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
	if (SDL_OpenAudioDevice(NULL, 0, &desired, &audioSpec, SDL_AUDIO_ALLOW_FORMAT_CHANGE) < 0) {
		fail("Failed to open audio device: %s\n", SDL_GetError());
	}

	audioAccumSamples = audioSpec.samples * 2; /* stereo */
	audioAccum = globalAlloc(audioAccumSamples * sizeof(float));
}

void audioFini(void) {
}
