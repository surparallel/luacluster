#include"plateform.h"
#include "3dmath.h"
#include "quaternion.h"
#include "vector.h"
#include "transform.h"
#include <math.h>
static double PIOver2 = 0xc.90fdaa22168cp-3;

//方向向量转欧拉角
void LookRotation(struct Vector3 fromDir, struct Vector3* eulerAngles)
{
    //AngleX = arc cos(sqrt((x^2 + z^2)/(x^2+y^2+z^2)))
    eulerAngles->x = cosf(sqrtf((fromDir.x * fromDir.x + fromDir.z * fromDir.z) / (fromDir.x * fromDir.x + fromDir.y * fromDir.y + fromDir.z * fromDir.z)));
    if (fromDir.y > 0) eulerAngles->x = 360 - eulerAngles->x;

    //AngleY = arc tan(x/z)
    eulerAngles->y = atan2(fromDir.x, fromDir.z);
    if (eulerAngles->y < 0) eulerAngles->y += 180;
    if (fromDir.x < 0) eulerAngles->y += 180;
    //AngleZ = 0
    eulerAngles->z = 0;
}

typedef float Matrix[3][3];
struct EulerAngle { float x, y, z; };

// Euler Order enum.
enum EEulerOrder
{
    ORDER_XYZ,
    ORDER_YZX,
    ORDER_ZXY,
    ORDER_ZYX,
    ORDER_YXZ,
    ORDER_XZY
};

//欧拉角返回向量
void EulerAnglesToMatrix(const struct EulerAngle* inEulerAngle, enum EEulerOrder EulerOrder, Matrix Mx)
{
    // Convert Euler Angles passed in a vector of Radians
    // into a rotation matrix.  The individual Euler Angles are
    // processed in the order requested.

    const float    Sx = sinf(inEulerAngle->x);
    const float    Sy = sinf(inEulerAngle->y);
    const float    Sz = sinf(inEulerAngle->z);
    const float    Cx = cosf(inEulerAngle->z);
    const float    Cy = cosf(inEulerAngle->y);
    const float    Cz = cosf(inEulerAngle->z);

    switch (EulerOrder)
    {
    case ORDER_XYZ:
        Mx[0][0] = Cy * Cz;
        Mx[0][1] = -Cy * Sz;
        Mx[0][2] = Sy;
        Mx[1][0] = Cz * Sx * Sy + Cx * Sz;
        Mx[1][1] = Cx * Cz - Sx * Sy * Sz;
        Mx[1][2] = -Cy * Sx;
        Mx[2][0] = -Cx * Cz * Sy + Sx * Sz;
        Mx[2][1] = Cz * Sx + Cx * Sy * Sz;
        Mx[2][2] = Cx * Cy;
        break;

    case ORDER_YZX:
        Mx[0][0] = Cy * Cz;
        Mx[0][1] = Sx * Sy - Cx * Cy * Sz;
        Mx[0][2] = Cx * Sy + Cy * Sx * Sz;
        Mx[1][0] = Sz;
        Mx[1][1] = Cx * Cz;
        Mx[1][2] = -Cz * Sx;
        Mx[2][0] = -Cz * Sy;
        Mx[2][1] = Cy * Sx + Cx * Sy * Sz;
        Mx[2][2] = Cx * Cy - Sx * Sy * Sz;
        break;

    case ORDER_ZXY:
        Mx[0][0] = Cy * Cz - Sx * Sy * Sz;
        Mx[0][1] = -Cx * Sz;
        Mx[0][2] = Cz * Sy + Cy * Sx * Sz;
        Mx[1][0] = Cz * Sx * Sy + Cy * Sz;
        Mx[1][1] = Cx * Cz;
        Mx[1][2] = -Cy * Cz * Sx + Sy * Sz;
        Mx[2][0] = -Cx * Sy;
        Mx[2][1] = Sx;
        Mx[2][2] = Cx * Cy;
        break;

    case ORDER_ZYX:
        Mx[0][0] = Cy * Cz;
        Mx[0][1] = Cz * Sx * Sy - Cx * Sz;
        Mx[0][2] = Cx * Cz * Sy + Sx * Sz;
        Mx[1][0] = Cy * Sz;
        Mx[1][1] = Cx * Cz + Sx * Sy * Sz;
        Mx[1][2] = -Cz * Sx + Cx * Sy * Sz;
        Mx[2][0] = -Sy;
        Mx[2][1] = Cy * Sx;
        Mx[2][2] = Cx * Cy;
        break;

    case ORDER_YXZ:
        Mx[0][0] = Cy * Cz + Sx * Sy * Sz;
        Mx[0][1] = Cz * Sx * Sy - Cy * Sz;
        Mx[0][2] = Cx * Sy;
        Mx[1][0] = Cx * Sz;
        Mx[1][1] = Cx * Cz;
        Mx[1][2] = -Sx;
        Mx[2][0] = -Cz * Sy + Cy * Sx * Sz;
        Mx[2][1] = Cy * Cz * Sx + Sy * Sz;
        Mx[2][2] = Cx * Cy;
        break;

    case ORDER_XZY:
        Mx[0][0] = Cy * Cz;
        Mx[0][1] = -Sz;
        Mx[0][2] = Cz * Sy;
        Mx[1][0] = Sx * Sy + Cx * Cy * Sz;
        Mx[1][1] = Cx * Cz;
        Mx[1][2] = -Cy * Sx + Cx * Sy * Sz;
        Mx[2][0] = -Cx * Sy + Cy * Sx * Sz;
        Mx[2][1] = Cz * Sx;
        Mx[2][2] = Cx * Cy + Sx * Sy * Sz;
        break;
    }
}

