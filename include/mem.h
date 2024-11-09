#ifndef MEM_H
#define MEM_H

#include <main.h>
#include <stddef.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CacheEntry;
struct CachePage;
struct Cache {
	const char *name;
	size_t objSize;
	struct CacheEntry *list;
	struct CachePage *pList;
};

/*
 * Initialize various allocators.
 */
void memInit(void);

/*
 * Allocator for temporary memory, like a second stack. Data allocated must be deallocated in reverse order
 */
void *stackAlloc(size_t sz);

/*
 * Deallocate memory allocated by stackAlloc
 */
void stackDealloc(size_t sz);

/*
 * Allocate memory on the global heap.
 */
void *globalAlloc(size_t sz);

/*
 * Deallocate memory on the global heap.
 */
void globalDealloc(void *mem);

/*
 * Reallocate memory on the global heap.
 */
void *globalRealloc(void *mem, size_t newSz);

/*
 * Create a cache for object storage
 */
void cacheCreate(struct Cache * newCache, size_t size, const char * name);

/*
 * Clear the cache of all objects, does not deallocate the cache struct
 */
void cachePurge(struct Cache *cache);

/*
 * Allocate an object in a cache
 */
void *cacheGet(struct Cache *cache);

/*
 * Deallocate an object in a cache
 */
void cacheRelease(struct Cache * cache, void * obj);


/*
 * Linked list
 */

struct _LLEntry {
	struct _LLEntry *next;
	struct _LLEntry *prev;
};

typedef struct _LLHead {
	struct _LLEntry *first;
	struct _LLEntry *last;
	unsigned int count;
} LinkedList;

/* Put these in your structs */
#define llEntry struct _LLEntry _llEntry
#define llHead struct _LLHead _llHead

/* Add/remove functions */
/* l = list */
/* e = entry */
#define llAdd(l, e) do { \
	(e)._llEntry.next = NULL; \
	(e)._llEntry.prev = (l).last; \
	if ((l).last) \
		(l).last->next = &((e)._llEntry); \
	else \
		(l).first = &((e)._llEntry); \
	(l).last = &((e)._llEntry); \
	(l).count += 1; \
} while (0)

#define llAddFront(l, e) do { \
	(e)._llEntry.prev = NULL; \
	(e)._llEntry.next = (l).first; \
	if ((l).first) \
		(l).first->prev = &((e)._llEntry); \
	else \
		(l).last = &((e)._llEntry); \
	(l).first = &((e)._llEntry); \
	(l).count += 1; \
} while (0)

#define llRemove(l, e) do { \
	if ((e)._llEntry.prev) \
		(e)._llEntry.prev->next = (e)._llEntry.next; \
	else \
		(l).first = (e)._llEntry.next; \
	if ((e)._llEntry.next) \
		(e)._llEntry.next->prev = (e)._llEntry.prev; \
	else \
		(l).last = (e)._llEntry.prev; \
	(l).count -= 1; \
} while (0)

#define llCount(l) (l).count

/* Iterators */
/* t = entry type */
/* i = iterator */
#define llBegin(t, l) (t*)( (uintptr_t)((l).first) - offsetof(t, _llEntry) )

#define llEnd(t, l) NULL

#define llNext(t, i) (t*)( (uintptr_t)((i)->_llEntry.next) - offsetof(t, _llEntry) )

#define llPrev(t, i) (t*)( (uintptr_t)((i)->_llEntry.prev) - offsetof(t, _llEntry) )

#define llForEach(t, i, l) for (t *i = llBegin(t, l); i != llEnd(t, l); i = llNext(t, i))


/*
* Vector
 */

struct Vector {
	size_t elementSize;
	unsigned int nElements;
	unsigned int nAllocations;
	char *data;
};

/**
 * Create a vector
 */
void vecCreate(struct Vector *vec, size_t elementSize);

/**
 * Destroy a vector
 */
void vecDestroy(struct Vector *vec);

/**
 * Insert into a vector
 * if pos == -1 insert at the end
 */
void *vecInsert(struct Vector *vec, int pos);

/**
 * Delete a vector entry
 */
void vecDelete(struct Vector *vec, unsigned int index);

void vecClear(struct Vector *vec);

/**
 * Get vector element
 */
static inline void *vecAt(struct Vector *vec, unsigned int index) {
	return (index < vec->nElements) ? (void *)&vec->data[index * vec->elementSize] : NULL;
}

/**
 * Get the number of elements in a vector
 */
static inline unsigned int vecCount(struct Vector *vec) {
	return vec->nElements;
}


/*
Hashtable
*/

struct HTBucket;

struct HTEntry {
	llEntry;
	struct HTBucket *parent;
	const char *key;
};

struct HTBucket {
	LinkedList list;
};

struct HashTable {
	struct HTBucket *bu;
	unsigned int nBuckets;
};

/**
 * Create a hash table
 */
int HTCreate(struct HashTable *tbl, unsigned int nBuckets);

/**
 * Get a hashtable entry
 */
struct HTEntry *HTGet(struct HashTable *tbl, const char *key);

/**
 * Add a hashtable entry
 */
void HTAdd(struct HashTable *tbl, struct HTEntry *entry);

/**
 * Delete a hashtable entry
 * Does not deallocate entry from memory
 */
void HTDelete(struct HashTable *tbl, struct HTEntry *entry);

/**
 * Delete the whole hashtable
 * DOES NOT DELETE INDIVIDUAL ENTRIES
 */
void HTDestroy(struct HashTable *tbl);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
