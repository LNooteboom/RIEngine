#include <gfx/draw.h>
#include <SDL2/SDL.h>
#include <assets.h>
#include <mem.h>
#include <string.h>
#include <gfx/ttf.h>
#include <gfx/drawvm.h>
#include "gfx.h"

float cam3DX, cam3DY, cam3DZ;
float cam3DRX, cam3DRY, cam3DRZ;
float cam3DFov = DEG2RAD(70);
float camX;
float camY;
uint32_t clearColor;
unsigned int winW;
unsigned int winH;
unsigned int realWinW;
unsigned int realWinH;

int rttX, rttY;
unsigned int rttW = 640, rttH = 480;
unsigned int rttIntW = 640, rttIntH = 480;

struct Light dirLight;
struct Light pointLights[DRAW_MAX_POINTLIGHTS];

uint32_t fogColor;
float fogMin, fogMax;
int drawFlushes;

Mat cam3DMatrix;
Vec3 cam3DPos;

static struct HashTable modelTable;


/*
 * MATRIX
 */

void cam3DLook(float x, float y, float z, float dx, float dy, float dz, float upx, float upy, float upz) {
	cam3DMatrix = Mat::look(Vec3{ x, y, z }, Vec3{ dx, dy, dz }, Vec3{ upx, upy, upz });
	cam3DPos = Vec3{ x, y, z };
}
void cam3DRotate(float x, float y, float z, float rx, float ry, float rz) {
	Mat m{ 1.0f };
	m.translate(Vec3{ -x, -y, -z });
	cam3DMatrix = (Mat::fromRotation(Vec4::eulerAngles(rx, 0, 0)) * Mat::fromRotation(Vec4::eulerAngles(0, ry, 0)) * Mat::fromRotation(Vec4::eulerAngles(0, 0, rz))) * m;
	cam3DPos = Vec3{ x, y, z };
}

void camReset(void) {
	cam3DLook(0, 0, 0, 0, 1, 0, 0, 0, 1);
	cam3DFov = DEG2RAD(70);
	clearColor = 0;
	camX = camY = 0;
	fogMin = 4096.0f;
	fogMax = 8192.0f;
	fogColor = 0xFFFFFF;
}

void drawTranslate3D(float x, float y, float z) {
	drawState.matStack[drawState.matStackIdx].translate(Vec3{ x, y, z });
}

void drawRotateX(float r) {
	if (drawState.drawPhase <= DP_3D_NO_CULL)
		r = -r;
	float c = cosf(r), s = sinf(r);
	Mat m{ 1.0f };
	m.m[5] = c;
	m.m[6] = s;
	m.m[9] = -s;
	m.m[10] = c;
	drawState.matStack[drawState.matStackIdx] = drawState.matStack[drawState.matStackIdx] * m;
	drawState.normMatValid = false;
}
void drawRotateY(float r) {
	if (drawState.drawPhase <= DP_3D_NO_CULL)
		r = -r;
	float c = cosf(r), s = sinf(r);
	Mat m{ 1.0f };
	m.m[0] = c;
	m.m[2] = -s;
	m.m[8] = s;
	m.m[10] = c;
	drawState.matStack[drawState.matStackIdx] = drawState.matStack[drawState.matStackIdx] * m;
	drawState.normMatValid = false;
}
void drawRotateZ(float r) {
	if (drawState.drawPhase <= DP_3D_NO_CULL)
		r = -r;
	float c = cosf(r), s = sinf(r);
	Mat m{ 1.0f };
	m.m[0] = c;
	m.m[1] = s;
	m.m[4] = -s;
	m.m[5] = c;
	drawState.matStack[drawState.matStackIdx] = drawState.matStack[drawState.matStackIdx] * m;
	drawState.normMatValid = false;
}
void drawRotateXYZ(float rx, float ry, float rz) {
	float r[3];
	if (drawState.drawPhase <= DP_3D_NO_CULL) {
		r[0] = -rx;
		r[1] = -ry;
		r[2] = -rz;
	} else {
		r[0] = rx;
		r[1] = ry;
		r[2] = rz;
	}
	drawState.matStack[drawState.matStackIdx].rotate(Vec4::eulerAngles(r[0], r[1], r[2]));
	drawState.normMatValid = false;
}

