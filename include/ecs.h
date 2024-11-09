#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <mem.h>

#include <stdbool.h>
#include <main.h>

typedef uint32_t entity_t;
typedef uint32_t idx_t;

#define MAX_ENTITY 0x00100000UL
#define ENTITY_VERSION_MASK 0xFFF
#define ENTITY_ID_SHIFT 12
#define ENTITY_ID_MASK 0xFFFFF000

/* Component list types */
/* basics.h */
#define ICHIGO_VM 100
#define ICHIGO_LOCALS 101
#define TRANSFORM 102
/* DrawVM */
#define ANIM_STATE 103
#define DRAW_VM 104
#define DRAW_VM_VM 105
#define DRAW_VM_LOCALS 106
/* physics/collision.h */
#define STATIC_COLLIDER 107
#define DYNAMIC_COLLIDER 108
/* physics/mesh.h */
#define COLL_MESH 109
#define PHYS_BODY 110
#define PHYS_CHARACTER 111
/* Ichigo heap objects */
#define ICHIGO_HEAP_OBJ 112
#define MAX_COMPONENTLIST 128

/* Notfier definitions */
#define NOTIFY_CREATE	0
#define NOTIFY_DELETE	1
#define NOTIFY_PURGE	2


/**
 * Create a component list, components must have an entity_t entity as their first variable
 */
#define componentListInit(cl, type) componentListInitSz(cl, sizeof(type))
void componentListInitSz(int id, unsigned int elementSize);

/**
 * Delete a component list
 */
void componentListFini(int id);

/**
 * Search for a component using an object ID, fails if component not found
 */
void *getComponent(int d, entity_t entity);

/**
 * Search for a component using an object ID, returns NULL if not found
 */
void *getComponentOpt(int d, entity_t entity);

/**
 * Allocate room for a new component
 */
void *newComponent(int id, entity_t entity);

/**
 * Deallocate a component
 */
void removeComponent(int id, entity_t entity);

/**
 * Get the number of components inside a component list
 */
idx_t clCount(int id);

void *clAt(int id, idx_t idx);

void *clBegin(int id);

void *clNext(int id, void *it);


/**
 * Set a notifier for adding and removing components
 * Bool added is set to true if a component is added, false if removed
 */
void setNotifier(int id, void (*func)(void *arg, void *component, int type), void *arg);


/**
 * Allow a component list to be purged by componentListPurge
 * Default: true
 */
void componentListAllowPurge(int id, bool allow);

void componentListKeepOrdering(int id, bool keep);
void componentListOrderedClean(int id);

/**
 * Purge all component lists that allow it
 */
void componentListEndScene(void);


/**
 * Get a new entity ID
 */
entity_t newEntity(void);
#define getNewEntity newEntity

void deleteEntity(entity_t entity);


#ifdef __cplusplus

} // extern "C"

template <typename T, int id>
class ClIterator {
public:
	ClIterator(void *p) : data(p) {

	}
	ClIterator(const ClIterator &other) : data(other.data) {

	}

	ClIterator &operator=(const ClIterator &other) {
		data = other.data;
		return *this;
	}

	bool operator==(const ClIterator &other) const {
		return data == other.data;
	}
	bool operator!=(const ClIterator &other) const {
		return data != other.data;
	}

	ClIterator &operator++() {
		data = clNext(id, data);
		return *this;
	}

	T *ptr() const {
		return static_cast<T *>(data);
	}
	T *operator->() {
		return static_cast<T *>(data);
	}

private:
	void *data;
};

template <typename T, int id>
class ClWrapper {
public:
	void init() const {
		componentListInitSz(id, sizeof(T));
	}
	void fini() const {
		componentListFini(id);
	}

	T &operator[](entity_t en) const {
		return *static_cast<T *>(getComponent(id, en));
	}
	T &at(entity_t en) const {
		return *static_cast<T *>(getComponent(id, en));
	}
	T *opt(entity_t en) const {
		return static_cast<T *>(getComponentOpt(id, en));
	}

	T &add(entity_t en) const {
		return *static_cast<T *>(newComponent(id, en));
	}
	void remove(entity_t en) const {
		removeComponent(id, en);
	}

	ClIterator<T, id> begin() const {
		return ClIterator<T, id>(clBegin(id));
	}
	ClIterator<T, id> end() const {
		return ClIterator<T, id>(nullptr);
	}
};

#endif

#endif
