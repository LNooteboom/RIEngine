#ifndef VEC_H
#define VEC_H

#include "basics.h"
#include <math.h>

#define USE_SSE
#ifdef USE_SSE
#include <immintrin.h>
#define splat_x(r) _mm_shuffle_ps(r, r, 0)
#define splat_y(r) _mm_shuffle_ps(r, r, 0x55)
#define splat_z(r) _mm_shuffle_ps(r, r, 0xAA)
#define splat_w(r) _mm_shuffle_ps(r, r, 0xFF)
#endif

#ifdef _MSC_VER
#define ALIGN_MAT __declspec(align(16))
#else
#define ALIGN_MAT __attribute((aligned(16)))
#endif

#ifdef __cplusplus
#define ALIGN_MAT_DECL alignas(16)
#else
#define ALIGN_MAT_DECL
#endif

struct ALIGN_MAT_DECL Vec {
	float x, y, z, w;

#ifdef __cplusplus
	Vec();
	Vec(float v);
	Vec(float xv, float yv, float zv);
	Vec(float xv, float yv, float zv, float wv);
	Vec(Vec v, float wv);
	Vec(float *v);
	Vec normalized() const;
	Vec operator-() const;
	Vec operator+(const Vec &v2) const;
	Vec operator-(const Vec &v2) const;
	Vec operator*(float s) const;
	Vec operator*(const Vec &v2) const;
	static float dot(const Vec &v1, const Vec &v2);
	static Vec cross(const Vec &v1, const Vec &v2);
	static Vec eulerAngles(float rx, float ry, float rz);
	static Vec zAxis(float rz);
	static Vec lerp(const Vec &v1, const Vec &v2, float t);
	static Vec nlerp(const Vec &v1, const Vec &v2, float t);
	static Vec fromTfRot(const Transform &tf);
	static void setTfRot(Transform &tf, Vec v);
	static Vec fromTfPos(const Transform &tf);
#endif
};

struct ALIGN_MAT_DECL Mat {
	float m[16];

#ifdef __cplusplus
	Mat();
	Mat(float ident);
	Mat(float *v);
	Mat operator*(const Mat &m2) const;
	Vec operator*(const Vec &v) const;
	Mat transposed() const;
	void translate(const Vec &v);
	void scale(const Vec &v);
	void rotate(const Vec &v);
	void inverse3();
	void transposeSave3(float *dat);
	static Mat lookat(const Vec &eye, const Vec &center, const Vec &up);
	static Mat look(const Vec &eye, const Vec &dir, const Vec &up);
	static Mat fromRotation(const Vec &r);
	static Mat fromTranslation(const Vec &v);
	static Mat fromScale(const Vec &v);
	static Mat perspective(float fovy, float aspect, float fnear, float ffar);
#endif
};

typedef ALIGN_MAT struct Vec Vec;
typedef ALIGN_MAT struct Mat Mat;

// legacy
typedef ALIGN_MAT struct Vec Vec3;
typedef ALIGN_MAT struct Vec Vec4;

#ifdef USE_SSE
static inline Vec *vecZero(Vec *out) {
	_mm_store_ps(&out->x, _mm_set_ps1(0));
}
static inline Vec *vecCopy(Vec *out, const Vec *v) {
	_mm_store_ps(&out->x, _mm_load_ps(&v->x));
}

static inline Vec *vecAdd(Vec *out, const Vec *a, const Vec *b) {
	__m128 v = _mm_add_ps(_mm_load_ps(&a->x), _mm_load_ps(&b->x));
	_mm_store_ps(&out->x, v);
	return out;
}
static inline Vec *vecSub(Vec *out, const Vec *a, const Vec *b) {
	__m128 v = _mm_sub_ps(_mm_load_ps(&a->x), _mm_load_ps(&b->x));
	_mm_store_ps(&out->x, v);
	return out;
}
static inline Vec *vecMul(Vec *out, const Vec *a, const Vec *b) {
	__m128 v = _mm_mul_ps(_mm_load_ps(&a->x), _mm_load_ps(&b->x));
	_mm_store_ps(&out->x, v);
	return out;
}
static inline Vec *vecDiv(Vec *out, const Vec *a, const Vec *b) {
	__m128 v = _mm_div_ps(_mm_load_ps(&a->x), _mm_load_ps(&b->x));
	_mm_store_ps(&out->x, v);
	return out;
}

