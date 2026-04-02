#include "D3dutil.h"
#include <stdio.h>
#include <math.h>

void D3DUtil_InitSurfaceDesc(_DDSURFACEDESC2* ddsd, unsigned int dwFlags, unsigned int dwCaps) {
    memset(ddsd, 0, sizeof(_DDSURFACEDESC2));
    ddsd->dwSize = sizeof(_DDSURFACEDESC2);
    ddsd->dwFlags = dwFlags;
    ddsd->ddsCaps.dwCaps = dwCaps;
    ddsd->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
}

void D3DUtil_InitViewport(_D3DVIEWPORT7* vp, unsigned int dwWidth, unsigned int dwHeight) {
    memset(vp, 0, sizeof(_D3DVIEWPORT7));
    vp->dwWidth = dwWidth;
    vp->dwHeight = dwHeight;
    vp->dvMaxZ = 1.0f;
}

int D3DUtil_SetProjectionMatrix(_D3DMATRIX* mat, float fFOV, float fAspect, float fNearPlane, float fFarPlane) {
    float fFarNear = fFarPlane - fNearPlane;
    if (fabs(fFarNear) < 0.01f) return -1;
    
    float fHalfFOV = fFOV * 0.5f;
    if (fabs(sin(fHalfFOV)) < 0.01f) return -1;
    
    float w = (cos(fHalfFOV) / sin(fHalfFOV)) * fAspect;
    float h = cos(fHalfFOV) / sin(fHalfFOV);
    float Q = fFarPlane / fFarNear;

    memset(mat, 0, sizeof(_D3DMATRIX));
    mat->_11 = w;
    mat->_22 = h;
    mat->_33 = Q;
    mat->_34 = 1.0f;
    mat->_43 = -(Q * fNearPlane);
    return 0;
}

int _DbgOut(char* strFile, unsigned int dwLine, int hr, char* strMsg) {
    char buffer[256];
    sprintf(buffer, "%s(%u): %s (hr=%08x)\n", strFile, dwLine, strMsg, static_cast<unsigned int>(hr));
    OutputDebugStringA(buffer);
    return hr;
}
