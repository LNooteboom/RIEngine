#include <gfx/drawvm.h>
#include <gfx/ttf.h>
#include <events.h>
#include <mem.h>
#include <string.h>
#include "gfx.h"

int drawVmUpdateSkip;
int drawVmLanguage;
int drawVmApi;
int drawVmCount;

float drawVmGlobalsF[8];

static struct DrawVm *curDvm;

struct DrawVmLayer {
	struct DrawVm *first;
	struct DrawVm *last;
};
static struct DrawVmLayer layers[DVM_N_LAYERS];

static struct IchigoState iState;

#define TEX_KEY_SZ 64
struct DrawVmTextureList {
	char key[TEX_KEY_SZ];
	struct Texture *tex;
};
static struct Vector texList;

#define POSE_KEY_SZ 64
struct DrawVmPoseFileList {
	char key[POSE_KEY_SZ];
	struct LoadedPoseFile *lpf;
};
static struct Vector poseList;

struct DrawVmInterpTargetInfo {
	int offset;
	int n;
};
#define offset(a, b) ((int)offsetof(a, b))
static const struct DrawVmInterpTargetInfo interpTargets[DVM_INTERP_N] = {
	[DVM_INTERP_NONE	 ] = {0									, 0},
	[DVM_INTERP_OFFS	 ] = {offset(struct DrawVm, xOffs)	, 3},
	[DVM_INTERP_SCALE	 ] = {offset(struct DrawVm, xScale)	, 3},
	[DVM_INTERP_ROT		 ] = {offset(struct DrawVm, xRot)		, 3},
	[DVM_INTERP_WVEL     ] = {offset(struct DrawVm, xWVel)      , 3},
	[DVM_INTERP_COL1	 ] = {offset(struct DrawVm, col1[0])	, 3},
	[DVM_INTERP_COL2	 ] = {offset(struct DrawVm, col2[0])	, 3},
	[DVM_INTERP_ALPHA1	 ] = {offset(struct DrawVm, col1[3])	, 1},
	[DVM_INTERP_ALPHA2	 ] = {offset(struct DrawVm, col2[3])	, 1},
	[DVM_INTERP_SRC_OFFS ] = {offset(struct DrawVm, srcX)		, 2},
	[DVM_INTERP_SRC_SCALE] = {offset(struct DrawVm, srcW)		, 2},

	[DVM_INTERP_TEX0_OFFS + 0] = {offset(struct DrawVm, tex[0].x), 2},
	[DVM_INTERP_TEX0_OFFS + 1] = {offset(struct DrawVm, tex[1].x), 2},
	[DVM_INTERP_TEX0_OFFS + 2] = {offset(struct DrawVm, tex[2].x), 2},
	[DVM_INTERP_TEX0_OFFS + 3] = {offset(struct DrawVm, tex[3].x), 2},
	[DVM_INTERP_TEX0_OFFS + 4] = {offset(struct DrawVm, tex[4].x), 2},
	[DVM_INTERP_TEX0_OFFS + 5] = {offset(struct DrawVm, tex[5].x), 2},
	[DVM_INTERP_TEX0_OFFS + 6] = {offset(struct DrawVm, tex[6].x), 2},
	[DVM_INTERP_TEX0_OFFS + 7] = {offset(struct DrawVm, tex[7].x), 2},

	[DVM_INTERP_TEX0_SCALE + 0] = {offset(struct DrawVm, tex[0].xs), 2},
	[DVM_INTERP_TEX0_SCALE + 1] = {offset(struct DrawVm, tex[1].xs), 2},
	[DVM_INTERP_TEX0_SCALE + 2] = {offset(struct DrawVm, tex[2].xs), 2},
	[DVM_INTERP_TEX0_SCALE + 3] = {offset(struct DrawVm, tex[3].xs), 2},
	[DVM_INTERP_TEX0_SCALE + 4] = {offset(struct DrawVm, tex[4].xs), 2},
	[DVM_INTERP_TEX0_SCALE + 5] = {offset(struct DrawVm, tex[5].xs), 2},
	[DVM_INTERP_TEX0_SCALE + 6] = {offset(struct DrawVm, tex[6].xs), 2},
	[DVM_INTERP_TEX0_SCALE + 7] = {offset(struct DrawVm, tex[7].xs), 2},

	[DVM_INTERP_TEX0_SCROLL + 0] = {offset(struct DrawVm, texScroll[0].x), 2},
	[DVM_INTERP_TEX0_SCROLL + 1] = {offset(struct DrawVm, texScroll[1].x), 2},
	[DVM_INTERP_TEX0_SCROLL + 2] = {offset(struct DrawVm, texScroll[2].x), 2},
	[DVM_INTERP_TEX0_SCROLL + 3] = {offset(struct DrawVm, texScroll[3].x), 2},
	[DVM_INTERP_TEX0_SCROLL + 4] = {offset(struct DrawVm, texScroll[4].x), 2},
	[DVM_INTERP_TEX0_SCROLL + 5] = {offset(struct DrawVm, texScroll[5].x), 2},
	[DVM_INTERP_TEX0_SCROLL + 6] = {offset(struct DrawVm, texScroll[6].x), 2},
	[DVM_INTERP_TEX0_SCROLL + 7] = {offset(struct DrawVm, texScroll[7].x), 2},
};

