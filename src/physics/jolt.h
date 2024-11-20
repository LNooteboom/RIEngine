#ifndef JOLT_H
#define JOLT_H

#ifdef __cplusplus
extern "C" {
#endif

void joltInit(void);
void joltFini(void);
void joltUpdate(void);

#ifdef __cplusplus
} // extern "C"

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>

// STL includes
#include <iostream>
#include <cstdarg>
#include <thread>

// Engine includes
#include <ecs.h>
#include <basics.h>
#include <physics/3d.h>

// Disable common warnings triggered by Jolt, you can use JPH_SUPPRESS_WARNING_PUSH / JPH_SUPPRESS_WARNING_POP to store and restore the warning state
JPH_SUPPRESS_WARNINGS

// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
namespace BroadPhaseLayers {
	static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
	static constexpr JPH::BroadPhaseLayer MOVING(1);
	static constexpr JPH::BroadPhaseLayer DEBRIS(2);
	static constexpr JPH::uint NUM_LAYERS(2);
};

extern JPH::TempAllocatorImpl *tempAllocator;
extern JPH::PhysicsSystem *physicsSystem;


static inline PhysBody *getBody(entity_t entity) {
	return &PHYS_BODIES[entity];
}
static inline PhysBody *addBody(entity_t entity) {
	return &PHYS_BODIES.add(entity);
}
static inline Transform *getTf(entity_t entity) {
	//return static_cast<Transform *>(getComponent(TRANSFORM, entity));
	return &TRANSFORMS[entity];
}
static inline JPH::BodyID getJBody(PhysBody *pb) {
	return JPH::BodyID{ pb->joltBody };
}
static inline entity_t getEntity(JPH::Body *b) {
	return static_cast<entity_t>(b->GetUserData());
}


void joltBodyUpdatePre(JPH::BodyInterface &bi);
void joltBodyUpdatePost(JPH::BodyInterface &bi);
void joltBodyInit(void);
void joltBodyFini(void);

void joltCharacterUpdatePre(JPH::BodyInterface &bi);
void joltCharacterUpdatePost(JPH::BodyInterface &bi);
void joltCharacterInit(void);
void joltCharacterFini(void);

#endif // ifdef __cplusplus

#endif