#pragma once

#include "Types.h"

#include <cstring>

#if RO_PLATFORM_WINDOWS

#include <ddraw.h>
#include <d3d.h>

#else

struct GUID {
    std::uint32_t Data1;
    std::uint16_t Data2;
    std::uint16_t Data3;
    unsigned char Data4[8];
};

inline bool operator==(const GUID& lhs, const GUID& rhs)
{
    return lhs.Data1 == rhs.Data1
        && lhs.Data2 == rhs.Data2
        && lhs.Data3 == rhs.Data3
        && std::memcmp(lhs.Data4, rhs.Data4, sizeof(lhs.Data4)) == 0;
}

using REFIID = const GUID&;
using HRESULT = long;
using D3DCOLOR = DWORD;

constexpr HRESULT S_OK = 0;
constexpr HRESULT E_NOTIMPL = static_cast<HRESULT>(0x80004001L);
constexpr HRESULT E_NOINTERFACE = static_cast<HRESULT>(0x80004002L);
constexpr HRESULT E_POINTER = static_cast<HRESULT>(0x80004003L);

constexpr GUID IID_IUnknown = { 0x00000000u, 0x0000u, 0x0000u, { 0xC0u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x46u } };

#ifndef RO_WINDOWS_COMPAT_HAS_PALETTEENTRY
struct PALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
};
#endif

struct IUnknown {
    virtual HRESULT QueryInterface(REFIID riid, void** outObject) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;

protected:
    virtual ~IUnknown() = default;
};

struct D3DVECTOR {
    float x;
    float y;
    float z;
};

struct D3DCOLORVALUE {
    float r;
    float g;
    float b;
    float a;
};

enum D3DLIGHTTYPE : DWORD {
    D3DLIGHT_POINT = 1,
    D3DLIGHT_SPOT = 2,
    D3DLIGHT_DIRECTIONAL = 3,
    D3DLIGHT_PARALLELPOINT = 4,
    D3DLIGHT_GLSPOT = 5,
};

struct D3DLIGHT7 {
    D3DLIGHTTYPE dltType;
    D3DCOLORVALUE dcvDiffuse;
    D3DCOLORVALUE dcvSpecular;
    D3DCOLORVALUE dcvAmbient;
    D3DVECTOR dvPosition;
    D3DVECTOR dvDirection;
    float dvRange;
    float dvFalloff;
    float dvAttenuation0;
    float dvAttenuation1;
    float dvAttenuation2;
    float dvTheta;
    float dvPhi;
};

struct IDirectDrawSurface7 : IUnknown {
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual HRESULT Lock(RECT*, struct _DDSURFACEDESC2*, DWORD, HANDLE) = 0;
    virtual HRESULT Unlock(LPVOID) = 0;
};
struct IDirectDrawClipper;
struct IDirectDraw7;
struct IDirect3D7;
struct IDirect3DDevice7 : IUnknown {
    virtual HRESULT SetLight(DWORD, D3DLIGHT7*) = 0;
    virtual HRESULT LightEnable(DWORD, BOOL) = 0;
};
struct IDirect3DVertexBuffer7;

using LPDDPIXELFORMAT = struct _DDPIXELFORMAT*;
using VOID = void;

enum D3DPRIMITIVETYPE : DWORD {
    D3DPT_POINTLIST = 1,
    D3DPT_LINELIST = 2,
    D3DPT_LINESTRIP = 3,
    D3DPT_TRIANGLELIST = 4,
    D3DPT_TRIANGLESTRIP = 5,
    D3DPT_TRIANGLEFAN = 6,
};

enum D3DCULL : DWORD {
    D3DCULL_NONE = 1,
    D3DCULL_CW = 2,
    D3DCULL_CCW = 3,
};

enum D3DBLEND : DWORD {
    D3DBLEND_ZERO = 1,
    D3DBLEND_ONE = 2,
    D3DBLEND_SRCCOLOR = 3,
    D3DBLEND_INVSRCCOLOR = 4,
    D3DBLEND_SRCALPHA = 5,
    D3DBLEND_INVSRCALPHA = 6,
    D3DBLEND_DESTALPHA = 7,
    D3DBLEND_INVDESTALPHA = 8,
    D3DBLEND_DESTCOLOR = 9,
    D3DBLEND_INVDESTCOLOR = 10,
};

