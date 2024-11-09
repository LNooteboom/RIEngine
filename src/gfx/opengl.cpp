#include <gfx/draw.h>
#include <gfx/opengl.h>
#include <mem.h>
#include <events.h>
#include <stdio.h>
#include <string.h>
#include <vec.h>
#include <assets.h>

#include <SDL2/SDL.h>

#define SHADER_BUF_SZ	0x40000

#ifdef RELEASE
#define VBO_MAXSZ		0x40000
#define EBO_MAXSZ		0x40000
#else
#define VBO_MAXSZ		0x80000
#define EBO_MAXSZ		0x80000
#endif

struct StdVbo {
	float x, y, z;
	float nx, ny, nz;
	float u, v;
};
struct StdVboColor {
	struct StdVbo s;
	float r, g, b, a;
};
struct StdVboAnim {
	struct StdVbo s;
	float weight[4];
	uint8_t bones[4];
};

static unsigned int stdVao;
static unsigned int stdVbo;
static unsigned int stdEbo;
static struct Shader stdShader2D;
static struct Shader stdShader3D;
static struct Shader stdShader3DClip;
static struct Shader post3DShader;
static struct Shader skyboxShader;
static struct Shader animShader;

static struct Texture *skyboxTexture;

static void *curVboData;
static unsigned int *curEboData;
static int curNVerts;
static int curNIndices;

static unsigned int fbo1;
static struct Texture fbo1Col, fbo1Depth;
static unsigned int fbo2;
static struct Texture fbo2Col, fbo2Depth;

static Mat cam3DOvMat;
static Mat cam2DMat;
static Mat cam2DUiMat;
static Mat proj3DMat;
static Mat identMat{ 1.0f };

static struct SDL_Window *window;
static SDL_GLContext glContext;

static bool compat31;

struct DwState drawState;

struct StdUniformVert {
	float model[16];
	float view[16];
	float projection[16];
	float normMat[16];
};
struct StdUniformFragTex {
	float offs[2];
	float scale[2];
};
struct StdUniformFragLight {
	float pos[4];
	float ambient[4];
	float diffuse[4];
	float specular[4];
	float constant;
	float linear;
	float quadratic;
	int unused[1];
};
struct StdUniformFrag {
	struct StdUniformFragLight dirLight;
	struct StdUniformFragLight pointLights[8];
	struct StdUniformFragTex tex[8];
	float glossiness;
	float fogMin, fogMax;
	int unused1;
	float fogColor[4];
};
static unsigned int stdUboVert, stdUboFrag, animUbo;

/* replace "version 330 core" with (17 chars) */
static const char compatRepl[] = "#version 140\n#extension GL_ARB_explicit_attrib_location : require";



static void doGamma(float *r, float *g, float *b) {
	*r = 0.75f * (*r * *r) + 0.25f * *r * (*r * *r);
	*g = 0.75f * (*g * *g) + 0.25f * *g * (*g * *g);
	*b = 0.75f * (*b * *b) + 0.25f * *b * (*b * *b);
}

void showError(const char *title, const char *message) {
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, window);
}

/*
 * SHADER 
 */

static unsigned int loadShader(const char *file, unsigned int type) {
	unsigned int shader;
	char *shaderBuf = static_cast<char *>(stackAlloc(SHADER_BUF_SZ));

	int offset = compat31? sizeof(compatRepl) - 1 : 0;
	int off2 = compat31 ? 17 : 0;

	/* Open shader source */
	struct Asset *a = assetOpen(file);
	if (!a) {
		fail("Could not find shader: %s\n", file);
	}
	size_t read = assetRead(a, shaderBuf + offset - off2, SHADER_BUF_SZ);
	if (read == 0 || read >= SHADER_BUF_SZ) {
		fail("Invalid shader %s size %d\n", file, read);
	}
	shaderBuf[read + offset - off2] = 0;
	assetClose(a);

	if (compat31) {
		memcpy(shaderBuf, compatRepl, sizeof(compatRepl) - 1);
	}

	/* Compile shader */
	shader = glCreateShader(type);
	glShaderSource(shader, 1, (const char *const *)&shaderBuf, NULL);
	glCompileShader(shader);
	
	/* Check for errors */
	int success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(shader, SHADER_BUF_SZ, NULL, shaderBuf);
		fail("Shader %s compilation failed:\n%s\n", file, shaderBuf);
	}

	stackDealloc(SHADER_BUF_SZ);
	return shader;
}

