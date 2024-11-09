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

struct ASTNode *allocNode(void) {
	struct ASTNode *node = calloc(1, sizeof(struct ASTNode));
	if (!node) {
		fprintf(stderr, "Out of memory!\n");
		return NULL;
	}
	node->parentListIdx = -1;
	return node;
}
void freeNode(struct ASTNode *node) {
	if (node->a) {
		freeNode(node->a);
	}
	if (node->b) {
		freeNode(node->b);
	}
	if (node->c) {
		freeNode(node->c);
	}
	if (node->d) {
		freeNode(node->d);
	}
	if (node->list) {
		if (node->type != STMT_SWITCH) {
			for (int i = 0; i < node->listLen; i++) {
				freeNode(node->list[i]);
			}
		}
		free(node->list);
	}
	free(node);
}
struct ASTNode *copyNode(struct ASTNode *node) {
	struct ASTNode *n = allocNode();
	if (!n)
		return NULL;
	memcpy(n, node, sizeof(*n));
	n->a = n->b = n->c = n->d = NULL;
	n->list = NULL;
	n->listLen = 0;
	if (node->a) {
		n->a = copyNode(node->a);
		if (!n->a)
			goto free_node;
		n->a->parent = n;
	}
	if (node->b) {
		n->b = copyNode(node->b);
		if (!n->b)
			goto free_node;
		n->b->parent = n;
	}
	if (node->c) {
		n->c = copyNode(node->c);
		if (!n->c)
			goto free_node;
		n->c->parent = n;
	}
	if (node->d) {
		n->d = copyNode(node->d);
		if (!n->d)
			goto free_node;
		n->d->parent = n;
	}
	if (node->list) {
		n->list = malloc(node->listLen * sizeof(struct ASTNode *));
		for (int i = 0; i < node->listLen; i++) {
			n->list[i] = copyNode(node->list[i]);
			if (!n->list[i])
				goto free_node;
			n->list[i]->parent = n;
			assert(n->list[i]->parentListIdx == i);
			n->listLen++;
		}
	}

	return n;

free_node:
	freeNode(n);
	return NULL;
}


int addToNodeList(struct ASTNode *parent, struct ASTNode *child) {
	parent->listLen += 1;
	parent->list = realloc(parent->list, parent->listLen * sizeof(struct ASTNode *));
	if (!parent->list) {
		fprintf(stderr, "Out of memory!\n");
		return ERR_NO_MEM;
	}
	child->parentListIdx = parent->listLen - 1;
	child->parent = parent;
	parent->list[child->parentListIdx] = child;
	return 0;
}



/* --- EXPRESSIONS --- */

static int parseExpr(struct ASTNode **newNode);

static int parseExprIdent(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, IDENTIFIER);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = EXPR_IDENT;
	n->fp = t->fp;
	n->val.sLen = t->textLen;
	n->val.s = t->text;
	*newNode = n;

	return 0;
}
static int parseExprInt(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, LIT_INT);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = EXPR_INT;
	n->fp = t->fp;
	n->val.type = VT_INT;

	/* Parse the int, auto-detect base */
	errno = 0;
	n->val.i = strtoll(t->text, NULL, 0);
	if (errno) {
		logErrorAtLine(t->fp, "Invalid integer literal: %s", t->text);
		err = ERR_INVALID_INT;
		goto free_node;
	}

	*newNode = n;

	return err;

free_node:
	freeNode(n);
	return err;
}
static int parseExprFloat(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, LIT_FLOAT);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = EXPR_FLOAT;
	n->fp = t->fp;
	n->val.type = VT_FLOAT;

	/* Parse the float */
	errno = 0;
	n->val.f = strtof(t->text, NULL);
	if (errno) {
		logErrorAtLine(t->fp, "Invalid floating-point literal: %s", t->text);
		err = ERR_INVALID_FLOAT;
		goto free_node;
	}

	*newNode = n;

	return err;

free_node:
	freeNode(n);
	return err;
}
static int parseExprString(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, LIT_STRING);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = EXPR_STRING;
	n->fp = t->fp;
	n->val.type = VT_STRING;
	n->val.sLen = t->textLen;
	n->val.s = t->text;
	*newNode = n;

	return 0;
}
static int parseExprArray(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, SYM_BRACE_OPEN);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = EXPR_ARRAY;
	n->fp = t->fp;
	n->val.type = VT_INVALID;

	while (true) {
		struct ASTNode *child;
		err = parseExpr(&child);
		if (err) {
			goto free_node;
		}

		addToNodeList(n, child);

		t = nextToken();
		if (!t) {
			err = ERR_UNEXPECTED_EOF;
			goto free_node;
		}

		if (t->type == SYM_BRACE_CLOSE) {
			break;
		} else if (t->type != SYM_COMMA) {
			logErrorAtLine(t->fp, "Expected a ',' in array literal\n");
			err = ERR_INVALID_EXPR;
			goto free_node;
		}
	}
	n->val.sLen = t->textLen;
	n->val.s = t->text;
	n->flags |= VAR_ARRAY;

	*newNode = n;
	return 0;

