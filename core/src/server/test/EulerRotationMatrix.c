#include <math.h>
#include "EulerRotationMatrix.h"

#define Value(r, c) matrix[r * 3 + c]
#define PI 3.141592657

static int
XYZ(float* matrix, float* eular) {

    return 0x001;
}

static int
XZY(float* matrix, float* eular) {

    return 0x001;
}

//RzRxRy
static int
YXZ(float* matrix, float* eular) {

    double a1 = 0.0f, a2 = 0.0f, b1 = 0.0f, b2 = 0.0f, c1 = 0.0f, c2 = 0.0f;

    if (Value(2, 1) != 1 && Value(2, 1) != -1) {
        a1 = asin(Value(2, 1));
        a2 = PI + a1;

        b1 = atan2(-Value(2, 0) / cos(a1), Value(2, 2) / cos(a1));
        b2 = atan2(-Value(2, 0) / cos(a2), Value(2, 2) / cos(a2));

        c1 = atan2(-Value(0, 1) / cos(a1), Value(1, 1) / cos(a1));
        c2 = atan2(-Value(0, 1) / cos(a2), Value(1, 1) / cos(a2));

        eular[0] = (float)a1;
        eular[1] = (float)b1;
        eular[2] = (float)c1;
        eular[3] = (float)a2;
        eular[4] = (float)b2;
        eular[5] = (float)c2;
    }
    else {
        b1 = 0;
        if (Value(1, 2) == -1) {//-PI/2
            a1 = -PI / 2;
            c1 = b1 + atan2(Value(1, 0), Value(0, 0));
        }
        else {//PI/2
            a1 = PI / 2;
            c1 = -b1 + atan2(Value(1, 0), Value(0, 0));
        }

        eular[0] = (float)a1;
        eular[1] = (float)b1;
        eular[2] = (float)c1;
    }

    return 0;
}

static int
YZX(float* matrix, float* eular) {

    double a1 = 0.0f, a2 = 0.0f, b1 = 0.0f, b2 = 0.0f, c1 = 0.0f, c2 = 0.0f;

    if (Value(0, 1) != 1 && Value(0, 1) != -1) {
        c1 = -asin(Value(0, 1));
        c2 = PI - c1;

        b1 = atan2(Value(0, 2) / cos(c1), Value(0, 0) / cos(c1));
        b2 = atan2(Value(0, 2) / cos(c2), Value(0, 0) / cos(c2));

        a1 = atan2(Value(2, 1) / cos(c1), Value(1, 1) / cos(c1));
        a2 = atan2(Value(2, 1) / cos(c2), Value(1, 1) / cos(c2));

        eular[0] = (float)a1;
        eular[1] = (float)b1;
        eular[2] = (float)c1;
        eular[3] = (float)a2;
        eular[4] = (float)b2;
        eular[5] = (float)c2;
    }
    else {
        b1 = 0;
        if (Value(0, 1) == -1) {//PI/2
            c1 = PI / 2;
            a1 = b1 + atan2(Value(2, 0), Value(1, 0));
        }
        else {//-PI/2
            c1 = -PI / 2;
            a1 = -b1 + atan2(-Value(2, 0), -Value(1, 0));
        }

        eular[0] = (float)a1;
        eular[1] = (float)b1;
        eular[2] = (float)c1;
    }

    return 0;
}

//RyRxRz
static int
ZXY(float* matrix, float* eular) {

    double a1 = 0.0f, a2 = 0.0f, b1 = 0.0f, b2 = 0.0f, c1 = 0.0f, c2 = 0.0f;

    if (Value(1, 2) != 1 && Value(1, 2) != -1) {
        a1 = -asin(Value(1, 2));
        a2 = PI - a1;

        b1 = atan2(Value(0, 2) / cos(a1), Value(2, 2) / cos(a1));
        b2 = atan2(Value(0, 2) / cos(a2), Value(2, 2) / cos(a2));

        c1 = atan2(Value(1, 0) / cos(a1), Value(1, 1) / cos(a1));
        c2 = atan2(Value(1, 0) / cos(a2), Value(1, 1) / cos(a2));

        eular[0] = (float)a1;
        eular[1] = (float)b1;
        eular[2] = (float)c1;
        eular[3] = (float)a2;
        eular[4] = (float)b2;
        eular[5] = (float)c2;
    }
    else {
        b1 = 0;
        if (Value(1, 2) == -1) {//PI/2
            a1 = PI / 2;
            c1 = b1 + atan2(Value(0, 1), Value(2, 1));
        }
        else {//-PI/2
            a1 = -PI / 2;
            c1 = -b1 + atan2(-Value(0, 1), -Value(2, 1));
        }

        eular[0] = (float)a1;
        eular[1] = (float)b1;
        eular[2] = (float)c1;
    }

    return 0;
}

