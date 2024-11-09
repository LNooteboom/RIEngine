#include "ich.h"

int ichigoVmNew(struct IchigoState *state, struct IchigoVm *newVm, ENTITY en) {
	newVm->en = en;
	newVm->is = state;

	for (int i = 0; i < ICHIGO_VM_MAX_COROUT; i++) {
		newVm->coroutines[i].active = false;
	}

	return 0;
}
void ichigoVmDelete(struct IchigoVm *vm) {
	ichigoVmKillAll(vm);
}

struct IchigoFn *ichFindFn(struct IchigoState *state, const char *name) {
	int nameLen = (int)strlen(name);
	for (int i = 0; i < state->fns.nElements; i++) {
		struct IchigoFn **fnp = ichVecAt(&state->fns, i);
		struct IchigoFn *fn = *fnp;
		const char *fnName = (const char *)(fn + 1) + fn->paramCount;
		if (nameLen == fn->nameLen && !memcmp(fnName, name, nameLen)) {
			return fn;
		}
	}
	return NULL;
}

static void ichPushFnArg(struct IchigoCorout *co, struct IchigoReg *reg) {
	struct IchigoCallFrame *cf = ichVecAt(&co->callFrames, co->callFrames.nElements - 1);
	struct IchigoReg *dest = ichVecAppend(&co->regs);
	memcpy(dest, reg, sizeof(*dest));
	cf->nArgs++;
	cf->regBase++;
}

static int ichPushFn(struct IchigoFn **icf, struct IchigoState *is, struct IchigoCorout *co, const char *fn) {
	struct IchigoFn *f = ichFindFn(is, fn);
	if (!f) {
		logError("Function %s not found\n", fn);
		return -1;
	}
	struct IchigoCallFrame *cf = ichVecAppend(&co->callFrames);
	if (co->callFrames.nElements > 1) {
		struct IchigoCallFrame *prevCf = ichVecAt(&co->callFrames, co->callFrames.nElements - 2);
		cf->regBase = prevCf->regBase + prevCf->nRegs;
	} else {
		cf->regBase = 0;
	}
	cf->nArgs = 0;
	cf->nRegs = 0;
	cf->retAddr = co->pc;
	int instrOffset = f->paramCount + f->nameLen;
	if (instrOffset % 2)
		instrOffset += 1;
	else
		instrOffset += 2;
	co->pc = (const uint16_t *)((const char *)(f + 1) + instrOffset);

	*icf = f;

	return 0;
}

int ichigoVmExec(struct IchigoVm *vm, const char *fn, const char *params, ...) {
	struct IchigoCorout *c = NULL;
	int coId;
	for (int i = 0; i < ICHIGO_VM_MAX_COROUT; i++) {
		if (!vm->coroutines[i].active) {
			c = &vm->coroutines[i];
			coId = i;
			break;
		}
	}
	if (!c) {
		logError("No available coroutines\n");
		return -1;
	}

	struct IchigoFn *f = ichFindFn(vm->is, fn);
	if (!f) {
		logError("Function %s not found\n", fn);
		return -1;
	}

	ichVecCreate(&c->callFrames, sizeof(struct IchigoCallFrame));
	ichVecCreate(&c->regs, sizeof(struct IchigoReg));

	struct IchigoFn *icf;
	int err = ichPushFn(&icf, vm->is, c, fn);
	if (err)
		return err;

	if ((int)strlen(params) != icf->paramCount) {
		logError("Ichigo exec %s: Incorrect number of params\n", fn);
	}

	va_list pList;
	va_start(pList, params);
	for (int i = 0; params[i]; i++) {
		struct IchigoReg reg = { 0 };
		switch (params[i]) {
		case 'e':
			reg.regType = REG_ENTITY;
			reg.e = va_arg(pList, entity_t);
			break;
		case 'i':
			reg.regType = REG_INT;
			reg.i = va_arg(pList, int);
			break;
		case 'f':
			reg.regType = REG_FLOAT;
			reg.f = va_arg(pList, double);
			break;
		default:
			logError("Undefined arg: %c\n", params[i]);
			return -1;
		}
		ichPushFnArg(c, &reg);
	}

	c->active = true;
	c->waitTime = 0;
	return coId;
}

void ichigoVmKill(struct IchigoVm *vm, int coroutine) {
	struct IchigoCorout *c = &vm->coroutines[coroutine];
	if (c->active) {
		c->active = false;
		ichVecDestroy(&c->callFrames);
		for (unsigned int i = 0; i < c->regs.nElements; i++) {
			ichDeleteReg(ichVecAt(&c->regs, i));
		}
		ichVecDestroy(&c->regs);
	}
}

void ichigoVmKillAll(struct IchigoVm *vm) {
	for (int i = 0; i < ICHIGO_VM_MAX_COROUT; i++) {
		ichigoVmKill(vm, i);
	}
}