free_node:
	freeNode(n);
	return err;
}
static int parseExprPrimary(struct ASTNode **newNode) {
	struct Token *t = nextToken();
	if (!t)
		return ERR_UNEXPECTED_EOF;

	int err = 0;
	if (t->type == SYM_PARENTH_OPEN) {
		struct ASTNode *n;
		err = parseExpr(&n);
		if (err) {
			return err;
		}

		err = nextTokenExpect(&t, SYM_PARENTH_CLOSE);
		if (err) {
			freeNode(n);
			return err;
		}
		*newNode = n;
		return 0;
	}
	switch (t->type) {
	case IDENTIFIER:
		tokenBacktrack(1);
		err = parseExprIdent(newNode);
		if (err)
			return err;
		break;
	case LIT_INT:
		tokenBacktrack(1);
		err = parseExprInt(newNode);
		if (err)
			return err;
		break;
	case LIT_FLOAT:
		tokenBacktrack(1);
		err = parseExprFloat(newNode);
		if (err)
			return err;
		break;
	case LIT_STRING:
		tokenBacktrack(1);
		err = parseExprString(newNode);
		if (err)
			return err;
		break;
	case SYM_BRACE_OPEN:
		tokenBacktrack(1);
		err = parseExprArray(newNode);
		if (err)
			return err;
		break;
	default:
		logErrorAtLine(t->fp, "Invalid expression: %s\n", t->text);
		err = ERR_INVALID_EXPR;
		return err;
	}
	return 0;
}

static int parseExprFnCall(struct ASTNode **newNode, struct ASTNode *fnName) {
	struct Token *t;
	int err = nextTokenExpect(&t, SYM_PARENTH_OPEN);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = EXPR_FN_CALL;
	n->fp = t->fp;
	n->a = fnName;
	fnName->parent = n;

	/* Parse arguments */
	bool needsComma = false;
	while (true) {
		t = nextToken();
		if (!t) {
			err = ERR_UNEXPECTED_EOF;
			goto free_node;
		}
		if (t->type == SYM_COMMA) {
			if (!needsComma) {
				logErrorAtLine(t->fp, "Erroneous comma in function call args");
				err = ERR_UNEXPECTED_TOKEN;
				goto free_node;
			}
			needsComma = false;
		} else if (t->type == SYM_PARENTH_CLOSE) {
			break;
		} else {
			if (needsComma) {
				logErrorAtLine(t->fp, "Missing comma in function call args");
				err = ERR_UNEXPECTED_TOKEN;
				goto free_node;
			}
			needsComma = true;

			tokenBacktrack(1);

			struct ASTNode *param;
			err = parseExpr(&param);
			if (err)
				goto free_node;

			err = addToNodeList(n, param);
			if (err)
				goto free_node;
		}
	}

	*newNode = n;

	return err;

free_node:
	freeNode(n);
	return err;
}

static int parseExprArrayIdx(struct ASTNode **newNode, struct ASTNode *arrayName) {
	struct Token *t;
	int err = nextTokenExpect(&t, SYM_BRACE_OPEN);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = EXPR_ARRAY_IDX;
	n->fp = t->fp;
	n->a = arrayName;
	arrayName->parent = n;

	struct ASTNode *idx;
	err = parseExpr(&idx);
	if (err)
		goto free_node;

	n->b = idx;
	idx->parent = n;

	err = nextTokenExpect(&t, SYM_BRACE_CLOSE);
	if (err)
		goto free_node;

	*newNode = n;

	return 0;

free_node:
	freeNode(n);
	return err;
}

static int parseExprPostfix(struct ASTNode **newNode) {
	struct ASTNode *a;
	int err = parseExprPrimary(&a);
	if (err)
		return err;

	while (true) {
		struct Token *t = nextToken();
		if (!t) {
			err = ERR_UNEXPECTED_EOF;
			goto free_a;
		}

		struct ASTNode *n;
		switch (t->type) {
		case SYM_PARENTH_OPEN:
			tokenBacktrack(1);
			err = parseExprFnCall(&n, a);
			if (err)
				return err;
			a = n;
			break;
		case OP_INCR:
			n = allocNode();
			if (!n) {
				err = ERR_NO_MEM;
				goto free_a;
			}
			n->type = EXPR_POSTINCR;
			n->fp = t->fp;
			n->a = a;
			a->parent = n;
			a = n;
			break;
		case OP_DECR:
			n = allocNode();
			if (!n) {
				err = ERR_NO_MEM;
				goto free_a;
			}
			n->type = EXPR_POSTDECR;
			n->fp = t->fp;
			n->a = a;
			a->parent = n;
			a = n;
			break;
		case SYM_BRACE_OPEN:
			tokenBacktrack(1);
			err = parseExprArrayIdx(&n, a);
			if (err)
				return err;
			a = n;
			break;
		default:
			tokenBacktrack(1);
			*newNode = a;
			return 0;
		}
	}

free_a:
	freeNode(a);
	return err;
}

