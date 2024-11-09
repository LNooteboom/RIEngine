#ifndef VEC_H
#define VEC_H

#include "basics.h"
#include <math.h>

#ifdef __cplusplus

#define USE_SSE
#ifdef USE_SSE
#include <immintrin.h>
#define splat_x(r) _mm_shuffle_ps(r, r, 0)
#define splat_y(r) _mm_shuffle_ps(r, r, 0x55)
#define splat_z(r) _mm_shuffle_ps(r, r, 0xAA)
#define splat_w(r) _mm_shuffle_ps(r, r, 0xFF)
#endif

struct Vec3 {
	Vec3() : x(0), y(0), z(0) {

	}
	Vec3(float xv, float yv, float zv) : x(xv), y(yv), z(zv) {

	}
	Vec3(float *v) : x(v[0]), y(v[1]), z(v[2]) {

	}
	Vec3(float v) : x(v), y(v), z(v) {

	}

	Vec3 operator+(const Vec3 &other) const {
		return Vec3{ x + other.x, y + other.y, z + other.z };
	}
	Vec3 &operator+=(const Vec3 &other) {
		x += other.x;
		y += other.y;
		z += other.z;
	}
	Vec3 operator-() const { // Unary
		return Vec3{ -x, -y, -z };
	}
	Vec3 operator-(const Vec3 &other) const {
		return Vec3{ x - other.x, y - other.y, z - other.z };
	}
	Vec3 &operator-=(const Vec3 &other) {
		x -= other.x;
		y -= other.y;
		z -= other.z;
	}
	Vec3 operator*(const Vec3 &other) const {
		return Vec3{ x * other.x, y * other.y, z * other.z };
	}
	Vec3 &operator*=(const Vec3 &other) {
		x *= other.x;
		y *= other.y;
		z *= other.z;
	}
	Vec3 operator*(float s) const {
		return Vec3{ x * s, y * s, z * s };
	}

	float length() const {
		return sqrtf(x * x + y * y + z * z);
	}

	void round() {
		x = roundf(x);
		y = roundf(y);
		z = roundf(z);
	}

	Vec3 normalized() const {
		float il = 1.0f / length();
		return Vec3{
			x * il,
			y * il,
			z * il
		};
	}


	static float dot(const Vec3 &v1, const Vec3 &v2) {
		return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
	}
	static Vec3 cross(const Vec3 &v1, const Vec3 &v2)  {
		return Vec3{
			v1.y *v2.z - v1.z * v2.y,
			v1.z *v2.x - v1.x * v2.z,
			v1.x *v2.y - v1.y * v2.x
		};
	}
	static Vec3 lerp(const Vec3 &v1, const Vec3 &v2, float x) {
		float ix = 1.0f - x;
		return Vec3{
			ix * v1.x + x * v2.x,
			ix * v1.y + x * v2.y,
			ix * v1.z + x * v2.z
		};
	}

	static void setTfPos(Transform &tf, const Vec3 &v) {
		tf.x = v.x;
		tf.y = v.y;
		tf.z = v.z;
	}
	static Vec3 fromTfPos(const Transform &tf) {
		return Vec3{ tf.x, tf.y, tf.z };
	}

	float x, y, z;
};

