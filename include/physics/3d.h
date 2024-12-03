#ifndef PHYS3D_H
#define PHYS3D_H

#include <ecs.h>
#include <vec.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PHYS_TICKRATE 60.0f
#define PHYS_DELTATIME (1.0f / PHYS_TICKRATE)

#define PHYS_FLAG_SYNC_TO_TF 1
#define PHYS_FLAG_SYNC_FROM_TF 2
#define PHYS_FLAG_MOVING 4
#define PHYS_FLAG_KINEMATIC 8
#define PHYS_FLAG_HAS_BODY 16
#define PHYS_FLAG_CONTINUOUS_COLLISION 32
#define PHYS_FLAG_SENSOR 64

enum PhysLayer {
	PHYS_LAYER_STATIC,
	PHYS_LAYER_MOVING,
	PHYS_LAYER_CHAR_HITBOX,
	PHYS_LAYER_CHAR_HURTBOX,
	PHYS_LAYER_WEAPON,
	PHYS_LAYER_DEBRIS,

	PHYS_LAYER_N
};

#define PHYS_LAYER_STATIC_MASK 1
#define PHYS_LAYER_MOVING_MASK 2
#define PHYS_LAYER_CHAR_HITBOX_MASK 4
#define PHYS_LAYER_CHAR_HURTBOX_MASK 8
#define PHYS_LAYER_WEAPON_MASK 16
#define PHYS_LAYER_DEBRIS_MASK 32


struct PhysCollisionInfo {
	int todo;
};
struct PhysBodyCollFuncs {
	void (*onAdded)(struct PhysBody *b, struct PhysBody *other, struct PhysCollisionInfo *info);
	void (*onPersisted)(struct PhysBody *b, struct PhysBody *other, struct PhysCollisionInfo *info);
	void (*onRemoved)(struct PhysBody *b, struct PhysBody *other, struct PhysCollisionInfo *info);
};

struct PhysBody {
	entity_t entity;
	int flags;
	enum PhysLayer layer;

	const struct PhysBodyCollFuncs *collFuncs;

	uint32_t joltBody;
};

enum PhysCharacterGroundState {
	PHYS_CHAR_SUPPORTED,
	PHYS_CHAR_TOO_STEEP,
	PHYS_CHAR_UNSUPPORTED,
	PHYS_CHAR_IN_AIR
};
struct PhysCharacter {
	entity_t entity;
	bool enable;

	enum PhysCharacterGroundState groundState;
	Vec groundNorm;
	Vec groundVel;

	Vec vel;

	bool isVirtual;
	void *joltCharacter;
};

struct PhysMaterial {
	int aaa;
};

void physNewBodyBox(struct PhysBody *b, const Vec *offset,  const Vec *rotation, const Vec *halfSize, struct PhysMaterial *mat);
void physNewBodyMesh(struct PhysBody *b, const char *meshName);
void physNewBodySphere(struct PhysBody *b, float radius, struct PhysMaterial *mat);
void physNewBodyCapsule(struct PhysBody *b, float halfZ, float radius, struct PhysMaterial *mat);
void physNewBodyCylinder(struct PhysBody *b, float halfZ, float radius, struct PhysMaterial *mat);

void physDeleteBody(struct PhysBody *b);
void physAddBody(struct PhysBody *b);
void physRemoveBody(struct PhysBody *b);
void physBodyUpdatePosition(struct PhysBody *b);
void physBodySetVelocity(struct PhysBody *b, const Vec *vel);

void physNewCharacter(struct PhysCharacter *ch, float radius, float halfHeight, float friction);
void physNewCharacterVirtual(struct PhysCharacter *ch, float radius, float halfHeight);
void physDeleteCharacter(struct PhysCharacter *ch);

void physCharacterSetVelocity(struct PhysCharacter *ch, float vx, float vy, float vz);
void physCharacterSetPosition(struct PhysCharacter *ch, float x, float y, float z);

entity_t physDoRayCast(float *outFraction, int layerMask, const Vec *pos, const Vec *dir);

#ifndef RELEASE
void physEnableDebugRender();
void physDisableDebugRender();
#endif

#ifdef __cplusplus
} // extern "C"

constexpr ClWrapper<PhysBody, PHYS_BODY> PHYS_BODIES;
constexpr ClWrapper<PhysCharacter, PHYS_CHARACTER> PHYS_CHARACTERS;
#endif

#endif