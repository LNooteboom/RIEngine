#include "jolt.h"

// All Jolt symbols are in the JPH namespace
using namespace JPH;

void physNewCharacter(struct PhysCharacter *ch, float radius, float halfHeight, float friction) {
	Transform *tf = getTf(ch->entity);
	// Create 'player' character
	Ref<CharacterSettings> settings = new CharacterSettings();
	RefConst<Shape> shape = RotatedTranslatedShapeSettings(JPH::Vec3(0, 0, halfHeight + radius), Quat::sEulerAngles(JPH::Vec3{ DEG2RAD(-90), 0, 0 }), new CapsuleShape(halfHeight, radius)).Create().Get();
	settings->mMaxSlopeAngle = DEG2RAD(45.0f);
	settings->mLayer = Layers::MOVING;
	settings->mShape = shape;
	settings->mFriction = friction;
	settings->mMass = 60;
	settings->mSupportingVolume = Plane(JPH::Vec3::sAxisZ(), -radius); // Accept contacts that touch the lower sphere of the capsule
	Character *character = new Character(settings, RVec3{ tf->x, tf->y, tf->z }, Quat{ tf->rx, tf->ry, tf->rz, tf->rw }, ch->entity, physicsSystem);
	character->SetUp(JPH::Vec3(0, 0, 1.0f));
	character->AddToPhysicsSystem(EActivation::Activate);
	ch->joltCharacter = character;
}
void physDeleteCharacter(struct PhysCharacter *ch) {
	Character *c = static_cast<Character *>(ch->joltCharacter);
	c->RemoveFromPhysicsSystem();
	delete c;
}
void physCharacterGetVelocity(struct PhysCharacter *ch, float *vel) {
	Character *c = static_cast<Character *>(ch->joltCharacter);
	JPH::Vec3 v = c->GetLinearVelocity();
	vel[0] = v.GetX();
	vel[1] = v.GetY();
	vel[2] = v.GetZ();
}
void physCharacterSetVelocity(struct PhysCharacter *ch, float *vel) {
	Character *c = static_cast<Character *>(ch->joltCharacter);
	c->SetLinearVelocity(JPH::Vec3{ vel[0], vel[1], vel[2] });
}
void physCharacterGetPosition(struct PhysCharacter *ch, float *pos) {
	Character *c = static_cast<Character *>(ch->joltCharacter);
	JPH::Vec3 v = c->GetPosition();
	pos[0] = v.GetX();
	pos[1] = v.GetY();
	pos[2] = v.GetZ();
}
void physCharacterSetPosition(struct PhysCharacter *ch, float *pos) {
	Character *c = static_cast<Character *>(ch->joltCharacter);
	c->SetPosition(JPH::Vec3{ pos[0], pos[1], pos[2] });
}


void joltCharacterUpdate(BodyInterface &bi) {
	for (auto b = PHYS_BODIES.begin(); b != PHYS_BODIES.end(); ++b) {
		if (b->flags & PHYS_SYNC_TO_TF) {
			Transform *tf = getTf(b->entity);
			RVec3 pos;
			Quat rot;
			bi.GetPositionAndRotation(getJBody(b.ptr()), pos, rot);
			tf->x = pos.GetX();
			tf->y = pos.GetY();
			tf->z = pos.GetZ();
			tf->rx = rot.GetX();
			tf->ry = rot.GetY();
			tf->rz = rot.GetZ();
			tf->rw = rot.GetW();
		}
	}
}


static void physCharacterNotifier(void *arg, void *component, int type) {
	if (type == NOTIFY_DELETE) {
		PhysCharacter *ch = static_cast<PhysCharacter *>(component);
		if (ch->joltCharacter) {
			Character *c = static_cast<Character *>(ch->joltCharacter);
			c->RemoveFromPhysicsSystem();
			delete c;
		}
	} else if (type == NOTIFY_PURGE) {
		for (PhysCharacter *ch = static_cast<PhysCharacter *>(clBegin(PHYS_CHARACTER)); ch; ch = static_cast<PhysCharacter *>(clNext(PHYS_CHARACTER, ch))) {
			if (ch->joltCharacter) {
				Character *c = static_cast<Character *>(ch->joltCharacter);
				c->RemoveFromPhysicsSystem();
				delete c;
			}
		}
	}
}

void joltCharacterInit(void) {
	componentListInitSz(PHYS_CHARACTER, sizeof(PhysCharacter));
	setNotifier(PHYS_CHARACTER, physCharacterNotifier, nullptr);
}

void joltCharacterFini(void) {
	componentListFini(PHYS_CHARACTER);
}