#ifndef DRAWVM_H
#define DRAWVM_H

#include <ecs.h>
#include <ich.h>
#include <basics.h>
#include <gfx/draw.h>

#ifdef __cplusplus
extern "C" {
#endif

enum DrawVmState {
	DVM_STOPPED,
	DVM_RUNNING,
	DVM_STATIC,
	DVM_DELETED
};
enum DrawVmMode {
	DVM_NONE,
	DVM_RECT,
	DVM_MODEL,
	DVM_ELLIPSE,
	DVM_ARC,
	DVM_ANIM,
	DVM_PLANE,
	DVM_BILLBOARD,
	DVM_ANIM_PARENT
};
enum DrawVmRotMode {
	DVM_ROT_XYZ,
	DVM_ROT_XZY,
	DVM_ROT_YXZ,
	DVM_ROT_YZX,
	DVM_ROT_ZXY,
	DVM_ROT_ZYX
};
enum DrawVmInterpTarget {
	DVM_INTERP_NONE,
	DVM_INTERP_OFFS,
	DVM_INTERP_ANCHOR,
	DVM_INTERP_SCALE,
	DVM_INTERP_ROT,
	DVM_INTERP_GROW,
	DVM_INTERP_WVEL,
	DVM_INTERP_COL1,
	DVM_INTERP_COL2,
	DVM_INTERP_ALPHA1,
	DVM_INTERP_ALPHA2,
	DVM_INTERP_SRC_OFFS,
	DVM_INTERP_SRC_SCALE,

	DVM_INTERP_TEX0_OFFS,
	DVM_INTERP_TEX0_SCALE = (DVM_INTERP_TEX0_OFFS + DRAW_MAX_TEX),
	DVM_INTERP_TEX0_SCROLL = (DVM_INTERP_TEX0_SCALE + DRAW_MAX_TEX),

	DVM_INTERP_N = (DVM_INTERP_TEX0_SCROLL + DRAW_MAX_TEX)
};
#define DVM_EVENT_CREATE 0
#define DVM_EVENT_DELETE 1
#define DVM_EVENT_ANIM 2

#define DVM_FLAG_INVISIBLE 1
#define DVM_FLAG_FLIP_SRC_X 2
#define DVM_FLAG_FLIP_SRC_Y 4
#define DVM_FLAG_ANIM_EVENT 8
#define DVM_FLAG_NO_Z_BUFFER_WRITE 16
#define DVM_FLAG_NO_CHILD_EVENT 32
#define DVM_FLAG_TF_ROTATION 64
#define DVM_FLAG_UV_MODEL 128
#define DVM_FLAG_ALPHA_CHILDREN 256
#define DVM_FLAG_COLOR_CHILDREN 512
#define DVM_FLAG_TF_3D_ROTATION 1024
#define DVM_FLAG_SHADER_ARGS 2048
#define DVM_FLAG_NO_FCULL 4096
#define DVM_FLAG_ROUNDED_POS 8192
#define DVM_FLAG_CULL_FRONT 16384
#define DVM_FLAG_DELETE_ALL (1 << 29)
#define DVM_FLAG_TTF (1 << 30)

#define DVM_N_LAYERS 48
#define DVM_N_INTERPS 4
#define DVM_MAX_CHILDREN 8

typedef struct Shader *DrawVmCustomShaderGetter(void *arg, int idx);

struct DrawVmTextureScroll {
	float x, y;
};

struct DrawVm {
	entity_t entity;
	const char *mainFn;

	entity_t parent;
	entity_t childStart, childEnd;
	entity_t childNext, childPrev;
	int nChildren;

	enum DrawVmState state;
	int flags;

	struct DrawVm *layerNext;
	int layer;

	float xOffs, yOffs, zOffs;

	int anchor;

	float xScale, yScale, zScale;
	float xRot, yRot, zRot;
	enum DrawVmRotMode rotMode;

	float xGrow, yGrow, zGrow;
	float xWVel, yWVel, zWVel;

	int nPoints; /* For ellipse and ring */
	enum DrawVmMode mode;

	struct Model *model;

	enum StdShader stdShader;
	struct Shader *customShader;

	struct DwTexture tex[DRAW_MAX_TEX];
	struct DrawVmTextureScroll texScroll[DRAW_MAX_TEX];

	enum BlendMode blend;
	enum ColorMode colMode;
	float col1[4];
	float col2[4];

	float srcX, srcY, srcW, srcH;

	enum DrawVmInterpTarget interpTargets[DVM_N_INTERPS];
	struct Interp interps[DVM_N_INTERPS];
	float interpEx1[3];
	float interpEx2[3];
};

extern int drawVmUpdateSkip;
extern int drawVmLanguage;
extern int drawVmCount;
extern float drawVmGlobalsF[8];

struct DrawVm *drawVmNew(entity_t entity, const char *fn);
struct DrawVm *drawVmNewChild(struct DrawVm *parent, const char *fn);
void drawVmDelete(entity_t entity);
void drawVmEvent(struct DrawVm *d, int event);
void drawVmEvent2(entity_t entity, int event);
void drawVmEventAll(int event);
void drawVmTexture(struct DrawVm *d, int slot, const char *texture);
void drawVmInterp(struct DrawVm *d, int slot, int target, float time, int mode, float a, float b, float c);
void drawVmAddFile(const char *file);

#ifdef __cplusplus
} // extern "C"
#endif

#endif