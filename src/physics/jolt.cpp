#include "jolt.h"
#include <physics/3d.h>
#include <assets.h>
#include <basics.h>
#include <gfx/draw.h> // for model files and debug render
#include <events.h>

#define DRAW_PHYS_DEBUG 499

using namespace JPH;

static const uint32_t layerCollides[PHYS_LAYER_N] = {
	0x36, // 110110
	0x17, // 010111
	0x07, // 000111
	0x10, // 010000
	0x0D, // 001011
	0x01  // 000001
};

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

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:
	virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override {
		return layerCollides[inObject1] & (1 << inObject2);
	}
};

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
	BPLayerInterfaceImpl() {
		// Create a mapping table from object to broad phase layer
		mObjectToBroadPhase[PHYS_LAYER_STATIC] = BroadPhaseLayers::NON_MOVING;
		mObjectToBroadPhase[PHYS_LAYER_MOVING] = BroadPhaseLayers::MOVING;
		mObjectToBroadPhase[PHYS_LAYER_CHAR_HITBOX] = BroadPhaseLayers::MOVING;
		mObjectToBroadPhase[PHYS_LAYER_CHAR_HURTBOX] = BroadPhaseLayers::MOVING;
		mObjectToBroadPhase[PHYS_LAYER_WEAPON] = BroadPhaseLayers::MOVING;
		mObjectToBroadPhase[PHYS_LAYER_DEBRIS] = BroadPhaseLayers::DEBRIS;
	}

	virtual uint GetNumBroadPhaseLayers() const override {
		return BroadPhaseLayers::NUM_LAYERS;
	}

	virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override {
		JPH_ASSERT(inLayer < PHYS_LAYER_N);
		return mObjectToBroadPhase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char *GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override {
		switch ((BroadPhaseLayer::Type)inLayer) {
		case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
		case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
		case (BroadPhaseLayer::Type)BroadPhaseLayers::DEBRIS:		return "DEBRIS";
		default:													JPH_ASSERT(false); return "INVALID";
		}
	}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
	BroadPhaseLayer					mObjectToBroadPhase[PHYS_LAYER_N];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
public:
	virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
		switch (inLayer2.GetValue()) {
		case 0:
			return 0x36 & (1 << inLayer1);
		case 1:
			return inLayer1 != PHYS_LAYER_STATIC && inLayer1 != PHYS_LAYER_DEBRIS;
		case 2:
			return inLayer1 == PHYS_LAYER_STATIC;
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
		PhysBody *a = &PHYS_BODIES[inBody1.GetUserData()];
		PhysBody *b = &PHYS_BODIES[inBody2.GetUserData()];
		if (a->collFuncs && a->collFuncs->onAdded)
			a->collFuncs->onAdded(a, b, NULL);
		if (b->collFuncs && b->collFuncs->onAdded)
			b->collFuncs->onAdded(b, a, NULL);
	}

	virtual void OnContactPersisted(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override {
		PhysBody *a = &PHYS_BODIES[inBody1.GetUserData()];
		PhysBody *b = &PHYS_BODIES[inBody2.GetUserData()];
		if (a->collFuncs && a->collFuncs->onPersisted)
			a->collFuncs->onPersisted(a, b, NULL);
		if (b->collFuncs && b->collFuncs->onPersisted)
			b->collFuncs->onPersisted(b, a, NULL);
	}

	virtual void OnContactRemoved(const SubShapeIDPair &inSubShapePair) override {
		//logDebug("[JOLT] Contact Removed\n");
	}
};

// An example activation listener
class MyBodyActivationListener : public BodyActivationListener {
public:
	virtual void OnBodyActivated(const BodyID &inBodyID, uint64 inBodyUserData) override {
		//logDebug("[JOLT] Body Activated\n");
	}

	virtual void OnBodyDeactivated(const BodyID &inBodyID, uint64 inBodyUserData) override {
		//logDebug("[JOLT] Body Deactivated\n");
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
		Initialize();
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
		settings.mDrawShapeWireframe = true;
		physicsSystem->DrawBodies(settings, r);
	}

private:
	struct Texture *tex;
};
MyDebugRenderer *renderer;
#endif

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
	
	joltBodyUpdatePre(bi);
	joltCharacterUpdatePre(bi);

	const int cCollisionSteps = 1;
	// Step the world
	physicsSystem->OptimizeBroadPhase();
	physicsSystem->Update(PHYS_DELTATIME, cCollisionSteps, tempAllocator, jobSystem);

	joltBodyUpdatePost(bi);
	joltCharacterUpdatePost(bi);
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
	joltBodyInit();
	joltCharacterInit();

#ifndef RELEASE
	// Setup debug renderer
	renderer = new MyDebugRenderer();
#endif

	logDebug("[JOLT] Initialized\n");
}

void joltFini(void) {
	joltBodyFini();
	joltCharacterFini();

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