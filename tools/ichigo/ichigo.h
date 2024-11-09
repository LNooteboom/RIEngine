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

#ifndef ICHIGO_H
#define ICHIGO_H

#define ERR_OUT	stdout

//#define TEST

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

#define TOKEN_MAXLEN 255

#define logErrorAtLine(fp, msg, ...) fprintf(ERR_OUT, "[ERROR] %s:%d: "msg"\n", (fp).file, (fp).line, ##__VA_ARGS__)
#define logWarnAtLine(fp, msg, ...) fprintf(ERR_OUT, "[WARN] %s:%d: "msg"\n", (fp).file, (fp).line,  ##__VA_ARGS__)

/* Errors*/
/* Generic */
#define ERR_NO_MEM				-1
#define ERR_NO_FILE				-2
#define ERR_IMPORT_LIMIT		-3
#define ERR_INTERNAL			-4
#define ERR_UNIMPLEMENTED		-5
#define ERR_LIMIT				-6
/* Lexer*/
#define ERR_INVALID_TOKEN		-20
#define ERR_UNKNOWN_TOKEN		-21
/* Parser*/
#define ERR_UNEXPECTED_TOKEN	-40
#define ERR_UNEXPECTED_EOF		-41
#define ERR_INVALID_INT			-42
#define ERR_INVALID_FLOAT		-43
#define ERR_INVALID_EXPR		-44
/* Semantic validator */
#define ERR_INVALID_VAR_DECL	-60
#define ERR_REDEFINED_VAR		-61
#define ERR_REDEFINED_FUNC		-62
#define ERR_DUPLICATE_PARAM		-63
#define ERR_NOT_A_FUNC			-64
#define ERR_NOT_A_VAR			-65
#define ERR_UNDEFINED_IDENT		-66
#define ERR_VOID_EXPR			-67
#define ERR_INCORRECT_N_PARAMS	-68
#define ERR_NO_CONVERT			-69
#define ERR_INVALID_BREAK		-70
#define ERR_INVALID_CONTINUE	-71
#define ERR_NOT_MODIFIABLE		-72
#define ERR_RETURN_VAL			-73
#define ERR_CONST_WRITE			-74
#define ERR_INVALID_LABEL		-75
#define ERR_INT_REQUIRED		-76
/* Instr gen */
#define ERR_NO_REGISTERS		-100
#define ERR_BYTECODE			-101
/* Output */
#define ERR_WRITE_FAIL			-102

/*
* Lexer
*/

#define KEYW_VAR_START KEYW_CONST
#define KEYW_VAR_END KEYW_STRING
enum TokenType {
	/* Keywords */
	KEYW_IF,
	KEYW_ELSE,
	KEYW_WHILE,
	KEYW_FOR,
	KEYW_RETURN,
	KEYW_BREAK,
	KEYW_CONTINUE,
	KEYW_SWITCH,
	KEYW_CASE,
	KEYW_DEFAULT,
	KEYW_GOTO,
	KEYW_LOOP,

	KEYW_INLINE,
	KEYW_CONST,
	KEYW_VOID,
	KEYW_INT,
	KEYW_FLOAT,
	KEYW_STRING,

	KEYW_ENTITY,
	KEYW_TYPE,
	KEYW_EXTERN,
	KEYW_IMPORT,
	KEYW_ASYNC,
	KEYW_REF,

	/* Symbols */
	SYM_CURLY_OPEN,
	SYM_CURLY_CLOSE,
	SYM_PARENTH_OPEN,
	SYM_PARENTH_CLOSE,
	SYM_BRACE_OPEN,
	SYM_BRACE_CLOSE,
	SYM_COMMA,
	SYM_SEMICOLON,
	SYM_COLON,

	/* Operators */
	OP_ASSIGN,
	OP_PLUS,
	OP_PLUSEQ,
	OP_MIN,
	OP_MINEQ,
	OP_MULT,
	OP_MULTEQ,
	OP_DIV,
	OP_DIVEQ,
	OP_MOD,
	OP_MODEQ,
	OP_INCR,
	OP_DECR,

