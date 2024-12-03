#include "jolt.h"

// All Jolt symbols are in the JPH namespace
using namespace JPH;

void physNewCharacter(struct PhysCharacter *ch, float radius, float halfHeight, float friction) {
	Transform *tf = getTf(ch->entity);
	// Create 'player' character
	Ref<CharacterSettings> settings = new CharacterSettings();
	RefConst<Shape> shape = RotatedTranslatedShapeSettings(JPH::Vec3(0, 0, halfHeight + radius), Quat::sEulerAngles(JPH::Vec3{ DEG2RAD(-90), 0, 0 }), new CapsuleShape(halfHeight, radius)).Create().Get();
	settings->mMaxSlopeAngle = DEG2RAD(45.0f);
	settings->mLayer = PHYS_LAYER_CHAR_HITBOX;
	settings->mShape = shape;
	settings->mFriction = friction;
	settings->mMass = 60;
	settings->mSupportingVolume = Plane(JPH::Vec3::sAxisZ(), -radius); // Accept contacts that touch the lower sphere of the capsule
	Character *character = new Character(settings, RVec3{ tf->x, tf->y, tf->z }, Quat{ tf->rx, tf->ry, tf->rz, tf->rw }, ch->entity, physicsSystem);
	character->SetUp(JPH::Vec3(0, 0, 1.0f));
	character->AddToPhysicsSystem(EActivation::Activate);
	ch->joltCharacter = character;
	ch->isVirtual = false;
	ch->enable = true;
}

void physNewCharacterVirtual(struct PhysCharacter *ch, float radius, float halfHeight) {
	Transform *tf = getTf(ch->entity);
	Ref<CharacterVirtualSettings> settings = new CharacterVirtualSettings();
	RefConst<Shape> shape = RotatedTranslatedShapeSettings(JPH::Vec3(0, 0, halfHeight + radius), Quat::sEulerAngles(JPH::Vec3{ DEG2RAD(-90), 0, 0 }), new CapsuleShape(halfHeight, radius)).Create().Get();
	settings->mMaxSlopeAngle = DEG2RAD(45.0f);
	settings->mShape = shape;
	settings->mMass = 60;
	settings->mSupportingVolume = Plane(JPH::Vec3::sAxisZ(), -radius); // Accept contacts that touch the lower sphere of the capsule
	settings->mUp = RVec3{ 0, 0, 1 };
	settings->mBackFaceMode = EBackFaceMode::IgnoreBackFaces;
	CharacterVirtual *c = new CharacterVirtual(settings, RVec3{ tf->x, tf->y, tf->z }, Quat{ tf->rx, tf->ry, tf->rz, tf->rw }, ch->entity, physicsSystem);
	ch->joltCharacter = c;
	ch->isVirtual = true;
	ch->enable = true;
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

void physCharacterSetVelocity(struct PhysCharacter *ch, float vx, float vy, float vz) {
	JPH::Vec3 v{ vx, vy, vz };
	if (ch->isVirtual) {
		CharacterVirtual *c = static_cast<CharacterVirtual *>(ch->joltCharacter);
		static_cast<CharacterVirtual *>(ch->joltCharacter)->SetLinearVelocity(v);
	} else {
		static_cast<Character *>(ch->joltCharacter)->SetLinearVelocity(v);
	}
}

void physCharacterSetPosition(struct PhysCharacter *ch, float x, float y, float z) {
	JPH::Vec3 v{ x, y, z };
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
		ch->groundNorm.x = gn.GetX();
		ch->groundNorm.y = gn.GetY();
		ch->groundNorm.z = gn.GetZ();
	}

	JPH::Vec3 vel = jch->GetLinearVelocity();
	ch->vel.x = vel.GetX();
	ch->vel.y = vel.GetY();
	ch->vel.z = vel.GetZ();

	Transform *tf = getTf(ch->entity);
	JPH::Vec3 pos = jch->GetPosition();
	tf->x = pos.GetX();
	tf->y = pos.GetY();
	tf->z = pos.GetZ();
}


class CharVirtBPLayerFilter : public JPH::BroadPhaseLayerFilter {
	virtual bool ShouldCollide([[maybe_unused]] BroadPhaseLayer inLayer) const
	{
		return inLayer == BroadPhaseLayers::NON_MOVING || inLayer == BroadPhaseLayers::MOVING;
	}
};
CharVirtBPLayerFilter charVirtBPLayerFilter;

class CharVirtObjLayerFilter : public JPH::ObjectLayerFilter {
	virtual bool ShouldCollide([[maybe_unused]] ObjectLayer inLayer) const
	{
		switch (inLayer) {
		case PHYS_LAYER_STATIC:
		case PHYS_LAYER_MOVING:
		case PHYS_LAYER_CHAR_HITBOX:
			return true;

		default:
		case PHYS_LAYER_CHAR_HURTBOX:
		case PHYS_LAYER_WEAPON:
		case PHYS_LAYER_DEBRIS:
			return false;
		}
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
	Transform *tf = getTf(ch->entity);

	CharacterVirtual *c = static_cast<CharacterVirtual *>(ch->joltCharacter);
	CharacterVirtual::ExtendedUpdateSettings settings;
	settings.mStickToFloorStepDown = RVec3{ 0, 0, -0.5f };
	settings.mWalkStairsStepUp = RVec3{ 0, 0, 0.6f };
	//settings.mWalkStairsMinStepForward = 0.1f;
	//settings.mWalkStairsStepForwardTest = 0.3f;
	c->ExtendedUpdate(PHYS_DELTATIME, RVec3{ 0, 0, -9.8f }, settings, charVirtBPLayerFilter, charVirtObjLayerFilter, charVirtBodyFilter, charVirtShapeFilter, *tempAllocator);
	
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
		ch->groundNorm.x = gn.GetX();
		ch->groundNorm.y = gn.GetY();
		ch->groundNorm.z = gn.GetZ();
	}

	JPH::Vec3 pos = c->GetPosition();
	Vec pos2 = {
		pos.GetX() - tf->x,
		pos.GetY() - tf->y,
		pos.GetZ() - tf->z
	};
	vecMulS(&ch->vel, &pos2, PHYS_TICKRATE);
	JPH::Vec3 vel = c->GetGroundVelocity();
	ch->groundVel.x = vel.GetX();
	ch->groundVel.y = vel.GetY();
	ch->groundVel.z = vel.GetZ();
	tf->x = pos.GetX();
	tf->y = pos.GetY();
	tf->z = pos.GetZ();
}

void joltCharacterUpdatePre(JPH::BodyInterface &bi) {
	for (auto ch = PHYS_CHARACTERS.begin(); ch != PHYS_CHARACTERS.end(); ++ch) {
		if (ch->isVirtual && ch->enable) {
			charVirtualUpdate(bi, ch.ptr());
		}
	}
}

void joltCharacterUpdatePost(BodyInterface &bi) {
	for (auto ch = PHYS_CHARACTERS.begin(); ch != PHYS_CHARACTERS.end(); ++ch) {
		if (ch->isVirtual) {
			//charVirtualUpdate(bi, ch.ptr());
		} else if (ch->enable) {
			charNormalUpdate(bi, ch.ptr());
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