static unsigned int linkShaders(unsigned int vert, unsigned int frag) {
	unsigned int prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);

	char *buf = static_cast<char *>(stackAlloc(SHADER_BUF_SZ));
	int success;
	glGetProgramiv(prog, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(prog, SHADER_BUF_SZ, NULL, buf);
		fail("Shader linking failed: %s\n", buf);
	}
	stackDealloc(SHADER_BUF_SZ);
	return prog;
}

struct Shader *drawShaderNew(const char *vert, const char *frag) {
	struct Shader *s = new Shader;
	unsigned int v = loadShader(vert, GL_VERTEX_SHADER);
	unsigned int f = loadShader(frag, GL_FRAGMENT_SHADER);

	s->glShader = linkShaders(v, f);

	glDeleteShader(v);
	glDeleteShader(f);
	return s;
}

void drawShaderDelete(struct Shader *s) {
	if (s) {
		glDeleteShader(s->glShader);
		delete s;
	}
}

static unsigned int getUniform(unsigned int shaderProg, const char *name) {
	int ret = glGetUniformLocation(shaderProg, name);
	if (ret == -1) {
		logNorm("Could not find uniform: %s\n", name);
	}
	return ret;
}

void drawShaderUse(struct Shader *s) {
	if (drawState.shader != s) {
		drawFlush();
		drawState.shader = s;
		glUseProgram(s->glShader);
	}
}
void drawShaderUseStd(enum StdShader s) {
	switch (s) {
	case SHADER_2D:
		drawShaderUse(&stdShader2D);
		break;
	case SHADER_3D:
		drawShaderUse(&stdShader3D);
		break;
	case SHADER_3D_CLIP:
		drawShaderUse(&stdShader3DClip);
		break;
	case SHADER_3D_ANIM:
		drawShaderUse(&animShader);
		break;
	case SHADER_3D_POST:
		drawShaderUse(&post3DShader);
	}
}



/*
 * DRAW EVENTS
 */

static void drawStart(void *arg) {
	(void)arg;
	drawFlushes = 0;
	drawState.drawPhase = DP_3D_BG;

	/* Update 2D cam mat */
	cam2DMat = Mat::fromScale(Vec3{ 2.0f / rttW, 2.0f / rttH, 1 });
	cam2DMat.translate(Vec3{ -camX, -camY, 0 });

	cam2DUiMat = Mat::fromScale(Vec3{ 2.0f / winW, -2.0f / winH, 1.0f / 2000 });
	cam2DUiMat.translate(Vec3{ 0, winH / -2.0f, 1000 });

	/* Update 3D overlay cam mat */
	cam3DOvMat = Mat::fromScale(Vec3{ 2.0f / rttW, -2.0f / rttH, 1 });
	cam3DOvMat.translate(Vec3{ -camX, -camY, 0 });

	/* 3D */
	proj3DMat = Mat::perspective(cam3DFov, (float)rttIntW / rttIntH, CAM_3D_NEAR, CAM_3D_FAR);
	drawUpdateFrustum(&proj3DMat);

	/* Clear */
	glBindFramebuffer(GL_FRAMEBUFFER, fbo1);
	glViewport(0, 0, rttIntW, rttIntH);
	glDepthMask(true);
	float clearCol[3] = {
		((clearColor >> 16) & 0xFF) / 255.0f,
		((clearColor >> 8) & 0xFF) / 255.0f,
		((clearColor >> 0) & 0xFF) / 255.0f
	};
	doGamma(&clearCol[0], &clearCol[1], &clearCol[2]);
	glClearColor(clearCol[0], clearCol[1], clearCol[2], 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	/* Draw skybox */
	if (skyboxTexture) {
		glDisable(GL_CULL_FACE);
		drawState.nTex = 0;
		glDepthMask(GL_FALSE);
		drawShaderUse(&skyboxShader);
		glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture->glTexture);
		drawMatIdentity();
		drawSkybox();
		drawFlush();
		glDepthMask(GL_TRUE);
	}

	/* CCW = front */
	glEnable(GL_CULL_FACE);
}

static void drawStart3D(void *arg) {
	(void)arg;
	drawFlush();
	drawState.drawPhase = DP_3D;
}

static void drawDisableCull(void *arg) {
	(void)arg;
	drawFlush();
	drawState.drawPhase = DP_3D_NO_CULL;
	glDisable(GL_CULL_FACE);
}

