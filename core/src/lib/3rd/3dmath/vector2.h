
#ifndef _MATH_VECTOR2_H
#define _MATH_VECTOR2_H

struct Vector2 {
    union {
        struct {
            float x, y;
        };
        float el[2];
    };
};

extern struct Vector2 gRight2;
extern struct Vector2 gUp2;

void vector2ComplexMul(struct Vector2* a, struct Vector2* b, struct Vector2* out);
void vector2RotateTowards(struct Vector2* from, struct Vector2* towards, struct Vector2* max, struct Vector2* out);
float vector2Cross(struct Vector2* a, struct Vector2* b);
float vector2Dot(struct Vector2* a, struct Vector2* b);
void vector2Scale(struct Vector2* a, float scale, struct Vector2* out);
void vector2Normalize(struct Vector2* a, struct Vector2* out);

#endif