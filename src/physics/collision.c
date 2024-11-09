#include <math.h>
#include <physics/collision.h>
#include <basics.h>
#include <mem.h>
#include <main.h>

bool collisionCC(const struct CollisionInfo *inf) {
	float dx = inf->x1 - inf->x2;
	float dy = inf->y1 - inf->y2;
	float r = inf->w1 + inf->w2;
	return dx * dx + dy * dy < r * r;
}
bool collisionCR(const struct CollisionInfo *inf) {
	float x = inf->x1 - inf->x2;
	float y = inf->y1 - inf->y2;
	/* Inverse rotate with rect rotation */
	CMUL(x, y, x, y, inf->rr2, -inf->ri2);
	return fabsf(x) < inf->w1 + inf->w2 * 0.5f && fabsf(y) < inf->w1 + inf->h2 * 0.5f;
}

static bool checkOverlap(float *pointsX, float *pointsY, float x, float y) {
	float dots[8];
	for (int i = 0; i < 8; i++) {
		dots[i] = pointsX[i] * x + pointsY[i] * y;
	}
	float min1 = fminf(dots[0],
				 fminf(dots[1],
				 fminf(dots[2],
					   dots[3])));
	float max1 = fmaxf(dots[0],
				 fmaxf(dots[1],
				 fmaxf(dots[2],
					   dots[3])));
	float min2 = fminf(dots[4],
				 fminf(dots[5],
				 fminf(dots[6],
					   dots[7])));
	float max2 = fmaxf(dots[4],
				 fmaxf(dots[5],
				 fmaxf(dots[6],
					   dots[7])));
	return !(max1 < min2 || min1 > max2);
}
bool collisionRR(const struct CollisionInfo *inf) {
	float w1 = inf->w1 * 0.5f, h1 = inf->h1 * 0.5f, w2 = inf->w2 * 0.5f, h2 = inf->h2 * 0.5f;
	if (!inf->ri1 && !inf->ri2) {
		return !(inf->x1 + w1 < inf->x2 - w2 ||
			inf->x1 - w1 > inf->x2 + w2 ||
			inf->y1 + h1 < inf->y2 - h2 ||
			inf->y1 - h1 > inf->y2 + h2);
	} else {
		/* Apply Separating axis theorem */
		float pointsX[8];
		float pointsY[8];
		CMUL(pointsX[0], pointsY[0], inf->rr1, inf->ri1, -w1, -h1);
		CMUL(pointsX[1], pointsY[1], inf->rr1, inf->ri1, +w1, -h1);
		CMUL(pointsX[2], pointsY[2], inf->rr1, inf->ri1, -w1, +h1);
		CMUL(pointsX[3], pointsY[3], inf->rr1, inf->ri1, +w1, +h1);
		CMUL(pointsX[4], pointsY[4], inf->rr2, inf->ri2, -w2, -h2);
		CMUL(pointsX[5], pointsY[5], inf->rr2, inf->ri2, +w2, -h2);
		CMUL(pointsX[6], pointsY[6], inf->rr2, inf->ri2, -w2, +h2);
		CMUL(pointsX[7], pointsY[7], inf->rr2, inf->ri2, +w2, +h2);

		for (int i = 0; i < 4; i++) {
			pointsX[i] += inf->x1;
			pointsY[i] += inf->y1;
		}
		for (int i = 0; i < 4; i++) {
			pointsX[i + 4] += inf->x2;
			pointsY[i + 4] += inf->y2;
		}

		return checkOverlap(pointsX, pointsY, inf->rr1, inf->ri1) &&
			checkOverlap(pointsX, pointsY, -inf->ri1, inf->rr1) &&
			checkOverlap(pointsX, pointsY, inf->rr2, inf->ri2) &&
			checkOverlap(pointsX, pointsY, -inf->ri2, inf->rr2);
	}
}