static void draw3DSetOverlay(void *arg) {
	(void)arg;
	drawState.drawPhase = DP_3D_OVERLAY;
}

static void draw2DLowRes(void *arg) {
	(void)arg;
	drawFlush();

	drawState.drawPhase = DP_2D_LOWRES;

	glBindFramebuffer(GL_FRAMEBUFFER, fbo2);
	glViewport(0, 0, rttIntW, rttIntH);
	glDepthMask(true);
	glClearColor(0.2f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glDepthMask(false);

	/* Copy fbo1 to fbo2 */
	drawReset();
	drawShaderUse(&post3DShader);
	drawTexture(0, &fbo1Col);
	//drawTexture(1, &fbo1Depth);
	drawSrcRect(0, 0, rttIntW, rttIntH);
	drawMatIdentity();
	
	drawTranslate(camX, camY);
	drawScale(1, -1);
	//drawColor(1, 1, 1, 1);
	drawRect(rttW, rttH);
}

static void draw2DHiRes(void *arg) {
	(void)arg;
	drawFlush();

	drawState.drawPhase = DP_2D_HIRES;

	int hiResW = rttW * (float)realWinW / winW;
	int hiResH = rttH * (float)realWinH / winH;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo1);
	glViewport(0, 0, hiResW, hiResH);
	glDepthMask(true);
	glClearColor(0.2f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glDepthMask(false);

	/* Copy fbo2 to fbo1, scaled up */
	drawReset();
	//drawShaderUse(&post3DShader);
	drawTexture(0, &fbo2Col);
	//drawTexture(1, &fbo2Depth);
	drawSrcRect(0, 0, rttIntW, rttIntH);
	drawMatIdentity();

	drawTranslate(camX, camY);
	drawScale(1, 1);
	//drawColor(1, 1, 1, 1);
	drawRect(rttW, rttH);
}

static void drawRttEnd(void *arg) {
	(void)arg;
	drawFlush();

	drawState.drawPhase = DP_BACKBUFFER;

	int hiResW = rttW * (float)realWinW / winW;
	int hiResH = rttH * (float)realWinH / winH;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, realWinW, realWinH);
	glDepthMask(true);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glDepthMask(false);

	drawReset();
	drawTexture(0, &fbo1Col);
	//drawTexture(1, &fbo1Depth);
	drawSrcRect(0, 0, hiResW, hiResH);
	drawMatIdentity();
	drawTranslate(rttX, rttY);
	//drawColor(1, 0, 1, 0.5);
	drawRect(rttW, rttH);
}

static void drawEnd(void *arg) {
	(void)arg;
	drawFlush();
	SDL_GL_SwapWindow(window);
}

void drawSetSkybox(const char *texture) {
	if (skyboxTexture) {
		deleteTexture(skyboxTexture);
	}
	if (texture) {
		skyboxTexture = loadTextureCube(texture);
	} else {
		skyboxTexture = NULL;
	}
}

/*
 * PROPERTIES
 */
void drawTexture(int slot, struct Texture *tex) {
	struct DwTexture *dt = &drawState.tex[slot];
	if (slot >= drawState.nTex || dt->tex != tex) {
		drawFlush();

		if (slot >= drawState.nTex && tex) {
			drawState.nTex = slot + 1;
		}
		dt->tex = tex;
	}
}
struct Texture *drawGetFboTexture(int which) {
	struct Texture *tex = NULL;
	switch (which) {
	case 0:
		tex = &fbo1Col;
		break;
	case 1:
		tex = &fbo1Depth;
		break;
	case 2:
		tex = &fbo2Col;
		break;
	case 3:
		tex = &fbo2Depth;
		break;
	}
	return tex;
}
void drawTextureOffsetScale(int slot, float x, float y, float xs, float ys) {
	struct DwTexture *dt = &drawState.tex[slot];
	if (x == dt->x && y == dt->y && xs == dt->xs && ys == dt->ys)
		return;

	drawFlush();
	dt->x = x;
	dt->y = y;
	dt->xs = xs;
	dt->ys = ys;
}

void drawBlend(enum BlendMode blend) {
	if (drawState.blend != blend) {
		drawFlush();
		drawState.blend = blend;

		switch (blend) {
		case BLEND_MULTIPLY:
			glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
			glBlendEquation(GL_FUNC_ADD);
			break;
		case BLEND_ADD:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glBlendEquation(GL_FUNC_ADD);
			break;
		case BLEND_SUBTRACT:
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
			glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
			break;
		case BLEND_REPLACE:
			glBlendFunc(GL_ONE, GL_ZERO);
			glBlendEquation(GL_FUNC_ADD);
			break;
		case BLEND_SCREEN:
			glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR);
			glBlendEquation(GL_FUNC_ADD);
			break;
		default:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glBlendEquation(GL_FUNC_ADD);
			break;
		}
	}
}


