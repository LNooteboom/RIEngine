#include "ich.h"

struct IchigoVar *ichGetExternVar(struct IchigoState *state, int reg) {
	if (!state->vars || reg >= state->nVars) {
		logError("No script variable: %d\n", reg);
		return NULL;
	}
	return &state->vars[reg];
}

struct IchigoReg *ichGetReg(struct IchigoState *state, struct IchigoCallFrame *cf, int reg) {
	struct IchigoCorout *co = state->curCorout;
	int r = reg >= 0 ? cf->regBase + reg : cf->regBase - cf->nArgs - (reg + 1);

	if (r < 0 || r >= 0x1000) {
		logError("Register out of range: %d\n", r);
		return NULL;
	}

	if (reg >= cf->nRegs) {
		cf->nRegs = reg + 1;
		if (r >= co->regs.nElements) {
			if (r >= co->regs.nAllocations) {
				/* Allocate new regs */
				int st = co->regs.nAllocations;
				co->regs.nAllocations = r + 4;
				co->regs.data = ichRealloc(co->regs.data, co->regs.nAllocations * co->regs.elementSize);
				for (int i = st; i < co->regs.nAllocations; i++) {
					memset(ichVecAt(&co->regs, i), 0, sizeof(struct IchigoReg));
				}
			}
			co->regs.nElements = r + 1;
			memset(ichVecAt(&co->regs, r), 0, sizeof(struct IchigoReg));
		}
	}
	return ichVecAt(&co->regs, r);
}

void ichCreateRegRef(struct IchigoState *state, struct IchigoCallFrame *cf, struct IchigoReg *dst, struct IchigoInstrArg *arg, int arrayIdx) {
	if (arg->type == PARAM_REG) {
		int reg = GET_REG(arg);
		if (IS_EXTERN(arg)) {
			struct IchigoVar *var = ichGetExternVar(state, GET_EXTERN(reg));
			if (!var)
				return;
			dst->regType = var->regType;
			dst->refType = T_EXTERN_REF;
			dst->sLen = 0;
			dst->i = GET_EXTERN(reg);
		} else {
			struct IchigoReg *r = ichGetReg(state, cf, reg);
			dst->regType = r->regType;
			dst->refType = T_REG_REF;
			dst->sLen = arrayIdx;
			dst->i = reg >= 0 ? cf->regBase + reg : cf->regBase - cf->nArgs - (reg + 1);
		}
	} else if (arg->type == PARAM_REG_IND) {
		struct IchigoReg *src = ichGetReg(state, cf, GET_REG(arg));
		dst->regType = src->regType;
		dst->refType = src->refType;
		dst->sLen = src->sLen;
		dst->s = src->s;
	}
}

static struct IchigoReg *ichGetCurrentCfReg(struct IchigoState *state, struct IchigoInstrArg *a) {
	struct IchigoCorout *co = state->curCorout;
	return ichGetReg(state, ichVecAt(&co->callFrames, co->callFrames.nElements - 1), GET_REG(a));
}

void ichDeleteReg(struct IchigoReg *reg) {
	if (reg->refType == T_ARRAY || reg->refType == T_OBJ) {
		struct IchigoHeapObject *ho = ichHeapGet(reg->e);
		if (ho) {
			ho->refCount -= 1;
			if (ho->refCount == 0) {
				ichHeapDelete(ho->entity);
			}
		}
		
	}
	reg->refType = T_REG;
	reg->regType = REG_NIL;
	reg->sLen = 0;
	reg->s = NULL;
}


/* INT */

static int ichGetIntExtern(struct IchigoVar *var, struct IchigoVm *vm) {
	if (!var || !var->get.i) {
		logError("Extern does not exist");
		return 0;
	}
	if (var->regType == REG_INT) {
		return var->get.i(vm);
	} else if (var->regType == REG_FLOAT) {
		return var->get.f(vm);
	} else {
		logError("Cannot do extern var conversion\n");
		return 0;
	}
}

