#include "RenderDevice.h"

#include "Device.h"
#include "D3dutil.h"
#include "DebugLog.h"
#include "render/Renderer.h"
#include "res/Texture.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

namespace {

template <typename T>
void SafeRelease(T*& value)
{
    if (value) {
        value->Release();
        value = nullptr;
    }
}

unsigned int CountTrailingZeros(unsigned int mask)
{
    if (mask == 0u) {
        return 0u;
    }

    unsigned int shift = 0u;
    while ((mask & 1u) == 0u) {
        mask >>= 1u;
        ++shift;
    }
    return shift;
}

unsigned int CountBits(unsigned int mask)
{
    unsigned int bits = 0u;
    while (mask != 0u) {
        bits += mask & 1u;
        mask >>= 1u;
    }
    return bits;
}

unsigned int PackChannel(unsigned int value, unsigned int mask)
{
    if (mask == 0u) {
        return 0u;
    }

    const unsigned int shift = CountTrailingZeros(mask);
    const unsigned int bits = CountBits(mask);
    if (bits == 0u) {
        return 0u;
    }

    const unsigned int maxValue = (1u << bits) - 1u;
    const unsigned int scaled = (value * maxValue + 127u) / 255u;
    return (scaled << shift) & mask;
}

unsigned int ConvertArgbToSurfacePixel(unsigned int argb, const DDPIXELFORMAT& pf)
{
    const unsigned int alpha = (argb >> 24) & 0xFFu;
    const unsigned int red = (argb >> 16) & 0xFFu;
    const unsigned int green = (argb >> 8) & 0xFFu;
    const unsigned int blue = argb & 0xFFu;

    if (pf.dwRGBBitCount == 32
        && pf.dwRBitMask == 0x00FF0000u
        && pf.dwGBitMask == 0x0000FF00u
        && pf.dwBBitMask == 0x000000FFu
        && pf.dwRGBAlphaBitMask == 0xFF000000u) {
        return argb;
    }

    return PackChannel(alpha, pf.dwRGBAlphaBitMask)
        | PackChannel(red, pf.dwRBitMask)
        | PackChannel(green, pf.dwGBitMask)
        | PackChannel(blue, pf.dwBBitMask);
}

unsigned int GetSurfaceColorKey(const DDPIXELFORMAT& pf)
{
    return pf.dwRBitMask | pf.dwBBitMask;
}

void ReleaseTextureMembers(CTexture* texture)
{
    if (!texture) {
        return;
    }

    if (texture->m_pddsSurface) {
        texture->m_pddsSurface->Release();
        texture->m_pddsSurface = nullptr;
    }

    if (texture->m_backendTextureView) {
        texture->m_backendTextureView->Release();
        texture->m_backendTextureView = nullptr;
    }

    if (texture->m_backendTextureObject) {
        texture->m_backendTextureObject->Release();
        texture->m_backendTextureObject = nullptr;
    }
}

void WritePackedPixel(unsigned char* dst, unsigned int bytesPerPixel, unsigned int value)
{
    switch (bytesPerPixel) {
    case 4:
        *reinterpret_cast<unsigned int*>(dst) = value;
        break;
    case 3:
        dst[0] = static_cast<unsigned char>(value & 0xFFu);
        dst[1] = static_cast<unsigned char>((value >> 8) & 0xFFu);
        dst[2] = static_cast<unsigned char>((value >> 16) & 0xFFu);
        break;
    case 2:
        *reinterpret_cast<unsigned short*>(dst) = static_cast<unsigned short>(value & 0xFFFFu);
        break;
    case 1:
        *dst = static_cast<unsigned char>(value & 0xFFu);
        break;
    default:
        break;
    }
}

constexpr DWORD kLmFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX2;

struct D3D11DrawConstants {
    float screenWidth;
    float screenHeight;
    float alphaRef;
    unsigned int flags;
};

enum D3D11DrawFlags : unsigned int {
    D3D11DrawFlag_Texture0Enabled = 1u << 0,
    D3D11DrawFlag_Texture1Enabled = 1u << 1,
    D3D11DrawFlag_AlphaTestEnabled = 1u << 2,
    D3D11DrawFlag_ColorKeyEnabled = 1u << 3,
    D3D11DrawFlag_Stage0AlphaUseTexture = 1u << 4,
    D3D11DrawFlag_Stage0AlphaModulate = 1u << 5,
    D3D11DrawFlag_Stage1LightmapAlpha = 1u << 6,
};

D3D11_BLEND ConvertBlendFactor(D3DBLEND blend)
{
    switch (blend) {
    case D3DBLEND_ZERO:
        return D3D11_BLEND_ZERO;
    case D3DBLEND_ONE:
        return D3D11_BLEND_ONE;
    case D3DBLEND_SRCCOLOR:
        return D3D11_BLEND_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR:
        return D3D11_BLEND_INV_SRC_COLOR;
    case D3DBLEND_SRCALPHA:
        return D3D11_BLEND_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA:
        return D3D11_BLEND_INV_SRC_ALPHA;
    case D3DBLEND_DESTALPHA:
        return D3D11_BLEND_DEST_ALPHA;
    case D3DBLEND_INVDESTALPHA:
        return D3D11_BLEND_INV_DEST_ALPHA;
    case D3DBLEND_DESTCOLOR:
        return D3D11_BLEND_DEST_COLOR;
    case D3DBLEND_INVDESTCOLOR:
        return D3D11_BLEND_INV_DEST_COLOR;
    default:
        return D3D11_BLEND_ONE;
    }
}

D3D11_PRIMITIVE_TOPOLOGY ConvertPrimitiveTopology(D3DPRIMITIVETYPE primitiveType)
{
    switch (primitiveType) {
    case D3DPT_POINTLIST:
        return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
    case D3DPT_LINELIST:
        return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    case D3DPT_LINESTRIP:
        return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case D3DPT_TRIANGLELIST:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case D3DPT_TRIANGLESTRIP:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    default:
        return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }
}

const char* GetD3D11ShaderSource()
{
    return R"(
cbuffer DrawConstants : register(b0)
{
    float g_screenWidth;
    float g_screenHeight;
    float g_alphaRef;
    uint g_flags;
};

Texture2D g_texture0 : register(t0);
Texture2D g_texture1 : register(t1);
SamplerState g_sampler0 : register(s0);

struct VSInputTL {
    float4 pos : POSITION0;
    float4 color : COLOR0;
    float2 uv0 : TEXCOORD0;
};

struct VSInputLM {
    float4 pos : POSITION0;
    float4 color : COLOR0;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

struct VSOutput {
    float4 pos : SV_Position;
    float4 color : COLOR0;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

VSOutput VSMainTL(VSInputTL input)
{
    VSOutput output;
    float rhw = max(input.pos.w, 1e-6f);
    float clipW = 1.0f / rhw;
    float ndcX = (input.pos.x / max(g_screenWidth, 1.0f)) * 2.0f - 1.0f;
    float ndcY = 1.0f - (input.pos.y / max(g_screenHeight, 1.0f)) * 2.0f;
    output.pos = float4(ndcX * clipW, ndcY * clipW, input.pos.z * clipW, clipW);
    output.color = input.color;
    output.uv0 = input.uv0;
    output.uv1 = float2(0.0f, 0.0f);
    return output;
}

VSOutput VSMainLM(VSInputLM input)
{
    VSOutput output;
    float rhw = max(input.pos.w, 1e-6f);
    float clipW = 1.0f / rhw;
    float ndcX = (input.pos.x / max(g_screenWidth, 1.0f)) * 2.0f - 1.0f;
    float ndcY = 1.0f - (input.pos.y / max(g_screenHeight, 1.0f)) * 2.0f;
    output.pos = float4(ndcX * clipW, ndcY * clipW, input.pos.z * clipW, clipW);
    output.color = input.color;
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float4 color = input.color;
    float tex0Alpha = 1.0f;

    if ((g_flags & 1u) != 0u) {
        float4 tex0 = g_texture0.Sample(g_sampler0, input.uv0);
        color.rgb *= tex0.rgb;
        tex0Alpha = tex0.a;
        if ((g_flags & 16u) != 0u) {
            color.a = tex0Alpha;
        } else if ((g_flags & 32u) != 0u) {
            color.a *= tex0Alpha;
        }
    }

    if ((g_flags & 64u) != 0u) {
        float lightmapAlpha = g_texture1.Sample(g_sampler0, input.uv1).a;
        color.rgb *= lightmapAlpha.xxx;
    }

    if ((g_flags & 8u) != 0u && (g_flags & 1u) != 0u && tex0Alpha <= 0.0f) {
        discard;
    }

    if ((g_flags & 4u) != 0u && color.a <= g_alphaRef) {
        discard;
    }

    return color;
}
)";
}

bool CompileShaderBlob(const char* source, const char* entryPoint, const char* target, ID3DBlob** outBlob)
{
    if (!source || !entryPoint || !target || !outBlob) {
        return false;
    }

    *outBlob = nullptr;
    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3DCompile(source,
        std::strlen(source),
        nullptr,
        nullptr,
        nullptr,
        entryPoint,
        target,
        0,
        0,
        &shaderBlob,
        &errorBlob);
    if (FAILED(hr) || !shaderBlob) {
        if (errorBlob && errorBlob->GetBufferPointer()) {
            DbgLog("[Render] D3D11 shader compile failed (%s/%s): %s\n",
                entryPoint,
                target,
                static_cast<const char*>(errorBlob->GetBufferPointer()));
        } else {
            DbgLog("[Render] D3D11 shader compile failed (%s/%s) hr=0x%08X.\n",
                entryPoint,
                target,
                static_cast<unsigned int>(hr));
        }
        SafeRelease(shaderBlob);
        SafeRelease(errorBlob);
        return false;
    }

    SafeRelease(errorBlob);
    *outBlob = shaderBlob;
    return true;
}

class LegacyRenderDevice final : public IRenderDevice {
public:
    LegacyRenderDevice()
        : m_hwnd(nullptr), m_renderWidth(0), m_renderHeight(0)
    {
        m_bootstrap.backend = RenderBackendType::LegacyDirect3D7;
        m_bootstrap.initHr = -1;
    }

    RenderBackendType GetBackendType() const override
    {
        return RenderBackendType::LegacyDirect3D7;
    }

    bool Initialize(HWND hwnd, RenderBackendBootstrapResult* outResult) override
    {
        Shutdown();
        m_hwnd = hwnd;
        GUID deviceCandidates[] = {
            IID_IDirect3DTnLHalDevice,
            IID_IDirect3DHALDevice,
            IID_IDirect3DRGBDevice
        };

        m_bootstrap.backend = RenderBackendType::LegacyDirect3D7;
        m_bootstrap.initHr = -1;
        for (GUID& deviceGuid : deviceCandidates) {
            m_bootstrap.initHr = g_3dDevice.Init(hwnd, nullptr, &deviceGuid, nullptr, 0);
            if (m_bootstrap.initHr >= 0) {
                break;
            }
        }

        RefreshRenderSize();
        if (outResult) {
            *outResult = m_bootstrap;
        }
        if (m_bootstrap.initHr >= 0) {
            DbgLog("[Render] Initialized backend '%s'.\n", GetRenderBackendName(m_bootstrap.backend));
        }
        return m_bootstrap.initHr >= 0;
    }

    void Shutdown() override
    {
        g_3dDevice.DestroyObjects();
        m_renderWidth = 0;
        m_renderHeight = 0;
    }

    void RefreshRenderSize() override
    {
        if (!m_hwnd) {
            m_renderWidth = 0;
            m_renderHeight = 0;
            return;
        }

        RECT clientRect{};
        GetClientRect(m_hwnd, &clientRect);
        m_renderWidth = (std::max)(1L, clientRect.right - clientRect.left);
        m_renderHeight = (std::max)(1L, clientRect.bottom - clientRect.top);
    }

    int GetRenderWidth() const override
    {
        return m_renderWidth;
    }

    int GetRenderHeight() const override
    {
        return m_renderHeight;
    }

    HWND GetWindowHandle() const override
    {
        return m_hwnd;
    }

    IDirect3DDevice7* GetLegacyDevice() const override
    {
        return g_3dDevice.m_pd3dDevice;
    }

    int ClearColor(unsigned int color) override
    {
        return g_3dDevice.Clear(color);
    }

    int ClearDepth() override
    {
        return g_3dDevice.ClearZBuffer();
    }

    int Present(bool vertSync) override
    {
        return g_3dDevice.ShowFrame(vertSync);
    }

    bool AcquireBackBufferDC(HDC* outDc) override
    {
        if (!outDc) {
            return false;
        }

        *outDc = nullptr;
        IDirectDrawSurface7* backBuffer = g_3dDevice.m_pddsBackBuffer;
        if (!backBuffer) {
            return false;
        }

        HDC dc = nullptr;
        if (FAILED(backBuffer->GetDC(&dc)) || !dc) {
            return false;
        }

        *outDc = dc;
        return true;
    }

    void ReleaseBackBufferDC(HDC dc) override
    {
        if (!dc) {
            return;
        }

        IDirectDrawSurface7* backBuffer = g_3dDevice.m_pddsBackBuffer;
        if (backBuffer) {
            backBuffer->ReleaseDC(dc);
        }
    }

    bool BeginScene() override
    {
        IDirect3DDevice7* device = g_3dDevice.m_pd3dDevice;
        return device && SUCCEEDED(device->BeginScene());
    }

    void EndScene() override
    {
        IDirect3DDevice7* device = g_3dDevice.m_pd3dDevice;
        if (device) {
            device->EndScene();
        }
    }

    void SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) override
    {
        IDirect3DDevice7* device = g_3dDevice.m_pd3dDevice;
        if (device && matrix) {
            device->SetTransform(state, const_cast<D3DMATRIX*>(matrix));
        }
    }

    void SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override
    {
        IDirect3DDevice7* device = g_3dDevice.m_pd3dDevice;
        if (device) {
            device->SetRenderState(state, value);
        }
    }

    void SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override
    {
        IDirect3DDevice7* device = g_3dDevice.m_pd3dDevice;
        if (device) {
            device->SetTextureStageState(stage, type, value);
        }
    }

    void BindTexture(DWORD stage, CTexture* texture) override
    {
        IDirect3DDevice7* device = g_3dDevice.m_pd3dDevice;
        if (!device) {
            return;
        }

        IDirectDrawSurface7* surface = texture ? texture->m_pddsSurface : nullptr;
        device->SetTexture(stage, surface);
    }

    void DrawPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat,
        const void* vertices, DWORD vertexCount, DWORD flags) override
    {
        IDirect3DDevice7* device = g_3dDevice.m_pd3dDevice;
        if (device && vertices && vertexCount > 0) {
            device->DrawPrimitive(primitiveType, vertexFormat, const_cast<void*>(vertices), vertexCount, flags);
        }
    }

