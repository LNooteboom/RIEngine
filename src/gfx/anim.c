#include <gfx/draw.h>
#include <events.h>
#include <assets.h>
#include <string.h>
#include <math.h>
#include <vec.h>
#include "gfx.h"

struct AnimUbo {
	Mat finalMats[DRAW_MAX_BONE];
};

struct LoadedPoseFile *loadPoseFile(const char *name) {
	logDebug("Loading pose file %s\n", name);
	struct Asset *a = assetOpen(name);
	if (!a) {
		fail("Pose file %s not found\n", name);
		return NULL;
	}
	struct LoadedPoseFile *lpf = globalAlloc(a->bufferSize);
	assetRead(a, lpf, a->bufferSize);

	assetClose(a);

	return lpf;
}
void deletePoseFile(struct LoadedPoseFile *lpf) {
	globalDealloc(lpf);
}

static struct PoseFileAnim *getAnim(struct PoseFileHeader *h, const char *animName) {
	struct PoseFileAnim *a = (struct PoseFileAnim *)((char *)(h + 1) + sizeof(struct PoseFileBone) * h->nBones);
	bool found = false;
	for (unsigned int i = 0; i < h->nAnim; i++) {
		if (!strcmp(a->name, animName)) {
			found = true;
			break;
		}
		a = ((struct PoseFileAnim *)((char *)a + a->size));
	}
	if (!found) {
		return NULL;
	}
	return a;
}

static void poseFileGet3(Vec *out, int n, struct PoseFileVec3 *v, float time) {
	if (time < v[0].t) {
		vecSet3(out, v[0].v[0], v[0].v[1], v[0].v[2]); /* First */
		return;
	}
	for (int i = 0; i < n - 1; i++) {
		if (time >= v[i].t && time < v[i + 1].t) {
			Vec v1, v2;
			vecSet3(&v1, v[i].v[0], v[i].v[1], v[i].v[2]);
			vecSet3(&v2, v[i + 1].v[0], v[i + 1].v[1], v[i + 1].v[2]);
			vecLerp(out, &v1, &v2, (time - v[i].t) / (v[i + 1].t - v[i].t));
			return;
		}
	}
	int last = n - 1;
	vecSet3(out, v[last].v[0], v[last].v[1], v[last].v[2]); /* Last */
}
static void poseFileGet4(Vec *out, int n, struct PoseFileVec4 *v, float time) {
	if (time < v[0].t) {
		vecSet4(out, v[0].v[0], v[0].v[1], v[0].v[2], v[0].v[3]); /* First */
		return;
	}
	for (int i = 0; i < n - 1; i++) {
		if (time >= v[i].t && time < v[i + 1].t) {
			Vec v1, v2;
			vecSet4(&v1, v[i].v[0], v[i].v[1], v[i].v[2], v[i].v[3]);
			vecSet4(&v2, v[i + 1].v[0], v[i + 1].v[1], v[i + 1].v[2], v[i + 1].v[3]);
			quatNlerp(out, &v1, &v2, (time - v[i].t) / (v[i + 1].t - v[i].t));
			return;
		}
	}
	int last = n - 1;
	vecSet4(out, v[last].v[0], v[last].v[1], v[last].v[2], v[last].v[3]); /* Last */
}

static void animUpdateState(struct Anim3DState *s, struct PoseFileAnim *a) {
	struct PoseFileHeader *h = &s->poseFile->hdr;
	struct PoseFileBone *b = (struct PoseFileBone *)(h + 1);

	if (!s->boneState || s->nBones != h->nBones) {
		s->nBones = h->nBones;
		s->boneState = globalRealloc(s->boneState, s->nBones * sizeof(struct Anim3DBoneState));
	}
	for (int i = 0; i < s->nBones; i++) {
		s->boneState[i].pos.x = b[i].trans[0];
		s->boneState[i].pos.y = b[i].trans[1];
		s->boneState[i].pos.z = b[i].trans[2];
		s->boneState[i].rot.x = b[i].rot[0];
		s->boneState[i].rot.y = b[i].rot[1];
		s->boneState[i].rot.z = b[i].rot[2];
		s->boneState[i].rot.w = b[i].rot[3];
		s->boneState[i].scale.x = b[i].scale[0];
		s->boneState[i].scale.y = b[i].scale[1];
		s->boneState[i].scale.z = b[i].scale[2];
	}

	float t = s->animTime / 60.0f;
	struct PoseFileAnimChannel *ch = (struct PoseFileAnimChannel *)(a + 1);
	for (unsigned int i = 0; i < a->nChannels; i++) {
		if (ch->bone < h->nBones) {
			struct PoseFileVec3 *v3 = (struct PoseFileVec3 *)(ch + 1);
			struct PoseFileVec4 *v4 = (struct PoseFileVec4 *)v3;
			switch (ch->type) {
			case POSE_TRANSLATE:
				poseFileGet3(&s->boneState[ch->bone].pos, ch->nEntries, v3, t);
				break;
			case POSE_ROTATE:
				poseFileGet4(&s->boneState[ch->bone].rot, ch->nEntries, v4, t);
				break;
			case POSE_SCALE:
				poseFileGet3(&s->boneState[ch->bone].scale, ch->nEntries, v3, t);
				break;
			}
		}

		ch = ((struct PoseFileAnimChannel *)((char *)ch + ch->size));
	}
}