void drawScale3D(float sx, float sy, float sz) {
	drawState.matStack[drawState.matStackIdx].scale(Vec3{ sx, sy, sz });
	drawState.normMatValid = false;
}

void drawTransform(struct Transform *tf) {
	drawState.matStack[drawState.matStackIdx] = Mat::fromTranslation(Vec3::fromTfPos(*tf));
	drawState.normMatValid = false;

}
void drawTransformRounded(struct Transform *tf) {
	Vec3 v = Vec3::fromTfPos(*tf);
	vecRound3(&v, &v);
	drawState.matStack[drawState.matStackIdx] = Mat::fromTranslation(v);
	drawState.normMatValid = false;

}
void drawTransformRotation(struct Transform *tf) {
	Mat m{ 1.0f };
	if (drawState.drawPhase <= DP_3D_NO_CULL) {
		m.m[0] = tf->rotReal;
		m.m[4] = tf->rotImag;
		m.m[1] = -tf->rotImag;
		m.m[5] = tf->rotReal;
	} else {
		m.m[0] = tf->rotReal;
		m.m[4] = -tf->rotImag;
		m.m[1] = tf->rotImag;
		m.m[5] = tf->rotReal;
	}
	drawState.matStack[drawState.matStackIdx] = drawState.matStack[drawState.matStackIdx] * m;
	drawState.normMatValid = false;
}
void drawTransform3D(struct Transform *tf) {
	drawState.matStack[drawState.matStackIdx] = Mat::fromTranslation(Vec3::fromTfPos(*tf));
	drawState.matStack[drawState.matStackIdx].rotate(Vec4::fromTfRot(*tf));
	drawState.normMatValid = false;
}

void drawMatIdentity(void) {
	drawState.matStack[drawState.matStackIdx] = Mat{ 1.0f };
	drawState.normMatValid = false;
}

void drawSetMatrix(float *mat) {
	for (int i = 0; i < 16; i++) {
		drawState.matStack[drawState.matStackIdx].m[i] = mat[i];
	}
	drawState.normMatValid = false;
}

void drawPushMat(void) {
	drawState.matStackIdx += 1;
	drawState.matStack[drawState.matStackIdx] = drawState.matStack[drawState.matStackIdx - 1];
}
void drawPopMat(void) {
	drawState.matStackIdx -= 1;
	drawState.normMatValid = false;
}


/*
 * Shapes
*/

void drawRect(float w, float h) {
	float *col[4];
	switch (drawState.colMode) {
	default:
	case COLOR1:
		col[0] = drawState.col1;
		col[1] = drawState.col1;
		col[2] = drawState.col1;
		col[3] = drawState.col1;
		break;
	case COLOR2:
		col[0] = drawState.col2;
		col[1] = drawState.col2;
		col[2] = drawState.col2;
		col[3] = drawState.col2;
		break;
	case COLOR_LR:
		col[0] = drawState.col1;
		col[1] = drawState.col2;
		col[2] = drawState.col1;
		col[3] = drawState.col2;
		break;
	case COLOR_UD:
		col[0] = drawState.col1;
		col[1] = drawState.col1;
		col[2] = drawState.col2;
		col[3] = drawState.col2;
		break;
	}

	drawPreflush(4, 6);

	float w2 = w / 2, h2 = h / 2;
	drawVertex(-w2, -h2, 0, drawState.srcX, drawState.srcY, col[0][0], col[0][1], col[0][2], col[0][3]);
	drawVertex(+w2, -h2, 0, drawState.srcX + drawState.srcW, drawState.srcY, col[1][0], col[1][1], col[1][2], col[1][3]);
	drawVertex(-w2, +h2, 0, drawState.srcX, drawState.srcY + drawState.srcH, col[2][0], col[2][1], col[2][2], col[2][3]);
	drawVertex(+w2, +h2, 0, drawState.srcX + drawState.srcW, drawState.srcY + drawState.srcH, col[3][0], col[3][1], col[3][2], col[3][3]);

	const unsigned int indices[6] = { 2, 3, 0, 3, 1, 0 };
	drawIndices(4, 6, indices);
}