static int parseExprPrefix(struct ASTNode **newNode) {
	struct Token *t = nextToken();
	if (!t)
		return ERR_UNEXPECTED_EOF;

	enum ASTNodeType type = 0;
	switch (t->type) {
	case OP_INCR:
		type = EXPR_PREINCR;
		break;
	case OP_DECR:
		type = EXPR_PREDECR;
		break;
	case OP_PLUS:
		type = EXPR_PLUS;
		break;
	case OP_MIN:
		type = EXPR_MIN;
		break;
	case OP_NOT:
		type = EXPR_NOT;
		break;
	case OP_BW_INV:
		type = EXPR_BW_INV;
		break;
	default:
		break;
	}
	if (type) {
		struct ASTNode *n = allocNode();
		if (!n)
			return ERR_NO_MEM;
		n->type = type;
		n->fp = t->fp;
		int err = parseExprPrefix(&n->a);
		if (err) {
			freeNode(n);
			return err;
		}
		n->a->parent = n;
		*newNode = n;
		return 0;
	} else {
		tokenBacktrack(1);
		return parseExprPostfix(newNode);
	}
}

/* Generic function for parsing left-to-right infix operators */
static int parseExprInfix(struct ASTNode **newNode, const int *opConv, int nOps, int (*parseSubExpr)(struct ASTNode **)) {
	struct ASTNode *a;
	int err = parseSubExpr(&a);
	if (err)
		return err;

	while (true) {
		struct Token *t = nextToken();
		if (!t) {
			err = ERR_UNEXPECTED_EOF;
			goto free_a;
		}

		enum ASTNodeType type = 0;
		for (int i = 0; i < nOps; i++) {
			if ((enum TokenType)(opConv[i * 2]) == t->type) {
				type = opConv[i * 2 + 1];
				break;
			}
		}

		if (type) {
			struct ASTNode *b;
			err = parseSubExpr(&b);
			if (err)
				goto free_a;
			struct ASTNode *n = allocNode();
			if (!n) {
				err = ERR_NO_MEM;
				goto free_a;
			}
			n->type = type;
			n->fp = t->fp;
			n->a = a;
			a->parent = n;
			n->b = b;
			b->parent = n;
			a = n;
		} else {
			tokenBacktrack(1);
			*newNode = a;
			return 0;
		}
	}

	*newNode = a;
	return err;

free_a:
	freeNode(a);
	return err;
}
static int parseExprMult(struct ASTNode **newNode) {
	const int op[6] = {
		OP_MULT, EXPR_MULT,
		OP_DIV, EXPR_DIV,
		OP_MOD, EXPR_MOD
	};
	return parseExprInfix(newNode, op, 3, parseExprPrefix);
}
static int parseExprAdd(struct ASTNode **newNode) {
	const int op[4] = {
		OP_PLUS, EXPR_ADD,
		OP_MIN, EXPR_SUB,
	};
	return parseExprInfix(newNode, op, 2, parseExprMult);
}
static int parseExprBwShift(struct ASTNode **newNode) {
	const int op[4] = {
		OP_BW_SHL, EXPR_BW_SHL,
		OP_BW_SHR, EXPR_BW_SHR,
	};
	return parseExprInfix(newNode, op, 2, parseExprAdd);
}
static int parseExprRelat(struct ASTNode **newNode) {
	const int op[8] = {
		OP_LESS, EXPR_LESS,
		OP_LESSEQ, EXPR_LESSEQ,
		OP_GTR, EXPR_GTR,
		OP_GTREQ, EXPR_GTREQ
	};
	return parseExprInfix(newNode, op, 4, parseExprBwShift);
}
static int parseExprEqual(struct ASTNode **newNode) {
	const int op[4] = {
		OP_EQ, EXPR_EQ,
		OP_NEQ, EXPR_NEQ,
	};
	return parseExprInfix(newNode, op, 2, parseExprRelat);
}
static int parseExprBwAnd(struct ASTNode **newNode) {
	const int op[2] = {
		OP_BW_AND, EXPR_BW_AND,
	};
	return parseExprInfix(newNode, op, 1, parseExprEqual);
}
static int parseExprBwXor(struct ASTNode **newNode) {
	const int op[2] = {
		OP_BW_XOR, EXPR_BW_XOR,
	};
	return parseExprInfix(newNode, op, 1, parseExprBwAnd);
}
static int parseExprBwOr(struct ASTNode **newNode) {
	const int op[2] = {
		OP_BW_OR, EXPR_BW_OR,
	};
	return parseExprInfix(newNode, op, 1, parseExprBwXor);
}
static int parseExprAnd(struct ASTNode **newNode) {
	const int op[2] = {
		OP_AND, EXPR_AND,
	};
	return parseExprInfix(newNode, op, 1, parseExprBwOr);
}
static int parseExprOr(struct ASTNode **newNode) {
	const int op[2] = {
		OP_OR, EXPR_OR,
	};
	return parseExprInfix(newNode, op, 1, parseExprAnd);
}
static int parseExprCond(struct ASTNode **newNode) {
	struct ASTNode *a;
	int err = parseExprOr(&a);
	if (err)
		return err;

	struct Token *t = nextToken();
	if (!t) {
		err = ERR_UNEXPECTED_EOF;
		goto free_a;
	}
	if (t->type != OP_TERNARY) {
		tokenBacktrack(1);
		*newNode = a;
		return err;
	}

	struct ASTNode *n = allocNode();
	if (!n) {
		err = ERR_NO_MEM;
		goto free_a;
	}
	n->type = EXPR_TERNARY;
	n->fp = t->fp;
	n->a = a;
	a->parent = n;
	err = parseExpr(&n->b);
	if (err)
		goto free_n;
	n->b->parent = n;

	err = nextTokenExpect(&t, SYM_COLON);
	if (err)
		goto free_n;

	err = parseExpr(&n->c);
	if (err)
		goto free_n;
	n->c->parent = n;

	*newNode = n;
	return err;

free_n:
	freeNode(n);
	return err;
free_a:
	freeNode(a);
	return err;
}
static int parseExprAssign(struct ASTNode **newNode) {
	struct ASTNode *a;
	int err = parseExprCond(&a);
	if (err)
		return err;

	struct Token *t = nextToken();
	if (!t)
		return ERR_UNEXPECTED_EOF;

	const int assignOps[22] = {
		OP_ASSIGN, EXPR_ASSIGN,
		OP_PLUSEQ, EXPR_ADDEQ,
		OP_MINEQ, EXPR_SUBEQ,
		OP_MULTEQ, EXPR_MULTEQ,
		OP_DIVEQ, EXPR_DIVEQ,
		OP_MODEQ, EXPR_MODEQ,
		OP_BW_ANDEQ, EXPR_BW_ANDEQ,
		OP_BW_OREQ, EXPR_BW_OREQ,
		OP_BW_XOREQ, EXPR_BW_XOREQ,
		OP_BW_SHLEQ, EXPR_BW_SHLEQ,
		OP_BW_SHREQ, EXPR_BW_SHREQ,
	};
	enum ASTNodeType type = 0;
	for (int i = 0; i < 11; i++) {
		if (t->type == (enum TokenType)assignOps[i * 2]) {
			type = assignOps[i * 2 + 1];
			break;
		}
	}
	if (!type) {
		tokenBacktrack(1);
		*newNode = a;
		return err;
	}

	//if (a->type != EXPR_IDENT ) {
	//	logErrorAtLine(t->fp, "Expression left of assignment is not a valid identifier");
	//	err = ERR_INVALID_EXPR;
	//	goto free_a;
	//}

	struct ASTNode *n = allocNode();
	if (!n) {
		err = ERR_NO_MEM;
		goto free_a;
	}
	n->type = type;
	n->fp = t->fp;
	n->a = a;
	a->parent = n;

	err = parseExprAssign(&n->b);
	if (err)
		goto free_n;
	n->b->parent = n;

	*newNode = n;
	return err;

free_n:
	freeNode(n);
	return err;
free_a:
	freeNode(a);
	return err;
}

