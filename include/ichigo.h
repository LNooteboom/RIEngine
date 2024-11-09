#ifndef ICHIGO_INTERP_H
#define ICHIGO_INTERP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ICHIGO_PATH_MAX 64
#define ICHIGO_VM_MAX_COROUT 16
#define ICHIGO_MAX_INSTR_ARGS 64

/* DEFINE THESE FUNCTIONS */
size_t ichLoadFile(const char **fileData, void **userData, const char *fileName); /* returns data length or zero on error */
void ichFreeFile(void *userData);

/* ENGINE SPECIFIC */
#include <mem.h>
#include <assets.h>
#include <ecs.h>
#define ENTITY entity_t
#define ichAlloc(sz) globalAlloc(sz)
#define ichFree(ptr) globalDealloc(ptr)
#define ichRealloc(ptr, sz) globalRealloc(ptr, sz)
#define logError(...) logNorm(__VA_ARGS__);

struct IchigoVector {
	unsigned int elementSize;
	uint16_t nElements;
	uint16_t nAllocations;
	char *data;
};


struct IchigoLoadedFile {
	char path[ICHIGO_PATH_MAX];

	void *userData;
	const uint16_t *data;
	size_t dataLen2;
};


struct IchigoState;

#define REG_NIL 0
#define REG_INT 1
#define REG_FLOAT 2
#define REG_ENTITY 3
#define REG_BYTE 4
#define REG_STRING 4 /* Used in externs only */

struct IchigoReg {
	uint8_t regType, refType;
	uint32_t sLen;
	union {
		int i;
		float f;
		void *s;
		ENTITY e;
	};
};

struct IchigoCallFrame {
	int regBase;
	uint16_t nArgs;
	uint16_t nRegs;
	const uint16_t *retAddr;
};

struct IchigoCorout {
	bool active;

	float waitTime;

	const uint16_t *pc; /* ret addr is pc + call length */

	struct IchigoVector regs;
	struct IchigoVector callFrames;
};

struct IchigoVm {
	ENTITY en;
	struct IchigoState *is;

	struct IchigoCorout coroutines[ICHIGO_VM_MAX_COROUT];
};

typedef void IchigoInstr(struct IchigoVm *vm);

union IchigoGetter {
	int (*i)(struct IchigoVm *vm);
	float (*f)(struct IchigoVm *vm);
	const char *(*s)(struct IchigoVm *vm, uint16_t *sLen);
	ENTITY (*e)(struct IchigoVm *vm);
};

union IchigoSetter {
	void (*i)(struct IchigoVm *vm, int i);
	void (*f)(struct IchigoVm *vm, float f);
	void (*s)(struct IchigoVm *vm, const char *s, uint16_t sLen);
	void (*e)(struct IchigoVm *vm, ENTITY e);
};

struct IchigoVar {
	int regType;
	union IchigoGetter get;
	union IchigoSetter set;
};

struct IchigoInstrArg {
	int type;
	const uint16_t *ptr;
};

struct IchigoState {
	struct IchigoVector files;
	struct IchigoVector fns;
	struct IchigoVector globals;
	struct IchigoVector vms;

	const char *baseDir;

	/* Custom instructions */
	IchigoInstr **instrs;
	int nInstrs;
	struct IchigoVar *vars;
	int nVars;

	/* Current instruction state */
	uint16_t curInstr;
	int curInstrLen;
	int nArgs;
	struct IchigoInstrArg args[ICHIGO_MAX_INSTR_ARGS];

	struct IchigoVm *curVm;
	struct IchigoCorout *curCorout;
	int curCoroutId;
};

struct IchigoHeapObject {
	ENTITY entity;
	int refCount;
	uint32_t len;
	void *data;
};

void ichigoHeapInit(void);
void ichigoHeapFini(void);

void ichigoInit(struct IchigoState *newState, const char *baseDir);
void ichigoFini(struct IchigoState *state);
int ichigoAddFile(struct IchigoState *state, const char *file);
void ichigoClear(struct IchigoState *state);

void ichigoSetInstrTable(struct IchigoState *state, IchigoInstr **instrs, int nInstrs);
void ichigoSetVarTable(struct IchigoState *state, struct IchigoVar *vars, int nVars);

int ichigoVmNew(struct IchigoState *state, struct IchigoVm *newVm, ENTITY en);
void ichigoVmDelete(struct IchigoVm *vm);
int ichigoVmExec(struct IchigoVm *vm, const char *fn, const char *params, ...); /* returns coroutine ID (or negative if error) */
void ichigoVmKill(struct IchigoVm *vm, int coroutine);
void ichigoVmKillAll(struct IchigoVm *vm);
void ichigoVmUpdate(struct IchigoVm *vm, float time);

int ichigoGetInt(struct IchigoVm *vm, int arg);
void ichigoSetInt(struct IchigoVm *vm, int arg, int val);
float ichigoGetFloat(struct IchigoVm *vm, int arg);
void ichigoSetFloat(struct IchigoVm *vm, int arg, float val);
ENTITY ichigoGetEntity(struct IchigoVm *vm, int arg);
void ichigoSetEntity(struct IchigoVm *vm, int arg, ENTITY val);
const char *ichigoGetString(uint16_t *strLen, struct IchigoVm *vm, int arg);
void ichigoSetString(struct IchigoVm *vm, int arg, const char *str, uint16_t strLen);

const void *ichigoGetArray(uint16_t *len, struct IchigoVm *vm, int arg, int regType);
void ichigoSetArray(struct IchigoVm *vm, int arg, const void *data, uint32_t len, int regType);
struct IchigoHeapObject *ichigoGetArrayMut(struct IchigoVm *vm, int arg);
struct IchigoHeapObject *ichigoSetArrayMut(struct IchigoVm *vm, int arg, const void *data, uint32_t len, int regType);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
