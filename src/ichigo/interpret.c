#include "ich.h"

static void ichDecodeInstr(struct IchigoState *is, struct IchigoCorout *co) {
	const uint16_t *startPc = co->pc;
	is->curInstr = *(co->pc++);
	uint16_t params = *(co->pc++);
	bool lf = (params & 0xF) == 0xF;
	int nArgs;
	const uint16_t *lfParams = NULL;
	if (lf) {
		nArgs = params >> 4;
		lfParams = co->pc;
		co->pc += nArgs / 8;
		if (nArgs % 8)
			co->pc++;
	} else {
		nArgs = params & 0xF;
		params >>= 4;
	}

	is->nArgs = 0;
	if (nArgs >= ICHIGO_MAX_INSTR_ARGS) {
		logError("Max instruction arg count exceeded\n");
		return;
	}

	
	for (int i = 0; i < nArgs; i++) {
		struct IchigoInstrArg *arg = &is->args[is->nArgs];
		uint16_t sLen;
		arg->type = lf ? (lfParams[i / 8] >> i % 8 * 2) & 3 : (params >> i * 2) & 3;
		arg->ptr = co->pc;
		switch (arg->type) {
		case PARAM_REG:
		case PARAM_REG_IND:
			co->pc += 1;
			break;
		case PARAM_4:
			co->pc += 2;
			break;
		case PARAM_STR:
			sLen = *co->pc;
			if (sLen % 2 == 0)
				sLen += 4;
			else
				sLen += 3;
			co->pc = (uint16_t *)(((char *)co->pc) + sLen);
			break;
		}
		is->nArgs += 1;
	}
	is->curInstrLen = (co->pc - startPc) * 2;
}

static int ichInstrCall(struct IchigoVm *vm, struct IchigoCorout *co, bool async) {
	struct IchigoState *is = vm->is;
	uint16_t nameLen;
	const char *fnName = ichigoGetString(&nameLen, vm, 0);
	struct IchigoFn *fn = ichFindFn(is, fnName);
	if (!fn) {
		logError("Cannot find function %s\n", fnName);
		co->active = false;
		return 0;
	}
	int paramIdx = async ? 2 : 1;
	if (is->nArgs != fn->paramCount + paramIdx) {
		logError("Function call to %s: incorrect number of args\n", fnName);
		co->active = false;
		return 0;
	}

	for (int i = 0; i < fn->paramCount; i++) {
		struct IchigoInstrArg *arg = &is->args[i + paramIdx];
		if (arg->type == PARAM_REG && !IS_EXTERN(arg)) {
			/* Allocate a register to use for output parameter */
			struct IchigoCallFrame *srcCf = ichVecAt(&co->callFrames, co->callFrames.nElements - 1);
			ichGetReg(is, srcCf, GET_REG(arg)); /* Allocate reg if necessary */
		}
	}

	struct IchigoCorout *destCo = NULL;
	int destCoId = 0;
	if (async) {
		for (int i = 1; i < ICHIGO_VM_MAX_COROUT; i++) {
			if (!vm->coroutines[i].active) {
				destCo = &vm->coroutines[i];
				destCoId = i;
				break;
			}
		}
		if (!destCo) {
			logError("Max number of coroutines reached");
			return 0;
		}
		destCo->active = true;
		destCo->waitTime = 0;
		destCo->pc = NULL;
		ichVecCreate(&destCo->callFrames, sizeof(struct IchigoCallFrame));
		ichVecCreate(&destCo->regs, sizeof(struct IchigoReg));
	} else {
		destCo = co;
	}

	const char *params = (const char *)(fn + 1);
	struct IchigoCallFrame *srcCf = ichVecAt(&co->callFrames, co->callFrames.nElements - 1);
	for (int i = 0; i < fn->paramCount; i++) {
		struct IchigoReg *dest = ichVecAppend(&destCo->regs);
		dest->sLen = 1;
		dest->refType = T_REG;
		struct IchigoInstrArg *arg = &is->args[i + paramIdx];
		switch (params[i]) {
		case 'I':
		case 'F':
		case 'S':
		case 's':
		case 'E':
			ichCreateRegRef(is, srcCf, dest, arg, 0);
			break;
		case 'i':
			dest->regType = REG_INT;
			dest->i = ichigoGetInt(vm, i + paramIdx);
			break;
		case 'f':
			dest->regType = REG_FLOAT;
			dest->f = ichigoGetFloat(vm, i + paramIdx);
			break;
		case 'e':
			dest->regType = REG_ENTITY;
			dest->e = ichigoGetEntity(vm, i + paramIdx);
			break;
		default:
			logError("Undefined parameter type in function %s\n", fnName);
			break;

		}
	}
	struct IchigoCallFrame *destCf = ichVecAppend(&destCo->callFrames);
	if (async)
		destCf->regBase = fn->paramCount;
	else
		destCf->regBase = srcCf->regBase + srcCf->nRegs + fn->paramCount;
	destCf->nArgs = fn->paramCount;
	destCf->nRegs = 0;
	destCf->retAddr = destCo->pc;
	int instrOffset = fn->paramCount + fn->nameLen;
	if (instrOffset % 2)
		instrOffset += 1;
	else
		instrOffset += 2;
	destCo->pc = (const uint16_t *)((const char *)(fn + 1) + instrOffset);

	return destCoId;
}
static void ichInstrRet(struct IchigoVm *vm, struct IchigoCorout *co) {
	(void)vm;
	int idx = co->callFrames.nElements - 1;
	if (idx == 0) {
		/* End this coroutine */
		ichigoVmKill(vm, vm->is->curCoroutId);
		return;
	}
	struct IchigoCallFrame *cf = ichVecAt(&co->callFrames, idx);
	for (unsigned int i = cf->regBase - cf->nArgs; i < co->regs.nElements; i++) {
		ichDeleteReg(ichVecAt(&co->regs, i));
	}
	co->regs.nElements -= cf->nArgs + cf->nRegs;
	co->pc = cf->retAddr;
	ichVecDelete(&co->callFrames, idx);
}

