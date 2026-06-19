#ifndef DRAW_H
#define DRAW_H

#include <basics.h>
#include "texture.h"
#include "anim.h"
#include <vec.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRAW_MAX_TEX 8
#define DRAW_MAX_POINTLIGHTS 8

enum BlendMode {
	BLEND_ALPHA,
	BLEND_MULTIPLY,
	BLEND_ADD,
	BLEND_SUBTRACT,
	BLEND_REPLACE,
	BLEND_SCREEN,

	BLEND_N
};

enum ColorMode {
	COLOR1,
	COLOR2,
	COLOR_LR,
	COLOR_UD,

	COLOR_INOUT = COLOR_LR
};

enum CullMode {
	CULL_NONE,
	CULL_BACK,
	CULL_FRONT
};

enum DepthStencilMode {
	DEPTH_STENCIL_DISABLE,
	DEPTH_STENCIL_DEPTH,
	DEPTH_STENCIL_DEPTH_NO_WRITE
};

enum SamplerMode {
	SAMPLER_LINEAR,
	SAMPLER_POINT
};

enum ProjectionMode {
	PROJECTION_IDENT,
	PROJECTION_PERSPECTIVE,
	PROJECTION_ORTHO,
	PROJECTION_CUSTOM
};

enum StdShader {
	SHADER_2D,
	SHADER_3D,
	SHADER_3D_CLIP,
	SHADER_3D_ANIM,
	SHADER_3D_POST,
	SHADER_STD_N,
};

//#define DRAW_STD_UNIFORM_MODEL		0
//#define DRAW_STD_UNIFORM_VIEW		1
//#define DRAW_STD_UNIFORM_PROJ		2
//#define DRAW_STD_UNIFORM_NORMMAT	3
//#define DRAW_STD_UNIFORM_COLBLEND	4
//#define DRAW_STD_UNIFORM_NTEX		5
//#define DRAW_STD_UNIFORM_TEX0		6
//#define DRAW_STD_UNIFORM_TEXOFF0	(DRAW_STD_UNIFORM_TEX0 + DRAW_MAX_TEX)
//#define DRAW_STD_UNIFORM_TEXSCALE0	(DRAW_STD_UNIFORM_TEXOFF0 + DRAW_MAX_TEX)
//#define DRAW_STD_UNIFORM_TEXBLEND0	(DRAW_STD_UNIFORM_TEXSCALE0 + DRAW_MAX_TEX)


struct DwTexture {
	struct Texture *tex;
	float x, y, xs, ys;
};

struct DrawPass;

struct DwState {
	Mat matStack[16];
	int matStackIdx;

	Mat normMat;
	bool normMatValid;

	struct Shader *shader;

	enum BlendMode blend;
	struct DwTexture tex[DRAW_MAX_TEX];
	int nTex;

	enum ColorMode colMode;
	float col1[4];
	float col2[4];

	bool zWrite;
	bool uvModelMat;
	bool wireframe;
	bool cullInvert;

	float srcX, srcY, srcW, srcH;

	bool hasBuffer;

	int currentPass;
	int nPasses;
	struct DrawPass *passes;

	int totalFlushes;
};

extern struct DwState drawState;
extern unsigned int windowWidth;
extern unsigned int windowHeight;

#define DRAW_PASS_FLAG_Z_BUFFER 1
#define DRAW_PASS_FLAG_FRUSTUM_CULL 2
#define DRAW_PASS_FLAG_NORMAL_MATRIX 4
#define DRAW_PASS_FLAG_SCENE_CONSTANTS 8
#define DRAW_PASS_FLAG_3D_ROTATION 16
#define DRAW_PASS_FLAG_GAMMA 32

#define DRAW_PASS_FLAG_3D (DRAW_PASS_FLAG_Z_BUFFER | DRAW_PASS_FLAG_FRUSTUM_CULL | DRAW_PASS_FLAG_NORMAL_MATRIX | DRAW_PASS_FLAG_SCENE_CONSTANTS | DRAW_PASS_FLAG_3D_ROTATION | DRAW_PASS_FLAG_GAMMA)