void drawZBufferWrite(bool enable) {
	if (drawState.zWrite != enable) {
		drawFlush();
		if (enable)
			glDepthMask(GL_TRUE);
		else
			glDepthMask(GL_FALSE);
		drawState.zWrite = enable;
	}
}
void drawUvModelMat(bool enable) {
	if (drawState.uvModelMat != enable) {
		drawFlush();
		drawState.uvModelMat = enable;
	}
}
void drawWireframe(bool wf) {
	if (drawState.wireframe != wf) {
		drawFlush();
		drawState.wireframe = wf;
		if (wf) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		} else {
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
	}
}


/*
 * DRAWING
 */

static void setNormMat(struct StdUniformVert *v, const Mat *model, Mat *view) {
	Mat mv = (*model) * (*view);
	mv.inverse3();
	mv.transposeSave3(v->normMat);
}
static void setLightUniform(struct StdUniformFragLight *fl, struct Light *l, bool point) {
	if (point) {
		Vec4 v = Mat{ cam3DMatrix.m } * Vec4{ l->x, l->y, l->z, 1 };
		fl->pos[0] = v.x;
		fl->pos[1] = v.y;
		fl->pos[2] = v.z;
	} else {
		Vec4 v = Mat{ cam3DMatrix.m } * Vec4{ l->x, l->y, l->z, 0 };
		fl->pos[0] = v.x;
		fl->pos[1] = v.y;
		fl->pos[2] = v.z;
	}
	
	fl->ambient[0] = l->ambientR;
	fl->ambient[1] = l->ambientG;
	fl->ambient[2] = l->ambientB;
	fl->diffuse[0] = l->diffuseR;
	fl->diffuse[1] = l->diffuseG;
	fl->diffuse[2] = l->diffuseB;
	fl->specular[0] = l->specularR;
	fl->specular[1] = l->specularG;
	fl->specular[2] = l->specularB;
	fl->constant = l->constant;
	fl->linear = l->linear;
	fl->quadratic = l->quadratic;
}
static void setStdUniforms(const Mat *model) {
	StdUniformVert *v = static_cast<StdUniformVert *>(stackAlloc(sizeof(*v)));
	StdUniformFrag *f = static_cast<StdUniformFrag *>(stackAlloc(sizeof(*f)));

	memcpy(v->model, model->m, sizeof(v->model));
	if (drawState.drawPhase <= DP_3D_NO_CULL && drawState.drawPhase != DP_3D_BG) {
		memcpy(v->view, cam3DMatrix.m, sizeof(v->view));
		memcpy(v->projection, proj3DMat.m, sizeof(v->projection));
		setNormMat(v, model, &cam3DMatrix);
	} else if (drawState.drawPhase == DP_BACKBUFFER) {
		memcpy(v->view, cam2DUiMat.m, sizeof(v->view));
		memcpy(v->projection, identMat.m, sizeof(v->projection));
		setNormMat(v, model, &cam2DUiMat);
	} else if (drawState.drawPhase == DP_3D_BG || drawState.drawPhase == DP_3D_OVERLAY) {
		memcpy(v->view, cam3DOvMat.m, sizeof(v->view));
		memcpy(v->projection, identMat.m, sizeof(v->projection));
		setNormMat(v, model, &cam3DOvMat);
	} else {
		memcpy(v->view, cam2DMat.m, sizeof(v->view));
		memcpy(v->projection, identMat.m, sizeof(v->projection));
		setNormMat(v, model, &cam2DMat);
	}
	glBindBuffer(GL_UNIFORM_BUFFER, stdUboVert);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(*v), v, GL_STREAM_DRAW);
	
	setLightUniform(&f->dirLight, &dirLight, false);
	for (int i = 0; i < DRAW_MAX_POINTLIGHTS; i++) {
		setLightUniform(&f->pointLights[i], &pointLights[i], true);
	}
	for (int i = 0; i < DRAW_MAX_TEX; i++) {
		f->tex[i].offs[0] = drawState.tex[i].x;
		f->tex[i].offs[1] = drawState.tex[i].y;
		f->tex[i].scale[0] = drawState.tex[i].xs;
		f->tex[i].scale[1] = drawState.tex[i].ys;
	}
	f->glossiness = 64;
	f->fogMin = fogMin;
	f->fogMax = fogMax;
	f->fogColor[0] = ((fogColor >> 16) & 0xFF) / 255.0f;
	f->fogColor[1] = ((fogColor >> 8) & 0xFF) / 255.0f;
	f->fogColor[2] = ((fogColor) & 0xFF) / 255.0f;
	doGamma(&f->fogColor[0], &f->fogColor[1], &f->fogColor[2]);

	glBindBuffer(GL_UNIFORM_BUFFER, stdUboFrag);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(*f), f, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	stackDealloc(sizeof(*f) + sizeof(*v));

	unsigned int vIdx = glGetUniformBlockIndex(drawState.shader->glShader, "stdVert");
	glUniformBlockBinding(drawState.shader->glShader, vIdx, 0);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, stdUboVert);

	unsigned int fIdx = glGetUniformBlockIndex(drawState.shader->glShader, "stdFrag");
	glUniformBlockBinding(drawState.shader->glShader, fIdx, 1);
	glBindBufferBase(GL_UNIFORM_BUFFER, 1, stdUboFrag);

	if (drawState.nTex) {
		glUniform1i(getUniform(drawState.shader->glShader, "texBase"), 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, drawState.tex[0].tex->glTexture);
	}
	for (int i = 1; i < drawState.nTex; i++) {
		char buf[64];
		snprintf(buf, 64, "tex%d", i);
		glUniform1i(getUniform(drawState.shader->glShader, buf), i);
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, drawState.tex[i].tex->glTexture);
	}

}
void drawSetAnimUbo(void *data, size_t dataSize) {
	glBindBuffer(GL_UNIFORM_BUFFER, animUbo);
	glBufferData(GL_UNIFORM_BUFFER, dataSize, data, GL_STREAM_DRAW);

	unsigned int aIdx = glGetUniformBlockIndex(drawState.shader->glShader, "animData");
	glUniformBlockBinding(drawState.shader->glShader, aIdx, 2);
	glBindBufferBase(GL_UNIFORM_BUFFER, 2, animUbo);
}