struct Vec4 {
	Vec4() : x(0), y(0), z(0), w(0) {

	}
	Vec4(float xv, float yv, float zv, float wv) : x(xv), y(yv), z(zv), w(wv) {

	}
	Vec4(float *v) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) {

	}
	Vec4(Vec3 v, float wv) : x(v.x), y(v.y), z(v.z), w(wv) {

	}

	Vec3 xyz() const {
		return Vec3{ x, y, z };
	}

	Vec4 normalized() const {
		float invMag = 1.0f / sqrtf(x * x + y * y + z * z + w * w);
		return Vec4{
			x * invMag,
			y * invMag,
			z * invMag,
			w * invMag
		};
	}

	Vec4 operator*(float s) const {
		return Vec4{ x * s, y * s, z * s, w * s };
	}

	static float dot(const Vec4 &v1, const Vec4 &v2) {
		return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z) + (v1.w * v2.w);
	}

	static Vec4 eulerAngles(float rx, float ry, float rz) {
		float cx = cosf(0.5f * rx), sx = sinf(0.5f * rx);
		float cy = cosf(0.5f * ry), sy = sinf(0.5f * ry);
		float cz = cosf(0.5f * rz), sz = sinf(0.5f * rz);
		return Vec4{
			cz * sx * cy - sz * cx * sy,
			cz * cx * sy + sz * sx * cy,
			sz * cx * cy - cz * sx * sy,
			cz * cx * cy + sz * sx * sy
		};
	}
	static Vec4 zAxis(float rz) {
		return Vec4{ 0, 0, sinf(0.5f * rz), cosf(0.5f * rz) };
	}
	static Vec4 lerp(const Vec4 &v1, const Vec4 &v2, float t) {
		float it = 1.0f - t;
		return Vec4{
			it * v1.x + t * v2.x,
			it * v1.y + t * v2.y,
			it * v1.z + t * v2.z,
			it * v1.w + t * v2.w
		};
	}
	static Vec4 nlerp(const Vec4 &v1, const Vec4 &v2, float t) {
		float dt = dot(v1, v2);
		if (dt < 0)
			return lerp(v1, v2 * -1.0f, t).normalized();
		else
			return lerp(v1, v2, t).normalized();
	}

	static Vec4 fromTfRot(const Transform &tf) {
		return Vec4{ tf.rx, tf.ry, tf.rz, tf.rw };
	}
	static void setTfRot(Transform &tf, Vec4 v) {
		tf.rx = v.x;
		tf.ry = v.y;
		tf.rz = v.z;
		tf.rw = v.w;
	}

	float x, y, z, w;
};

class alignas(16) Mat {
public:
	Mat() {
		for (int i = 0; i < 16; i++) {
			m[i] = 0;
		}
	}
	Mat(float ident) {
		for (int i = 0; i < 16; i++) {
			m[i] = 0;
		}
		m[0] = ident;
		m[5] = ident;
		m[10] = ident;
		m[15] = ident;
	}
	Mat(float *src) {
		for (int i = 0; i < 16; i++) {
			m[i] = src[i];
		}
	}