void drawRectBillboard(float w, float h) {
	float *col[4];
	switch (drawState.colMode) {
	default:
	case COLOR1:
		col[0] = drawState.col1;
		col[1] = drawState.col1;
		col[2] = drawState.col1;
		col[3] = drawState.col1;
		break;
	case COLOR2:
		col[0] = drawState.col2;
		col[1] = drawState.col2;
		col[2] = drawState.col2;
		col[3] = drawState.col2;
		break;
	case COLOR_LR:
		col[0] = drawState.col1;
		col[1] = drawState.col2;
		col[2] = drawState.col1;
		col[3] = drawState.col2;
		break;
	case COLOR_UD:
		col[0] = drawState.col1;
		col[1] = drawState.col1;
		col[2] = drawState.col2;
		col[3] = drawState.col2;
		break;
	}

	drawPreflush(4, 6);

	float rx = cam3DMatrix.m[0], ry = cam3DMatrix.m[4], rz = cam3DMatrix.m[8];
	float ux = cam3DMatrix.m[1], uy = cam3DMatrix.m[5], uz = cam3DMatrix.m[9];

	float w2 = w / 2, h2 = h / -2;
	drawVertex(rx * -w2 + ux * -h2, ry * -w2 + uy * -h2, rz * -w2 + uz * -h2, drawState.srcX, drawState.srcY, col[0][0], col[0][1], col[0][2], col[0][3]);
	drawVertex(rx * +w2 + ux * -h2, ry * +w2 + uy * -h2, rz * +w2 + uz * -h2, drawState.srcX + drawState.srcW, drawState.srcY, col[1][0], col[1][1], col[1][2], col[1][3]);
	drawVertex(rx * -w2 + ux * +h2, ry * -w2 + uy * +h2, rz * -w2 + uz * +h2, drawState.srcX, drawState.srcY + drawState.srcH, col[2][0], col[2][1], col[2][2], col[2][3]);
	drawVertex(rx * +w2 + ux * +h2, ry * +w2 + uy * +h2, rz * +w2 + uz * +h2, drawState.srcX + drawState.srcW, drawState.srcY + drawState.srcH, col[3][0], col[3][1], col[3][2], col[3][3]);

	const unsigned int indices[6] = { 2, 0, 3, 3, 0, 1 };
	drawIndices(4, 6, indices);
}

void drawEllipse(int nPoints, float w, float h) {
	if (nPoints < 3)
		return;
	float *col[2];
	switch (drawState.colMode) {
	default:
	case COLOR1:
		col[0] = drawState.col1;
		col[1] = drawState.col1;
		break;
	case COLOR2:
		col[0] = drawState.col2;
		col[1] = drawState.col2;
		break;
	case COLOR_INOUT:
		col[0] = drawState.col1;
		col[1] = drawState.col2;
		break;
	}

	float w2 = w / 2.0f, h2 = h / 2.0f;
	float sw2 = drawState.srcW / 2.0f, sh2 = drawState.srcH / 2.0f;
	float smx = drawState.srcX + sw2, smy = drawState.srcY + sh2; /* Middle point of srcRect */

	unsigned int *indices = static_cast<unsigned int *>(stackAlloc(nPoints * 3 * sizeof(unsigned int)));

	/* Middle point vertex */
	drawVertex(0, 0, 0, drawState.srcX + sw2, drawState.srcY + sh2, col[0][0], col[0][1], col[0][2], col[0][3]);

	float ang = -PI / 2;
	for (int i = 0; i < nPoints; i++) {
		float s = sinf(ang), c = cosf(ang);
		drawVertex(c * w2, s * h2, 0, c * sw2 + smx, s * sh2 + smy, col[1][0], col[1][1], col[1][2], col[1][3]);

		indices[i * 3 + 0] = i + 1;
		indices[i * 3 + 1] = i == nPoints - 1 ? 1 : i + 2;
		indices[i * 3 + 2] = 0;

		ang += (2 * PI) / nPoints;
	}
	drawIndices(nPoints + 1, nPoints * 3, indices);
	stackDealloc(nPoints * 3 * sizeof(unsigned int));
}

