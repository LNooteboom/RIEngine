#include "jolt.h"
#include <physics/3d.h>
#include <assets.h>
#include <basics.h>
#include <gfx/draw.h> // for model files and debug render
#include <events.h>

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
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

// STL includes
#include <iostream>
#include <cstdarg>
#include <thread>

// Disable common warnings triggered by Jolt, you can use JPH_SUPPRESS_WARNING_PUSH / JPH_SUPPRESS_WARNING_POP to store and restore the warning state
JPH_SUPPRESS_WARNINGS

// All Jolt symbols are in the JPH namespace
using namespace JPH;
// If you want your code to compile using single or double precision write 0.0_r to get a Real value that compiles to double or float depending if JPH_DOUBLE_PRECISION is set or not.
using namespace JPH::literals;

#define DRAW_PHYS_DEBUG 499

namespace physics {

// Callback for traces, connect this to your own trace function if you have one
static void TraceImpl(const char *inFMT, ...) {
	// Format the message
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	// Print to the TTY
	logNorm("[JOLT] %s", buffer);
}

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint inLine) {
	// Print to the TTY
	logNorm("[JOLT] Assert failed at (%s:%d) (%s) %s\n", inFile, inLine, inExpression, inMessage ? inMessage : "");

	// Breakpoint
	return true;
};

#endif // JPH_ENABLE_ASSERTS