static int ichGetIntReg(struct IchigoReg *reg, uint32_t idx) {
	if (reg->refType == T_REG) {
		if (reg->regType == REG_INT) {
			return reg->i;
		} else if (reg->regType == REG_FLOAT) {
			return reg->f;
		} else {
			logError("Invalid reg conversion\n");
		}
	} else if (reg->refType == T_STATIC_ARRAY) {
		if (idx < reg->sLen) {
			if (reg->regType == REG_INT) {
				return ((int *)reg->s)[idx];
			} else if (reg->regType == REG_FLOAT) {
				return ((float *)reg->s)[idx];
			} else {
				logError("Invalid reg conversion\n");
			}
		} else {
			logError("Array read out of range\n");
		}
	} else if (reg->refType == T_ARRAY) {
		struct IchigoHeapObject *ho = ichHeapGet(reg->e);
		if (ho && idx < ho->len) {
			if (reg->regType == REG_INT) {
				return ((int *)ho->data)[idx];
			} else if (reg->regType == REG_FLOAT) {
				return ((float *)ho->data)[idx];
			} else {
				logError("Invalid reg conversion\n");
			}
		} else {
			logError("Array read out of range\n");
		}
	} else {
		logError("Register is not a scalar or array value");
	}
	return 0;
}

int ichigoGetInt(struct IchigoVm *vm, int arg) {
	struct IchigoState *state = vm->is;
	if (arg >= state->nArgs) {
		logError("Argument %d is out of range\n", arg);
		return 0;
	}
	struct IchigoInstrArg *a = &state->args[arg];
	if (a->type == PARAM_4) {
		return GET_INT(a);
	} else if (a->type == PARAM_REG) {
		int r = GET_REG(a);
		if (IS_EXTERN(a)) {
			struct IchigoVar *var = ichGetExternVar(state, GET_EXTERN(GET_REG(a)));
			return ichGetIntExtern(var, vm);
		} else {
			struct IchigoReg *reg = ichGetCurrentCfReg(state, a);
			return ichGetIntReg(reg, 0);
		}
	} else if (a->type == PARAM_REG_IND) {
		struct IchigoReg *ref = ichGetCurrentCfReg(state, a);
		if (ref->refType == T_REG_REF) {
			return ichGetIntReg(ichVecAt(&state->curCorout->regs, ref->i), ref->sLen);
		} else if (ref->refType == T_EXTERN_REF) {
			return ichGetIntExtern(ichGetExternVar(state, ref->i), vm);
		}
	}
	logError("Invalid instruction arg\n");
	return 0;
}

static void ichSetIntExtern(struct IchigoVar *var, struct IchigoVm *vm, int val) {
	if (!var || !var->set.i) {
		logError("Extern does not allow writing");
		return;
	}
	if (var->regType == REG_INT) {
		var->set.i(vm, val);
	} else if (var->regType == REG_FLOAT) {
		var->set.f(vm, val);
	} else {
		logError("Cannot do extern var conversion\n");
	}
}

void ichigoSetInt(struct IchigoVm *vm, int arg, int val) {
	struct IchigoState *state = vm->is;
	if (arg >= state->nArgs) {
		logError("Argument %d is out of range\n", arg);
		return;
	}
	struct IchigoInstrArg *a = &state->args[arg];
	if (a->type == PARAM_REG) {
		if (IS_EXTERN(a)) {
			struct IchigoVar *var = ichGetExternVar(state, GET_EXTERN(GET_REG(a)));
			ichSetIntExtern(var, vm, val);
			return;
		} else {
			struct IchigoReg *reg = ichGetCurrentCfReg(state, a);
			ichDeleteReg(reg);
			reg->regType = REG_INT;
			reg->refType = T_REG;
			reg->sLen = 1;
			reg->i = val;
			return;
		}
	} else if (a->type == PARAM_REG_IND) {
		struct IchigoReg *ref = ichGetCurrentCfReg(state, a);
		if (ref->refType == T_REG_REF) {
			struct IchigoReg *reg = ichVecAt(&state->curCorout->regs, ref->i);
			if (reg->refType == T_ARRAY) {
				struct IchigoHeapObject *ho = ichHeapGet(reg->e);
				if (ho && ref->sLen < ho->len) {
					if (reg->regType == REG_INT) {
						((int *)ho->data)[ref->sLen] = val;
						return;
					} else if (reg->regType == REG_FLOAT) {
						((float *)ho->data)[ref->sLen] = val;
						return;
					}
				} else {
					logError("Array write out of range\n");
					return;
				}
			} else {
				ichDeleteReg(reg);
				reg->regType = REG_INT;
				reg->refType = T_REG;
				reg->sLen = 1;
				reg->i = val;
			}
			return;
		} else if (ref->refType == T_EXTERN_REF) {
			ichSetIntExtern(ichGetExternVar(state, ref->i), vm, val);
			return;
		}
	}
	logError("Invalid instruction arg\n");
	return;
}



