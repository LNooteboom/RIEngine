#ifndef BASICS_H
#define BASICS_H

#include <ecs.h>
#include <math.h>
#include <ich.h>

#ifdef __cplusplus
extern "C" {
#endif


enum InterpModes {
	INTERP_LINEAR,

	INTERP_EASEIN1,
	INTERP_EASEIN2,
	INTERP_EASEIN3,

	INTERP_EASEOUT1,
	INTERP_EASEOUT2,
	INTERP_EASEOUT3,

	INTERP_SMOOTHSTEP,
	INTERP_EASEINOUT1,
	INTERP_EASEINOUT2,
	INTERP_EASEINOUT3,

	INTERP_EASEOUTIN1,
	INTERP_EASEOUTIN2,
	INTERP_EASEOUTIN3,

	INTERP_STEP,

	INTERP_EASEINBACK1,
	INTERP_EASEINBACK2,
	INTERP_EASEINBACK3,
	INTERP_EASEINBACK4,
	INTERP_EASEINBACK5,

	INTERP_EASEOUTBACK1,
	INTERP_EASEOUTBACK2,
	INTERP_EASEOUTBACK3,
	INTERP_EASEOUTBACK4,
	INTERP_EASEOUTBACK5,

	INTERP_BEZIER
};


/* Basic components */

/* This is so fucky i cant */
#define rotReal rw
#define rotImag rz

struct Transform {
	entity_t entity;
	float x, y, z;
	float rx, ry, rz, rw;
};

struct IchigoLocals {
	entity_t entity;
	int i[4];
	float f[8];
	const char *str[4];
};

struct Interp {
	enum InterpModes mode;
	float time;
	float endTime;
	float start[4];
	float end[4];
};

/* Math */

#define PI 3.14159265358979323846f
#define DEG2RAD(d) ((d)/180.0f * PI)

#define CMUL(ro, io, r1, i1, r2, i2) do { \
		float u = (r1) * (r2) - (i1) * (i2); \
		float a = (r1) * (i2) + (i1) * (r2); \
		(ro) = u; \
		(io) = a; \
	} while (0)

#define ANGLE2C(ro, io, ang) do { \
		(ro) = cosf(ang);\
		(io) = sinf(ang);\
	} while (0)

static inline void tfSetRotation(struct Transform *tf, float rotation) {
	tf->rotReal = cosf(rotation);
	tf->rotImag = sinf(rotation);
}

static inline float lerp(float v0, float v1, float t) {
	return (1 - t) * v0 + t * v1;
}

static inline void normalize(float *x, float *y, float inX, float inY) {
	/* Divide the vector by its magnitude */
	float mag = sqrtf(inX*inX + inY*inY);
	*x = inX / mag;
	*y = inY / mag;
}

uint32_t randomGetSeed(void);
void randomSetSeed(uint32_t seed);
int randomInt(int min, int max);
float randomFloat(float min, float max);

void tfSetRotation3D(struct Transform *tf, float rx, float ry, float rz);


extern struct IchigoLocals *ichigoLocalsCur;
void ichigoBindLocals(struct IchigoState *is);
void ichigoCopyLocals(struct IchigoLocals *dst, struct IchigoLocals *src);


float modeLerp(float v0, float v1, float t, enum InterpModes mode);
void i_modeLerp(struct IchigoVm *vm);

void interpStart(struct Interp *in, enum InterpModes mode, int n, float *start, float *goal, float time);

void interpUpdate(struct Interp *in, int n, float *val, float timeDelta);

void interpUpdateBezier(struct Interp *in, int n, float *val, float timeDelta, float *ex1, float *ex2);

float normalizeRot(float r);

int strCharUtf(const char *s, int l, int *pos);

#ifdef __cplusplus
} // extern "C"

constexpr ClWrapper<Transform, TRANSFORM> TRANSFORMS;
constexpr ClWrapper<IchigoLocals, ICHIGO_LOCALS> ICH_LOCALS;
#endif

#endif
