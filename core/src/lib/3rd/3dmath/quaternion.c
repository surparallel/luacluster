
#include <math.h>
#include <float.h>
#include "quaternion.h"
#include "mathf.h"
//https://quaternions.online/

void Radians(struct Vector3* in, struct Vector3* out) {
    out->x = in->x * DEG_TO_RAD;
    out->y = in->y * DEG_TO_RAD;
    out->z = in->z * DEG_TO_RAD;
}

void Degree(struct Vector3* in, struct Vector3* out) {
    out->x = in->x * RAD_TO_DEG;
    out->y = in->y * RAD_TO_DEG;
    out->z = in->z * RAD_TO_DEG;
}

void quatIdent(struct Quaternion* q) {
    q->x = 0.0f;
    q->y = 0.0f;
    q->z = 0.0f;
    q->w = 1.0f;
}

void quatAxisAngle(struct Vector3* axis, float angle, struct Quaternion* out) {
    float sinTheta = sinf(angle * 0.5f);
    float cosTheta = cosf(angle * 0.5f);

    out->x = axis->x * sinTheta;
    out->y = axis->y * sinTheta;
    out->z = axis->z * sinTheta;
    out->w = cosTheta;
}

float NormalizeAngle(float angle)
{
    while (angle > 360)
        angle -= 360;
    while (angle < 0)
        angle += 360;
    return angle;
}

NormalizeAngles(struct Vector3* angles, struct Vector3* out)
{
    out->x = NormalizeAngle(angles->x);
    out->y = NormalizeAngle(angles->y);
    out->z = NormalizeAngle(angles->z);
}

//https://quaternions.online/ xyz order
void quatEulerAngles(struct Vector3* angles, struct Quaternion* out) {

    struct Vector3 myangles;
    NormalizeAngles(angles, &myangles);
    Radians(&myangles, &myangles);

    struct Quaternion angle;
    struct Quaternion tmp;

    quatAxisAngle(&gRight, myangles.x, &angle);
    quatAxisAngle(&gUp, myangles.y, out);
    quatMultiply(out, &angle, &tmp);
    quatAxisAngle(&gForward, myangles.z, &angle);
    quatMultiply(&angle, &tmp, out);
}

void quatAxisComplex(struct Vector3* axis, struct Vector2* complex, struct Quaternion* out) {
    float sinTheta = 0.5f - complex->x * 0.5f;

    if (sinTheta < 0.0f) {
        sinTheta = 0.0f;
    } else {
        sinTheta = sqrtf(sinTheta);

        if (complex->y < 0.0f) {
            sinTheta = -sinTheta;
        }
    }

    float cosTheta = 0.5f + complex->x * 0.5f;

    if (cosTheta < 0.0f) {
        cosTheta = 0.0f;
    } else {
        cosTheta = sqrtf(cosTheta);
    }

    out->x = axis->x * sinTheta;
    out->y = axis->y * sinTheta;
    out->z = axis->z * sinTheta;
    out->w = cosTheta;
}

void quatConjugate(struct Quaternion* in, struct Quaternion* out) {
    out->x = -in->x;
    out->y = -in->y;
    out->z = -in->z;
    out->w = in->w;
}

//»¡¶È
void quatMultVector(struct Quaternion* q, struct Vector3* a, struct Vector3* out) {
    struct Quaternion tmp;
    struct Quaternion asQuat;
    struct Quaternion conj;
    asQuat.x = a->x;
    asQuat.y = a->y;
    asQuat.z = a->z;
    asQuat.w = 0.0f;

    quatMultiply(q, &asQuat, &tmp);
    quatConjugate(q, &conj);
    quatMultiply(&tmp, &conj, &asQuat);
    
    out->x = asQuat.x;
    out->y = asQuat.y;
    out->z = asQuat.z;
}

void quatMultiply(struct Quaternion* a, struct Quaternion* b, struct Quaternion* out) {
    out->x = a->w*b->x + a->x*b->w + a->y*b->z - a->z*b->y;
    out->y = a->w*b->y + a->y*b->w + a->z*b->x - a->x*b->z;
    out->z = a->w*b->z + a->z*b->w + a->x*b->y - a->y*b->x;
    out->w = a->w*b->w - a->x*b->x - a->y*b->y - a->z*b->z;
}

void quatToMatrix(struct Quaternion* q, float out[4][4]) {
    float xx = q->x*q->x;
    float yy = q->y*q->y;
    float zz = q->z*q->z;

    float xy = q->x*q->y;
    float yz = q->y*q->z;
    float xz = q->x*q->z;

    float xw = q->x*q->w;
    float yw = q->y*q->w;
    float zw = q->z*q->w;

    out[0][0] = 1.0f - 2.0f * (yy + zz);
    out[0][1] = 2.0f * (xy + zw);
    out[0][2] = 2.0f * (xz - yw);
    out[0][3] = 0.0f;
    out[1][0] = 2.0f * (xy - zw);
    out[1][1] = 1.0f - 2.0f * (xx + zz);
    out[1][2] = 2.0f * (yz + xw);
    out[1][3] = 0.0f;
    out[2][0] = 2.0f * (xz + yw);
    out[2][1] = 2.0f * (yz - xw);
    out[2][2] = 1.0f - 2.0f * (xx + yy);
    out[2][3] = 0.0f;
    out[3][0] = 0.0f;
    out[3][1] = 0.0f;
    out[3][2] = 0.0f;
    out[3][3] = 1.0f;
}