/* FLOAT, same as int but different return/set type */

static float ichGetFloatExtern(struct IchigoVar *var, struct IchigoVm *vm) {
	if (!var || !var->get.i) {
		logError("Extern does not exist");
		return 0;
	}
	if (var->regType == REG_INT) {
		return var->get.i(vm);
	} else if (var->regType == REG_FLOAT) {
		return var->get.f(vm);
	} else {
		logError("Cannot do extern var conversion\n");
		return 0;
	}
}

static float ichGetFloatReg(struct IchigoReg *reg, uint32_t idx) {
	if (reg->refType == T_REG) {
		if (reg->regType == REG_INT) {
			return reg->i;
		} else if (reg->regType == REG_FLOAT) {
			return reg->f;
		} else {
			logError("Invalid reg conversion\n");
		}
	} else if (reg->refType == T_STATIC_ARRAY) {
		if (idx < reg->sLen) {
			if (reg->regType == REG_INT) {
				return ((int *)reg->s)[idx];
			} else if (reg->regType == REG_FLOAT) {
				return ((float *)reg->s)[idx];
			} else {
				logError("Invalid reg conversion\n");
			}
		} else {
			logError("Array read out of range\n");
		}
	} else if (reg->refType == T_ARRAY) {
		struct IchigoHeapObject *ho = ichHeapGet(reg->e);
		if (ho && idx < ho->len) {
			if (reg->regType == REG_INT) {
				return ((int *)ho->data)[idx];
			} else if (reg->regType == REG_FLOAT) {
				return ((float *)ho->data)[idx];
			} else {
				logError("Invalid reg conversion\n");
			}
		} else {
			logError("Array read out of range\n");
		}
	} else {
		logError("Register is not a scalar or array value");
	}
	return 0;
}

float ichigoGetFloat(struct IchigoVm *vm, int arg) {
	struct IchigoState *state = vm->is;
	if (arg >= state->nArgs) {
		logError("Argument %d is out of range\n", arg);
		return 0;
	}
	struct IchigoInstrArg *a = &state->args[arg];
	if (a->type == PARAM_4) {
		return GET_FLOAT(a);
	} else if (a->type == PARAM_REG) {
		if (IS_EXTERN(a)) {
			struct IchigoVar *var = ichGetExternVar(state, GET_EXTERN(GET_REG(a)));
			return ichGetFloatExtern(var, vm);
		} else {
			struct IchigoReg *reg = ichGetCurrentCfReg(state, a);
			return ichGetFloatReg(reg, 0);
		}
	} else if (a->type == PARAM_REG_IND) {
		struct IchigoReg *ref = ichGetCurrentCfReg(state, a);
		if (ref->refType == T_REG_REF) {
			return ichGetFloatReg(ichVecAt(&state->curCorout->regs, ref->i), ref->sLen);
		} else if (ref->refType == T_EXTERN_REF) {
			return ichGetFloatExtern(ichGetExternVar(state, ref->i), vm);
		}
	}
	logError("Invalid instruction arg\n");
	return 0;
}