    void DrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat,
        const void* vertices, DWORD vertexCount, const unsigned short* indices,
        DWORD indexCount, DWORD flags) override
    {
        IDirect3DDevice7* device = g_3dDevice.m_pd3dDevice;
        if (device && vertices && vertexCount > 0 && indices && indexCount > 0) {
            device->DrawIndexedPrimitive(primitiveType, vertexFormat, const_cast<void*>(vertices), vertexCount,
                const_cast<unsigned short*>(indices), indexCount, flags);
        }
    }

    void AdjustTextureSize(unsigned int* width, unsigned int* height) override
    {
        if (!width || !height) {
            return;
        }
        g_3dDevice.AdjustTextureSize(width, height);
    }

    void ReleaseTextureResource(CTexture* texture) override
    {
        ReleaseTextureMembers(texture);
    }

    bool CreateTextureResource(CTexture* texture, unsigned int requestedWidth, unsigned int requestedHeight,
        int pixelFormat, unsigned int* outSurfaceWidth, unsigned int* outSurfaceHeight) override
    {
        (void)pixelFormat;
        if (!texture || !g_3dDevice.m_pDD) {
            return false;
        }

        ReleaseTextureMembers(texture);

        unsigned int surfaceWidth = requestedWidth;
        unsigned int surfaceHeight = requestedHeight;
        AdjustTextureSize(&surfaceWidth, &surfaceHeight);

        DDSURFACEDESC2 ddsd{};
        auto initDesc = [&](DWORD caps) {
            std::memset(&ddsd, 0, sizeof(ddsd));
            D3DUtil_InitSurfaceDesc(&ddsd, DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT, caps);
            ddsd.dwWidth = surfaceWidth;
            ddsd.dwHeight = surfaceHeight;
            ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
            ddsd.ddpfPixelFormat.dwRGBBitCount = 32;
            ddsd.ddpfPixelFormat.dwRBitMask = 0x00FF0000;
            ddsd.ddpfPixelFormat.dwGBitMask = 0x0000FF00;
            ddsd.ddpfPixelFormat.dwBBitMask = 0x000000FF;
            ddsd.ddpfPixelFormat.dwRGBAlphaBitMask = 0xFF000000;
        };

        IDirectDrawSurface7* surface = nullptr;
        const DWORD preferredCaps = DDSCAPS_TEXTURE | (g_3dDevice.m_dwDeviceMemType ? g_3dDevice.m_dwDeviceMemType : DDSCAPS_SYSTEMMEMORY);
        initDesc(preferredCaps);
        HRESULT createHr = g_3dDevice.m_pDD->CreateSurface(&ddsd, &surface, nullptr);
        if (createHr != DD_OK && (preferredCaps & DDSCAPS_VIDEOMEMORY)) {
            initDesc(DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY);
            createHr = g_3dDevice.m_pDD->CreateSurface(&ddsd, &surface, nullptr);
        }

        if (createHr != DD_OK || !surface) {
            return false;
        }

        DDCOLORKEY colorKey{};
        colorKey.dwColorSpaceLowValue = GetSurfaceColorKey(ddsd.ddpfPixelFormat);
        colorKey.dwColorSpaceHighValue = colorKey.dwColorSpaceLowValue;
        surface->SetColorKey(DDCKEY_SRCBLT, &colorKey);

        if (outSurfaceWidth) {
            *outSurfaceWidth = surfaceWidth;
        }
        if (outSurfaceHeight) {
            *outSurfaceHeight = surfaceHeight;
        }
        texture->m_pddsSurface = surface;
        return true;
    }

    bool UpdateTextureResource(CTexture* texture, int x, int y, int w, int h,
        const unsigned int* data, bool skipColorKey, int pitch) override
    {
        IDirectDrawSurface7* surface = texture ? texture->m_pddsSurface : nullptr;
        if (!surface || !data || w <= 0 || h <= 0) {
            return false;
        }

        DDSURFACEDESC2 ddsd{};
        ddsd.dwSize = sizeof(ddsd);
        if (surface->Lock(nullptr, &ddsd, DDLOCK_WAIT, nullptr) != DD_OK) {
            return false;
        }

        unsigned char* dstBase = static_cast<unsigned char*>(ddsd.lpSurface);
        const unsigned int bytesPerPixel = (ddsd.ddpfPixelFormat.dwRGBBitCount + 7u) / 8u;
        const int srcPitch = pitch > 0 ? pitch : w * static_cast<int>(sizeof(unsigned int));
        const unsigned int colorKey = GetSurfaceColorKey(ddsd.ddpfPixelFormat);
        for (int row = 0; row < h; ++row) {
            unsigned char* dstRow = dstBase + (y + row) * static_cast<int>(ddsd.lPitch) + x * static_cast<int>(bytesPerPixel);
            const unsigned int* srcRow = reinterpret_cast<const unsigned int*>(reinterpret_cast<const unsigned char*>(data) + static_cast<size_t>(row) * static_cast<size_t>(srcPitch));
            for (int col = 0; col < w; ++col) {
                const unsigned int srcPixel = srcRow[col];
                unsigned int packedPixel = ConvertArgbToSurfacePixel(srcPixel, ddsd.ddpfPixelFormat);
                if (!skipColorKey && (srcPixel & 0x00FFFFFFu) == 0x00FF00FFu) {
                    packedPixel = colorKey;
                }
                WritePackedPixel(dstRow + static_cast<size_t>(col) * bytesPerPixel, bytesPerPixel, packedPixel);
            }
        }

        surface->Unlock(nullptr);
        return true;
    }