void quatNormalize(struct Quaternion* q, struct Quaternion* out) {
    float magSqr = q->x * q->x + q->y * q->y + q->z * q->z + q->w * q->w;

    if (magSqr <= 0.0f) {
        out->w = 1.0f;
        out->x = 0.0f;
        out->y = 0.0f;
        out->z = 0.0f;
    }
    else {
        magSqr = 1.0f / sqrtf(magSqr);

        out->x = q->x * magSqr;
        out->y = q->y * magSqr;
        out->z = q->z * magSqr;
        out->w = q->w * magSqr;
    }
}

void quatRandom(struct Quaternion* q) {
    q->x = mathfRandomFloat() - 0.5f;
    q->y = mathfRandomFloat() - 0.5f;
    q->z = mathfRandomFloat() - 0.5f;
    q->w = mathfRandomFloat() - 0.5f;
    quatNormalize(q, q);
}

//https://github.com/kermado/M3D
void quatFromToRotation(struct Vector3* fromDirection, struct Vector3* toDirection, struct Quaternion* out)
{
    struct Vector3 unitFrom;
    vector3Normalize(fromDirection, &unitFrom);
    struct Vector3 unitTo;
    vector3Normalize(toDirection, &unitTo);
    float d = vector3Dot(&unitFrom, &unitTo);

    if (d >= 1.0f)
    {
        out->w = 1.0f;
        out->x = 0.0f;
        out->y = 0.0f;
        out->z = 0.0f;
        return;
    }
    else if (d <= -1.0f)
    {
        struct Vector3 axis;
        vector3Cross(&unitFrom, &gRight, &axis);
        if (vector3MagSqrd(&axis) < 1e-6)
        {
            vector3Cross(&unitFrom, &gUp, &axis);
        }

        vector3Normalize(&axis, &axis);
        quatAxisAngle(&axis, (float)PI, out);
        return;
    }
    else
    {
        float s = sqrtf(vector3MagSqrd(&unitFrom) * vector3MagSqrd(&unitTo))
            + vector3Dot(&unitFrom, &unitTo);

        struct Vector3 v;
        vector3Cross(&unitFrom, &unitTo, &v);

        out->w = s;
        out->x = v.x;
        out->y = v.y;
        out->z = v.z;

        quatNormalize(out, out);
        return;
    }
}

void quatLookRotation(struct Vector3* forward, struct Quaternion* out)
{
    quatFromToRotation(&gForward, forward, out);
    return;
}

void quatLookRotationWithUpwards(struct Vector3* forward, struct Vector3* upwards, struct Quaternion* out)
{
    struct Quaternion q1;
    quatLookRotation(forward, &q1);

    struct Vector3 t;
    vector3Cross(forward, upwards, &t);
    if (vector3MagSqrd(&t) < 1e-6)
    {
        *out = q1;
        return;
    }

    struct Vector3 newUp;
    quatMultVector(&q1, &gUp, &newUp);

    struct Quaternion q2;
    quatFromToRotation(&newUp, upwards, &q2);
    quatMultiply(&q1, &q2, out);
    return;
}

void quatToEulerAngles(struct Quaternion* q, struct Vector3* out)
{
    float sqw = q->w * q->w;
    float sqx = q->x * q->x;
    float sqy = q->y * q->y;
    float sqz = q->z * q->z;

    out->x = atan2f(-2.f * (q->x * q->y - q->w * q->z), sqw + sqx - sqy - sqz); //-2 * (q.x * q.y - q.w * q.z),q.w* q.w + q.x * q.x - q.y * q.y - q.z * q.z,
    out->y = asinf(2.f * (q->x * q->z + q->y * q->w));//2 * (q.x * q.z + q.w * q.y),
    out->z = atan2f(-2.f * (q->y * q->z + q->w * q->x), sqw - sqx - sqy + sqz);//-2 * (q.y * q.z - q.w * q.x)  q.w* q.w - q.x * q.x - q.y * q.y + q.z * q.z,

    Degree(out, out);
    return;
};

float ClampAxis(float Angle)
{
    // returns Angle in the range (-360,360)
    Angle = fmod(Angle, 360.f);

    if (Angle < 0.f)
    {
        // shift to [0,360) range
        Angle += 360.f;
    }

    return Angle;
}

float NormalizeAxis(float Angle)
{
    // returns Angle in the range [0,360)
    Angle = ClampAxis(Angle);

    if (Angle > 180.f)
    {
        // shift to (-180,180]
        Angle -= 360.f;
    }

    return Angle;
}