static inline Vec *vecAddS(Vec *out, const Vec *a, float b) {
	__m128 v = _mm_add_ps(_mm_load_ps(&a->x), _mm_set_ps1(b));
	_mm_store_ps(&out->x, v);
	return out;
}
static inline Vec *vecMulS(Vec *out, const Vec *a, float b) {
	__m128 v = _mm_mul_ps(_mm_load_ps(&a->x), _mm_set_ps1(b));
	_mm_store_ps(&out->x, v);
	return out;
}
static inline Vec *vecDivS(Vec *out, const Vec *a, float b) {
	__m128 v = _mm_div_ps(_mm_load_ps(&a->x), _mm_set_ps1(b));
	_mm_store_ps(&out->x, v);
	return out;
}

static inline Vec *vecFromTfPos(Vec *out, const struct Transform *tf) {
	_mm_store_ps(&out->x, _mm_loadu_ps(&tf->x));
	return out;
}
static inline Vec *vecFromTfRot(Vec *out, const struct Transform *tf) {
	_mm_store_ps(&out->x, _mm_loadu_ps(&tf->rx));
	return out;
}
static inline void vecToTfRot(struct Transform *tf, const Vec *v) {
	_mm_storeu_ps(&tf->rx, _mm_load_ps(&v->x));
}

static inline Vec *vecLerp(Vec *out, const Vec *a, const Vec *b, float t) {
	__m128 t2 = splat_x(_mm_set_ss(t));
	__m128 t3 = _mm_sub_ps(_mm_set_ps1(1.0f), t2);

	__m128 a2 = _mm_mul_ps(t3, _mm_load_ps(&a->x));
	__m128 b2 = _mm_mul_ps(t2, _mm_load_ps(&b->x));
	_mm_store_ps(&out->x, _mm_add_ps(a2, b2));
	return out;
}


static inline Mat *matCopy(Mat *out, const Mat *a) {
	_mm_store_ps(&out->m[0], _mm_load_ps(&a->m[0]));
	_mm_store_ps(&out->m[4], _mm_load_ps(&a->m[4]));
	_mm_store_ps(&out->m[8], _mm_load_ps(&a->m[8]));
	_mm_store_ps(&out->m[12], _mm_load_ps(&a->m[12]));
	return out;
}

static inline Mat *matMul(Mat *out, const Mat *a, const Mat *b) {
	__m128 l, r0, r1, r2, r3, v0, v1, v2, v3;
	r0 = _mm_load_ps(&b->m[0]);
	r1 = _mm_load_ps(&b->m[4]);
	r2 = _mm_load_ps(&b->m[8]);
	r3 = _mm_load_ps(&b->m[12]);

	l = _mm_load_ps(&a->m[0]);
	v0 = _mm_mul_ps(l, splat_x(r0));
	v1 = _mm_mul_ps(l, splat_x(r1));
	v2 = _mm_mul_ps(l, splat_x(r2));
	v3 = _mm_mul_ps(l, splat_x(r3));

	l = _mm_load_ps(&a->m[4]);
	v0 = _mm_add_ps(v0, _mm_mul_ps(l, splat_y(r0)));
	v1 = _mm_add_ps(v1, _mm_mul_ps(l, splat_y(r1)));
	v2 = _mm_add_ps(v2, _mm_mul_ps(l, splat_y(r2)));
	v3 = _mm_add_ps(v3, _mm_mul_ps(l, splat_y(r3)));

	l = _mm_load_ps(&a->m[8]);
	v0 = _mm_add_ps(v0, _mm_mul_ps(l, splat_z(r0)));
	v1 = _mm_add_ps(v1, _mm_mul_ps(l, splat_z(r1)));
	v2 = _mm_add_ps(v2, _mm_mul_ps(l, splat_z(r2)));
	v3 = _mm_add_ps(v3, _mm_mul_ps(l, splat_z(r3)));

	l = _mm_load_ps(&a->m[12]);
	v0 = _mm_add_ps(v0, _mm_mul_ps(l, splat_w(r0)));
	v1 = _mm_add_ps(v1, _mm_mul_ps(l, splat_w(r1)));
	v2 = _mm_add_ps(v2, _mm_mul_ps(l, splat_w(r2)));
	v3 = _mm_add_ps(v3, _mm_mul_ps(l, splat_w(r3)));

	_mm_store_ps(&out->m[0], v0);
	_mm_store_ps(&out->m[4], v1);
	_mm_store_ps(&out->m[8], v2);
	_mm_store_ps(&out->m[12], v3);
	return out;
}

