#include <mem.h>
#include <assets.h>

#include <string.h>
#include <stdlib.h>

#define BIG_STACK_SIZE	0x4000000 //64MB

static char *bigStackStart;
static size_t bigStackPtr;


void memInit(void) {
	bigStackStart = globalAlloc(BIG_STACK_SIZE);
	bigStackPtr = BIG_STACK_SIZE;
}

/*
 * BigStack
 */

void *stackAlloc(size_t sz) {
	if (sz > bigStackPtr) {
		fail("Out of bigstack memory!\n");
	}
	bigStackPtr -= sz;
	return bigStackStart + bigStackPtr;
}

void stackDealloc(size_t sz) {
	bigStackPtr += sz;
}

/*
 * Global alloc
 */

void *globalAlloc(size_t sz) {
	void *ret = calloc(sz, 1);
	if (!ret) {
		fail("Out of memory!\n");
	}
	return ret;
}

void globalDealloc(void *mem) {
	free(mem);
}

void *globalRealloc(void *mem, size_t newSz) {
	void *ret = realloc(mem, newSz);
	if (!ret) {
		fail("Out of memory!\n");
	}
	return ret;
}

/*
 * Cache
 */

#define CACHE_BLOCK_SIZE	0x100000
#define CACHE_MAX_OBJ_SIZE	(CACHE_BLOCK_SIZE / 16)
#define CACHE_ALIGN			8

struct CachePage {
	struct CachePage *next;
	void *unused;
};

struct CacheEntry {
	struct CacheEntry *next;
};

void cacheCreate(struct Cache * newCache, size_t size, const char * name) {
#ifdef CACHE_ALIGN
	if (size < CACHE_ALIGN) {
		size = CACHE_ALIGN;
	} else {
		size_t mod = size % CACHE_ALIGN;
		if (mod) {
			size -= mod;
			size += CACHE_ALIGN;
		}
	}
#endif

	if (size > CACHE_MAX_OBJ_SIZE) {
		fail("Cache: (%s) Attempted too big allocation: %ld, max is %ld\n", name, size, CACHE_MAX_OBJ_SIZE);
	}

	memset(newCache, 0, sizeof(*newCache));
	newCache->name = name;
	newCache->objSize = size;
}

void cachePurge(struct Cache *cache) {
	struct CachePage *block = cache->pList;
	while (block) {
		struct CachePage *next = block->next;
		globalDealloc(block);
		block = next;
	}
	cache->pList = NULL;
	cache->list = NULL;
}

static void addNewPage(struct Cache *cache) {
	struct CachePage *block = globalAlloc(CACHE_BLOCK_SIZE);
	block->next = cache->pList;
	cache->pList = block;

	uintptr_t p2 = (uintptr_t)(block + 1);
	uint32_t nEntries = (uint32_t)((CACHE_BLOCK_SIZE - sizeof(struct CachePage)) / cache->objSize);
	for (uint32_t i = 0; i < nEntries; i++) {
		struct CacheEntry *en = (struct CacheEntry *)(p2);
		en->next = cache->list;
		cache->list = en;
		p2 += cache->objSize;
	}
}

void *cacheGet(struct Cache *cache) {
	struct CacheEntry *en = cache->list;
	if (!en) {
		addNewPage(cache);
		en = cache->list;
	}
	cache->list = en->next;
	return en;
}

void cacheRelease(struct Cache * cache, void * obj) {
	struct CacheEntry *en = obj;
	en->next = cache->list;
	cache->list = en;
}


/*
Vector
*/

void vecCreate(struct Vector *vec, size_t elementSize) {
	vec->elementSize = elementSize;
	vec->nElements = 0;
	vec->nAllocations = 0;
	vec->data = NULL;
}

void vecDestroy(struct Vector *vec) {
	vec->elementSize = 0;
	vec->nElements = 0;
	vec->nAllocations = 0;
	if (vec->data)
		globalDealloc(vec->data);
	vec->data = NULL;
}

void *vecInsert(struct Vector *vec, int pos) {
	assert(vec->elementSize);
	if (vec->nElements == vec->nAllocations) {
		/* Allocate new */
		unsigned int newAllocations = (vec->nAllocations == 0) ? 1 : vec->nAllocations * 2;
		vec->data = globalRealloc(vec->data, newAllocations * vec->elementSize);
		vec->nAllocations = newAllocations;
	}
	if (pos < 0) {
		pos = vec->nElements;
	} else {
		memmove(&vec->data[(pos + 1) * vec->elementSize], &vec->data[pos * vec->elementSize],
			(vec->nElements - pos) * vec->elementSize);
	}
	vec->nElements += 1;
	return &vec->data[pos * vec->elementSize];
}

void vecDelete(struct Vector *vec, unsigned int index) {
	memmove(&vec->data[index * vec->elementSize], &vec->data[(index + 1) * vec->elementSize],
		(vec->nElements - index - 1) * vec->elementSize);
	vec->nElements -= 1;
}

void vecClear(struct Vector *vec) {
	if (vec->data)
		globalDealloc(vec->data);
	vec->nAllocations = 0;
	vec->nElements = 0;
	vec->data = NULL;
}


/*
Hash table
*/

int HTCreate(struct HashTable *tbl, unsigned int nBuckets) {
	tbl->nBuckets = nBuckets;
	tbl->bu = globalAlloc(nBuckets * sizeof(struct HTBucket));
	return 0;
}

static unsigned int doHash(const char *key, unsigned int nBu) {
	/* Do a simple checksum */
	const uint8_t *c = (uint8_t *)key;
	unsigned int sum = 0;
	while (*c) {
		sum += *c;
		c++;
	}
	return sum % nBu;
}

struct HTEntry *HTGet(struct HashTable *tbl, const char *key) {
	struct HTBucket *bu = &tbl->bu[doHash(key, tbl->nBuckets)];
	llForEach(struct HTEntry, en, bu->list) {
		if (!strcmp(key, en->key)) {
			return en;
		}
	}
	return NULL;
}

void HTAdd(struct HashTable *tbl, struct HTEntry *entry) {
	struct HTBucket *bu = &tbl->bu[doHash(entry->key, tbl->nBuckets)];

	llAdd(bu->list, *entry);
	entry->parent = bu;

	return;
}

void HTDelete(struct HashTable *tbl, struct HTEntry *entry) {
	(void) tbl;
	struct HTBucket *bu = entry->parent;
	llRemove(bu->list, *entry);
	return;
}

void HTDestroy(struct HashTable *tbl) {
	globalDealloc(tbl->bu);
	tbl->bu = NULL;
}


