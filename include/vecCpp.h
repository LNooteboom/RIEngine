#ifndef VEC_CPP_H
#define VEC_CPP_H

#include "basics.h"
#include <math.h>

#ifdef __cplusplus

inline Vec::Vec() {
	x = y = z = w = 0;
}
inline Vec::Vec(float v) {
	x = v;
	y = v;
	z = v;
	w = 0;
}
inline Vec::Vec(float xv, float yv, float zv) {
	x = xv;
	y = yv;
	z = zv;
	w = 0;
}
inline Vec::Vec(float xv, float yv, float zv, float wv) {
	x = xv;
	y = yv;
	z = zv;
	w = wv;
}
inline Vec::Vec(Vec v, float wv) {
	x = v.x;
	y = v.y;
	z = v.z;
	w = wv;
}
inline Vec::Vec(float *v) {
	x = v[0];
	y = v[1];
	z = v[2];
	w = v[3];
}

inline Vec Vec::normalized() const {
	Vec v;
	vecNormalize3(&v, this);
	return v;
}
inline Vec Vec::operator-() const {
	Vec v;
	vecMulS(&v, this, -1);
	return v;
}
inline Vec Vec::operator+(const Vec &v2) const {
	Vec v;
	vecAdd(&v, this, &v2);
	return v;
}
inline Vec Vec::operator-(const Vec &v2) const {
	Vec v;
	vecSub(&v, this, &v2);
	return v;
}
inline Vec Vec::operator*(float s) const {
	Vec v;
	vecMulS(&v, this, s);
	return v;
}
inline Vec Vec::operator*(const Vec &v2) const {
	Vec v;
	vecMul(&v, this, &v2);
	return v;
}
inline float Vec::dot(const Vec &v1, const Vec &v2) {
	return vecDot3(&v1, &v2);
}
inline Vec Vec::cross(const Vec3 &v1, const Vec3 &v2) {
	Vec v;
	vecCross3(&v, &v1, &v2);
	return v;
}
inline Vec Vec::eulerAngles(float rx, float ry, float rz) {
	Vec v = { rx, ry, rz, 0 };
	quatEulerAngles(&v, &v);
	return v;
}
inline Vec Vec::zAxis(float rz) {
	return Vec{ 0, 0, sinf(0.5f * rz), cosf(0.5f * rz) };
}
inline Vec Vec::lerp(const Vec &v1, const Vec &v2, float t) {
	Vec v;
	vecLerp(&v, &v1, &v2, t);
	return v;
}
inline Vec Vec::nlerp(const Vec &v1, const Vec &v2, float t) {
	Vec v;
	quatNlerp(&v, &v1, &v2, t);
	return v;
}
inline Vec Vec::fromTfRot(const Transform &tf) {
	Vec v;
	vecFromTfRot(&v, &tf);
	return v;
}
inline void Vec::setTfRot(Transform &tf, Vec v) {
	vecToTfRot(&tf, &v);
}
inline Vec Vec::fromTfPos(const Transform &tf) {
	Vec v;
	vecFromTfPos(&v, &tf);
	return v;
}



inline Mat::Mat() {
	matIdent(this, 0.0f);
}
inline Mat::Mat(float ident) {
	matIdent(this, ident);
}
inline Mat::Mat(float *v) {
	for (int i = 0; i < 16; i++) {
		m[i] = v[i];
	}
}
inline Mat Mat::operator*(const Mat &m2) const {
	Mat ret;
	matMul(&ret, this, &m2);
	return ret;
}
inline Vec Mat::operator*(const Vec &v) const {
	Vec ret;
	matMulV4(&ret, this, &v);
	return ret;
}
inline Mat Mat::transposed() const {
	Mat m;
	matTranspose3(&m, this);
	return m;
}
inline void Mat::translate(const Vec &v) {
	matTranslate(this, this, &v);
}
inline void Mat::scale(const Vec &v) {
	matScale(this, this, &v);
}
inline void Mat::rotate(const Vec &v) {
	matRotate(this, this, &v);
}
inline void Mat::inverse3() {
	matInverse3(this, this);
}
inline void Mat::transposeSave3(float *dat) {
	Mat ret;
	matTranspose3(&ret, this);
	for (int i = 0; i < 16; i++)
		ret.m[i] = m[i];
}
inline Mat Mat::lookat(const Vec &eye, const Vec &center, const Vec &up) {
	Mat ret;
	matLookAt(&ret, &eye, &center, &up);
	return ret;
}
inline Mat Mat::look(const Vec &eye, const Vec &dir, const Vec &up) {
	Mat ret;
	matLook(&ret, &eye, &dir, &up);
	return ret;
}
inline Mat Mat::fromRotation(const Vec &r) {
	Mat ret;
	matFromRotation(&ret, &r);
	return ret;
}
inline Mat Mat::fromTranslation(const Vec &v) {
	Mat ret;
	matFromTranslation(&ret, &v);
	return ret;
}
inline Mat Mat::fromScale(const Vec &v) {
	Mat ret;
	matFromScale(&ret, &v);
	return ret;
}
inline Mat Mat::perspective(float fovy, float aspect, float fnear, float ffar) {
	Mat ret;
	matPerspective(&ret, fovy, aspect, fnear, ffar);
	return ret;
}

#endif // __cplusplus

#endif