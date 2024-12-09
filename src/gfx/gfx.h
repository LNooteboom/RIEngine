#ifndef GFX_H
#define GFX_H

#ifdef __cplusplus
extern "C" {
#endif

struct Model;

// Defined in driver
void uploadModel(struct Model *m, void *verts, void *indices);
void deleteModel(struct Model *m);
void drawDriverInit();
void drawDriverFini();

void drawVmInit(void);
void drawVmFini(void);

void ttfInit(void);
void ttfFini(void);

void anim3DInit(void);
void anim3DFini(void);

void drawSetAnimUbo(void *data, size_t dataSize);

void drawUpdateFrustum(const struct Mat *projMat);

#ifdef __cplusplus
}
#endif

#endif