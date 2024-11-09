#include <basics.h>
#include <stdlib.h>
#include "system_init.h"
#include <ich.h>
#include <string.h>
#include <events.h>

static uint32_t randState;

uint32_t randomGetSeed(void) {
	return randState;
}
void randomSetSeed(uint32_t seed) {
	randState = seed;
}
static uint32_t randNext(void) {
	uint32_t x = randState;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	randState = x;
	return randState;
}

int randomInt(int min, int max) {
	if (min > max) {
		int min2 = min;
		min = max;
		max = min2;
	}
	uint32_t range = max - min;
	if (!range)
		return min;
	int32_t v = randNext() % range;
	return v + min;
}

float randomFloat(float min, float max) {
	if (min > max) {
		float min2 = min;
		min = max;
		max = min2;
	}
	float range = max - min;
	float v = (float)randNext() / (UINT32_MAX / range);
	return v + min;
}

static void newTransform(void *arg, void *component, int type) {
	(void) arg;
	struct Transform *tf = component;
	if (type == NOTIFY_CREATE) {
		tf->rotReal = 1.0f;
		tf->rw = 1.0f;
	}
}

static void ichigoNotifier(void *arg, void *component, int type) {
	(void)arg;
	struct IchigoVm *vm = component;
	if (type == NOTIFY_CREATE) {
		ichigoVmNew(NULL, vm, vm->en);
	} else if (type == NOTIFY_DELETE) {
		ichigoVmDelete(vm);
	} else if (type == NOTIFY_PURGE) {
		for (vm = clBegin(DRAW_VM_VM); vm; vm = clNext(DRAW_VM_VM, vm)) {
			ichigoVmDelete(vm);
		}
	}
}

struct IchigoLocals *ichigoLocalsCur;
#define ICH_LCL_GET(T, NAME, DST) static T i_getVar##NAME(struct IchigoVm *vm) {struct IchigoLocals *l = ichigoLocalsCur? ichigoLocalsCur : getComponent(ICHIGO_LOCALS, vm->en); return l->DST;}
#define ICH_LCL_SET(T, NAME, DST) static void i_setVar##NAME(struct IchigoVm *vm, T val) {struct IchigoLocals *l = ichigoLocalsCur? ichigoLocalsCur : getComponent(ICHIGO_LOCALS, vm->en); l->DST = val;}
#define ICH_LCL_GETSET(T, NAME, DST) ICH_LCL_GET(T, NAME, DST) ICH_LCL_SET(T, NAME, DST)
#define ICH_LCL_STR_GET(IDX) static const char *i_getVarSTR##IDX(struct IchigoVm *vm, uint16_t *sLen) {struct IchigoLocals *l = ichigoLocalsCur? ichigoLocalsCur : getComponent(ICHIGO_LOCALS, vm->en); *sLen = (uint16_t)strlen(l->str[IDX]); return l->str[IDX];}
#define ICH_LCL_STR_SET(IDX) static void i_setVarSTR##IDX(struct IchigoVm *vm, const char *s, uint16_t sLen) {(void) sLen; struct IchigoLocals *l = ichigoLocalsCur? ichigoLocalsCur : getComponent(ICHIGO_LOCALS, vm->en); l->str[IDX] = s;}
#define ICH_LCL_STR_GETSET(IDX) ICH_LCL_STR_GET(IDX) ICH_LCL_STR_SET(IDX)
ICH_LCL_GETSET(int, I0, i[0])
ICH_LCL_GETSET(int, I1, i[1])
ICH_LCL_GETSET(int, I2, i[2])
ICH_LCL_GETSET(int, I3, i[3])
ICH_LCL_GETSET(float, F0, f[0])
ICH_LCL_GETSET(float, F1, f[1])
ICH_LCL_GETSET(float, F2, f[2])
ICH_LCL_GETSET(float, F3, f[3])
ICH_LCL_GETSET(float, F4, f[4])
ICH_LCL_GETSET(float, F5, f[5])
ICH_LCL_GETSET(float, F6, f[6])
ICH_LCL_GETSET(float, F7, f[7])
ICH_LCL_STR_GETSET(0)
ICH_LCL_STR_GETSET(1)
ICH_LCL_STR_GETSET(2)
ICH_LCL_STR_GETSET(3)

