#include "jolt.h"
#include <gfx/draw.h>

using namespace JPH;

static void newBody(struct PhysBody *b, Ref<Shape> ss) {
	if (b->flags & PHYS_FLAG_HAS_BODY) {
		physDeleteBody(b);
	}

	struct Transform *tf = getTf(b->entity);
	EMotionType motionType;
	if (b->flags & PHYS_FLAG_KINEMATIC) {
		motionType = EMotionType::Kinematic;
	} else if (b->flags & PHYS_FLAG_MOVING) {
		motionType = EMotionType::Dynamic;
	} else {
		motionType = EMotionType::Static;
	}
	BodyCreationSettings bcs{ ss, RVec3{tf->x, tf->y, tf->z}, Quat{tf->rx, tf->ry, tf->rz, tf->rw}, motionType, ObjectLayer(b->layer) };
	bcs.mUserData = b->entity;
	bcs.mMotionQuality = b->flags & PHYS_FLAG_CONTINUOUS_COLLISION ? EMotionQuality::LinearCast : EMotionQuality::Discrete;
	bcs.mIsSensor = b->flags & PHYS_FLAG_SENSOR ? true : false;

	Body *body{ physicsSystem->GetBodyInterface().CreateBody(bcs) };
	b->joltBody = body->GetID().GetIndexAndSequenceNumber();
	b->flags |= PHYS_FLAG_HAS_BODY;
}

void physNewBodyBox(struct PhysBody *b, const Vec *offset, const Vec *rotation, const Vec *halfSize, struct PhysMaterial *mat) {
	BoxShapeSettings ss{ RVec3{halfSize->x, halfSize->y, halfSize->z} };
	if (offset || rotation) {
		JPH::Vec3 off;
		if (offset)
			off.Set(offset->x, offset->y, offset->z);
		else
			off = JPH::Vec3::sZero();

		JPH::Quat rot;
		if (rotation)
			rot.Set(rotation->x, rotation->y, rotation->z, rotation->w);
		else
			rot = JPH::Quat::sIdentity();

		ss.SetEmbedded();
		RotatedTranslatedShapeSettings rtss{ off, rot, &ss };
		newBody(b, rtss.Create().Get());
	} else {
		newBody(b, ss.Create().Get());
	}
}

void physNewBodyMesh(struct PhysBody *b, const char *meshName) {
	struct Model *m = getModel(meshName);
	if (!m) {
		logNorm("Couldn't find physics mesh %s\n", meshName);
		return;
	}

	VertexList verts;
	IndexedTriangleList indices;
	for (uint32_t i = 0; i < m->nVertices; i++) {
		float *v = (float *)((char *)m->verts + i * m->pitch);
		verts.push_back({ v[0], v[1], v[2] });
	}
	for (uint32_t i = 0; i < m->nTriangles; i++) {
		uint32_t *v = &m->indices[i * 3];
		indices.push_back({ v[0], v[1], v[2], 0 });
	}

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
	b->flags &= ~(PHYS_FLAG_HAS_BODY);
}

void physAddBody(struct PhysBody *b) {
	physicsSystem->GetBodyInterface().AddBody(getJBody(b), EActivation::Activate);
}

void physRemoveBody(struct PhysBody *b) {
	physicsSystem->GetBodyInterface().RemoveBody(getJBody(b));
}

void physBodyUpdatePosition(struct PhysBody *b) {
	struct Transform *tf = getTf(b->entity);
	physicsSystem->GetBodyInterface().SetPositionAndRotation(getJBody(b), RVec3{ tf->x, tf->y, tf->z }, Quat{ tf->rx, tf->ry, tf->rz, tf->rw }, EActivation::Activate);
}

void physBodySetVelocity(struct PhysBody *b, const Vec *vel) {
	physicsSystem->GetBodyInterface().SetLinearVelocity(getJBody(b), { vel->x, vel->y, vel->z });
}


void joltBodyUpdatePre(BodyInterface &bi) {
	for (auto b = PHYS_BODIES.begin(); b != PHYS_BODIES.end(); ++b) {
		if (b->flags & PHYS_FLAG_SYNC_FROM_TF) {
			Transform *tf = getTf(b->entity);
			if (b->flags & PHYS_FLAG_KINEMATIC) {
				bi.MoveKinematic(getJBody(b.ptr()), RVec3{ tf->x, tf->y, tf->z }, Quat{ tf->rx, tf->ry, tf->rz, tf->rw }, PHYS_DELTATIME);
			} else {
				bi.SetPositionAndRotation(getJBody(b.ptr()), RVec3{ tf->x, tf->y, tf->z }, Quat{ tf->rx, tf->ry, tf->rz, tf->rw }, EActivation::DontActivate);
			}
		}
	}
}

void joltBodyUpdatePost(BodyInterface &bi) {
	for (auto b = PHYS_BODIES.begin(); b != PHYS_BODIES.end(); ++b) {
		if (b->flags & PHYS_FLAG_SYNC_TO_TF) {
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
		if (b->flags & PHYS_FLAG_HAS_BODY) {
			BodyInterface &bi = physicsSystem->GetBodyInterface();
			BodyID id = getJBody(b);
			bi.RemoveBody(id);
			bi.DestroyBody(id);
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
				if (b->flags & PHYS_FLAG_HAS_BODY) {
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