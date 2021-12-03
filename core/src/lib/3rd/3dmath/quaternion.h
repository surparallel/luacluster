
#ifndef _QUATERNION_H
#define _QUATERNION_H

#include "vector.h"
#include "vector2.h"

//角度转弧度： π/180×角度  ；弧度变角度：180/π×弧度
#undef PI
#define PI (3.14159265358979323846f)
#define DEG_TO_RAD (PI/180.0f)
#define RAD_TO_DEG (180.0f/PI)

struct Quaternion {
    float x, y, z, w;
};

void quatIdent(struct Quaternion* q);
void quatAxisAngle(struct Vector3* axis, float angle, struct Quaternion* out);
void quatAxisComplex(struct Vector3* axis, struct Vector2* complex, struct Quaternion* out);
void quatConjugate(struct Quaternion* in, struct Quaternion* out);
void quatMultVector(struct Quaternion* q, struct Vector3* a, struct Vector3* out);
void quatMultiply(struct Quaternion* a, struct Quaternion* b, struct Quaternion* out);
void quatToMatrix(struct Quaternion* q, float out[4][4]);
void quatNormalize(struct Quaternion* q, struct Quaternion* out);
void quatRandom(struct Quaternion* q);
void quatEulerAngles(struct Vector3* angles, struct Quaternion* out);
void quatToEulerAngles(struct Quaternion* q, struct Vector3* out);
void quatLookRotation(struct Vector3* forward, struct Quaternion* out);
void quatFromToRotation(struct Vector3* fromDirection, struct Vector3* toDirection, struct Quaternion* out);
void quatLookRotationWithUpwards(struct Vector3* forward, struct Vector3* upwards, struct Quaternion* out);
void Radians(struct Vector3* in, struct Vector3* out);
void Degree(struct Vector3* in, struct Vector3* out);

void ToQ(struct Vector3* in, struct Quaternion* out);
void FromQ2(struct Quaternion* in, struct Vector3* out);

#endif