static void ichSetFloatExtern(struct IchigoVar *var, struct IchigoVm *vm, float val) {
	if (!var || !var->set.i) {
		logError("Extern does not allow writing");
		return;
	}
	if (var->regType == REG_INT) {
		var->set.i(vm, val);
	} else if (var->regType == REG_FLOAT) {
		var->set.f(vm, val);
	} else {
		logError("Cannot do extern var conversion\n");
	}
}

void ichigoSetFloat(struct IchigoVm *vm, int arg, float val) {
	struct IchigoState *state = vm->is;
	if (arg >= state->nArgs) {
		logError("Argument %d is out of range\n", arg);
		return;
	}
	struct IchigoInstrArg *a = &state->args[arg];
	if (a->type == PARAM_REG) {
		if (IS_EXTERN(a)) {
			struct IchigoVar *var = ichGetExternVar(state, GET_EXTERN(GET_REG(a)));
			ichSetFloatExtern(var, vm, val);
			return;
		} else {
			struct IchigoReg *reg = ichGetCurrentCfReg(state, a);
			ichDeleteReg(reg);
			reg->regType = REG_FLOAT;
			reg->refType = T_REG;
			reg->sLen = 1;
			reg->f = val;
			return;
		}
	} else if (a->type == PARAM_REG_IND) {
		struct IchigoReg *ref = ichGetCurrentCfReg(state, a);
		if (ref->refType == T_REG_REF) {
			struct IchigoReg *reg = ichVecAt(&state->curCorout->regs, ref->i);
			if (reg->refType == T_ARRAY) {
				struct IchigoHeapObject *ho = ichHeapGet(reg->e);
				if (ho && ref->sLen < ho->len) {
					if (reg->regType == REG_INT) {
						((int *)ho->data)[ref->sLen] = val;
						return;
					} else if (reg->regType == REG_FLOAT) {
						((float *)ho->data)[ref->sLen] = val;
						return;
					}
				} else {
					logError("Array write out of range\n");
					return;
				}
			} else {
				ichDeleteReg(reg);
				reg->regType = REG_INT;
				reg->refType = T_REG;
				reg->sLen = 1;
				reg->i = val;
			}
			return;
		} else if (ref->refType == T_EXTERN_REF) {
			ichSetFloatExtern(ichGetExternVar(state, ref->i), vm, val);
			return;
		}
	}
	logError("Invalid instruction arg\n");
	return;
}


/* ENTITY */

static ENTITY ichGetEntityExtern(struct IchigoVar *var, struct IchigoVm *vm) {
	if (!var || !var->get.i) {
		logError("Extern does not exist");
		return 0;
	}
	if (var->regType == REG_ENTITY) {
		return var->get.e(vm);
	} else {
		logError("Cannot do extern var conversion\n");
		return 0;
	}
}

static float ichGetEntityReg(struct IchigoReg *reg, uint32_t idx) {
	if (reg->refType == T_REG) {
		if (reg->regType == REG_ENTITY) {
			return reg->e;
		} else {
			logError("Invalid reg conversion\n");
		}
	} else if (reg->refType == T_STATIC_ARRAY) {
		if (idx < reg->sLen) {
			if (reg->regType == REG_ENTITY) {
				return ((entity_t *)reg->s)[idx];
			} else {
				logError("Invalid reg conversion\n");
			}
		} else {
			logError("Array read out of range\n");
		}
	} else if (reg->refType == T_ARRAY) {
		struct IchigoHeapObject *ho = ichHeapGet(reg->e);
		if (ho && idx < ho->len) {
			if (reg->regType == REG_ENTITY) {
				return ((entity_t *)ho->data)[idx];
			} else {
				logError("Invalid reg conversion\n");
			}
		} else {
			logError("Array read out of range\n");
		}
	} else {
		logError("Register is not a scalar or array value");
		return 0;
	}
	return 0;
}