void drawFlush(void) {
	if (curVboData) {
		glUnmapBuffer(GL_ARRAY_BUFFER);
		curVboData = NULL;
		drawFlushes++;
	}
	if (curEboData) {
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
		curEboData = NULL;
	}
	if (curNIndices) {
		setStdUniforms(&identMat);
		
		if (drawState.hasBuffer) {
			glBindVertexArray(stdVao);
			glDrawElements(GL_TRIANGLES, curNIndices, GL_UNSIGNED_INT, 0);

			glBufferData(GL_ARRAY_BUFFER, VBO_MAXSZ, NULL, GL_STREAM_DRAW);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, EBO_MAXSZ, NULL, GL_STREAM_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}
		curNIndices = 0;
	}
	curNVerts = 0;
	drawState.hasBuffer = false;
}

static void drawPrepare(void) {
	if (!drawState.hasBuffer) {
		drawFlush();

		glBindVertexArray(stdVao);

		glBindBuffer(GL_ARRAY_BUFFER, stdVbo);
		//curVboData = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
		curVboData = glMapBufferRange(GL_ARRAY_BUFFER, 0, VBO_MAXSZ, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, stdEbo);
		//curEboData = static_cast<unsigned int *>(glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY));
		curEboData = static_cast<unsigned int *>(glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, EBO_MAXSZ, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

		drawState.hasBuffer = true;
	}
}
void drawPreflush(int nverts, int nindices) {
	int maxVerts = VBO_MAXSZ / sizeof(struct StdVboColor);
	int maxIndices = EBO_MAXSZ / sizeof(unsigned int);
	if (curNVerts + nverts >= maxVerts || curNIndices + nindices >= maxIndices) {
		drawFlush();
	}
}

void drawVertex(float x, float y, float z, float u, float v, float r, float g, float b, float a) {
	drawPrepare();

	int maxVerts = VBO_MAXSZ / sizeof(struct StdVboColor);
	if (curNVerts == maxVerts)
		return;

	Vec4 pos = drawState.matStack[drawState.matStackIdx] * Vec4{ x, y, z, 1 };

	struct StdVboColor *d = &((struct StdVboColor *)curVboData)[curNVerts];
	d->s.x = pos.x;
	d->s.y = pos.y;
	d->s.z = pos.z;
	d->s.nx = 0;
	d->s.ny = 0;
	d->s.nz = 1;

	if (drawState.uvModelMat && drawState.tex[0].tex) {
		d->s.u = pos.x / drawState.tex[0].tex->w + drawState.srcX;
		d->s.v = pos.y / drawState.tex[0].tex->h + drawState.srcY;
		if (drawState.srcH < 0)
			d->s.v = 1 - d->s.v;
	} else {
		d->s.u = u;
		d->s.v = v;
	}
	
	if (drawState.drawPhase <= DP_3D_OVERLAY)
		doGamma(&r, &g, &b);
	d->r = r;
	d->g = g;
	d->b = b;
	d->a = a;
	curNVerts++;
}

void drawIndices(int verts, int n, const unsigned int *lst) {
	int maxIndices = EBO_MAXSZ / sizeof(unsigned int);
	if (curNIndices + n >= maxIndices)
		return;
	int base = curNVerts - verts;
	for (int i = 0; i < n; i++) {
		curEboData[curNIndices++] = base + lst[i];
	}
}

void drawModel3D(struct Model *m) {
	drawFlush();

	setStdUniforms(&drawState.matStack[drawState.matStackIdx]);

	glBindVertexArray(m->glVao);
	glDrawElements(GL_TRIANGLES, m->nTriangles * 3, GL_UNSIGNED_INT, 0);
}


/*
 * MISC
 */

#ifndef RELEASE
void GLAPIENTRY openglDebugCallback(GLenum source, GLenum type, GLuint id,
		GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	(void) source;
	(void) id;
	(void) length;
	(void) userParam;
	const char *sev = NULL;
	switch(severity) {
		case GL_DEBUG_SEVERITY_HIGH:
			sev = "High";
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			sev = "Medium";
			break;
		case GL_DEBUG_SEVERITY_LOW:
			sev = "Low";
			break;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			sev = "Notification";
			break;
		default:
			sev = "Undefined";
			break;
	}
	logDebug("GL CALLBACK: %s type = 0x%x, severity = %s, message = %s\n",
		   ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
			type, sev, message );
}
#endif

static void dwModelInit(unsigned int *vao, unsigned int *vbo, unsigned int *ebo, bool anim) {
	size_t sz = anim ? sizeof(struct StdVboAnim) : sizeof(struct StdVboColor);
	glGenVertexArrays(1, vao);
	glBindVertexArray(*vao);
	glGenBuffers(1, vbo);
	glGenBuffers(1, ebo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ebo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sz, (void *)0); /* Position */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sz, (void *)12); /* Position */
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sz, (void *)24); /* UV */
	glEnableVertexAttribArray(2);
	if (anim) {
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sz, (void *)32); /* Weigths */
		glEnableVertexAttribArray(3);
		glVertexAttribIPointer(4, 4, GL_BYTE, sz, (void *)48);
		glEnableVertexAttribArray(4);
	} else {
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sz, (void *)32); /* Color */
		glEnableVertexAttribArray(3);
	}
}
void uploadModel(Model *m, void *verts, void *indices) {
	dwModelInit(&m->glVao, &m->glVbo, &m->glEbo, m->flags & MODEL_FILE_ANIM);
	size_t sz = m->flags & MODEL_FILE_ANIM ? sizeof(struct StdVboAnim) : sizeof(struct StdVboColor);
	glBufferData(GL_ARRAY_BUFFER, sz * m->nVertices, verts, GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m->nTriangles * 12ULL, indices, GL_STATIC_DRAW);
}
void deleteModel(Model *m) {
	glDeleteVertexArrays(1, &m->glVao);
	glDeleteBuffers(1, &m->glVbo);
	glDeleteBuffers(1, &m->glEbo);
}

