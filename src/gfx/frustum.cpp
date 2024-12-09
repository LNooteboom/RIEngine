#include <gfx/draw.h>
#include <vec.h>
#include "gfx.h"

struct Plane {
	Vec3 normal;
	float dist;

	Plane() : normal{ 0, 0, 0 }, dist(0) {

	}

	Plane(float x, float y, float z, float w) : normal { x, y, z }, dist (w) {
	}

	Plane(const Vec3 &inPos, const Vec3 &inNormal) {
		normal = inNormal.normalized();
		dist = Vec3::dot(normal, inPos);
	}

	float getSignedDistance(const Vec3 &point) const {
		return Vec3::dot(point, normal) - dist;
	}
};

struct Frustum {
	Plane topFace;
	Plane bottomFace;
	
	Plane rightFace;
	Plane leftFace;
	
	Plane farFace;
	Plane nearFace;
};

struct AABB {
	Vec3 center;
	Vec3 extent;

	AABB(const Vec3 &inCenter, const Vec3 &inExtent) : center(inCenter), extent(inExtent) {

	}

	bool isOnOrForwardPlane(const Plane &plane) const {
		float r = extent.x * fabsf(plane.normal.x) + extent.y * fabsf(plane.normal.y) + extent.z * fabsf(plane.normal.z);
		bool ret = -r <= plane.getSignedDistance(center);
		return ret;
	}
};

static Frustum curFrustum;

void drawUpdateFrustum(const Mat *projMat) {
	Vec3 position{ cam3DPos.x, cam3DPos.y, cam3DPos.z };
	Vec3 right{ Vec3{ cam3DMatrix.m[0], cam3DMatrix.m[4], cam3DMatrix.m[8] }.normalized() };
	Vec3 up{ Vec3{ cam3DMatrix.m[1], cam3DMatrix.m[5], cam3DMatrix.m[9] }.normalized() };
	Vec3 forward{ Vec3{ -cam3DMatrix.m[2], -cam3DMatrix.m[6], -cam3DMatrix.m[10] }.normalized() };

	const float zNear = CAM_3D_NEAR;
	const float zFar = CAM_3D_FAR;
	const float aspect = (float)rttIntW / rttIntH;
	const float halfVSide = zFar * tanf(cam3DFov * 0.5f);
	const float halfHSide = halfVSide * aspect;
	const Vec3 frontMultFar = forward * zFar;

	curFrustum.nearFace = { position + forward * zNear, forward };
	curFrustum.farFace = { position + frontMultFar, -forward };
	curFrustum.rightFace = { position, Vec3::cross(frontMultFar - right * halfHSide, up) };
	curFrustum.leftFace = { position, Vec3::cross(up, frontMultFar + right * halfHSide) };
	curFrustum.topFace = { position, Vec3::cross(right, frontMultFar - up * halfVSide) };
	curFrustum.bottomFace = { position, Vec3::cross(frontMultFar + up * halfVSide, right) };
}

bool drawModelInFrustum(struct Model *m) {
	const Mat &transform = drawState.matStack[drawState.matStackIdx];
	Vec3 center = transform * Vec4(m->aabbCenter, 1);

	Vec3 right = Vec3{ transform.m[0], transform.m[1], transform.m[2] } * m->aabbHalfExtent.x;
	Vec3 fwd = Vec3{ transform.m[4], transform.m[5], transform.m[6] } * m->aabbHalfExtent.y;
	Vec3 up = Vec3{ transform.m[8], transform.m[9], transform.m[10] } * m->aabbHalfExtent.z;
	float newIi =
		fabsf(Vec3::dot({ 1, 0, 0 }, right)) +
		fabsf(Vec3::dot({ 1, 0, 0 }, fwd)) +
		fabsf(Vec3::dot({ 1, 0, 0 }, up));
	float newIj =
		fabsf(Vec3::dot({ 0, 1, 0 }, right)) +
		fabsf(Vec3::dot({ 0, 1, 0 }, fwd)) +
		fabsf(Vec3::dot({ 0, 1, 0 }, up));
	float newIk =
		fabsf(Vec3::dot({ 0, 0, 1 }, right)) +
		fabsf(Vec3::dot({ 0, 0, 1 }, fwd)) +
		fabsf(Vec3::dot({ 0, 0, 1 }, up));

	const AABB aabb{ center, Vec3{newIi, newIj, newIk} };
	return (aabb.isOnOrForwardPlane(curFrustum.leftFace) &&
		aabb.isOnOrForwardPlane(curFrustum.rightFace) &&
		aabb.isOnOrForwardPlane(curFrustum.topFace) &&
		aabb.isOnOrForwardPlane(curFrustum.bottomFace) &&
		aabb.isOnOrForwardPlane(curFrustum.nearFace) &&
		aabb.isOnOrForwardPlane(curFrustum.farFace));
}