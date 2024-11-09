#include <ecs.h>
#include <assets.h>
#include <mem.h>
#include <string.h>
#include <main.h>

#define DENSE_PAGE_SIZE 1024
#define DENSE_PAGELIST_SIZE (MAX_ENTITY / DENSE_PAGE_SIZE)
#define SPARSE_NONE ((idx_t)-1)

struct ComponentList {
	bool initialized;
	bool noPurge;
	bool deletedComponent;
	bool keepOrdering;

	unsigned int componentSize;
	unsigned int count;
	unsigned int nDensePages;

	/* Create/delete notifier */
	void (*notifier)(void *arg, void *component, int type);
	void *arg;

	idx_t *sparse;
	void *dense[DENSE_PAGELIST_SIZE];
};
static struct ComponentList componentLists[MAX_COMPONENTLIST];

uint32_t firstFreeEntity;
entity_t entityList[MAX_ENTITY];
uint64_t entityComponentLists[MAX_ENTITY * 2];


void componentListInitSz(int id, unsigned int elementSize) {
	struct ComponentList *cl = &componentLists[id];
	memset(cl, 0, sizeof(*cl));
	cl->initialized = true;
	cl->notifier = NULL;
	cl->componentSize = elementSize;

	cl->sparse = globalAlloc(sizeof(idx_t) * MAX_ENTITY);
	for (unsigned int i = 0; i < MAX_ENTITY; i++) {
		cl->sparse[i] = SPARSE_NONE;
	}

	logDebug("New component list (%d)\n", id);
}

void componentListFini(int id) {
	struct ComponentList *cl = &componentLists[id];
	cl->initialized = false;

	for (unsigned int i = 0; i < cl->nDensePages; i++) {
		globalDealloc(cl->dense[i]);
	}
	globalDealloc(cl->sparse);
}

static inline entity_t *denseAt(struct ComponentList *cl, idx_t idx) {
	return (entity_t *)((char *)(cl->dense[idx / DENSE_PAGE_SIZE]) + cl->componentSize * (idx % DENSE_PAGE_SIZE));
}

void *getComponentOpt(int id, entity_t entity) {
	struct ComponentList *cl = &componentLists[id];
	idx_t idx = cl->sparse[entity >> ENTITY_ID_SHIFT];
	if (idx != SPARSE_NONE) {
		entity_t *ret = denseAt(cl, idx);
		if (entity == *ret)
			return ret;
	}
	return NULL;
}
void *getComponent(int id, entity_t entity) {
	void *ret = getComponentOpt(id, entity);
	if (!ret) {
		fail("Component 0x%x in list %d not found!\n", entity, id);
	}
	return ret;
}

void *newComponent(int id, entity_t entity) {
	struct ComponentList *cl = &componentLists[id];
	if (!cl->initialized) {
		fail("newComponent: Invalid component list\n");
	}

	entity_t sparseIdx = entity >> ENTITY_ID_SHIFT;
	idx_t idx;
	if (cl->sparse[sparseIdx] != SPARSE_NONE) {
		/* Overwrite existing component if it already exists */
		idx = cl->sparse[sparseIdx];
		logDebug("Overwriting component %x in list %d\n", entity, id);
	} else {
		idx = cl->count;
		cl->sparse[sparseIdx] = idx;
		if (idx % DENSE_PAGE_SIZE == 0) {
			size_t sz = (size_t)DENSE_PAGE_SIZE * cl->componentSize;
			char *dense = globalAlloc(sz + 4);
			*(entity_t *)(dense + sz) = ((idx / DENSE_PAGE_SIZE) + 1) << ENTITY_ID_SHIFT;
			cl->dense[idx / DENSE_PAGE_SIZE] = dense;
			cl->nDensePages++;
		}
		cl->count++;
	}
	void *ret = denseAt(cl, idx);
	memset(ret, 0, cl->componentSize);
	*(entity_t *)ret = entity;

	/* Set entityComponentLists */
	entityComponentLists[sparseIdx * 2 + id / 64] |= (1ULL << id % 64);

	if (cl->notifier) {
		cl->notifier(cl->arg, ret, NOTIFY_CREATE);
	}

	return ret;
}

