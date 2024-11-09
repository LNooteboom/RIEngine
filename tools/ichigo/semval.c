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
#include <math.h>

#define DESTREG_NONE (-0x3FFF)

typedef int (*passFunc)(struct ASTNode *node);


static const char *ValTypeText[N_VTS] = {
	[VT_INVALID] = "INVALID",
	[VT_VOID] = "void",
	[VT_INT] = "int",
	[VT_FLOAT] = "float",
	[VT_STRING] = "string",
	[VT_ENTITY] = "entity",
	[VT_REG] = "REG",
	[VT_LBL_IDX] = "LABEL IDX",
};

enum Builtin {
	BUILTIN_KILL,
	BUILTIN_KILLALL,
	BUILTIN_WAIT,
	BUILTIN_RAD,
	BUILTIN_SQRT,
	BUILTIN_SIN,
	BUILTIN_COS,
	BUILTIN_ATAN2,
	BUILTIN_ABS,
	BUILTIN_FLOOR,
	BUILTIN_CEIL,
	BUILTIN_ROUND,
	BUILTIN_LERP,
	BUILTIN_MINF,
	BUILTIN_MAXF,

	N_BUILTINS
};
struct BuiltinInfo {
	const char *name;
	uint16_t instr;
	enum ValType retType;
	int nParams;
};
static struct ASTNode builtins[N_BUILTINS];
static const struct BuiltinInfo builtinInfos[N_BUILTINS] = {
	[BUILTIN_KILL] = {"kill", INSTR_KILL, VT_VOID, 1},
	[BUILTIN_KILLALL] = {"killAll", INSTR_KILLALL, VT_VOID, 0},
	[BUILTIN_WAIT] =  {"wait", INSTR_WAIT, VT_VOID, 1},
	[BUILTIN_RAD] =	  {"rad", INSTR_MULF, VT_FLOAT, 1},
	[BUILTIN_SQRT] =  {"sqrt", INSTR_SQRT, VT_FLOAT, 1},
	[BUILTIN_SIN] =   {"sin", INSTR_SIN, VT_FLOAT, 1},
	[BUILTIN_COS] =   {"cos", INSTR_COS, VT_FLOAT, 1},
	[BUILTIN_ATAN2] = {"atan2", INSTR_ATAN2, VT_FLOAT, 2},
	[BUILTIN_ABS] =   {"abs", INSTR_ABS, VT_FLOAT, 1},
	[BUILTIN_FLOOR] = {"floor", INSTR_FLOOR, VT_FLOAT, 1},
	[BUILTIN_CEIL] =  {"ceil", INSTR_CEIL, VT_FLOAT, 1},
	[BUILTIN_ROUND] = {"round", INSTR_ROUND, VT_FLOAT, 1},
	[BUILTIN_LERP] =  {"lerp", INSTR_LERP, VT_FLOAT, 3},
	[BUILTIN_MINF] = {"minf", INSTR_MINF, VT_FLOAT, 2},
	[BUILTIN_MAXF] = {"maxf", INSTR_MAXF, VT_FLOAT, 2},
};

static void setBuiltins(void) {
	for (int i = 0; i < N_BUILTINS; i++) {
		builtins[i].type = DECL_FN_IMPL;
		builtins[i].flags = FN_BUILTIN;
		builtins[i].val.s = builtinInfos[i].name;
		builtins[i].val.sLen = (int)strlen(builtinInfos[i].name);
		builtins[i].val.type = i == BUILTIN_WAIT ? VT_VOID : VT_FLOAT;
		builtins[i].reg = i;
	}
}



static int doPass(struct ASTNode *root, passFunc topDown, passFunc bottomUp) {
	/* Recurse over the AST, and call passFuncs on each node */
	int err = 0;
	if (topDown) {
		err = topDown(root);
		if (err)
			return err;
	}

	if (root->list && root->type != STMT_SWITCH) {
		for (int i = 0; i < root->listLen; i++) {
			err = doPass(root->list[i], topDown, bottomUp);
			if (err)
				return err;
		}
	}

	if (root->a) {
		err = doPass(root->a, topDown, bottomUp);
		if (err)
			return err;
	}
	if (root->b) {
		err = doPass(root->b, topDown, bottomUp);
		if (err)
			return err;
	}
	if (root->c) {
		err = doPass(root->c, topDown, bottomUp);
		if (err)
			return err;
	}
	if (root->d) {
		err = doPass(root->d, topDown, bottomUp);
		if (err)
			return err;
	}

	if (bottomUp) {
		err = bottomUp(root);
		if (err)
			return err;
	}

	return 0;
}




static bool identMatches(struct ASTNode *a, struct ASTNode *b) {
	return a->val.sLen == b->val.sLen && !memcmp(a->val.s, b->val.s, a->val.sLen);
}
static struct ASTNode *identInVarDeclList(struct ASTNode *varList, struct ASTNode *ident) {
	assert(varList->type == DECL_VAR_LIST);
	for (int i = 0; i < varList->listLen; i++) {
		if (identMatches(varList->list[i], ident)) {
			return varList->list[i];
		}
	}
	return NULL;
}

static struct ASTNode *resolveRefGlobal(struct ASTNode *root, struct ASTNode *ident) {
	for (int i = 0; i < root->listLen; i++) {
		struct ASTNode *node = root->list[i];
		if (node->type == DECL_IMPORT && node->list) {
			struct ASTNode *found = resolveRefGlobal(node, ident);
			if (found)
				return found;
		} else if (node->type == DECL_VAR_LIST) {
			struct ASTNode *found = identInVarDeclList(node, ident);
			if (found)
				return found;
		} else if (node->type == DECL_FN_IMPL && identMatches(node, ident)) {
			return node;
		}
	}
	return NULL;
}
static struct ASTNode *resolveRef(struct ASTNode *ident) {
	struct ASTNode *node = ident;
	struct ASTNode *found = NULL;
	bool isGoto = ident->parent->type == STMT_GOTO;
	for (int i = 0; i < N_BUILTINS; i++) {
		if (identMatches(ident, &builtins[i])) {
			found = &builtins[i];
			break;
		}
	}
	bool inlineParam = true;
	while (!found) {
		int listIdx = node->parentListIdx;

		node = node->parent;
		if (!node) {
			return NULL;
		}

		switch (node->type) { /* Parents where vars/funcs can be declared */
		case AST_ROOT: /* Globals */
			return isGoto? NULL : resolveRefGlobal(node, ident);
		case STMT_BLOCK: /* Locals in block */
			for (int i = 0; i < (isGoto? node->listLen : listIdx); i++) {
				struct ASTNode *sib = node->list[i];
				if (sib->type == DECL_VAR_LIST) {
					found = identInVarDeclList(sib, ident);
					if (found)
						break;
				}
				if ((sib->type == DECL_FN_IMPL || sib->type == LBL_GOTO) && identMatches(ident, sib)) {
					found = sib;
					break;
				}
			}
			break;
		case DECL_FN_IMPL: /* Parameters */
		case EXPR_INLINE_FN:
			if (listIdx >= 0 && inlineParam) {
				inlineParam = false;
			} else {
				for (int i = 0; i < node->listLen; i++) {
					if (identMatches(ident, node->list[i])) { /* No need to check type, as all entries are DECL_FN_PARAM*/
						found = node->list[i];
						break;
					}
				}
			}
			break;
		case STMT_FOR: /* For initializer */
			if (node->a->type == DECL_VAR_LIST) {
				found = identInVarDeclList(node->a, ident);
				if (found)
					break;
			}
			break;
		default:
			break;
		}
		//inlineParam = false;
	}
	return found;
}

static int inlineFnExpand(struct ASTNode *node) {
	if (node->type != EXPR_FN_CALL || node->a->type != EXPR_IDENT)
		return 0;

	struct ASTNode *fn = resolveRef(node->a);
	if (!fn || !(fn->flags & FN_INLINE))
		return 0;

	if (fn->listLen != node->listLen) {
		logErrorAtLine(node->fp, "Incorrect number of parameters for inline fn %s\n", fn->val.s);
		return ERR_INCORRECT_N_PARAMS;
	}

	node->type = EXPR_INLINE_FN;
	freeNode(node->a);
	node->a = NULL;

	fn = copyNode(fn);
	if (!fn)
		return ERR_NO_MEM;
	
	node->val.type = fn->val.type;
	node->a = fn->a;
	node->a->parent = node;
	for (int i = 0; i < fn->listLen; i++) {
		struct ASTNode *arg = node->list[i];
		struct ASTNode *param = fn->list[i];
		assert(param->type == DECL_FN_PARAM);
		param->a = arg;
		arg->parent = param;

		node->list[i] = param;
		param->parent = node;
	}

	/* Shallow free */
	free(fn->list);
	free(fn);

	return 0;
}