// 由旋转矩阵创建四元数
void CQuaternion(const float m[4][4], struct Quaternion* out)
{
    float tr, s, q[4];
    int i, j, k;

    int nxt[3] = { 1, 2, 0 };
    // 计算矩阵轨迹
    tr = m[1][1] + m[2][2] + m[3][3];

    // 检查矩阵轨迹是正还是负
    if (tr > 0.0f)
    {
        s = sqrt(tr + 1.0f);
        out->w = s / 2.0f;
        s = 0.5f / s;
        out->x = (m[2][3] - m[3][2]) * s;
        out->y = (m[3][1] - m[1][3]) * s;
        out->z = (m[1][2] - m[2][1]) * s;
    }
    else
    {
        // 轨迹是负
        // 寻找m11 m22 m33中的最大分量
        i = 0;
        if (m[1][1] > m[0][0]) i = 1;
        if (m[2][2] > m[i][i]) i = 2;
        j = nxt[i];
        k = nxt[j];

        s = sqrt((m[i][i] - (m[j][j] + m[k][k])) + 1.0f);
        q[i] = s * 0.5f;
        if (s != 0.0f) s = 0.5f / s;
        q[3] = (m[j][k] - m[k][j]) * s;
        q[j] = (m[i][j] - m[j][i]) * s;
        q[k] = (m[i][k] - m[k][i]) * s;
        out->x = q[0];
        out->y = q[1];
        out->z = q[2];
        out->w = q[3];
    }
};

// 由欧拉角创建四元数
void CAngleToQuaternion(const struct Vector3* angle, struct Quaternion* out)
{
    float cx = cos(angle->x / 2);
    float sx = sin(angle->x / 2);
    float cy = cos(angle->y / 2);
    float sy = sin(angle->y / 2);
    float cz = cos(angle->z / 2);
    float sz = sin(angle->z / 2);

    out->w = cx * cy * cz + sx * sy * sz;
    out->x = sx * cy * cz - cx * sy * sz;
    out->y = cx * sy * cz + sx * cy * sz;
    out->z = cx * cy * sz - sx * sy * cz;
};

// 给定角度和轴创建四元数
void CAnxiToQuaternion(struct Vector3 anxi, const float angle, struct Quaternion* out)
{
    struct Vector3 t;
    t.x = anxi.x;
    t.y = anxi.y;
    t.z = anxi.z;
    vector3Normalize(&t, &t);
    float cosa = cos(angle);
    float sina = sin(angle);
    out->w = cosa;
    out->x = sina * t.x;
    out->y = sina * t.y;
    out->z = sina * t.z;
};

// 由旋转四元数推导出矩阵
void GetMatrixLH(struct Quaternion* in, float ret[4][4])
{
    float xx = in->x * in->x;
    float yy = in->y * in->y;
    float zz = in->z * in->z;
    float xy = in->x * in->y;
    float wz = in->w * in->z;
    float wy = in->w * in->y;
    float xz = in->x * in->z;
    float yz = in->y * in->z;
    float wx = in->w * in->x;

    ret[1][1] = 1.0f - 2 * (yy + zz);
    ret[1][2] = 2 * (xy - wz);
    ret[1][3] = 2 * (wy + xz);
    ret[1][4] = 0.0f;

    ret[2][1] = 2 * (xy + wz);
    ret[2][2] = 1.0f - 2 * (xx + zz);
    ret[2][3] = 2 * (yz - wx);
    ret[2][4] = 0.0f;

    ret[3][1] = 2 * (xy - wy);
    ret[3][2] = 2 * (yz + wx);
    ret[3][3] = 1.0f - 2 * (xx + yy);
    ret[3][4] = 0.0f;

    ret[4][1] = 0.0f;
    ret[4][2] = 0.0f;
    ret[4][3] = 0.0f;
    ret[4][4] = 1.0f;
};
void GetMatrixRH(struct Quaternion* in, float ret[4][4])
{
    float xx = in->x * in->x;
    float yy = in->y * in->y;
    float zz = in->z * in->z;
    float xy = in->x * in->y;
    float wz = -in->w * in->z;
    float wy = -in->w * in->y;
    float xz = in->x * in->z;
    float yz = in->y * in->z;
    float wx = -in->w * in->x;

    ret[1][1] = 1.0f - 2 * (yy + zz);
    ret[1][2] = 2 * (xy - wz);
    ret[1][3] = 2 * (wy + xz);
    ret[1][4] = 0.0f;

    ret[2][1] = 2 * (xy + wz);
    ret[2][2] = 1.0f - 2 * (xx + zz);
    ret[2][3] = 2 * (yz - wx);
    ret[2][4] = 0.0f;

    ret[3][1] = 2 * (xy - wy);
    ret[3][2] = 2 * (yz + wx);
    ret[3][3] = 1.0f - 2 * (xx + yy);
    ret[3][4] = 0.0f;

    ret[4][1] = 0.0f;
    ret[4][2] = 0.0f;
    ret[4][3] = 0.0f;
    ret[4][4] = 1.0f;
};