static void modeRot(struct DrawVm *d) {
	switch (d->rotMode) {
	case DVM_ROT_XYZ:
		/*drawRotateX(d->xRot);
		drawRotateY(d->yRot);
		drawRotateZ(d->zRot);*/
		drawRotateXYZ(d->xRot, d->yRot, d->zRot);
		break;
	case DVM_ROT_XZY:
		drawRotateX(d->xRot);
		drawRotateZ(d->zRot);
		drawRotateY(d->yRot);
		break;
	case DVM_ROT_YXZ:
		drawRotateY(d->yRot);
		drawRotateX(d->xRot);
		drawRotateZ(d->zRot);
		break;
	case DVM_ROT_YZX:
		drawRotateY(d->yRot);
		drawRotateZ(d->zRot);
		drawRotateX(d->xRot);
		break;
	case DVM_ROT_ZXY:
		drawRotateZ(d->zRot);
		drawRotateX(d->xRot);
		drawRotateY(d->yRot);
		break;
	case DVM_ROT_ZYX:
		drawRotateZ(d->zRot);
		drawRotateY(d->yRot);
		drawRotateX(d->xRot);
		break;
	}
}

static struct DrawVm *drawVmDrawTransform(struct DrawVm *d, struct DrawVm *parent) {
	/* Do transformation */
	struct DrawVm *ret = parent;
	if (!parent) {
		if (d->parent) {
			struct DrawVm *p2 = getComponentOpt(DRAW_VM, d->parent);
			ret = p2;
			if (p2) {
				drawVmDrawTransform(p2, NULL);
			}
		} else {
			struct Transform *tf = getComponentOpt(TRANSFORM, d->entity);
			if (tf) {
				if (d->flags & DVM_FLAG_TF_3D_ROTATION) {
					drawTransform3D(tf);
				} else {
					if (d->flags & DVM_FLAG_ROUNDED_POS) {
						drawTransformRounded(tf);
					} else {
						drawTransform(tf);
					}
					if (d->flags & DVM_FLAG_TF_ROTATION) {
						drawTransformRotation(tf);
					}
				}
			} else {
				drawMatIdentity();
			}
		}
	}
	if (d->flags & DVM_FLAG_ROUNDED_POS) {
		drawTranslate3D(roundf(d->xOffs), roundf(d->yOffs), roundf(d->zOffs));
	} else {
		drawTranslate3D(d->xOffs, d->yOffs, d->zOffs);
	}
	modeRot(d);
	drawScale3D(d->xScale, d->yScale, d->zScale);
	return ret;
}

static void drawVmDraw(struct DrawVm *d, struct DrawVm *par) {
	struct DrawVm *parent = drawVmDrawTransform(d, par);
	
	bool inView = true;
	if ((d->mode == DVM_MODEL || d->mode == DVM_ANIM) && !(d->flags & DVM_FLAG_NO_FCULL)) {
		if (!d->model || !drawModelInFrustum(d->model)) {
			inView = false;
		}
	}
	if (inView && d->mode != DVM_NONE) {
		if (d->customShader) {
			drawShaderUse(d->customShader);
		} else {
			drawShaderUseStd(d->stdShader);
		}
		if (d->flags & DVM_FLAG_SHADER_ARGS) {
			struct IchigoLocals *l = getComponent(DRAW_VM_LOCALS, d->entity);
			drawShaderArgs(8, l->f);
		}
		drawZBufferWrite(!(d->flags & DVM_FLAG_NO_Z_BUFFER_WRITE));
		drawUvModelMat(d->flags & DVM_FLAG_UV_MODEL);

		for (int i = 0; i < DRAW_MAX_TEX; i++) {
			if (d->tex[i].tex) {
				drawTexture(i, d->tex[i].tex);
				drawTextureOffsetScale(i, d->tex[i].x, d->tex[i].y, d->tex[i].xs, d->tex[i].ys);
			}
		}
		drawBlend(d->blend);

		drawColorMode(d->colMode);
		if (parent && (parent->flags & (DVM_FLAG_COLOR_CHILDREN | DVM_FLAG_ALPHA_CHILDREN))) {
			float col1[4] = { d->col1[0], d->col1[1], d->col1[2], d->col1[3] };
			float col2[4] = { d->col2[0], d->col2[1], d->col2[2], d->col2[3] };
			float *parentCol = parent->colMode == COLOR2 ? &parent->col2 : &parent->col1;
			if (parent->flags & DVM_FLAG_ALPHA_CHILDREN) {
				col1[3] *= parentCol[3];
				col2[3] *= parentCol[3];
			}
			if (parent->flags & DVM_FLAG_COLOR_CHILDREN) {
				col1[0] *= parentCol[0];
				col1[1] *= parentCol[1];
				col1[2] *= parentCol[2];
				col2[0] *= parentCol[0];
				col2[1] *= parentCol[1];
				col2[2] *= parentCol[2];
			}
			drawColor(col1[0], col1[1], col1[2], col1[3]);
			drawColor2(col2[0], col2[1], col2[2], col2[3]);
		} else {
			drawColor(d->col1[0], d->col1[1], d->col1[2], d->col1[3]);
			drawColor2(d->col2[0], d->col2[1], d->col2[2], d->col2[3]);
		}
		
		float sx = d->srcX, sy = d->srcY, sw = d->srcW, sh = d->srcH;
		if (d->flags & DVM_FLAG_FLIP_SRC_X) {
			sx += sw;
			sw = -sw;
		}
		if (d->flags & DVM_FLAG_FLIP_SRC_Y) {
			sy += sh;
			sh = -sh;
		}

		if (d->anchor) {
			float xAnchor = 0, yAnchor = 0;
			if ((d->anchor & 3) == 1)
				xAnchor = sw / 2.0f;
			else if ((d->anchor & 3) == 2)
				xAnchor = sw / -2.0f;
			if ((d->anchor & 12) == 4)
				yAnchor = sh / 2.0f;
			else if ((d->anchor & 12) == 8)
				yAnchor = sh / -2.0f;

			if (d->nChildren)
				drawPushMat();
			drawTranslate(xAnchor, yAnchor);
		}

		drawCullInvert(d->flags & DVM_FLAG_CULL_FRONT);

		drawSrcRect(sx, sy, sw, sh);
		switch (d->mode) {
		case DVM_NONE: /* Should never happen */
		case DVM_RECT:
			drawRect(d->srcW, d->srcH);
			break;
		case DVM_MODEL:
			if (d->model) {
				drawModel3D(d->model);
			}
			break;
		case DVM_ELLIPSE:
			drawEllipse(d->nPoints, d->srcW, d->srcH);
			break;
		case DVM_ARC:
		{
			struct IchigoLocals *l = getComponent(DRAW_VM_LOCALS, d->entity);
			drawArc(d->nPoints, l->f[0], l->f[1], l->f[2], l->f[3]);
			break;
		}
		case DVM_ANIM:
			if (d->model) {
				drawAnim(d->model, getComponent(ANIM_STATE, d->entity));
			}
			break;
		case DVM_ANIM_PARENT:
			if (d->model && d->parent) {
				drawAnim(d->model, getComponent(ANIM_STATE, d->parent)); /* Use AnimState of parent */
			}
			break;
		case DVM_PLANE:
			drawRect(1, -1);
			break;
		case DVM_BILLBOARD:
			drawRectBillboard(1, 1);
			break;
		}

		if (d->anchor && d->nChildren) {
			drawPopMat();
		}
	}

	if (d->nChildren) {
		entity_t child = d->childStart;
		while (child) {
			struct DrawVm *c = getComponent(DRAW_VM, child);
			if (c->layer == d->layer) {
				drawPushMat();
				drawVmDraw(c, d);
				drawPopMat();
			}
			child = c->childNext;
		}
	}
}

