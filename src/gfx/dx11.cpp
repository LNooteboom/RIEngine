#include <gfx/draw.h>
#include <gfx/drawvm.h>
#include <gfx/ttf.h>
#include <main.h>
#include <assets.h>
#include <string.h>
#include <events.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include <map>
#include <string>

#pragma comment( lib, "user32" )          // link against the win32 library
#pragma comment( lib, "d3d11.lib" )       // direct3D library
#pragma comment( lib, "dxgi.lib" )        // directx graphics interface

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

struct DwState drawState;

struct StdConstantVS {
	Mat modelView;
	Mat projection;
	Mat normMat;
	float args[8];
	float col1[4];
	float col2[4];
};
struct StdConstantPSTex {
	float offs[2];
	float scale[2];
};
struct StdConstantPSLight {
	float pos[4];
	float ambient[4];
	float diffuse[4];
	float specular[4];
	float constant;
	float linear;
	float quadratic;
	int unused[1];
};
struct StdConstantPSScene {
	StdConstantPSLight dirLight;
	StdConstantPSLight pointLights[8];
	float fogMin, fogMax;
	int unused[2];
	float fogColor[4];
};
struct StdConstantPS {
	StdConstantPSTex tex[8];
	float args[8];
	float col1[4];
	float col2[4];
};

struct VertexShader {
	VertexShader() : vertex(nullptr), inputLayout(nullptr) {

	}
	VertexShader(ID3D11VertexShader *vs, ID3D11InputLayout *il) : vertex(vs), inputLayout(il) {

	}
	ID3D11VertexShader *vertex;
	ID3D11InputLayout *inputLayout;
};

static Mat cam3DOvMat;
static Mat cam2DMat;
static Mat cam2DUiMat;
static Mat proj3DMat;
static Mat identMat{ 1.0f };

static SDL_Window *window;
static HWND hWnd;
static ID3D11Device *device;
static ID3D11DeviceContext *deviceContext;
static IDXGISwapChain *swapChain;
static ID3D11Texture2D *framebuffer;
static ID3D11RenderTargetView *framebufferView;

#ifdef RELEASE
#define VERTEX_BUFFER_SIZE		0x40000
#define INDEX_BUFFER_SIZE		0x40000
#else
#define VERTEX_BUFFER_SIZE		0x80000
#define INDEX_BUFFER_SIZE		0x80000
#endif
static ID3D11Buffer *streamVertexBuffer;
static ID3D11Buffer *streamIndexBuffer;
static ID3D11Buffer *stdConstantVSBuffer;
static ID3D11Buffer *stdConstantVSAnimBuffer;
static ID3D11Buffer *stdConstantPSBuffer;
static ID3D11Buffer *stdConstantPSSceneBuffer;
static int curNVerts;
static int curNIndices;
static D3D11_MAPPED_SUBRESOURCE streamVertexMappedBuffer;
static D3D11_MAPPED_SUBRESOURCE streamIndexMappedBuffer;

static ID3D11RasterizerState *rasterize2D;
static ID3D11RasterizerState *rasterize3D;
static ID3D11RasterizerState *rasterize3DInvert;
static ID3D11SamplerState *linearSampler;
static ID3D11SamplerState *pointSampler;
static ID3D11BlendState *blendStates[BLEND_N];
static ID3D11DepthStencilState *depthStencil3D;
static ID3D11DepthStencilState *depthStencil3DNoWrite;
static ID3D11DepthStencilState *depthStencil2D;

static float shaderArgs[8];

struct TargetSurface {
	Texture color;
	Texture depthStencil;
	ID3D11RenderTargetView *colorView;
	ID3D11DepthStencilView *depthStencilView;
};
TargetSurface surface1;
TargetSurface surface2;

static Shader *stdShaders[SHADER_STD_N];

static std::map<std::string, VertexShader> vsMap;
static std::map<std::string, ID3D11PixelShader *> psMap;

extern "C" {
	extern int drawVmApi;
}

/*
 * SHADER
 */
struct Shader {
	VertexShader vertex;
	ID3D11PixelShader *pixel;
};

struct Shader *drawShaderNew(const char *vert, const char *frag) {
	HRESULT hr;
	Shader *shader;

	Asset *vs = assetOpen(vert);
	if (!vs) {
		logNorm("Could not open VS: %s\n", vert);
		return nullptr;
	}
	Asset *ps = assetOpen(frag);
	if (!ps) {
		logNorm("Could not open PS: %s\n", frag);
		assetClose(vs);
		return nullptr;
	}