	Mat operator*(const Mat &m2) const {
		Mat ret;
#ifdef USE_SSE
		__m128 l, r0, r1, r2, r3, v0, v1, v2, v3;
		r0 = _mm_load_ps(&m2.m[0]);
		r1 = _mm_load_ps(&m2.m[4]);
		r2 = _mm_load_ps(&m2.m[8]);
		r3 = _mm_load_ps(&m2.m[12]);

		l = _mm_load_ps(&m[0]);
		v0 = _mm_mul_ps(l, splat_x(r0));
		v1 = _mm_mul_ps(l, splat_x(r1));
		v2 = _mm_mul_ps(l, splat_x(r2));
		v3 = _mm_mul_ps(l, splat_x(r3));

		l = _mm_load_ps(&m[4]);
		v0 = _mm_add_ps(v0, _mm_mul_ps(l, splat_y(r0)));
		v1 = _mm_add_ps(v1, _mm_mul_ps(l, splat_y(r1)));
		v2 = _mm_add_ps(v2, _mm_mul_ps(l, splat_y(r2)));
		v3 = _mm_add_ps(v3, _mm_mul_ps(l, splat_y(r3)));

		l = _mm_load_ps(&m[8]);
		v0 = _mm_add_ps(v0, _mm_mul_ps(l, splat_z(r0)));
		v1 = _mm_add_ps(v1, _mm_mul_ps(l, splat_z(r1)));
		v2 = _mm_add_ps(v2, _mm_mul_ps(l, splat_z(r2)));
		v3 = _mm_add_ps(v3, _mm_mul_ps(l, splat_z(r3)));

		l = _mm_load_ps(&m[12]);
		v0 = _mm_add_ps(v0, _mm_mul_ps(l, splat_w(r0)));
		v1 = _mm_add_ps(v1, _mm_mul_ps(l, splat_w(r1)));
		v2 = _mm_add_ps(v2, _mm_mul_ps(l, splat_w(r2)));
		v3 = _mm_add_ps(v3, _mm_mul_ps(l, splat_w(r3)));

		_mm_store_ps(&ret.m[0], v0);
		_mm_store_ps(&ret.m[4], v1);
		_mm_store_ps(&ret.m[8], v2);
		_mm_store_ps(&ret.m[12], v3);
#else
		for (int i = 0; i < 4; i++) {
			int col = i * 4;
			ret.m[col + 0] = m[0] * m2.m[col + 0] + m[4] * m2.m[col + 1] + m[8] * m2.m[col + 2] + m[12] * m2.m[col + 3];
			ret.m[col + 1] = m[1] * m2.m[col + 0] + m[5] * m2.m[col + 1] + m[9] * m2.m[col + 2] + m[13] * m2.m[col + 3];
			ret.m[col + 2] = m[2] * m2.m[col + 0] + m[6] * m2.m[col + 1] + m[10] * m2.m[col + 2] + m[14] * m2.m[col + 3];
			ret.m[col + 3] = m[3] * m2.m[col + 0] + m[7] * m2.m[col + 1] + m[11] * m2.m[col + 2] + m[15] * m2.m[col + 3];
		}
#endif
		return ret;
	}
	Vec4 operator*(const Vec4 &v) const {
#ifdef USE_SSE
		Vec4 ret;
		__m128 r = _mm_loadu_ps(&v.x);
		__m128 l0 = _mm_load_ps(&m[0]);
		__m128 l1 = _mm_load_ps(&m[4]);
		__m128 l2 = _mm_load_ps(&m[8]);
		__m128 l3 = _mm_load_ps(&m[12]);
		__m128 v0 = _mm_mul_ps(l0, splat_x(r));
		v0 = _mm_add_ps(v0, _mm_mul_ps(l1, splat_y(r)));
		v0 = _mm_add_ps(v0, _mm_mul_ps(l2, splat_z(r)));
		v0 = _mm_add_ps(v0, _mm_mul_ps(l3, splat_w(r)));
		_mm_storeu_ps(&ret.x, v0);
		return ret;
#else
		return Vec4{
			m[0] * v.x + m[4] * v.y + m[ 8] * v.z + m[12] * v.w,
			m[1] * v.x + m[5] * v.y + m[ 9] * v.z + m[13] * v.w,
			m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
			m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w,
		};
#endif
	}