using D3DRENDERSTATETYPE = DWORD;
using D3DTEXTURESTAGESTATETYPE = DWORD;
using D3DTRANSFORMSTATETYPE = DWORD;

struct _D3DMATRIX {
    float _11, _12, _13, _14;
    float _21, _22, _23, _24;
    float _31, _32, _33, _34;
    float _41, _42, _43, _44;
};

using D3DMATRIX = _D3DMATRIX;

struct _D3DVIEWPORT7 {
    DWORD dwX;
    DWORD dwY;
    DWORD dwWidth;
    DWORD dwHeight;
    float dvMinZ;
    float dvMaxZ;
};

struct _DDPIXELFORMAT {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwFourCC;
    union {
        DWORD dwRGBBitCount;
        DWORD dwYUVBitCount;
        DWORD dwZBufferBitDepth;
        DWORD dwAlphaBitDepth;
        DWORD dwLuminanceBitCount;
        DWORD dwBumpBitCount;
        DWORD dwPrivateFormatBitCount;
    };
    union {
        DWORD dwRBitMask;
        DWORD dwYBitMask;
        DWORD dwStencilBitDepth;
        DWORD dwLuminanceBitMask;
        DWORD dwBumpDuBitMask;
        DWORD dwOperations;
    };
    union {
        DWORD dwGBitMask;
        DWORD dwUBitMask;
        DWORD dwZBitMask;
        DWORD dwBumpDvBitMask;
        struct {
            WORD wFlipMSTypes;
            WORD wBltMSTypes;
        } MultiSampleCaps;
    };
    union {
        DWORD dwBBitMask;
        DWORD dwVBitMask;
        DWORD dwStencilBitMask;
        DWORD dwBumpLuminanceBitMask;
    };
    union {
        DWORD dwRGBAlphaBitMask;
        DWORD dwYUVAlphaBitMask;
        DWORD dwLuminanceAlphaBitMask;
        DWORD dwRGBZBitMask;
        DWORD dwYUVZBitMask;
    };
};

using DDPIXELFORMAT = _DDPIXELFORMAT;

struct _DDSCAPS2 {
    DWORD dwCaps;
    DWORD dwCaps2;
    DWORD dwCaps3;
    union {
        DWORD dwCaps4;
        DWORD dwVolumeDepth;
    } DUMMYUNIONNAMEN;
};

struct _DDCOLORKEY {
    DWORD dwColorSpaceLowValue;
    DWORD dwColorSpaceHighValue;
};

struct _DDSURFACEDESC2 {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwHeight;
    DWORD dwWidth;
    union {
        LONG lPitch;
        DWORD dwLinearSize;
    } DUMMYUNIONNAMEN1;
    union {
        DWORD dwBackBufferCount;
        DWORD dwDepth;
    } DUMMYUNIONNAMEN5;
    union {
        DWORD dwMipMapCount;
        DWORD dwRefreshRate;
        DWORD dwSrcVBHandle;
    } DUMMYUNIONNAMEN2;
    DWORD dwAlphaBitDepth;
    DWORD dwReserved;
    LPVOID lpSurface;
    union {
        _DDCOLORKEY ddckCKDestOverlay;
        DWORD dwEmptyFaceColor;
    } DUMMYUNIONNAMEN3;
    _DDPIXELFORMAT ddpfPixelFormat;
    _DDSCAPS2 ddsCaps;
    DWORD dwTextureStage;
};

struct D3DDEVICEDESC7 {
    DWORD dwDevCaps;
    DWORD dwMinTextureWidth;
    DWORD dwMinTextureHeight;
    DWORD dwMaxTextureWidth;
    DWORD dwMaxTextureHeight;
    DWORD dwMaxTextureAspectRatio;
};

using DDSURFACEDESC2 = _DDSURFACEDESC2;