private:
    HWND m_hwnd;
    int m_renderWidth;
    int m_renderHeight;
    RenderBackendBootstrapResult m_bootstrap;
};

class D3D11RenderDevice final : public IRenderDevice {
public:
    D3D11RenderDevice()
        : m_hwnd(nullptr), m_renderWidth(0), m_renderHeight(0),
          m_swapChain(nullptr), m_device(nullptr), m_context(nullptr),
          m_renderTargetView(nullptr), m_depthStencilTexture(nullptr), m_depthStencilView(nullptr),
          m_captureTexture(nullptr),
          m_vertexShaderTl(nullptr), m_vertexShaderLm(nullptr), m_pixelShader(nullptr),
          m_inputLayoutTl(nullptr), m_inputLayoutLm(nullptr), m_constantBuffer(nullptr),
          m_vertexBuffer(nullptr), m_vertexBufferSize(0), m_indexBuffer(nullptr), m_indexBufferSize(0),
          m_samplerState(nullptr), m_alphaRef(207), m_alphaTestEnable(TRUE),
          m_alphaBlendEnable(FALSE), m_depthEnable(TRUE), m_depthWriteEnable(TRUE),
          m_cullMode(D3DCULL_NONE), m_colorKeyEnable(TRUE), m_srcBlend(D3DBLEND_SRCALPHA),
          m_destBlend(D3DBLEND_INVSRCALPHA),
          m_captureDc(nullptr), m_captureBitmap(nullptr), m_captureBits(nullptr), m_captureWidth(0), m_captureHeight(0)
    {
        ResetTextureStageStates();
        m_boundTextures[0] = nullptr;
        m_boundTextures[1] = nullptr;
    }

    RenderBackendType GetBackendType() const override
    {
        return RenderBackendType::Direct3D11;
    }

    bool Initialize(HWND hwnd, RenderBackendBootstrapResult* outResult) override
    {
        Shutdown();
        m_hwnd = hwnd;

        DXGI_SWAP_CHAIN_DESC swapChainDesc{};
        swapChainDesc.BufferCount = 1;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = hwnd;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        const D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };

        const UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createFlags,
            featureLevels,
            static_cast<UINT>(std::size(featureLevels)),
            D3D11_SDK_VERSION,
            &swapChainDesc,
            &m_swapChain,
            &m_device,
            &featureLevel,
            &m_context);
        if (FAILED(hr)) {
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                createFlags,
                featureLevels,
                static_cast<UINT>(std::size(featureLevels)),
                D3D11_SDK_VERSION,
                &swapChainDesc,
                &m_swapChain,
                &m_device,
                &featureLevel,
                &m_context);
        }

        RenderBackendBootstrapResult result{};
        result.backend = RenderBackendType::Direct3D11;
        result.initHr = static_cast<int>(hr);
        if (FAILED(hr)) {
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }

        if (!RefreshRenderTarget() || !CreatePipelineResources()) {
            result.initHr = -1;
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }

        RefreshRenderSize();
        DbgLog("[Render] Initialized backend '%s' with feature level 0x%04X.\n",
            GetRenderBackendName(result.backend),
            static_cast<unsigned int>(featureLevel));

        if (outResult) {
            *outResult = result;
        }
        return true;
    }

    void Shutdown() override
    {
        ReleaseCachedStates();
        ReleaseCaptureResources();
        SafeRelease(m_samplerState);
        SafeRelease(m_indexBuffer);
        m_indexBufferSize = 0;
        SafeRelease(m_vertexBuffer);
        m_vertexBufferSize = 0;
        SafeRelease(m_constantBuffer);
        SafeRelease(m_inputLayoutLm);
        SafeRelease(m_inputLayoutTl);
        SafeRelease(m_pixelShader);
        SafeRelease(m_vertexShaderLm);
        SafeRelease(m_vertexShaderTl);
        SafeRelease(m_depthStencilView);
        SafeRelease(m_depthStencilTexture);
        SafeRelease(m_renderTargetView);
        SafeRelease(m_context);
        SafeRelease(m_device);
        SafeRelease(m_swapChain);
        m_renderWidth = 0;
        m_renderHeight = 0;
        m_hwnd = nullptr;
        ResetTextureStageStates();
        m_boundTextures[0] = nullptr;
        m_boundTextures[1] = nullptr;
    }

    void RefreshRenderSize() override
    {
        if (!m_hwnd) {
            m_renderWidth = 0;
            m_renderHeight = 0;
            return;
        }

        RECT clientRect{};
        GetClientRect(m_hwnd, &clientRect);
        const int newWidth = (std::max)(1L, clientRect.right - clientRect.left);
        const int newHeight = (std::max)(1L, clientRect.bottom - clientRect.top);
        if (newWidth != m_renderWidth || newHeight != m_renderHeight) {
            m_renderWidth = newWidth;
            m_renderHeight = newHeight;
            ResizeSwapChainBuffers();
        }
    }

    int GetRenderWidth() const override { return m_renderWidth; }
    int GetRenderHeight() const override { return m_renderHeight; }
    HWND GetWindowHandle() const override { return m_hwnd; }
    IDirect3DDevice7* GetLegacyDevice() const override { return nullptr; }

    int ClearColor(unsigned int color) override
    {
        if (!m_context || !m_renderTargetView) {
            return -1;
        }

        const float clearColor[4] = {
            static_cast<float>((color >> 16) & 0xFFu) / 255.0f,
            static_cast<float>((color >> 8) & 0xFFu) / 255.0f,
            static_cast<float>(color & 0xFFu) / 255.0f,
            static_cast<float>((color >> 24) & 0xFFu) / 255.0f,
        };
        m_context->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
        m_context->ClearRenderTargetView(m_renderTargetView, clearColor);
        return 0;
    }

    int ClearDepth() override
    {
        if (m_context && m_depthStencilView) {
            m_context->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
        }
        return 0;
    }

    int Present(bool vertSync) override
    {
        if (!m_swapChain) {
            return -1;
        }
        CaptureRenderTargetSnapshot();
        return static_cast<int>(m_swapChain->Present(vertSync ? 1 : 0, 0));
    }

    bool AcquireBackBufferDC(HDC* outDc) override
    {
        if (!outDc) {
            return false;
        }
        *outDc = m_captureDc;
        return *outDc != nullptr;
    }

    void ReleaseBackBufferDC(HDC dc) override
    {
        (void)dc;
    }

    bool BeginScene() override
    {
        if (m_context && m_renderTargetView) {
            m_context->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
            ApplyViewport();
        }
        return m_context != nullptr;
    }

    void EndScene() override {}
    void SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) override { (void)state; (void)matrix; }

    void SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override
    {
        switch (state) {
        case D3DRENDERSTATE_ALPHAREF: m_alphaRef = static_cast<unsigned int>(value & 0xFFu); break;
        case D3DRENDERSTATE_ALPHATESTENABLE: m_alphaTestEnable = value; break;
        case D3DRENDERSTATE_ALPHABLENDENABLE: m_alphaBlendEnable = value; break;
        case D3DRENDERSTATE_ZENABLE: m_depthEnable = value; break;
        case D3DRENDERSTATE_ZWRITEENABLE: m_depthWriteEnable = value; break;
        case D3DRENDERSTATE_CULLMODE: m_cullMode = static_cast<D3DCULL>(value); break;
        case D3DRENDERSTATE_COLORKEYENABLE: m_colorKeyEnable = value; break;
        case D3DRENDERSTATE_SRCBLEND: m_srcBlend = static_cast<D3DBLEND>(value); break;
        case D3DRENDERSTATE_DESTBLEND: m_destBlend = static_cast<D3DBLEND>(value); break;
        default: break;
        }
    }

    void SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override
    {
        if (stage >= 2) {
            return;
        }

        TextureStageState& stageState = m_textureStageStates[stage];
        switch (type) {
        case D3DTSS_TEXCOORDINDEX: stageState.texCoordIndex = value; break;
        case D3DTSS_COLORARG1: stageState.colorArg1 = value; break;
        case D3DTSS_COLOROP: stageState.colorOp = value; break;
        case D3DTSS_COLORARG2: stageState.colorArg2 = value; break;
        case D3DTSS_ALPHAARG1: stageState.alphaArg1 = value; break;
        case D3DTSS_ALPHAOP: stageState.alphaOp = value; break;
        case D3DTSS_ALPHAARG2: stageState.alphaArg2 = value; break;
        default: break;
        }
    }

    void BindTexture(DWORD stage, CTexture* texture) override
    {
        if (stage < 2) {
            m_boundTextures[stage] = texture;
        }
    }

    void DrawPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat,
        const void* vertices, DWORD vertexCount, DWORD flags) override
    {
        (void)flags;
        DrawTransformedPrimitive(primitiveType, vertexFormat, vertices, vertexCount, nullptr, 0);
    }

    void DrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat,
        const void* vertices, DWORD vertexCount, const unsigned short* indices,
        DWORD indexCount, DWORD flags) override
    {
        (void)flags;
        DrawTransformedPrimitive(primitiveType, vertexFormat, vertices, vertexCount, indices, indexCount);
    }

    void AdjustTextureSize(unsigned int* width, unsigned int* height) override
    {
        if (width && height) {
            *width = (std::max)(1u, *width);
            *height = (std::max)(1u, *height);
        }
    }

    void ReleaseTextureResource(CTexture* texture) override { ReleaseTextureMembers(texture); }

    bool CreateTextureResource(CTexture* texture, unsigned int requestedWidth, unsigned int requestedHeight,
        int pixelFormat, unsigned int* outSurfaceWidth, unsigned int* outSurfaceHeight) override
    {
        (void)pixelFormat;
        if (!texture || !m_device) {
            return false;
        }

        ReleaseTextureMembers(texture);
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = (std::max)(1u, requestedWidth);
        desc.Height = (std::max)(1u, requestedHeight);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        ID3D11Texture2D* textureObject = nullptr;
        HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &textureObject);
        if (FAILED(hr) || !textureObject) {
            SafeRelease(textureObject);
            return false;
        }

        ID3D11ShaderResourceView* textureView = nullptr;
        hr = m_device->CreateShaderResourceView(textureObject, nullptr, &textureView);
        if (FAILED(hr) || !textureView) {
            SafeRelease(textureView);
            SafeRelease(textureObject);
            return false;
        }

        texture->m_backendTextureObject = textureObject;
        texture->m_backendTextureView = textureView;
        if (outSurfaceWidth) {
            *outSurfaceWidth = desc.Width;
        }
        if (outSurfaceHeight) {
            *outSurfaceHeight = desc.Height;
        }
        return true;
    }

    bool UpdateTextureResource(CTexture* texture, int x, int y, int w, int h,
        const unsigned int* data, bool skipColorKey, int pitch) override
    {
        if (!texture || !texture->m_backendTextureObject || !m_context || !data || w <= 0 || h <= 0) {
            return false;
        }

        ID3D11Texture2D* textureObject = static_cast<ID3D11Texture2D*>(texture->m_backendTextureObject);
        const int srcPitch = pitch > 0 ? pitch : w * static_cast<int>(sizeof(unsigned int));
        std::vector<unsigned int> uploadBuffer(static_cast<size_t>(w) * static_cast<size_t>(h));
        for (int row = 0; row < h; ++row) {
            const unsigned int* srcRow = reinterpret_cast<const unsigned int*>(reinterpret_cast<const unsigned char*>(data) + static_cast<size_t>(row) * static_cast<size_t>(srcPitch));
            unsigned int* dstRow = uploadBuffer.data() + static_cast<size_t>(row) * static_cast<size_t>(w);
            for (int col = 0; col < w; ++col) {
                unsigned int pixel = srcRow[col];
                if (!skipColorKey && (pixel & 0x00FFFFFFu) == 0x00FF00FFu) {
                    pixel = 0x00000000u;
                }
                dstRow[col] = pixel;
            }
        }

        D3D11_BOX updateBox{};
        updateBox.left = static_cast<UINT>(x);
        updateBox.top = static_cast<UINT>(y);
        updateBox.front = 0;
        updateBox.right = static_cast<UINT>(x + w);
        updateBox.bottom = static_cast<UINT>(y + h);
        updateBox.back = 1;
        m_context->UpdateSubresource(textureObject, 0, &updateBox, uploadBuffer.data(), static_cast<UINT>(w * sizeof(unsigned int)), 0);
        return true;
    }