void drawArc(int nPoints, float rStart, float r, float w1, float w2) {
	if (nPoints < 2)
		return;
	float *col[2];
	switch (drawState.colMode) {
	default:
	case COLOR1:
		col[0] = drawState.col1;
		col[1] = drawState.col1;
		break;
	case COLOR2:
		col[0] = drawState.col2;
		col[1] = drawState.col2;
		break;
	case COLOR_INOUT:
		col[0] = drawState.col1;
		col[1] = drawState.col2;
		break;
	}

	float rr, ri, rdr, rdi;
	int nPoints1 = nPoints - 1;
	ANGLE2C(rr, ri, rStart);
	ANGLE2C(rdr, rdi, r / nPoints1);
	unsigned int iSize = nPoints1 * 6 * sizeof(int);
	unsigned int *ind = static_cast<unsigned int *>(stackAlloc(iSize));
	for (int i = 0; i < nPoints; i++) {
		float v = lerp(drawState.srcY, drawState.srcY + drawState.srcH, (float)i / nPoints1);
		drawVertex(rr * w1, ri * w1, 0, drawState.srcX, v, col[0][0], col[0][1], col[0][2], col[0][3]); /* Inner */
		drawVertex(rr * (w1 + w2), ri * (w1 + w2), 0, drawState.srcX + drawState.srcW, v, col[1][0], col[1][1], col[1][2], col[1][3]); /* Outer */

		if (i != nPoints1) {
			ind[i * 6 + 0] = (i * 2) + 1;
			ind[i * 6 + 1] = (i * 2) + 0;
			ind[i * 6 + 2] = (i * 2) + 2;
			ind[i * 6 + 3] = (i * 2) + 3;
			ind[i * 6 + 4] = (i * 2) + 1;
			ind[i * 6 + 5] = (i * 2) + 2;
		}

		CMUL(rr, ri, rr, ri, rdr, rdi);
	}
	drawIndices(nPoints * 2, nPoints1 * 6, ind);
	stackDealloc(iSize);
}

void drawSkybox(void) {
	drawVertex(-1, -1, -1, 0, 0, 0, 0, 0, 1);
	drawVertex(+1, -1, -1, 0, 0, 0, 0, 0, 1);
	drawVertex(-1, +1, -1, 0, 0, 0, 0, 0, 1);
	drawVertex(+1, +1, -1, 0, 0, 0, 0, 0, 1);
	drawVertex(-1, -1, +1, 0, 0, 0, 0, 0, 1);
	drawVertex(+1, -1, +1, 0, 0, 0, 0, 0, 1);
	drawVertex(-1, +1, +1, 0, 0, 0, 0, 0, 1);
	drawVertex(+1, +1, +1, 0, 0, 0, 0, 0, 1);
	const unsigned int indices[36] = {
		2,6,7, 2,3,7,
		0,4,5, 0,1,5,
		0,2,6, 0,4,6,
		1,3,7, 1,5,7,
		0,2,3, 0,1,3,
		4,6,7, 4,5,7
	};
	drawIndices(8, 36, indices);
}

void drawGetMonitorResolution(int *w, int *h) {
	int w2 = 0, h2 = 0;
	for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
		SDL_DisplayMode dm;
		SDL_GetDesktopDisplayMode(i, &dm);
		if (dm.w > w2)
			w2 = dm.w;
		if (dm.h > h2)
			h2 = dm.h;
	}
	*w = w2;
	*h = h2;

}


