
#include "box.h"

void boxOffset(struct Box* box, struct Vector3* by, struct Box* result) {
    vector3Add(&box->min, by, &result->min);
    vector3Add(&box->max, by, &result->max);
}