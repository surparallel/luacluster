

struct Matrix4 {
    union {
        float el_4[4][4];
        float el_16[16];
    };
};

#define MATRIX4F_IDENTITY(NAME) \
   struct Matrix4 NAME = { \
       1.f, 0.f, 0.f, 0.f, \
       0.f, 1.f, 0.f, 0.f, \
       0.f, 0.f, 1.f, 0.f, \
       0.f, 0.f, 0.f, 1.f };

void matrix4LookAt(struct Vector3* eye, struct Vector3* target, struct Vector3* up, struct Matrix4* out);