static void drawVmDrawLayer(void *arg) {
	int layer = (uintptr_t)arg;
	for (struct DrawVm *d = layers[layer].first; d; d = d->layerNext) {
		if (!(d->flags & DVM_FLAG_INVISIBLE)) {
			drawVmDraw(d, NULL);
		}
	}
}

static void dvmDelete(struct DrawVm *d) {
	entity_t en = d->entity;
	if (d->flags & DVM_FLAG_DELETE_ALL) {
		deleteEntity(en);
	} else {
		removeComponent(DRAW_VM, en);
		removeComponent(DRAW_VM_VM, en);
		removeComponent(DRAW_VM_LOCALS, en);
	}
}

static float addNormalized(float x, float add, float scale) {
	float v = x + add;
	if (v < -scale)
		v += 2 * scale;
	else if (v > scale)
		v -= 2 * scale;
	return v;
}
static bool drawVmUpdate(struct DrawVm *d) {
	/* Update VM */
	if (d->state == DVM_RUNNING) {
		struct IchigoVm *vm = getComponent(DRAW_VM_VM, d->entity);
		curDvm = d;
		ichigoLocalsCur = getComponent(DRAW_VM_LOCALS, d->entity);
		ichigoVmUpdate(vm, gameSpeed);
		ichigoLocalsCur = NULL;
		curDvm = NULL;

		if (d->state != DVM_RUNNING) { /* State changed inside update */
			ichigoVmKillAll(vm);
		} else {
			/* Check if any coroutines are still running */
			d->state = DVM_STOPPED;
			for (int i = 0; i < ICHIGO_VM_MAX_COROUT; i++) {
				if (vm->coroutines[i].active) {
					d->state = DVM_RUNNING;
					break;
				}
			}
		}
	}
	if (d->state == DVM_DELETED) {
		dvmDelete(d);
		return false;
	} else if (d->state == DVM_STATIC) {
		return true;
	}

	d->xScale += d->xGrow;
	d->yScale += d->yGrow;
	d->zScale += d->zGrow;
	d->xRot = addNormalized(d->xRot, d->xWVel, PI);
	d->yRot = addNormalized(d->yRot, d->yWVel, PI);
	d->zRot = addNormalized(d->zRot, d->zWVel, PI);
	for (int i = 0; i < DVM_N_INTERPS; i++) {
		if (d->interpTargets[i] == DVM_INTERP_NONE)
			continue;
		const struct DrawVmInterpTargetInfo *info = &interpTargets[d->interpTargets[i]];
		float *val = (float *)((char *)d + info->offset);
		interpUpdateBezier(&d->interps[i], info->n, val, gameSpeed, d->interpEx1, d->interpEx2);
		if (d->interps[i].time >= d->interps[i].endTime) {
			d->interpTargets[i] = DVM_INTERP_NONE;
		}
	}

	for (int i = 0; i < DRAW_MAX_TEX; i++) {
		d->tex[i].x = addNormalized(d->tex[i].x, d->texScroll[i].x, 1);
		d->tex[i].y = addNormalized(d->tex[i].y, d->texScroll[i].y, 1);
	}
	return true;
}
static void drawVmAddToLayerList(struct DrawVm *d, struct DrawVm *parent) {
	if (!parent || parent->layer != d->layer) {
		/* Add it to the layer list */
		int layer = d->layer;
		if (layers[layer].first) {
			layers[layer].last->layerNext = d;
		} else {
			layers[layer].first = d;
		}
		layers[layer].last = d;
	}
	d->layerNext = NULL;
}

static void drawVmUpdateAll(void *arg) {
	(void)arg;
	/* Clear linked lists */
	for (int i = 0; i < DVM_N_LAYERS; i++) {
		layers[i].first = NULL;
		layers[i].last = NULL;
	}

	drawVmCount = 0;
	for (struct DrawVm *d = clBegin(DRAW_VM); d; d = clNext(DRAW_VM, d)) {
		struct DrawVm *parent = NULL;
		if (d->parent) {
			parent = getComponentOpt(DRAW_VM, d->parent);
			if (!parent) {
				/* Parent deleted, delete this too */
				dvmDelete(d);
				continue;
			}
		}
		bool draw = true;
		if (!d->layer || d->layer >= drawVmUpdateSkip)
			draw = drawVmUpdate(d);

		if (draw) {
			drawVmAddToLayerList(d, parent);
			drawVmCount++;
		}
	}
}

