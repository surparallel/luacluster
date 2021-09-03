
#ifndef _VECTOR_H
#define _VECTOR_H

struct Vector3 {
    union {
        struct {
            float x, y, z;
        };
        float el[3];
    };
};

extern struct Vector3 gRight;
extern struct Vector3 gUp;
extern struct Vector3 gForward;
extern struct Vector3 gZeroVec;

void vector3Negate(struct Vector3* in, struct Vector3* out);
void vector3Scale(struct Vector3* in, struct Vector3* out, float scale);
void vector3Add(struct Vector3* a, struct Vector3* b, struct Vector3* out);
void vector3AddScaled(struct Vector3* a, struct Vector3* normal, float scale, struct Vector3* out);
void vector3Sub(struct Vector3* a, struct Vector3* b, struct Vector3* out);
void vector3Normalize(struct Vector3* in, struct Vector3* out);
void vector3Lerp(struct Vector3* a, struct Vector3* b, float t, struct Vector3* out);
float vector3Dot(struct Vector3* a, struct Vector3* b);
float vector3MagSqrd(struct Vector3* a);
float vector3DistSqrd(struct Vector3* a, struct Vector3* b);
void vector3Cross(struct Vector3* a, struct Vector3* b, struct Vector3* out);
void vector3Project(struct Vector3* in, struct Vector3* normal, struct Vector3* out);
void vector3ProjectPlane(struct Vector3* in, struct Vector3* normal, struct Vector3* out);
void vector3MoveTowards(struct Vector3* from, struct Vector3* towards, float maxDistance, struct Vector3* out);

#endif