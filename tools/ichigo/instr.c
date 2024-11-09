/*
* Copyright 2022 Luna Nooteboom
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/

#include "ichigo.h"
#include "instr.h"

int newInstrGen(struct InstrGen **state) {
	*state = calloc(1, sizeof(**state));
	if (!state) {
		fprintf(stderr, "Out of memory!\n");
		return ERR_NO_MEM;
	}
	return 0;
}

int pushImport(struct InstrGen *state, struct Import *import) {
	state->nImports += 1;
	printf("import %s, %d\n", import->name, import->nameLen);
	state->imports = realloc(state->imports, state->nImports * sizeof(struct Import));
	if (!state->imports) {
		fprintf(stderr, "Out of memory!\n");
		return ERR_NO_MEM;
	}
	memcpy(&state->imports[state->nImports - 1], import, sizeof(*import));
	return 0;
}
int pushGlobal(struct InstrGen *state, struct Global *global) {
	state->nGlobals += 1;
	state->globals = realloc(state->globals, state->nGlobals * sizeof(struct Import));
	if (!state->globals) {
		fprintf(stderr, "Out of memory!\n");
		return ERR_NO_MEM;
	}
	memcpy(&state->globals[state->nGlobals - 1], global, sizeof(*global));
	return 0;
}

int pushFnStart(struct InstrGen *state, struct Fn *fn) {
	state->nFns += 1;
	state->fns = realloc(state->fns, state->nFns * sizeof(struct Fn));
	if (!state->fns) {
		fprintf(stderr, "Out of memory!\n");
		return ERR_NO_MEM;
	}
	memcpy(&state->fns[state->nFns - 1], fn, sizeof(*fn));
	state->curFn = &state->fns[state->nFns - 1];
	state->wasRet = false;
	return 0;
}

int pushFnEnd(struct InstrGen *state) {
	if (!state->wasRet) {
		/* Push the return instruction if we didnt use a return statement */
		struct Instr instr = { 0 };
		instr.type = INSTR_RET;
		pushInstr(state, &instr);
	}
	state->curFn = NULL;
	return 0;
}

static int instrLen(struct Instr *instr) {
	int ret = 4; /* 4 bytes for the instr itself */
	if (instr->nArgs >= 256) {
		fprintf(stderr, "Instruction has too many arguments\n");
		return ERR_BYTECODE;
	}
	if (instr->nArgs > 6) {
		/* Use long-form instr args, multiply nArgs by 2 for 2 bit param type */
		ret += instr->nArgs / 8 * 2;
		int mod = instr->nArgs % 8;
		if (mod)
			ret += 2;
	}
	for (int i = 0; i < instr->nArgs; i++) {
		struct InstrArg *arg;
		if (i < 4)
			arg = &instr->args[i];
		else
			arg = &instr->args2[i - 4];

		switch (arg->val.type) {
		case VT_INT:
		case VT_FLOAT:
		case VT_LBL_IDX:
			ret += 4;
			break;
		case VT_REG:
		case VT_REG_REF:
			ret += 2;
			break;
		case VT_ENTITY:
			ret += 8;
			break;
		case VT_STRING:
			ret += 2; /* String length */
			ret += arg->val.sLen;
			ret += arg->val.sLen % 2 ? 1 : 2;
			break;
		default:
			fprintf(stderr, "Internal error: Cannot calculate instruction length\n");
			break;
		}
	}
	return ret;
}
int pushInstr(struct InstrGen *state, struct Instr *instr) {
	struct Fn *fn = state->curFn;
	if (!fn) {
		fprintf(stderr, "Internal error: pushing instr to inexistant fn\n");
		return ERR_INTERNAL;
	}
	
	fn->nInstrs += 1;
	fn->instrs = realloc(fn->instrs, fn->nInstrs * sizeof(struct Instr));
	if (!fn->instrs) {
		fprintf(stderr, "Out of memory!\n");
		return ERR_NO_MEM;
	}
	struct Instr *dest = &fn->instrs[fn->nInstrs - 1];
	memcpy(dest, instr, sizeof(*dest));

	dest->byteLen = instrLen(instr);
	if (fn->nInstrs > 1) {
		struct Instr *prev = &fn->instrs[fn->nInstrs - 2];
		dest->byteOffset = prev->byteOffset + prev->byteLen;
	} else {
		dest->byteOffset = 0;
	}

	state->wasRet = instr->type == INSTR_RET;

	return 0;
}

