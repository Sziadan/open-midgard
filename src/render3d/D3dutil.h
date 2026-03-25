#pragma once
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

void D3DUtil_InitSurfaceDesc(_DDSURFACEDESC2* ddsd, unsigned int dwFlags = 0, unsigned int dwCaps = 0);
void D3DUtil_InitViewport(_D3DVIEWPORT7* vp, unsigned int dwWidth, unsigned int dwHeight);
int D3DUtil_SetProjectionMatrix(_D3DMATRIX* mat, float fFOV, float fAspect, float fNearPlane, float fFarPlane);
int _DbgOut(char* strFile, unsigned int dwLine, int hr, char* strMsg);
