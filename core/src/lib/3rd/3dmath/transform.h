
#ifndef _MATH_BASICTRANSFORM_H
#define _MATH_BASICTRANSFORM_H

#include "quaternion.h"
#include "vector.h"

struct BasicTransform {
    struct Vector3 position;
    struct Quaternion rotation;
    float scale;
};

// result must not equal a nor b 
void transformConcat(struct BasicTransform* a, struct BasicTransform* b, struct BasicTransform* result);
void transformPoint(struct BasicTransform* trans, struct Vector3* in, struct Vector3* out);
void transformPointInverse(struct BasicTransform* trans, struct Vector3* in, struct Vector3* out);
void transformDirection(struct BasicTransform* trans, struct Vector3* in, struct Vector3* out);
void transformDirectionInverse(struct BasicTransform* trans, struct Vector3* in, struct Vector3* out);
void transformInvert(struct BasicTransform* in, struct BasicTransform* out);
void transformToMatrix(struct BasicTransform* in, float worldScale, float mtx[4][4]);
void transformForward(struct BasicTransform* in, struct Vector3* out);

void transformIdentity(struct BasicTransform* trans);

#endif