/* Resolve references */
static int passTd1(struct ASTNode *node) {
	int err = inlineFnExpand(node);
	if (err)
		return err;


	if (node->type == DECL_VAR_LIST) {
		struct ASTNode *parent = node->parent;
		if (parent->type != AST_ROOT && parent->type != DECL_IMPORT && parent->type != STMT_BLOCK && !(parent->type == STMT_FOR && parent->a == node)) {
			logErrorAtLine(node->fp, "Variable declaration is not allowed here");
			return ERR_INVALID_VAR_DECL;
		}
		if (parent->parent && parent->parent->type == STMT_SWITCH) {
			logErrorAtLine(node->fp, "Variable declaration not allowed inside switch");
			return ERR_INVALID_VAR_DECL;
		}

		for (int i = 0; i < node->listLen; i++) {
			struct ASTNode *n = node->list[i];
			/* See if it's already defined */
			struct ASTNode *ref = resolveRef(n);
			if (ref && ref != n) {
				if (ref->parent == node->parent || ref->parent == node || ref->parent->parent == node->parent) { /* Defined in same block */
					logErrorAtLine(node->fp, "Variable \'%s\' redefined (Previous declaration was at %s:%d)", node->val.s, ref->fp.file, ref->fp.line);
					return ERR_REDEFINED_VAR;
				} else {
					logWarnAtLine(node->fp, "Variable declaration \'%s\' hides previous declaration at %s:%d", node->val.s, ref->fp.file, ref->fp.line);
				}
			}

			if (parent->type == AST_ROOT || parent->type == DECL_IMPORT) {
				if (!(n->flags & VAR_CONST)) {
					if (!(n->flags & VAR_FN_EXTERN)) {
						n->reg = -0x7000 - parent->reg;
						parent->reg += 1;
					}
				}
			}
		}
	} else if (node->type == DECL_FN_IMPL) {
		struct ASTNode *ref = resolveRef(node);
		if (ref && ref != node) {
			logErrorAtLine(ref->fp, "Function \'%s\' redefined (Previous declaration was at %s:%d)", node->val.s, ref->fp.file, ref->fp.line);
			return ERR_REDEFINED_FUNC;
		}
	} else if (node->type == DECL_FN_PARAM) {
		struct ASTNode *ref = resolveRef(node);
		if (ref) {
			if (ref->parent == node->parent) {
				if (ref != node) {
					logErrorAtLine(node->fp, "Duplicate parameter \'%s\'", node->val.s);
					return ERR_DUPLICATE_PARAM;
				}
			} else {
				//logWarnAtLine(node->fp, "Parameter \'%s\' hides global variable at line %s:%d", node->val.s, ref->fp.file, ref->fp.line);
			}
		}
		if (node->parent->type == DECL_FN_IMPL)
			node->reg = (node->parent->val.type == VT_VOID || node->parent->flags & FN_ASYNC? -1 : -2) - node->parentListIdx;
	} else if (node->type == EXPR_FN_CALL) {
		if (node->a->type != EXPR_IDENT) {
			logErrorAtLine(node->a->fp, "Unimplemented: Calling function pointers");
			return ERR_UNIMPLEMENTED;
		}
	} else if (node->type >= EXPR_ASSIGN && node->type <= EXPR_BW_SHREQ) {
		if (node->a->type != EXPR_IDENT && node->a->type != EXPR_ARRAY_IDX) {
			logErrorAtLine(node->a->fp, "Invalid lvalue expression");
			return ERR_INVALID_EXPR;
		}
	} else if (node->type == EXPR_IDENT) {
		struct ASTNode *ref = resolveRef(node);
		if (!ref) {
			logErrorAtLine(node->fp, "Undefined identifier: %s", node->val.s);
			return ERR_UNDEFINED_IDENT;
		}

		/* Change this when implementing fn ptrs */
		if (node->parent->type == EXPR_FN_CALL && node->parent->a == node) {
			if (ref->type != DECL_FN_IMPL) {
				logErrorAtLine(node->fp, "Not a function: \'%s\' (declared at %s:%d)", node->val.s, ref->fp.file, ref->fp.line);
				return ERR_NOT_A_FUNC;
			}
		} else if (node->parent->type == STMT_GOTO) {
			if (ref->type != LBL_GOTO) {
				logErrorAtLine(node->fp, "Not a goto label: \'%s\' (declared at %s:%d)", node->val.s, ref->fp.file, ref->fp.line);
				return ERR_INVALID_LABEL;
			}
		} else { /* Use as variable */
			if (ref->type == DECL_FN_IMPL) {
				logErrorAtLine(node->fp, "Not a variable: \'%s\' (declared at %s:%d)", node->val.s, ref->fp.file, ref->fp.line);
				return ERR_NOT_A_VAR;
			}
			bool written = ref->flags & VAR_WRITTEN;
			if ((node->parent->type >= EXPR_POSTINCR && node->parent->type <= EXPR_PREDECR) ||
				(node->parent->type == DECL_FN_PARAM && node->parent->flags & VAR_REF)) {
				ref->flags |= VAR_WRITTEN | VAR_READ;
			} else if ((node->parent->type >= EXPR_ASSIGN && node->parent->type <= EXPR_BW_SHREQ) && node == node->parent->a) {
				/* On the left side of an assignment */
				ref->flags |= VAR_WRITTEN;
			} else {
				ref->flags |= VAR_READ;
			}
			if (ref->flags & VAR_WRITTEN && ref->flags & VAR_CONST) {
				logErrorAtLine(node->fp, "Writing to const variable %s\n", ref->val.s);
				return ERR_CONST_WRITE;
			}
			if (!written && ref->type != DECL_FN_PARAM && !(ref->flags & VAR_FN_EXTERN) && ref->flags & VAR_READ && !ref->a) {
				logWarnAtLine(node->fp, "Reading from variable %s which might be uninitialized", ref->val.s);
			}
		}

		/* Set the resolved ptr */
		node->identRef = ref;
		node->val.type = ref->val.type;
	} else if (node->type == STMT_RETURN) {
		/* Set vt to parent function's vt */
		struct ASTNode *parent;
		for (parent = node->parent; parent && parent->type != DECL_FN_IMPL; parent = parent->parent) {

		}
		assert(parent && parent->type == DECL_FN_IMPL);
		node->val.type = parent->val.type;
	}

	return 0;
}

static bool allowedConversion(enum ValType from, enum ValType to) {
	return from == to || (from == VT_INT && to == VT_FLOAT);
}
static bool isCompare(enum ASTNodeType type) {
	return type >= EXPR_LESS && type <= EXPR_NEQ;
}

static int passBu1(struct ASTNode *node) {
	if (node->type == EXPR_TERNARY || node->type == STMT_IF) {
		node->val.type = node->b->val.type;
		if (node->val.type == VT_INT && node->c) {
			node->val.type = node->c->val.type;
		}

		if (node->a->val.type == VT_STRING) {
			logWarnAtLine(node->fp, "Comparing a string will always return true");
		}
	} else if (node->type >= EXPR_FN_CALL && node->type <= EXPR_BW_SHREQ) {
		if (node->type != EXPR_INLINE_FN)
			node->val.type = node->a->val.type;

		if ((node->val.type == VT_INT || isCompare(node->a->type)) && node->b) {
			node->val.type = node->b->val.type; /* Convert int to float */
		}

		if (node->type == EXPR_BW_INV || node->type == EXPR_MOD || node->type == EXPR_BW_SHL || node->type == EXPR_BW_SHR
			|| (node->type >= EXPR_BW_AND && node->type <= EXPR_OR)) {
			node->val.type = VT_INT;
		}

		assert(node->val.type != VT_INVALID);
		if (node->type != EXPR_FN_CALL && node->type != EXPR_INLINE_FN && node->val.type == VT_VOID) {
			logErrorAtLine(node->fp, "Void expression");
			return ERR_VOID_EXPR;
		}
		if (node->val.type == VT_STRING || node->val.type == VT_ENTITY) {
			if (node->type != EXPR_FN_CALL && (node->type != EXPR_ASSIGN || node->a->val.type != node->b->val.type)) {
				logErrorAtLine(node->fp, "Unimplemented: expressions based on strings or entities");
				return ERR_UNIMPLEMENTED;
			}
		}

		if (node->type >= EXPR_POSTINCR && node->type <= EXPR_PREDECR && node->type != EXPR_ARRAY_IDX) {
			/* TODO lvalue expressions that aren't just EXPR_IDENT */
			if (node->a->type != EXPR_IDENT || node->a->identRef->flags & VAR_CONST) {
				logErrorAtLine(node->fp, "Expression must be a modifiable lvalue");
				return ERR_NOT_MODIFIABLE;
			}
		}
	} else if (node->type == STMT_SWITCH || node->type == STMT_LOOP) {
		if (node->a && node->a->val.type != VT_INT) {
			logErrorAtLine(node->fp, "Integer type required");
			return ERR_INT_REQUIRED;
		}
	}

	if (node->type >= EXPR_ASSIGN && node->type <= EXPR_BW_SHREQ) {
		if (node->a->val.type == VT_INT && node->b->val.type == VT_FLOAT) {
			logErrorAtLine(node->fp, "Implicit float-to-int conversion not allowed");
			return ERR_NO_CONVERT;
		}
		node->val.type = node->a->val.type;
	} else if (node->type == EXPR_FN_CALL && !(node->a->identRef->flags & FN_BUILTIN)) {
		/* Argument type-checking */
		struct ASTNode *ref = node->a->identRef;
		if (node->listLen != ref->listLen) {
			logErrorAtLine(node->fp, "Call to function '%s': incorrect number of parameters (expected %d, got %d)", node->a->val.s, ref->listLen, node->listLen);
			return ERR_INCORRECT_N_PARAMS;
		}
		for (int i = 0; i < node->listLen; i++) {
			struct ASTNode *arg = node->list[i];
			struct ASTNode *param = ref->list[i];
			if (param->flags & VAR_REF) {
				if (arg->type != EXPR_IDENT) {
					logErrorAtLine(node->fp, "Call to function '%s', argument %d: ref parameter must be an identifier", node->a->val.s, i);
					return ERR_NO_CONVERT;
				}
				arg->identRef->flags |= VAR_WRITTEN;
			}

			if (!allowedConversion(arg->val.type, param->val.type)) {
				logErrorAtLine(node->fp, "Call to function '%s', argument %d: Implicit conversion (%s to %s) not allowed",
					node->a->val.s, i, ValTypeText[arg->val.type], ValTypeText[param->val.type]);
				return ERR_NO_CONVERT;
			}
		}
	} else if (node->type == EXPR_ARRAY) {
		/* TODO check types */
		if (node->parent->type == DECL_VAR) {
			node->val.type = node->parent->val.type;
		} else if (node->parent->type == EXPR_ASSIGN) {
			assert(node == node->parent->b);
			node->val.type = node->parent->a->val.type;
		} else if (node->parent->type == EXPR_ARRAY_IDX) {
			/* infer array type */
			node->val.type = VT_INT;
			for (int i = 0; i < node->listLen; i++) {
				if (node->list[i]->val.type != VT_INT) {
					node->val.type = node->list[i]->val.type;
				}
			}
		}
		for (int i = 0; i < node->listLen; i++) {
			enum ValType vt = node->list[i]->val.type;
			if (vt != node->val.type && !(node->val.type == VT_FLOAT && vt == VT_INT)) {
				logErrorAtLine(node->fp, "Cannot convert %s to %s in array literal\n", ValTypeText[vt], ValTypeText[node->val.type]);
				return ERR_NO_CONVERT;
			}
		}
	}

	return 0;
}


