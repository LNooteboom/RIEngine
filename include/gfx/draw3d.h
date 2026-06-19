#ifndef DRAW_3D_H
#define DRAW_3D_H

#include "draw.h"

#ifdef __cplusplus
extern "C" {
#endif

enum LightStrength {
	LIGHT_MINI,
	LIGHT_TINY,
	LIGHT_SMALL,
	LIGHT_MEDIUM,
	LIGHT_LARGE,
	LIGHT_HUGE,

	LIGHT_N
};

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

int loadModelFile(const char *name);
void clearModels(void);
struct Model *getModel(const char *name);

void drawSetSkybox(const char *texture);

/*
 * Lights
 */
void clearLights(void);
void setLight(struct Light *l, float x, float y, float z, uint32_t color, float ambient, float diffuse, float specular, enum LightStrength strength);

/* Draw a Model */
void drawModel3D(struct Model *m);

/* Returns true if this 3D model is in the frustum */
bool drawModelInFrustum(struct Model *m);

#ifdef __cplusplus
} // extern "C"
#endif

#endif