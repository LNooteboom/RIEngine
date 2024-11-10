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
	ch->isVirtual = false;
}

void physNewCharacterVirtual(struct PhysCharacter *ch, float radius, float halfHeight) {
	Transform *tf = getTf(ch->entity);
	Ref<CharacterVirtualSettings> settings = new CharacterVirtualSettings();
	RefConst<Shape> shape = RotatedTranslatedShapeSettings(JPH::Vec3(0, 0, halfHeight + radius), Quat::sEulerAngles(JPH::Vec3{ DEG2RAD(-90), 0, 0 }), new CapsuleShape(halfHeight, radius)).Create().Get();
	settings->mMaxSlopeAngle = DEG2RAD(45.0f);
	settings->mShape = shape;
	settings->mMass = 60;
	settings->mSupportingVolume = Plane(JPH::Vec3::sAxisZ(), -radius); // Accept contacts that touch the lower sphere of the capsule
	CharacterVirtual *c = new CharacterVirtual(settings, RVec3{ tf->x, tf->y, tf->z }, Quat{ tf->rx, tf->ry, tf->rz, tf->rw }, ch->entity, physicsSystem);
	c->SetUp(JPH::Vec3(0, 0, 1.0f));
	ch->joltCharacter = c;
	ch->isVirtual = true;
}


void physDeleteCharacter(struct PhysCharacter *ch) {
	if (ch->isVirtual) {
		delete static_cast<CharacterVirtual *>(ch->joltCharacter);
	} else {
		Character *c = static_cast<Character *>(ch->joltCharacter);
		c->RemoveFromPhysicsSystem();
		delete c;
	}
	ch->joltCharacter = NULL;
}

void physCharacterGetVelocity(struct PhysCharacter *ch, float *vel) {
	JPH::Vec3 v;
	if (ch->isVirtual) {
		v = static_cast<CharacterVirtual *>(ch->joltCharacter)->GetLinearVelocity();
	} else {
		v = static_cast<Character *>(ch->joltCharacter)->GetLinearVelocity();
	}
	vel[0] = v.GetX();
	vel[1] = v.GetY();
	vel[2] = v.GetZ();
}

void physCharacterSetVelocity(struct PhysCharacter *ch, float *vel) {
	JPH::Vec3 v{ vel[0], vel[1], vel[2] };
	if (ch->isVirtual) {
		static_cast<CharacterVirtual *>(ch->joltCharacter)->SetLinearVelocity(v);
	} else {
		static_cast<Character *>(ch->joltCharacter)->SetLinearVelocity(v);
	}
}

void physCharacterGetPosition(struct PhysCharacter *ch, float *pos) {
	JPH::Vec3 v;
	if (ch->isVirtual) {
		v = static_cast<CharacterVirtual *>(ch->joltCharacter)->GetPosition();
	} else {
		v = static_cast<Character *>(ch->joltCharacter)->GetPosition();
	}
	pos[0] = v.GetX();
	pos[1] = v.GetY();
	pos[2] = v.GetZ();
}

void physCharacterSetPosition(struct PhysCharacter *ch, float *pos) {
	JPH::Vec3 v{ pos[0], pos[1], pos[2] };
	if (ch->isVirtual) {
		static_cast<CharacterVirtual *>(ch->joltCharacter)->SetPosition(v);
	} else {
		static_cast<Character *>(ch->joltCharacter)->SetPosition(v);
	}
}


static void charNormalUpdate(BodyInterface &bi, PhysCharacter *ch) {
	Character *jch = static_cast<Character *>(ch->joltCharacter);
	jch->PostSimulation(0.05f);
	switch (jch->GetGroundState()) {
	case Character::EGroundState::OnGround:
		ch->groundState = PHYS_CHAR_SUPPORTED;
		break;
	case Character::EGroundState::OnSteepGround:
		ch->groundState = PHYS_CHAR_TOO_STEEP;
		break;
	case Character::EGroundState::NotSupported:
		ch->groundState = PHYS_CHAR_UNSUPPORTED;
		break;
	case Character::EGroundState::InAir:
		ch->groundState = PHYS_CHAR_IN_AIR;
		break;
	}
	if (ch->groundState != PHYS_CHAR_IN_AIR) {
		JPH::Vec3 gn = jch->GetGroundNormal();
		ch->groundNormX = gn.GetX();
		ch->groundNormY = gn.GetY();
		ch->groundNormZ = gn.GetZ();
	}
}