	Mat transposed() const {
		Mat ret;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				ret.m[i * 4 + j] = m[j * 4 + i];
			}
		}
		return ret;
	}

	void translate(const Vec3 &v) {
		m[12] += m[0] * v.x + m[4] * v.y + m[8] * v.z;
		m[13] += m[1] * v.x + m[5] * v.y + m[9] * v.z;
		m[14] += m[2] * v.x + m[6] * v.y + m[10] * v.z;
	}
	void scale(const Vec3 &v) {
		for (int i = 0; i < 3; i++) {
			m[0 + i] *= v.x;
			m[4 + i] *= v.y;
			m[8 + i] *= v.z;
		}
	}
	void rotate(const Vec4 &v) {
		Mat rot = fromRotation(v);
		*this = (*this) * rot;
	}

	void save(float *dat) const {
		for (int i = 0; i < 16; i++) {
			dat[i] = m[i];
		}
	}

	void inverse3() {
		Mat ret;
		float det;
		float a = m[0 * 4 + 0], b = m[0 * 4 + 1], c = m[0 * 4 + 2],
			d = m[1 * 4 + 0], e = m[1 * 4 + 1], f = m[1 * 4 + 2],
			g = m[2 * 4 + 0], h = m[2 * 4 + 1], i = m[2 * 4 + 2];

		m[0 * 4 + 0] = e * i - f * h;
		m[0 * 4 + 1] = -(b * i - h * c);
		m[0 * 4 + 2] = b * f - e * c;
		m[1 * 4 + 0] = -(d * i - g * f);
		m[1 * 4 + 1] = a * i - c * g;
		m[1 * 4 + 2] = -(a * f - d * c);
		m[2 * 4 + 0] = d * h - g * e;
		m[2 * 4 + 1] = -(a * h - g * b);
		m[2 * 4 + 2] = a * e - b * d;

		det = 1.0f / (a * m[0 * 4 + 0] + b * m[1 * 4 + 0] + c * m[2 * 4 + 0]);
		for (int i = 0; i < 16; i++) {
			m[i] *= det;
		}
	}
	void transposeSave3(float *dat) const {
		dat[0] = m[0];
		dat[1] = m[4];
		dat[2] = m[8];
		dat[3] = 0;
		dat[4] = m[1];
		dat[5] = m[5];
		dat[6] = m[9];
		dat[7] = 0;
		dat[8] = m[2];
		dat[9] = m[6];
		dat[10] = m[10];
		dat[11] = 0;
		dat[12] = 0;
		dat[13] = 0;
		dat[14] = 0;
		dat[15] = 1.0f;
	}

	static Mat lookat(const Vec3 &eye, const Vec3 &center, const Vec3 &up) {
		Vec3 f = (center - eye).normalized(); // forward
		Vec3 s = Vec3::cross(f, up).normalized(); // right
		Vec3 u = Vec3::cross(s, f); // up
		Mat ret;
		ret.m[0] = s.x;
		ret.m[1] = u.x;
		ret.m[2] = -f.x;
		ret.m[3] = 0;
		ret.m[4] = s.y;
		ret.m[5] = u.y;
		ret.m[6] = -f.y;
		ret.m[7] = 0;
		ret.m[8] = s.z;
		ret.m[9] = u.z;
		ret.m[10] = -f.z;
		ret.m[11] = 0;
		ret.m[12] = -Vec3::dot(s, eye);
		ret.m[13] = -Vec3::dot(u, eye);
		ret.m[14] = Vec3::dot(f, eye);
		ret.m[15] = 1.0f;
		return ret;
	}
	static Mat look(const Vec3 &eye, const Vec3 &dir, const Vec3 &up) {
		return lookat(eye, eye + dir, up);
	}

	static Mat fromRotation(const Vec4 &r) {
		Mat ret;
		float x = r.x;
		float y = r.y;
		float z = r.z;
		float w = r.w;

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

		ret.m[0] = (1.0f - yy) - zz;
		ret.m[1] = xy + zw;
		ret.m[2] = xz - yw;
		ret.m[3] = 0;
		ret.m[4] = xy - zw;
		ret.m[5] = (1.0f - zz) - xx;
		ret.m[6] = yz + xw;
		ret.m[7] = 0;
		ret.m[8] = xz + yw;
		ret.m[9] = yz - xw;
		ret.m[10] = (1.0f - xx) - yy;
		ret.m[11] = 0;
		ret.m[12] = 0;
		ret.m[13] = 0;
		ret.m[14] = 0;
		ret.m[15] = 1.0f;
		return ret;
	}
	static Mat fromTranslation(const Vec3 &v) {
		Mat ret{ 1.0f };
		ret.m[12] = v.x;
		ret.m[13] = v.y;
		ret.m[14] = v.z;
		return ret;
	}
	static Mat fromScale(const Vec3 &v) {
		Mat ret{ 1.0f };
		ret.m[0] = v.x;
		ret.m[5] = v.y;
		ret.m[10] = v.z;
		return ret;
	}

	static Mat perspective(float fovy, float aspect, float fnear, float ffar) {
		Mat mat;
		float f = 1.0f / tanf(fovy * 0.5f);
		float fn = 1.0f / (fnear - ffar);
		mat.m[0] = f / aspect;
		mat.m[5] = f;
		mat.m[10] = ffar * fn;
		mat.m[11] = -1.0f;
		mat.m[14] = fnear * ffar * fn;
		return mat;
	}

	float m[16];
};

#else

typedef struct {
	float x, y, z;
} Vec3;
typedef struct {
	float x, y, z, w;
} Vec4;

#ifdef _MSC_VER
#define ALIGN_MAT __declspec(align(16))
#else
#define ALIGN_MAT __attribute((aligned(16)))
#endif
struct MyMat {
	float m[16];
};
typedef ALIGN_MAT struct MyMat Mat;

#endif // __cplusplus

#endif