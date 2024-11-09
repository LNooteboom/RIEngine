#ifndef COLLISION_H
#define COLLISION_H

#include <ecs.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CollisionInfo {
	float x1, y1;
	float rr1, ri1;
	float w1, h1;

	float x2, y2;
	float rr2, ri2;
	float w2, h2;
};
bool collisionCC(const struct CollisionInfo *inf);
bool collisionCR(const struct CollisionInfo *inf); /* Circle in x1/y1, rect in x2/y2 */
bool collisionRR(const struct CollisionInfo *inf);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