int addInstrArg(struct Instr *instr, struct InstrArg *arg) {
	instr->nArgs += 1;
	struct InstrArg *dest;
	if (instr->nArgs > 4) {
		int n2 = instr->nArgs - 4;
		instr->args2 = realloc(instr->args2, n2 * sizeof(struct InstrArg));
		if (!instr->args2) {
			fprintf(stderr, "Out of memory!\n");
			return ERR_NO_MEM;
		}
		dest = &instr->args2[n2 - 1];
	} else {
		dest = &instr->args[instr->nArgs - 1];
	}
	memcpy(dest, arg, sizeof(*dest));
	return 0;
}
struct InstrArg *getInstrArg(struct Instr *instr, int arg) {
	if (arg < 4) {
		return &instr->args[arg];
	} else {
		return &instr->args2[arg - 4];
	}
}

int pushLabel(struct InstrGen *state, const char *description, int *labelIdx) {
	*labelIdx = -1;
	struct Fn *fn = state->curFn;
	if (!fn) {
		fprintf(stderr, "Internal error: pushing label to inexistant fn\n");
		return ERR_INTERNAL;
	}

	fn->nLabels += 1;
	fn->labels = realloc(fn->labels, fn->nLabels * sizeof(struct Label));
	*labelIdx = fn->nLabels - 1;
	struct Label *dest = &fn->labels[*labelIdx];

	int err = snprintf(dest->name, LABEL_MAXLEN, ".%d_%s", *labelIdx, description);
	if (err < 0) {
		fprintf(stderr, "label snprintf failed");
		return ERR_INTERNAL;
	}
	dest->name[err] = 0;
	dest->nameLen = err;
	dest->offset = -1;

	return 0;
}
int setLabelOffset(struct InstrGen *state, int labelIdx) {
	struct Fn *fn = state->curFn;
	if (labelIdx < 0 || labelIdx >= fn->nLabels) {
		fprintf(stderr, "Internal error: Invalid label idx\n");
		return ERR_INTERNAL;
	}
	struct Label *l = &fn->labels[labelIdx];
	l->offset = fn->nInstrs;

	if (fn->nInstrs) {
		struct Instr *instr = &fn->instrs[fn->nInstrs - 1];
		l->byteOffset = instr->byteOffset + instr->byteLen;
	} else {
		l->byteOffset = 0;
	}

	return 0;
}

int pushInstrJmp(struct InstrGen *state, uint16_t instr, int labelIdx, struct InstrArg *condition) {
	struct Instr ins = { 0 };
	ins.type = instr;
	int err = 0;
	if (instr != INSTR_JMP) {
		/* Conditional */
		err = addInstrArg(&ins, condition);
		if (err)
			return err;
	}
	struct InstrArg toArg = { 0 };
	toArg.val.type = VT_LBL_IDX;
	toArg.val.i = labelIdx;
	err = addInstrArg(&ins, &toArg);
	if (err)
		return err;

	return pushInstr(state, &ins);
}
int pushInstrMovReg(struct InstrGen *state, uint16_t instr, int destReg, int srcReg) {
	struct Instr ins = { 0 };
	struct InstrArg arg = { 0 };
	ins.type = instr;
	arg.val.type = VT_REG;
	arg.val.i = destReg;
	int err = addInstrArg(&ins, &arg);
	if (err)
		return err;

	arg.val.i = srcReg;
	err = addInstrArg(&ins, &arg);
	if (err)
		return err;

	return pushInstr(state, &ins);
}