static int i_getVarRAND(struct IchigoVm *vm) {
	(void)vm;
	return randomInt(0, 0x7FFFFFFF);
}
static float i_getVarRANDF(struct IchigoVm *vm) {
	(void)vm;
	return randomFloat(0, 1);
}
static float i_getVarRANDF2(struct IchigoVm *vm) {
	(void)vm;
	return randomFloat(-1, 1);
}
static float i_getVarRANDRAD(struct IchigoVm *vm) {
	(void)vm;
	return randomFloat(-PI, PI);
}
static float i_getVarPOS_X(struct IchigoVm *vm) {
	struct Transform *tf = getComponentOpt(TRANSFORM, vm->en);
	return tf ? tf->x : 0;
}
static void i_setVarPOS_X(struct IchigoVm *vm, float val) {
	struct Transform *tf = getComponentOpt(TRANSFORM, vm->en);
	if (tf)
		tf->x = val;
}
static float i_getVarPOS_Y(struct IchigoVm *vm) {
	struct Transform *tf = getComponentOpt(TRANSFORM, vm->en);
	return tf? tf->y : 0;
}
static void i_setVarPOS_Y(struct IchigoVm *vm, float val) {
	struct Transform *tf = getComponentOpt(TRANSFORM, vm->en);
	if (tf)
		tf->y = val;
}
static float i_getVarPOS_Z(struct IchigoVm *vm) {
	struct Transform *tf = getComponentOpt(TRANSFORM, vm->en);
	return tf ? tf->z : 0;
}
static void i_setVarPOS_Z(struct IchigoVm *vm, float val) {
	struct Transform *tf = getComponentOpt(TRANSFORM, vm->en);
	if (tf)
		tf->z = val;
}

static inline void ichVar(struct IchigoState *is, int idx, enum IchigoRegType type, void *get, void *set) {
	is->vars[idx].regType = type;
	is->vars[idx].get.i = get;
	is->vars[idx].set.i = set;
}
void ichigoBindLocals(struct IchigoState *is) {
	ichVar(is, 0, REG_INT, i_getVarI0, i_setVarI0);
	ichVar(is, 1, REG_INT, i_getVarI1, i_setVarI1);
	ichVar(is, 2, REG_INT, i_getVarI2, i_setVarI2);
	ichVar(is, 3, REG_INT, i_getVarI3, i_setVarI3);
	ichVar(is, 4, REG_FLOAT, i_getVarF0, i_setVarF0);
	ichVar(is, 5, REG_FLOAT, i_getVarF1, i_setVarF1);
	ichVar(is, 6, REG_FLOAT, i_getVarF2, i_setVarF2);
	ichVar(is, 7, REG_FLOAT, i_getVarF3, i_setVarF3);
	ichVar(is, 8, REG_FLOAT, i_getVarF4, i_setVarF4);
	ichVar(is, 9, REG_FLOAT, i_getVarF5, i_setVarF5);
	ichVar(is, 10, REG_FLOAT, i_getVarF6, i_setVarF6);
	ichVar(is, 11, REG_FLOAT, i_getVarF7, i_setVarF7);
	ichVar(is, 12, REG_STRING, i_getVarSTR0, i_setVarSTR0);
	ichVar(is, 13, REG_STRING, i_getVarSTR1, i_setVarSTR1);
	ichVar(is, 14, REG_STRING, i_getVarSTR2, i_setVarSTR2);
	ichVar(is, 15, REG_STRING, i_getVarSTR3, i_setVarSTR3);
	ichVar(is, 16, REG_INT, i_getVarRAND, NULL);
	ichVar(is, 17, REG_FLOAT, i_getVarRANDF, NULL);
	ichVar(is, 18, REG_FLOAT, i_getVarRANDF2, NULL);
	ichVar(is, 19, REG_FLOAT, i_getVarRANDRAD, NULL);
	ichVar(is, 20, REG_FLOAT, i_getVarPOS_X, i_setVarPOS_X);
	ichVar(is, 21, REG_FLOAT, i_getVarPOS_Y, i_setVarPOS_Y);
	ichVar(is, 22, REG_FLOAT, i_getVarPOS_Z, i_setVarPOS_Z);
}