static int parseExpr(struct ASTNode **newNode) {
	return parseExprAssign(newNode);
}

/* --- STATEMENTS --- */

static int parseStmt(struct ASTNode **newNode);
static int parseDeclVar(struct ASTNode *n);

static int parseStmtBlock(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, SYM_CURLY_OPEN);
	if (err)
		return err;
	
	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = STMT_BLOCK;
	n->fp = t->fp;

	while (true) {
		t = nextToken();
		if (!t) {
			err = ERR_UNEXPECTED_EOF;
			goto free_node;
		}

		if (t->type == SYM_CURLY_CLOSE) {
			break;
		} else {
			tokenBacktrack(1);

			struct ASTNode *child;
			err = parseStmt(&child);
			if (err)
				goto free_node;
			err = addToNodeList(n, child);
			if (err)
				goto free_node;
		}
	}

	*newNode = n;

	return err;

free_node:
	freeNode(n);
	return err;
}

static int parseStmtIf(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, KEYW_IF);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = STMT_IF;
	n->fp = t->fp;

	t = nextToken();
	if (!t) {
		err = ERR_UNEXPECTED_EOF;
		goto free_node;
	}
	if (t->type == OP_NOT) {
		n->flags |= COND_INVERT;
		err = nextTokenExpect(&t, SYM_PARENTH_OPEN);
		if (err)
			goto free_node;
	} else if (t->type != SYM_PARENTH_OPEN) {
		logErrorAtLine(t->fp, "Expected '(' or '!' for if condition");
		err = ERR_UNEXPECTED_TOKEN;
		goto free_node;
	}

	err = parseExpr(&n->a);
	if (err)
		goto free_node;
	n->a->parent = n;

	err = nextTokenExpect(&t, SYM_PARENTH_CLOSE);
	if (err)
		goto free_node;

	err = parseStmt(&n->b);
	if (err)
		goto free_node;
	n->b->parent = n;

	t = nextToken();
	if (!t) {
		err = ERR_UNEXPECTED_EOF;
		goto free_node;
	}

	if (t->type != KEYW_ELSE) {
		tokenBacktrack(1);
		*newNode = n;
		return err;
	}

	err = parseStmt(&n->c);
	if (err)
		goto free_node;
	n->c->parent = n;

	*newNode = n;

	return err;

free_node:
	freeNode(n);
	return err;
}

static int parseStmtWhile(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, KEYW_WHILE);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = STMT_WHILE;
	n->fp = t->fp;

	t = nextToken();
	if (!t) {
		err = ERR_UNEXPECTED_EOF;
		goto free_node;
	}
	if (t->type == OP_NOT) {
		n->flags |= COND_INVERT;
		err = nextTokenExpect(&t, SYM_PARENTH_OPEN);
		if (err)
			goto free_node;
	} else if (t->type != SYM_PARENTH_OPEN) {
		logErrorAtLine(t->fp, "Expected '(' or '!' for while condition");
		err = ERR_UNEXPECTED_TOKEN;
		goto free_node;
	}

	err = parseExpr(&n->a);
	if (err)
		goto free_node;
	n->a->parent = n;

	err = nextTokenExpect(&t, SYM_PARENTH_CLOSE);
	if (err)
		goto free_node;

	err = parseStmt(&n->b);
	if (err)
		goto free_node;
	n->b->parent = n;

	*newNode = n;

	return err;