ENTITY ichigoGetEntity(struct IchigoVm *vm, int arg) {
	struct IchigoState *state = vm->is;
	if (arg >= state->nArgs) {
		logError("Argument %d is out of range\n", arg);
		return 0;
	}
	struct IchigoInstrArg *a = &state->args[arg];
	if (a->type == PARAM_4) {
		return GET_INT(a);
	} else if (a->type == PARAM_REG) {
		if (IS_EXTERN(a)) {
			struct IchigoVar *var = ichGetExternVar(state, GET_EXTERN(GET_REG(a)));
			return ichGetEntityExtern(var, vm);
		} else {
			struct IchigoReg *reg = ichGetCurrentCfReg(state, a);
			return ichGetEntityReg(reg, 0);
		}
	} else if (a->type == PARAM_REG_IND) {
		struct IchigoReg *ref = ichGetCurrentCfReg(state, a);
		if (ref->refType == T_REG_REF) {
			return ichGetEntityReg(ichVecAt(&state->curCorout->regs, ref->i), 0);
		} else if (ref->refType == T_EXTERN_REF) {
			return ichGetEntityExtern(ichGetExternVar(state, ref->i), vm);
		}
	}
	logError("Invalid instruction arg\n");
	return 0;
}

static void ichSetEntityExtern(struct IchigoVar *var, struct IchigoVm *vm, ENTITY val) {
	if (!var || !var->set.i) {
		logError("Extern does not allow writing");
		return;
	}
	if (var->regType == REG_ENTITY) {
		var->set.e(vm, val);
	} else {
		logError("Cannot do extern var conversion\n");
	}
}

void ichigoSetEntity(struct IchigoVm *vm, int arg, ENTITY val) {
	struct IchigoState *state = vm->is;
	if (arg >= state->nArgs) {
		logError("Argument %d is out of range\n", arg);
		return;
	}
	struct IchigoInstrArg *a = &state->args[arg];
	if (a->type == PARAM_REG) {
		if (IS_EXTERN(a)) {
			struct IchigoVar *var = ichGetExternVar(state, GET_EXTERN(GET_REG(a)));
			ichSetEntityExtern(var, vm, val);
			return;
		} else {
			struct IchigoReg *reg = ichGetCurrentCfReg(state, a);
			ichDeleteReg(reg);
			reg->regType = REG_ENTITY;
			reg->refType = T_REG;
			reg->sLen = 1;
			reg->e = val;
			return;
		}
	} else if (a->type == PARAM_REG_IND) {
		struct IchigoReg *ref = ichGetCurrentCfReg(state, a);
		if (ref->refType == T_REG_REF) {
			struct IchigoReg *reg = ichVecAt(&state->curCorout->regs, ref->i);
			if (reg->refType == T_ARRAY) {
				struct IchigoHeapObject *ho = ichHeapGet(reg->e);
				if (ho && ref->sLen < ho->len) {
					if (reg->regType == REG_ENTITY) {
						((int *)ho->data)[ref->sLen] = val;
						return;
					}
				} else {
					logError("Array write out of range\n");
					return;
				}
			} else {
				ichDeleteReg(reg);
				reg->regType = REG_INT;
				reg->refType = T_REG;
				reg->sLen = 1;
				reg->i = val;
			}
			return;
		} else if (ref->refType == T_EXTERN_REF) {
			ichSetEntityExtern(ichGetExternVar(state, ref->i), vm, val);
			return;
		}
	}
	logError("Invalid instruction arg\n");
	return;
}



/* ARRAY */

const int dataElemSize[] = { 0, 4, 4, 4, 1 };
static const char *ichGetStringExtern(uint16_t *strLen, struct IchigoVar *var, struct IchigoVm *vm) {
	if (!var || !var->get.i) {
		logError("Extern does not exist");
		return 0;
	}
	if (var->regType == REG_STRING) {
		uint16_t sLen;
		const char *ret = var->get.s(vm, &sLen);
		if (strLen)
			*strLen = sLen;
		return ret;
	} else {
		logError("Cannot do extern var conversion\n");
		return 0;
	}
}