void ichigoCopyLocals(struct IchigoLocals *dst, struct IchigoLocals *src) {
	for (int i = 0; i < 8; i++) {
		if (i < 4) {
			dst->i[i] = src->i[i];
			dst->str[i] = src->str[i];
		}
		dst->f[i] = src->f[i];
	}
}

static void updateVms(void *arg) {
	(void)arg;
	for (struct IchigoVm *vm = clBegin(ICHIGO_VM); vm; vm = clNext(ICHIGO_VM, vm)) {
		ichigoLocalsCur = getComponentOpt(ICHIGO_LOCALS, vm->en);
		ichigoVmUpdate(vm, gameSpeed);
	}
	ichigoLocalsCur = NULL;
}

#define OVERSHOOT(x, a) (((x - a) * (x - a) - a * a) * (1.0f / ((1.0f - a) * (1.0f - a) - a * a)))
float modeLerp(float v0, float v1, float t, enum InterpModes mode) {
	float it = 1.0f - t;
	float t2 = t * 2;
	float fact = 0;
	float a;
	switch (mode) {
	default:
	case INTERP_LINEAR:
		fact = t;
		break;
	case INTERP_EASEIN1:
		fact = t * t;
		break;
	case INTERP_EASEIN2:
		fact = t * t * t;
		break;
	case INTERP_EASEIN3:
		a = t * t;
		fact = a * a;
		break;
	case INTERP_EASEOUT1:
		fact = 1.0f - it * it;
		break;
	case INTERP_EASEOUT2:
		fact = 1.0f - it * it * it;
		break;
	case INTERP_EASEOUT3:
		a = it * it;
		fact = 1.0f - a * a;
		break;
	case INTERP_SMOOTHSTEP:
		a = t * t;
		fact = 3.0f * a - 2.0f * a * t;
		break;
	case INTERP_EASEINOUT1:
		if (t < 0.5f) {
			fact = 0.5f * t2 * t2;
		} else {
			float itt = 2.0f - t2;
			fact = 0.5f * (1 + (1.0f - itt * itt));
		}
		//fact = t < 0.5f ? t * t : 1.0f - it * it;
		break;
	case INTERP_EASEINOUT2:
		if (t < 0.5f) {
			fact = 0.5f * t2 * t2 * t2;
		} else {
			float itt = 2.0f - t2;
			fact = 0.5f * (1 + (1.0f - itt * itt * itt));
		}
		break;
	case INTERP_EASEINOUT3:
		if (t < 0.5f) {
			fact = 0.5f * (t2 * t2) * (t2 * t2);
		} else {
			float itt = 2.0f - t2;
			fact = 0.5f * (1 + (1.0f - (itt * itt) * (itt * itt)));
		}
		break;
	case INTERP_EASEOUTIN1:
		if (t < 0.5f) {
			float itt = 1.0f - t2;
			fact = 0.5f * (1.0f - itt * itt);
		} else {
			float tt = t2 - 1;
			fact = 0.5f * (1 + tt * tt);
		}
		//fact = t > 0.5f ? t * t : 1.0f - it * it;
		break;
	case INTERP_EASEOUTIN2:
		if (t < 0.5f) {
			float itt = 1.0f - t2;
			fact = 0.5f * (1.0f - itt * itt * itt);
		} else {
			float tt = t2 - 1;
			fact = 0.5f * (1 + tt * tt * tt);
		}
		break;
	case INTERP_EASEOUTIN3:
		if (t < 0.5f) {
			float itt = 1.0f - t2;
			fact = 0.5f * (1.0f - (itt * itt) * (itt * itt));
		} else {
			float tt = t2 - 1;
			fact = 0.5f * (1 + (tt * tt) * (tt * tt));
		}
		break;
	case INTERP_STEP:
		fact = t < 0.5f ? 0 : 1;
		break;
	case INTERP_EASEINBACK1:
		fact = OVERSHOOT(t, 0.25f);
		break;
	case INTERP_EASEINBACK2:
		fact = OVERSHOOT(t, 0.30f);
		break;
	case INTERP_EASEINBACK3:
		fact = OVERSHOOT(t, 0.35f);
		break;
	case INTERP_EASEINBACK4:
		fact = OVERSHOOT(t, 0.38f);
		break;
	case INTERP_EASEINBACK5:
		fact = OVERSHOOT(t, 0.40f);
		break;
	case INTERP_EASEOUTBACK1:
		fact = OVERSHOOT(t, 0.75f);
		break;
	case INTERP_EASEOUTBACK2:
		fact = OVERSHOOT(t, 0.70f);
		break;
	case INTERP_EASEOUTBACK3:
		fact = OVERSHOOT(t, 0.65f);
		break;
	case INTERP_EASEOUTBACK4:
		fact = OVERSHOOT(t, 0.62f);
		break;
	case INTERP_EASEOUTBACK5:
		fact = OVERSHOOT(t, 0.60f);
		break;
	}
	return lerp(v0, v1, fact);
}
void i_modeLerp(struct IchigoVm *vm) {
	float t = ichigoGetFloat(vm, 3);
	t = fmaxf(0, fminf(1, t));
	float f = modeLerp(ichigoGetFloat(vm, 1), ichigoGetFloat(vm, 2), t, ichigoGetInt(vm, 4));
	ichigoSetFloat(vm, 0, f);
}