static int constFoldBuiltin(struct ASTNode *node) {
	if (node->type != EXPR_FN_CALL)
		return 0;
	if (!(node->a->identRef->flags & FN_BUILTIN))
		return 0;

	float params[8];
	for (int i = 0; i < node->listLen; i++) {
		struct ASTNode *p = node->list[i];
		while (p && p->type == EXPR_IDENT) {
			if (p->identRef->flags & VAR_WRITTEN)
				return 0;
			p = p->identRef->a;
		}
		if (!p)
			return 0;
		if (p->type == EXPR_FLOAT)
			params[i] = p->val.f;
		else if (p->type == EXPR_INT)
			params[i] = p->val.i;
		else
			return 0;
	}

	float retVal;
	switch (node->a->identRef->reg) {
	case BUILTIN_RAD:
		retVal = params[0] * (3.14159265359f / 180.0f);
		break;
	case BUILTIN_SQRT:
		retVal = sqrtf(params[0]);
		break;
	case BUILTIN_SIN:
		retVal = sinf(params[0]);
		break;
	case BUILTIN_COS:
		retVal = cosf(params[0]);
		break;
	case BUILTIN_ATAN2:
		retVal = atan2f(params[0], params[1]);
		break;
	default:
		return 0;
	}

	for (int i = 0; i < node->listLen; i++) {
		freeNode(node->list[i]);
	}
	free(node->list);
	node->list = NULL;
	free(node->a);
	node->a = NULL;

	node->type = EXPR_FLOAT;
	node->val.type = VT_FLOAT;
	node->val.f = retVal;

	return 0;
}


static int constFold(struct ASTNode *node) {
	if (!(node->type >= EXPR_PLUS && node->type <= EXPR_BW_OR))
		return constFoldBuiltin(node);
	
	assert(node->a);
	struct ASTNode *a = node->a;
	while (a && a->type == EXPR_IDENT) {
		if (a->identRef->flags & VAR_WRITTEN)
			return 0;
		a = a->identRef->a;
	}
	if (!a || (a->type != EXPR_INT && a->type != EXPR_FLOAT))
		return 0;

	struct ASTNode *b = node->b;
	if (b) {
		while (b && b->type == EXPR_IDENT) {
			if (b->identRef->flags & VAR_WRITTEN)
				return 0;
			b = b->identRef->a;
		}
		if (!b || (b->type != EXPR_INT && b->type != EXPR_FLOAT))
			return 0;
	}
	bool isF;
	if (isCompare(node->type)) {
		isF = a->val.type == VT_FLOAT || b->val.type == VT_FLOAT;
	} else {
		isF = node->val.type == VT_FLOAT;
	}
	float lf = 0, rf = 0;
	int li = 0, ri = 0;
	if (isF) {
		if (a->type == EXPR_INT)
			lf = a->val.i;
		else
			lf = a->val.f;
		if (b) {
			if (b->type == EXPR_INT)
				rf = b->val.i;
			else
				rf = b->val.f;
		}
	} else {
		if (a->type == EXPR_INT)
			li = a->val.i;
		else
			li = a->val.f;
		if (b) {
			if (b->type == EXPR_INT)
				ri = b->val.i;
			else
				ri = b->val.f;
		}
	}

	
	switch (node->type) {
	case EXPR_PLUS:
		node->val.i = a->val.i;
		break;
	case EXPR_MIN:
		if (isF)
			node->val.f = -lf;
		else
			node->val.i = -li;
		break;
	case EXPR_NOT:
		if (a->val.type == VT_FLOAT)
			node->val.i = a->val.f == 0;
		else
			node->val.i = !a->val.i;
		break;
	case EXPR_BW_INV:
		if (a->val.type == VT_FLOAT)
			node->val.i = ~((int)a->val.f);
		else
			node->val.i = ~(a->val.i);
		break;
	case EXPR_MULT:
		if (isF)
			node->val.f = lf * rf;
		else
			node->val.i = li * ri;
		break;
	case EXPR_DIV:
		if (isF)
			node->val.f = lf / rf;
		else
			node->val.i = li / ri;
		break;
	case EXPR_MOD:
		node->val.i = li % ri;
		break;
	case EXPR_ADD:
		if (isF)
			node->val.f = lf + rf;
		else
			node->val.i = li + ri;
		break;
	case EXPR_SUB:
		if (isF)
			node->val.f = lf - rf;
		else
			node->val.i = li - ri;
		break;
	case EXPR_BW_SHL:
		node->val.i = li << ri;
		break;
	case EXPR_BW_SHR:
		node->val.i = li >> ri;
		break;
	case EXPR_LESS:
		if (isF)
			node->val.i = lf < rf;
		else
			node->val.i = li < ri;
		break;
	case EXPR_LESSEQ:
		if (isF)
			node->val.i = lf <= rf;
		else
			node->val.i = li <= ri;
		break;
	case EXPR_GTR:
		if (isF)
			node->val.i = lf > rf;
		else
			node->val.i = li > ri;
		break;
	case EXPR_GTREQ:
		if (isF)
			node->val.i = lf >= rf;
		else
			node->val.i = li >= ri;
		break;
	case EXPR_EQ:
		if (isF)
			node->val.i = lf == rf;
		else
			node->val.i = li == ri;
		break;
	case EXPR_NEQ:
		if (isF)
			node->val.i = lf != rf;
		else
			node->val.i = li == ri;
		break;
	case EXPR_BW_AND:
		node->val.i = li & ri;
		break;
	case EXPR_BW_XOR:
		node->val.i = li ^ ri;
		break;
	case EXPR_BW_OR:
		node->val.i = li | ri;
		break;
	default:
		assert(false);
		break;
	}

	freeNode(node->a);
	node->a = NULL;
	if (node->b) {
		freeNode(node->b);
		node->b = NULL;
	}
	node->type = node->val.type == VT_FLOAT ? EXPR_FLOAT : EXPR_INT;
	//fprintf(ERR_OUT, "Folded constant at line %d\n", node->fp.line);

	return 0;
}

static struct ASTNode *findParentSwitch(struct ASTNode *node) {
	node = node->parent;
	while (node->type != STMT_SWITCH && node->type != AST_ROOT) {
		node = node->parent;
	}
	return node->type == STMT_SWITCH ? node : NULL;
}
static int passTd2(struct ASTNode *node) {
	switch (node->type) {
	case DECL_VAR:
	case DECL_FN_IMPL:
		if (node->flags & VAR_FN_EXTERN) {
			struct ASTNode *ext = node->type == DECL_VAR ? node->parent->d : node->d;
			assert(ext);
			if (ext->type != EXPR_INT) {
				logErrorAtLine(node->fp, "Extern declaration must have constant integer value");
				return ERR_INVALID_VAR_DECL;
			}
			node->reg = node->type == DECL_VAR ? -0x6000 - ext->val.i : ext->val.i;
		}
		break;
	case LBL_CASE:
		node->identRef = findParentSwitch(node);
		if (!node->identRef) {
			logErrorAtLine(node->fp, "Case label outside switch");
			return ERR_INVALID_LABEL;
		}
		assert(node->a);
		if (node->a->type == EXPR_IDENT) {
			if (node->a->identRef->type != DECL_VAR || !(node->a->identRef->flags & VAR_CONST) || node->a->identRef->a->type != EXPR_INT) {
				logErrorAtLine(node->fp, "Case label must be a constant integer");
				return ERR_INVALID_LABEL;
			}
			node->a->type = EXPR_INT;
			memcpy(&node->a->val, &node->a->identRef->a->val, sizeof(node->a->val));
		} else if (node->a->type != EXPR_INT) {
			logErrorAtLine(node->fp, "Case label must be a constant integer");
			return ERR_INVALID_LABEL;
		}

		for (int i = 0; i < node->identRef->listLen; i++) {
			struct ASTNode *otherCase = node->identRef->list[i];
			if (otherCase->type == LBL_CASE && otherCase->a->val.i == node->a->val.i) {
				logErrorAtLine(node->fp, "Duplicate case label (Previous was at %s:%d)", otherCase->fp.file, otherCase->fp.line);
				return ERR_INVALID_LABEL;
			}
		}
		addToNodeList(node->identRef, node);
		break;
	case LBL_DEFAULT:
		node->identRef = findParentSwitch(node);
		if (!node->identRef) {
			logErrorAtLine(node->fp, "Case label outside switch");
			return ERR_INVALID_LABEL;
		}
		for (int i = 0; i < node->identRef->listLen; i++) {
			struct ASTNode *otherCase = node->identRef->list[i];
			if (otherCase->type == LBL_DEFAULT) {
				logErrorAtLine(node->fp, "Duplicate default label (Previous was at %s:%d)", otherCase->fp.file, otherCase->fp.line);
				return ERR_INVALID_LABEL;
			}
		}
		addToNodeList(node->identRef, node);
		break;
	case EXPR_NOT:
		if ((node->parent->type == STMT_IF || node->parent->type == STMT_WHILE) && node == node->parent->a) {
			node->parent->flags ^= COND_INVERT;
			node->type = EXPR_PLUS;
		}
		break;
	}
	return 0;
}


/*
* INSTRUCTION GENERATION
*/