struct DrawVm *drawVmNew(entity_t entity, const char *fn) {
	struct DrawVm *d = newComponent(DRAW_VM, entity);
	newComponent(DRAW_VM_VM, entity);
	newComponent(DRAW_VM_LOCALS, entity);
	d->mainFn = fn;
	d->flags = 0;
	drawVmEvent(d, DVM_EVENT_CREATE);
	return d;
}
struct DrawVm *drawVmNewChild(struct DrawVm *parent, const char *fn) {
	entity_t entity = getNewEntity();
	struct DrawVm *d = newComponent(DRAW_VM, entity);
	newComponent(DRAW_VM_VM, entity);
	struct IchigoLocals *l = newComponent(DRAW_VM_LOCALS, entity);

	d->parent = parent->entity;
	parent->nChildren++;
	if (parent->childEnd) {
		struct DrawVm *sib = getComponent(DRAW_VM, parent->childEnd);
		sib->childNext = entity;
		d->childPrev = parent->childEnd;
	} else {
		parent->childStart = entity;
	}
	parent->childEnd = entity;

	struct IchigoLocals *parentLocals = getComponent(DRAW_VM_LOCALS, parent->entity);
	ichigoCopyLocals(l, parentLocals);

	d->flags = 0;
	if (parent->flags & DVM_FLAG_TTF) {
		d->flags |= DVM_FLAG_TTF;
		d->tex[0].tex = parent->tex[0].tex;
		if (d->tex[0].tex) {
			d->tex[0].tex->refs++;
		}
	}

	d->mainFn = fn;
	d->layer = parent->layer;
	drawVmEvent(d, DVM_EVENT_CREATE);
	return d;
}
void drawVmDelete(entity_t entity) {
	struct DrawVm *d = getComponentOpt(DRAW_VM, entity);
	if (d)
		drawVmEvent(d, DVM_EVENT_DELETE);
}
void drawVmEvent(struct DrawVm *d, int event) {
	if (d->state != DVM_RUNNING && d->state != DVM_STOPPED) {
		if (event == DVM_EVENT_DELETE) {
			dvmDelete(d);
		}
		return;
	}

	/* Create a new coroutine */
	struct IchigoVm *vm = getComponent(DRAW_VM_VM, d->entity);
	ichigoVmExec(vm, d->mainFn, "i", event);
	d->state = DVM_RUNNING;

	if (!(d->flags & DVM_FLAG_NO_CHILD_EVENT) && d->nChildren) {
		entity_t child = d->childStart;
		while (child) {
			struct DrawVm *c = getComponent(DRAW_VM, child);
			drawVmEvent(c, event);
			child = c->childNext;
		}
	}
}
void drawVmEvent2(entity_t entity, int event) {
	struct DrawVm *d = getComponentOpt(DRAW_VM, entity);
	if (d)
		drawVmEvent(d, event);
}
void drawVmEventAll(int event) {
	for (struct DrawVm *d = clBegin(DRAW_VM); d; d = clNext(DRAW_VM, d)) {
		drawVmEvent(d, event);
	}
}

void drawVmTexture(struct DrawVm *d, int slot, const char *texture) {
	struct DrawVmTextureList *tl = NULL;
	if (texture[0] == '@') {
		d->tex[slot].tex = drawGetFboTexture(texture[1] - '0');
		return;
	}
	for (unsigned int i = 0; i < vecCount(&texList); i++) {
		struct DrawVmTextureList *t = vecAt(&texList, i);
		if (!strcmp(texture, t->key)) {
			tl = t;
			break;
		}
	}

	if (tl) {
		d->tex[slot].tex = tl->tex;
	} else {
		bool srgb = d->customShader || d->stdShader != SHADER_2D;
		struct Texture *tex = srgb ? loadTexture3D(texture) : loadTexture(texture);
		if (tex) {
			tl = vecInsert(&texList, -1);
			strncpy(tl->key, texture, TEX_KEY_SZ - 1);
			tl->key[TEX_KEY_SZ - 1] = 0;
			tl->tex = tex;
			d->tex[slot].tex = tex;
		}
	}
}

void drawVmPoseFile(struct DrawVm *d, const char *poseFile) {
	struct DrawVmPoseFileList *pl = NULL;
	for (unsigned int i = 0; i < vecCount(&poseList); i++) {
		struct DrawVmPoseFileList *p = vecAt(&poseList, i);
		if (!strcmp(poseFile, p->key)) {
			pl = p;
			break;
		}
	}

	struct Anim3DState *as = getComponent(ANIM_STATE, d->entity);
	if (pl) {
		as->poseFile = pl->lpf;
	} else {
		struct LoadedPoseFile *lpf = loadPoseFile(poseFile);
		if (lpf) {
			pl = vecInsert(&poseList, -1);
			strncpy(pl->key, poseFile, POSE_KEY_SZ - 1);
			pl->key[POSE_KEY_SZ - 1] = 0;
			pl->lpf = lpf;
			as->poseFile = pl->lpf;
		}
	}
}

void drawVmInterp(struct DrawVm *d, int slot, int target, float time, int mode, float a, float b, float c) {
	float goal[3] = { a, b, c };
	const struct DrawVmInterpTargetInfo *t = &interpTargets[target];
	float *start = (float *)((char *)d + t->offset);
	interpStart(&d->interps[slot], mode, t->n, start, goal, time);
	d->interpTargets[slot] = target;
}
void drawVmAddFile(const char *file) {
	ichigoAddFile(&iState, file);
}


