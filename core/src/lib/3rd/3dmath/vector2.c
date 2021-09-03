
#include "vector2.h"
#include <math.h>

struct Vector2 gRight2 = {1.0f, 0.0f};
struct Vector2 gUp2 = {0.0f, 1.0f};

void vector2ComplexMul(struct Vector2* a, struct Vector2* b, struct Vector2* out) {
    float x = a->x * b->x - a->y * b->y;
    out->y = a->x * b->y + a->y * b->x;
    out->x = x;
}

void vector2RotateTowards(struct Vector2* from, struct Vector2* towards, struct Vector2* max, struct Vector2* out) {
    struct Vector2 fromInv = {from->x, -from->y};
    struct Vector2 diff;
    vector2ComplexMul(&fromInv, towards, &diff);

    if (diff.x > max->x) {
        *out = *towards;
    } else {
        if (diff.y < 0) {
            diff.x = max->x;
            diff.y = -max->y;
        } else {
            diff = *max;
        }
        vector2ComplexMul(from, &diff, out);
    }
}

float vector2Cross(struct Vector2* a, struct Vector2* b) {
    return a->x * b->y - a->y * b->x;
}

float vector2Dot(struct Vector2* a, struct Vector2* b) {
    return a->x * b->x + a->y * b->y;
}

void vector2Scale(struct Vector2* a, float scale, struct Vector2* out) {
    out->x = a->x * scale;
    out->y = a->y * scale;
}

void vector2Normalize(struct Vector2* a, struct Vector2* out) {
    if (a->x == 0.0f && a->y == 0.0f) {
        *out = *a;
    }

    float scale = 1.0f / sqrtf(a->x * a->x + a->y * a->y);
    out->x = a->x * scale;
    out->y = a->y * scale;
}