static inline Vec *matMulV4(Vec *out, const Mat *a, const Vec *b) {
	__m128 r = _mm_loadu_ps(&b->x);
	__m128 l0 = _mm_load_ps(&a->m[0]);
	__m128 l1 = _mm_load_ps(&a->m[4]);
	__m128 l2 = _mm_load_ps(&a->m[8]);
	__m128 l3 = _mm_load_ps(&a->m[12]);
	__m128 v0 = _mm_mul_ps(l0, splat_x(r));
	v0 = _mm_add_ps(v0, _mm_mul_ps(l1, splat_y(r)));
	v0 = _mm_add_ps(v0, _mm_mul_ps(l2, splat_z(r)));
	v0 = _mm_add_ps(v0, _mm_mul_ps(l3, splat_w(r)));
	_mm_storeu_ps(&out->x, v0);
	return out;
}
static inline Vec *matMulV3(Vec *out, const Mat *a, Vec *b) {
	b->w = 1.0f;
	return matMulV4(out, a, b);
}

#else
#error "I need SSE!"
#endif

static inline Vec *vecSet4(Vec *out, float x, float y, float z, float w) {
	out->x = x;
	out->y = y;
	out->z = z;
	out->w = w;
	return out;
}
static inline Vec *vecSet3(Vec *out, float x, float y, float z) {
	return vecSet4(out, x, y, z, 1.0f);
}
static inline Vec *vecSet1(Vec *out, float v) {
	return vecSet4(out, v, v, v, v);
}
static inline void vecToTfPos(struct Transform *tf, const Vec *v) {
	tf->x = v->x;
	tf->y = v->y;
	tf->z = v->z;
}

static inline float vecDot3(const Vec *a, const Vec *b) {
	return a->x * b->x + a->y * b->y + a->z * b->z;
}
static inline float vecDot4(const Vec *a, const Vec *b) {
	return a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w;
}
static inline Vec *vecCross3(Vec *out, const Vec *a, const Vec *b) {
	out->x = a->y * b->z - a->z * b->y;
	out->y = a->z * b->x - a->x * b->z;
	out->z = a->x * b->y - a->y * b->x;
	return out;
}

static inline float vecLength3(const Vec *v) {
	return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}
static inline Vec *vecNormalize3(Vec *out, const Vec *v) {
	return vecMulS(out, v, 1.0f / vecLength3(v));
}
static inline float vecLength4(const Vec *v) {
	return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z + v->w * v->w);
}
static inline Vec *vecNormalize4(Vec *out, const Vec *v) {
	return vecMulS(out, v, 1.0f / vecLength4(v));
}

static inline Vec *vecRound3(Vec *out, const Vec *v) {
	out->x = roundf(v->x);
	out->y = roundf(v->y);
	out->z = roundf(v->z);
	return out;
}


/* Quaternions */

static inline Vec *quatNlerp(Vec *out, const Vec *a, const Vec *b, float t) {
	return vecNormalize4(out, vecLerp(out, a, b, t));
}

static inline Vec *quatEulerAngles(Vec *out, const Vec *ang) {
	float cx = cosf(0.5f * ang->x), sx = sinf(0.5f * ang->x);
	float cy = cosf(0.5f * ang->y), sy = sinf(0.5f * ang->y);
	float cz = cosf(0.5f * ang->z), sz = sinf(0.5f * ang->z);
	out->x = cz * sx * cy - sz * cx * sy;
	out->y = cz * cx * sy + sz * sx * cy;
	out->z = sz * cx * cy - cz * sx * sy;
	out->w = cz * cx * cy + sz * sx * sy;
	return out;
}

static inline Vec *quatSlerp(Vec *out, const Vec *a, const Vec *b, float t) {
	float cosTheta = vecDot4(a, b);
	if (cosTheta > 0.99f)
		return quatNlerp(out, a, b, t); // Rotation is very close

	float theta = acosf(cosTheta);
	Vec v2;
	vecMulS(out, a, sinf((1.0f - t) * theta));
	vecMulS(&v2, b, sinf(t * theta));
	vecAdd(out, out, &v2);
	vecDivS(out, out, sinf(theta));
	return out;
}

static inline Vec *quatAngleAxis(Vec *out, const Vec *axis, float angle) {
	float ang2 = angle * 0.5f;
	float s = sinf(ang2), c = cosf(ang2);
	out->x = axis->x * s;
	out->y = axis->y * s;
	out->z = axis->z * s;
	out->w = c;
	return out;
}