static void drawVmNotifier(void *arg, void *component, int type) {
	(void) arg;
	struct DrawVm *d = component;
	if (type == NOTIFY_CREATE) {
		/* Set some default values, rest is initialized to zero */
		
		d->xScale = d->yScale = d->zScale = 1;
		for (int i = 0; i < DRAW_MAX_TEX; i++) {
			d->tex[i].xs = d->tex[i].ys = 1;
		}
		d->nPoints = 8;
		d->mode = DVM_NONE;
		d->stdShader = SHADER_2D;
		d->customShader = NULL;
		for (int i = 0; i < 4; i++) {
			d->col1[i] = 1;
			d->col2[i] = 1;
		}
	} else if (type == NOTIFY_DELETE) {
		if (d->flags & DVM_FLAG_TTF) {
			deleteTexture(d->tex[0].tex);
		}
		if (d->parent) {
			struct DrawVm *parent = getComponentOpt(DRAW_VM, d->parent);
			if (parent) {
				parent->nChildren--;
				if (d->childPrev) {
					struct DrawVm *sib = getComponent(DRAW_VM, d->childPrev);
					sib->childNext = d->childNext;
				} else {
					parent->childStart = d->childNext;
				}
				if (d->childNext) {
					struct DrawVm *sib = getComponent(DRAW_VM, d->childNext);
					sib->childPrev = d->childPrev;
				} else {
					parent->childEnd = d->childPrev;
				}
			}
		}
		if (d->customShader) {
			drawShaderDelete(d->customShader);
			d->customShader = NULL;
		}
		/*if (d->nChildren) {
			entity_t child = d->childStart;
			while (child) {
				struct DrawVm *c = getComponent(DRAW_VM, child);
				c->parent = 0;
				child = c->childNext;
				c->childNext = 0;
				c->childPrev = 0;
				deleteEntity(c->entity);
			}
		}*/
	} else if (type == NOTIFY_PURGE) {
		for (d = clBegin(DRAW_VM); d; d = clNext(DRAW_VM, d)) {
			if (d->customShader) {
				drawShaderDelete(d->customShader);
				d->customShader = NULL;
			}
		}
	}
}

static void drawVmEndScene(void);

static void drawVmVmNotifier(void *arg, void *component, int type) {
	(void) arg;
	struct IchigoVm *vm = component;
	if (type == NOTIFY_CREATE) {
		ichigoVmNew(&iState, vm, vm->en);
	} else if (type == NOTIFY_DELETE) {
		ichigoVmDelete(vm);
	} else if (type == NOTIFY_PURGE) {
		for (vm = clBegin(DRAW_VM_VM); vm; vm = clNext(DRAW_VM_VM, vm)) {
			ichigoVmDelete(vm);
		}
		drawVmEndScene();
	}
}

static void drawVmEndScene(void) {
	for (unsigned int i = 0; i < vecCount(&texList); i++) {
		struct DrawVmTextureList *tl = vecAt(&texList, i);
		deleteTexture(tl->tex);
		tl->tex = NULL;
	}
	for (unsigned int i = 0; i < vecCount(&poseList); i++) {
		struct DrawVmPoseFileList *pl = vecAt(&poseList, i);
		deletePoseFile(pl->lpf);
		pl->lpf = NULL;
	}

	/* Clear layer list */
	for (int i = 0; i < DVM_N_LAYERS; i++) {
		layers[i].first = NULL;
		layers[i].last = NULL;
	}

	vecDestroy(&texList);
	vecDestroy(&poseList);
	vecCreate(&texList, sizeof(struct DrawVmTextureList));
	vecCreate(&poseList, sizeof(struct DrawVmPoseFileList));

	clearModels();
	ichigoClear(&iState);
}

static void drawVmAnimEvent(struct Anim3DState *as) {
	struct DrawVm *d = as->globals;
	if (d->flags & DVM_FLAG_ANIM_EVENT) {
		drawVmEvent(d, DVM_EVENT_ANIM);
	}
}

#define GET_DVM(vm) curDvm; (void) vm
static int i_getVarFLAGS(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	return d->flags;
}
static void i_setVarFLAGS(struct IchigoVm *vm, int val) {
	struct DrawVm *d = GET_DVM(vm);
	d->flags = val;
}
static int i_getVarLANG(struct IchigoVm *vm) {
	(void) vm;
	return drawVmLanguage;
}
static int i_getVarAPI(struct IchigoVm *vm) {
	(void)vm;
	return drawVmApi;
}
static int i_getVarANIM_FLAGS(struct IchigoVm *vm) {
	struct Anim3DState *a = getComponentOpt(ANIM_STATE, vm->en);
	return a ? a->flags : 0;
}
static void i_setVarANIM_FLAGS(struct IchigoVm *vm, int val) {
	struct Anim3DState *a = getComponentOpt(ANIM_STATE, vm->en);
	if (a) {
		a->flags = val;
	}
}
static float i_getVarANIM_LENGTH(struct IchigoVm *vm) {
	struct Anim3DState *a = getComponentOpt(ANIM_STATE, vm->en);
	return a ? anim3DLength(a) : 0;
}

#define DVM_GLBL_GET(T, NAME, DST) static T i_getVar##NAME(struct IchigoVm *vm) {return DST;}
#define DVM_GLBL_SET(T, NAME, DST) static void i_setVar##NAME(struct IchigoVm *vm, T val) {DST = val;}
#define DVM_GLBL_GETSET(T, NAME, DST) DVM_GLBL_GET(T, NAME, DST) DVM_GLBL_SET(T, NAME, DST)

DVM_GLBL_GETSET(float, GF0, drawVmGlobalsF[0])
DVM_GLBL_GETSET(float, GF1, drawVmGlobalsF[1])
DVM_GLBL_GETSET(float, GF2, drawVmGlobalsF[2])
DVM_GLBL_GETSET(float, GF3, drawVmGlobalsF[3])
DVM_GLBL_GETSET(float, GF4, drawVmGlobalsF[4])
DVM_GLBL_GETSET(float, GF5, drawVmGlobalsF[5])
DVM_GLBL_GETSET(float, GF6, drawVmGlobalsF[6])
DVM_GLBL_GETSET(float, GF7, drawVmGlobalsF[7])