private:
    struct TextureStageState {
        DWORD texCoordIndex;
        DWORD colorArg1;
        DWORD colorOp;
        DWORD colorArg2;
        DWORD alphaArg1;
        DWORD alphaOp;
        DWORD alphaArg2;
    };

    struct BlendStateEntry { D3D11_BLEND_DESC desc; ID3D11BlendState* state; };
    struct DepthStateEntry { D3D11_DEPTH_STENCIL_DESC desc; ID3D11DepthStencilState* state; };
    struct RasterizerStateEntry { D3D11_RASTERIZER_DESC desc; ID3D11RasterizerState* state; };

    void ResetTextureStageStates()
    {
        m_textureStageStates[0] = { 0, D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_DIFFUSE, D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_DIFFUSE };
        m_textureStageStates[1] = { 1, D3DTA_TEXTURE, D3DTOP_DISABLE, D3DTA_CURRENT, D3DTA_TEXTURE, D3DTOP_DISABLE, D3DTA_CURRENT };
    }

    bool CreateDepthStencilResources()
    {
        SafeRelease(m_depthStencilView);
        SafeRelease(m_depthStencilTexture);
        if (!m_device) {
            return false;
        }

        int targetWidth = m_renderWidth;
        int targetHeight = m_renderHeight;
        if ((targetWidth <= 0 || targetHeight <= 0) && m_hwnd) {
            RECT clientRect{};
            GetClientRect(m_hwnd, &clientRect);
            targetWidth = (std::max)(1L, clientRect.right - clientRect.left);
            targetHeight = (std::max)(1L, clientRect.bottom - clientRect.top);
        }
        if (targetWidth <= 0 || targetHeight <= 0) {
            return false;
        }

        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width = static_cast<UINT>(targetWidth);
        depthDesc.Height = static_cast<UINT>(targetHeight);
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        HRESULT hr = m_device->CreateTexture2D(&depthDesc, nullptr, &m_depthStencilTexture);
        if (FAILED(hr) || !m_depthStencilTexture) {
            return false;
        }

        hr = m_device->CreateDepthStencilView(m_depthStencilTexture, nullptr, &m_depthStencilView);
        return SUCCEEDED(hr) && m_depthStencilView != nullptr;
    }

    bool CreatePipelineResources()
    {
        if (!m_device) {
            return false;
        }

        const char* shaderSource = GetD3D11ShaderSource();
        ID3DBlob* vertexShaderTlBlob = nullptr;
        ID3DBlob* vertexShaderLmBlob = nullptr;
        ID3DBlob* pixelShaderBlob = nullptr;
        const bool compiled = CompileShaderBlob(shaderSource, "VSMainTL", "vs_4_0", &vertexShaderTlBlob)
            && CompileShaderBlob(shaderSource, "VSMainLM", "vs_4_0", &vertexShaderLmBlob)
            && CompileShaderBlob(shaderSource, "PSMain", "ps_4_0", &pixelShaderBlob);
        if (!compiled) {
            SafeRelease(vertexShaderTlBlob);
            SafeRelease(vertexShaderLmBlob);
            SafeRelease(pixelShaderBlob);
            return false;
        }

        HRESULT hr = m_device->CreateVertexShader(vertexShaderTlBlob->GetBufferPointer(), vertexShaderTlBlob->GetBufferSize(), nullptr, &m_vertexShaderTl);
        if (FAILED(hr) || !m_vertexShaderTl) {
            SafeRelease(vertexShaderTlBlob);
            SafeRelease(vertexShaderLmBlob);
            SafeRelease(pixelShaderBlob);
            return false;
        }

        hr = m_device->CreateVertexShader(vertexShaderLmBlob->GetBufferPointer(), vertexShaderLmBlob->GetBufferSize(), nullptr, &m_vertexShaderLm);
        if (FAILED(hr) || !m_vertexShaderLm) {
            SafeRelease(vertexShaderTlBlob);
            SafeRelease(vertexShaderLmBlob);
            SafeRelease(pixelShaderBlob);
            return false;
        }

        hr = m_device->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, &m_pixelShader);
        if (FAILED(hr) || !m_pixelShader) {
            SafeRelease(vertexShaderTlBlob);
            SafeRelease(vertexShaderLmBlob);
            SafeRelease(pixelShaderBlob);
            return false;
        }

        const D3D11_INPUT_ELEMENT_DESC tlLayoutDesc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(tlvertex3d, x)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, static_cast<UINT>(offsetof(tlvertex3d, color)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(tlvertex3d, tu)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        hr = m_device->CreateInputLayout(tlLayoutDesc,
            static_cast<UINT>(std::size(tlLayoutDesc)),
            vertexShaderTlBlob->GetBufferPointer(),
            vertexShaderTlBlob->GetBufferSize(),
            &m_inputLayoutTl);
        if (FAILED(hr) || !m_inputLayoutTl) {
            SafeRelease(vertexShaderTlBlob);
            SafeRelease(vertexShaderLmBlob);
            SafeRelease(pixelShaderBlob);
            return false;
        }

        const D3D11_INPUT_ELEMENT_DESC lmLayoutDesc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(lmtlvertex3d, vert) + offsetof(tlvertex3d, x)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, static_cast<UINT>(offsetof(lmtlvertex3d, vert) + offsetof(tlvertex3d, color)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(lmtlvertex3d, vert) + offsetof(tlvertex3d, tu)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(lmtlvertex3d, tu2)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        hr = m_device->CreateInputLayout(lmLayoutDesc,
            static_cast<UINT>(std::size(lmLayoutDesc)),
            vertexShaderLmBlob->GetBufferPointer(),
            vertexShaderLmBlob->GetBufferSize(),
            &m_inputLayoutLm);
        SafeRelease(vertexShaderTlBlob);
        SafeRelease(vertexShaderLmBlob);
        SafeRelease(pixelShaderBlob);
        if (FAILED(hr) || !m_inputLayoutLm) {
            return false;
        }

        D3D11_BUFFER_DESC constantBufferDesc{};
        constantBufferDesc.ByteWidth = static_cast<UINT>(sizeof(D3D11DrawConstants));
        constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = m_device->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer);
        if (FAILED(hr) || !m_constantBuffer) {
            return false;
        }

        D3D11_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = m_device->CreateSamplerState(&samplerDesc, &m_samplerState);
        return SUCCEEDED(hr) && m_samplerState != nullptr;
    }

    void ReleaseCachedStates()
    {
        for (BlendStateEntry& entry : m_blendStates) {
            SafeRelease(entry.state);
        }
        m_blendStates.clear();
        for (DepthStateEntry& entry : m_depthStates) {
            SafeRelease(entry.state);
        }
        m_depthStates.clear();
        for (RasterizerStateEntry& entry : m_rasterizerStates) {
            SafeRelease(entry.state);
        }
        m_rasterizerStates.clear();
    }

    void ApplyViewport()
    {
        if (!m_context || m_renderWidth <= 0 || m_renderHeight <= 0) {
            return;
        }
        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(m_renderWidth);
        viewport.Height = static_cast<float>(m_renderHeight);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &viewport);
    }

    ID3D11BlendState* GetBlendState()
    {
        D3D11_BLEND_DESC desc{};
        desc.RenderTarget[0].BlendEnable = m_alphaBlendEnable ? TRUE : FALSE;
        desc.RenderTarget[0].SrcBlend = ConvertBlendFactor(m_srcBlend);
        desc.RenderTarget[0].DestBlend = ConvertBlendFactor(m_destBlend);
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        for (BlendStateEntry& entry : m_blendStates) {
            if (std::memcmp(&entry.desc, &desc, sizeof(desc)) == 0) {
                return entry.state;
            }
        }

        ID3D11BlendState* state = nullptr;
        if (FAILED(m_device->CreateBlendState(&desc, &state)) || !state) {
            return nullptr;
        }
        m_blendStates.push_back({ desc, state });
        return state;
    }

    ID3D11DepthStencilState* GetDepthStencilState()
    {
        D3D11_DEPTH_STENCIL_DESC desc{};
        desc.DepthEnable = m_depthEnable != D3DZB_FALSE ? TRUE : FALSE;
        desc.DepthWriteMask = m_depthWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

        for (DepthStateEntry& entry : m_depthStates) {
            if (std::memcmp(&entry.desc, &desc, sizeof(desc)) == 0) {
                return entry.state;
            }
        }

        ID3D11DepthStencilState* state = nullptr;
        if (FAILED(m_device->CreateDepthStencilState(&desc, &state)) || !state) {
            return nullptr;
        }
        m_depthStates.push_back({ desc, state });
        return state;
    }

    ID3D11RasterizerState* GetRasterizerState()
    {
        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.FrontCounterClockwise = FALSE;
        desc.DepthClipEnable = TRUE;
        switch (m_cullMode) {
        case D3DCULL_CW: desc.CullMode = D3D11_CULL_FRONT; break;
        case D3DCULL_CCW: desc.CullMode = D3D11_CULL_BACK; break;
        default: desc.CullMode = D3D11_CULL_NONE; break;
        }

        for (RasterizerStateEntry& entry : m_rasterizerStates) {
            if (std::memcmp(&entry.desc, &desc, sizeof(desc)) == 0) {
                return entry.state;
            }
        }

        ID3D11RasterizerState* state = nullptr;
        if (FAILED(m_device->CreateRasterizerState(&desc, &state)) || !state) {
            return nullptr;
        }
        m_rasterizerStates.push_back({ desc, state });
        return state;
    }

    bool EnsureDynamicBuffer(ID3D11Buffer** buffer, size_t* currentSize, size_t requiredSize, UINT bindFlags)
    {
        if (!buffer || !currentSize || requiredSize == 0) {
            return false;
        }
        if (*buffer && *currentSize >= requiredSize) {
            return true;
        }

        SafeRelease(*buffer);
        size_t newSize = 4096;
        while (newSize < requiredSize) {
            newSize *= 2;
        }

        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = static_cast<UINT>(newSize);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = bindFlags;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        HRESULT hr = m_device->CreateBuffer(&desc, nullptr, buffer);
        if (FAILED(hr) || !*buffer) {
            *currentSize = 0;
            return false;
        }
        *currentSize = newSize;
        return true;
    }

    bool UploadBuffer(ID3D11Buffer* buffer, const void* data, size_t dataSize)
    {
        if (!buffer || !data || dataSize == 0 || !m_context) {
            return false;
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = m_context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr) || !mapped.pData) {
            return false;
        }
        std::memcpy(mapped.pData, data, dataSize);
        m_context->Unmap(buffer, 0);
        return true;
    }

    unsigned int BuildDrawFlags(DWORD vertexFormat, ID3D11ShaderResourceView* texture0View, ID3D11ShaderResourceView* texture1View) const
    {
        unsigned int flags = 0;
        if (texture0View && m_textureStageStates[0].colorOp != D3DTOP_DISABLE) {
            flags |= D3D11DrawFlag_Texture0Enabled;
        }
        if (texture1View && vertexFormat == kLmFvf
            && m_textureStageStates[1].colorOp == D3DTOP_MODULATE
            && m_textureStageStates[1].colorArg1 == (D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE)
            && m_textureStageStates[1].colorArg2 == D3DTA_CURRENT) {
            flags |= D3D11DrawFlag_Texture1Enabled | D3D11DrawFlag_Stage1LightmapAlpha;
        }
        if (m_alphaTestEnable) {
            flags |= D3D11DrawFlag_AlphaTestEnabled;
        }
        if (m_colorKeyEnable) {
            flags |= D3D11DrawFlag_ColorKeyEnabled;
        }
        if (texture0View && m_textureStageStates[0].alphaOp == D3DTOP_SELECTARG1 && m_textureStageStates[0].alphaArg1 == D3DTA_TEXTURE) {
            flags |= D3D11DrawFlag_Stage0AlphaUseTexture;
        }
        if (texture0View && m_textureStageStates[0].alphaOp == D3DTOP_MODULATE
            && m_textureStageStates[0].alphaArg1 == D3DTA_TEXTURE
            && m_textureStageStates[0].alphaArg2 == D3DTA_DIFFUSE) {
            flags |= D3D11DrawFlag_Stage0AlphaModulate;
        }
        return flags;
    }

    std::vector<unsigned short> BuildTriangleFanIndices(const unsigned short* indices, DWORD vertexCount, DWORD indexCount) const
    {
        std::vector<unsigned short> expanded;
        const DWORD sourceCount = indices && indexCount > 0 ? indexCount : vertexCount;
        if (sourceCount < 3) {
            return expanded;
        }
        expanded.reserve(static_cast<size_t>(sourceCount - 2) * 3u);
        for (DWORD i = 1; i + 1 < sourceCount; ++i) {
            expanded.push_back(indices ? indices[0] : static_cast<unsigned short>(0));
            expanded.push_back(indices ? indices[i] : static_cast<unsigned short>(i));
            expanded.push_back(indices ? indices[i + 1] : static_cast<unsigned short>(i + 1));
        }
        return expanded;
    }

    void DrawTransformedPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat,
        const void* vertices, DWORD vertexCount, const unsigned short* indices, DWORD indexCount)
    {
        if (!m_context || !m_renderTargetView || !vertices || vertexCount == 0) {
            return;
        }

        const bool isLightmap = vertexFormat == kLmFvf;
        if (!isLightmap && vertexFormat != D3DFVF_TLVERTEX) {
            return;
        }

        const size_t vertexStride = isLightmap ? sizeof(lmtlvertex3d) : sizeof(tlvertex3d);
        const size_t vertexBytes = vertexStride * static_cast<size_t>(vertexCount);
        if (!EnsureDynamicBuffer(&m_vertexBuffer, &m_vertexBufferSize, vertexBytes, D3D11_BIND_VERTEX_BUFFER)
            || !UploadBuffer(m_vertexBuffer, vertices, vertexBytes)) {
            return;
        }

        std::vector<unsigned short> convertedIndices;
        const unsigned short* drawIndices = indices;
        DWORD drawIndexCount = indexCount;
        D3D11_PRIMITIVE_TOPOLOGY topology = ConvertPrimitiveTopology(primitiveType);
        if (primitiveType == D3DPT_TRIANGLEFAN) {
            convertedIndices = BuildTriangleFanIndices(indices, vertexCount, indexCount);
            if (convertedIndices.empty()) {
                return;
            }
            drawIndices = convertedIndices.data();
            drawIndexCount = static_cast<DWORD>(convertedIndices.size());
            topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
        if (topology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED) {
            return;
        }

        if (drawIndices && drawIndexCount > 0) {
            const size_t indexBytes = static_cast<size_t>(drawIndexCount) * sizeof(unsigned short);
            if (!EnsureDynamicBuffer(&m_indexBuffer, &m_indexBufferSize, indexBytes, D3D11_BIND_INDEX_BUFFER)
                || !UploadBuffer(m_indexBuffer, drawIndices, indexBytes)) {
                return;
            }
            m_context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R16_UINT, 0);
        } else {
            m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
        }

        ID3D11ShaderResourceView* textureViews[2] = {
            m_boundTextures[0] ? static_cast<ID3D11ShaderResourceView*>(m_boundTextures[0]->m_backendTextureView) : nullptr,
            m_boundTextures[1] ? static_cast<ID3D11ShaderResourceView*>(m_boundTextures[1]->m_backendTextureView) : nullptr,
        };

        D3D11DrawConstants constants{};
        constants.screenWidth = static_cast<float>((std::max)(1, m_renderWidth));
        constants.screenHeight = static_cast<float>((std::max)(1, m_renderHeight));
        constants.alphaRef = static_cast<float>(m_alphaRef) / 255.0f;
        constants.flags = BuildDrawFlags(vertexFormat, textureViews[0], textureViews[1]);
        if (!UploadBuffer(m_constantBuffer, &constants, sizeof(constants))) {
            return;
        }

        const UINT stride = static_cast<UINT>(vertexStride);
        const UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
        m_context->IASetPrimitiveTopology(topology);
        m_context->IASetInputLayout(isLightmap ? m_inputLayoutLm : m_inputLayoutTl);
        m_context->VSSetShader(isLightmap ? m_vertexShaderLm : m_vertexShaderTl, nullptr, 0);
        m_context->PSSetShader(m_pixelShader, nullptr, 0);
        m_context->VSSetConstantBuffers(0, 1, &m_constantBuffer);
        m_context->PSSetConstantBuffers(0, 1, &m_constantBuffer);
        m_context->PSSetSamplers(0, 1, &m_samplerState);
        m_context->PSSetShaderResources(0, 2, textureViews);

        const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_context->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
        m_context->OMSetBlendState(GetBlendState(), blendFactor, 0xFFFFFFFFu);
        m_context->OMSetDepthStencilState(GetDepthStencilState(), 0);
        m_context->RSSetState(GetRasterizerState());
        ApplyViewport();

        if (drawIndices && drawIndexCount > 0) {
            m_context->DrawIndexed(drawIndexCount, 0, 0);
        } else {
            m_context->Draw(vertexCount, 0);
        }
    }

    bool RefreshRenderTarget()
    {
        if (!m_swapChain || !m_device) {
            return false;
        }
        SafeRelease(m_renderTargetView);
        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
        if (FAILED(hr) || !backBuffer) {
            SafeRelease(backBuffer);
            return false;
        }
        hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_renderTargetView);
        SafeRelease(backBuffer);
        if (FAILED(hr) || !m_renderTargetView) {
            return false;
        }
        if (!CreateDepthStencilResources()) {
            return false;
        }
        m_context->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
        ApplyViewport();
        return true;
    }

    void ResizeSwapChainBuffers()
    {
        if (!m_swapChain || m_renderWidth <= 0 || m_renderHeight <= 0) {
            return;
        }
        ReleaseCaptureResources();
        SafeRelease(m_depthStencilView);
        SafeRelease(m_depthStencilTexture);
        SafeRelease(m_renderTargetView);
        HRESULT hr = m_swapChain->ResizeBuffers(0,
            static_cast<UINT>(m_renderWidth),
            static_cast<UINT>(m_renderHeight),
            DXGI_FORMAT_UNKNOWN,
            0);
        if (FAILED(hr)) {
            DbgLog("[Render] D3D11 swap-chain resize failed hr=0x%08X.\n", static_cast<unsigned int>(hr));
            return;
        }
        RefreshRenderTarget();
    }

    void ReleaseCaptureResources()
    {
        SafeRelease(m_captureTexture);
        if (m_captureBitmap) {
            DeleteObject(m_captureBitmap);
            m_captureBitmap = nullptr;
        }
        if (m_captureDc) {
            DeleteDC(m_captureDc);
            m_captureDc = nullptr;
        }
        m_captureBits = nullptr;
        m_captureWidth = 0;
        m_captureHeight = 0;
    }

    bool EnsureCaptureResources()
    {
        if (!m_device || m_renderWidth <= 0 || m_renderHeight <= 0) {
            return false;
        }

        const bool sizeMatches = m_captureTexture && m_captureDc && m_captureBitmap
            && m_captureWidth == m_renderWidth && m_captureHeight == m_renderHeight;
        if (sizeMatches) {
            return true;
        }

        ReleaseCaptureResources();

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(m_renderWidth);
        desc.Height = static_cast<UINT>(m_renderHeight);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_captureTexture);
        if (FAILED(hr) || !m_captureTexture) {
            ReleaseCaptureResources();
            return false;
        }

        HDC screenDc = GetDC(nullptr);
        if (!screenDc) {
            ReleaseCaptureResources();
            return false;
        }

        m_captureDc = CreateCompatibleDC(screenDc);
        ReleaseDC(nullptr, screenDc);
        if (!m_captureDc) {
            ReleaseCaptureResources();
            return false;
        }

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = m_renderWidth;
        bmi.bmiHeader.biHeight = -m_renderHeight;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        m_captureBitmap = CreateDIBSection(m_captureDc, &bmi, DIB_RGB_COLORS, &m_captureBits, nullptr, 0);
        if (!m_captureBitmap || !m_captureBits) {
            ReleaseCaptureResources();
            return false;
        }

        SelectObject(m_captureDc, m_captureBitmap);
        m_captureWidth = m_renderWidth;
        m_captureHeight = m_renderHeight;
        return true;
    }

    void CaptureRenderTargetSnapshot()
    {
        if (!m_context || !m_swapChain || !EnsureCaptureResources()) {
            return;
        }

        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
        if (FAILED(hr) || !backBuffer) {
            SafeRelease(backBuffer);
            return;
        }

        m_context->CopyResource(m_captureTexture, backBuffer);
        SafeRelease(backBuffer);

        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr = m_context->Map(m_captureTexture, 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr) || !mapped.pData || !m_captureBits) {
            return;
        }

        const size_t dstPitch = static_cast<size_t>(m_captureWidth) * sizeof(unsigned int);
        for (int row = 0; row < m_captureHeight; ++row) {
            const unsigned char* srcRow = static_cast<const unsigned char*>(mapped.pData) + static_cast<size_t>(row) * static_cast<size_t>(mapped.RowPitch);
            unsigned char* dstRow = static_cast<unsigned char*>(m_captureBits) + static_cast<size_t>(row) * dstPitch;
            std::memcpy(dstRow, srcRow, dstPitch);
        }
        m_context->Unmap(m_captureTexture, 0);
    }

    HWND m_hwnd;
    int m_renderWidth;
    int m_renderHeight;
    IDXGISwapChain* m_swapChain;
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;
    ID3D11RenderTargetView* m_renderTargetView;
    ID3D11Texture2D* m_depthStencilTexture;
    ID3D11DepthStencilView* m_depthStencilView;
    ID3D11Texture2D* m_captureTexture;
    ID3D11VertexShader* m_vertexShaderTl;
    ID3D11VertexShader* m_vertexShaderLm;
    ID3D11PixelShader* m_pixelShader;
    ID3D11InputLayout* m_inputLayoutTl;
    ID3D11InputLayout* m_inputLayoutLm;
    ID3D11Buffer* m_constantBuffer;
    ID3D11Buffer* m_vertexBuffer;
    size_t m_vertexBufferSize;
    ID3D11Buffer* m_indexBuffer;
    size_t m_indexBufferSize;
    ID3D11SamplerState* m_samplerState;
    unsigned int m_alphaRef;
    DWORD m_alphaTestEnable;
    DWORD m_alphaBlendEnable;
    DWORD m_depthEnable;
    DWORD m_depthWriteEnable;
    D3DCULL m_cullMode;
    DWORD m_colorKeyEnable;
    D3DBLEND m_srcBlend;
    D3DBLEND m_destBlend;
    TextureStageState m_textureStageStates[2];
    CTexture* m_boundTextures[2];
    std::vector<BlendStateEntry> m_blendStates;
    std::vector<DepthStateEntry> m_depthStates;
    std::vector<RasterizerStateEntry> m_rasterizerStates;
    HDC m_captureDc;
    HBITMAP m_captureBitmap;
    void* m_captureBits;
    int m_captureWidth;
    int m_captureHeight;
};

