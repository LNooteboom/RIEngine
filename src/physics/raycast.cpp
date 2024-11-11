#include "Jolt.h"

// All Jolt symbols are in the JPH namespace
using namespace JPH;

entity_t physDoRayCast(float x, float y, float z, float dx, float dy, float dz, float *fraction) {
	const NarrowPhaseQuery &npq{ physicsSystem->GetNarrowPhaseQuery() };
	RRayCast ray{ Vec3{x, y, z}, Vec3{dx, dy, dz} };
	RayCastResult result;
	bool hit = npq.CastRay(ray, result, {}, {}, {});

	if (hit) {
		if (fraction)
			*fraction = result.mFraction;
		entity_t en = physicsSystem->GetBodyInterface().GetUserData(result.mBodyID);
		return en;
	} else {
		return NULL;
	}
}