	/* Compare operators */
	OP_EQ,
	OP_NEQ,
	OP_LESS,
	OP_LESSEQ,
	OP_GTR,
	OP_GTREQ,

	/* Boolean operators */
	OP_OR,
	OP_AND,
	OP_NOT,

	/* Bitwise operators */
	OP_BW_OR,
	OP_BW_OREQ,
	OP_BW_AND,
	OP_BW_ANDEQ,
	OP_BW_XOR,
	OP_BW_XOREQ,
	OP_BW_INV,
	OP_BW_SHL,
	OP_BW_SHLEQ,
	OP_BW_SHR,
	OP_BW_SHREQ,

	OP_TERNARY,

	/* Literals */
	LIT_INT,
	LIT_FLOAT,
	LIT_STRING,

	IDENTIFIER,

	N_TOKENS
};

struct FilePos {
	const char *file;
	int line;
};

struct Token {
	enum TokenType type;
	int textLen;
	char text[TOKEN_MAXLEN + 1];

	struct FilePos fp;
};

struct LexState {
	struct Token *tokenList;
	int nTokens;
	int tokenIndex;
};

extern struct LexState lexState;

void printTokenList(void);
int tokenize(void);

struct Token *nextToken(void);
int nextTokenExpect(struct Token **token, enum TokenType expected);
int tokenBacktrack(int amt);
bool haveMoreTokens(void);


/*
* Parser
*/

enum ASTNodeType {
	AST_ROOT,

	/* Top level declarations */
	DECL_TYPE,
	DECL_IMPORT,
	DECL_FN_IMPL,
	DECL_FN_PARAM,
	DECL_VAR_LIST,
	DECL_VAR,

	/* Statements */
	STMT_BLOCK,
	STMT_IF,
	STMT_WHILE,
	STMT_FOR,
	STMT_RETURN,
	STMT_BREAK,
	STMT_CONTINUE,
	STMT_SWITCH,
	STMT_GOTO,
	STMT_LOOP,

	LBL_CASE,
	LBL_DEFAULT,
	LBL_GOTO,

	/* Expressions, ordered from highest to lowest precedence */
	/* primary */
	EXPR_IDENT,
	EXPR_INT,
	EXPR_FLOAT,
	EXPR_STRING,
	EXPR_ARRAY, /* array literal: [x, y, z] */

	/* postfix */
	EXPR_FN_CALL,
	EXPR_INLINE_FN, /* Inline fn call */
	EXPR_POSTINCR,
	EXPR_POSTDECR,
	EXPR_ARRAY_IDX,

	/* prefix */
	EXPR_PREINCR,
	EXPR_PREDECR,
	EXPR_PLUS, /* unary */
	EXPR_MIN,
	EXPR_NOT,
	EXPR_BW_INV,

	/* mult */
	EXPR_MULT,
	EXPR_DIV,
	EXPR_MOD,

	/* add */
	EXPR_ADD,
	EXPR_SUB,

	/* shift */
	EXPR_BW_SHL,
	EXPR_BW_SHR,

	/* relat */
	EXPR_LESS,
	EXPR_LESSEQ,
	EXPR_GTR,
	EXPR_GTREQ,

	/* equal */
	EXPR_EQ,
	EXPR_NEQ,

	/* bwAnd */
	EXPR_BW_AND,

	/* bwXor */
	EXPR_BW_XOR,

	/* bwOr */
	EXPR_BW_OR,

	/* and */
	EXPR_AND,

	/* or */
	EXPR_OR,

	/* tern | a = condition, b = if_true, c = if_false */
	EXPR_TERNARY,

	/* assign */
	EXPR_ASSIGN,
	EXPR_ADDEQ,
	EXPR_SUBEQ,
	EXPR_MULTEQ,
	EXPR_DIVEQ,
	EXPR_MODEQ,
	EXPR_BW_OREQ,
	EXPR_BW_ANDEQ,
	EXPR_BW_XOREQ,
	EXPR_BW_SHLEQ,
	EXPR_BW_SHREQ,
};