class RoutedRenderDevice final : public IRenderDevice {
public:
    RoutedRenderDevice()
        : m_active(&m_legacy)
    {
    }

    RenderBackendType GetBackendType() const override
    {
        return m_active->GetBackendType();
    }

    bool Initialize(HWND hwnd, RenderBackendBootstrapResult* outResult) override
    {
        Shutdown();

        const RenderBackendType requestedBackend = GetRequestedRenderBackend();
        RenderBackendBootstrapResult result{};
        result.backend = requestedBackend;
        result.initHr = -1;

        switch (requestedBackend) {
        case RenderBackendType::Direct3D11:
            if (m_d3d11.Initialize(hwnd, &result)) {
                m_active = &m_d3d11;
            } else {
                DbgLog("[Render] Failed to initialize backend '%s' (hr=0x%08X). Falling back to Direct3D7.\n",
                    GetRenderBackendName(requestedBackend),
                    static_cast<unsigned int>(result.initHr));
                m_active = &m_legacy;
                if (!m_legacy.Initialize(hwnd, &result)) {
                    if (outResult) {
                        *outResult = result;
                    }
                    return false;
                }
            }
            break;

        case RenderBackendType::Direct3D12:
        case RenderBackendType::Vulkan:
            DbgLog("[Render] Requested backend '%s' is not implemented yet. Falling back to Direct3D7.\n",
                GetRenderBackendName(requestedBackend));
            m_active = &m_legacy;
            if (!m_legacy.Initialize(hwnd, &result)) {
                if (outResult) {
                    *outResult = result;
                }
                return false;
            }
            break;

        case RenderBackendType::LegacyDirect3D7:
        default:
            m_active = &m_legacy;
            if (!m_legacy.Initialize(hwnd, &result)) {
                if (outResult) {
                    *outResult = result;
                }
                return false;
            }
            break;
        }

        if (outResult) {
            *outResult = result;
        }
        return true;
    }

