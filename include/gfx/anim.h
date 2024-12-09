#ifndef ANIM_H
#define ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

#define DRAW_MAX_BONE 32

#define ANIM_FLAG_SINGLE 1
#define ANIM_FLAG_ENDED 2

struct Model;

struct PoseFileBone {
	char name[32];
	int parent;
	float inverseBindMat[16];

	float trans[3];
	float rot[4];
	float scale[3];
};
struct PoseFileVec3 {
	float t;
	float v[3];
};
struct PoseFileVec4 {
	float t;
	float v[4];
};
enum PoseFileAnimChannelType {
	POSE_TRANSLATE,
	POSE_ROTATE,
	POSE_SCALE
};
struct PoseFileAnimChannel {
	size_t size;
	unsigned int bone;
	enum PoseFileAnimChannelType type;
	unsigned int nEntries;
};
struct PoseFileAnim {
	char name[32];
	size_t size;
	unsigned int nChannels;
	float duration;
};
struct PoseFileHeader {
	char sig[4];
	unsigned int version;
	unsigned int nBones;
	unsigned int nAnim;
};
struct LoadedPoseFile {
	struct PoseFileHeader hdr;
};

struct Anim3DBoneState {
	float x, y, z;
	float rx, ry, rz, rw;
	float sx, sy, sz;
};
struct Anim3DState {
	entity_t entity;
	struct LoadedPoseFile *poseFile;
	const char *animName;

	void *globals;
	void (*event)(struct Anim3DState *self);

	int flags;

	float animSpeed;
	float animTime, lerpTime;
	struct Anim3DBoneState lerpState[DRAW_MAX_BONE];
};


struct LoadedPoseFile *loadPoseFile(const char *name);
void deletePoseFile(struct LoadedPoseFile *lpf);

void drawAnim(struct Model *m, struct Anim3DState *s);

float anim3DLength(struct Anim3DState *s); /* Returns duration of current animation in frames */

void anim3DInit(void);
void anim3DFini(void);

#ifdef __cplusplus
} // extern "C"

constexpr ClWrapper<Anim3DState, ANIM_STATE> ANIM_STATES;
#endif

#endif