static const char *instrText[] = {
	[INSTR_NOP - INSTR_BASE] = "NOP",
	[INSTR_MOVI - INSTR_BASE] = "MOVI",
	[INSTR_MOVF - INSTR_BASE] = "MOVF",
	[INSTR_MOVSTR - INSTR_BASE] = "MOVSTR",
	[INSTR_MOVENT - INSTR_BASE] = "MOVENT",

	[INSTR_CALL - INSTR_BASE] = "CALL",
	[INSTR_RET - INSTR_BASE] = "RET",
	[INSTR_JMP - INSTR_BASE] = "JMP",
	[INSTR_JZ - INSTR_BASE] = "JZ",
	[INSTR_JNZ - INSTR_BASE] = "JNZ",
	[INSTR_SWITCH - INSTR_BASE] = "SWITCH",

	[INSTR_CALLA - INSTR_BASE] = "CALLA",
	[INSTR_KILL - INSTR_BASE] = "KILL",
	[INSTR_KILLALL - INSTR_BASE] = "KILLALL",
	[INSTR_WAIT - INSTR_BASE] = "WAIT",

	[INSTR_ADDI - INSTR_BASE] = "ADDI",
	[INSTR_ADDF - INSTR_BASE] = "ADDF",
	[INSTR_SUBI - INSTR_BASE] = "SUBI",
	[INSTR_SUBF - INSTR_BASE] = "SUBF",
	[INSTR_MULI - INSTR_BASE] = "MULI",
	[INSTR_MULF - INSTR_BASE] = "MULF",
	[INSTR_DIVI - INSTR_BASE] = "DIVI",
	[INSTR_DIVF - INSTR_BASE] = "DIVF",

	[INSTR_EQI - INSTR_BASE] = "EQI",
	[INSTR_EQF - INSTR_BASE] = "EQF",
	[INSTR_NEQI - INSTR_BASE] = "NEQI",
	[INSTR_NEQF - INSTR_BASE] = "NEQF",
	[INSTR_LTI - INSTR_BASE] = "LTI",
	[INSTR_LTF - INSTR_BASE] = "LTF",
	[INSTR_LEI - INSTR_BASE] = "LEI",
	[INSTR_LEF - INSTR_BASE] = "LEF",
	[INSTR_GTI - INSTR_BASE] = "GTI",
	[INSTR_GTF - INSTR_BASE] = "GTF",
	[INSTR_GEI - INSTR_BASE] = "GEI",
	[INSTR_GEF - INSTR_BASE] = "GEF",

	[INSTR_MOD - INSTR_BASE] = "MOD",
	[INSTR_AND - INSTR_BASE] = "AND",
	[INSTR_OR - INSTR_BASE] = "OR",
	[INSTR_XOR - INSTR_BASE] = "XOR",
	[INSTR_SHL - INSTR_BASE] = "SHL",
	[INSTR_SHR - INSTR_BASE] = "SHR",
	[INSTR_NOT - INSTR_BASE] = "NOT",
	[INSTR_INV - INSTR_BASE] = "INV",

	[INSTR_SQRT - INSTR_BASE] = "SQRT",
	[INSTR_SIN - INSTR_BASE] = "SIN",
	[INSTR_COS - INSTR_BASE] = "COS",
	[INSTR_ATAN2 - INSTR_BASE] = "ATAN2",
	[INSTR_ABS - INSTR_BASE] = "ABS",
	[INSTR_FLOOR - INSTR_BASE] = "FLOOR",
	[INSTR_CEIL - INSTR_BASE] = "CEIL",
	[INSTR_ROUND - INSTR_BASE] = "ROUND",
	[INSTR_LERP - INSTR_BASE] = "LERP",
	[INSTR_MINF - INSTR_BASE] = "MINF",
	[INSTR_MAXF - INSTR_BASE] = "MAXF",

	[INSTR_MOVIARR - INSTR_BASE] = "MOVIARR",
	[INSTR_MOVFARR - INSTR_BASE] = "MOVFARR",
	[INSTR_MOVEARR - INSTR_BASE] = "MOVEARR",
	[INSTR_LDIARR - INSTR_BASE] = "LDIARR",
	[INSTR_LDFARR - INSTR_BASE] = "LDFARR",
	[INSTR_LDEARR - INSTR_BASE] = "LDEARR",
	[INSTR_STIARR - INSTR_BASE] = "STIARR",
	[INSTR_STFARR - INSTR_BASE] = "STFARR",
	[INSTR_STEARR - INSTR_BASE] = "STEARR",
	[INSTR_REFIARR - INSTR_BASE] = "REFIARR",
	[INSTR_REFFARR - INSTR_BASE] = "REFFARR",
	[INSTR_REFEARR - INSTR_BASE] = "REFEARR",
};
#define TAB_CHAR "\t"
void printInstrs(struct InstrGen *state) {
	for (int i = 0; i < state->nFns; i++) {
		struct Fn *fn = &state->fns[i];
		printf("Function: %s\n", fn->name);
		for (int j = 0; j < fn->nInstrs; j++) {
			for (int k = 0; k < fn->nLabels; k++) {
				struct Label *l = &fn->labels[k];
				if (l->offset == j) {
					printf("%s:\n", l->name);
				}
			}

			struct Instr *ins = &fn->instrs[j];
			if (ins->type >= INSTR_BASE) {
				printf(TAB_CHAR"%s ", instrText[ins->type - INSTR_BASE]);
			} else {
				printf(TAB_CHAR"$%d ", ins->type);
			}
			for (int k = 0; k < ins->nArgs; k++) {
				struct InstrArg *arg = k < 4 ? &ins->args[k] : &ins->args2[k - 4];
				if (arg->val.type == VT_INT) {
					printf("#%d", arg->val.i);
				} else if (arg->val.type == VT_FLOAT) {
					printf("#%f", arg->val.f);
				} else if (arg->val.type == VT_REG) {
					if (arg->val.i >= 0) {
						printf("r%d", arg->val.i);
					} else {
						if (arg->val.i <= -0x6000)
							printf("ex%d", -arg->val.i - 0x6000);
						else
							printf("a%d", -arg->val.i - 1);
					}
				} else if (arg->val.type == VT_REG_REF) {
					if (arg->val.i >= 0) {
						printf("[r%d]", arg->val.i);
					} else {
						if (arg->val.i <= -0x6000)
							printf("[ex%d]", -arg->val.i - 0x6000);
						else
							printf("[a%d]", -arg->val.i - 1);
					}
				} else if (arg->val.type == VT_STRING) {
					printf("\"%s\"", arg->val.s);
				} else if (arg->val.type == VT_LBL_IDX) {
					printf("%s", fn->labels[arg->val.i].name);
				} else {
					printf("UNDEFINED");
				}
				if (k < ins->nArgs - 1) {
					printf(", ");
				}
			}
			printf("\n");
		}
		printf("\n");
	}
}