/* Matrix */

static inline Mat *matLoad(Mat *out, float *data) {
	for (int i = 0; i < 16; i++) {
		out->m[i] = data[i];
	}
}

static inline Mat *matIdent(Mat *out, float ident) {
	out->m[0] = ident; out->m[1] = 0; out->m[2] = 0; out->m[3] = 0;
	out->m[4] = 0; out->m[5] = ident; out->m[6] = 0; out->m[7] = 0;
	out->m[8] = 0; out->m[9] = 0; out->m[10] = ident; out->m[11] = 0;
	out->m[12] = 0; out->m[13] = 0; out->m[14] = 0; out->m[15] = ident;
	return out;
}

static inline Mat *matFromTranslation(Mat *out, const Vec *a) {
	matIdent(out, 1.0f);
	out->m[12] = a->x;
	out->m[13] = a->y;
	out->m[14] = a->z;
	return out;
}
static inline Mat *matFromScale(Mat *out, const Vec *a) {
	matIdent(out, 1.0f);
	out->m[0] = a->x;
	out->m[5] = a->y;
	out->m[10] = a->z;
	return out;
}
static inline Mat *matFromRotation(Mat *out, const Vec *a) {
	float x = a->x;
	float y = a->y;
	float z = a->z;
	float w = a->w;

	float tx = x + x; // Note: Using x + x instead of 2.0f * x to force this function to return the same value as the SSE4.1 version across platforms.
	float ty = y + y;
	float tz = z + z;

	float xx = tx * x;
	float yy = ty * y;
	float zz = tz * z;
	float xy = tx * y;
	float xz = tx * z;
	float xw = tx * w;
	float yz = ty * z;
	float yw = ty * w;
	float zw = tz * w;

	out->m[0] = (1.0f - yy) - zz;
	out->m[1] = xy + zw;
	out->m[2] = xz - yw;
	out->m[3] = 0;
	out->m[4] = xy - zw;
	out->m[5] = (1.0f - zz) - xx;
	out->m[6] = yz + xw;
	out->m[7] = 0;
	out->m[8] = xz + yw;
	out->m[9] = yz - xw;
	out->m[10] = (1.0f - xx) - yy;
	out->m[11] = 0;
	out->m[12] = 0;
	out->m[13] = 0;
	out->m[14] = 0;
	out->m[15] = 1.0f;
	return out;
}
static inline Mat *matTranslate(Mat *out, const Mat *a, const Vec *b) {
	out->m[12] += a->m[0] * b->x + a->m[4] * b->y + a->m[8] * b->z;
	out->m[13] += a->m[1] * b->x + a->m[5] * b->y + a->m[9] * b->z;
	out->m[14] += a->m[2] * b->x + a->m[6] * b->y + a->m[10] * b->z;
	return out;
}
static inline Mat *matScale(Mat *out, const Mat *a, const Vec *b) {
	for (int i = 0; i < 3; i++) {
		out->m[0 + i] = a->m[0 + i] * b->x;
		out->m[4 + i] = a->m[4 + i] * b->y;
		out->m[8 + i] = a->m[8 + i] * b->z;
	}
	return out;
}
static inline Mat *matRotate(Mat *out, const Mat *a, const Vec *b) {
	Mat rot;
	matFromRotation(&rot, b);
	return matMul(out, a, &rot);
}

static inline Mat *matInverse3(Mat *out, const Mat *m) {
	float det;
	float a = m->m[0], b = m->m[1], c = m->m[2],
		  d = m->m[4], e = m->m[5], f = m->m[6],
		  g = m->m[8], h = m->m[9], i = m->m[10];

	out->m[0] = e * i - f * h;
	out->m[1] = -(b * i - h * c);
	out->m[2] = b * f - e * c;
	out->m[3] = 0;
	out->m[4] = -(d * i - g * f);
	out->m[5] = a * i - c * g;
	out->m[6] = -(a * f - d * c);
	out->m[7] = 0;
	out->m[8] = d * h - g * e;
	out->m[9] = -(a * h - g * b);
	out->m[10] = a * e - b * d;
	out->m[11] = 0;
	out->m[12] = 0;
	out->m[13] = 0;
	out->m[14] = 0;
	out->m[15] = 0;

	det = 1.0f / (a * out->m[0] + b * out->m[4] + c * out->m[8]);
	for (int i = 0; i < 16; i++) {
		out->m[i] *= det;
	}
	return out;
}