free_node:
	freeNode(n);
	return err;
}

static int parseStmtFor(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, KEYW_FOR);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = STMT_FOR;
	n->fp = t->fp;

	err = nextTokenExpect(&t, SYM_PARENTH_OPEN);
	if (err)
		goto free_node;

	/* Initializer */
	t = nextToken();
	if (!t) {
		err = ERR_UNEXPECTED_EOF;
		goto free_node;
	}
	tokenBacktrack(1);
	if (t->type >= KEYW_VAR_START && t->type <= KEYW_VAR_END) {
		/* Variable declaration in initializer */
		struct ASTNode *v = allocNode();
		if (!v) {
			err = ERR_NO_MEM;
			goto free_node;
		}
		err = parseDeclVar(v);
		if (err) {
			freeNode(v);
			goto free_node;
		}
		n->a = v;
		n->a->parent = n;

		/* parseDeclVar already expects a ';' */
	} else {
		err = parseExpr(&n->a);
		if (err)
			goto free_node;
		n->a->parent = n;

		err = nextTokenExpect(&t, SYM_SEMICOLON);
		if (err)
			goto free_node;
	}

	/* Condition */
	err = parseExpr(&n->b);
	if (err)
		goto free_node;
	n->b->parent = n;

	err = nextTokenExpect(&t, SYM_SEMICOLON);
	if (err)
		goto free_node;

	/* Iteration */
	err = parseExpr(&n->c);
	if (err)
		goto free_node;
	n->c->parent = n;

	err = nextTokenExpect(&t, SYM_PARENTH_CLOSE);
	if (err)
		goto free_node;

	err = parseStmt(&n->d);
	if (err)
		goto free_node;
	n->d->parent = n;

	*newNode = n;

	return err;

free_node:
	freeNode(n);
	return err;
}

static int parseStmtReturn(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, KEYW_RETURN);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = STMT_RETURN;
	n->fp = t->fp;

	t = nextToken();
	if (!t) {
		err = ERR_UNEXPECTED_EOF;
		goto free_node;
	}
	if (t->type != SYM_SEMICOLON) {
		tokenBacktrack(1);

		err = parseExpr(&n->a);
		if (err)
			goto free_node;
		n->a->parent = n;

		err = nextTokenExpect(&t, SYM_SEMICOLON);
		if (err)
			goto free_node;
	}

	*newNode = n;

	return err;

free_node:
	freeNode(n);
	return err;
}

static int parseStmtSwitch(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, KEYW_SWITCH);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = STMT_SWITCH;
	n->val.type = VT_INT;
	n->fp = t->fp;

	err = nextTokenExpect(&t, SYM_PARENTH_OPEN);
	if (err)
		goto free_node;

	err = parseExpr(&n->a);
	if (err)
		goto free_node;
	n->a->parent = n;

	err = nextTokenExpect(&t, SYM_PARENTH_CLOSE);
	if (err)
		goto free_node;

	err = parseStmt(&n->b);
	if (err)
		goto free_node;
	n->b->parent = n;

	*newNode = n;

	return err;

free_node:
	freeNode(n);
	return err;
}

static int parsetStmtLoop(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, KEYW_LOOP);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = STMT_LOOP;
	n->val.type = VT_INT;
	n->fp = t->fp;

	t = nextToken();
	if (!t) {
		err = ERR_UNEXPECTED_EOF;
		goto free_node;
	}
	if (t->type == SYM_PARENTH_OPEN) {
		/* loop N times */
		err = parseExpr(&n->a);
		if (err)
			goto free_node;
		n->a->parent = n;

		err = nextTokenExpect(&t, SYM_PARENTH_CLOSE);
		if (err)
			goto free_node;
	} else {
		/* infinite loop */
		tokenBacktrack(1);
	}

	err = parseStmt(&n->b);
	if (err)
		goto free_node;
	n->b->parent = n;

	*newNode = n;

	return err;

free_node:
	freeNode(n);
	return err;
}

