
#ifndef _MATH_BOX_H
#define _MATH_BOX_H

#include "vector.h"

struct Box {
    struct Vector3 min, max;
};

void boxOffset(struct Box* box, struct Vector3* by, struct Box* result);

#endif