static void sdlFail(const char *func) {
	fail("SDL Error in %s: %s\n", func, SDL_GetError());
}

void drawSetVsync(int mode) {
	if (SDL_GL_SetSwapInterval(mode))
		sdlFail("SDL_GL_SetSwapInterval");
}


/*
 * INIT/FINI
 */

static void setupFbo(unsigned int *fbo, unsigned int *col, unsigned int *depth) {
	glGenFramebuffers(1, fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
	glGenTextures(1, col);
	glBindTexture(GL_TEXTURE_2D, *col);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, realWinW, realWinH, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenTextures(1, depth);
	glBindTexture(GL_TEXTURE_2D, *depth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, realWinW, realWinH, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *col, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, *depth, 0);
	int error = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (error != GL_FRAMEBUFFER_COMPLETE)
		fail("Framebuffer is incomplete: %d %d\n", error, GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
}

void drawDriverInit(void) {
	int error = SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	if (error) sdlFail("SDL_GL_SetAttribute");

	/* Use openGL 3.3 */
	error = SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	if (error) sdlFail("SDL_GL_SetAttribute");
	error = SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	if (error) sdlFail("SDL_GL_SetAttribute");

	error = SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	if (error) sdlFail("SDL_GL_SetAttribute");

	/* TODO */
	winW = realWinW = 854;
	winH = realWinH = 480;
	realWinW = engineSettings->resW;
	realWinH = engineSettings->resH;
	
	rttY = winH / 2.0f;
	window = SDL_CreateWindow(gameTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, realWinW, realWinH, SDL_WINDOW_OPENGL);
	if (!window)
		sdlFail("SDL_CreateWindow");

	glContext = SDL_GL_CreateContext(window);
	if (!glContext) {
		/* Use openGL 3.1 */
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		glContext = SDL_GL_CreateContext(window);
		compat31 = true;
		if (!glContext)
			sdlFail("SDL_GL_CreateContext (Most likely unsupported OpenGL version)");
		else
			logNorm("WARN: OpenGL 3.3 is not supported on this platform, using OpenGL 3.1 instead.\
					You may experience problems\n");
	}

	if (SDL_GL_MakeCurrent(window, glContext))
		sdlFail("SDL_GL_MakeCurrent");

#ifndef APPLE
	glewExperimental = GL_TRUE;
	GLenum glewerr = glewInit();
	if (glewerr != GLEW_OK) {
		fail("GLEW Failed to initialize!\n");
	}
#endif

	/* Enable blending */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);
	glEnable(GL_BLEND);

	/* Enable depth testing */
	glEnable(GL_DEPTH_TEST);

#ifndef RELEASE
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(openglDebugCallback, 0);
#endif

	/* Setup 2D */
	unsigned int v = loadShader("shaders/glsl/std.vert", GL_VERTEX_SHADER);
	unsigned int f = loadShader("shaders/glsl/std2d.frag", GL_FRAGMENT_SHADER);
	stdShader2D.glShader = linkShaders(v, f);
	glDeleteShader(f);
	dwModelInit(&stdVao, &stdVbo, &stdEbo, false);
	glBufferData(GL_ARRAY_BUFFER, VBO_MAXSZ, NULL, GL_STREAM_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, EBO_MAXSZ, NULL, GL_STREAM_DRAW);

	/* Setup 3D */
	f = loadShader("shaders/glsl/std3d.frag", GL_FRAGMENT_SHADER);
	stdShader3D.glShader = linkShaders(v, f);
	glDeleteShader(f);

	f = loadShader("shaders/glsl/std3d_clip.frag", GL_FRAGMENT_SHADER);
	stdShader3DClip.glShader = linkShaders(v, f);
	glDeleteShader(f);

	f = loadShader("shaders/glsl/post3d.frag", GL_FRAGMENT_SHADER);
	post3DShader.glShader = linkShaders(v, f);
	glDeleteShader(v);
	glDeleteShader(f);

	v = loadShader("shaders/glsl/skybox.vert", GL_VERTEX_SHADER);
	f = loadShader("shaders/glsl/skybox.frag", GL_FRAGMENT_SHADER);
	skyboxShader.glShader = linkShaders(v, f);
	glDeleteShader(v);
	glDeleteShader(f);

	v = loadShader("shaders/glsl/anim.vert", GL_VERTEX_SHADER);
	f = loadShader("shaders/glsl/std3d.frag", GL_FRAGMENT_SHADER);
	animShader.glShader = linkShaders(v, f);
	glDeleteShader(v);
	glDeleteShader(f);

	/* Setup FBOs */
	setupFbo(&fbo1, &fbo1Col.glTexture, &fbo1Depth.glTexture);
	setupFbo(&fbo2, &fbo2Col.glTexture, &fbo2Depth.glTexture);
	fbo2Depth.w = fbo1Depth.w = fbo2Col.w = fbo1Col.w = realWinW;
	fbo2Depth.h = fbo1Depth.h = fbo2Col.h = fbo1Col.h = realWinH;
	
	glGenBuffers(1, &stdUboVert);
	glGenBuffers(1, &stdUboFrag);
	glGenBuffers(1, &animUbo);

	/* Enable some settings */
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	/* Set the updates */
	addDrawUpdate(0, drawStart, NULL);
	addDrawUpdate(engineSettings->draw3DStart, drawStart3D, NULL);
	addDrawUpdate(engineSettings->draw3DNoCull, drawDisableCull, NULL);
	addDrawUpdate(engineSettings->draw3DOverlay, draw3DSetOverlay, NULL);
	addDrawUpdate(engineSettings->draw2DLowRes, draw2DLowRes, NULL);
	addDrawUpdate(engineSettings->draw2DHiRes, draw2DHiRes, NULL);
	addDrawUpdate(engineSettings->drawRttEnd, drawRttEnd, NULL);
	addDrawUpdate(9999, drawEnd, NULL);
}

void drawDriverFini(void) {
	removeDrawUpdate(0); /* drawStart */
	removeDrawUpdate(engineSettings->draw3DStart);
	removeDrawUpdate(engineSettings->draw3DNoCull);
	removeDrawUpdate(engineSettings->draw3DOverlay);
	removeDrawUpdate(engineSettings->draw2DLowRes);
	removeDrawUpdate(engineSettings->draw2DHiRes);
	removeDrawUpdate(engineSettings->drawRttEnd);
	removeDrawUpdate(9999); /* drawEnd */

	glDeleteVertexArrays(1, &stdVao);
	glDeleteBuffers(1, &stdVbo);
	glDeleteBuffers(1, &stdEbo);
	//glDeleteShader(stdShader2D.glShader);
	//glDeleteShader(stdShader3D.glShader);

	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(window);
}


void drawSetResolution(int w, int h) {
	SDL_SetWindowSize(window, w, h);
	realWinW = w;
	realWinH = h;

	int mw, mh;
	drawGetMonitorResolution(&mw, &mh);
	SDL_SetWindowBordered(window, static_cast<SDL_bool>(mw != w || mh != h));
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	
	glDeleteFramebuffers(1, &fbo1);
	glDeleteTextures(1, &fbo1Col.glTexture);
	glDeleteTextures(1, &fbo1Depth.glTexture);
	glDeleteFramebuffers(1, &fbo2);
	glDeleteTextures(1, &fbo2Col.glTexture);
	glDeleteTextures(1, &fbo2Depth.glTexture);
	setupFbo(&fbo1, &fbo1Col.glTexture, &fbo1Depth.glTexture);
	setupFbo(&fbo2, &fbo2Col.glTexture, &fbo2Depth.glTexture);

	fbo2Depth.w = fbo1Depth.w = fbo2Col.w = fbo1Col.w = realWinW;
	fbo2Depth.h = fbo1Depth.h = fbo2Col.h = fbo1Col.h = realWinH;
}

struct Texture *loadTextureFromPixels(int w, int h, unsigned char *pixels, int flags) {
	struct Texture *tex = new Texture;
	tex->w = w;
	tex->h = h;
	tex->refs = 1;
	tex->flags = flags;
	glGenTextures(1, &tex->glTexture);
	glBindTexture(GL_TEXTURE_2D, tex->glTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, flags & TEXTURE_SRGB ? GL_SRGB_ALPHA : GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	if (flags & TEXTURE_POINT) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	} else if (flags & TEXTURE_MIPMAP) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glGenerateMipmap(GL_TEXTURE_2D);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	return tex;
}
static const char *cubeSides[6] = {
	"right",
	"left",
	"bottom",
	"top",
	"front",
	"back"
};
struct Texture *loadTextureCube(const char *name) {
	struct Texture *tex = new Texture;
	glGenTextures(1, &tex->glTexture);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex->glTexture);

	for (int i = 0; i < 6; i++) {
		int w, h, ch = 3;
		char nameBuf[50];
		snprintf(nameBuf, 50, "%s/%s.png", name, cubeSides[i]);
		unsigned char *pixels;
		bool f = loadPixels(&w, &h, &ch, &pixels, nameBuf, 0);
		if (!f) {
			glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
			delete tex;
			return nullptr;
		}
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_SRGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
		deletePixels(pixels);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return tex;
}

void deleteTexture(struct Texture *texture) {
	if (!texture)
		return;
	texture->refs -= 1;
	if (texture->refs > 0)
		return;

	glDeleteTextures(1, &texture->glTexture);
	globalDealloc(texture);
}