void interpStart(struct Interp *in, enum InterpModes mode, int n, float *start, float *goal, float time) {
	in->mode = mode;
	in->time = 0;
	in->endTime = time;
	for (int i = 0; i < 4; i++) {
		in->start[i] = i < n ? start[i] : 0;
		in->end[i] = i < n ? goal[i] : 0;
	}
}

void interpUpdate(struct Interp *in, int n, float *val, float timeDelta) {
	in->time += timeDelta;
	float f = fminf(in->endTime > 0.0001f ? in->time / in->endTime : 1, 1);
	for (int i = 0; i < n; i++) {
		val[i] = modeLerp(in->start[i], in->end[i], f, in->mode);
	}
}

void interpUpdateBezier(struct Interp *in, int n, float *val, float timeDelta, float *ex1, float *ex2) {
	if (in->mode != INTERP_BEZIER) {
		interpUpdate(in, n, val, timeDelta);
		return;
	}
	in->time += timeDelta;
	float t = fminf(in->endTime > 0.0001f ? in->time / in->endTime : 1, 1);
	for (int i = 0; i < n; i++) {
		float it = 1.0f - t;
		float it2 = it * it;
		float t2 = t * t;

		float p0 = in->start[i];
		float p1 = ex1[i];
		float p2 = ex2[i];
		float p3 = in->end[i];

		val[i] = p1*t*it2 - p2*t2*it + p3*t2*(3 - 2*t) + p0*it2*(2*t + 1);
	}
}

float normalizeRot(float r) {
	return fmodf(r, 2 * PI);
}

int strCharUtf(const char *s, int l, int *pos) {
	if (*pos < 0 || *pos >= l) {
		return 0;
	}
	int ret = 0;
	unsigned char c = s[*pos];
	*pos += 1;
	int extraLen = 0;
	if (!(c & 0x80)) {
		ret = c;
	} else if ((c & 0xE0) == 0xC0) {
		extraLen = 1;
		ret = c & 0x1F;
	} else if ((c & 0xF0) == 0xE0) {
		extraLen = 2;
		ret = c & 0x0F;
	} else if ((c & 0xF8) == 0xF0) {
		extraLen = 3;
		ret = c & 0x07;
	} else {
		/* Error */
		ret = 0xFFFD;
	}
	for (int i = 0; i < extraLen; i++) {
		c = s[*pos];
		*pos += 1;
		ret <<= 6;
		ret |= c & 0x3F;
	}
	return ret;
}


void basicsInit(void) {
	componentListInit(ICHIGO_VM, struct IchigoVm);
	setNotifier(ICHIGO_VM, ichigoNotifier, NULL);

	componentListInit(ICHIGO_LOCALS, struct IchigoLocals);

	componentListInit(TRANSFORM, struct Transform);
	setNotifier(TRANSFORM, newTransform, NULL);

	addUpdate(UPDATE_NORM, updateVms, NULL);

	randomSetSeed(rand() | 1);
	randNext();
	randNext();
	randNext();
	randNext();
}

void basicsFini(void) {
	removeUpdate(UPDATE_NORM, updateVms);
	componentListFini(TRANSFORM);
	componentListFini(ICHIGO_LOCALS);
	componentListFini(ICHIGO_VM);
}
