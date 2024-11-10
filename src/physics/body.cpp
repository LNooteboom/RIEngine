#include "jolt.h"
#include <gfx/draw.h>

using namespace JPH;

static void newBody(struct PhysBody *b, Ref<Shape> ss) {
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

	MeshShapeSettings ms{ verts, indices };
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


void joltBodyUpdatePre(BodyInterface &bi) {
	for (auto b = PHYS_BODIES.begin(); b != PHYS_BODIES.end(); ++b) {
		if (b->flags & PHYS_SYNC_FROM_TF) {
			Transform *tf = getTf(b->entity);
			bi.SetPositionAndRotation(getJBody(b.ptr()), RVec3{ tf->x, tf->y, tf->z }, Quat{ tf->rx, tf->ry, tf->rz, tf->rw }, EActivation::DontActivate);
		}
	}
}

void joltBodyUpdatePost(BodyInterface &bi) {
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


static void physBodyNotifier(void *arg, void *component, int type) {
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

void joltBodyInit(void) {
	componentListInitSz(PHYS_BODY, sizeof(PhysBody));
	setNotifier(PHYS_BODY, physBodyNotifier, nullptr);
}

void joltBodyFini(void) {
	componentListFini(PHYS_BODY);
}