static void unregEntity(entity_t entity);
void removeComponent(int id, entity_t entity) {
	struct ComponentList *cl = &componentLists[id];
	entity_t sparseIdx = entity >> ENTITY_ID_SHIFT;
	idx_t idx = cl->sparse[sparseIdx];
	if (idx == SPARSE_NONE)
		return;

	cl->sparse[sparseIdx] = SPARSE_NONE;
	
	entity_t *dst = denseAt(cl, idx);
	if (entity != *dst)
		return; /* Deleting an older version */

	if (cl->notifier)
		cl->notifier(cl->arg, dst, NOTIFY_DELETE);

	if (cl->keepOrdering) {
		*dst = 0;
	} else {
		cl->count -= 1;
		if (idx != cl->count) {
			/* Do swap */
			entity_t *src = denseAt(cl, cl->count);
			memcpy(dst, src, cl->componentSize);
			entity_t swapped = *dst;
			cl->sparse[swapped >> ENTITY_ID_SHIFT] = idx;
			*src = 0;
		} else {
			*dst = 0;
		}

		if (cl->count % DENSE_PAGE_SIZE == 0) {
			uint32_t i = cl->count / DENSE_PAGE_SIZE;
			globalDealloc(cl->dense[i]);
			cl->dense[i] = NULL;
			cl->nDensePages--;
		}
		cl->deletedComponent = true;
	}

	entityComponentLists[sparseIdx * 2 + id / 64] &= ~(1ULL << id % 64);
	if (!entityComponentLists[sparseIdx * 2] && !entityComponentLists[sparseIdx * 2 + 1]) {
		/* Unregister this entity if it doesnt have any more components */
		unregEntity(entity);
	}
}
void componentListOrderedClean(int id) {
	struct ComponentList *cl = &componentLists[id];
	int srcIdx = 0, dstIdx = 0;
	for (unsigned int i = 0; i < cl->count; i++) {
		entity_t *src = denseAt(cl, srcIdx);
		if (*src) {
			if (srcIdx != dstIdx) {
				entity_t *dst = denseAt(cl, dstIdx);
				memcpy(dst, src, cl->componentSize);
				*src = 0;
				cl->sparse[*dst >> ENTITY_ID_SHIFT] = dstIdx;
			}
			dstIdx++;
		}
		srcIdx++;
	}
	cl->count = dstIdx;
	int start = (cl->count / DENSE_PAGE_SIZE);
	if (cl->count % DENSE_PAGE_SIZE)
		start++;
	for (int i = start; i < cl->nDensePages; i++) {
		if (cl->dense[i]) {
			globalDealloc(cl->dense[i]);
			cl->dense[i] = NULL;
		}
	}
	cl->nDensePages = start;
}

idx_t clCount(int id) {
	struct ComponentList *cl = &componentLists[id];
	return cl->count;
}
void *clAt(int id, idx_t idx) {
	struct ComponentList *cl = &componentLists[id];
	return denseAt(cl, idx);
}

void *clBegin(int id) {
	struct ComponentList *cl = &componentLists[id];
	cl->deletedComponent = false;
	return cl->dense[0];
}
void *clNext(int id, void *it) {
	struct ComponentList *cl = &componentLists[id];
	entity_t *next;
	if (cl->deletedComponent) {
		cl->deletedComponent = false;
		if (!cl->count)
			return NULL;
		next = it;
	} else {
		next = (entity_t *)((char *)it + cl->componentSize);
	}
	if (!(*next & ENTITY_VERSION_MASK)) {
		if (*next & ENTITY_ID_MASK) {
			idx_t denseIdx = *next >> ENTITY_ID_SHIFT;
			next = cl->dense[denseIdx];
		} else {
			return NULL;
		}
	}
	return next;
}