static int
ZYX(float* matrix, float* eular) {

    double a1 = 0.0f, a2 = 0.0f, b1 = 0.0f, b2 = 0.0f, c1 = 0.0f, c2 = 0.0f;
    int num = 0;

    if (Value(0, 2) != 1 && Value(0, 2) != -1) {
        b1 = asin(Value(0, 2));
        b2 = PI - b1;

        a1 = atan2(-Value(1, 2) / cos(b1), Value(2, 2) / cos(b1));
        a2 = atan2(-Value(1, 2) / cos(b2), Value(2, 2) / cos(b2));

        c1 = atan2(-Value(0, 1) / cos(b1), Value(0, 0) / cos(b1));
        c2 = atan2(-Value(0, 1) / cos(b2), Value(0, 0) / cos(b2));

        num = 2;

        eular[0] = (float)a1;
        eular[1] = (float)b1;
        eular[2] = (float)c1;
        eular[3] = (float)a2;
        eular[4] = (float)b2;
        eular[5] = (float)c2;
    }
    else {
        c1 = 0;
        if (Value(0, 2) == -1) {//-PI/2
            b1 = -PI / 2;
            a1 = c1 + atan2(Value(2, 1), Value(1, 1));
        }
        else {//PI/2
            b1 = PI / 2;
            a1 = -c1 + atan2(Value(2, 1), Value(1, 1));
        }

        num = 1;
        eular[0] = (float)a1;
        eular[1] = (float)b1;
        eular[2] = (float)c1;
    }

    return num;
}

typedef int(*RM2E)(float* matrix, float* eular);

const RM2E fRM2E[ORDER_NUM] = { &XYZ, &XZY, &YXZ, &YZX, &ZXY, &ZYX };

//this solution is based on paper: Computing Euler angles from a rotation matrix, Gregory G. Slabaugh
int
RotationMatrix2Euler(float* matrix, float* eular, ROTATE_ORDER order) {

    return fRM2E[order](matrix, eular);
}

static void
Product(float* a, float* b, float* o) {

    int r, c, i;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            for (i = 0; i < 3; i++) {
                o[r * 3 + c] += a[r * 3 + i] * b[i * 3 + c];
            }
        }
    }
}

int
Euler2RotationMatrix(float* eular, float* matrix, ROTATE_ORDER order) {

    float Rx[3 * 3] = { 1.0f         ,  0.0f                ,  0.0f                ,
                        0.0f         ,  (float)cos(eular[0]), (float)-sin(eular[0]),
                        0.0f         ,  (float)sin(eular[0]), (float)cos(eular[0]) };
    float Ry[3 * 3] = { (float)cos(eular[1]) ,  0.0f         ,  (float)sin(eular[1]),
                        0.0f                 ,  1.0f         ,  0.0f                ,
                        (float)-sin(eular[1]),  0.0f         ,  (float)cos(eular[1]) };
    float Rz[3 * 3] = { (float)cos(eular[2]),  (float)-sin(eular[2]),  0.0f         ,
                        (float)sin(eular[2]),  (float)cos(eular[2]) ,  0.0f         ,
                        0.0f                ,  0.0f                 ,  1.0f };
    float mid[3 * 3] = { 0.0f };

    int i;

    for (i = 0; i < 9; i++)
        matrix[i] = 0;

    switch (order)
    {
    case ORDER_XYZ:
        Product(Rz, Ry, mid);
        Product(mid, Rx, matrix);
        break;
    case ORDER_XZY:
        Product(Ry, Rz, mid);
        Product(mid, Rx, matrix);
        break;
    case ORDER_YXZ:
        Product(Rz, Rx, mid);
        Product(mid, Ry, matrix);
        break;
    case ORDER_YZX:
        Product(Rx, Rz, mid);
        Product(mid, Ry, matrix);
        break;
    case ORDER_ZXY:
        Product(Ry, Rx, mid);
        Product(mid, Rz, matrix);
        break;
    case ORDER_ZYX:
        Product(Rx, Ry, mid);
        Product(mid, Rz, matrix);
        break;
    default:
        break;
    }
    return 0;
}