static void ichStdInstr(bool *killed, struct IchigoVm *vm, struct IchigoCorout *co) {
	struct IchigoState *is = vm->is;

	switch (is->curInstr) {
	case INSTR_NOP:
		break;
	case INSTR_MOVI: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1)); break;
	case INSTR_MOVF: ichigoSetFloat(vm, 0, ichigoGetFloat(vm, 1)); break;
	case INSTR_MOVENT: ichigoSetEntity(vm, 0, ichigoGetEntity(vm, 1)); break;
	case INSTR_MOVSTR:
	{
		uint16_t sLen;
		const char *s = ichigoGetString(&sLen, vm, 1);
		ichigoSetString(vm, 0, s, sLen);
		break;
	}

	case INSTR_CALL: ichInstrCall(vm, co, false); break;
	case INSTR_RET: ichInstrRet(vm, co); break;
	case INSTR_JMP: co->pc += (ichigoGetInt(vm, 0) - is->curInstrLen) / 2; break;
	case INSTR_JZ:
		if (!ichigoGetInt(vm, 0))
			co->pc += (ichigoGetInt(vm, 1) - is->curInstrLen) / 2;
		break;
	case INSTR_JNZ:
		if (ichigoGetInt(vm, 0))
			co->pc += (ichigoGetInt(vm, 1) - is->curInstrLen) / 2;
		break;
	case INSTR_SWITCH:
	{
		int idx = ichigoGetInt(vm, 0);
		int jmp;
		if (idx < 0 || idx + 2 >= is->nArgs) {
			jmp = ichigoGetInt(vm, 1);
		} else {
			jmp = ichigoGetInt(vm, idx + 2);
		}
		co->pc += (jmp - is->curInstrLen) / 2;
		break;
	}

	case INSTR_CALLA: ichigoSetInt(vm, 1, ichInstrCall(vm, co, true)); break;
	case INSTR_KILL:
	{
		int idx = ichigoGetInt(vm, 0);
		ichigoVmKill(vm, idx);
		if (&vm->coroutines[idx] == co)
			*killed = true;
		break;
	}
	case INSTR_KILLALL:
		for (int i = 0; i < ICHIGO_VM_MAX_COROUT; i++) {
			if (&vm->coroutines[i] != co) {
				ichigoVmKill(vm, i);
			}
		}
		break;

	case INSTR_WAIT: co->waitTime += ichigoGetFloat(vm, 0); break;

	case INSTR_ADDI: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) + ichigoGetInt(vm, 2)); break;
	case INSTR_ADDF: ichigoSetFloat(vm, 0, ichigoGetFloat(vm, 1) + ichigoGetFloat(vm, 2)); break;

	case INSTR_SUBI: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) - ichigoGetInt(vm, 2)); break;
	case INSTR_SUBF: ichigoSetFloat(vm, 0, ichigoGetFloat(vm, 1) - ichigoGetFloat(vm, 2)); break;

	case INSTR_MULI: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) * ichigoGetInt(vm, 2)); break;
	case INSTR_MULF: ichigoSetFloat(vm, 0, ichigoGetFloat(vm, 1) * ichigoGetFloat(vm, 2)); break;

	case INSTR_DIVI: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) / ichigoGetInt(vm, 2)); break;
	case INSTR_DIVF: ichigoSetFloat(vm, 0, ichigoGetFloat(vm, 1) / ichigoGetFloat(vm, 2)); break;

	case INSTR_EQI: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) == ichigoGetInt(vm, 2)); break;
	case INSTR_EQF: ichigoSetFloat(vm, 0, ichigoGetFloat(vm, 1) == ichigoGetFloat(vm, 2)); break;
	case INSTR_NEQI: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) != ichigoGetInt(vm, 2)); break;
	case INSTR_NEQF: ichigoSetFloat(vm, 0, ichigoGetFloat(vm, 1) != ichigoGetFloat(vm, 2)); break;
	case INSTR_LTI: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) < ichigoGetInt(vm, 2)); break;
	case INSTR_LTF: ichigoSetFloat(vm, 0, ichigoGetFloat(vm, 1) < ichigoGetFloat(vm, 2)); break;
	case INSTR_LEI: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) <= ichigoGetInt(vm, 2)); break;
	case INSTR_LEF: ichigoSetFloat(vm, 0, ichigoGetFloat(vm, 1) <= ichigoGetFloat(vm, 2)); break;
	case INSTR_GTI: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) > ichigoGetInt(vm, 2)); break;
	case INSTR_GTF: ichigoSetFloat(vm, 0, ichigoGetFloat(vm, 1) > ichigoGetFloat(vm, 2)); break;
	case INSTR_GEI: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) >= ichigoGetInt(vm, 2)); break;
	case INSTR_GEF: ichigoSetFloat(vm, 0, ichigoGetFloat(vm, 1) >= ichigoGetFloat(vm, 2)); break;

	case INSTR_MOD: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) % ichigoGetInt(vm, 2)); break;
	case INSTR_AND: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) & ichigoGetInt(vm, 2)); break;
	case INSTR_OR: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) | ichigoGetInt(vm, 2)); break;
	case INSTR_XOR: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) ^ ichigoGetInt(vm, 2)); break;
	case INSTR_SHL: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) << ichigoGetInt(vm, 2)); break;
	case INSTR_SHR: ichigoSetInt(vm, 0, ichigoGetInt(vm, 1) >> ichigoGetInt(vm, 2)); break;
	case INSTR_NOT: ichigoSetInt(vm, 0, !ichigoGetInt(vm, 1)); break;
	case INSTR_INV: ichigoSetInt(vm, 0, ~ichigoGetInt(vm, 1)); break;

	case INSTR_SQRT: ichigoSetFloat(vm, 0, sqrtf(ichigoGetFloat(vm, 1))); break;
	case INSTR_SIN: ichigoSetFloat(vm, 0, sinf(ichigoGetFloat(vm, 1))); break;
	case INSTR_COS: ichigoSetFloat(vm, 0, cosf(ichigoGetFloat(vm, 1))); break;
	case INSTR_ATAN2: ichigoSetFloat(vm, 0, atan2f(ichigoGetFloat(vm, 1), ichigoGetFloat(vm, 2))); break;
	case INSTR_ABS: ichigoSetFloat(vm, 0, fabsf(ichigoGetFloat(vm, 1))); break;
	case INSTR_FLOOR: ichigoSetFloat(vm, 0, floorf(ichigoGetFloat(vm, 1))); break;
	case INSTR_CEIL: ichigoSetFloat(vm, 0, ceilf(ichigoGetFloat(vm, 1))); break;
	case INSTR_ROUND: ichigoSetFloat(vm, 0, roundf(ichigoGetFloat(vm, 1))); break;
	case INSTR_LERP:
	{
		float v0 = ichigoGetFloat(vm, 1);
		float v1 = ichigoGetFloat(vm, 2);
		float t = ichigoGetFloat(vm, 3);
		ichigoSetFloat(vm, 0, (1 - t) * v0 + t * v1);
		break;
	}
	case INSTR_MINF: ichigoSetFloat(vm, 0, fminf(ichigoGetFloat(vm, 1), ichigoGetFloat(vm, 2))); break;
	case INSTR_MAXF: ichigoSetFloat(vm, 0, fmaxf(ichigoGetFloat(vm, 1), ichigoGetFloat(vm, 2))); break;

	case INSTR_MOVIARR:
	{
		struct IchigoHeapObject *ho = ichigoSetArrayMut(vm, 0, NULL, is->nArgs - 1, REG_INT);
		for (int i = 0; i < is->nArgs - 1; i++) {
			((int *)ho->data)[i] = ichigoGetInt(vm, i + 1);
		}
		break;
	}
	case INSTR_MOVFARR:
	{
		struct IchigoHeapObject *ho = ichigoSetArrayMut(vm, 0, NULL, is->nArgs - 1, REG_FLOAT);
		for (int i = 0; i < is->nArgs - 1; i++) {
			((float *)ho->data)[i] = ichigoGetFloat(vm, i + 1);
		}
		break;
	}
	case INSTR_MOVEARR:
	{
		struct IchigoHeapObject *ho = ichigoSetArrayMut(vm, 0, NULL, is->nArgs - 1, REG_ENTITY);
		for (int i = 0; i < is->nArgs - 1; i++) {
			((ENTITY *)ho->data)[i] = ichigoGetEntity(vm, i + 1);
		}
		break;
	}
	case INSTR_LDIARR:
	{
		uint16_t len;
		const int *data = ichigoGetArray(&len, vm, 1, REG_INT);
		int idx = ichigoGetInt(vm, 2);
		if (idx >= len) {
			logError("Array read out of bounds\n");
			break;
		}
		ichigoSetInt(vm, 0, data[idx]);
		break;
	}
	case INSTR_LDFARR:
	{
		uint16_t len;
		const float *data = ichigoGetArray(&len, vm, 1, REG_FLOAT);
		int idx = ichigoGetInt(vm, 2);
		if (idx >= len) {
			logError("Array read out of bounds\n");
			break;
		}
		ichigoSetFloat(vm, 0, data[idx]);
		break;
	}
	case INSTR_LDEARR:
	{
		uint16_t len;
		const ENTITY *data = ichigoGetArray(&len, vm, 1, REG_ENTITY);
		int idx = ichigoGetInt(vm, 2);
		if (idx >= len) {
			logError("Array read out of bounds\n");
			break;
		}
		ichigoSetEntity(vm, 0, data[idx]);
		break;
	}
	case INSTR_STIARR:
	{
		struct IchigoHeapObject *ho = ichigoGetArrayMut(vm, 0);
		uint32_t idx = ichigoGetInt(vm, 1);
		if (ho) {
			if (ho->len < idx) {
				ho->len = idx;
				ho->data = globalRealloc(ho->data, idx * 4);
			}
			((int *)ho->data)[idx] = ichigoGetInt(vm, 2);
		}
		break;
	}
	case INSTR_STFARR:
	{
		struct IchigoHeapObject *ho = ichigoGetArrayMut(vm, 0);
		uint32_t idx = ichigoGetInt(vm, 1);
		if (ho) {
			if (ho->len < idx) {
				ho->len = idx;
				ho->data = globalRealloc(ho->data, idx * 4);
			}
			((float *)ho->data)[idx] = ichigoGetFloat(vm, 2);
		}
		break;
	}
	case INSTR_STEARR:
	{
		struct IchigoHeapObject *ho = ichigoGetArrayMut(vm, 0);
		uint32_t idx = ichigoGetInt(vm, 1);
		if (ho) {
			if (ho->len < idx) {
				ho->len = idx;
				ho->data = globalRealloc(ho->data, idx * sizeof(ENTITY));
			}
			((ENTITY *)ho->data)[idx] = ichigoGetEntity(vm, 2);
		}
		break;
	}
	case INSTR_REFIARR:
	case INSTR_REFFARR:
	case INSTR_REFEARR:
	{
		if (is->args[0].type != PARAM_REG || IS_EXTERN(is->args)) {
			break;
		}
		ichigoGetArrayMut(vm, 1); /* Make the array mutable */
		struct IchigoCallFrame *cf = ichVecAt(&co->callFrames, co->callFrames.nElements - 1);
		ichCreateRegRef(is, cf, ichGetReg(is, cf, GET_REG(is->args)), &is->args[1], ichigoGetInt(vm, 2));
		break;
	}
	}
}

static void ichUpdateCoroutine(struct IchigoVm *vm, int corout, float time) {
	struct IchigoState *is = vm->is;
	struct IchigoCorout *co = &vm->coroutines[corout];
	is->curCorout = co;
	is->curCoroutId = corout;
	bool killed = false;
	for (int instrCount = 0; instrCount < 10000; instrCount++) {
		if (killed || !co->active)
			return;
		if (co->waitTime > 0.001f) {
			co->waitTime -= time;
			return;
		}

		ichDecodeInstr(is, co);
		if (is->curInstr >= INSTR_BASE) {
			ichStdInstr(&killed, vm, co);
		} else if (is->curInstr < is->nInstrs && is->instrs[is->curInstr]) {
			is->instrs[is->curInstr](vm);
		} else {
			logError("Undefined instruction: 0x%x\n", is->curInstr);
			co->active = false;
			return;
		}
	}
	/* Infinite loop */
	co->active = false;
	logError("Coroutine has infinite loop");
}
void ichigoVmUpdate(struct IchigoVm *vm, float time) {
	for (int i = 0; i < ICHIGO_VM_MAX_COROUT; i++) {
		if (vm->coroutines[i].active)
			ichUpdateCoroutine(vm, i, time);
	}
}