struct Light;

struct DrawProjPerspective {
	float fovy;
	float nr;
	float fr;
};
struct DrawProjOrtho {
	float l, r, t, b, n, f;
};

struct DrawPass {
	bool active;
	Mat *viewMatrix;
	Vec3 *camPosition;
	uint32_t flags;
	enum StdShader stdShader;

	enum CullMode cullMode;
	enum DepthStencilMode depthStencilMode;
	enum SamplerMode samplerMode;
	enum ProjectionMode projectionMode;

	union {
		struct DrawProjPerspective perspective;
		struct DrawProjOrtho ortho;
		Mat *custom;
	} proj;

	int target; /* -1: backbuffer, >=0: RTT */
	int viewportX, viewportY;
	int viewportW, viewportH;

	struct Light *dirLight;
	struct Light *pointLights;

	uint32_t fogColor;
	float fogMin, fogMax;

	void (*draw)(struct DrawPass *pass);
	void *user;
};


struct Shader;



/* Shader stuff */
struct Shader *drawShaderNew(const char *vert, const char *frag);
void drawShaderDelete(struct Shader *s);
void drawShaderUse(struct Shader *s);
void drawShaderUseStd(enum StdShader s);

void drawSetVsync(int mode);

/* Reset the current state to basic settings */
void drawReset(void);

/* Flush currently queued vertices */
void drawFlush(void);

/*
 * TRANSFORM/MATRIX STUFF
 */
void drawTranslate3D(float x, float y, float z);
static inline void drawTranslate(float x, float y) {
	drawTranslate3D(x, y, 0);
}

void drawRotateX(float r);
void drawRotateY(float r);
void drawRotateZ(float r);
void drawRotateXYZ(float rx, float ry, float rz);
static inline void drawRotate(float r) {
	drawRotateZ(r);
}

void drawScale3D(float sx, float sy, float sz);
static inline void drawScale(float sx, float sy) {
	drawScale3D(sx, sy, 1);
}

void drawTransform(struct Transform *tf);
void drawTransformRounded(struct Transform *tf);
void drawTransformRotation(struct Transform *tf);
void drawTransform3D(struct Transform *t);

void drawMatIdentity(void);

void drawSetMatrix(float *mat);

void drawPushMat(void);
void drawPopMat(void);

/*
 * PROPERTIES
 */
void drawTexture(int slot, struct Texture *tex);
void drawTextureOffsetScale(int slot, float x, float y, float xs, float ys);
struct Texture *drawGetFboTexture(int which);

void drawBlend(enum BlendMode blend);
void drawShaderArgs(int n, float *args);

void drawSrcRect(int x, int y, int w, int h);
void drawColor(float r, float g, float b, float a);
void drawColor2(float r, float g, float b, float a);
void drawColorMode(enum ColorMode mode);

void drawZBufferWrite(bool enable);
void drawUvModelMat(bool enable);
void drawWireframe(bool wf);
void drawCullInvert(bool invert);

/*
 * DRAWING
 */
void drawPreflush(int nverts, int nindices);
void drawVertex3D(float x, float y, float z, float nx, float ny, float nz, float u, float v, float r, float g, float b, float a);
static inline void drawVertex(float x, float y, float z, float u, float v, float r, float g, float b, float a) {
	drawVertex3D(x, y, z, 0, 0, 1, u, v, r, g, b, a);
}

void drawIndices(int verts, int n, const unsigned int *lst);

void drawClear(uint32_t color);

/*
 * Shapes
 */
void drawRect(float w, float h);
void drawRectBillboard(float w, float h);
void drawEllipse(int nPoints, float w, float h);
void drawArc(int nPoints, float rStart, float r, float w1, float w2);
void drawSkybox(void);


/* Global control */
void drawGetMonitorResolution(int *w, int *h);
void drawSetResolution(int w, int h);
void drawSetPipeline(int nPasses, struct DrawPass *passes);
void drawFullFrame(void);
void drawSetPass2D(struct DrawPass *pass);
void drawSetPass3D(struct DrawPass *pass);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