static int parseStmt(struct ASTNode **newNode) {
	struct Token *t = nextToken();
	if (!t)
		return ERR_UNEXPECTED_EOF;
	tokenBacktrack(1);

	int err = 0;
	struct ASTNode *n;
	switch (t->type) {
	case SYM_CURLY_OPEN:
		/* Block */
		err = parseStmtBlock(newNode);
		if (err)
			return err;
		break;
	case SYM_SEMICOLON:
		/* no-op */
		nextToken();
		break;
	case KEYW_CONST: /* fallthrough*/
	case KEYW_INT:
	case KEYW_FLOAT:
	case KEYW_STRING:
	case KEYW_ENTITY:
		/* Variable declaration */
		n = allocNode();
		if (!n)
			return ERR_NO_MEM;
		err = parseDeclVar(n);
		if (err) {
			freeNode(n);
			return err;
		}
		*newNode = n;
		break;
	case KEYW_IF:
		err = parseStmtIf(newNode);
		if (err)
			return err;
		break;
	case KEYW_WHILE:
		err = parseStmtWhile(newNode);
		if (err)
			return err;
		break;
	case KEYW_FOR:
		err = parseStmtFor(newNode);
		if (err)
			return err;
		break;
	case KEYW_RETURN:
		err = parseStmtReturn(newNode);
		if (err)
			return err;
		break;
	case KEYW_BREAK:
		err = nextTokenExpect(&t, KEYW_BREAK);
		if (err)
			return err;
		n = allocNode();
		if (!n)
			return ERR_NO_MEM;
		n->type = STMT_BREAK;
		n->fp = t->fp;
		err = nextTokenExpect(&t, SYM_SEMICOLON);
		if (err) {
			freeNode(n);
			return err;
		}
		*newNode = n;
		break;
	case KEYW_CONTINUE:
		err = nextTokenExpect(&t, KEYW_CONTINUE);
		if (err)
			return err;
		n = allocNode();
		if (!n)
			return ERR_NO_MEM;
		n->type = STMT_CONTINUE;
		n->fp = t->fp;
		err = nextTokenExpect(&t, SYM_SEMICOLON);
		if (err) {
			freeNode(n);
			return err;
		}
		*newNode = n;
		break;
	case KEYW_SWITCH:
		err = parseStmtSwitch(newNode);
		if (err)
			return err;
		break;
	case KEYW_DEFAULT:
		err = nextTokenExpect(&t, KEYW_DEFAULT);
		if (err)
			return err;
		n = allocNode();
		if (!n)
			return ERR_NO_MEM;
		n->type = LBL_DEFAULT;
		n->fp = t->fp;
		err = nextTokenExpect(&t, SYM_COLON);
		if (err) {
			freeNode(n);
			return err;
		}
		*newNode = n;
		break;
	case KEYW_CASE:
		err = nextTokenExpect(&t, KEYW_CASE);
		if (err)
			return err;
		n = allocNode();
		if (!n)
			return ERR_NO_MEM;
		n->type = LBL_CASE;
		n->fp = t->fp;
		err = parseExpr(&n->a);
		if (err) {
			freeNode(n);
			return err;
		}
		n->a->parent = n;
		err = nextTokenExpect(&t, SYM_COLON);
		*newNode = n;
		break;
	case KEYW_GOTO:
		err = nextTokenExpect(&t, KEYW_GOTO);
		if (err)
			return err;
		n = allocNode();
		if (!n)
			return ERR_NO_MEM;
		n->type = STMT_GOTO;
		n->fp = t->fp;
		err = parseExprIdent(&n->a);
		if (err) {
			freeNode(n);
			return err;
		}
		n->a->parent = n;
		err = nextTokenExpect(&t, SYM_SEMICOLON);
		*newNode = n;
		break;
	case KEYW_LOOP:
		err = parsetStmtLoop(newNode);
		if (err)
			return err;
		break;
	case IDENTIFIER:
	{
		err = nextTokenExpect(&t, IDENTIFIER);
		if (err)
			return err;
		struct Token *t2 = nextToken();
		if (!t2)
			return ERR_UNEXPECTED_EOF;
		if (t2->type == SYM_COLON) {
			n = allocNode();
			if (!n)
				return ERR_NO_MEM;
			n->type = LBL_GOTO;
			n->fp = t->fp;
			n->reg = -1;
			n->val.s = t->text;
			n->val.sLen = t->textLen;
			*newNode = n;
			break;
		}
		err = tokenBacktrack(2);
		if (err)
			return err;
		/* fallthrough */
	}
	default:
		/* Expression */
		err = parseExpr(newNode);
		if (err)
			return err;
		err = nextTokenExpect(&t, SYM_SEMICOLON);
		if (err)
			return err;
		break;
	}
	return err;
}

/* --- Top-level declarations --- */

static enum ValType keywToVt(enum TokenType t) {
	switch (t) {
	case KEYW_VOID:
		return VT_VOID;
	case KEYW_INT:
		return VT_INT;
	case KEYW_FLOAT:
		return VT_FLOAT;
	case KEYW_STRING:
		return VT_STRING;
	case KEYW_ENTITY:
		return VT_ENTITY;
	default:
		return VT_INVALID;
	}
}

