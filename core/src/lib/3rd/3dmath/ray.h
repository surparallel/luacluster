
#ifndef _MATH_RAY_H
#define _MATH_RAY_H

#include "vector.h"

void rayProjectOnto(struct Vector3* origin, struct Vector3* dir, struct Vector3* point, struct Vector3* out);
void rayPointAtDistance(struct Vector3* origin, struct Vector3* dir, float distance, struct Vector3* out);

#endif