// 由四元数返回欧拉角
inline struct Vector3 GetEulerAngle(struct Quaternion* in)
{
    struct Vector3 ret;

    float test = in->y * in->z + in->x * in->w;
    if (test > 0.4999f)
    {
        ret.z = 2.0f * atan2(in->y, in->w);
        ret.y = PIOver2;
        ret.x = 0.0f;
        return ret;
    }
    if (test < -0.4999f)
    {
        ret.z = 2.0f * atan2(in->y, in->w);
        ret.y = -PIOver2;
        ret.x = 0.0f;
        return ret;
    }
    float sqx = in->x * in->x;
    float sqy = in->y * in->y;
    float sqz = in->z * in->z;
    ret.z = atan2(2.0f * in->z * in->w - 2.0f * in->y * in->x, 1.0f - 2.0f * sqz - 2.0f * sqx);
    ret.y = asin(2.0f * test);
    ret.x = atan2(2.0f * in->y * in->w - 2.0f * in->z * in->x, 1.0f - 2.0f * sqy - 2.0f * sqx);

    return ret;
};

void GetEulerAngle2(struct Quaternion* q, struct Vector3* EulerAngle)
{
    float sqw = q->w * q->w;
    float sqx = q->x * q->x;
    float sqy = q->y * q->y;
    float sqz = q->z * q->z;

    EulerAngle->z = atan2f(2.f * (q->x * q->y + q->z * q->w), sqx - sqy - sqz + sqw);
    EulerAngle->y = asinf(-2.f * (q->x * q->z - q->y * q->w));
    EulerAngle->x = atan2f(2.f * (q->y * q->z + q->w * q->x), -sqx - sqy + sqz + sqw);

}

void do3dlibTest() {
    /*
    struct Quaternion qout;
    struct Vector3 qin1, qin2;

    qin1.x = 0;
    qin1.y = 45;
    qin1.z = 0;

    ToQ(&qin1, &qout);
    FromQ2(&qout, &qin2);

   // for (size_t x = 0; x <= 360; x++)
    {
        for (int y = -360; y <= 360; y++)
        {
           // for (size_t z = 0; z <= 360; z++)
            {
                qin1.x = 0;
                qin1.y = y;
                qin1.z = 0;

                ToQ(&qin1, &qout);
                FromQ2(&qout, &qin2);

                if (round(qin2.x) != qin1.x || round(qin2.y) != qin1.y || round(qin2.z) != qin1.z) {
                    if (abs(round(qin2.x)) + qin1.x == 360 || abs(round(qin2.y)) + qin1.y == 360 || abs(round(qin2.z)) + qin1.z == 360)
                        continue;
                    printf("error x1:%f x2:%f y1:%f y2:%f z1:%f z2:%f\n", qin1.x, qin2.x, qin1.y, qin2.y, qin1.z, qin2.z);
                }
            }
        }
    }

*/

    struct Quaternion rotation, rotation1;
    struct Vector3 Euler, Euler2, Euler3;
    Euler.x = 0.f;
    Euler.y = 45.f;
    Euler.z = 0.f;
    quatEulerAngles(&Euler, &rotation1);
    quatToEulerAngles(&rotation1, &Euler2);

    struct Vector3 forwd;
    forwd.x = 1;
    forwd.y = 0;
    forwd.z = 1;
    quatLookRotation(&forwd, &rotation);
    quatToEulerAngles(&rotation, &Euler2);

    for (float x = -1; x <= 1; x++) {
        for (float z = -1; z <= 1; z++) {
            struct Vector3 forwd;
            forwd.x = x;
            forwd.y = 0;
            forwd.z = z;
            quatLookRotation(&forwd, &rotation);
            quatToEulerAngles(&rotation, &Euler2);

            printf("in.x:%f in.z:%f q.x:%f q.y:%f q.z:%f q.w:%f out.x:%f out.y:%f out.z:%f\n", forwd.x, forwd.z, rotation.x, rotation.y, rotation.z, rotation.w, Euler2.x, Euler2.y, Euler2.z);
        }
    }

    getc(stdin);
}