static void *ichGetArrayReg(uint16_t *len, struct IchigoReg *reg, int regType) {
	if (reg->regType != regType) {
		logError("Cannot do array conversion");
		return NULL;
	}
	if (reg->refType == T_STATIC_ARRAY) {
		if (len) {
			*len = reg->sLen;
		}
		return reg->s;
	} else if (reg->refType == T_ARRAY) {
		struct IchigoHeapObject *ho = ichHeapGet(reg->e);
		if (!ho) {
			return NULL;
		}
		if (len) {
			*len = ho->len;
		}
		return ho->data;
	}
	logError("Invalid reg conversion\n");
	return NULL;
}

const void *ichigoGetArray(uint16_t *len, struct IchigoVm *vm, int arg, int regType) {
	struct IchigoState *state = vm->is;
	if (arg >= state->nArgs) {
		logError("Argument %d is out of range\n", arg);
		return NULL;
	}
	struct IchigoInstrArg *a = &state->args[arg];
	if (a->type == PARAM_STR && regType == REG_BYTE) {
		if (len) {
			*len = *a->ptr;
		}
		return (const char *)(a->ptr + 1);
	} else if (a->type == PARAM_REG) {
		if (IS_EXTERN(a)) {
			struct IchigoVar *var = ichGetExternVar(state, GET_EXTERN(GET_REG(a)));
			return ichGetStringExtern(len, var, vm);
		} else {
			struct IchigoReg *reg = ichGetCurrentCfReg(state, a);
			return ichGetArrayReg(len, reg, regType);
		}
	} else if (a->type == PARAM_REG_IND) {
		struct IchigoReg *ref = ichGetCurrentCfReg(state, a);
		if (ref->refType == T_REG_REF) {
			return ichGetArrayReg(len, ichVecAt(&state->curCorout->regs, ref->i), regType);
		} else if (ref->refType == T_EXTERN_REF) {
			return ichGetStringExtern(len, ichGetExternVar(state, ref->i), vm);
		}
	}
	logError("Invalid instruction arg\n");
	return NULL;
}


static void ichSetStringExtern(struct IchigoVar *var, struct IchigoVm *vm, const char *str, uint16_t strLen) {
	if (!var || !var->set.i) {
		logError("Extern does not allow writing");
		return;
	}
	if (var->regType == REG_STRING) {
		var->set.s(vm, str, strLen);
	} else {
		logError("Cannot do extern var conversion\n");
	}
}

void ichigoSetArray(struct IchigoVm *vm, int arg, const void *data, uint32_t len, int regType) {
	struct IchigoState *state = vm->is;
	if (arg >= state->nArgs) {
		logError("Argument %d is out of range\n", arg);
		return;
	}
	struct IchigoInstrArg *a = &state->args[arg];
	if (a->type == PARAM_REG) {
		if (IS_EXTERN(a)) {
			struct IchigoVar *var = ichGetExternVar(state, GET_EXTERN(GET_REG(a)));
			ichSetStringExtern(var, vm, data, len);
			return;
		} else {
			struct IchigoReg *reg = ichGetCurrentCfReg(state, a);
			ichDeleteReg(reg);
			reg->regType = regType;
			reg->refType = T_STATIC_ARRAY;
			reg->sLen = len;
			reg->s = (void *)data;
			return;
		}
	} else if (a->type == PARAM_REG_IND) {
		struct IchigoReg *ref = ichGetCurrentCfReg(state, a);
		if (ref->refType == T_REG_REF) {
			struct IchigoReg *reg = ichVecAt(&state->curCorout->regs, ref->i);
			ichDeleteReg(reg);
			reg->regType = regType;
			reg->refType = T_STATIC_ARRAY;
			reg->sLen = len;
			reg->s = (void *)data;
			return;
		} else if (ref->refType == T_EXTERN_REF) {
			ichSetStringExtern(ichGetExternVar(state, ref->i), vm, data, len);
			return;
		}
	}
	logError("Invalid instruction arg\n");
	return;
}

static struct IchigoHeapObject *ichigoSetArrayMutReg(struct IchigoReg *reg, const void *data, uint32_t len, int regType) {
	ichDeleteReg(reg);
	struct IchigoHeapObject *ho = ichHeapNew();
	ho->refCount = 1;
	ho->len = len;