static int parseExtern(struct ASTNode *node, struct Token **t) {
	node->flags |= VAR_FN_EXTERN;
	int err = nextTokenExpect(t, SYM_PARENTH_OPEN);
	if (err)
		return err;

	struct ASTNode *sub;
	err = parseExpr(&sub);
	if (err)
		return err;
	node->d = sub;
	sub->parent = node;

	err = nextTokenExpect(t, SYM_PARENTH_CLOSE);
	if (err)
		return err;

	*t = nextToken();
	if (!(*t)) {
		err = ERR_UNEXPECTED_EOF;
		return err;
	}
	return 0;
}
static int parseDeclVar(struct ASTNode *n) {
	struct Token *t = nextToken();
	if (!t)
		return ERR_UNEXPECTED_EOF;

	n->type = DECL_VAR_LIST;
	n->fp = t->fp;

	int err = 0;
	if (t->type == KEYW_CONST) {
		n->flags |= VAR_CONST;
		t = nextToken();
		if (!t) {
			err = ERR_UNEXPECTED_EOF;
			goto free_node;
		}
	}

	n->val.type = keywToVt(t->type);
	if (!n->val.type || n->val.type == VT_VOID) {
		logErrorAtLine(t->fp, "Invalid variable type: %s", t->text);
		err = ERR_UNEXPECTED_TOKEN;
		goto free_node;
	}

	/* Check if array */
	t = nextToken();
	if (!t) {
		err = ERR_UNEXPECTED_EOF;
		goto free_node;
	}
	if (t->type == SYM_BRACE_OPEN) {
		n->flags |= VAR_ARRAY;
		err = nextTokenExpect(&t, SYM_BRACE_CLOSE);
		if (err)
			goto free_node;
	} else {
		tokenBacktrack(1);
	}

	while (true) {
		err = nextTokenExpect(&t, IDENTIFIER);
		if (err)
			goto free_node;

		struct ASTNode *item = allocNode();
		item->type = DECL_VAR;
		item->fp = t->fp;
		item->flags = n->flags;
		item->val.type = n->val.type;
		item->val.s = t->text;
		item->val.sLen = t->textLen;
		addToNodeList(n, item);

		t = nextToken();
		if (!t) {
			err = ERR_UNEXPECTED_EOF;
			goto free_node;
		}
		if (t->type == OP_ASSIGN) {
			err = parseExpr(&item->a);
			if (err)
				goto free_node;
			item->a->parent = item;
			t = nextToken();
			if (!t) {
				err = ERR_UNEXPECTED_EOF;
				goto free_node;
			}
		}
		
		if (t->type == SYM_SEMICOLON) {
			return 0;
		} else if (t->type != SYM_COMMA) {
			logErrorAtLine(t->fp, "Expected ';', '=' or ',' in variable declaration");
			err = ERR_UNEXPECTED_TOKEN;
			goto free_node;
		}
	}

free_node:
	//freeNode(n); Handled by parseFnOrVar
	return err;
}

static int parseDeclFnParam(struct ASTNode **newNode) {
	int err = 0;
	struct Token *t = nextToken();
	if (!t) {
		return ERR_UNEXPECTED_EOF;
	}

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = DECL_FN_PARAM;
	n->fp = t->fp;

	if (t->type == KEYW_REF) {
		n->flags |= VAR_REF;
		t = nextToken();
		if (!t) {
			err = ERR_UNEXPECTED_EOF;
			goto free_node;
		}
	}

	n->val.type = keywToVt(t->type);
	if (!n->val.type || n->val.type == VT_VOID) {
		logErrorAtLine(t->fp, "Invalid function parameter type: %s", t->text);
		err = ERR_UNEXPECTED_TOKEN;
		goto free_node;
	}

	err = nextTokenExpect(&t, IDENTIFIER);
	if (err)
		goto free_node;

	n->val.s = t->text;
	n->val.sLen = t->textLen;

	*newNode = n;

	return err;

free_node:
	freeNode(n);
	return err;
}
static int parseDeclFn(struct ASTNode *n) {
	int err = 0;
	struct Token *t = nextToken();
	if (!t)
		return ERR_UNEXPECTED_EOF;

	n->type = DECL_FN_IMPL;
	n->fp = t->fp;

	if (t->type == KEYW_INLINE) {
		n->flags |= FN_INLINE;
		t = nextToken();
		if (!t) {
			err = ERR_UNEXPECTED_EOF;
			goto free_node;
		}
	}

	if (t->type == KEYW_ASYNC) {
		n->val.type = VT_INT; /* asyncs return an int for coroutine id */
		n->flags |= FN_ASYNC;
	} else {
		n->val.type = keywToVt(t->type);
		if (!n->val.type) {
			logErrorAtLine(t->fp, "Invalid function return type: %s", t->text);
			err = ERR_UNEXPECTED_TOKEN;
			goto free_node;
		}
	}

	err = nextTokenExpect(&t, IDENTIFIER);
	if (err)
		goto free_node;

	n->val.s = t->text;
	n->val.sLen = t->textLen;

	/* '(' */
	err = nextTokenExpect(&t, SYM_PARENTH_OPEN);
	if (err)
		goto free_node;

	/* Parameter list */
	bool endParenth = false;
	bool needsComma = false;
	for (int i = 0; !endParenth; i++) {
		t = nextToken();
		if (!t) {
			err = ERR_UNEXPECTED_EOF;
			goto free_node;
		}

		struct ASTNode *paramNode = NULL;
		switch (t->type) {
		case SYM_PARENTH_CLOSE:
			endParenth = true;
			break;
		case KEYW_VOID:
			if (i) {
				logErrorAtLine(t->fp, "Invalid void defined in function parameters");
				err = ERR_UNEXPECTED_TOKEN;
				goto free_node;
			}
			err = nextTokenExpect(NULL, SYM_PARENTH_CLOSE);
			if (err)
				goto free_node;

			endParenth = true;
			break;
		case SYM_COMMA:
			if (!needsComma) {
				logErrorAtLine(t->fp, "Erroneous comma in function parameters");
				err = ERR_UNEXPECTED_TOKEN;
				goto free_node;
			}
			needsComma = false;
			break;
		default:
			if (needsComma) {
				logErrorAtLine(t->fp, "Missing comma in function parameters");
				err = ERR_UNEXPECTED_TOKEN;
				goto free_node;
			}
			tokenBacktrack(1);
			err = parseDeclFnParam(&paramNode);
			if (err)
				goto free_node;
			err = addToNodeList(n, paramNode);
			if (err)
				goto free_node;
			needsComma = true;
			break;
		}
	}

	if (n->flags & VAR_FN_EXTERN) { /* externs dont have a body and cannot be async */
		err = nextTokenExpect(&t, SYM_SEMICOLON);
		if (err)
			goto free_node;
		return 0;
	}

	/* Parse statement block */
	err = parseStmtBlock(&n->a);
	if (err) {
		goto free_node;
	}
	n->a->parent = n;

	return err;

free_node:
	//freeNode(n); Handled by parseFnOrVar
	return err;
}
static int parseDeclImport(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, KEYW_IMPORT);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = DECL_IMPORT;
	n->fp = t->fp;

	err = nextTokenExpect(&t, LIT_STRING);
	if (err)
		goto free_node;
	n->val.s = t->text;
	n->val.sLen = t->textLen;

	err = nextTokenExpect(&t, SYM_SEMICOLON);
	if (err)
		goto free_node;

	err = doFile(n->val.s, n);
	*newNode = n;

	return err;

