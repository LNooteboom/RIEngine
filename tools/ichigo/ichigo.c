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

#define MAX_IMPORTS 64
static const char *imports[MAX_IMPORTS];
static int nImports;

const char *curFileName;

static char inFileBuf[256];
static char *inFileBaseName;
static char *inDir;

#ifndef TEST

static FILE *inFile;
static FILE *outFile;

#define DBG_PRINT_TOKENS	1
#define DBG_PRINT_AST		2
#define DBG_PRINT_ASM		4
static int debugFlags;

int getChar(void) {
	return fgetc(inFile);
}

void ungetChar(int c) {
	ungetc(c, inFile);
}

int writeBytes(const void *data, int dataSize) {
	if (!dataSize)
		return 0;
	assert(dataSize > 0);
	assert(data);
	size_t s = fwrite(data, dataSize, 1, outFile);
	if (s != 1) {
		perror("Writing output failed");
		return ERR_WRITE_FAIL;
	}
	return 0;
}


int doFile(const char *name, struct ASTNode *root) {
	curFileName = name;
	/* See if the file has already been imported */
	for (int i = 0; i < nImports; i++) {
		if (!strcmp(name, imports[i])) {
			return 0;
		}
	}
	/* Add it to the imports list */
	if (nImports == MAX_IMPORTS) {
		fprintf(ERR_OUT, "Import limit reached\n");
		return ERR_IMPORT_LIMIT;
	}
	imports[nImports] = name;
	nImports++;

	char buf[256];
	if (inDir) {
		snprintf(buf, 256, "%s/%s.i", inDir, name);
	} else {
		snprintf(buf, 256, "%s.i", name);
	}

	inFile = fopen(buf, "rb");
	if (!inFile) {
		fprintf(ERR_OUT, "Could not find file: %s\n", buf);
		return ERR_NO_FILE;
	}

	struct LexState oldState;
	memcpy(&oldState, &lexState, sizeof(oldState));

	int err = tokenize();
	if (err)
		return err;

	fclose(inFile);
	inFile = NULL;

	if (debugFlags & DBG_PRINT_TOKENS) {
		printf("Tokens in file %s:\n", name);
		printTokenList();
		printf("\n\n");
	}

	err = parse(root);
	if (err)
		return err;

	/* This leaks memory */
	memcpy(&lexState, &oldState, sizeof(oldState));

	return 0;
}


int main(int argc, char **argv) {
	const char *inFileName;
	const char *outFileName;
	if (argc != 3) {
		//printf("Usage: %s <input file> <output file>", argv[0]);
		//return -1;
		inFileName = "dat\\dvm\\ascii.i";
		outFileName = "dat\\dvm\\ascii.ich";
	} else {
		inFileName = argv[1];
		outFileName = argv[2];
	}

	strncpy(inFileBuf, inFileName, 256);
	char *inFileBase = strrchr(inFileBuf, '/');
	if (inFileBase) {
		inDir = inFileBuf;
		*inFileBase = 0;
		inFileBaseName = inFileBase + 1;
		inFileName = inFileBaseName;
	} else {
		inFileName = inFileBuf;
		inFileBaseName = inFileBuf;
	}
	inFileBaseName[strlen(inFileBaseName) - 2] = 0;

	debugFlags = DBG_PRINT_ASM;

	/* Create root astnode */
	struct ASTNode *rootNode = allocNode();
	if (!rootNode)
		return ERR_NO_MEM;
	rootNode->type = AST_ROOT;

	int err = doFile(inFileName, rootNode);
	if (err)
		return err;

	if (debugFlags & DBG_PRINT_AST) {
		printf("AST:\n");
		printASTNode(rootNode);
		printf("\n\n");
	}

	struct InstrGen *ig;
	err = semVal(rootNode, &ig);
	if (err)
		return err;

	if (debugFlags & DBG_PRINT_ASM) {
		printf("Instructions:\n");
		printInstrs(ig);
		printf("\n\n");
	}

	outFile = fopen(outFileName, "wb");
	if (!outFile) {
		perror("Failed to open output");
		return ERR_WRITE_FAIL;
	}

	err = outputFile(ig);
	if (err)
		return err;
	
	fclose(outFile);

	return 0;
}

#else

#include "test.c"

#endif