static inline Mat *matTranspose3(Mat *out, const Mat *m) {
	out->m[0] = m->m[0];
	out->m[1] = m->m[4];
	out->m[2] = m->m[8];
	out->m[3] = 0;
	out->m[4] = m->m[1];
	out->m[5] = m->m[5];
	out->m[6] = m->m[9];
	out->m[7] = 0;
	out->m[8] = m->m[2];
	out->m[9] = m->m[6];
	out->m[10] = m->m[10];
	out->m[11] = 0;
	out->m[12] = 0;
	out->m[13] = 0;
	out->m[14] = 0;
	out->m[15] = 1.0f;
	return out;
}

static inline Mat *matLookAt(Mat *out, const Vec *eye, const Vec *center, const Vec *up) {
	Vec f, s, u;
	vecNormalize3(&f, vecSub(&f, center, eye));
	vecNormalize3(&s, vecCross3(&s, &f, up));
	vecCross3(&u, &s, &f);

	out->m[0] = s.x;
	out->m[1] = u.x;
	out->m[2] = -f.x;
	out->m[3] = 0;
	out->m[4] = s.y;
	out->m[5] = u.y;
	out->m[6] = -f.y;
	out->m[7] = 0;
	out->m[8] = s.z;
	out->m[9] = u.z;
	out->m[10] = -f.z;
	out->m[11] = 0;
	out->m[12] = -vecDot3(&s, eye);
	out->m[13] = -vecDot3(&u, eye);
	out->m[14] = vecDot3(&f, eye);
	out->m[15] = 1.0f;
	return out;
}
static inline Mat *matLook(Mat *out, const Vec *eye, const Vec *dir, const Vec *up) {
	Vec center;
	vecAdd(&center, eye, dir);
	return matLookAt(out, eye, &center, up);
}

static inline Mat *matPerspective(Mat *out, float fovy, float aspect, float fnear, float ffar) {
	float f = 1.0f / tanf(fovy * 0.5f);
	float fn = 1.0f / (fnear - ffar);
	matIdent(out, 0.0f);
	out->m[0] = f / aspect;
	out->m[5] = f;
	out->m[10] = ffar * fn;
	out->m[11] = -1.0f;
	out->m[14] = fnear * ffar * fn;
	return out;
}

static inline Vec *matGetRotation(Vec *out, const Mat *mat) {
	const float *m = mat->m;

	float tr = m[0] + m[5] + m[10];
	if (tr >= 0.0f) {
		float s = sqrt(tr + 1.0f);
		float is = 0.5f / s;
		out->x = (m[1 * 4 + 2] - m[2 * 4 + 1]) * is;
		out->y = (m[2 * 4 + 0] - m[0 * 4 + 2]) * is;
		out->z = (m[0 * 4 + 1] - m[1 * 4 + 0]) * is;
		out->w = 0.5f * s;
		return out;
	}

	int i = 0;
	if (m[1 * 4 + 1] > m[0 * 4 + 0]) i = 1;
	if (m[2 * 4 + 2] > m[i * 4 + i]) i = 2;

	if (i == 0) {
		float s = sqrt(m[0] - (m[5] + m[10]) + 1);
		float is = 0.5f / s;
		out->x = 0.5f * s;
		out->y = (m[1 * 4 + 0] + m[0 * 4 + 1]) * is;
		out->z = (m[0 * 4 + 2] + m[2 * 4 + 0]) * is;
		out->w = (m[1 * 4 + 2] - m[2 * 4 + 1]) * is;
		return out;
	} else if (i == 1) {
		float s = sqrt(m[5] - (m[10] + m[0]) + 1);
		float is = 0.5f / s;
		out->x = (m[1 * 4 + 0] + m[0 * 4 + 1]) * is;
		out->y = 0.5f * s;
		out->z = (m[2 * 4 + 1] + m[1 * 4 + 2]) * is;
		out->w = (m[2 * 4 + 0] - m[0 * 4 + 2]) * is;
		return out;
	} else {
		float s = sqrt(m[10] - (m[0] + m[5]) + 1);
		float is = 0.5f / s;
		out->x = (m[0 * 4 + 2] + m[2 * 4 + 0]) * is;
		out->y = (m[2 * 4 + 1] + m[1 * 4 + 2]) * is;
		out->z = 0.5f * s;
		out->w = (m[0 * 4 + 1] - m[1 * 4 + 0]) * is;
	}
}

#include "vecCpp.h"

#endif