enum ValType {
	VT_INVALID,
	VT_VOID,
	VT_INT,
	VT_FLOAT,
	VT_STRING,
	VT_ENTITY,

	/* Internal */
	VT_REG,
	VT_REG_REF,
	VT_LBL_IDX,

	N_VTS
};

/* Flags */
#define COND_INVERT		1

#define VAR_CONST		1
#define VAR_REF			2
#define VAR_WRITTEN		4
#define VAR_READ		16
#define VAR_ARRAY		32

#define FN_INLINE		1
#define FN_BUILTIN		2
#define FN_ASYNC		4

#define VAR_FN_EXTERN	8

struct Val {
	enum ValType type;
	union {
		float f;
		int i;
		const char *s;
		int64_t e;
	};
	int sLen;
};

struct ASTNode {
	enum ASTNodeType type;

	struct ASTNode *parent;
	int parentListIdx;

	struct ASTNode *a;
	struct ASTNode *b;
	struct ASTNode *c;
	struct ASTNode *d;

	struct Val val;

	struct ASTNode **list;
	int listLen;

	int flags;

	struct FilePos fp;

	/* Stuff used in semval */
	struct ASTNode *identRef; /* if type == EXPR_IDENT, points to the declaration of the referenced func/var, points to parent switch in case/default lbls */
	int reg; /* for DECL_VAR, describes which reg this local is in. for switch lbl, label idx */
};

struct ASTNode *allocNode(void);
void freeNode(struct ASTNode *node);
struct ASTNode *copyNode(struct ASTNode *node); /* Deep copy */
int addToNodeList(struct ASTNode *parent, struct ASTNode *child);

int parse(struct ASTNode *rootNode);
void printASTNode(struct ASTNode *node);

/*
* Semantic validator
*/

struct InstrArg {
	struct Val val;
};
struct Instr {
	uint16_t type;
	int nArgs;
	struct InstrArg args[4];
	struct InstrArg *args2;

	int byteLen;
	int byteOffset;
};
#define LABEL_MAXLEN 63
struct Label {
	char name[LABEL_MAXLEN];
	int nameLen;

	int offset;
	int byteOffset;
};
struct Fn {
	const char *name;
	int nameLen;

	int nParams;
	char *params;

	int nInstrs;
	struct Instr *instrs;

	int nLabels;
	struct Label *labels;
};
struct Global {
	const char *name;
	int nameLen;

	bool isConst;

	struct Val val;
};
struct Import {
	const char *name;
	int nameLen;
};

struct InstrGen {
	int nImports;
	struct Import *imports;

	int nGlobals;
	struct Global *globals;

	int nFns;
	struct Fn *fns;
	struct Fn *curFn;

	bool wasRet;
};

int semVal(struct ASTNode *root, struct InstrGen **igState);

int newInstrGen(struct InstrGen **state);

int pushImport(struct InstrGen *state, struct Import *import);
int pushGlobal(struct InstrGen *state, struct Global *global);

int pushFnStart(struct InstrGen *state, struct Fn *fn);
int pushFnEnd(struct InstrGen *state);

int pushInstr(struct InstrGen *state, struct Instr *instr);
int addInstrArg(struct Instr *instr, struct InstrArg *arg);

/* Setting offset gets delayed */
int pushLabel(struct InstrGen *state, const char *description, int *labelIdx);
int setLabelOffset(struct InstrGen *state, int labelIdx);

/* Helper functions */
int pushInstrJmp(struct InstrGen *state, uint16_t instr, int labelIdx, struct InstrArg *condition);
int pushInstrMovReg(struct InstrGen *state, uint16_t instr, int destReg, int srcReg);

void printInstrs(struct InstrGen *state);

int outputFile(struct InstrGen *ig);

/*
* Main
*/

extern const char *curFileName;

int getChar(void);
void ungetChar(int c);

int writeBytes(const void *data, int dataSize);

int doFile(const char *name, struct ASTNode *root);

#endif