	size_t dataSz = len * dataElemSize[regType];
	if (regType == REG_BYTE)
		dataSz += 1;
	ho->data = globalAlloc(dataSz);
	if (data) {
		memcpy(ho->data, data, dataSz);
	}
	if (regType == REG_BYTE)
		((char *)ho->data)[dataSz - 1] = 0;

	reg->regType = regType;
	reg->refType = T_ARRAY;
	reg->e = ho->entity;

	return ho;
}

struct IchigoHeapObject *ichigoGetArrayMut(struct IchigoVm *vm, int arg) {
	struct IchigoState *state = vm->is;
	if (arg >= state->nArgs) {
		logError("Argument %d is out of range\n", arg);
		return NULL;
	}
	struct IchigoInstrArg *a = &state->args[arg];
	if (a->type == PARAM_REG) {
		if (IS_EXTERN(a)) {
			//struct IchigoVar *var = ichGetExternVar(state, GET_EXTERN(GET_REG(a)));
			//ichSetStringExtern(var, vm, data, len);
			//return;
		} else {
			struct IchigoReg *reg = ichGetCurrentCfReg(state, a);
			if (reg->refType == T_ARRAY) {
				return ichHeapGet(reg->e);
			} else if (reg->refType == T_STATIC_ARRAY) {
				return ichigoSetArrayMutReg(reg, reg->s, reg->sLen, reg->regType);
			}
		}
	} else if (a->type == PARAM_REG_IND) {
		struct IchigoReg *ref = ichGetCurrentCfReg(state, a);
		if (ref->refType == T_REG_REF) {
			struct IchigoReg *reg = ichVecAt(&state->curCorout->regs, ref->i);
			if (reg->refType == T_ARRAY) {
				return ichHeapGet(reg->e);
			} else if (reg->refType == T_STATIC_ARRAY) {
				return ichigoSetArrayMutReg(reg, reg->s, reg->sLen, reg->regType);
			}
		} else if (ref->refType == T_EXTERN_REF) {
			//ichSetStringExtern(ichGetExternVar(state, ref->i), vm, data, len);
			//return;
		}
	}
	logError("Invalid instruction arg\n");
	return NULL;
}

struct IchigoHeapObject *ichigoSetArrayMut(struct IchigoVm *vm, int arg, const void *data, uint32_t len, int regType) {
	struct IchigoState *state = vm->is;
	if (arg >= state->nArgs) {
		logError("Argument %d is out of range\n", arg);
		return NULL;
	}
	struct IchigoInstrArg *a = &state->args[arg];
	if (a->type == PARAM_REG) {
		if (IS_EXTERN(a)) {
			//struct IchigoVar *var = ichGetExternVar(state, GET_EXTERN(GET_REG(a)));
			//ichSetStringExtern(var, vm, data, len);
			//return;
		} else {
			struct IchigoReg *reg = ichGetCurrentCfReg(state, a);
			return ichigoSetArrayMutReg(reg, data, len, regType);
		}
	} else if (a->type == PARAM_REG_IND) {
		struct IchigoReg *ref = ichGetCurrentCfReg(state, a);
		if (ref->refType == T_REG_REF) {
			struct IchigoReg *reg = ichVecAt(&state->curCorout->regs, ref->i);
			return ichigoSetArrayMutReg(reg, data, len, regType);
		} else if (ref->refType == T_EXTERN_REF) {
			//ichSetStringExtern(ichGetExternVar(state, ref->i), vm, data, len);
			//return;
		}
	}
	logError("Invalid instruction arg\n");
	return NULL;
}

/* STRING */

const char *ichigoGetString(uint16_t *strLen, struct IchigoVm *vm, int arg) {
	return ichigoGetArray(strLen, vm, arg, REG_BYTE);
}

void ichigoSetString(struct IchigoVm *vm, int arg, const char *str, uint16_t strLen) {
	ichigoSetArray(vm, arg, str, strLen, REG_BYTE);
}