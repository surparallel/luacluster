
#include "ray.h"

void rayProjectOnto(struct Vector3* origin, struct Vector3* dir, struct Vector3* point, struct Vector3* out) {
    struct Vector3 offset;
    vector3Sub(point, origin, &offset);
    float scalar = vector3Dot(&offset, dir) / vector3MagSqrd(dir);

    out->x = origin->x + dir->x * scalar;
    out->y = origin->y + dir->y * scalar;
    out->z = origin->z + dir->z * scalar;
}

void rayPointAtDistance(struct Vector3* origin, struct Vector3* dir, float distance, struct Vector3* out) {
    out->x = origin->x + dir->x * distance;
    out->y = origin->y + dir->y * distance;
    out->z = origin->z + dir->z * distance;
}