static int genExprAsReg(struct InstrGen *ig, struct ASTNode *node, int destReg, int tmpReg);
static int genExprAsArg(struct InstrGen *ig, struct ASTNode *node, struct InstrArg *arg, int destReg, int tmpReg);
static int genStmt(struct InstrGen *ig, struct ASTNode *node, int destReg, int tmpReg, int breakLbl, int continueLbl);

static uint16_t vtToInstr(uint16_t instr, enum ValType vt) {
	assert(vt == VT_FLOAT || vt == VT_INT);
	return vt == VT_FLOAT ? instr + 1 : instr;
}
static uint16_t vtToMovInstr(enum ValType vt) {
	switch (vt) {
	case VT_INT:
		return INSTR_MOVI;
	case VT_FLOAT:
		return INSTR_MOVF;
	case VT_STRING:
		return INSTR_MOVSTR;
	case VT_ENTITY:
		return INSTR_MOVENT;
	default:
		printf("%d\n", vt);
		fflush(stdout);
		assert(false);
		return INSTR_MOVI;
	}
}

static int genExprBinOp(struct InstrGen *ig, struct ASTNode *node, uint16_t type, bool hasFloat, int destReg, int tmpReg) {
	struct Instr instr = { 0 };
	struct InstrArg arg = { 0 };

	enum ValType vt = hasFloat ? node->val.type : VT_INT;
	instr.type = vtToInstr(type, vt);

	/* DEST */
	arg.val.type = VT_REG;
	arg.val.i = destReg;
	int err = addInstrArg(&instr, &arg);;
	if (err)
		return err;

	/* LEFT */
	arg.val.type = vt;
	err = genExprAsArg(ig, node->a, &arg, destReg, tmpReg);
	if (err)
		return err;
	bool usesDestReg = arg.val.type == VT_REG && arg.val.i >= destReg;
	err = addInstrArg(&instr, &arg);;
	if (err)
		return err;

	/* RIGHT */
	arg.val.type = vt;
	if (usesDestReg)
		err = genExprAsArg(ig, node->b, &arg, tmpReg, tmpReg + 1); /* <-- Allocating a new tmpReg here */
	else
		err = genExprAsArg(ig, node->b, &arg, destReg, tmpReg);
	if (err)
		return err;
	err = addInstrArg(&instr, &arg);;
	if (err)
		return err;

	return pushInstr(ig, &instr);
}
static bool needsDestWrite(struct ASTNode *node) {
	if (node->parent->type >= EXPR_FN_CALL || node->parent->type == DECL_VAR
		|| node->parent->type == DECL_FN_PARAM || node->parent->type == STMT_RETURN)
		return true;
	if (node->parent->type == STMT_IF || node->parent->type == STMT_WHILE || node->parent->type == STMT_SWITCH || node->parent->type == STMT_LOOP)
		return node == node->parent->a;
	if (node->parent->type == STMT_FOR)
		return node == node->parent->b;
	return false;
}
static int genExprUnOp(struct InstrGen *ig, struct ASTNode *node, uint16_t type, int destReg, int tmpReg) {
	struct Instr instr = { 0 };
	struct InstrArg arg = { 0 };
	int err = 0;
	if (node->type == EXPR_MIN) {
		instr.type = vtToInstr(type, node->val.type);
		/* DEST */
		arg.val.type = VT_REG;
		arg.val.i = destReg;
		err = addInstrArg(&instr, &arg);;
		if (err)
			return err;
		/* LEFT (= 0) */
		arg.val.type = node->val.type;
		arg.val.i = 0; /* Float also becomes 0.0f */
		err = addInstrArg(&instr, &arg);;
		if (err)
			return err;
		/* RIGHT */
		arg.val.type = node->val.type;
		err = genExprAsArg(ig, node->a, &arg, destReg, tmpReg);
		if (err)
			return err;
		err = addInstrArg(&instr, &arg);
		if (err)
			return err;

		return pushInstr(ig, &instr);
	} else if (node->type == EXPR_BW_INV || node->type == EXPR_NOT) {
		instr.type = node->type == EXPR_BW_INV ? INSTR_INV : INSTR_NOT;
		arg.val.type = VT_INT;
		err = genExprAsArg(ig, node->a, &arg, destReg, tmpReg);
		if (err)
			return err;
		err = addInstrArg(&instr, &arg);
		if (err)
			return err;
		return pushInstr(ig, &instr);
	} else if (node->type == EXPR_PLUS) {
		return genExprAsReg(ig, node->a, destReg, tmpReg);
	}

	bool pre = node->type == EXPR_PREINCR || node->type == EXPR_PREDECR;
	int srcReg = node->a->identRef->reg;
	if (!pre && needsDestWrite(node)) {
		err = pushInstrMovReg(ig, vtToMovInstr(node->val.type), destReg, srcReg);
		if (err)
			return err;
	}

	/* inc/dec itself (ex. ADDI [a] [a] 1 */
	instr.type = vtToInstr(type, node->val.type);
	/* DEST (srcreg) */
	arg.val.type = VT_REG;
	arg.val.i = srcReg;
	err = addInstrArg(&instr, &arg);;
	if (err)
		return err;
	/* LEFT (= duplicate) */
	err = addInstrArg(&instr, &arg);;
	if (err)
		return err;
	/* RIGHT */
	if (node->val.type == VT_FLOAT) {
		arg.val.type = VT_FLOAT;
		arg.val.f = 1; /* inc/dec by 1 */
	} else {
		arg.val.type = VT_INT;
		arg.val.i = 1;
	}
	err = addInstrArg(&instr, &arg);;
	if (err)
		return err;
	err = pushInstr(ig, &instr);
	if (err)
		return err;

	/*if (pre && node->parent->type >= EXPR_FN_CALL) {
		err = pushInstrMovReg(ig, vtToMovInstr(node->val.type), destReg, srcReg);
		if (err)
			return err;
	}*/
	return err;
}
static int genExprFnCallBuiltin(struct InstrGen *ig, struct ASTNode *node, int destReg, int tmpReg) {
	struct ASTNode *decl = node->a->identRef;
	struct Instr instr = { 0 };
	struct InstrArg arg = { 0 };
	int err;

	const struct BuiltinInfo *info = &builtinInfos[decl->reg];
	if (node->listLen != info->nParams) {
		logErrorAtLine(node->fp, "Builtin '%s': Incorrect number of arguments\n", info->name);
		return ERR_INCORRECT_N_PARAMS;
	}

	instr.type = info->instr;
	if (info->retType != VT_VOID) {
		arg.val.type = VT_REG;
		arg.val.i = destReg;
		err = addInstrArg(&instr, &arg);
		if (err)
			return err;
	}

	int nRegs = -1;
	for (int i = 0; i < info->nParams; i++) {
		arg.val.type = decl->reg == BUILTIN_KILL? VT_INT : VT_FLOAT;
		if (nRegs == -1)
			err = genExprAsArg(ig, node->list[i], &arg, destReg, tmpReg);
		else
			err = genExprAsArg(ig, node->list[i], &arg, tmpReg + nRegs, tmpReg + nRegs + 1);
		if (err)
			return err;
		err = addInstrArg(&instr, &arg);
		if (err)
			return err;

		if (arg.val.type == VT_REG)
			nRegs++;
	}

	if (decl->reg == BUILTIN_RAD) {
		arg.val.type = VT_FLOAT;
		arg.val.f = (3.14159265359f / 180.0f);
		err = addInstrArg(&instr, &arg);
		if (err)
			return err;
	}

	return pushInstr(ig, &instr);
}

static int genExprFnCall(struct InstrGen *ig, struct ASTNode *node, int destReg, int tmpReg) {
	struct Instr instr = { 0 };
	struct InstrArg arg = { 0 };
	struct ASTNode *decl = node->a->identRef;
	int err = 0;
	if (decl->flags & VAR_FN_EXTERN) {
		instr.type = decl->reg;
	} else if (decl->flags & FN_BUILTIN) {
		return genExprFnCallBuiltin(ig, node, destReg, tmpReg);
	} else { /* DECL_FN_IMPL */
		instr.type = decl->flags & FN_ASYNC? INSTR_CALLA : INSTR_CALL;
		arg.val.type = VT_STRING;
		arg.val.s = decl->val.s;
		arg.val.sLen = decl->val.sLen;
		err = addInstrArg(&instr, &arg);
		if (err)
			return err;
	}
	if (decl->val.type != VT_VOID) { /* Return value is just a hidden param ref */
		arg.val.type = VT_REG;
		arg.val.i = destReg;
		err = addInstrArg(&instr, &arg);
		if (err)
			return err;
	}
	assert(node->listLen == decl->listLen); /* Has already been checked, just making sure here */
	int nRegs = -1;
	for (int i = 0; i < node->listLen; i++) {
		arg.val.type = decl->list[i]->val.type;
		if (nRegs == -1)
			err = genExprAsArg(ig, node->list[i], &arg, destReg, tmpReg);
		else
			err = genExprAsArg(ig, node->list[i], &arg, tmpReg + nRegs, tmpReg + nRegs + 1);

		if (err)
			return err;
		err = addInstrArg(&instr, &arg);
		if (err)
			return err;

		if (arg.val.type == VT_REG)
			nRegs++;
	}
	return pushInstr(ig, &instr);
}
static int genExprInlineFn(struct InstrGen *ig, struct ASTNode *node, int destReg, int tmpReg) {
	struct InstrArg arg = { 0 };
	int err = 0;

	/* Gen the args */
	int nRegs = -1;
	for (int i = 0; i < node->listLen; i++) {
		arg.val.type = node->list[i]->val.type;
		int d, tmp;
		if (nRegs == -1) {
			d = destReg;
			tmp = tmpReg;
		} else {
			d = tmpReg + nRegs;
			tmp = tmpReg + nRegs + 1;
		}

		if (node->list[i]->flags & VAR_REF) {
			assert(node->list[i]->a->type == EXPR_IDENT);
			arg.val.type = VT_REG;
			arg.val.i = node->list[i]->a->identRef->reg;
			arg.val.sLen = 0;
			node->list[i]->reg = node->list[i]->a->identRef->reg;
		} else if (node->list[i]->flags & VAR_WRITTEN) {
			arg.val.type = VT_REG;
			arg.val.i = d;
			arg.val.sLen = 0;
			node->list[i]->reg = d;
			err = genExprAsReg(ig, node->list[i]->a, d, tmp);
			nRegs++;
		} else {
			err = genExprAsArg(ig, node->list[i]->a, &arg, d, tmp);
			if (arg.val.type == VT_REG)
				nRegs++;
		}
		if (err)
			return err;

		memcpy(&node->list[i]->val, &arg.val, sizeof(arg.val)); /* Put it in val */
	}
	err = genStmt(ig, node->a, destReg, tmpReg + nRegs + 1, -1, -1); /* Generating statements inside expressions. Yaaaay! */
	if (err)
		return err;

	return 0;
}