void setNotifier(int id, void (*func)(void *arg, void *component, int type), void *arg) {
	struct ComponentList *cl = &componentLists[id];
	cl->notifier = func;
	cl->arg = arg;
}

void componentListAllowPurge(int id, bool allow) {
	struct ComponentList *cl = &componentLists[id];
	cl->noPurge = !allow;
}
void componentListKeepOrdering(int id, bool keep) {
	struct ComponentList *cl = &componentLists[id];
	cl->keepOrdering = keep;
}

void componentListEndScene(void) {
	for (int id = 0; id < 128; id++) {
		struct ComponentList *cl = &componentLists[id];
		if (!cl->initialized || cl->noPurge)
			continue;

		if (cl->notifier) {
			cl->notifier(cl->arg, NULL, NOTIFY_PURGE);
		}

		for (unsigned int i = 0; i < cl->nDensePages; i++) {
			globalDealloc(cl->dense[i]);
			cl->dense[i] = NULL;
		}
		for (unsigned int i = 0; i < MAX_ENTITY; i++) {
			cl->sparse[i] = SPARSE_NONE;
		}
		cl->nDensePages = 0;
		cl->count = 0;
	}

	for (unsigned int i = 0; i < MAX_ENTITY; i++) {
		entityList[i] = (i + 1) << ENTITY_ID_SHIFT | 1;
		entityComponentLists[i * 2] = 0;
		entityComponentLists[i * 2 + 1] = 0;
	}
	firstFreeEntity = 0;
}


entity_t newEntity(void) {
	if (firstFreeEntity == MAX_ENTITY) {
		fail("Entity limit reached");
	}
	entity_t *en = &entityList[firstFreeEntity];
	entity_t ret = (firstFreeEntity << ENTITY_ID_SHIFT) | (*en & ENTITY_VERSION_MASK);
	if (firstFreeEntity == *en >> ENTITY_ID_SHIFT) {
		logNorm("ECS Error\n");
	}
	//logDebug("New: %d\n", firstFreeEntity);
	firstFreeEntity = *en >> ENTITY_ID_SHIFT;
	*en = ((MAX_ENTITY - 1) << ENTITY_ID_SHIFT) | (*en & ENTITY_VERSION_MASK);
	return ret;
}

static void unregEntity(entity_t entity) {
	idx_t idx = entity >> ENTITY_ID_SHIFT;
	entity_t *en = &entityList[idx];

	//logDebug("Del: %d\n", idx);

	/* Delete entity from list */
	unsigned int ver = entity & ENTITY_VERSION_MASK;
	ver = ver == ENTITY_VERSION_MASK ? 1 : ver + 1;
	*en = (firstFreeEntity << ENTITY_ID_SHIFT) | ver;
	firstFreeEntity = idx;
}

void deleteEntity(entity_t entity) {
	idx_t idx = entity >> ENTITY_ID_SHIFT;
	entity_t *en = &entityList[idx];
	if (*en >> ENTITY_ID_SHIFT != MAX_ENTITY - 1)
		return; /* Entity doesnt exist */
	if ((*en & ENTITY_VERSION_MASK) != (entity & ENTITY_VERSION_MASK)) {
		return; /* Entity already has a new version */
	}
	

	/* Delete components */
	uint64_t *components = &entityComponentLists[idx * 2];
	for (int i = 0; i < 2; i++) {
		for (int j = 0; components[i] && j < 64; j++) {
			if (components[i] & (1ULL << j)) {
				removeComponent(i * 64 + j, entity);
			}
		}
	}
}

void ecsInit(void) {
	for (unsigned int i = 0; i < MAX_ENTITY; i++) {
		entityList[i] = (i + 1) << ENTITY_ID_SHIFT | 1;
	}
}