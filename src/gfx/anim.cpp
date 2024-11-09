#include <gfx/draw.h>
#include <events.h>
#include <assets.h>
#include <string.h>
#include <cmath>
#include <vec.h>

void drawSetAnimUbo(void *data, size_t dataSize);

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
	LoadedPoseFile *lpf = static_cast<LoadedPoseFile *>(globalAlloc(a->bufferSize));
	assetRead(a, lpf, a->bufferSize);

	assetClose(a);

	return lpf;
}
void deletePoseFile(struct LoadedPoseFile *lpf) {
	globalDealloc(lpf);
}




static int poseFileIdx3(int n, struct PoseFileVec3 *v, float time) {
	int idx = -1;
	for (int i = 0; i < n; i++) {
		if (v[i].t < time)
			idx = i;
		else
			break;
	}
	return idx;
}
static int poseFileIdx4(int n, struct PoseFileVec4 *v, float time) {
	int idx = -1;
	for (int i = 0; i < n; i++) {
		if (v[i].t < time)
			idx = i;
		else
			break;
	}
	return idx;
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
static bool animGetBones(struct Anim3DBoneState *dest, struct Anim3DState *s) {
	struct PoseFileHeader *h = &s->poseFile->hdr;
	struct PoseFileAnim *a = getAnim(h, s->animName);
	if (!a)
		return false;

	struct PoseFileBone *b = (struct PoseFileBone *)(h + 1);
	for (unsigned int i = 0; i < h->nBones; i++) {
		dest[i].x = b[i].trans[0];
		dest[i].y = b[i].trans[1];
		dest[i].z = b[i].trans[2];

		dest[i].rx = b[i].rot[0];
		dest[i].ry = b[i].rot[1];
		dest[i].rz = b[i].rot[2];
		dest[i].rw = b[i].rot[3];

		dest[i].sx = b[i].scale[0];
		dest[i].sy = b[i].scale[1];
		dest[i].sz = b[i].scale[2];
	}

	float t = s->animTime / 60.0f;
	struct PoseFileAnimChannel *ch = (struct PoseFileAnimChannel *)(a + 1);
	for (unsigned int i = 0; i < a->nChannels; i++) {
		if (ch->bone >= h->nBones) {
			ch = ((struct PoseFileAnimChannel *)((char *)ch + ch->size));
			continue;
		}
		struct PoseFileVec3 *v3 = (struct PoseFileVec3 *)(ch + 1);
		struct PoseFileVec4 *v4 = (struct PoseFileVec4 *)v3;
		struct Anim3DBoneState *d = &dest[ch->bone];
		int n1 = ch->nEntries - 1;
		int idx, idx1;
		switch (ch->type) {
		case POSE_TRANSLATE:
			idx = poseFileIdx3(ch->nEntries, v3, t);
			idx1 = idx + 1;
			if (idx == -1) {
				d->x = v3[0].v[0];
				d->y = v3[0].v[1];
				d->z = v3[0].v[2];
			} else if (idx == n1) {
				d->x = v3[n1].v[0];
				d->y = v3[n1].v[1];
				d->z = v3[n1].v[2];
			} else {
				float f = (t - v3[idx].t) / (v3[idx1].t - v3[idx].t);
				d->x = lerp(v3[idx].v[0], v3[idx1].v[0], f);
				d->y = lerp(v3[idx].v[1], v3[idx1].v[1], f);
				d->z = lerp(v3[idx].v[2], v3[idx1].v[2], f);
			}
			break;
		case POSE_ROTATE:
			idx = poseFileIdx4(ch->nEntries, v4, t);
			idx1 = idx + 1;
			if (idx == -1) {
				d->rx = v4[0].v[0];
				d->ry = v4[0].v[1];
				d->rz = v4[0].v[2];
				d->rw = v4[0].v[3];
			} else if (idx == n1) {
				d->rx = v4[n1].v[0];
				d->ry = v4[n1].v[1];
				d->rz = v4[n1].v[2];
				d->rw = v4[n1].v[3];
			} else {
				float f = (t - v4[idx].t) / (v4[idx1].t - v4[idx].t);
				Vec4 dst = Vec4::nlerp(Vec4{ v4[idx].v }, Vec4{ v4[idx1].v }, f);
				d->rx = dst.x;
				d->ry = dst.y;
				d->rz = dst.z;
				d->rw = dst.w;
			}
			break;
		case POSE_SCALE:
			idx = poseFileIdx3(ch->nEntries, v3, t);
			idx1 = idx + 1;
			if (idx == -1) {
				d->sx = v3[0].v[0];
				d->sy = v3[0].v[1];
				d->sz = v3[0].v[2];
			} else if (idx == n1) {
				d->sx = v3[n1].v[0];
				d->sy = v3[n1].v[1];
				d->sz = v3[n1].v[2];
			} else {
				float f = (t - v3[idx].t) / (v3[idx1].t - v3[idx].t);
				d->sx = lerp(v3[idx].v[0], v3[idx1].v[0], f);
				d->sy = lerp(v3[idx].v[1], v3[idx1].v[1], f);
				d->sz = lerp(v3[idx].v[2], v3[idx1].v[2], f);
			}
			break;
		}

		ch = ((struct PoseFileAnimChannel *)((char *)ch + ch->size));
	}
	return true;
}

void drawAnim(struct Model *m, struct Anim3DState *s) {
	if (!s->poseFile || !s->animName)
		return;
	drawFlush();

	struct Anim3DBoneState bs[DRAW_MAX_BONE];
	if (!animGetBones(bs, s))
		return;

	struct AnimUbo ubo;
	memset(&ubo, 0, sizeof(ubo));
	Mat mats[DRAW_MAX_BONE];
	struct PoseFileHeader *h = &s->poseFile->hdr;
	struct PoseFileBone *b = (struct PoseFileBone *)(h + 1);
	for (unsigned int i = 0; i < h->nBones; i++) {
		/* convert bone to mat */
		Mat mat{ 1.0f };
		mat.translate(Vec3{ bs[i].x, bs[i].y, bs[i].z });
		mat.rotate(Vec4{ bs[i].rx, bs[i].ry, bs[i].rz, bs[i].rw });
		mat.scale(Vec3{ bs[i].sx, bs[i].sy, bs[i].sz });

		if (b[i].parent >= 0) {
			mats[i] = mats[b[i].parent] * mat;
		} else {
			mats[i] = mat;
		}
		ubo.finalMats[i] = mats[i] * Mat{ b[i].inverseBindMat };
	}
	drawSetAnimUbo(&ubo, sizeof(ubo));
	drawModel3D(m);
}

static void anim3DUpdate(void *arg) {
	(void)arg;
	for (auto s = ANIM_STATES.begin(); s != ANIM_STATES.end(); ++s) {
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
						s->event(s.ptr());
					s->flags |= ANIM_FLAG_ENDED;
				}
			} else {
				s->animTime = 0;
				if (s->event)
					s->event(s.ptr());
			}
		}
	}
}

static void anim3DNotify(void *arg, void *component, int type) {
	(void)arg;
	Anim3DState *s = static_cast<Anim3DState *>(component);
	if (type == NOTIFY_CREATE) {
		s->animSpeed = 1;
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
	addUpdate(UPDATE_LATE, anim3DUpdate, NULL);
}
void anim3DFini(void) {
	removeUpdate(UPDATE_LATE, anim3DUpdate);
	componentListFini(ANIM_STATE);
}
