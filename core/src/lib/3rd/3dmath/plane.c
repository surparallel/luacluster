
#include "plane.h"

void planeFromNormalPoint(struct Vector3* normal, struct Vector3* point, struct Plane* out) {
    out->a = normal->x;
    out->b = normal->y;
    out->c = normal->z;
    out->d = -(normal->x * point->x + normal->y * point->y + normal->z * point->z);
}

void planeProjectOnto(struct Plane* plane, struct Vector3* point, struct Vector3* out) {
    float distance = planeDistanceFromPoint(plane, point);
    out->x = point->x - plane->a * distance;
    out->y = point->y - plane->b * distance;
    out->z = point->z - plane->c * distance;
}

float planeDistanceFromPoint(struct Plane* plane, struct Vector3* point) {
    return plane->a * point->x + plane->b * point->y + plane->c * point->z + plane->d;
}

void planeFrustumExtractFromMtx(float mtx[4][4], struct Plane planes[4]) {
    planes[0].a = mtx[0][3] - mtx[0][0];
    planes[0].b = mtx[1][3] - mtx[1][0];
    planes[0].c = mtx[2][3] - mtx[2][0];
    planes[0].d = mtx[3][3] - mtx[3][0];

    planes[1].a = mtx[0][3] + mtx[0][0];
    planes[1].b = mtx[1][3] + mtx[1][0];
    planes[1].c = mtx[2][3] + mtx[2][0];
    planes[1].d = mtx[3][3] + mtx[3][0];

    planes[2].a = mtx[0][3] - mtx[0][1];
    planes[2].b = mtx[1][3] - mtx[1][1];
    planes[2].c = mtx[2][3] - mtx[2][1];
    planes[2].d = mtx[3][3] - mtx[3][1];

    planes[3].a = mtx[0][3] + mtx[0][1];
    planes[3].b = mtx[1][3] + mtx[1][1];
    planes[3].c = mtx[2][3] + mtx[2][1];
    planes[3].d = mtx[3][3] + mtx[3][1];

    planeNormalize(&planes[0]);
    planeNormalize(&planes[1]);
    planeNormalize(&planes[2]);
    planeNormalize(&planes[3]);
}

void planeNormalize(struct Plane* plane) {

}

int planeIsBoxBehind(struct Plane* plane, struct Box* box) {
    struct Vector3 pointToUse;

    int i;
    for (i = 0; i < 3; ++i) {
        if (plane->normal.el[i] > 0.0f) {
            pointToUse.el[i] = box->max.el[i];
        } else {
            pointToUse.el[i] = box->min.el[i];
        }
    }

    return planeDistanceFromPoint(plane, &pointToUse) < 0.0f;
}

int planeIsSphereBehind(struct Plane* plane, struct Vector3* origin, float radius) {
    return planeDistanceFromPoint(plane, origin) < -radius;
}