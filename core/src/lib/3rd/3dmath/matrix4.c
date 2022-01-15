#include <math.h>
#include <float.h>
#include "matrix4.h"
#include "vector.h"
#include "mathf.h"
#include "quaternion.h"


float matrix4Determinant(struct Matrix4* in) {

	const float n11 = in->el_16[0], n12 = in->el_16[4], n13 = in->el_16[8], n14 = in->el_16[12];
	const float n21 = in->el_16[1], n22 = in->el_16[5], n23 = in->el_16[9], n24 = in->el_16[13];
	const float n31 = in->el_16[2], n32 = in->el_16[6], n33 = in->el_16[10], n34 = in->el_16[14];
	const float n41 = in->el_16[3], n42 = in->el_16[7], n43 = in->el_16[11], n44 = in->el_16[15];

	//TODO: make this more efficient
	//( based on http://www.euclideanspace.com/maths/algebra/matrix/functions/inverse/fourD/index.htm )

	return (
		n41 * (
			+n14 * n23 * n32
			- n13 * n24 * n32
			- n14 * n22 * n33
			+ n12 * n24 * n33
			+ n13 * n22 * n34
			- n12 * n23 * n34
			) +
		n42 * (
			+n11 * n23 * n34
			- n11 * n24 * n33
			+ n14 * n21 * n33
			- n13 * n21 * n34
			+ n13 * n24 * n31
			- n14 * n23 * n31
			) +
		n43 * (
			+n11 * n24 * n32
			- n11 * n22 * n34
			- n14 * n21 * n32
			+ n12 * n21 * n34
			+ n14 * n22 * n31
			- n12 * n24 * n31
			) +
		n44 * (
			-n13 * n22 * n31
			- n11 * n23 * n32
			+ n11 * n22 * n33
			+ n13 * n21 * n32
			- n12 * n21 * n33
			+ n12 * n23 * n31
			)

		);
}

void matrix4LookAt(struct Vector3 *eye, struct Vector3* target, struct Vector3* up, struct Matrix4* out) {

	MATRIX4F_IDENTITY(const_el);
	*out = const_el;

	struct Vector3 _z;
	vector3Sub(eye, target, &_z);

	if (vector3MagSqrd(&_z) == 0) {

		// eye and target are in the same position
		_z.z = 1;
	}

	vector3Normalize(&_z, &_z);

	struct Vector3 _x;
	vector3Cross(up, &_z, &_x);

	if (vector3MagSqrd(&_x) == 0) {

		// up and z are parallel
		if (fabs(up->z) == 1) {

			_z.x += 0.0001f;

		}
		else {

			_z.z += 0.0001f;

		}
		vector3Normalize(&_z, &_z);
		vector3Cross(up, &_z, &_x);

	}

	vector3Normalize(&_x, &_x);

	struct Vector3 _y;
	vector3Cross(&_z, &_x, &_y);

	out->el_16[0] = _x.x; out->el_16[4] = _y.x; out->el_16[8] = _z.x;
	out->el_16[1] = _x.y; out->el_16[5] = _y.y; out->el_16[9] = _z.y;
	out->el_16[2] = _x.z; out->el_16[6] = _y.z; out->el_16[10] = _z.z;
}

void matrix4Compose(struct Vector3* position, struct Quaternion* quaternion, struct Vector3* scale, struct Matrix4* out) {

	MATRIX4F_IDENTITY(const_el);
	*out = const_el;

	const float x = quaternion->x, y = quaternion->y, z = quaternion->z, w = quaternion->w;
	const float x2 = x + x, y2 = y + y, z2 = z + z;
	const float xx = x * x2, xy = x * y2, xz = x * z2;
	const float yy = y * y2, yz = y * z2, zz = z * z2;
	const float wx = w * x2, wy = w * y2, wz = w * z2;

	const float sx = scale->x, sy = scale->y, sz = scale->z;

	out->el_16[0] = (1 - (yy + zz)) * sx;
	out->el_16[1] = (xy + wz) * sx;
	out->el_16[2] = (xz - wy) * sx;
	out->el_16[3] = 0;

	out->el_16[4] = (xy - wz) * sy;
	out->el_16[5] = (1 - (xx + zz)) * sy;
	out->el_16[6] = (yz + wx) * sy;
	out->el_16[7] = 0;

	out->el_16[8] = (xz + wy) * sz;
	out->el_16[9] = (yz - wx) * sz;
	out->el_16[10] = (1 - (xx + yy)) * sz;
	out->el_16[11] = 0;

	out->el_16[12] = position->x;
	out->el_16[13] = position->y;
	out->el_16[14] = position->z;
	out->el_16[15] = 1;
}

void decompose(struct Matrix4* in, struct Vector3* position, struct Quaternion* quaternion, struct Vector3* scale) {

	struct Vector3 _v1 = { in->el_16[0], in->el_16[1], in->el_16[2] };
	float sx = vector3MagLength(&_v1);
	struct Vector3 _v2 = { in->el_16[4], in->el_16[5], in->el_16[6] };
	float sy = vector3MagLength(&_v2);
	struct Vector3 _v3 = { in->el_16[8], in->el_16[9], in->el_16[10] };
	float sz = vector3MagLength(&_v3);

	// if determine is negative, we need to invert one scale
	const float det = matrix4Determinant(in);
	if (det < 0) sx = -sx;

	position->x = in->el_16[12];
	position->y = in->el_16[13];
	position->z = in->el_16[14];

	// scale the rotation part
	struct Matrix4 _m1 = *in;

	const float invSX = 1 / sx;
	const float invSY = 1 / sy;
	const float invSZ = 1 / sz;

	_m1.el_16[0] *= invSX;
	_m1.el_16[1] *= invSX;
	_m1.el_16[2] *= invSX;

	_m1.el_16[4] *= invSY;
	_m1.el_16[5] *= invSY;
	_m1.el_16[6] *= invSY;

	_m1.el_16[8] *= invSZ;
	_m1.el_16[9] *= invSZ;
	_m1.el_16[10] *= invSZ;

	quatFromRotationMatrix(&_m1, quaternion);

	scale->x = sx;
	scale->y = sy;
	scale->z = sz;

}

void matrix4RotationFromQuaternion(struct Quaternion* q, struct Matrix4* out) {
	matrix4Compose(&gZeroVec, q, &gOneVec, out);
}