static int genExprArrayIdx(struct InstrGen *ig, struct ASTNode *node, int destReg, int tmpReg, struct ASTNode *assign, bool ref) {
	struct Instr instr = { 0 };
	struct InstrArg arg = { 0 };
	if (assign) {
		switch (node->val.type) {
		case VT_INT:
			instr.type = INSTR_STIARR;
			break;
		case VT_FLOAT:
			instr.type = INSTR_STFARR;
			break;
		case VT_ENTITY:
			instr.type = INSTR_STEARR;
			break;
		default:
			assert(false);
			break;
		}
	} else if (ref) {
		switch (node->val.type) {
		case VT_INT:
			instr.type = INSTR_REFIARR;
			break;
		case VT_FLOAT:
			instr.type = INSTR_REFFARR;
			break;
		case VT_ENTITY:
			instr.type = INSTR_REFEARR;
			break;
		default:
			assert(false);
			break;
		}
	} else {
		switch (node->val.type) {
		case VT_INT:
			instr.type = INSTR_LDIARR;
			break;
		case VT_FLOAT:
			instr.type = INSTR_LDFARR;
			break;
		case VT_ENTITY:
			instr.type = INSTR_LDEARR;
			break;
		default:
			assert(false);
			break;
		}
	}

	int err;
	if (!assign) {
		/* Dest */
		arg.val.type = VT_REG;
		arg.val.i = destReg;
		err = addInstrArg(&instr, &arg);
		if (err)
			return err;
	}

	/* Array */
	arg.val.type = VT_INVALID;
	err = genExprAsArg(ig, node->a, &arg, destReg, tmpReg);
	if (err)
		return err;
	err = addInstrArg(&instr, &arg);
	if (err)
		return err;

	/* Index */
	arg.val.type = VT_INT;
	err = genExprAsArg(ig, node->b, &arg, tmpReg, tmpReg + 1);
	if (err)
		return err;
	int nRegs = arg.val.type == VT_REG ? 1 : 0;
	err = addInstrArg(&instr, &arg);
	if (err)
		return err;

	if (assign) {
		/* Dest */
		arg.val.type = node->val.type;
		err = genExprAsArg(ig, assign, &arg, tmpReg + nRegs, tmpReg + nRegs + 1);
		if (err)
			return err;
		err = addInstrArg(&instr, &arg);
		if (err)
			return err;
	}

	return pushInstr(ig, &instr);
}

static int genExprLogOp(struct InstrGen *ig, struct ASTNode *node, int destReg, int tmpReg) {
	bool isOr = node->type == EXPR_OR;

	int skipLabel;
	int err = pushLabel(ig, isOr ? "or_skip" : "and_skip", &skipLabel);
	if (err)
		return err;

	err = genExprAsReg(ig, node->a, destReg, tmpReg);
	if (err)
		return err;

	struct InstrArg arg = { 0 };
	arg.val.type = VT_REG;
	arg.val.i = destReg;

	err = pushInstrJmp(ig, isOr ? INSTR_JNZ : INSTR_JZ, skipLabel, &arg);
	if (err)
		return err;

	err = genExprAsReg(ig, node->b, destReg, tmpReg);
	if (err)
		return err;

	err = setLabelOffset(ig, skipLabel);
	if (err)
		return err;

	return err;
}

static int genExprTernary(struct InstrGen *ig, struct ASTNode *node, int destReg, int tmpReg) {
	//struct Instr instr = { 0 };
	struct InstrArg arg = { 0 };

	/* Condition */
	arg.val.type = VT_INT;
	int err = genExprAsArg(ig, node->a, &arg, destReg, tmpReg);
	if (err)
		return err;


	int ternElseLbl;
	err = pushLabel(ig, "tern_else", &ternElseLbl);
	if (err)
		return err;
	err = pushInstrJmp(ig, INSTR_JZ, ternElseLbl, &arg); /* Jump to the else clause if the condition evals to zero */
	if (err)
		return err;

	/* cond is true */
	err = genExprAsReg(ig, node->b, destReg, tmpReg);
	if (err)
		return err;


	int ternEndLbl;
	err = pushLabel(ig, "tern_end", &ternEndLbl);
	if (err)
		return err;
	err = pushInstrJmp(ig, INSTR_JMP, ternEndLbl, NULL);
	if (err)
		return err;


	/* cond is false */
	err = setLabelOffset(ig, ternElseLbl);
	if (err)
		return err;

	err = genExprAsReg(ig, node->c, destReg, tmpReg);
	if (err)
		return err;

	return setLabelOffset(ig, ternEndLbl);
}

static int genExprAssign(struct InstrGen *ig, struct ASTNode *node, int destReg, int tmpReg) {
	int err = 0;
	if (node->a->type == EXPR_ARRAY_IDX) {
		err = genExprArrayIdx(ig, node->a, destReg, tmpReg, node->b, false);
		if (err)
			return err;
	} else {
		int reg = node->a->identRef->reg;
		err = genExprAsReg(ig, node->b, reg, tmpReg);
		if (err)
			return err;

		if (node->val.type != node->a->val.type) {
			/* Generate converting mov */
			err = pushInstrMovReg(ig, vtToMovInstr(node->val.type), reg, reg);
			if (err)
				return err;
		}

		if (needsDestWrite(node)) {
			err = pushInstrMovReg(ig, vtToMovInstr(node->val.type), destReg, reg);
		}
	}
	return err;
}

static int genExprShAssign(struct InstrGen *ig, struct ASTNode *node, uint16_t type, bool hasFloat, int destReg, int tmpReg) {
	struct Instr instr = { 0 };
	struct InstrArg arg = { 0 };
	int reg, err;
	if (node->a->type == EXPR_ARRAY_IDX) {
		err = genExprArrayIdx(ig, node->a, destReg, tmpReg, NULL, true);
		if (err)
			return err;

		arg.val.type = VT_REG_REF;
		reg = destReg;
		destReg = tmpReg;
		tmpReg += 1;
	} else {
		arg.val.type = node->a->identRef->flags & VAR_REF? VT_REG_REF : VT_REG;
		reg = node->a->identRef->reg;
	}
	arg.val.i = reg;
	
	enum ValType vt = hasFloat ? node->val.type : VT_INT;
	instr.type = vtToInstr(type, vt);

	err = addInstrArg(&instr, &arg);
	if (err)
		return err;
	err = addInstrArg(&instr, &arg); /* Duplicated */
	if (err)
		return err;

	arg.val.type = vt;
	err = genExprAsArg(ig, node->b, &arg, destReg, tmpReg); /* Can use destReg as a scratch register */
	if (err)
		return err;
	err = addInstrArg(&instr, &arg);
	if (err)
		return err;

	err = pushInstr(ig, &instr);
	if (err)
		return err;

	if (node->val.type != node->a->val.type) {
		/* Generate converting mov */
		err = pushInstrMovReg(ig, vtToMovInstr(node->val.type), reg, reg);
		if (err)
			return err;
	}

	if (needsDestWrite(node)) {
		err = pushInstrMovReg(ig, vtToMovInstr(node->val.type), destReg, reg);
	}
	return err;
}

static int genExprLit(struct InstrGen *ig, struct ASTNode *node, int destReg) {
	struct Instr instr = { 0 };
	struct InstrArg arg = { 0 };
	//enum ValType type = node->val.type;
	enum ValType type;
	if (needsDestWrite(node))
		type = node->parent->val.type;
	else
		type = node->val.type;

	instr.type = vtToMovInstr(type);
	arg.val.type = VT_REG;
	arg.val.i = destReg;
	int err = addInstrArg(&instr, &arg);
	if (err)
		return err;

	arg.val.type = type;
	err = genExprAsArg(ig, node, &arg, -1, -1);
	if (err)
		return err;
	err = addInstrArg(&instr, &arg);
	if (err)
		return err;

	return pushInstr(ig, &instr);
}

static int genExprArray(struct InstrGen *ig, struct ASTNode *node, int destReg, int tmpReg) {
	struct Instr instr = { 0 };
	struct InstrArg arg = { 0 };
	switch (node->val.type) {
	case VT_INT:
		instr.type = INSTR_MOVIARR;
		break;
	case VT_FLOAT:
		instr.type = INSTR_MOVFARR;
		break;
	case VT_ENTITY:
		instr.type = INSTR_MOVEARR;
		break;
	default:
		assert(false);
		return ERR_INTERNAL;
	}
	
	/* Dest reg */
	arg.val.type = VT_REG;
	arg.val.i = destReg;
	int err = addInstrArg(&instr, &arg);
	if (err)
		return err;

	for (int i = 0; i < node->listLen; i++) {
		arg.val.type = node->val.type;
		err = genExprAsArg(ig, node->list[i], &arg, tmpReg, tmpReg + 1);
		if (err) {
			return err;
		}
		if (arg.val.type == VT_REG)
			tmpReg += 1;

		err = addInstrArg(&instr, &arg);
		if (err)
			return err;
	}

	return pushInstr(ig, &instr);
}

