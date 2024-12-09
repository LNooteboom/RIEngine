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

enum StdShader {
	SHADER_2D,
	SHADER_3D,
	SHADER_3D_CLIP,
	SHADER_3D_ANIM,
	SHADER_3D_POST,
	SHADER_STD_N,
};

enum DrawPhase {
	DP_3D_BG,
	DP_3D,
	DP_3D_NO_CULL,
	DP_3D_OVERLAY,
	DP_2D_LOWRES,
	DP_2D_HIRES,
	DP_BACKBUFFER
};

enum LightStrength {
	LIGHT_MINI,
	LIGHT_TINY,
	LIGHT_SMALL,
	LIGHT_MEDIUM,
	LIGHT_LARGE,
	LIGHT_HUGE,

	LIGHT_N
};

#define DRAW_STD_UNIFORM_MODEL		0
#define DRAW_STD_UNIFORM_VIEW		1
#define DRAW_STD_UNIFORM_PROJ		2
#define DRAW_STD_UNIFORM_NORMMAT	3
#define DRAW_STD_UNIFORM_COLBLEND	4
#define DRAW_STD_UNIFORM_NTEX		5
#define DRAW_STD_UNIFORM_TEX0		6
#define DRAW_STD_UNIFORM_TEXOFF0	(DRAW_STD_UNIFORM_TEX0 + DRAW_MAX_TEX)
#define DRAW_STD_UNIFORM_TEXSCALE0	(DRAW_STD_UNIFORM_TEXOFF0 + DRAW_MAX_TEX)
#define DRAW_STD_UNIFORM_TEXBLEND0	(DRAW_STD_UNIFORM_TEXSCALE0 + DRAW_MAX_TEX)


#define MODEL_FILE_VCOL		1
#define MODEL_FILE_ANIM		2
#define MODEL_FILE_SIG		"MES0"
#define MODEL_NAME_LEN		64

struct ModelFileHeader {
	char sig[4];
	uint32_t nEntries;
};

struct ModelFileEntry {
	char name[MODEL_NAME_LEN];
	uint32_t flags;
	uint32_t nVertices;
	uint32_t nTriangles;
};

struct Model {
	struct HTEntry en;
	char name[MODEL_NAME_LEN];
	int flags;
	uint32_t nTriangles;
	uint32_t nVertices;
	uint32_t pitch;

	float *verts;
	uint32_t *indices;

	Vec3 aabbCenter;
	Vec3 aabbHalfExtent;

	void *d3dVerts;
	void *d3dIndices;

	unsigned int glVao;
	unsigned int glVbo;
	unsigned int glEbo;
};

struct Light {
	float x, y, z;
	float ambientR, ambientG, ambientB;
	float diffuseR, diffuseG, diffuseB;
	float specularR, specularG, specularB;

	/* For point lights only */
	float constant;
	float linear;
	float quadratic;
};

extern struct Light dirLight;
extern struct Light pointLights[DRAW_MAX_POINTLIGHTS];
extern uint32_t fogColor;
extern float fogMin, fogMax;

struct DwTexture {
	struct Texture *tex;
	float x, y, xs, ys;
};
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

	enum DrawPhase drawPhase;
	bool hasBuffer;
};

extern struct DwState drawState;

struct Shader;

#define CAM_3D_NEAR 0.1f
#define CAM_3D_FAR 200.0f

extern float camX;
extern float camY;
extern Mat cam3DMatrix;
extern Vec3 cam3DPos;
extern float cam3DFov;
extern uint32_t clearColor;
extern unsigned int winW;
extern unsigned int winH;
extern unsigned int realWinW;
extern unsigned int realWinH;

extern int rttX, rttY;
extern unsigned int rttW, rttH;
extern unsigned int rttIntW, rttIntH;

extern int drawFlushes;

int loadModelFile(const char *name);
void clearModels(void);
struct Model *getModel(const char *name);

void drawSetSkybox(const char *texture);

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

void drawModel3D(struct Model *m);

bool drawModelInFrustum(struct Model *m);


/*
 * Shapes
 */
void drawRect(float w, float h);
void drawRectBillboard(float w, float h);
void drawEllipse(int nPoints, float w, float h);
void drawArc(int nPoints, float rStart, float r, float w1, float w2);
void drawSkybox(void);

/*
 * Lights
 */
void clearLights(void);
void setLight(struct Light *l, float x, float y, float z, uint32_t color, float ambient, float diffuse, float specular, enum LightStrength strength);


/*
 * Camera
*/

void cam3DLook(float x, float y, float z, float dx, float dy, float dz, float upx, float upy, float upz);
void cam3DRotate(float x, float y, float z, float rx, float ry, float rz);
void camReset(void);

void drawGetMonitorResolution(int *w, int *h);
void drawSetResolution(int w, int h);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
