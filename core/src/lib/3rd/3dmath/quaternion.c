
#include <math.h>
#include <float.h>
#include "matrix4.h"
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

void NormalizeAngles(struct Vector3* angles, struct Vector3* out)
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

void quatFromRotationMatrix(struct Matrix4* m, struct Quaternion* q) {

    // http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/index.htm

    // assumes the upper 3x3 of m is a pure rotation matrix (i.e, unscaled)

    float m11 = m->el_16[0], m12 = m->el_16[4], m13 = m->el_16[8],
        m21 = m->el_16[1], m22 = m->el_16[5], m23 = m->el_16[9],
        m31 = m->el_16[2], m32 = m->el_16[6], m33 = m->el_16[10],

        trace = m11 + m22 + m33;

    if (trace > 0) {

        const float s = 0.5 / sqrtf(trace + 1.0);

        q->w = 0.25 / s;
        q->x = (m32 - m23) * s;
        q->y = (m13 - m31) * s;
        q->z = (m21 - m12) * s;

    }
    else if (m11 > m22 && m11 > m33) {

        const float s = 2.0 * sqrtf(1.0 + m11 - m22 - m33);

        q->w = (m32 - m23) / s;
        q->x = 0.25 * s;
        q->y = (m12 + m21) / s;
        q->z = (m13 + m31) / s;

    }
    else if (m22 > m33) {

        const float s = 2.0 * sqrtf(1.0 + m22 - m11 - m33);

        q->w = (m13 - m31) / s;
        q->x = (m12 + m21) / s;
        q->y = 0.25 * s;
        q->z = (m23 + m32) / s;

    }
    else {

        const float s = 2.0 * sqrtf(1.0 + m33 - m11 - m22);

        q->w = (m21 - m12) / s;
        q->x = (m13 + m31) / s;
        q->y = (m23 + m32) / s;
        q->z = 0.25 * s;

    }
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
    quatFromToRotation(&gRight, forward, out);
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