static int genExprAsReg(struct InstrGen *ig, struct ASTNode *node, int destReg, int tmpReg) {
	int err = 0;
	switch (node->type) {
	case EXPR_IDENT:
	case EXPR_INT:
	case EXPR_FLOAT:
	case EXPR_STRING:
		err = genExprLit(ig, node, destReg);
		break;
	case EXPR_ARRAY:
		err = genExprArray(ig, node, destReg, tmpReg);
		break;
	case EXPR_FN_CALL:
		err = genExprFnCall(ig, node, destReg, tmpReg);
		break;
	case EXPR_ARRAY_IDX:
		err = genExprArrayIdx(ig, node, destReg, tmpReg, NULL, false);
		break;
	case EXPR_INLINE_FN:
		err = genExprInlineFn(ig, node, destReg, tmpReg);
		break;
	case EXPR_POSTINCR:
		err = genExprUnOp(ig, node, INSTR_ADDI, destReg, tmpReg);
		break;
	case EXPR_POSTDECR:
		err = genExprUnOp(ig, node, INSTR_SUBI, destReg, tmpReg);
		break;
	case EXPR_PREINCR:
		err = genExprUnOp(ig, node, INSTR_ADDI, destReg, tmpReg);
		break;
	case EXPR_PREDECR:
		err = genExprUnOp(ig, node, INSTR_SUBI, destReg, tmpReg);
		break;
	case EXPR_PLUS:
		err = genExprAsReg(ig, node->a, destReg, tmpReg);
		break;
	case EXPR_MIN:
		err = genExprUnOp(ig, node, INSTR_SUBI, destReg, tmpReg);
		break;
	case EXPR_NOT:
		err = genExprUnOp(ig, node, INSTR_NOT, destReg, tmpReg);
		break;
	case EXPR_BW_INV:
		err = genExprUnOp(ig, node, INSTR_INV, destReg, tmpReg);
		break;
	case EXPR_MULT:
		err = genExprBinOp(ig, node, INSTR_MULI, true, destReg, tmpReg);
		break;
	case EXPR_DIV:
		err = genExprBinOp(ig, node, INSTR_DIVI, true, destReg, tmpReg);
		break;
	case EXPR_MOD:
		err = genExprBinOp(ig, node, INSTR_MOD, false, destReg, tmpReg);
		break;
	case EXPR_ADD:
		err = genExprBinOp(ig, node, INSTR_ADDI, true, destReg, tmpReg);
		break;
	case EXPR_SUB:
		err = genExprBinOp(ig, node, INSTR_SUBI, true, destReg, tmpReg);
		break;
	case EXPR_BW_SHL:
		err = genExprBinOp(ig, node, INSTR_SHL, false, destReg, tmpReg);
		break;
	case EXPR_BW_SHR:
		err = genExprBinOp(ig, node, INSTR_SHR, false, destReg, tmpReg);
		break;
	case EXPR_LESS:
		err = genExprBinOp(ig, node, INSTR_LTI, true, destReg, tmpReg);
		break;
	case EXPR_LESSEQ:
		err = genExprBinOp(ig, node, INSTR_LEI, true, destReg, tmpReg);
		break;
	case EXPR_GTR:
		err = genExprBinOp(ig, node, INSTR_GTI, true, destReg, tmpReg);
		break;
	case EXPR_GTREQ:
		err = genExprBinOp(ig, node, INSTR_GEI, true, destReg, tmpReg);
		break;
	case EXPR_EQ:
		err = genExprBinOp(ig, node, INSTR_EQI, true, destReg, tmpReg);
		break;
	case EXPR_NEQ:
		err = genExprBinOp(ig, node, INSTR_NEQI, true, destReg, tmpReg);
		break;
	case EXPR_BW_AND:
		err = genExprBinOp(ig, node, INSTR_AND, false, destReg, tmpReg);
		break;
	case EXPR_BW_XOR:
		err = genExprBinOp(ig, node, INSTR_XOR, false, destReg, tmpReg);
		break;
	case EXPR_BW_OR:
		err = genExprBinOp(ig, node, INSTR_OR, false, destReg, tmpReg);
		break;
	case EXPR_AND:
		err = genExprLogOp(ig, node, destReg, tmpReg);
		break;
	case EXPR_OR:
		err = genExprLogOp(ig, node, destReg, tmpReg);
		break;
	case EXPR_TERNARY:
		err = genExprTernary(ig, node, destReg, tmpReg);
		break;
	case EXPR_ASSIGN:
		err = genExprAssign(ig, node, destReg, tmpReg);
		break;
	case EXPR_ADDEQ:
		err = genExprShAssign(ig, node, INSTR_ADDI, true, destReg, tmpReg);
		break;
	case EXPR_SUBEQ:
		err = genExprShAssign(ig, node, INSTR_SUBI, true, destReg, tmpReg);
		break;
	case EXPR_MULTEQ:
		err = genExprShAssign(ig, node, INSTR_MULI, true, destReg, tmpReg);
		break;
	case EXPR_DIVEQ:
		err = genExprShAssign(ig, node, INSTR_DIVI, true, destReg, tmpReg);
		break;
	case EXPR_MODEQ:
		err = genExprShAssign(ig, node, INSTR_MOD, false, destReg, tmpReg);
		break;
	case EXPR_BW_OREQ:
		err = genExprShAssign(ig, node, INSTR_OR, false, destReg, tmpReg);
		break;
	case EXPR_BW_ANDEQ:
		err = genExprShAssign(ig, node, INSTR_AND, false, destReg, tmpReg);
		break;
	case EXPR_BW_XOREQ:
		err = genExprShAssign(ig, node, INSTR_XOR, false, destReg, tmpReg);
		break;
	case EXPR_BW_SHLEQ:
		err = genExprShAssign(ig, node, INSTR_SHL, false, destReg, tmpReg);
		break;
	case EXPR_BW_SHREQ:
		err = genExprShAssign(ig, node, INSTR_SHR, false, destReg, tmpReg);
		break;
	default:
		assert(false);
		break;
	}
	return err;
}
static int genExprAsArg(struct InstrGen *ig, struct ASTNode *node, struct InstrArg *arg, int destReg, int tmpReg) {
	bool isConst = false;
	struct Val constVal = {0};
	int glblIndex = 0;
	if (node->type >= EXPR_INT && node->type <= EXPR_STRING) {
		/* Constant */
		memcpy(&constVal, &node->val, sizeof(arg->val));
		isConst = true;
	} else if (node->type == EXPR_IDENT) {
		if (node->identRef->parent->type == AST_ROOT || node->identRef->parent->type == DECL_IMPORT) {
			/* Global var */
			if (node->identRef->flags & VAR_CONST) {
				assert(node->identRef->a->type >= EXPR_INT && node->identRef->a->type <= EXPR_STRING);
				memcpy(&constVal, &node->identRef->a->val, sizeof(arg->val));
				isConst = true;
			} else {
				glblIndex = node->identRef->reg;
			}
		} else if (node->identRef->parent->type == EXPR_INLINE_FN) {
			/* Just copy the arg lol */
			memcpy(&arg->val, &node->identRef->val, sizeof(arg->val));
			return 0;
		} else {
			if (node->identRef->flags & (VAR_WRITTEN | VAR_FN_EXTERN) || node->identRef->type == DECL_FN_PARAM || node->identRef->a->type < EXPR_INT || node->identRef->a->type >= EXPR_STRING) {
				/* Local var */
				arg->val.type = node->identRef->flags & VAR_REF ? VT_REG_REF : VT_REG;
				arg->val.i = node->identRef->reg;
				return 0;
			} else {
				assert(node->identRef->a->type >= EXPR_INT && node->identRef->a->type < EXPR_STRING);
				memcpy(&constVal, &node->identRef->a->val, sizeof(arg->val));
				isConst = true;
			}
			
		}
	} else if (node->type >= EXPR_PREINCR && node->type <= EXPR_PREDECR) {
		arg->val.type = VT_REG;
		assert(node->a->type == EXPR_IDENT);
		arg->val.i = node->a->reg;
		return genExprAsReg(ig, node, destReg, tmpReg);
	} else if (node->type == EXPR_PLUS) {
		return genExprAsArg(ig, node->a, arg, destReg, tmpReg);
	}
	
	if (isConst) {
		/* Do literal conversion if necessary */
		/* Uses already set val type */
		switch (arg->val.type) {
		case VT_INT:
			if (constVal.type == VT_FLOAT) {
				arg->val.i = constVal.f;
			} else if (constVal.type == VT_INT) {
				arg->val.i = constVal.i;
			} else {
				logErrorAtLine(node->fp, "Cannot convert from %s to %s\n", ValTypeText[constVal.type], ValTypeText[arg->val.type]);
				return ERR_NO_CONVERT;
			}
			break;
		case VT_FLOAT:
			if (constVal.type == VT_FLOAT) {
				arg->val.f = constVal.f;
			} else if (constVal.type == VT_INT) {
				arg->val.f = constVal.i;
			} else {
				logErrorAtLine(node->fp, "Cannot convert from %s to %s\n", ValTypeText[constVal.type], ValTypeText[arg->val.type]);
				return ERR_NO_CONVERT;
			}
			break;
		default:
			if (arg->val.type != constVal.type) {
				logErrorAtLine(node->fp, "Cannot convert from %s to %s\n", ValTypeText[constVal.type], ValTypeText[arg->val.type]);
				return ERR_NO_CONVERT;
			}
			arg->val.s = constVal.s;
			arg->val.sLen = constVal.sLen;
			break;
		}
		return 0;
	} else if (glblIndex) {
		arg->val.type = VT_REG;
		arg->val.i = glblIndex;
		return 0;
	}  else {
		/* Tmp Reg */
		arg->val.type = VT_REG;
		arg->val.i = destReg;
		return genExprAsReg(ig, node, destReg, tmpReg);
	}
}