static int i_getVarWIN_W(struct IchigoVm *vm) {
	return winW;
}
static int i_getVarWIN_H(struct IchigoVm *vm) {
	return winH;
}

static void i_setDelete(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->state = DVM_DELETED;
	vm->is->curCorout->waitTime = 999;
}
static void i_setStatic(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->state = DVM_STATIC;
	vm->is->curCorout->waitTime = 999;
}
static void i_stop(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->state = DVM_STOPPED;
	vm->is->curCorout->waitTime = 999;
}

static void i_layer(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->layer = ichigoGetInt(vm, 0);
}
static void i_rotMode(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->rotMode = ichigoGetInt(vm, 0);
}
static void i_mode(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->mode = ichigoGetInt(vm, 0);
}
static void i_colMode(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->colMode = ichigoGetInt(vm, 0);
}
static void i_blendMode(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->blend = ichigoGetInt(vm, 0);
}
static void i_points(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->nPoints = ichigoGetInt(vm, 0);
}

static void i_pos(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->xOffs = ichigoGetFloat(vm, 0);
	d->yOffs = ichigoGetFloat(vm, 1);
	d->zOffs = ichigoGetFloat(vm, 2);
}
static void i_anchor(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->anchor = (ichigoGetInt(vm, 0) & 3) | ((ichigoGetInt(vm, 1) & 3) << 2);
}
static void i_scale(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->xScale = ichigoGetFloat(vm, 0);
	d->yScale = ichigoGetFloat(vm, 1);
	d->zScale = ichigoGetFloat(vm, 2);
}
static void i_rotate(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->xRot = ichigoGetFloat(vm, 0);
	d->yRot = ichigoGetFloat(vm, 1);
	d->zRot = ichigoGetFloat(vm, 2);
}
static void i_grow(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->xGrow = ichigoGetFloat(vm, 0);
	d->yGrow = ichigoGetFloat(vm, 1);
	d->zGrow = ichigoGetFloat(vm, 2);
}
static void i_wvel(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->xWVel = ichigoGetFloat(vm, 0);
	d->yWVel = ichigoGetFloat(vm, 1);
	d->zWVel = ichigoGetFloat(vm, 2);
}
static void i_shader(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->customShader = NULL;
	d->stdShader = ichigoGetInt(vm, 0);
}
static void i_shaderCustom(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	uint16_t strLen;
	d->customShader = drawShaderNew(ichigoGetString(&strLen, vm, 0), ichigoGetString(&strLen, vm, 1));
}
static void i_color(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->col1[0] = ichigoGetFloat(vm, 0);
	d->col1[1] = ichigoGetFloat(vm, 1);
	d->col1[2] = ichigoGetFloat(vm, 2);
}
static void i_color2(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->col2[0] = ichigoGetFloat(vm, 0);
	d->col2[1] = ichigoGetFloat(vm, 1);
	d->col2[2] = ichigoGetFloat(vm, 2);
}
static void i_alpha(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->col1[3] = ichigoGetFloat(vm, 0);
}
static void i_alpha2(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->col2[3] = ichigoGetFloat(vm, 0);
}
static void i_srcRect(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	d->srcX = ichigoGetFloat(vm, 0);
	d->srcY = ichigoGetFloat(vm, 1);
	d->srcW = ichigoGetFloat(vm, 2);
	d->srcH = ichigoGetFloat(vm, 3);
	if (!d->mode)
		d->mode = DVM_RECT;
}
static void i_texture(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	uint16_t strLen;
	const char *tex = ichigoGetString(&strLen, vm, 1);
	if (tex)
		drawVmTexture(d, ichigoGetInt(vm, 0), tex);
}
static void i_texturePos(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	int slot = ichigoGetInt(vm, 0);
	d->tex[slot].x = ichigoGetFloat(vm, 1);
	d->tex[slot].y = ichigoGetFloat(vm, 2);
}
static void i_textureScale(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	int slot = ichigoGetInt(vm, 0);
	d->tex[slot].xs = ichigoGetFloat(vm, 1);
	d->tex[slot].ys = ichigoGetFloat(vm, 2);
}
static void i_textureScroll(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	int slot = ichigoGetInt(vm, 0);
	d->texScroll[slot].x = ichigoGetFloat(vm, 1);
	d->texScroll[slot].y = ichigoGetFloat(vm, 2);
}
static void i_interp(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	drawVmInterp(d, ichigoGetInt(vm, 0), ichigoGetInt(vm, 1), ichigoGetFloat(vm, 2), ichigoGetInt(vm, 3),
		ichigoGetFloat(vm, 4), ichigoGetFloat(vm, 5), ichigoGetFloat(vm, 6));
}
static void i_posBezier(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	for (int i = 0; i < 3; i++) {
		d->interpEx1[i] = ichigoGetFloat(vm, 1 + i);
		d->interpEx2[i] = ichigoGetFloat(vm, 7 + i);
	}
	drawVmInterp(d, 0, 1, ichigoGetFloat(vm, 0), INTERP_BEZIER,
		ichigoGetFloat(vm, 4), ichigoGetFloat(vm, 5), ichigoGetFloat(vm, 6));
}
static void i_model(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	uint16_t strLen;
	const char *model = ichigoGetString(&strLen, vm, 0);
	if (model)
		d->model = getModel(model);
}