constexpr DWORD D3DFVF_XYZRHW = 0x00000400u;
constexpr DWORD D3DFVF_DIFFUSE = 0x00000040u;
constexpr DWORD D3DFVF_SPECULAR = 0x00000080u;
constexpr DWORD D3DFVF_TEX1 = 0x00000100u;
constexpr DWORD D3DFVF_TEX2 = 0x00000200u;
constexpr DWORD D3DFVF_TLVERTEX = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1;
constexpr DWORD DDLOCK_WAIT = 0x00000001u;
constexpr DWORD DDLOCK_READONLY = 0x00000010u;
constexpr DWORD D3DTFP_LINEAR = 2u;

constexpr D3DTRANSFORMSTATETYPE D3DTRANSFORMSTATE_VIEW = 2u;
constexpr D3DTRANSFORMSTATETYPE D3DTRANSFORMSTATE_PROJECTION = 3u;

constexpr D3DTEXTURESTAGESTATETYPE D3DTSS_COLOROP = 1u;
constexpr D3DTEXTURESTAGESTATETYPE D3DTSS_COLORARG1 = 2u;
constexpr D3DTEXTURESTAGESTATETYPE D3DTSS_COLORARG2 = 3u;
constexpr D3DTEXTURESTAGESTATETYPE D3DTSS_ALPHAOP = 4u;
constexpr D3DTEXTURESTAGESTATETYPE D3DTSS_ALPHAARG1 = 5u;
constexpr D3DTEXTURESTAGESTATETYPE D3DTSS_ALPHAARG2 = 6u;
constexpr D3DTEXTURESTAGESTATETYPE D3DTSS_TEXCOORDINDEX = 11u;
constexpr D3DTEXTURESTAGESTATETYPE D3DTSS_MINFILTER = 16u;
constexpr D3DTEXTURESTAGESTATETYPE D3DTSS_MAGFILTER = 17u;
constexpr D3DTEXTURESTAGESTATETYPE D3DTSS_MIPFILTER = 18u;

constexpr DWORD D3DTA_DIFFUSE = 0x00000000u;
constexpr DWORD D3DTA_CURRENT = 0x00000001u;
constexpr DWORD D3DTA_TEXTURE = 0x00000002u;
constexpr DWORD D3DTA_ALPHAREPLICATE = 0x00000020u;

constexpr DWORD D3DTOP_DISABLE = 1u;
constexpr DWORD D3DTOP_SELECTARG1 = 2u;
constexpr DWORD D3DTOP_MODULATE = 4u;

constexpr DWORD D3DTFN_POINT = 1u;
constexpr DWORD D3DTFN_LINEAR = 2u;
constexpr DWORD D3DTFG_POINT = 1u;
constexpr DWORD D3DTFG_LINEAR = 2u;
constexpr DWORD D3DTFP_POINT = 1u;

constexpr D3DRENDERSTATETYPE D3DRENDERSTATE_ZENABLE = 7u;
constexpr D3DRENDERSTATETYPE D3DRENDERSTATE_ZWRITEENABLE = 14u;
constexpr D3DRENDERSTATETYPE D3DRENDERSTATE_ALPHATESTENABLE = 15u;
constexpr D3DRENDERSTATETYPE D3DRENDERSTATE_SRCBLEND = 19u;
constexpr D3DRENDERSTATETYPE D3DRENDERSTATE_DESTBLEND = 20u;
constexpr D3DRENDERSTATETYPE D3DRENDERSTATE_CULLMODE = 22u;
constexpr D3DRENDERSTATETYPE D3DRENDERSTATE_ALPHAREF = 24u;
constexpr D3DRENDERSTATETYPE D3DRENDERSTATE_ALPHABLENDENABLE = 27u;
constexpr D3DRENDERSTATETYPE D3DRENDERSTATE_COLORKEYENABLE = 41u;
constexpr D3DRENDERSTATETYPE D3DRENDERSTATE_AMBIENT = 26u;
constexpr D3DRENDERSTATETYPE D3DRENDERSTATE_LIGHTING = 137u;

constexpr DWORD D3DZB_FALSE = 0u;
constexpr DWORD D3DZB_TRUE = 1u;

#ifndef FAILED
#define FAILED(hr) (static_cast<HRESULT>(hr) < 0)
#endif

#endif