static int genVarDecl(struct InstrGen *ig, struct ASTNode *varList, int locals) { /* destReg stored in ast */
	int count = 0;
	for (int i = 0; i < varList->listLen; i++) {
		struct ASTNode *node = varList->list[i];
		if (!(node->flags & VAR_WRITTEN) && (node->a->type == EXPR_INT || node->a->type == EXPR_FLOAT))
			continue;

		node->reg = locals + count;
		if (node->a) {
			int err = genExprAsReg(ig, node->a, node->reg, node->reg + 1);
			if (err)
				return err;

			if (node->val.type != node->a->val.type) {
				/* Generate converting mov */
				//err = pushInstrMovReg(ig, vtToMovInstr(node->val.type), node->reg, node->reg);
			}
		}
		count++;
	}
	
	return count;
}

static int genSwitch(struct InstrGen *ig, struct ASTNode *node, int destReg, int tmpReg, int breakLbl, int continueLbl) {
	if (!node->listLen) {
		logWarnAtLine(node->fp, "Switch without cases\n");
		return 0;
	}

	int minValue = INT32_MAX, maxValue = INT32_MIN;
	int nCases = 0;
	int endLbl;
	int err = pushLabel(ig, "switch_end", &endLbl);
	int defaultLbl = endLbl;
	for (int i = 0; i < node->listLen; i++) {
		struct ASTNode *caseNode = node->list[i];
		int idx;
		err = pushLabel(ig, caseNode->type == LBL_DEFAULT ? "switch_default" : "switch_case", &idx);
		if (err)
			return err;
		
		caseNode->reg = idx;

		if (caseNode->type == LBL_DEFAULT) {
			defaultLbl = idx;
			continue;
		}

		assert(caseNode->type == LBL_CASE);
		nCases++;
		assert(caseNode->a && caseNode->a->type == EXPR_INT);
		int v = caseNode->a->val.i;
		if (v < minValue)
			minValue = v;
		if (v > maxValue)
			maxValue = v;
	}
	int range = maxValue - minValue;

	struct Instr instr = { 0 };
	struct InstrArg arg = { 0 };
	if (nCases >= range / 2) {
		/* Generate as SWITCH instruction */
		if (minValue) {
			/* SUBI rx, rx, #minVal*/
			instr.type = INSTR_SUBI;
			arg.val.type = VT_REG;
			arg.val.i = tmpReg;
			err = addInstrArg(&instr, &arg);
			if (err)
				return err;
			err = genExprAsArg(ig, node->a, &arg, tmpReg, tmpReg + 1);
			if (err)
				return err;
			err = addInstrArg(&instr, &arg);
			if (err)
				return err;
			arg.val.type = VT_INT;
			arg.val.i = minValue;
			err = addInstrArg(&instr, &arg);
			if (err)
				return err;
			err = pushInstr(ig, &instr);
			if (err)
				return err;

			/* Set arg to condition */
			arg.val.type = VT_REG;
			arg.val.i = tmpReg;
			memset(&instr, 0, sizeof(instr));
		} else {
			/* Set arg to condition */
			arg.val.type = VT_INT;
			err = genExprAsArg(ig, node->a, &arg, tmpReg, tmpReg + 1);
			if (err)
				return err;
		}
		instr.type = INSTR_SWITCH;
		err = addInstrArg(&instr, &arg);
		if (err)
			return err;
		/* Default */
		arg.val.type = VT_LBL_IDX;
		arg.val.i = defaultLbl;
		err = addInstrArg(&instr, &arg);
		if (err)
			return err;

		/* Cases */
		for (int i = minValue; i <= maxValue; i++) {
			struct ASTNode *match = NULL;
			for (int j = 0; j < node->listLen; j++) {
				struct ASTNode *caseNode = node->list[j];
				if (caseNode->type == LBL_CASE && caseNode->a->val.i == i) {
					match = caseNode;
					break;
				}
			}
			arg.val.i = match ? match->reg : defaultLbl;
			err = addInstrArg(&instr, &arg);
			if (err)
				return err;
		}

		err = pushInstr(ig, &instr);
		if (err)
			return err;
	} else {
		err = genExprAsReg(ig, node->a, tmpReg, tmpReg + 1);
		if (err)
			return err;

		/* Generate as if chain */
		for (int i = 0; i < node->listLen; i++) {
			struct ASTNode *caseNode = node->list[i];
			if (caseNode->type != LBL_CASE)
				continue;

			memset(&instr, 0, sizeof(instr));
			instr.type = INSTR_EQI;
			arg.val.type = VT_REG;
			arg.val.i = tmpReg + 1;
			err = addInstrArg(&instr, &arg);
			if (err)
				return err;
			arg.val.i = tmpReg;
			err = addInstrArg(&instr, &arg);
			if (err)
				return err;
			arg.val.type = VT_INT;
			arg.val.i = caseNode->a->val.i;
			err = addInstrArg(&instr, &arg);
			if (err)
				return err;
			err = pushInstr(ig, &instr);
			if (err)
				return err;
			
			arg.val.type = VT_REG;
			arg.val.i = tmpReg + 1;
			err = pushInstrJmp(ig, INSTR_JNZ, caseNode->reg, &arg);
			if (err)
				return err;
		}
		err = pushInstrJmp(ig, INSTR_JMP, defaultLbl, NULL);
		if (err)
			return err;
	}

	err = genStmt(ig, node->b, destReg, tmpReg, endLbl, -1);
	if (err)
		return err;

	err = setLabelOffset(ig, endLbl);
	if (err)
		return err;

	return 0;
}