class CharVirtBPLayerFilter : public JPH::BroadPhaseLayerFilter {
	virtual bool ShouldCollide([[maybe_unused]] BroadPhaseLayer inLayer) const
	{
		return true;
	}
};
CharVirtBPLayerFilter charVirtBPLayerFilter;

class CharVirtObjLayerFilter : public JPH::ObjectLayerFilter {
	virtual bool ShouldCollide([[maybe_unused]] ObjectLayer inLayer) const
	{
		return true;
	}
};
CharVirtObjLayerFilter charVirtObjLayerFilter;

class CharVirtBodyFilter : public JPH::BodyFilter {
	virtual bool ShouldCollide([[maybe_unused]] const BodyID &inBodyID) const
	{
		return true;
	}

	virtual bool ShouldCollideLocked([[maybe_unused]] const Body &inBody) const
	{
		return true;
	}
};
CharVirtBodyFilter charVirtBodyFilter;

class CharVirtShapeFilter : public JPH::ShapeFilter {
	virtual bool ShouldCollide([[maybe_unused]] const Shape *inShape2, [[maybe_unused]] const SubShapeID &inSubShapeIDOfShape2) const
	{
		return true;
	}

	virtual bool ShouldCollide([[maybe_unused]] const Shape *inShape1, [[maybe_unused]] const SubShapeID &inSubShapeIDOfShape1, [[maybe_unused]] const Shape *inShape2, [[maybe_unused]] const SubShapeID &inSubShapeIDOfShape2) const
	{
		return true;
	}
};
CharVirtShapeFilter charVirtShapeFilter;


static void charVirtualUpdate(BodyInterface &bi, PhysCharacter *ch) {
	CharacterVirtual *c = static_cast<CharacterVirtual *>(ch->joltCharacter);
	CharacterVirtual::ExtendedUpdateSettings settings;
	settings.mStickToFloorStepDown = RVec3{ 0, 0, -0.5f };
	settings.mWalkStairsStepUp = RVec3{ 0, 0, 0.4f };
	c->ExtendedUpdate(1.0f / 60, RVec3{ 0, 0, -9.8f }, settings, charVirtBPLayerFilter, charVirtObjLayerFilter, charVirtBodyFilter, charVirtShapeFilter, *tempAllocator);
	
	switch (c->GetGroundState()) {
	case Character::EGroundState::OnGround:
		ch->groundState = PHYS_CHAR_SUPPORTED;
		break;
	case Character::EGroundState::OnSteepGround:
		ch->groundState = PHYS_CHAR_TOO_STEEP;
		break;
	case Character::EGroundState::NotSupported:
		ch->groundState = PHYS_CHAR_UNSUPPORTED;
		break;
	case Character::EGroundState::InAir:
		ch->groundState = PHYS_CHAR_IN_AIR;
		break;
	}

	if (ch->groundState != PHYS_CHAR_IN_AIR) {
		JPH::Vec3 gn = c->GetGroundNormal();
		ch->groundNormX = gn.GetX();
		ch->groundNormY = gn.GetY();
		ch->groundNormZ = gn.GetZ();
	}
}

void joltCharacterUpdate(BodyInterface &bi) {
	for (auto ch = PHYS_CHARACTERS.begin(); ch != PHYS_CHARACTERS.end(); ++ch) {
		if (ch->isVirtual) {
			charVirtualUpdate(bi, ch.ptr());
		} else {
			charNormalUpdate(bi, ch.ptr());
		}

		float pos[3];
		physCharacterGetPosition(ch.ptr(), pos);
		struct Transform *tf = getTf(ch->entity);
		tf->x = pos[0];
		tf->y = pos[1];
		tf->z = pos[2];
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