	VertexShader vertexShader;
	auto vsIt = vsMap.find(vert);
	if (vsIt != vsMap.end()) {
		vertexShader = vsIt->second;
	} else {
		hr = device->CreateVertexShader(vs->buffer, vs->bufferSize, nullptr, &vertexShader.vertex);
		if (FAILED(hr)) {
			logNorm("CreateVertexShader(%s) failed with %x\n", vert, hr);
			assetClose(vs);
			assetClose(ps);
			return nullptr;
		}

		if (!memcmp(vert, "shaders/anim", 12)) {
			D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
				{"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"NORM", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"BONES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
			};
			device->CreateInputLayout(inputElementDesc, 5, vs->buffer, vs->bufferSize, &vertexShader.inputLayout);
		} else {
			D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
				{"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"NORM", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			};
			device->CreateInputLayout(inputElementDesc, 4, vs->buffer, vs->bufferSize, &vertexShader.inputLayout);
		}

		vsMap.insert({ vert, vertexShader });
	}

	ID3D11PixelShader *pixelShader;
	auto psIt = psMap.find(frag);
	if (psIt != psMap.end()) {
		pixelShader = psIt->second;
	} else {
		hr = device->CreatePixelShader(ps->buffer, ps->bufferSize, nullptr, &pixelShader);
		if (FAILED(hr)) {
			logNorm("CreateVertexShader(%s) failed with %x\n", vert, hr);
			assetClose(vs);
			assetClose(ps);
			return nullptr;
		}
		psMap.insert({ frag, pixelShader });
	}

	shader = new Shader;
	shader->vertex = vertexShader;
	shader->pixel = pixelShader;

	return shader;
}

void drawShaderDelete(struct Shader *s) {
	delete s;
}

void drawShaderUse(struct Shader *s) {
	if (s && s != drawState.shader) {
		drawFlush();
		drawState.shader = s;
		deviceContext->VSSetShader(s->vertex.vertex, nullptr, 0);
		deviceContext->IASetInputLayout(s->vertex.inputLayout);
		deviceContext->PSSetShader(s->pixel, nullptr, 0);
	}
}
void drawShaderUseStd(enum StdShader s) {
	drawShaderUse(stdShaders[s]);
}



/*
 * DRAW EVENTS
 */
static void doGamma(float *r, float *g, float *b) {
	*r = 0.75f * (*r * *r) + 0.25f * *r * (*r * *r);
	*g = 0.75f * (*g * *g) + 0.25f * *g * (*g * *g);
	*b = 0.75f * (*b * *b) + 0.25f * *b * (*b * *b);
}

static void copyLight(StdConstantPSLight *dest, Light *l, bool point) {
	if (point) {
		Vec4 v = Mat{ cam3DMatrix.m } * Vec4{ l->x, l->y, l->z, 1 };
		dest->pos[0] = v.x;
		dest->pos[1] = v.y;
		dest->pos[2] = v.z;
	} else {
		Mat m = Mat{ cam3DMatrix.m };
		m.inverse3();
		m = m.transposed();
		Vec4 v = m * Vec4{ l->x, l->y, l->z, 0 };

		dest->pos[0] = v.x;
		dest->pos[1] = v.y;
		dest->pos[2] = v.z;
	}

	dest->ambient[0] = l->ambientR;
	dest->ambient[1] = l->ambientG;
	dest->ambient[2] = l->ambientB;
	dest->diffuse[0] = l->diffuseR;
	dest->diffuse[1] = l->diffuseG;
	dest->diffuse[2] = l->diffuseB;
	dest->specular[0] = l->specularR;
	dest->specular[1] = l->specularG;
	dest->specular[2] = l->specularB;
	dest->constant = l->constant;
	dest->linear = l->linear;
	dest->quadratic = l->quadratic;
}
static void setScenePSConstants() {
	D3D11_MAPPED_SUBRESOURCE mappedBuffer;
	deviceContext->Map(stdConstantPSSceneBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedBuffer);
	StdConstantPSScene *scene = static_cast<StdConstantPSScene *>(mappedBuffer.pData);
	copyLight(&scene->dirLight, &dirLight, false);
	for (int i = 0; i < DRAW_MAX_POINTLIGHTS; i++) {
		copyLight(&scene->pointLights[i], &pointLights[i], true);
	}
	float fog[3] = {
		((fogColor >> 16) & 0xFF) / 255.0f,
		((fogColor >> 8) & 0xFF) / 255.0f,
		((fogColor) & 0xFF) / 255.0f
	};
	doGamma(&fog[0], &fog[1], &fog[2]);
	scene->fogColor[0] = fog[0];
	scene->fogColor[1] = fog[1];
	scene->fogColor[2] = fog[2];
	scene->fogMin = fogMin;
	scene->fogMax = fogMax;
	deviceContext->Unmap(stdConstantPSSceneBuffer, 0);
}

static void drawStart(void *arg) {
	(void)arg;
	drawFlushes = 0;
	drawState.drawPhase = DP_3D_BG;

	/* Update 2D cam mat */
	cam2DMat = Mat::fromScale(Vec3{ 2.0f / rttW, -2.0f / rttH, 1.0f / 512 });
	cam2DMat.translate(Vec3{ -camX, -camY, 256 });
	cam2DUiMat = Mat::fromScale(Vec3{ 2.0f / winW, -2.0f / winH, 1.0f / 2000 });
	cam2DUiMat.translate(Vec3{ 0, winH / -2.0f, 1000 });
	/* Update 3D overlay cam mat */
	cam3DOvMat = Mat::fromScale(Vec3{ 2.0f / rttW, -2.0f / rttH, 1 });
	cam3DOvMat.translate(Vec3{ -camX, -camY, 0 });
	/* 3D */
	proj3DMat = Mat::perspective(cam3DFov, (float)rttIntW / rttIntH, CAM_3D_NEAR, CAM_3D_FAR);
	drawUpdateFrustum(&proj3DMat);

	setScenePSConstants();

	ID3D11ShaderResourceView *nullView = nullptr;
	deviceContext->PSSetShaderResources(0, 1, &nullView);

	deviceContext->RSSetState(rasterize3D);
	D3D11_VIEWPORT viewport = { 0, 0, (float)rttIntW, (float)rttIntH, 0, 1 };
	deviceContext->RSSetViewports(1, &viewport);
	deviceContext->OMSetRenderTargets(1, &surface1.colorView, surface1.depthStencilView);
	deviceContext->OMSetDepthStencilState(depthStencil3D, 0);
	drawState.zWrite = true;

	float clearCol[4] = {
		((clearColor >> 16) & 0xFF) / 255.0f,
		((clearColor >> 8) & 0xFF) / 255.0f,
		((clearColor >> 0) & 0xFF) / 255.0f,
		1.0f
	};
	doGamma(&clearCol[0], &clearCol[1], &clearCol[2]);
	deviceContext->ClearRenderTargetView(surface1.colorView, clearCol);
	deviceContext->ClearDepthStencilView(surface1.depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
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
	deviceContext->RSSetState(rasterize2D);
}

static void draw3DSetOverlay(void *arg) {
	(void)arg;
	drawFlush();
	drawState.drawPhase = DP_3D_OVERLAY;
	deviceContext->OMSetDepthStencilState(depthStencil2D, 0);
	drawState.zWrite = false;
}

static void draw2DLowRes(void *arg) {
	(void)arg;
	drawFlush();
	drawState.drawPhase = DP_2D_LOWRES;

	ID3D11ShaderResourceView *nullView = nullptr;
	deviceContext->PSSetShaderResources(0, 1, &nullView);

	//D3D11_VIEWPORT viewport = { 0, 0, rttIntW, rttIntH, 0, 1 };
	//deviceContext->RSSetViewports(1, &viewport);
	deviceContext->OMSetRenderTargets(1, &surface2.colorView, surface2.depthStencilView);

	float clearCol[4] = {0.2f,0.0f,0.0f,1.0f};
	deviceContext->ClearRenderTargetView(surface2.colorView, clearCol);
	deviceContext->ClearDepthStencilView(surface2.depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	drawReset();
	drawShaderUseStd(SHADER_3D_POST);
	drawTexture(0, &surface1.color);
	//drawTexture(1, &fbo1Depth);
	drawSrcRect(0, 0, rttIntW, rttIntH);
	drawMatIdentity();

	drawTranslate(camX, camY);
	drawScale(1, 1);
	//drawColor(1, 1, 1, 1);
	drawRect(rttW, rttH);
}

static void draw2DHiRes(void *arg) {
	(void)arg;
	drawFlush();
	drawState.drawPhase = DP_2D_HIRES;

	ID3D11ShaderResourceView *nullView = nullptr;
	deviceContext->PSSetShaderResources(0, 1, &nullView);
	D3D11_VIEWPORT viewport = { 0, 0, rttW * (float)realWinW / winW, rttH * (float)realWinH / winH, 0, 1 };
	deviceContext->RSSetViewports(1, &viewport);
	deviceContext->OMSetRenderTargets(1, &surface1.colorView, surface1.depthStencilView);
	deviceContext->OMSetDepthStencilState(depthStencil3D, 0);

	/* Copy fbo2 to fbo1, scaled up */
	drawReset();
	drawBlend(BLEND_REPLACE);
	//drawShaderUse(&post3DShader);
	drawShaderUseStd(SHADER_2D);
	drawTexture(0, &surface2.color);
	//drawTexture(1, &fbo2Depth);
	drawSrcRect(0, 0, rttIntW, rttIntH);
	drawMatIdentity();

	drawTranslate(camX, camY);
	drawScale(1, 1);
	drawRect(rttW, rttH);
}

static void drawRttEnd(void *arg) {
	(void)arg;
	drawFlush();
	drawState.drawPhase = DP_BACKBUFFER;

	ID3D11ShaderResourceView *nullView = nullptr;
	deviceContext->PSSetShaderResources(0, 1, &nullView);

	D3D11_VIEWPORT viewport = { 0, 0, (float)realWinW, (float)realWinH, 0, 1 };
	deviceContext->RSSetViewports(1, &viewport);
	deviceContext->OMSetRenderTargets(1, &framebufferView, nullptr);
	float clearColor[4] = { 1, 1, 1, 1 };
	deviceContext->ClearRenderTargetView(framebufferView, clearColor);

	drawReset();
	drawShaderUseStd(SHADER_2D);
	drawTexture(0, &surface1.color);
	//drawTexture(1, &fbo1Depth);
	drawSrcRect(0, 0, rttW * (float)realWinW / winW, rttH * (float)realWinH / winH);
	drawMatIdentity();
	drawTranslate(rttX, rttY);
	drawRect(rttW, rttH);
}

static void drawEnd(void *arg) {
	(void)arg;
	drawFlush();
	swapChain->Present(0, 0);
}

void drawSetSkybox(const char *texture) {
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
	switch (which) {
	case 0:
		return &surface1.color;
	case 1:
		return &surface1.depthStencil;
	case 2:
		return &surface2.color;
	case 3:
		return &surface2.depthStencil;
	default:
		return NULL;
	}
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

		deviceContext->OMSetBlendState(blendStates[blend], nullptr, 0xFFFFFFFF);
	}
}
void drawShaderArgs(int n, float *args) {
	drawFlush();
	for (int i = 0; i < n; i++) {
		shaderArgs[i] = args[i];
	}
}


void drawZBufferWrite(bool enable) {
	if (drawState.zWrite != enable && drawState.drawPhase < DP_3D_OVERLAY) {
		drawFlush();
		if (enable)
			deviceContext->OMSetDepthStencilState(depthStencil3D, 0);
		else
			deviceContext->OMSetDepthStencilState(depthStencil3DNoWrite, 0);
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
		/*if (wf) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		} else {
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}*/
	}
}

void drawCullInvert(bool invert) {
	if (drawState.cullInvert != invert && drawState.drawPhase == DP_3D) {
		drawFlush();
		if (invert)
			deviceContext->RSSetState(rasterize3DInvert);
		else
			deviceContext->RSSetState(rasterize3D);
	}
	drawState.cullInvert = invert;
}



/*
 * DRAWING
 */
void drawSetAnimUbo(void *data, size_t dataSize) {
	D3D11_MAPPED_SUBRESOURCE mappedAnimBuffer;
	deviceContext->Map(stdConstantVSAnimBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedAnimBuffer);
	memcpy(mappedAnimBuffer.pData, data, dataSize);
	deviceContext->Unmap(stdConstantVSAnimBuffer, 0);
	deviceContext->VSSetConstantBuffers(1, 1, &stdConstantVSAnimBuffer);
}

static void setNormMat(void) {
	drawState.normMat = drawState.matStack[drawState.matStackIdx];
	drawState.normMat.inverse3();
	drawState.normMat = drawState.normMat.transposed();
	drawState.normMatValid = true;
}

static void drawSetConstants(Mat *model) {
	D3D11_MAPPED_SUBRESOURCE stdConstantVSMappedBuffer;
	deviceContext->Map(stdConstantVSBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &stdConstantVSMappedBuffer);
	StdConstantVS *cvs = static_cast<StdConstantVS *>(stdConstantVSMappedBuffer.pData);
	
	if (drawState.drawPhase == DP_3D_BG) {
		cvs->projection = proj3DMat;
		cvs->normMat = identMat;
		Mat vw = cam3DMatrix;
		vw.m[3] = vw.m[7] = vw.m[11] = 0;
		vw.m[12] = vw.m[13] = vw.m[14] = 0;
		vw.m[15] = 1;
		if (model) {
			cvs->modelView = vw * *model;
		} else {
			cvs->modelView = vw;
		}
	} else {
		Mat *view;
		if (drawState.drawPhase <= DP_3D_NO_CULL) {
			view = &cam3DMatrix;
			cvs->projection = proj3DMat;
			
		} else if (drawState.drawPhase == DP_BACKBUFFER) {
			view = &cam2DUiMat;
			cvs->projection = identMat;
		} else if (drawState.drawPhase == DP_3D_OVERLAY) {
			view = &cam3DOvMat;
			cvs->projection = identMat;
		} else {
			view = &cam2DMat;
			cvs->projection = identMat;
		}
		if (model) {
			cvs->modelView = *view * *model;
		} else {
			cvs->modelView = *view;
		}
		if (drawState.drawPhase <= DP_3D_NO_CULL) {
			Mat m{ cvs->modelView };
			m.inverse3();
			cvs->normMat = m.transposed();
		} else {
			cvs->normMat = identMat;
		}
	}

	for (int i = 0; i < 8; i++) {
		cvs->args[i] = shaderArgs[i];
	}
	for (int i = 0; i < 4; i++) {
		cvs->col1[i] = drawState.col1[i];
		cvs->col2[i] = drawState.col2[i];
	}
	deviceContext->Unmap(stdConstantVSBuffer, 0);

	D3D11_MAPPED_SUBRESOURCE stdConstantPSMappedBuffer;
	deviceContext->Map(stdConstantPSBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &stdConstantPSMappedBuffer);
	StdConstantPS *cps = static_cast<StdConstantPS *>(stdConstantPSMappedBuffer.pData);
	for (int i = 0; i < DRAW_MAX_TEX; i++) {
		cps->tex[i].offs[0] = drawState.tex[i].x;
		cps->tex[i].offs[1] = drawState.tex[i].y;
		cps->tex[i].scale[0] = drawState.tex[i].xs;
		cps->tex[i].scale[1] = drawState.tex[i].ys;
	}
	for (int i = 0; i < 8; i++) {
		cps->args[i] = shaderArgs[i];
	}
	for (int i = 0; i < 4; i++) {
		cps->col1[i] = drawState.col1[i];
		cps->col2[i] = drawState.col2[i];
	}
	deviceContext->Unmap(stdConstantPSBuffer, 0);

	ID3D11ShaderResourceView *textures[16];
	for (int i = 0; i < drawState.nTex; i++) {
		if (drawState.tex[i].tex) {
			textures[i] = static_cast<ID3D11ShaderResourceView *>(drawState.tex[i].tex->d3dResourceView);
		} else {
			textures[i] = nullptr;
		}
	}
	deviceContext->PSSetShaderResources(0, drawState.nTex, textures);
	if (drawState.tex[0].tex && drawState.tex[0].tex->flags & TEXTURE_POINT) {
		deviceContext->PSSetSamplers(0, 1, &pointSampler);
	} else {
		deviceContext->PSSetSamplers(0, 1, &linearSampler);
	}

	deviceContext->VSSetConstantBuffers(0, 1, &stdConstantVSBuffer);
	if (drawState.drawPhase <= DP_3D_NO_CULL) {
		ID3D11Buffer *buffers[2] = { stdConstantPSBuffer, stdConstantPSSceneBuffer };
		deviceContext->PSSetConstantBuffers(0, 2, buffers);
	} else {
		deviceContext->PSSetConstantBuffers(0, 1, &stdConstantPSBuffer);
	}
}
void drawFlush(void) {
	if (drawState.hasBuffer) {
		deviceContext->Unmap(streamVertexBuffer, 0);
		streamVertexMappedBuffer.pData = nullptr;
		deviceContext->Unmap(streamIndexBuffer, 0);
		streamIndexMappedBuffer.pData = nullptr;

		drawSetConstants(nullptr);

		UINT stride = sizeof(StdVboColor);
		UINT offset = 0;
		deviceContext->IASetVertexBuffers(0, 1, &streamVertexBuffer, &stride, &offset);
		deviceContext->IASetIndexBuffer(streamIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		deviceContext->DrawIndexed(curNIndices, 0, 0);

		curNIndices = 0;
		curNVerts = 0;
		drawState.hasBuffer = false;
	}
}

static void drawPrepare(void) {
	if (!drawState.hasBuffer) {
		deviceContext->Map(streamVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &streamVertexMappedBuffer);
		deviceContext->Map(streamIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &streamIndexMappedBuffer);

		drawState.hasBuffer = true;
		curNIndices = 0;
		curNVerts = 0;
	}
}
void drawPreflush(int nverts, int nindices) {
	int maxVerts = VERTEX_BUFFER_SIZE / sizeof(struct StdVboColor);
	int maxIndices = INDEX_BUFFER_SIZE / sizeof(int);
	if (curNVerts + nverts >= maxVerts || curNIndices + nindices >= maxIndices) {
		drawFlush();
	}
}




void drawVertex3D(float x, float y, float z, float nx, float ny, float nz, float u, float v, float r, float g, float b, float a) {
	drawPrepare();

	int maxVerts = VBO_MAXSZ / sizeof(struct StdVboColor);
	if (curNVerts == maxVerts)
		return;

	Vec4 norm{ nx, ny, nz, 0 };
	if (drawState.drawPhase <= DP_3D_OVERLAY) {
		doGamma(&r, &g, &b);
		if (!drawState.normMatValid) {
			setNormMat();
		}
		norm = drawState.normMat * norm;
	}

	Vec4 pos = drawState.matStack[drawState.matStackIdx] * Vec4{ x, y, z, 1 };

	StdVboColor *d = &(static_cast<StdVboColor *>(streamVertexMappedBuffer.pData)[curNVerts]);
	d->s.x = pos.x;
	d->s.y = pos.y;
	d->s.z = pos.z;
	d->s.nx = norm.x;
	d->s.ny = norm.y;
	d->s.nz = norm.z;

	if (drawState.uvModelMat && drawState.tex[0].tex) {
		d->s.u = pos.x / drawState.tex[0].tex->w + drawState.srcX;
		d->s.v = pos.y / drawState.tex[0].tex->h + drawState.srcY;
		if (drawState.srcH < 0)
			d->s.v = 1 - d->s.v;
	} else {
		d->s.u = u;
		d->s.v = v;
	}

	
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
		static_cast<int *>(streamIndexMappedBuffer.pData)[curNIndices++] = base + lst[i];
	}
}

void drawModel3D(struct Model *m) {
	drawFlush();
	drawSetConstants(&drawState.matStack[drawState.matStackIdx]);
	ID3D11Buffer *verts = static_cast<ID3D11Buffer *>(m->d3dVerts);
	ID3D11Buffer *indices = static_cast<ID3D11Buffer *>(m->d3dIndices);
	UINT stride = m->flags & MODEL_FILE_ANIM ? sizeof(struct StdVboAnim) : sizeof(struct StdVboColor);
	UINT offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &verts, &stride, &offset);
	deviceContext->IASetIndexBuffer(indices, DXGI_FORMAT_R32_UINT, 0);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->DrawIndexed(m->nTriangles * 3, 0, 0);
}


/*
 * MISC
 */

void drawSetVsync(int mode) {
	
}

void uploadModel(Model *m, void *verts, void *indices) {
	D3D11_BUFFER_DESC bufferDesc = { 0 };
	size_t sz = m->flags & MODEL_FILE_ANIM ? sizeof(struct StdVboAnim) : sizeof(struct StdVboColor);
	bufferDesc.ByteWidth = sz * m->nVertices;
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA subResourceData;
	subResourceData.pSysMem = verts;
	subResourceData.SysMemPitch = sz;
	subResourceData.SysMemSlicePitch = 0;
	ID3D11Buffer *buffer;
	device->CreateBuffer(&bufferDesc, &subResourceData, &buffer);
	m->d3dVerts = buffer;
	bufferDesc.ByteWidth = m->nTriangles * 12U;
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	subResourceData.pSysMem = indices;
	subResourceData.SysMemPitch = 4;
	device->CreateBuffer(&bufferDesc, &subResourceData, &buffer);
	m->d3dIndices = buffer;
}
void deleteModel(Model *m) {
	ID3D11Buffer *buffer = static_cast<ID3D11Buffer *>(m->d3dVerts);
	buffer->Release();
	buffer = static_cast<ID3D11Buffer *>(m->d3dIndices);
	buffer->Release();
}


/*
 * INIT/FINI
 */
static void createTargetSurface(TargetSurface &surface) {
	D3D11_TEXTURE2D_DESC textureDesc = { 0 };
	textureDesc.Width = realWinW;
	textureDesc.Height = realWinH;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	//textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	ID3D11Texture2D *texture;
	HRESULT hr = device->CreateTexture2D(&textureDesc, nullptr, &texture);
	if (FAILED(hr)) {
		fail("CreateTexture2D failed with %x\n", hr);
	}
	surface.color.d3dTexture = texture;
	hr = device->CreateRenderTargetView(texture, nullptr, &surface.colorView);
	if (FAILED(hr)) {
		fail("CreateRenderTargetView with %x\n", hr);
	}
	ID3D11ShaderResourceView *shaderResourceView;
	hr = device->CreateShaderResourceView(texture, nullptr, &shaderResourceView);
	if (FAILED(hr)) {
		fail("CreateShaderResourceView with %x\n", hr);
	}
	surface.color.d3dResourceView = shaderResourceView;
	surface.color.w = realWinW;
	surface.color.h = realWinH;
	//surface.color.flags |= TEXTURE_POINT;

	textureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	hr = device->CreateTexture2D(&textureDesc, nullptr, &texture);
	if (FAILED(hr)) {
		fail("CreateTexture2D failed with %x\n", hr);
	}
	surface.depthStencil.d3dTexture = texture;
	hr = device->CreateDepthStencilView(texture, nullptr, &surface.depthStencilView);
	if (FAILED(hr)) {
		fail("CreateRenderTargetView with %x\n", hr);
	}
	surface.depthStencil.w = realWinW;
	surface.depthStencil.h = realWinH;
	surface.depthStencil.flags |= TEXTURE_POINT;
}

#define TITLE L"Dreaming Memories"
void drawDriverInit(void) {
	drawVmApi = 1;
	winW = 854;
	winH = 480;
	realWinW = engineSettings->resW;
	realWinH = engineSettings->resH;

	window = SDL_CreateWindow(engineSettings->gameTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, realWinW, realWinH, 0);
	if (!window) {
		fail("SDL_CreateWindow: %s\n", SDL_GetError());
	}
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(window, &wmInfo);
	hWnd = wmInfo.info.win.window;

	DXGI_SWAP_CHAIN_DESC scd = { 0 };
	scd.BufferDesc.RefreshRate.Numerator = 0;
	scd.BufferDesc.RefreshRate.Denominator = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	scd.SampleDesc.Count = 1;
	scd.SampleDesc.Quality = 0;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.BufferCount = 2;
	//scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; /* allows tearing, makes it so that 50hz displays don't slow down */
	scd.OutputWindow = hWnd;
	scd.Windowed = true;

	UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef RELEASE
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	D3D_FEATURE_LEVEL featureLevel;
	HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, flags,
		nullptr, 0,
		D3D11_SDK_VERSION,
		&scd,
		&swapChain,
		&device,
		&featureLevel,
		&deviceContext);
	if (FAILED(hr)) {
		fail("D3D11CreateDeviceAndSwapChain failed with: %x\n", hr);
	}

	swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&framebuffer));
	hr = device->CreateRenderTargetView(framebuffer, nullptr, &framebufferView);
	if (FAILED(hr)) {
		fail("CreateRenderTargetView failed with: %x\n", hr);
	}

	D3D11_BUFFER_DESC bufferDesc = { 0 };
	bufferDesc.ByteWidth = VERTEX_BUFFER_SIZE;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device->CreateBuffer(&bufferDesc, nullptr, &streamVertexBuffer);
	if (FAILED(hr)) {
		fail("CreateBuffer failed with: %x\n", hr);
	}

	bufferDesc.ByteWidth = INDEX_BUFFER_SIZE;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device->CreateBuffer(&bufferDesc, nullptr, &streamIndexBuffer);
	if (FAILED(hr)) {
		fail("CreateBuffer failed with: %x\n", hr);
	}

	bufferDesc.ByteWidth = sizeof(StdConstantVS);
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device->CreateBuffer(&bufferDesc, nullptr, &stdConstantVSBuffer);
	if (FAILED(hr)) {
		fail("CreateBuffer failed with: %x\n", hr);
	}
	bufferDesc.ByteWidth = DRAW_MAX_BONE * sizeof(Mat);
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device->CreateBuffer(&bufferDesc, nullptr, &stdConstantVSAnimBuffer);
	if (FAILED(hr)) {
		fail("CreateBuffer failed with: %x\n", hr);
	}
	bufferDesc.ByteWidth = sizeof(StdConstantPS);
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device->CreateBuffer(&bufferDesc, nullptr, &stdConstantPSBuffer);
	if (FAILED(hr)) {
		fail("CreateBuffer failed with: %x\n", hr);
	}
	bufferDesc.ByteWidth = sizeof(StdConstantPSScene);
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device->CreateBuffer(&bufferDesc, nullptr, &stdConstantPSSceneBuffer);
	if (FAILED(hr)) {
		fail("CreateBuffer failed with: %x\n", hr);
	}

	stdShaders[SHADER_2D] = drawShaderNew("shaders/std_vs.cso", "shaders/std_ps_2d.cso");
	stdShaders[SHADER_3D] = drawShaderNew("shaders/std_vs.cso", "shaders/std_ps_3d.cso");
	stdShaders[SHADER_3D_CLIP] = drawShaderNew("shaders/std_vs.cso", "shaders/std_ps_3d_clip.cso");
	stdShaders[SHADER_3D_ANIM] = drawShaderNew("shaders/anim_vs.cso", "shaders/std_ps_3d.cso");
	stdShaders[SHADER_3D_POST] = drawShaderNew("shaders/std_vs.cso", "shaders/std_ps_post_3d.cso");

	D3D11_RASTERIZER_DESC rasterizerDesc = { 0 };
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.FrontCounterClockwise = TRUE;
	rasterizerDesc.DepthClipEnable = TRUE;
	device->CreateRasterizerState(&rasterizerDesc, &rasterize2D);
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	device->CreateRasterizerState(&rasterizerDesc, &rasterize3D);
	rasterizerDesc.CullMode = D3D11_CULL_FRONT;
	device->CreateRasterizerState(&rasterizerDesc, &rasterize3DInvert);

	D3D11_SAMPLER_DESC samplerDesc = { 0 };
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	hr = device->CreateSamplerState(&samplerDesc, &linearSampler);
	if (FAILED(hr)) {
		fail("CreateSamplerState failed with: %x\n", hr);
	}
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	hr = device->CreateSamplerState(&samplerDesc, &pointSampler);
	if (FAILED(hr)) {
		fail("CreateSamplerState failed with: %x\n", hr);
	}

	D3D11_BLEND_DESC blendDesc = { 0 };
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	hr = device->CreateBlendState(&blendDesc, &blendStates[BLEND_ALPHA]);
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	hr = device->CreateBlendState(&blendDesc, &blendStates[BLEND_MULTIPLY]);
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	hr = device->CreateBlendState(&blendDesc, &blendStates[BLEND_ADD]);
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	hr = device->CreateBlendState(&blendDesc, &blendStates[BLEND_REPLACE]);
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_INV_DEST_COLOR;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
	hr = device->CreateBlendState(&blendDesc, &blendStates[BLEND_SCREEN]);

	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	hr = device->CreateBlendState(&blendDesc, &blendStates[BLEND_SUBTRACT]);

	drawState.blend = static_cast<BlendMode>(-1);

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = { 0 };
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthStencilDesc.StencilEnable = FALSE;
	hr = device->CreateDepthStencilState(&depthStencilDesc, &depthStencil3D);
	if (FAILED(hr)) {
		fail("CreateDepthStencilState failed with: %x\n", hr);
	}
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	hr = device->CreateDepthStencilState(&depthStencilDesc, &depthStencil3DNoWrite);
	if (FAILED(hr)) {
		fail("CreateDepthStencilState failed with: %x\n", hr);
	}
	depthStencilDesc.DepthEnable = FALSE;
	hr = device->CreateDepthStencilState(&depthStencilDesc, &depthStencil2D);
	if (FAILED(hr)) {
		fail("CreateDepthStencilState failed with: %x\n", hr);
	}


	createTargetSurface(surface1);
	createTargetSurface(surface2);
	surface2.color.flags |= TEXTURE_POINT;

	/* Set the updates */
	addDrawUpdate(0, drawStart, NULL);
	addDrawUpdate(engineSettings->draw3DStart, drawStart3D, NULL);
	addDrawUpdate(engineSettings->draw3DNoCull, drawDisableCull, NULL);
	addDrawUpdate(engineSettings->draw3DOverlay, draw3DSetOverlay, NULL);
	addDrawUpdate(engineSettings->draw2DLowRes, draw2DLowRes, NULL);
	addDrawUpdate(engineSettings->draw2DHiRes, draw2DHiRes, NULL);
	addDrawUpdate(engineSettings->drawRttEnd, drawRttEnd, NULL);
	addDrawUpdate(9999, drawEnd, NULL);

	cam3DRotate(0, 0, 10, 0, 0, 0);
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
}


