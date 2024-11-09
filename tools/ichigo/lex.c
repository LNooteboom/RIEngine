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

static const char *tokenText[N_TOKENS] = {
	/* Keywords */
	[KEYW_IF] = "if",
	[KEYW_ELSE] = "else",
	[KEYW_WHILE] = "while",
	[KEYW_FOR] = "for",
	[KEYW_RETURN] = "return",
	[KEYW_BREAK] = "break",
	[KEYW_CONTINUE] = "continue",
	[KEYW_SWITCH] = "switch",
	[KEYW_CASE] = "case",
	[KEYW_DEFAULT] = "default",
	[KEYW_GOTO] = "goto",
	[KEYW_LOOP] = "loop",

	[KEYW_INLINE] = "inline",
	[KEYW_CONST] = "const",
	[KEYW_VOID] = "void",
	[KEYW_INT] = "int",
	[KEYW_FLOAT] = "float",
	[KEYW_STRING] = "string",
	[KEYW_ENTITY] = "entity",

	[KEYW_TYPE] = "archetype",
	[KEYW_EXTERN] = "extern",
	[KEYW_IMPORT] = "import",
	[KEYW_ASYNC] = "async",
	[KEYW_REF] = "ref",

	/* Symbols */
	[SYM_CURLY_OPEN] = "{",
	[SYM_CURLY_CLOSE] = "}",
	[SYM_PARENTH_OPEN] = "(",
	[SYM_PARENTH_CLOSE] = ")",
	[SYM_BRACE_OPEN] = "[",
	[SYM_BRACE_CLOSE] = "]",
	[SYM_COMMA] = ",",
	[SYM_SEMICOLON] = ";",
	[SYM_COLON] = ":",

	/* Operators */
	[OP_ASSIGN] = "=",
	[OP_PLUS] = "+",
	[OP_PLUSEQ] = "+=",
	[OP_MIN] = "-",
	[OP_MINEQ] = "-=",
	[OP_MULT] = "*",
	[OP_MULTEQ] = "*=",
	[OP_DIV] = "/",
	[OP_DIVEQ] = "/=",
	[OP_MOD] = "%",
	[OP_MODEQ] = "%=",
	[OP_INCR] = "++",
	[OP_DECR] = "--",

	/* Compare operators */
	[OP_EQ] = "==",
	[OP_NEQ] = "!=",
	[OP_LESS] = "<",
	[OP_LESSEQ] = "<=",
	[OP_GTR] = ">",
	[OP_GTREQ] = ">=",

	/* Boolean operators */
	[OP_OR] = "||",
	[OP_AND] = "&&",
	[OP_NOT] = "!",

	/* Bitwise operators */
	[OP_BW_OR] = "|",
	[OP_BW_OREQ] = "|=",
	[OP_BW_AND] = "&",
	[OP_BW_ANDEQ] = "&=",
	[OP_BW_XOR] = "^",
	[OP_BW_XOREQ] = "^=",
	[OP_BW_INV] = "~",
	[OP_BW_SHL] = "<<",
	[OP_BW_SHLEQ] = "<<=",
	[OP_BW_SHR] = ">>",
	[OP_BW_SHREQ] = ">>=",

	[OP_TERNARY] = "?",

	/* For error handling purposes only */
	[LIT_INT] = "INTEGER_LITERAL",
	[LIT_FLOAT] = "FLOAT_LITERAL",
	[LIT_STRING] = "STRING_LITERAL",
	[IDENTIFIER] = "IDENTIFIER"
};

struct LexState lexState;