static void i_childNew(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	uint16_t strLen;
	struct DrawVm *child = drawVmNewChild(d, ichigoGetString(&strLen, vm, 0));
	child->xOffs = ichigoGetFloat(vm, 1);
	child->yOffs = ichigoGetFloat(vm, 2);
	child->zOffs = ichigoGetFloat(vm, 3);
	child->layer = d->layer;
	
}
static void i_childNewRoot(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	uint16_t strLen;
	entity_t en = newEntity();

	struct Transform *parentTf = getComponentOpt(TRANSFORM, vm->en);
	if (parentTf) {
		struct Transform *tf = newComponent(TRANSFORM, en);
		tf->x = parentTf->x;
		tf->y = parentTf->y;
		tf->z = parentTf->z;
		tf->rx = parentTf->rx;
		tf->ry = parentTf->ry;
		tf->rz = parentTf->rz;
		tf->rw = parentTf->rw;
	}

	struct DrawVm *child = drawVmNew(en, ichigoGetString(&strLen, vm, 0));
	child->flags |= DVM_FLAG_DELETE_ALL;
	struct IchigoLocals *parentL = getComponent(DRAW_VM_LOCALS, vm->en);
	child->xOffs = ichigoGetFloat(vm, 1) + d->xOffs;
	child->yOffs = ichigoGetFloat(vm, 2) + d->yOffs;
	child->zOffs = ichigoGetFloat(vm, 3) + d->zOffs;
	child->layer = d->layer;
	child->xScale = d->xScale;
	child->yScale = d->yScale;
	child->zScale = d->zScale;
	child->col1[3] = d->col1[3];
	
	struct IchigoLocals *l = getComponent(DRAW_VM_LOCALS, child->entity);
	
	ichigoCopyLocals(l, parentL);
}
static void i_getParentVars(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	if (d->parent) {
		int mask = ichigoGetInt(vm, 0);
		struct IchigoLocals *l = getComponent(DRAW_VM_LOCALS, vm->en);
		struct IchigoLocals *parentL = getComponent(DRAW_VM_LOCALS, d->parent);
		for (int i = 0; i < 4; i++) {
			if (mask & 1)
				l->i[i] = parentL->i[i];
			mask >>= 1;
		}
		for (int i = 0; i < 8; i++) {
			if (mask & 1)
				l->f[i] = parentL->f[i];
			mask >>= 1;
		}
		for (int i = 0; i < 4; i++) {
			if (mask & 1)
				l->str[i] = parentL->str[i];
			mask >>= 1;
		}
	}
}
static void i_childEvent(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	int ev = ichigoGetInt(vm, 0);
	if (d->nChildren) {
		entity_t child = d->childStart;
		while (child) {
			struct DrawVm *c = getComponent(DRAW_VM, child);
			drawVmEvent(c, ev);
			child = c->childNext;
		}
	}
}
static void i_normalizeRot(struct IchigoVm *vm) {
	float r = normalizeRot(ichigoGetFloat(vm, 1));
	ichigoSetFloat(vm, 0, r);
}
static void i_getTextureSize(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	int slot = ichigoGetInt(vm, 0);
	struct Texture *tex = d->tex[slot].tex;
	int w = 0, h = 0;
	if (tex) {
		w = tex->w;
		h = tex->h;
	}
	ichigoSetInt(vm, 1, w);
	ichigoSetInt(vm, 2, h);
}


static void i_animLoad(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	struct Anim3DState *as = getComponentOpt(ANIM_STATE, vm->en);
	if (!as) {
		as = newComponent(ANIM_STATE, vm->en);
		as->event = drawVmAnimEvent;
		as->globals = d;
	}
	uint16_t nameLen;
	drawVmPoseFile(d, ichigoGetString(&nameLen, vm, 0));
}
static void i_animSet(struct IchigoVm *vm) {
	struct Anim3DState *as = getComponentOpt(ANIM_STATE, vm->en);
	if (as) {
		uint16_t nameLen;
		as->animName = ichigoGetString(&nameLen, vm, 0);
	}
}
static void i_animTime(struct IchigoVm *vm) {
	struct Anim3DState *as = getComponentOpt(ANIM_STATE, vm->en);
	if (as) {
		as->animTime = ichigoGetFloat(vm, 0);
	}
}
static void i_animSpeed(struct IchigoVm *vm) {
	struct Anim3DState *as = getComponentOpt(ANIM_STATE, vm->en);
	if (as) {
		as->animSpeed = ichigoGetFloat(vm, 0);
	}
}

static void i_strChar(struct IchigoVm *vm) {
	uint16_t l;
	const char *s = ichigoGetString(&l, vm, 2);
	int pos = ichigoGetInt(vm, 1);
	
	int ret = strCharUtf(s, l, &pos);

	ichigoSetInt(vm, 1, pos);
	ichigoSetInt(vm, 0, ret);
}
static void i_textureTTF(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	if (d->flags & DVM_FLAG_TTF) {
		deleteTexture(d->tex[0].tex);
		d->flags &= ~DVM_FLAG_TTF;
	}
	uint16_t sLen;
	const char *text = ichigoGetString(&sLen, vm, 3);
	if (text && sLen) {
		d->tex[0].tex = ttfLoad(text, ichigoGetInt(vm, 0), ichigoGetInt(vm, 1), ichigoGetInt(vm, 2));
		if (d->tex[0].tex) {
			d->flags |= DVM_FLAG_TTF;
		}
	}
}
static void i_textureTTF2(struct IchigoVm *vm) {
	struct DrawVm *d = GET_DVM(vm);
	if (d->flags & DVM_FLAG_TTF) {
		deleteTexture(d->tex[0].tex);
		d->flags &= ~DVM_FLAG_TTF;
	}
	uint16_t sLen;
	const char *text = ichigoGetString(&sLen, vm, 3);
	int bufLen = ichigoGetInt(vm, 4);
	if (bufLen > sLen)
		bufLen = sLen;
	char *buf = stackAlloc(bufLen + 1);
	memcpy(buf, text, bufLen);
	buf[bufLen] = 0;

	if (text && sLen) {
		d->tex[0].tex = ttfLoad(buf, ichigoGetInt(vm, 0), ichigoGetInt(vm, 1), ichigoGetInt(vm, 2));
		if (d->tex[0].tex) {
			d->flags |= DVM_FLAG_TTF;
		}
	}

	stackDealloc(bufLen + 1);
}
static void i_ttfWidth(struct IchigoVm *vm) {
	int c = ichigoGetInt(vm, 1);
	int w = ttfWidth(c);
	ichigoSetInt(vm, 0, w);
}