static int genStmt(struct InstrGen *ig, struct ASTNode *node, int destReg, int tmpReg, int breakLbl, int continueLbl) {
	int err = 0;
	int locals = tmpReg;
	int loopStartLbl, loopIterLbl, loopCondLbl, loopEndLbl;
	int elseLbl = 0, endIfLbl;
	struct InstrArg arg = { 0 };
	switch (node->type) {
	case STMT_BLOCK:
		for (int i = 0; i < node->listLen; i++) {
			if (node->list[i]->type == DECL_VAR_LIST) {
				err = genVarDecl(ig, node->list[i], locals); /* Reuse the var reg for the expression */
				if (err < 0)
					return err;
				locals += err;
				err = 0;
			} else {
				err = genStmt(ig, node->list[i], destReg, locals, breakLbl, continueLbl);
				if (err)
					return err;
			}
		}
		break;
	case STMT_IF:
		/* Condition */
		arg.val.type = VT_INT;
		err = genExprAsArg(ig, node->a, &arg, locals, locals + 1);
		if (err)
			return err;
		
		err = pushLabel(ig, "if_end", &endIfLbl);
		if (err)
			return err;

		if (node->c) { /* Has else statement */
			err = pushLabel(ig, "if_else", &elseLbl);
			if (err)
				return err;
			err = pushInstrJmp(ig, node->flags & COND_INVERT? INSTR_JNZ : INSTR_JZ, elseLbl, &arg); /* Jump to the else clause if the condition evals to zero */
		} else {
			err = pushInstrJmp(ig, node->flags & COND_INVERT ? INSTR_JNZ : INSTR_JZ, endIfLbl, &arg);
		}
		if (err)
			return err;

		err = genStmt(ig, node->b, destReg, locals, breakLbl, continueLbl);
		if (err)
			return err;

		if (node->c) {
			err = pushInstrJmp(ig, INSTR_JMP, endIfLbl, NULL);
			if (err)
				return err;

			/* Else clause */
			err = setLabelOffset(ig, elseLbl);
			if (err)
				return err;

			err = genStmt(ig, node->c, destReg, locals, breakLbl, continueLbl);
			if (err)
				return err;
		}

		setLabelOffset(ig, endIfLbl);

		break;
	case STMT_WHILE:
		err = pushLabel(ig, "while_start", &loopStartLbl);
		if (err)
			return err;
		err = pushLabel(ig, "while_cond", &loopCondLbl);
		if (err)
			return err;
		err = pushLabel(ig, "while_end", &loopEndLbl);
		if (err)
			return err;

		bool infinite = false;
		if (node->a->type == EXPR_INT) {
			infinite = (node->a->val.i && !(node->flags & COND_INVERT)) || (node->a->val.i == 0 && node->flags & COND_INVERT);
		}

		if (!infinite) {
			err = pushInstrJmp(ig, INSTR_JMP, loopCondLbl, NULL);
			if (err)
				return err;
		}

		/* Start */
		err = setLabelOffset(ig, loopStartLbl);
		if (err)
			return err;

		/* Body */
		err = genStmt(ig, node->b, destReg, locals, loopEndLbl, loopCondLbl);
		if (err)
			return err;

		/* Condition */
		err = setLabelOffset(ig, loopCondLbl);
		if (err)
			return err;

		if (infinite) {
			err = pushInstrJmp(ig, INSTR_JMP, loopStartLbl, NULL); /* Jump to the start */
			if (err)
				return err;
		} else {
			arg.val.type = VT_INT;
			err = genExprAsArg(ig, node->a, &arg, locals, locals + 1);
			if (err)
				return err;

			err = pushInstrJmp(ig, node->flags & COND_INVERT ? INSTR_JZ : INSTR_JNZ, loopStartLbl, &arg); /* Jump to the start if nonzero (and COND_INVERT is false) */
			if (err)
				return err;
		}

		/* End */
		err = setLabelOffset(ig, loopEndLbl);
		if (err)
			return err;

		break;
	case STMT_FOR:
		err = pushLabel(ig, "for_start", &loopStartLbl);
		if (err)
			return err;
		err = pushLabel(ig, "for_iter", &loopIterLbl);
		if (err)
			return err;
		err = pushLabel(ig, "for_cond", &loopCondLbl);
		if (err)
			return err;
		err = pushLabel(ig, "for_end", &loopEndLbl);
		if (err)
			return err;

		/* Initializer */
		if (node->a->type == DECL_VAR_LIST) {
			err = genVarDecl(ig, node->a, locals);
			if (err < 0)
				return err;
			locals += err;
			err = 0;
		} else {
			err = genExprAsReg(ig, node->a, locals, locals + 1);
			if (err)
				return err;
		}

		if (true) { /* TODO */
			err = pushInstrJmp(ig, INSTR_JMP, loopCondLbl, NULL);
			if (err)
				return err;
		}

		/* Start */
		err = setLabelOffset(ig, loopStartLbl);
		if (err)
			return err;

		/* Do the loop body (in node->d), setting break and continue labels */
		err = genStmt(ig, node->d, destReg, locals, loopEndLbl, loopIterLbl);
		if (err)
			return err;

		/* Loop iteration */
		err = setLabelOffset(ig, loopIterLbl);
		if (err)
			return err;
		err = genExprAsReg(ig, node->c, locals, locals + 1);
		if (err)
			return err;

		/* Check loop condition */
		err = setLabelOffset(ig, loopCondLbl);
		if (err)
			return err;
		arg.val.type = VT_INT;
		err = genExprAsArg(ig, node->b, &arg, locals, locals + 1);
		if (err)
			return err;
		err = pushInstrJmp(ig, INSTR_JNZ, loopStartLbl, &arg); /* Jump to the start if nonzero */
		if (err)
			return err;

		/* End */
		err = setLabelOffset(ig, loopEndLbl);
		if (err)
			return err;

		break;
	case STMT_RETURN:
	{
		struct ASTNode *parentFn = node->parent;
		while (parentFn->type != DECL_FN_IMPL)
			parentFn = parentFn->parent;
		enum ValType retType = parentFn->val.type;

		struct Instr instr = { 0 };
		if (retType != VT_VOID) {
			if (!node->a) {
				logErrorAtLine(node->fp, "Return value needed");
				return ERR_RETURN_VAL;
			}
			/*instr.type = vtToMovInstr(retType);
			arg.val.type = VT_REG;
			arg.val.i = -1;
			err = addInstrArg(&instr, &arg);
			if (err)
				return err;

			arg.val.type = retType;
			err = genExprAsArg(ig, node->a, &arg, locals, locals + 1);
			if (err)
				return err;
			err = addInstrArg(&instr, &arg);
			if (err)
				return err;

			err = pushInstr(ig, &instr);
			if (err)
				return err;
			memset(&instr, 0, sizeof(instr));*/
			err = genExprAsReg(ig, node->a, destReg, locals);
			if (err)
				return err;
		} else {
			if (node->a) {
				logErrorAtLine(node->fp, "Cannot return a value from a void function");
				return ERR_RETURN_VAL;
			}
		}
		
		instr.type = INSTR_RET;
		err = pushInstr(ig, &instr);
		if (err)
			return err;
		break;
	}
	case STMT_BREAK:
		if (breakLbl < 0) {
			logErrorAtLine(node->fp, "\'break\' statement outside loop or switch");
			return ERR_INVALID_BREAK;
		}
		err = pushInstrJmp(ig, INSTR_JMP, breakLbl, NULL);
		if (err)
			return err;
		break;
	case STMT_CONTINUE:
		if (continueLbl < 0) {
			logErrorAtLine(node->fp, "\'continue\' statement outside loop");
			return ERR_INVALID_BREAK;
		}
		err = pushInstrJmp(ig, INSTR_JMP, continueLbl, NULL);
		if (err)
			return err;
		break;
	case STMT_SWITCH:
		err = genSwitch(ig, node, destReg, locals, breakLbl, continueLbl);
		if (err)
			return err;
		break;
	case STMT_GOTO:
		assert(node->a && node->a->type == EXPR_IDENT);
		if (node->a->identRef->reg == -1) {
			err = pushLabel(ig, "goto", &node->a->identRef->reg);
			if (err)
				return err;
		}
		err = pushInstrJmp(ig, INSTR_JMP, node->a->identRef->reg, NULL);
		break;
	case STMT_LOOP:
		err = pushLabel(ig, "loop_start", &loopStartLbl);
		if (err)
			return err;
		err = pushLabel(ig, "loop_iter", &loopIterLbl);
		if (err)
			return err;
		err = pushLabel(ig, "loop_cond", &loopCondLbl);
		if (err)
			return err;
		err = pushLabel(ig, "loop_end", &loopEndLbl);
		if (err)
			return err;

		if (node->a) {
			err = genExprAsReg(ig, node->a, locals, locals + 1);
			locals++;
			if (err)
				return err;
			if (!(node->a->type == EXPR_INT && node->a->val.i > 0)) {
				err = pushInstrJmp(ig, INSTR_JMP, loopCondLbl, NULL);
				if (err)
					return err;
			}
		}
		setLabelOffset(ig, loopStartLbl);
		err = genStmt(ig, node->b, destReg, locals, loopEndLbl, loopIterLbl);
		if (err)
			return err;
		setLabelOffset(ig, loopIterLbl);
		if (node->a) {
			locals--;
			struct Instr instr = { 0 };
			instr.type = INSTR_SUBI;
			arg.val.type = VT_REG;
			arg.val.i = locals;
			err = addInstrArg(&instr, &arg);
			if (err)
				return err;
			err = addInstrArg(&instr, &arg);
			if (err)
				return err;
			arg.val.type = VT_INT;
			arg.val.i = 1;
			err = addInstrArg(&instr, &arg);
			if (err)
				return err;

			err = pushInstr(ig, &instr);
			if (err)
				return err;

			setLabelOffset(ig, loopCondLbl);

			arg.val.type = VT_REG;
			arg.val.i = locals;
			err = pushInstrJmp(ig, INSTR_JNZ, loopStartLbl, &arg);
		} else {
			setLabelOffset(ig, loopCondLbl);
			err = pushInstrJmp(ig, INSTR_JMP, loopStartLbl, NULL);
		}
		if (err)
			return err;
		setLabelOffset(ig, loopEndLbl);

		break;
	case LBL_CASE:
	case LBL_DEFAULT:
		err = setLabelOffset(ig, node->reg);
		if (err)
			return err;
		break;
	case LBL_GOTO:
		if (node->reg == -1) {
			err = pushLabel(ig, "goto", &node->reg);
			if (err)
				return err;
		}
		err = setLabelOffset(ig, node->reg);
		if (err)
			return err;
		break;
	default:
		/* Expressions */
		err = genExprAsReg(ig, node, locals, locals + 1);
		if (err)
			return err;
		break;
	}
	return err;
}

static int valTypeToParam(enum ValType vt, bool ref) {
	switch(vt) {
	case VT_INT: return ref? 'I' : 'i';
	case VT_FLOAT: return ref? 'F' : 'f';
	case VT_STRING: return ref? 'S' : 's';
	case VT_ENTITY: return ref? 'E' : 'e';
	default:
		return 0;
	}
}
static int genRootDecls(struct InstrGen *ig, struct ASTNode *root) {
	int err = 0;
	for (int i = 0; i < root->listLen; i++) {
		struct ASTNode *node = root->list[i];

		struct Import import = { 0 };
		struct Global global = { 0 };
		struct Fn fn = { 0 };
		int pStart;
		switch (node->type) {
		case DECL_TYPE: /* TODO */
			break;
		case DECL_IMPORT:
			import.name = node->val.s;
			import.nameLen = node->val.sLen;
			err = pushImport(ig, &import);
			if (err)
				return err;
			break;
		case DECL_VAR:
			if (node->flags & VAR_FN_EXTERN || node->flags & VAR_CONST)
				continue;
			logErrorAtLine(node->fp, "Unimplemented: mutable globals");
			/*
			global.name = node->val.s;
			global.nameLen = node->val.sLen;
			if (node->a->type < EXPR_INT || node->a->type > EXPR_STRING) {
				logErrorAtLine(node->fp, "Unimplemented: constant expressions");
				return ERR_UNIMPLEMENTED;
			}
			global.isConst = true;
			memcpy(&global.val, &node->a->val, sizeof(global.val));*/
			break;
		case DECL_FN_IMPL:
			if (node->flags & VAR_FN_EXTERN || node->flags & FN_INLINE)
				continue;
			fn.name = node->val.s;
			fn.nameLen = node->val.sLen;
			fn.nParams = node->listLen;

			if (node->val.type == VT_VOID || node->flags & FN_ASYNC) {
				fn.params = malloc(node->listLen);
				pStart = 0;
			} else {
				fn.nParams++;
				fn.params = malloc(node->listLen + 1);
				pStart = 1;
				fn.params[0] = valTypeToParam(node->val.type, true);
			}
			for (int j = 0; j < node->listLen; j++) {
				struct ASTNode *param = node->list[j];
				fn.params[j + pStart] = valTypeToParam(param->val.type, param->flags & VAR_REF);
			}
			err = pushFnStart(ig, &fn);
			if (err)
				return err;
			err = genStmt(ig, node->a, -1, 0, -1, -1);
			if (err)
				return err;
			err = pushFnEnd(ig);
			if (err)
				return err;
			break;
		default:
			break;
		}
	}

	return err;
}

int semVal(struct ASTNode *root, struct InstrGen **igState) {
	setBuiltins();

	int err = doPass(root, passTd1, passBu1);
	if (err) {
		return err;
	}
	err = doPass(root, NULL, constFold);
	if (err) {
		return err;
	}
	err = doPass(root, passTd2, NULL);
	if (err) {
		return err;
	}


	struct InstrGen *ig;
	err = newInstrGen(&ig);
	if (err)
		return err;

	err = genRootDecls(ig, root);
	if (err)
		return err;

	*igState = ig;
	return 0;
}
