#ifndef PHYS3D_H
#define PHYS3D_H

#include <ecs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PHYS_SYNC_TO_TF 1
#define PHYS_SYNC_FROM_TF 2
#define PHYS_MOVING 4
#define PHYS_KINEMATIC 8
#define PHYS_HAS_BODY 16

struct PhysBody {
	entity_t entity;
	int flags;

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

	enum PhysCharacterGroundState groundState;
	float groundNormX, groundNormY, groundNormZ;

	void *joltCharacter;
};

struct PhysMaterial {
	int aaa;
};

void physNewBodyBox(struct PhysBody *b, float *halfSize, struct PhysMaterial *mat);
void physNewBodyMesh(struct PhysBody *b, const char *meshFile, const char *meshName);
void physNewBodySphere(struct PhysBody *b, float radius, struct PhysMaterial *mat);
void physNewBodyCapsule(struct PhysBody *b, float halfZ, float radius, struct PhysMaterial *mat);
void physNewBodyCylinder(struct PhysBody *b, float halfZ, float radius, struct PhysMaterial *mat);

void physDeleteBody(struct PhysBody *b);
void physAddBody(struct PhysBody *b);
void physRemoveBody(struct PhysBody *b);

void physNewCharacter(struct PhysCharacter *ch, float radius, float halfHeight, float friction);
void physDeleteCharacter(struct PhysCharacter *ch);
void physCharacterGetVelocity(struct PhysCharacter *ch, float *vel);
void physCharacterSetVelocity(struct PhysCharacter *ch, float *vel);
void physCharacterGetPosition(struct PhysCharacter *ch, float *pos);
void physCharacterSetPosition(struct PhysCharacter *ch, float *pos);

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