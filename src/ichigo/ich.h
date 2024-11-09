#ifndef ICH_H
#define ICH_H

#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "../../tools/ichigo/instr.h"

#include <ichigo.h>
#include <ecs.h>

#define T_REG 0
#define T_STATIC_ARRAY 1
#define T_ARRAY 2
#define T_OBJ 3
#define T_REG_REF 4
#define T_EXTERN_REF 5
#define T_OBJ_REF 6

#define GET_REG(arg) ((int)(*((int16_t *)arg->ptr)))
#define GET_INT(arg) (*((int *)arg->ptr))
#define GET_FLOAT(arg) (*((float *)arg->ptr))
#define IS_EXTERN(arg) (GET_REG(arg) <= -0x6000)
#define GET_EXTERN(r) (-r - 0x6000)


static inline void *ichVecAt(struct IchigoVector *vec, unsigned int index) {
	//return (index < vec->nElements) ? (void *)&vec->data[index * vec->elementSize] : NULL;
	return (void *)&vec->data[index * vec->elementSize];
}

static inline struct IchigoHeapObject *ichHeapNew(void) {
	return newComponent(ICHIGO_HEAP_OBJ, newEntity());
}
static inline struct IchigoHeapObject *ichHeapGet(ENTITY en) {
	return getComponentOpt(ICHIGO_HEAP_OBJ, en);
}
static inline void ichHeapDelete(ENTITY en) {
	deleteEntity(en);
}

void ichVecCreate(struct IchigoVector *vec, unsigned int elementSize);
void ichVecDestroy(struct IchigoVector *vec);
void *ichVecAppend(struct IchigoVector *vec);
void ichVecDelete(struct IchigoVector *vec, unsigned int index);

struct IchigoFn *ichFindFn(struct IchigoState *state, const char *name);

struct IchigoVar *ichGetExternVar(struct IchigoState *state, int reg);
struct IchigoReg *ichGetReg(struct IchigoState *state, struct IchigoCallFrame *cf, int reg);
void ichCreateRegRef(struct IchigoState *state, struct IchigoCallFrame *cf, struct IchigoReg *dst, struct IchigoInstrArg *arg, int arrayIdx);
void ichDeleteReg(struct IchigoReg *reg);

#endif