static int paramType(enum ValType type) {
	switch (type) {
	case VT_REG:
		return PARAM_REG;
	case VT_INT:
	case VT_FLOAT:
	case VT_LBL_IDX:
	case VT_ENTITY:
		return PARAM_4;
	case VT_REG_REF:
		return PARAM_REG_IND;
	case VT_STRING:
		return PARAM_STR;
	default:
		fprintf(stderr, "Internal error : cannot encode valType % d\n", type);
		return ERR_INTERNAL;
	}
}
static int outputInstr(struct Fn *fn, struct Instr *instr) {
	struct IchigoInstr ii;
	int err = 0;
	ii.instr = instr->type;
	bool l = instr->nArgs > 6;
	if (!l) {
		ii.params = instr->nArgs;
		for (int i = 0; i < instr->nArgs; i++) {
			struct InstrArg *arg = getInstrArg(instr, i);
			err = paramType(arg->val.type);
			if (err < 0)
				return err;
			ii.params |= err << (i * 2 + 4);
		}
	} else {
		ii.params = (instr->nArgs << 4) | 0xF;
	}

	err = writeBytes(&ii, 4);
	if (err)
		return err;
	int written = 4;

	if (l) {
		uint16_t regMask = 0;
		for (int i = 0; i < instr->nArgs; i++) {
			int mod = i % 8;
			if (i && !mod) {
				err = writeBytes(&regMask, 2);
				if (err)
					return err;
				written += 2;
				regMask = 0;
			}

			struct InstrArg *arg = getInstrArg(instr, i);
			err = paramType(arg->val.type);
			if (err < 0)
				return err;
			regMask |= err << (mod * 2);
		}
		err = writeBytes(&regMask, 2);
		if (err)
			return err;
		written += 2;
	}

	for (int i = 0; i < instr->nArgs; i++) {
		struct InstrArg *arg = getInstrArg(instr, i);
		int offset;
		int16_t reg;
		int zero = 0;
		switch (arg->val.type) {
		case VT_REG:
		case VT_REG_REF:
			reg = arg->val.i;
			err = writeBytes(&reg, 2);
			written += 2;
			break;
		case VT_INT:
			err = writeBytes(&arg->val.i, 4);
			written += 4;
			break;
		case VT_FLOAT:
			err = writeBytes(&arg->val.f, 4);
			written += 4;
			break;
		case VT_LBL_IDX:
			offset = fn->labels[arg->val.i].byteOffset - instr->byteOffset;
			err = writeBytes(&offset, 4);
			written += 4;
			break;
		case VT_STRING:
			err = writeBytes(&arg->val.sLen, 2);
			if (err)
				return err;
			written += 2;
			err = writeBytes(arg->val.s, arg->val.sLen);
			if (err)
				return err;
			written += arg->val.sLen;
			if (arg->val.sLen % 2) {
				err = writeBytes(&zero, 1); /* Add single terminating zero */
				written += 1;
			} else {
				err = writeBytes(&zero, 2); /* Add two terminating zeros (keep 16 bit alignment) */
				written += 2;
			}
			break;
		case VT_ENTITY:
			err = writeBytes(&arg->val.e, 8);
			written += 8;
			break;
		default:
			fprintf(stderr, "Internal error: Unknown instr arg type");
			return ERR_INTERNAL;
		}
		if (err)
			return err;
		
	}
	assert(written == instr->byteLen);
	return 0;
}