void drawSetResolution(int w, int h) {
}

void showError(const char *title, const char *message) {
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, window);
}

struct Texture *loadTextureFromPixels(int w, int h, unsigned char *pixels, int flags) {
	D3D11_TEXTURE2D_DESC textureDesc = { 0 };
	textureDesc.Width = w;
	textureDesc.Height = h;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = flags & TEXTURE_SRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	textureDesc.MiscFlags = 0;
	if (flags & TEXTURE_MIPMAP) {
		textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.MipLevels = 1;
	}

	D3D11_SUBRESOURCE_DATA textureData;
	textureData.pSysMem = pixels;
	textureData.SysMemPitch = w * 4;

	ID3D11Texture2D *texture2D;
	HRESULT hr = device->CreateTexture2D(&textureDesc, &textureData, &texture2D);
	if (FAILED(hr)) {
		logNorm("CreateTexture2D Failed with %x\n", hr);
		return nullptr;
	}
	ID3D11ShaderResourceView *resourceView;
	hr = device->CreateShaderResourceView(texture2D, nullptr, &resourceView);
	if (FAILED(hr)) {
		logNorm("CreateShaderResourceView Failed with %x\n", hr);
		return nullptr;
	}

	if (flags & TEXTURE_MIPMAP) {
		deviceContext->GenerateMips(resourceView);
	}

	Texture *tex = new Texture;
	tex->w = w;
	tex->h = h;
	tex->flags = flags;
	tex->refs = 1;
	tex->d3dTexture = texture2D;
	tex->d3dResourceView = resourceView;

	return tex;
}
struct Texture *loadTextureCube(const char *name) {
	return nullptr; // TODO
}

void deleteTexture(struct Texture *texture) {
	if (!texture)
		return;

	texture->refs -= 1;
	if (texture->refs > 0)
		return;

	ID3D11ShaderResourceView *resourceView = static_cast<ID3D11ShaderResourceView *>(texture->d3dResourceView);
	resourceView->Release();
	ID3D11Texture2D *texture2D = static_cast<ID3D11Texture2D *>(texture->d3dTexture);
	texture2D->Release();
	delete texture;
}