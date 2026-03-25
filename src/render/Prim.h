#pragma once

#include "Types.h"
#include <cmath>

struct w3d_quaternion {
    float qx, qy, qz, qw;
};

struct C3dOBB {
    vector3d center;
    vector3d u, v, w;
    vector3d halfSize;
};

struct lineSegment3d {
    vector3d org;
    vector3d dir;
    float limit;
};

// Trigonometric Tables
extern float g_sinTable[361];
extern float g_cosTable[361];
extern float g_radTable[361];

void CreateTrigonometricTable();
float GetSin(int degree);
float GetCos(int degree);
float GetRadian(int degree);

// Matrix Operations (extension of the matrix struct in Types.h)
void MatrixIdentity(matrix& m);
void MatrixMult(matrix& res, const matrix& a, const matrix& b);
void MatrixInverse(matrix& res, const matrix& src);
void MatrixMakeScale(matrix& m, float x, float y, float z);
void MatrixAppendScale(matrix& m, float x, float y, float z);
void MatrixAppendTranslate(matrix& m, const vector3d& v);
void MatrixAppendXRotation(matrix& m, float angle);
void MatrixAppendYRotation(matrix& m, float angle);
void MatrixAppendZRotation(matrix& m, float angle);

// Intersection Tests
bool CheckRayTriIntersect(const ray3d& ray, const vector3d& v0, const vector3d& v1, const vector3d& v2, float* u, float* v, float* t);
bool CheckLineSegmentTriIntersect(const lineSegment3d& ray, const vector3d& v0, const vector3d& v1, const vector3d& v2, float* u, float* v, float* t);
bool CheckRayOBBIntersect(const ray3d& ray, const C3dOBB& obb);
bool CheckLineSegmentOBBIntersect(const lineSegment3d& ray, const C3dOBB& obb);

// Quaternion Operations
void QuaternionToMatrix(matrix& m, const w3d_quaternion& q);
void MatrixToQuaternion(w3d_quaternion& q, const matrix& m);
float QuaternionDot(const w3d_quaternion& p, const w3d_quaternion& q);
w3d_quaternion QuaternionInterpolate(float t, const w3d_quaternion& p, const w3d_quaternion& q);