static int writePadding(int written, int align) {
	int mod = written % align;
	if (mod) {
		uint64_t zero = 0;
		int wr = align - mod;
		int err = writeBytes(&zero, wr);
		if (err)
			return err;
		return wr;
	}
	return 0;
}
static int outputFn(struct InstrGen *ig, struct Fn *fn) {
	(void)ig;
	struct IchigoChunk chk;
	struct IchigoFn ifn;
	memcpy(chk.sig, "FN\0", 4);

	ifn.paramCount = fn->nParams;
	ifn.instrCount = fn->nInstrs;
	ifn.nameLen = fn->nameLen;
	ifn.instrLen = fn->instrs[fn->nInstrs - 1].byteOffset + fn->instrs[fn->nInstrs - 1].byteLen;

	chk.len = sizeof(chk) + sizeof(ifn) + ifn.paramCount + ifn.nameLen;
	int mod1 = chk.len % 2;
	if (mod1) {
		chk.len++;
	} else {
		chk.len += 2;
	}
	chk.len += ifn.instrLen;
	int mod2 = chk.len % 4;
	if (mod2) {
		chk.len += 4 - mod2;
	}

	int err = writeBytes(&chk, sizeof(chk));
	if (err)
		return err;
	err = writeBytes(&ifn, sizeof(ifn));
	if (err)
		return err;
	err = writeBytes(fn->params, fn->nParams);
	if (err)
		return err;
	err = writeBytes(fn->name, fn->nameLen);
	if (err)
		return err;

	uint64_t zero = 0;
	if (mod1) {
		err = writeBytes(&zero, 1);
	} else {
		err = writeBytes(&zero, 2);
	}
	if (err)
		return err;

	for (int i = 0; i < fn->nInstrs; i++)
	{
		err = outputInstr(fn, &fn->instrs[i]);
		if (err)
			return err;
	}

	if (mod2) {
		err = writeBytes(&zero, 4 - mod2);
		if (err)
			return err;
	}

	return 0;
}

static int outputImport(struct InstrGen *ig, struct Import *imp) {
	(void) ig;
	struct IchigoChunk chk;
	struct IchigoImport ii;
	memcpy(chk.sig, "IMPT", 4);
	ii.nameLen = imp->nameLen;

	chk.len = sizeof(chk) + sizeof(ii) + ii.nameLen;
	int mod = chk.len % 4;
	if (mod) {
		chk.len += 4 - mod;
	}

	int err = writeBytes(&chk, sizeof(chk));
	if (err)
		return err;
	err = writeBytes(&ii, sizeof(ii));
	if (err)
		return err;
	err = writeBytes(imp->name, ii.nameLen);
	if (err)
		return err;
	err = writePadding(ii.nameLen, 4);
	if (err < 0)
		return err;

	return 0;
}

static int outputGlobal(struct InstrGen *ig, struct Global *glob) {
	(void) ig;
	struct IchigoChunk chk;
	struct IchigoGlobal g;
	memcpy(chk.sig, "GLBL", 4);
	g.nameLen = glob->nameLen;

	chk.len = sizeof(chk) + sizeof(g) + g.nameLen;
	int mod = chk.len % 4;
	if (mod) {
		chk.len += 4 - mod;
	}

	int err = writeBytes(&chk, sizeof(chk));
	if (err)
		return err;
	err = writeBytes(&g, sizeof(g));
	if (err)
		return err;
	err = writeBytes(glob->name, g.nameLen);
	if (err)
		return err;
	err = writePadding(g.nameLen, 4);
	if (err)
		return err;

	return 0;
}

int outputFile(struct InstrGen *ig) {
	int err = 0;

	struct IchigoFile icf;
	memcpy(&icf.signature, "Ichigo\0", 8);
	icf.version = 0x100;

	err = writeBytes(&icf, sizeof(icf));
	if (err)
		return err;

	for (int i = 0; i < ig->nImports; i++) {
		err = outputImport(ig, &ig->imports[i]);
		if (err)
			return err;
	}
	for (int i = 0; i < ig->nGlobals; i++) {
		err = outputGlobal(ig, &ig->globals[i]);
		if (err)
			return err;
	}
	for (int i = 0; i < ig->nFns; i++) {
		err = outputFn(ig, &ig->fns[i]);
		if (err)
			return err;
	}

	return 0;
}
