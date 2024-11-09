#include "ich.h"

int ichigoAddFile(struct IchigoState *state, const char *file) {
	if (strlen(file) >= ICHIGO_PATH_MAX) {
		logError("Ichigo: %s: Filename too long\n", file);
		return -1;
	}
	for (int i = 0; i < state->files.nElements; i++) {
		struct IchigoLoadedFile *other = ichVecAt(&state->files, i);
		if (!strcmp(file, other->path)) {
			return 0; /* File already loaded*/
		}
	}

	struct IchigoLoadedFile *lf = ichVecAppend(&state->files);

	char buf[256];
	snprintf(buf, 256, "%s/%s.ich", state->baseDir, file);

	const char *data;
	size_t rd = ichLoadFile(&data, &lf->userData, buf);
	if (!rd) {
		logError("Ichigo %s: failed to open file\n", file);
		state->files.nElements -= 1;
		return -1;
	}
	lf->data = (uint16_t *)data;
	lf->dataLen2 = rd / 2;


	strcpy(lf->path, file);

	struct IchigoFile *icf = (struct IchigoFile *)lf->data;
	if (memcmp(icf->signature, "Ichigo\0", 8)) {
		logError("Ichigo: %s: file has incorrect signature\n", file);
		state->files.nElements -= 1;
		return -1;
	}
	if (icf->version != 0x100) {
		logError("Ichigo: %s: version mismatch\n", file);
		state->files.nElements -= 1;
		return -1;
	}

	const char *end = data + rd;
	data = (const char *)lf->data + sizeof(*icf);
	while (data < end) {
		struct IchigoChunk *chk = (struct IchigoChunk *)data;
		//data += sizeof(sizeof(*chk));

		if (!memcmp(chk->sig, "FN\0", 4)) {
			struct IchigoFn **fn = ichVecAppend(&state->fns);
			*fn = (struct IchigoFn *)(chk + 1);
		} else if (!memcmp(chk->sig, "IMPT", 4)) {
			struct IchigoImport *imp = (struct IchigoImport *)(chk + 1);
			char nameBuf[128];
			memcpy(nameBuf, (const char *)(imp + 1), imp->nameLen);
			nameBuf[imp->nameLen] = 0;
			int err = ichigoAddFile(state, nameBuf);
			if (err)
				return err;
		} else if (!memcmp(chk->sig, "GLBL", 4)) {
			struct IchigoGlobal **g = ichVecAppend(&state->globals);
			*g = (struct IchigoGlobal *)(chk + 1);
		} else {
			logError("Ichigo: %s: Unknown chunk\n", file);
			return -1;
		}
		data += chk->len;
	}

	return 0;
}

void ichigoClear(struct IchigoState *state) {
	for (int i = 0; i < state->files.nElements; i++) {
		/* Delete file */
		struct IchigoLoadedFile *lf = ichVecAt(&state->files, i);
		ichFreeFile(lf->userData);

	}
	ichVecDestroy(&state->files);
	ichVecDestroy(&state->fns);
	ichVecDestroy(&state->globals);
}

void ichigoInit(struct IchigoState *newState, const char *baseDir) {
	struct IchigoState *s = newState;
	ichVecCreate(&s->files, sizeof(struct IchigoLoadedFile));
	ichVecCreate(&s->fns, sizeof(struct IchigoFn *));
	ichVecCreate(&s->globals, sizeof(struct IchigoGlobal *));

	s->baseDir = baseDir;
	s->nInstrs = 0;
	s->nVars = 0;
}

void ichigoFini(struct IchigoState *state) {
	ichigoClear(state);
}

void ichigoSetInstrTable(struct IchigoState *state, IchigoInstr **instrs, int nInstrs) {
	state->instrs = instrs;
	state->nInstrs = nInstrs;
}

void ichigoSetVarTable(struct IchigoState *state, struct IchigoVar *vars, int nVars) {
	state->vars = vars;
	state->nVars = nVars;
}


void ichVecCreate(struct IchigoVector *vec, unsigned int elementSize) {
	vec->elementSize = elementSize;
	vec->nElements = 0;
	vec->nAllocations = 0;
	vec->data = NULL;
}

void ichVecDestroy(struct IchigoVector *vec) {
	vec->nElements = 0;
	vec->nAllocations = 0;
	if (vec->data)
		ichFree(vec->data);
	vec->data = NULL;
}

void *ichVecAppend(struct IchigoVector *vec) {
	if (vec->nElements == vec->nAllocations) {
		/* Allocate new */
		unsigned int newAllocations = (vec->nAllocations == 0) ? 4 : vec->nAllocations + 4;
		vec->data = ichRealloc(vec->data, newAllocations * vec->elementSize);
		memset((char *)vec->data + vec->nAllocations * vec->elementSize, 0, (newAllocations - vec->nAllocations) * vec->elementSize);
		vec->nAllocations = newAllocations;
	}
	int pos = vec->nElements;
	vec->nElements += 1;
	return &vec->data[pos * vec->elementSize];
}

void ichVecDelete(struct IchigoVector *vec, unsigned int index) {
	memmove(&vec->data[index * vec->elementSize], &vec->data[(index + 1) * vec->elementSize],
		(vec->nElements - index - 1) * vec->elementSize);
	vec->nElements -= 1;
}

static void ichHeapNotifier(void *arg, void *component, int type) {
	(void)arg;
	if (type == NOTIFY_DELETE) {
		struct IchigoHeapObject *ho = component;
		ichFree(ho->data);
	} else if (type == NOTIFY_PURGE) {
		for (struct IchigoHeapObject *ho = clBegin(ICHIGO_HEAP_OBJ); ho; ho = clNext(ICHIGO_HEAP_OBJ, ho)) {
			ichFree(ho->data);
		}
	}
}

void ichigoHeapInit(void) {
	componentListInit(ICHIGO_HEAP_OBJ, struct IchigoHeapObject);
	setNotifier(ICHIGO_HEAP_OBJ, ichHeapNotifier, NULL);
}

void ichigoHeapFini(void) {
	componentListFini(ICHIGO_HEAP_OBJ);
}