static void i_subStr(struct IchigoVm *vm) {
	uint16_t totalLen;
	const char *in = ichigoGetString(&totalLen, vm, 1);
	int start = ichigoGetInt(vm, 2);
	int len = ichigoGetInt(vm, 3);
	if (len < 0 || len + start > totalLen) len = totalLen - start;
	
	char *buf = stackAlloc(len + 1);
	for (int i = 0; i < len; i++) {
		buf[i] = in[i + start];
	}
	buf[len] = 0;
	ichigoSetArrayMut(vm, 0, buf, len, REG_BYTE);
	stackDealloc(len + 1);
}

static struct IchigoVar iVars[64];
static IchigoInstr *iInstrs[] = {
	[1] = i_setDelete,
	[2] = i_setStatic,
	[3] = i_stop,
	[4] = i_layer,
	[5] = i_rotMode,
	[6] = i_mode,
	[7] = i_colMode,
	[8] = i_blendMode,
	[9] = i_points,
	[10] = i_pos,
	[11] = i_anchor,
	[12] = i_scale,
	[13] = i_rotate,
	[14] = i_grow,
	[15] = i_wvel,
	[16] = i_shader,
	[17] = i_shaderCustom,
	[18] = i_color,
	[19] = i_color2,
	[20] = i_alpha,
	[21] = i_alpha2,
	[22] = i_srcRect,
	[23] = i_texture,
	[24] = i_texturePos,
	[25] = i_textureScale,
	[26] = i_textureScroll,
	[27] = i_interp,
	[28] = i_posBezier,
	[29] = i_model,
	[30] = i_childNew,
	[31] = i_childNewRoot,
	[32] = i_getParentVars,
	[33] = i_childEvent,
	[34] = i_normalizeRot,
	[35] = i_getTextureSize,
	[36] = i_modeLerp,

	[40] = i_animLoad,
	[41] = i_animSet,
	[42] = i_animTime,
	[43] = i_animSpeed,

	[50] = i_strChar,
	[51] = i_textureTTF,
	[52] = i_ttfWidth,
	[53] = i_subStr
};


static void setVar(int idx, enum IchigoRegType type, void *get, void *set) {
	iVars[idx].regType = type;
	iVars[idx].get.i = get;
	iVars[idx].set.i = set;
}
void drawVmInit(void) {
	componentListInit(DRAW_VM, struct DrawVm);
	setNotifier(DRAW_VM, drawVmNotifier, NULL);
	componentListInit(DRAW_VM_VM, struct IchigoVm);
	setNotifier(DRAW_VM_VM, drawVmVmNotifier, NULL);
	componentListInit(DRAW_VM_LOCALS, struct IchigoLocals);

	addUpdate(UPDATE_UI, drawVmUpdateAll, NULL);
	for (int i = 0; i < DVM_N_LAYERS; i++) {
		/* Priority = layer * 100 except for layer 0 which has priority 1 */
		addDrawUpdate(i? i * 100 : 1, drawVmDrawLayer, (void *)(uintptr_t)i);
	}

	setVar(23, REG_INT, i_getVarFLAGS, i_setVarFLAGS);
	setVar(24, REG_INT, i_getVarLANG, NULL);
	setVar(25, REG_INT, i_getVarAPI, NULL);
	setVar(30, REG_INT, i_getVarANIM_FLAGS, i_setVarANIM_FLAGS);
	setVar(31, REG_FLOAT, i_getVarANIM_LENGTH, NULL);
	setVar(32, REG_FLOAT, i_getVarGF0, i_setVarGF0);
	setVar(33, REG_FLOAT, i_getVarGF1, i_setVarGF1);
	setVar(34, REG_FLOAT, i_getVarGF2, i_setVarGF2);
	setVar(35, REG_FLOAT, i_getVarGF3, i_setVarGF3);
	setVar(36, REG_FLOAT, i_getVarGF4, i_setVarGF4);
	setVar(37, REG_FLOAT, i_getVarGF5, i_setVarGF5);
	setVar(38, REG_FLOAT, i_getVarGF6, i_setVarGF6);
	setVar(39, REG_FLOAT, i_getVarGF7, i_setVarGF7);
	setVar(40, REG_INT, i_getVarWIN_W, NULL);
	setVar(41, REG_INT, i_getVarWIN_H, NULL);

	ichigoInit(&iState, "dvm");
	ichigoSetVarTable(&iState, iVars, sizeof(iVars) / sizeof(iVars[0]));
	ichigoSetInstrTable(&iState, iInstrs, sizeof(iInstrs) / sizeof(iInstrs[0]));
	ichigoBindLocals(&iState);

	vecCreate(&texList, sizeof(struct DrawVmTextureList));
	vecCreate(&poseList, sizeof(struct DrawVmPoseFileList));
}

void drawVmFini(void) {
	drawVmEndScene();
	vecDestroy(&poseList);
	vecDestroy(&texList);
	removeUpdate(UPDATE_UI, drawVmUpdateAll);
	for (int i = 0; i < DVM_N_LAYERS; i++) {
		removeDrawUpdate(i? i * 100 : 1);
	}
	componentListFini(DRAW_VM);
}