free_node:
	freeNode(n);
	return err;
}
static int parseDeclType(struct ASTNode **newNode) {
	struct Token *t;
	int err = nextTokenExpect(&t, KEYW_TYPE);
	if (err)
		return err;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;
	n->type = DECL_TYPE;
	n->fp = t->fp;
	err = nextTokenExpect(&t, LIT_STRING);
	if (err)
		goto free_node;
	n->val.s = t->text;
	n->val.sLen = t->textLen;

	err = nextTokenExpect(&t, SYM_SEMICOLON);
	if (err)
		goto free_node;

	*newNode = n;

	return err;

free_node:
	freeNode(n);
	return err;
}

static int parseDeclFnOrVar(struct ASTNode **newNode) {
	int bt = 1;
	int err;
	struct Token *t = nextToken();
	if (!t)
		return ERR_UNEXPECTED_EOF;

	struct ASTNode *n = allocNode();
	if (!n)
		return ERR_NO_MEM;

	*newNode = n;

	if (t->type == KEYW_EXTERN) {
		err = parseExtern(n, &t);
		if (err)
			goto free_n;
	}
	if (t->type == KEYW_INLINE || t->type == KEYW_ASYNC) {
		err = tokenBacktrack(bt);
		if (err)
			return err;
		return parseDeclFn(n);
	}

	if (t->type == KEYW_CONST) {
		err = tokenBacktrack(bt);
		if (err)
			return err;
		return parseDeclVar(n);
	}

	err = nextTokenExpect(&t, IDENTIFIER);
	if (err)
		return err;
	bt++;

	t = nextToken();
	if (!t)
		return ERR_UNEXPECTED_EOF;
	bt++;

	err = tokenBacktrack(bt);
	if (err)
		return err;

	if (t->type == OP_ASSIGN || t->type == SYM_SEMICOLON) {
		return parseDeclVar(n);
	} else {
		return parseDeclFn(n);
	}
free_n:
	freeNode(n);
	return err;
}

int parse(struct ASTNode *root) {
	while (haveMoreTokens()) {
		/* Do top-level declarations */
		struct Token *t = nextToken();
		if (!t) {
			return ERR_UNEXPECTED_EOF;
		}

		tokenBacktrack(1);

		struct ASTNode *newNode = NULL;
		int err;
		switch (t->type) {
		case KEYW_TYPE:
			err = parseDeclType(&newNode);
			if (err)
				return err;
			break;
		case KEYW_IMPORT:
			err = parseDeclImport(&newNode);
			if (err)
				return err;
			break;

		case KEYW_INLINE:
		case KEYW_VOID:
		case KEYW_ASYNC:
		case KEYW_CONST:
		case KEYW_INT:
		case KEYW_FLOAT:
		case KEYW_STRING:
		case KEYW_ENTITY:
		case KEYW_EXTERN:
			err = parseDeclFnOrVar(&newNode);
			if (err)
				return err;
			break;
		default:
			logErrorAtLine(t->fp, "Unexpected top-level token %s", t->text);
			return ERR_UNEXPECTED_TOKEN;
		}
		if (newNode) {
			if (addToNodeList(root, newNode))
				return ERR_NO_MEM;
		}
	}

	return 0;
}

void printASTNode(struct ASTNode *node) {
	printf("(%d %d ", node->type, node->val.type);
	if (node->type == DECL_FN_IMPL || node->type == EXPR_STRING) {
		printf("= %s ", node->val.s);
	}
	printf("| ");
	if (node->a) {
		printf("a: ");
		printASTNode(node->a);
	}
	if (node->b) {
		printf("b: ");
		printASTNode(node->b);
	}
	if (node->c) {
		printf("c: ");
		printASTNode(node->c);
	}
	if (node->d) {
		printf("d: ");
		printASTNode(node->d);
	}
	if (node->list) {
		printf("list: ");
		for (int i = 0; i < node->listLen; i++) {
			printASTNode(node->list[i]);
		}
	}
	printf(")");
}
