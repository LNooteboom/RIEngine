#include "Jolt.h"

// All Jolt symbols are in the JPH namespace
using namespace JPH;

class RaycastBPFilter : public JPH::BroadPhaseLayerFilter {
public:
	virtual bool ShouldCollide([[maybe_unused]] BroadPhaseLayer inLayer) const {
		return inLayer == BroadPhaseLayers::NON_MOVING || inLayer == BroadPhaseLayers::MOVING;
	}
};

class RaycastObjFilter : public JPH::ObjectLayerFilter {
public:
	RaycastObjFilter(int layerMask) {
		mLayerMask = layerMask;
	}

	virtual bool ShouldCollide([[maybe_unused]] ObjectLayer inLayer) const {
		return mLayerMask & (1 << inLayer);
	}

private:
	int mLayerMask;
};

entity_t physDoRayCast(float *outFraction, int layerMask, const Vec *pos, const Vec *dir) {
	const NarrowPhaseQuery &npq{ physicsSystem->GetNarrowPhaseQuery() };
	RRayCast ray{ {pos->x, pos->y, pos->z}, {dir->x, dir->y, dir->z} };
	RayCastResult result;
	RaycastBPFilter bpFilter;
	RaycastObjFilter objFilter(layerMask);

	bool hit = npq.CastRay(ray, result, bpFilter, objFilter, {});

	if (hit) {
		if (outFraction)
			*outFraction = result.mFraction;
		entity_t en = physicsSystem->GetBodyInterface().GetUserData(result.mBodyID);
		return en;
	} else {
		return 0;
	}
}