    void Shutdown() override
    {
        m_d3d11.Shutdown();
        m_legacy.Shutdown();
        m_active = &m_legacy;
    }

    void RefreshRenderSize() override { m_active->RefreshRenderSize(); }
    int GetRenderWidth() const override { return m_active->GetRenderWidth(); }
    int GetRenderHeight() const override { return m_active->GetRenderHeight(); }
    HWND GetWindowHandle() const override { return m_active->GetWindowHandle(); }
    IDirect3DDevice7* GetLegacyDevice() const override { return m_active->GetLegacyDevice(); }
    int ClearColor(unsigned int color) override { return m_active->ClearColor(color); }
    int ClearDepth() override { return m_active->ClearDepth(); }
    int Present(bool vertSync) override { return m_active->Present(vertSync); }
    bool AcquireBackBufferDC(HDC* outDc) override { return m_active->AcquireBackBufferDC(outDc); }
    void ReleaseBackBufferDC(HDC dc) override { m_active->ReleaseBackBufferDC(dc); }
    bool BeginScene() override { return m_active->BeginScene(); }
    void EndScene() override { m_active->EndScene(); }
    void SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) override { m_active->SetTransform(state, matrix); }
    void SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override { m_active->SetRenderState(state, value); }
    void SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override { m_active->SetTextureStageState(stage, type, value); }
    void BindTexture(DWORD stage, CTexture* texture) override { m_active->BindTexture(stage, texture); }
    void DrawPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat, const void* vertices, DWORD vertexCount, DWORD flags) override { m_active->DrawPrimitive(primitiveType, vertexFormat, vertices, vertexCount, flags); }
    void DrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat, const void* vertices, DWORD vertexCount, const unsigned short* indices, DWORD indexCount, DWORD flags) override { m_active->DrawIndexedPrimitive(primitiveType, vertexFormat, vertices, vertexCount, indices, indexCount, flags); }
    void AdjustTextureSize(unsigned int* width, unsigned int* height) override { m_active->AdjustTextureSize(width, height); }
    void ReleaseTextureResource(CTexture* texture) override { m_active->ReleaseTextureResource(texture); }
    bool CreateTextureResource(CTexture* texture, unsigned int requestedWidth, unsigned int requestedHeight, int pixelFormat, unsigned int* outSurfaceWidth, unsigned int* outSurfaceHeight) override { return m_active->CreateTextureResource(texture, requestedWidth, requestedHeight, pixelFormat, outSurfaceWidth, outSurfaceHeight); }
    bool UpdateTextureResource(CTexture* texture, int x, int y, int w, int h, const unsigned int* data, bool skipColorKey, int pitch) override { return m_active->UpdateTextureResource(texture, x, y, w, h, data, skipColorKey, pitch); }

private:
    LegacyRenderDevice m_legacy;
    D3D11RenderDevice m_d3d11;
    IRenderDevice* m_active;
};

} // namespace

IRenderDevice& GetRenderDevice()
{
    static RoutedRenderDevice s_renderDevice;
    return s_renderDevice;
}