static void anim3DUpdate(void *arg) {
	(void)arg;
	for (struct Anim3DState *s = clBegin(ANIM_STATE); s; s = clNext(ANIM_STATE, s)) {
		struct PoseFileHeader *h = &s->poseFile->hdr;
		struct PoseFileAnim *a = getAnim(h, s->animName);
		if (!a)
			continue;

		float spd = s->animSpeed * gameSpeed;
		s->animTime += spd;
		if (s->animTime >= a->duration * 60) {
			if (s->flags & ANIM_FLAG_SINGLE) {
				s->animTime = a->duration * 60;
				if (!(s->flags & ANIM_FLAG_ENDED)) {
					if (s->event)
						s->event(s);
					s->flags |= ANIM_FLAG_ENDED;
				}
			} else {
				s->animTime = 0;
				if (s->event)
					s->event(s);
			}
		}

		animUpdateState(s, a);
	}
}

void drawAnim(struct Model *m, struct Anim3DState *s) {
	if (!s->poseFile || !s->animName || !s->boneState)
		return;
	drawFlush();

	struct AnimUbo ubo;
	memset(&ubo, 0, sizeof(ubo));
	Mat mats[DRAW_MAX_BONE];
	struct PoseFileHeader *h = &s->poseFile->hdr;
	struct PoseFileBone *b = (struct PoseFileBone *)(h + 1);
	for (unsigned int i = 0; i < s->nBones; i++) {
		/* convert bonestate to mat */
		Mat mat;
		matFromTranslation(&mat, &s->boneState[i].pos);
		matRotate(&mat, &mat, &s->boneState[i].rot);
		matScale(&mat, &mat, &s->boneState[i].scale);

		if (b[i].parent >= 0) {
			matMul(&mats[i], &mats[b[i].parent], &mat);
		} else {
			matCopy(&mats[i], &mat);
		}

		Mat invBind;
		matLoad(&invBind, b[i].inverseBindMat);
		matMul(&ubo.finalMats[i], &mats[i], &invBind);
	}
	drawSetAnimUbo(&ubo, sizeof(ubo));
	drawModel3D(m);
}

static void anim3DNotify(void *arg, void *component, int type) {
	(void)arg;
	struct Anim3DState *s = (struct Anim3DState *)component;
	if (type == NOTIFY_CREATE) {
		s->animSpeed = 1;
	} else if (type == NOTIFY_DELETE) {
		globalDealloc(s->boneState);
		s->boneState = NULL;
	} else if (type == NOTIFY_PURGE) {
		for (s = clBegin(ANIM_STATE); s; s = clNext(ANIM_STATE, s)) {
			globalDealloc(s->boneState);
		}
	}
}

float anim3DLength(struct Anim3DState *s) {
	struct PoseFileAnim *a = getAnim(&s->poseFile->hdr, s->animName);
	if (a) {
		return a->duration * 60;
	} else {
		return 0;
	}
}

void anim3DInit(void) {
	componentListInit(ANIM_STATE, struct Anim3DState);
	setNotifier(ANIM_STATE, anim3DNotify, NULL);
	addUpdate(UPDATE_PHYS, anim3DUpdate, NULL);
}
void anim3DFini(void) {
	removeUpdate(UPDATE_PHYS, anim3DUpdate);
	componentListFini(ANIM_STATE);
}