void drawSrcRect(int x, int y, int w, int h) {
	if (drawState.nTex && drawState.tex[0].tex) {
		struct Texture *t = drawState.tex[0].tex;
		drawState.srcX = (float)x / t->w;
		drawState.srcY = (float)y / t->h;
		drawState.srcW = (float)w / t->w;
		drawState.srcH = (float)h / t->h;
	} else {
		drawState.srcX = x;
		drawState.srcY = y;
		drawState.srcW = w;
		drawState.srcH = h;
	}
}

void drawColor(float r, float g, float b, float a) {
	drawState.col1[0] = r;
	drawState.col1[1] = g;
	drawState.col1[2] = b;
	drawState.col1[3] = a;
}

void drawColor2(float r, float g, float b, float a) {
	drawState.col2[0] = r;
	drawState.col2[1] = g;
	drawState.col2[2] = b;
	drawState.col2[3] = a;
}

void drawColorMode(enum ColorMode mode) {
	drawState.colMode = mode;
}


void drawReset(void) {
	drawFlush();

	drawState.matStackIdx = 0;
	drawMatIdentity();

	if (drawState.drawPhase <= DP_3D_OVERLAY) {
		drawShaderUseStd(SHADER_3D);
	} else {
		drawShaderUseStd(SHADER_2D);
	}

	for (int i = 0; i < drawState.nTex; i++) {
		//drawTexture(i, NULL);
		drawTextureOffsetScale(i, 0, 0, 1, 1);
	}
	drawBlend(BLEND_ALPHA);
	drawState.nTex = 0;

	drawSrcRect(0, 0, 1, 1);
	drawColor(1, 1, 1, 1);
	drawColor2(1, 1, 1, 1);
	drawColorMode(COLOR1);
	drawZBufferWrite(true);
	drawUvModelMat(false);
	drawWireframe(false);
	drawCullInvert(false);
}

/*
 * MODEL
 */

static const char unexpectedEOFMsg[] = "Failed to read %s: Unexpected end of file encountered\n";
int loadModelFile(const char *name) {
	logDebug("Loading model file: %s\n", name);

	/* Open the file and check signature */
	int error = 0;
	struct Asset *a = assetOpen(name);
	if (!a) {
		logDebug("Failed to open model file %s\n", name);
		return -1;
	}

	struct ModelFileHeader header;
	if (assetRead(a, &header, sizeof(header)) != sizeof(header)) {
		error = -1;
		fail(unexpectedEOFMsg, name);
		goto closef;
	}
	if (memcmp(&header.sig, MODEL_FILE_SIG, 4) || header.nEntries == 0) {
		error = -1;
		fail("Failed to read %s: file is corrupted\n", name);
		goto closef;
	}

	for (unsigned int i = 0; i < header.nEntries; i++) {
		struct ModelFileEntry entry;
		if (assetRead(a, &entry, sizeof(entry)) != sizeof(entry)) {
			error = -1;
			fail(unexpectedEOFMsg, name);
			goto closef;
		}

		struct Model *m = new Model;
		m->en.key = m->name;
		strncpy(m->name, entry.name, MODEL_NAME_LEN);
		m->name[MODEL_NAME_LEN - 1] = 0;
		HTAdd(&modelTable, &m->en);
		logDebug("Model entry: %s\n", m->name);
		m->flags = entry.flags;
		m->nTriangles = entry.nTriangles;
		m->nVertices = entry.nVertices;

		/* Read vertices */
		uint32_t vboPitch = sizeof(float) * 12;
		if (entry.flags & MODEL_FILE_ANIM) {
			vboPitch += 4;
		}
		size_t vboSize = m->nVertices * vboPitch;
		m->pitch = vboPitch;
		m->verts = (float *)globalAlloc(vboSize);
		assetRead(a, m->verts, vboSize);

		Vec3 aabbMin{ 999999.0f };
		Vec3 aabbMax{ -999999.0f };
		for (uint32_t j = 0; j < m->nVertices; j++) {
			float *v = (float *)((char *)m->verts + j * vboPitch);
			aabbMin.x = fminf(aabbMin.x, v[0]);
			aabbMin.y = fminf(aabbMin.y, v[1]);
			aabbMin.z = fminf(aabbMin.z, v[2]);
			aabbMax.x = fmaxf(aabbMax.x, v[0]);
			aabbMax.y = fmaxf(aabbMax.y, v[1]);
			aabbMax.z = fmaxf(aabbMax.z, v[2]);
		}
		m->aabbCenter = (aabbMin + aabbMax) * 0.5f;
		m->aabbHalfExtent = aabbMax - m->aabbCenter;

		size_t eboSize = m->nTriangles * 4ULL * 3;
		m->indices = (uint32_t *)globalAlloc(eboSize);
		assetRead(a, m->indices, eboSize);

		uploadModel(m, m->verts, m->indices);
	}

closef:
	assetClose(a);
	return error;
}

