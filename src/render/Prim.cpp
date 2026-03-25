#include "Prim.h"
#include <cstring>
#include <algorithm>

// --- Trigonometric Tables ---

float g_sinTable[361];
float g_cosTable[361];
float g_radTable[361];

void CreateTrigonometricTable() {
    for (int i = 0; i <= 360; ++i) {
        float rad = (float)i * 3.14159265f / 180.0f;
        g_radTable[i] = rad;
        g_sinTable[i] = std::sin(rad);
        g_cosTable[i] = std::cos(rad);
    }
}

float GetSin(int degree) {
    degree %= 360;
    if (degree < 0) degree += 360;
    return g_sinTable[degree];
}

float GetCos(int degree) {
    degree %= 360;
    if (degree < 0) degree += 360;
    return g_cosTable[degree];
}

float GetRadian(int degree) {
    degree %= 360;
    if (degree < 0) degree += 360;
    return g_radTable[degree];
}

// --- Matrix Operations ---

const matrix g_IdentityMatrix = {
    {{1, 0, 0, 0},
     {0, 1, 0, 0},
     {0, 0, 1, 0},
     {0, 0, 0, 1}}
};

void MatrixIdentity(matrix& m) {
    m = g_IdentityMatrix;
}

void MatrixMult(matrix& res, const matrix& a, const matrix& b) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            res.m[i][j] = a.m[i][0] * b.m[0][j] +
                          a.m[i][1] * b.m[1][j] +
                          a.m[i][2] * b.m[2][j] +
                          a.m[i][3] * b.m[3][j];
        }
    }
}

void MatrixInverse(matrix& res, const matrix& src) {
    // 4x3 inverse for affine transforms (common in RO)
    float fDetInv = 1.0f / (src.m[0][0] * (src.m[1][1] * src.m[2][2] - src.m[1][2] * src.m[2][1]) -
                            src.m[0][1] * (src.m[1][0] * src.m[2][2] - src.m[1][2] * src.m[2][0]) +
                            src.m[0][2] * (src.m[1][0] * src.m[2][1] - src.m[1][1] * src.m[2][0]));

    res.m[0][0] = (src.m[1][1] * src.m[2][2] - src.m[1][2] * src.m[2][1]) * fDetInv;
    res.m[0][1] = -(src.m[0][1] * src.m[2][2] - src.m[0][2] * src.m[2][1]) * fDetInv;
    res.m[0][2] = (src.m[0][1] * src.m[1][2] - src.m[0][2] * src.m[1][1]) * fDetInv;
    res.m[0][3] = 0;

    res.m[1][0] = -(src.m[1][0] * src.m[2][2] - src.m[1][2] * src.m[2][0]) * fDetInv;
    res.m[1][1] = (src.m[0][0] * src.m[2][2] - src.m[0][2] * src.m[2][0]) * fDetInv;
    res.m[1][2] = -(src.m[0][0] * src.m[1][2] - src.m[0][2] * src.m[1][0]) * fDetInv;
    res.m[1][3] = 0;

    res.m[2][0] = (src.m[1][0] * src.m[2][1] - src.m[1][1] * src.m[2][0]) * fDetInv;
    res.m[2][1] = -(src.m[0][0] * src.m[2][1] - src.m[0][1] * src.m[2][0]) * fDetInv;
    res.m[2][2] = (src.m[0][0] * src.m[1][1] - src.m[0][1] * src.m[1][0]) * fDetInv;
    res.m[2][3] = 0;

    res.m[3][0] = -(src.m[3][0] * res.m[0][0] + src.m[3][1] * res.m[1][0] + src.m[3][2] * res.m[2][0]);
    res.m[3][1] = -(src.m[3][0] * res.m[0][1] + src.m[3][1] * res.m[1][1] + src.m[3][2] * res.m[2][1]);
    res.m[3][2] = -(src.m[3][0] * res.m[0][2] + src.m[3][1] * res.m[1][2] + src.m[3][2] * res.m[2][2]);
    res.m[3][3] = 1;
}

void MatrixMakeScale(matrix& m, float x, float y, float z) {
    MatrixIdentity(m);
    m.m[0][0] = x;
    m.m[1][1] = y;
    m.m[2][2] = z;
}

void MatrixAppendScale(matrix& m, float x, float y, float z) {
    matrix tm;
    MatrixMakeScale(tm, x, y, z);
    matrix res;
    MatrixMult(res, m, tm);
    m = res;
}

void MatrixAppendTranslate(matrix& m, const vector3d& v) {
    matrix tm;
    MatrixIdentity(tm);
    tm.m[3][0] = v.x;
    tm.m[3][1] = v.y;
    tm.m[3][2] = v.z;
    matrix res;
    MatrixMult(res, m, tm);
    m = res;
}

void MatrixAppendXRotation(matrix& m, float angle) {
    matrix tm;
    MatrixIdentity(tm);
    float s = std::sin(angle), c = std::cos(angle);
    tm.m[1][1] = c; tm.m[1][2] = s;
    tm.m[2][1] = -s; tm.m[2][2] = c;
    matrix res;
    MatrixMult(res, m, tm);
    m = res;
}

void MatrixAppendYRotation(matrix& m, float angle) {
    matrix tm;
    MatrixIdentity(tm);
    float s = std::sin(angle), c = std::cos(angle);
    tm.m[0][0] = c; tm.m[0][2] = -s;
    tm.m[2][0] = s; tm.m[2][2] = c;
    matrix res;
    MatrixMult(res, m, tm);
    m = res;
}

void MatrixAppendZRotation(matrix& m, float angle) {
    matrix tm;
    MatrixIdentity(tm);
    float s = std::sin(angle), c = std::cos(angle);
    tm.m[0][0] = c; tm.m[0][1] = s;
    tm.m[1][0] = -s; tm.m[1][1] = c;
    matrix res;
    MatrixMult(res, m, tm);
    m = res;
}

// --- Intersection Tests --- (Standard implementations based on Moller-Trumbore / Slab method)

bool CheckRayTriIntersect(const ray3d& ray, const vector3d& v0, const vector3d& v1, const vector3d& v2, float* u, float* v, float* t) {
    vector3d edge1 = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
    vector3d edge2 = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
    
    // Cross product ray.dir x edge2
    vector3d pvec = {ray.d.y * edge2.z - ray.d.z * edge2.y,
                     ray.d.z * edge2.x - ray.d.x * edge2.z,
                     ray.d.x * edge2.y - ray.d.y * edge2.x};
    
    float det = edge1.x * pvec.x + edge1.y * pvec.y + edge1.z * pvec.z;
    if (std::abs(det) < 1e-6f) return false;
    
    float invDet = 1.0f / det;
    vector3d tvec = {ray.p.x - v0.x, ray.p.y - v0.y, ray.p.z - v0.z};
    *u = (tvec.x * pvec.x + tvec.y * pvec.y + tvec.z * pvec.z) * invDet;
    if (*u < 0.0f || *u > 1.0f) return false;
    
    // Cross product tvec x edge1
    vector3d qvec = {tvec.y * edge1.z - tvec.z * edge1.y,
                     tvec.z * edge1.x - tvec.x * edge1.z,
                     tvec.x * edge1.y - tvec.y * edge1.x};
    
    *v = (ray.d.x * qvec.x + ray.d.y * qvec.y + ray.d.z * qvec.z) * invDet;
    if (*v < 0.0f || *u + *v > 1.0f) return false;
    
    *t = (edge2.x * qvec.x + edge2.y * qvec.y + edge2.z * qvec.z) * invDet;
    return (*t >= 0.0f);
}
