
#include "transform.h"


void transformConcat(struct BasicTransform* a, struct BasicTransform* b, struct BasicTransform* result) {
    result->scale = a->scale * b->scale;
    struct Vector3 rotatedOffset;
    quatMultVector(&a->rotation, &b->position, &rotatedOffset);
    quatMultiply(&a->rotation, &b->rotation, &result->rotation);

    result->position.x = a->position.x + rotatedOffset.x * a->scale;
    result->position.y = a->position.y + rotatedOffset.y * a->scale;
    result->position.z = a->position.z + rotatedOffset.z * a->scale;
}

void transformPoint(struct BasicTransform* trans, struct Vector3* in, struct Vector3* out) {
    quatMultVector(&trans->rotation, in, out);
    vector3Scale(out, out, trans->scale);
    out->x += trans->position.x;
    out->y += trans->position.y;
    out->z += trans->position.z;
}

void transformPointInverse(struct BasicTransform* trans, struct Vector3* in, struct Vector3* out) {
    vector3Sub(in, &trans->position, out);
    if (trans->scale != 1.0f) {
        vector3Scale(out, out, 1.0f / trans->scale);
    }
    struct Quaternion invRot;
    quatConjugate(&trans->rotation, &invRot);
    quatMultVector(&invRot, out, out);
}

void transformDirection(struct BasicTransform* trans, struct Vector3* in, struct Vector3* out) {
    quatMultVector(&trans->rotation, in, out);
}

void transformDirectionInverse(struct BasicTransform* trans, struct Vector3* in, struct Vector3* out) {
    struct Quaternion invRot;
    quatConjugate(&trans->rotation, &invRot);
    quatMultVector(&invRot, in, out);
}

void transformInvert(struct BasicTransform* in, struct BasicTransform* out) {
    out->scale = 1.0f / in->scale;
    quatConjugate(&in->rotation, &out->rotation);
    quatMultVector(&out->rotation, &in->position, &out->position);
    out->position.x *= -out->scale;
    out->position.y *= -out->scale;
    out->position.z *= -out->scale;
}

void transformToMatrix(struct BasicTransform* in, float worldScale, float mtx[4][4]) {
    quatToMatrix(&in->rotation, mtx);

    float finalScale = in->scale * worldScale;

    if (finalScale != 1.0f) {
        mtx[0][0] *= finalScale; mtx[0][1] *= finalScale; mtx[0][2] *= finalScale;
        mtx[1][0] *= finalScale; mtx[1][1] *= finalScale; mtx[1][2] *= finalScale;
        mtx[2][0] *= finalScale; mtx[2][1] *= finalScale; mtx[2][2] *= finalScale;
    }

    mtx[3][0] = in->position.x;
    mtx[3][1] = in->position.y;
    mtx[3][2] = in->position.z;
}

void transformIdentity(struct BasicTransform* trans) {
    trans->scale = 1.0f;
    trans->position = gZeroVec;
    quatIdent(&trans->rotation);
}

void transformForward(struct BasicTransform* in, struct Vector3* out) {
    quatMultVector(&in->rotation, &gForward, out);
}