static bool identChar(int c) {
	return c == '_' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int readToken(char *buffer, int *nRead, int *lineNumber, int bufLen) {
	bool alnum = false;
	int string = 0;
	for (int i = 0; i < bufLen; i++) {
		int c = getChar();
		if (!i) {
			while (c != EOF && isspace(c)) { /* Skip preceding whitespace */
				if (c == '\n') {
					*lineNumber += 1;
				}
				c = getChar();
			}

			if (c == '\"' || c == '\'') {
				string = c;
			} else {
				alnum = identChar(c);
			}
		} else if (isdigit(buffer[0]) && c == '.') {
			/* Include periods if the token is a number */
			buffer[i] = c;
			continue;
		}

		if (c == EOF) {
			*nRead = i;
			return 1;
		}
		if (!string && (c == ';' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == ':')) {
			buffer[i] = c;
			if (i) {
				ungetChar(c);
				*nRead = i;
			} else {
				*nRead = 1;
			}
			return 0;
		}

		if (i) {
			if (string) {
				if (buffer[i - 1] != '\\' && c == string) {
					buffer[i] = c;
					*nRead = i + 1;
					return 0;
				}
			} else if (isspace(c) || identChar(c) != alnum) {
				*nRead = i;
				ungetChar(c);
				return 0;
			}
		}

		buffer[i] = c;
	}
	struct FilePos fp = { curFileName, *lineNumber };
	logErrorAtLine(fp, "Max token length exceeded");
	return -1;
}
static int readUntil(int *lineNumber, const char *until) {
	int c, cprev = 0;
	while (true) {
		c = getChar();
		if (c == '\n') {
			*lineNumber += 1;
		}
		if (c == EOF) {
			return 1;
		}
		if (until[1]) {
			if (c == until[1] && cprev == until[0]) {
				return 0;
			}
		} else if (c == until[0]) {
			return 0;
		}
		cprev = c;
	}
}

/* Returns 0 if not match, 1 if match and -1 if error */
static int tokenIsMatch(enum TokenType type, const char *tText, int tLen, int lineNumber) {
	struct FilePos fp = { curFileName, lineNumber };
	if (type == LIT_INT) {
		if (!isdigit(tText[0]))
			return 0;
		/* Validate if this is a good int token */
		for (int i = 1; i < tLen; i++) {
			if (!isdigit(tText[i]) && !(tLen > 2 && tText[1] == 'x' && tText[0] == '0')) {
				return 0;
			}
		}
		return 1;
	}
	if (type == LIT_FLOAT) {
		if (!isdigit(tText[0]))
			return 0;
		for (int i = 1; i < tLen; i++) {
			if (!isdigit(tText[i]) && tText[i] != '.' && !(i == tLen - 1 && tText[i] == 'f')) {
				logErrorAtLine(fp, "Invalid number literal: %s", tText);
				return ERR_INVALID_TOKEN;
			}
		}
		return 1;
	}
	if (type == LIT_STRING) {
		if (tText[0] != '\'' && tText[0] != '\"')
			return 0;
		if (tText[tLen - 1] != tText[0]) {
			logErrorAtLine(fp, "Invalid string literal: %s", tText);
			return ERR_INVALID_TOKEN;
		}
		return 1;
	}
	if (type == IDENTIFIER) {
		if (!isalpha(tText[0]))
			return 0;
		for (int i = 0; i < tLen; i++) {
			if (!identChar(tText[i]))
				return 0;
		}
		return 1;
	}

	/* Type is a keyword, symbol or operator */
	for (int i = 0; i < tLen + 1; i++) {
		if (tText[i] != tokenText[type][i]) {
			return 0;
		}
	}
	return 1;
}
static int copyFormattedString(char *dest, const char *src, int srcLen) {
	bool bs = false;
	int destI = 0;
	/* Skip over the quotes */
	for (int i = 1; i < srcLen - 1; i++) {
		char c = src[i];
		if (bs) {
			bs = false;
			switch (src[i]) {
			case '\\':
				c = '\\';
				break;
			case 't':
				c = '\t';
				break;
			case 'r':
				c = '\r';
				break;
			case 'n':
				c = '\n';
				break;
			}
		} else if (c == '\\') {
			bs = true;
			continue;
		}
		dest[destI++] = c;
	}
	return destI;
}
static int newToken(struct Token *token, const char *tText, int tLen, int lineNumber) {
	struct FilePos fp = { curFileName, lineNumber };
	int foundMatch = -1;
	int err;
	for (int i = 0; i < N_TOKENS; i++) {
		err = tokenIsMatch(i, tText, tLen, lineNumber);
		if (err < 0)
			return err;
		if (err) {
			foundMatch = i;
			break;
		}
	}
	if (foundMatch == -1) {
		logErrorAtLine(fp, "Unknown token: %s", tText);
		return ERR_UNKNOWN_TOKEN;
	}
	token->type = foundMatch;
	token->fp.file = curFileName;
	token->fp.line = lineNumber;
	if (foundMatch == LIT_STRING) {
		token->textLen = copyFormattedString(token->text, tText, tLen);
	} else {
		token->textLen = tLen;
		memcpy(token->text, tText, tLen);
	}
	token->text[token->textLen] = 0;
	return 1;
}

int tokenize(void) {
	bool eof = false;
	int lineNumber = 1;
	if (lexState.tokenList) {
		//free(lexState.tokenList);
	}
	lexState.tokenList = NULL;
	lexState.nTokens = 0;
	lexState.tokenIndex = 0;

	while (!eof) {
		char tText[TOKEN_MAXLEN];
		int tLen = 0;
		int err = readToken(tText, &tLen, &lineNumber, TOKEN_MAXLEN - 1);
		tText[tLen] = 0;
		if (err == 1) {
			eof = true;
		} else if (err < 0) {
			return err;
		}
		if (!tLen) {
			continue;
		}

		/* Handle comments */
		if (tLen == 2 && tText[0] == '/') {
			if (tText[1] == '*') {
				if (readUntil(&lineNumber, "*/")) {
					break;
				}
				continue;
			} else if (tText[1] == '/') {
				if (readUntil(&lineNumber, "\n")) {
					break;
				}
				continue;
			}
		}

		struct Token newTok;
		err = newToken(&newTok, tText, tLen, lineNumber);
		if (err < 0)
			return err;
		if (err) {
			lexState.nTokens += 1;
			lexState.tokenList = realloc(lexState.tokenList, lexState.nTokens * sizeof(struct Token));
			if (!lexState.tokenList) {
				fprintf(stderr, "Out of memory!\n");
				return ERR_NO_MEM;
			}
			memcpy(&lexState.tokenList[lexState.nTokens - 1], &newTok, sizeof(struct Token));
		}
	}

	return 0;
}

void printTokenList(void) {
	for (int i = 0; i < lexState.nTokens; i++) {
		struct Token *t = &lexState.tokenList[i];
		printf("(_%s_ \"%.*s\")", tokenText[t->type], t->textLen, t->text);
		if (i && i % 10 == 0) {
			printf("\n");
		}
	}
	printf("\n\n");
}

struct Token *nextToken(void) {
	if (lexState.tokenIndex >= lexState.nTokens) {
		fprintf(stderr, "Unexpected end of file encountered\n");
		return NULL;
	}
	struct Token *t = &lexState.tokenList[lexState.tokenIndex];
	lexState.tokenIndex++;
	return t;
}

int nextTokenExpect(struct Token **token, enum TokenType expected) {
	struct Token *t = nextToken();
	if (!t)
		return ERR_UNEXPECTED_EOF;
	if (t->type != expected) {
		logErrorAtLine(t->fp, "Expected \'%s\', got \'%s\'", tokenText[expected], tokenText[t->type]);
		return ERR_UNEXPECTED_TOKEN;
	}

	if (token) {
		*token = t;
	}
	return 0;
}

int tokenBacktrack(int amt) {
	lexState.tokenIndex -= amt;
	if (lexState.tokenIndex < 0 || lexState.tokenIndex >= lexState.nTokens) {
		fprintf(stderr, "Internal error\n");
		return ERR_INTERNAL;
	}
	return 0;
}

bool haveMoreTokens(void) {
	return lexState.tokenIndex < lexState.nTokens;
}