// Layer that objects can be in, determines which other objects it can collide with
// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
// but only if you do collision testing).
namespace Layers {
	static constexpr ObjectLayer NON_MOVING = 0;
	static constexpr ObjectLayer MOVING = 1;
	static constexpr ObjectLayer NUM_LAYERS = 2;
};

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:
	virtual bool					ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override {
		switch (inObject1) {
		case Layers::NON_MOVING:
			return inObject2 == Layers::MOVING; // Non moving only collides with moving
		case Layers::MOVING:
			return true; // Moving collides with everything
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
namespace BroadPhaseLayers {
	static constexpr BroadPhaseLayer NON_MOVING(0);
	static constexpr BroadPhaseLayer MOVING(1);
	static constexpr uint NUM_LAYERS(2);
};

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
	BPLayerInterfaceImpl() {
		// Create a mapping table from object to broad phase layer
		mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
		mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
	}

	virtual uint					GetNumBroadPhaseLayers() const override {
		return BroadPhaseLayers::NUM_LAYERS;
	}

	virtual BroadPhaseLayer			GetBroadPhaseLayer(ObjectLayer inLayer) const override {
		JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
		return mObjectToBroadPhase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char *GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override {
		switch ((BroadPhaseLayer::Type)inLayer) {
		case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
		case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
		default:													JPH_ASSERT(false); return "INVALID";
		}
	}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
	BroadPhaseLayer					mObjectToBroadPhase[Layers::NUM_LAYERS];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
public:
	virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
		switch (inLayer1) {
		case Layers::NON_MOVING:
			return inLayer2 == BroadPhaseLayers::MOVING;
		case Layers::MOVING:
			return true;
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

// An example contact listener
class MyContactListener : public ContactListener {
public:
	// See: ContactListener
	virtual ValidateResult	OnContactValidate(const Body &inBody1, const Body &inBody2, RVec3Arg inBaseOffset, const CollideShapeResult &inCollisionResult) override {
		//logDebug("[JOLT] Contact Validate\n");

		// Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
		return ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	virtual void OnContactAdded(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override {
		//logDebug("[JOLT] Contact Added\n");
	}

	virtual void OnContactPersisted(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override {
		//logDebug("[JOLT] Contact Persisted\n");
	}

	virtual void OnContactRemoved(const SubShapeIDPair &inSubShapePair) override {
		//logDebug("[JOLT] Contact Removed\n");
	}
};

// An example activation listener
class MyBodyActivationListener : public BodyActivationListener {
public:
	virtual void OnBodyActivated(const BodyID &inBodyID, uint64 inBodyUserData) override {
		logDebug("[JOLT] Body Activated\n");
	}

	virtual void OnBodyDeactivated(const BodyID &inBodyID, uint64 inBodyUserData) override {
		logDebug("[JOLT] Body Deactivated\n");
	}
};

TempAllocatorImpl *tempAllocator;
PhysicsSystem *physicsSystem;
JobSystemThreadPool *jobSystem;
BPLayerInterfaceImpl *bpLayerInterfaceImpl;
ObjectVsBroadPhaseLayerFilterImpl *objectVsBroadPhaseLayerFilterImpl;
ObjectLayerPairFilterImpl *objectLayerPairFilterImpl;
MyBodyActivationListener *myBodyActivationListener;
MyContactListener *myContactListener;

#ifndef RELEASE

struct MyVertex {
	float x, y, z;
	float u, v;
	float r, g, b, a;
};
class MyTriangleBatch : public RefTargetVirtual {
public:
	virtual ~MyTriangleBatch() {
		if (vertices)
			globalDealloc((void *)vertices);
		if (indices)
			globalDealloc((void *)indices);
	}

	virtual void AddRef() override { ++refCount; }
	virtual void Release() override { if (--refCount == 0) delete this; }

	MyVertex *vertices;
	int nVertices;
	uint32_t *indices;
	int nIndices;
	int refCount;
};

class MyDebugRenderer : public DebugRenderer {
public:
	MyDebugRenderer() {
		tex = loadTexture("tex/White.png");
	}

	virtual void DrawLine(RVec3Arg inFrom, RVec3Arg inTo, ColorArg inColor) {

	}

	virtual void DrawTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3, ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off) {
		drawPreflush(3, 3);
		drawMatIdentity();
		drawTexture(0, tex);

		drawVertex(inV1.GetX(), inV1.GetY(), inV1.GetZ(), 0, 0, inColor.r, inColor.g, inColor.b, 1);
		drawVertex(inV2.GetX(), inV2.GetY(), inV2.GetZ(), 0, 0, inColor.r, inColor.g, inColor.b, 1);
		drawVertex(inV3.GetX(), inV3.GetY(), inV3.GetZ(), 0, 0, inColor.r, inColor.g, inColor.b, 1);
		unsigned int idx[3] = { 0, 1, 2 };
		drawIndices(3, 3, idx);
	}
	virtual Batch CreateTriangleBatch(const Triangle *inTriangles, int inTriangleCount) {
		int nVerts = inTriangleCount * 3;
		Vertex *verts = (Vertex *)stackAlloc(nVerts * sizeof(Vertex));
		int nIdx = nVerts;
		uint32_t *idx = (uint32_t *)stackAlloc(nIdx * sizeof(uint32_t));
		for (int i = 0; i < inTriangleCount; i++) {
			verts[i * 3 + 0] = inTriangles[i].mV[0];
			verts[i * 3 + 1] = inTriangles[i].mV[1];
			verts[i * 3 + 2] = inTriangles[i].mV[2];

			idx[i * 3 + 0] = i * 3 + 0;
			idx[i * 3 + 1] = i * 3 + 1;
			idx[i * 3 + 2] = i * 3 + 2;
		}
		return CreateTriangleBatch(verts, nVerts, idx, nIdx);
	}
	virtual Batch CreateTriangleBatch(const Vertex *inVertices, int inVertexCount, const uint32 *inIndices, int inIndexCount) {
		MyTriangleBatch *batch = new MyTriangleBatch();
		batch->vertices = (MyVertex *)globalAlloc(inVertexCount * sizeof(MyVertex));
		batch->nVertices = inVertexCount;
		for (int i = 0; i < inVertexCount; i++) {
			MyVertex *v = &batch->vertices[i];
			v->x = inVertices[i].mPosition.x;
			v->y = inVertices[i].mPosition.y;
			v->z = inVertices[i].mPosition.z;
			v->u = inVertices[i].mUV.x;
			v->v = inVertices[i].mUV.y;
			v->r = inVertices[i].mColor.r;
			v->g = inVertices[i].mColor.g;
			v->b = inVertices[i].mColor.b;
			v->a = inVertices[i].mColor.a;
		}
		batch->indices = (uint32_t *)globalAlloc(inIndexCount * sizeof(uint32_t));
		batch->nIndices = inIndexCount;
		for (int i = 0; i < inIndexCount; i++) {
			batch->indices[i] = inIndices[i];
		}
		return batch;
	}
	virtual void DrawGeometry(RMat44Arg inModelMatrix, const AABox &inWorldSpaceBounds, float inLODScaleSq, ColorArg inModelColor, const GeometryRef &inGeometry,
		ECullMode inCullMode = ECullMode::CullBackFace, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid) {
		if (!inGeometry.GetPtr())
			return;

		MyTriangleBatch *b = static_cast<MyTriangleBatch *>(inGeometry->mLODs[0].mTriangleBatch.GetPtr());
		//drawPreflush(b->nVertices, b->nIndices);
		drawFlush();
		drawShaderUseStd(SHADER_3D);
		drawWireframe(true);
		float mat[16];
		for (int i = 0; i < 16; i++) {
			mat[i] = inModelMatrix(i % 4, i / 4);
		}
		drawSetMatrix(mat);
		drawTexture(0, tex);
		for (int i = 0; i < b->nVertices; i++) {
			MyVertex *v = &b->vertices[i];
			drawVertex(v->x, v->y, v->z, v->u, v->v, v->r * inModelColor.r, v->g * inModelColor.g, v->b * inModelColor.b, v->a * inModelColor.a);
		}
		drawIndices(b->nVertices, b->nIndices, b->indices);
	}
	virtual void DrawText3D(RVec3Arg inPosition, const string_view &inString, ColorArg inColor = Color::sWhite, float inHeight = 0.5f) {
		// TODO
	}

	static void render(void *arg) {
		MyDebugRenderer *r = static_cast<MyDebugRenderer *>(arg);
		drawReset();
		BodyManager::DrawSettings settings;
		physicsSystem->DrawBodies(settings, r);
	}

private:
	struct Texture *tex;
};
MyDebugRenderer *renderer;
#endif

inline PhysBody *getBody(entity_t entity) {
	return &PHYS_BODIES[entity];
}
inline PhysBody *addBody(entity_t entity) {
	return &PHYS_BODIES.add(entity);
}
inline Transform *getTf(entity_t entity) {
	//return static_cast<Transform *>(getComponent(TRANSFORM, entity));
	return &TRANSFORMS[entity];
}
inline BodyID getJBody(PhysBody *pb) {
	return BodyID{ pb->joltBody };
}
inline entity_t getEntity(Body *b) {
	return static_cast<entity_t>(b->GetUserData());
}
void newBody(struct PhysBody *b, Ref<Shape> ss) {
	if (b->flags & PHYS_HAS_BODY) {
		physDeleteBody(b);
	}

	struct Transform *tf = getTf(b->entity);
	EMotionType motionType;
	ObjectLayer objectLayer;
	if (b->flags & PHYS_KINEMATIC) {
		motionType = EMotionType::Kinematic;
		objectLayer = Layers::MOVING;
	} else if (b->flags & PHYS_MOVING) {
		motionType = EMotionType::Dynamic;
		objectLayer = Layers::MOVING;
	} else {
		motionType = EMotionType::Static;
		objectLayer = Layers::NON_MOVING;
	}
	BodyCreationSettings bcs{ ss, RVec3{tf->x, tf->y, tf->z}, Quat{tf->rx, tf->ry, tf->rz, tf->rw}, motionType, objectLayer };
	Body *body{ physicsSystem->GetBodyInterface().CreateBody(bcs) };
	body->SetUserData(b->entity);
	b->joltBody = body->GetID().GetIndexAndSequenceNumber();
	b->flags |= PHYS_HAS_BODY;
}


void physBodyNotifier(void *arg, void *component, int type) {
	if (type == NOTIFY_DELETE) {
		PhysBody *b = static_cast<PhysBody *>(component);
		if (b->flags & PHYS_HAS_BODY) {
			physicsSystem->GetBodyInterface().DestroyBody(getJBody(b));
		}
	} else if (type == NOTIFY_PURGE) {
		int maxBodies = clCount(PHYS_BODY);
		BodyInterface &bi = physicsSystem->GetBodyInterface();
		if (maxBodies) {
			BodyID *bodies = new BodyID[maxBodies];
			BodyID *activeBodies = new BodyID[maxBodies];
			int bodiesCnt = 0;
			int activeCnt = 0;
			for (PhysBody *b = static_cast<PhysBody *>(clBegin(PHYS_BODY)); b; b = static_cast<PhysBody *>(clNext(PHYS_BODY, b))) {
				if (b->flags & PHYS_HAS_BODY) {
					BodyID id{ b->joltBody };
					bodies[bodiesCnt] = id;
					++bodiesCnt;

					if (bi.IsAdded(id)) {
						activeBodies[activeCnt] = id;
						++activeCnt;
					}
				}
			}

			if (activeCnt) {
				bi.RemoveBodies(activeBodies, activeCnt);
			}
			if (bodiesCnt) {
				bi.DestroyBodies(bodies, bodiesCnt);
			}
			delete activeBodies;
			delete bodies;
		}
	}
}
void physCharacterNotifier(void *arg, void *component, int type) {
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

} // Namespace physics
using namespace physics;


// C Interface


void physNewBodyBox(struct PhysBody *b, float *halfSize, struct PhysMaterial *mat) {
	BoxShapeSettings ss{ RVec3{halfSize[0], halfSize[1], halfSize[2]} };
	newBody(b, ss.Create().Get());
}

void physNewBodyMesh(struct PhysBody *b, const char *meshFile, const char *meshName) {
	static const char *mfCorrupt = "Collision mesh file is corrupted: %s\n";
	struct Asset *a = assetOpen(meshFile);
	if (!a) {
		fail("Failed to open coll mesh file %s\n", meshFile);
		return;
	}

	struct ModelFileHeader header;
	if (assetRead(a, &header, sizeof(header)) != sizeof(header)) {
		fail(mfCorrupt, meshFile);
		return;
	}
	if (memcmp(&header.sig, "MES0", 4) || header.nEntries == 0) {
		fail(mfCorrupt, meshFile);
		return;
	}

	struct ModelFileEntry mfe;
	size_t vPitch, vSize, iSize;
	bool found = false;
	for (unsigned int i = 0; i < header.nEntries; i++) {
		if (assetRead(a, &mfe, sizeof(mfe)) != sizeof(mfe)) {
			fail(mfCorrupt, meshFile);
			return;
		}
		vPitch = mfe.flags & MODEL_FILE_ANIM ? 52 : 48;
		vSize = vPitch * mfe.nVertices;
		iSize = mfe.nTriangles * 3 * 4;
		if (!strcmp(meshName, mfe.name)) {
			found = true;
			break;
		}
		assetSeek(a, (long)(vSize + iSize), ASSET_CUR);
	}
	if (!found) {
		logNorm("Could not find coll mesh %s in %s\n", meshName, meshFile);
		return;
	}

	VertexList verts;
	for (unsigned int i = 0; i < mfe.nVertices; i++) {
		float cv[3];
		assetRead(a, cv, sizeof(cv));
		assetSeek(a, (long)(vPitch - sizeof(cv)), ASSET_CUR);
		verts.push_back({ cv[0], cv[1], cv[2] });
	}
	IndexedTriangleList indices;
	for (unsigned int i = 0; i < mfe.nTriangles; i++) {
		uint32_t idx[3];
		assetRead(a, idx, sizeof(idx));
		indices.push_back({ idx[0], idx[1], idx[2], 0 });
	}

	assetClose(a);

	MeshShapeSettings ms{ verts, indices};
	ms.Sanitize();
	newBody(b, ms.Create().Get());
}
void physNewBodySphere(struct PhysBody *b, float radius, struct PhysMaterial *mat) {
	SphereShapeSettings set{ radius };
	newBody(b, set.Create().Get());
}
void physNewBodyCapsule(struct PhysBody *b, float halfZ, float radius, struct PhysMaterial *mat) {
	CapsuleShapeSettings set{ halfZ, radius };
	newBody(b, set.Create().Get());
}
void physNewBodyCylinder(struct PhysBody *b, float halfZ, float radius, struct PhysMaterial *mat) {
	CylinderShapeSettings set{ halfZ, radius };
	newBody(b, set.Create().Get());
}

void physDeleteBody(struct PhysBody *b) {
	physicsSystem->GetBodyInterface().DestroyBody(getJBody(b));
	b->flags &= ~(PHYS_HAS_BODY);
}
void physAddBody(struct PhysBody *b) {
	physicsSystem->GetBodyInterface().AddBody(getJBody(b), EActivation::Activate);
}
void physRemoveBody(struct PhysBody *b) {
	physicsSystem->GetBodyInterface().RemoveBody(getJBody(b));
}


// Character
void physNewCharacter(struct PhysCharacter *ch, float radius, float halfHeight, float friction) {
	Transform *tf = getTf(ch->entity);
	// Create 'player' character
	Ref<CharacterSettings> settings = new CharacterSettings();
	RefConst<Shape> shape = RotatedTranslatedShapeSettings(JPH::Vec3(0, 0, halfHeight + radius), Quat::sEulerAngles(JPH::Vec3{DEG2RAD(-90), 0, 0}), new CapsuleShape(halfHeight, radius)).Create().Get();
	settings->mMaxSlopeAngle = DEG2RAD(45.0f);
	settings->mLayer = Layers::MOVING;
	settings->mShape = shape;
	settings->mFriction = friction;
	settings->mMass = 60;
	settings->mSupportingVolume = Plane(JPH::Vec3::sAxisZ(), -radius); // Accept contacts that touch the lower sphere of the capsule
	Character *character = new Character(settings, RVec3{tf->x, tf->y, tf->z}, Quat{tf->rx, tf->ry, tf->rz, tf->rw}, ch->entity, physicsSystem);
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

#ifndef RELEASE
void physEnableDebugRender() {
	addDrawUpdate(DRAW_PHYS_DEBUG, MyDebugRenderer::render, renderer);
}
void physDisableDebugRender() {
	removeDrawUpdate(DRAW_PHYS_DEBUG);
}
#endif

void joltUpdate(void) {
	BodyInterface &bi = physicsSystem->GetBodyInterface();
	for (auto b = PHYS_BODIES.begin(); b != PHYS_BODIES.end(); ++b) {
		if (b->flags & PHYS_SYNC_FROM_TF) {
			Transform *tf = getTf(b->entity);
			bi.SetPositionAndRotation(getJBody(b.ptr()), RVec3{ tf->x, tf->y, tf->z }, Quat{ tf->rx, tf->ry, tf->rz, tf->rw }, EActivation::DontActivate);
		}
	}
	for (PhysBody *b = static_cast<PhysBody *>(clBegin(PHYS_BODY)); b; b = static_cast<PhysBody *>(clNext(PHYS_BODY, b))) {
		
	}

	const int cCollisionSteps = 1;
	const float cDeltaTime = 1.0f / 60.0f;
	// Step the world
	physicsSystem->OptimizeBroadPhase();
	physicsSystem->Update(cDeltaTime, cCollisionSteps, tempAllocator, jobSystem);

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

	for (auto ch = PHYS_CHARACTERS.begin(); ch != PHYS_CHARACTERS.end(); ++ch) {
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
}

void joltInit(void) {
	// Register allocation hook
	RegisterDefaultAllocator();

	// Install callbacks
	Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

	// Create a factory
	Factory::sInstance = new Factory();

	// Register all Jolt physics types
	RegisterTypes();

	// We need a temp allocator for temporary allocations during the physics update. We're
	// pre-allocating 10 MB to avoid having to do allocations during the physics update. 
	// B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
	// If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
	// malloc / free.
	tempAllocator = new TempAllocatorImpl(10 * 1024 * 1024);

	// We need a job system that will execute physics jobs on multiple threads. Typically
	// you would implement the JobSystem interface yourself and let Jolt Physics run on top
	// of your own job scheduler. JobSystemThreadPool is an example implementation.
	jobSystem = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

	// This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
	// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
	const uint cMaxBodies = 1024;

	// This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
	const uint cNumBodyMutexes = 0;

	// This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
	// body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
	// too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
	// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
	const uint cMaxBodyPairs = 1024;

	// This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
	// number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
	// Note: This value is low because this is a simple test. For a real project use something in the order of 10240.
	const uint cMaxContactConstraints = 1024;

	// Create mapping table from object layer to broadphase layer
	// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
	bpLayerInterfaceImpl = new BPLayerInterfaceImpl();

	// Create class that filters object vs broadphase layers
	// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
	objectVsBroadPhaseLayerFilterImpl = new ObjectVsBroadPhaseLayerFilterImpl();

	// Create class that filters object vs object layers
	// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
	objectLayerPairFilterImpl = new ObjectLayerPairFilterImpl();

	// Now we can create the actual physics system.
	physicsSystem = new PhysicsSystem();
	physicsSystem->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, *bpLayerInterfaceImpl, *objectVsBroadPhaseLayerFilterImpl, *objectLayerPairFilterImpl);

	// A body activation listener gets notified when bodies activate and go to sleep
	// Note that this is called from a job so whatever you do here needs to be thread safe.
	// Registering one is entirely optional.
	myBodyActivationListener = new MyBodyActivationListener();
	physicsSystem->SetBodyActivationListener(myBodyActivationListener);

	// A contact listener gets notified when bodies (are about to) collide, and when they separate again.
	// Note that this is called from a job so whatever you do here needs to be thread safe.
	// Registering one is entirely optional.
	myContactListener = new MyContactListener();
	physicsSystem->SetContactListener(myContactListener);


	// Set gravity in Z direction
	physicsSystem->SetGravity(RVec3{ 0, 0, -9.8f });

	// Initialize CL
	componentListInitSz(PHYS_BODY, sizeof(PhysBody));
	setNotifier(PHYS_BODY, physBodyNotifier, nullptr);
	componentListInitSz(PHYS_CHARACTER, sizeof(PhysCharacter));
	setNotifier(PHYS_CHARACTER, physCharacterNotifier, nullptr);

#ifndef RELEASE
	// Setup debug renderer
	renderer = new MyDebugRenderer();
#endif

	logDebug("[JOLT] Initialized\n");
}

void joltFini(void) {
	componentListFini(PHYS_CHARACTER);
	componentListFini(PHYS_BODY);

	// Unregisters all types with the factory and cleans up the default material
	UnregisterTypes();

	// Destroy the factory
	delete Factory::sInstance;
	Factory::sInstance = nullptr;

	delete physicsSystem;
	delete jobSystem;
	delete tempAllocator;
	delete bpLayerInterfaceImpl;
	delete objectVsBroadPhaseLayerFilterImpl;
	delete objectLayerPairFilterImpl;
	delete myBodyActivationListener;
	delete myContactListener;
}