void clearModels(void) {
	for (unsigned int bu = 0; bu < modelTable.nBuckets; bu++) {
		struct Model *next = NULL;
		for (struct Model *m = (struct Model *)modelTable.bu[bu].list.first; m; m = next) {
			next = (struct Model *)m->en._llEntry.next;
			HTDelete(&modelTable, &m->en);
			deleteModel(m);
			delete m;
		}
	}
}

struct Model *getModel(const char *name) {
	struct Model *ret = (struct Model *)HTGet(&modelTable, name);
	if (!ret) {
		logNorm("Model %s does not exist\n", name);
	}
	return ret;
}

void clearLights(void) {
	memset(&dirLight, 0, sizeof(dirLight));
	memset(&pointLights[0], 0, sizeof(struct Light) * DRAW_MAX_POINTLIGHTS);
}

static const float lightLinear[LIGHT_N] = { 0.22f, 0.14f, 0.09f, 0.07f, 0.045f, 0.027f };
static const float lightQuad[LIGHT_N] = { 0.20f, 0.07f, 0.032f, 0.017f, 0.0075f, 0.0028f };
void setLight(struct Light *l, float x, float y, float z, uint32_t color, float ambient, float diffuse, float specular, enum LightStrength strength) {
	float r = ((color >> 16) & 0xFF) / 255.0f;
	float g = ((color >> 8) & 0xFF) / 255.0f;
	float b = (color & 0xFF) / 255.0f;
	l->x = x;
	l->y = y;
	l->z = z;
	l->ambientR = r * ambient;
	l->ambientG = g * ambient;
	l->ambientB = b * ambient;
	l->diffuseR = r * diffuse;
	l->diffuseG = g * diffuse;
	l->diffuseB = b * diffuse;
	l->specularR = r * specular;
	l->specularG = g * specular;
	l->specularB = b * specular;
	l->constant = 1.0f;
	l->linear = lightLinear[strength];
	l->quadratic = lightQuad[strength];
}

extern "C" {
	void drawInit(void);
	void drawFini(void);
}

void drawInit(void) {
	drawDriverInit();
	HTCreate(&modelTable, 128);

	rttW = rttIntW = winW = realWinW;
	rttH = rttIntH = winH = realWinH;
	rttX = 0;
	rttY = winH / 2;

	anim3DInit();
	drawVmInit();
	ttfInit();

	cam3DRotate(0, 0, 10, 0, 0, 0);
}
void drawFini(void) {
	drawDriverFini();
	ttfFini();
	drawVmFini();
	anim3DFini();
	HTDestroy(&modelTable);
}

void tfSetRotation3D(struct Transform *tf, float rx, float ry, float rz) {
	Vec4 v = Vec4::eulerAngles(rx, ry, rz);
	tf->rx = v.x;
	tf->ry = v.y;
	tf->rz = v.z;
	tf->rw = v.w;
}