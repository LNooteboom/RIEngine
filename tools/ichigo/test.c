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

#include "instr.h"
#include <assert.h>
/* #include <stdio.h> */
/* #include <stdbool.h> */

static const char *inputStr;
static int inputIndex;

int getChar(void) {
	int c = inputStr[inputIndex++];
	if (!c)
		return EOF;
	return c;
}

void ungetChar(int c) {
	(void) c;
	inputIndex--;
}

int writeBytes(const void *data, int dataSize) {
	(void) data;
	(void) dataSize;
	assert(false);
	return 0;
}

int doFile(const char *name, struct ASTNode *root) {
	(void) name;
	(void) root;
	assert(false);
	return ERR_UNIMPLEMENTED;
}

int main(void) {
	/* TEST LEXER */
	curFileName = "TEST";
	inputStr = "import a;++(42*= //comment\n/*comment2*/3.0f\"\\\\str\\\"\\n\"";
	inputIndex = 0;

	assert(!tokenize());
	struct Token *t;
	assert(!nextTokenExpect(&t, KEYW_IMPORT));
	assert(!strcmp(t->text, "import"));
	assert(t->textLen == 6);
	assert(!nextTokenExpect(&t, IDENTIFIER));
	assert(!nextTokenExpect(NULL, SYM_SEMICOLON));
	assert(!nextTokenExpect(&t, OP_INCR));
	assert(!nextTokenExpect(&t, SYM_PARENTH_OPEN));
	assert(!nextTokenExpect(&t, LIT_INT));
	assert(!nextTokenExpect(&t, OP_MULTEQ));
	assert(!nextTokenExpect(&t, LIT_FLOAT));
	assert(!strcmp(t->text, "3.0f"));
	assert(!nextTokenExpect(&t, LIT_STRING));
	assert(!strcmp(t->text, "\\str\"\n"));

	/* TEST PARSER */

	inputStr =
		"type \"danmaku\";\n\
const int c = 3;\n\
entity foo(ref float bar, string baz) {\n\
	if (bar > 3.0f) {\n\
		return -1.0f;\n\
	} else {\n\
		bar -= ++bar * (2 << 3) | 3;\n\
	}\n\
	for (int a = 0; a < 10; a++)\n\
		continue;\n\
	return 1 + 2 + 3 * 4;\n\
}";
	inputIndex = 0;

	assert(!tokenize());
	struct ASTNode *rootNode = allocNode();
	assert(rootNode);
	rootNode->type = AST_ROOT;
	assert(!parse(rootNode));
	assert(rootNode->list);

	/* type "danmaku"; */
	struct ASTNode *n = rootNode->list[0];
	assert(n->type == DECL_TYPE);
	assert(n->a);
	assert(!strcmp(n->a->val.s, "danmaku"));

	/* const int c = 3; */
	n = rootNode->list[1];
	assert(n->type == DECL_VAR);
	assert(n->val.type == VT_INT);
	assert(!strcmp(n->val.s, "c"));
	assert(n->flags & VAR_CONST);
	assert(n->a->type == EXPR_INT);
	assert(n->a->val.i == 3);

	/* entity foo(ref float bar, string baz) { */
	struct ASTNode *func = rootNode->list[2];
	assert(func->type == DECL_FN_IMPL);
	assert(func->val.type == VT_ENTITY);
	assert(func->list[0]->type == DECL_FN_PARAM);
	assert(func->list[0]->val.type == VT_FLOAT);
	assert(func->list[0]->flags & VAR_REF);
	assert(!strcmp(func->list[0]->val.s, "bar"));
	assert(func->list[1]->val.type == VT_STRING);
	assert(!(func->list[1]->flags & VAR_REF));
	assert(!strcmp(func->list[1]->val.s, "baz"));
	assert(func->a->type == STMT_BLOCK);

	func = func->a;

	/* if (bar > 3.0f) { */
	n = func->list[0];
	assert(n->type == STMT_IF);
	assert(n->a->type == EXPR_GTR);
	assert(n->a->a->type == EXPR_IDENT);
	assert(!strcmp(n->a->a->val.s, "bar"));
	assert(n->a->b->type == EXPR_FLOAT);
	assert(n->a->b->val.f == 3.0f);

	assert(n->b->type == STMT_BLOCK);

	/* return -1.0f */
	n = n->b->list[0];
	assert(n->type == STMT_RETURN);
	assert(n->a->type == EXPR_MIN); /* EXPR_MIN is unary minus */
	assert(n->a->a->type == EXPR_FLOAT);
	assert(n->a->a->val.f = 1.0f);

	n = n->parent->parent;
	assert(n && n->type == STMT_IF); /* Test parent ptr */

	/* } else { */
	/* bar -= ++bar * (2 << 3) | 3; */
	n = n->c->list[0];
	assert(n->type == EXPR_SUBEQ);
	assert(n->a->type == EXPR_IDENT);
	assert(n->b->type == EXPR_BW_OR); /* '|' has lower precedence than '*' */
	assert(n->b->b->type == EXPR_INT); /* right side of '|' */
	assert(n->b->b->val.i == 3);
	assert(n->b->a->type == EXPR_MULT);
	assert(n->b->a->a->type == EXPR_PREINCR);
	assert(n->b->a->a->a->type == EXPR_IDENT);
	assert(n->b->a->b->type == EXPR_BW_SHL);

	/* for (int a = 0; a < 10; a++) */
	n = func->list[1];
	assert(n->type == STMT_FOR);
	assert(n->a->type == DECL_VAR);
	assert(n->b->type == EXPR_LESS);
	assert(n->c->type == EXPR_POSTINCR);
	/* continue; */
	assert(n->d->type == STMT_CONTINUE);

	/* return 1 + 2 + 3 * 4; */
	/* equivalent to: (1 + 2) + (3 * 4) */
	n = func->list[2];
	assert(n->type == STMT_RETURN);
	assert(n->a->type == EXPR_ADD);
	assert(n->a->a->type == EXPR_ADD);
	assert(n->a->a->a->type == EXPR_INT && n->a->a->a->val.i == 1);
	assert(n->a->a->b->type == EXPR_INT && n->a->a->b->val.i == 2);
	assert(n->a->b->type == EXPR_MULT);
	assert(n->a->b->a->type == EXPR_INT && n->a->b->a->val.i == 3);
	assert(n->a->b->b->type == EXPR_INT && n->a->b->b->val.i == 4);

	assert(func->listLen == 3);
	assert(rootNode->listLen == 3);

	inputStr = "int fib(int n) { if (n <= 1) return 1; return fib(n - 2) + fib(n - 1); }";
	inputIndex = 0;
	assert(!tokenize());
	rootNode = allocNode();
	assert(rootNode);
	rootNode->type = AST_ROOT;
	assert(!parse(rootNode));

	struct InstrGen *ig;
	assert(!semVal(rootNode, &ig));

	printInstrs(ig);
	assert(ig->nFns == 1);
	struct Fn *fn = &ig->fns[0];
	assert(!strcmp(fn->name, "fib"));
	assert(fn->nInstrs == 11);
	assert(fn->instrs[0].type == INSTR_LEI);
	assert(fn->instrs[0].nArgs == 3);
	assert(fn->instrs[0].args[0].val.type == VT_REG && fn->instrs[0].args[0].val.i == 0);
	assert(fn->instrs[0].args[1].val.type == VT_REG && fn->instrs[0].args[1].val.i == -2);
	assert(fn->instrs[0].args[2].val.type == VT_INT && fn->instrs[0].args[2].val.i == 1);
	assert(fn->instrs[1].type == INSTR_JZ);
	assert(fn->instrs[1].args[1].val.type == VT_LBL_IDX && fn->instrs[1].args[1].val.i == 0);
	assert(fn->instrs[2].type == INSTR_MOVI);
	assert(fn->instrs[3].type == INSTR_RET);
	assert(fn->instrs[4].type == INSTR_SUBI);
	assert(fn->instrs[5].type == INSTR_CALL);
	assert(fn->instrs[6].type == INSTR_SUBI);
	assert(fn->instrs[7].type == INSTR_CALL);
	assert(fn->instrs[8].type == INSTR_ADDI);
	assert(fn->instrs[9].type == INSTR_MOVI);
	assert(fn->instrs[10].type == INSTR_RET);

	return 0;
}
