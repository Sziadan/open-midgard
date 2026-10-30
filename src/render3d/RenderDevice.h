#pragma once

#include <cstdint>

#if RO_PLATFORM_WINDOWS
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
using RoDword = DWORD;
#else
#include "Dx7Compat.h"
using RoDword = DWORD;
#endif

#include "RenderBackend.h"

class CTexture;

struct QtUiRenderTargetInfo {
    bool available = false;
    RenderBackendType backend = RenderBackendType::LegacyDirect3D7;
    void* graphicsInstance = nullptr;
    void* graphicsPhysicalDevice = nullptr;
    void* graphicsDevice = nullptr;
    void* graphicsQueueOrContext = nullptr;
    void* colorTarget = nullptr;
    void* colorTargetView = nullptr;
    unsigned int width = 0;
    unsigned int height = 0;
    uint32_t queueFamilyIndex = 0xFFFFFFFFu;
    uint32_t colorFormat = 0u;
    uint32_t colorImageLayout = 0u;
    uint32_t colorTargetState = 0u;
    uint32_t targetSampleCount = 1u;
    uint32_t minimumFeatureLevel = 0u;
    uint32_t adapterLuidLow = 0u;
    int32_t adapterLuidHigh = 0;
};

class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    virtual RenderBackendType GetBackendType() const = 0;
    virtual bool Initialize(RoNativeWindowHandle hwnd, RenderBackendBootstrapResult* outResult) = 0;
    virtual void Shutdown() = 0;
    virtual void RefreshRenderSize() = 0;

    virtual int GetRenderWidth() const = 0;
    virtual int GetRenderHeight() const = 0;
    virtual RoNativeWindowHandle GetWindowHandle() const = 0;
    virtual IDirect3DDevice7* GetLegacyDevice() const = 0;

    virtual int ClearColor(unsigned int color) = 0;
    virtual int ClearDepth() = 0;
    virtual int Present(bool vertSync) = 0;
    virtual bool UpdateBackBufferFromMemory(const void* bgraPixels, int width, int height, int pitch) = 0;

    virtual bool BeginScene() = 0;
    virtual bool PrepareOverlayPass() = 0;
    virtual void EndScene() = 0;
    virtual void SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) = 0;
    virtual void SetRenderState(D3DRENDERSTATETYPE state, RoDword value) = 0;
    virtual void SetTextureStageState(RoDword stage, D3DTEXTURESTAGESTATETYPE type, RoDword value) = 0;
    virtual void BindTexture(RoDword stage, CTexture* texture) = 0;
    virtual void DrawPrimitive(D3DPRIMITIVETYPE primitiveType, RoDword vertexFormat,
        const void* vertices, RoDword vertexCount, RoDword flags) = 0;
    virtual void DrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType, RoDword vertexFormat,
        const void* vertices, RoDword vertexCount, const unsigned short* indices,
        RoDword indexCount, RoDword flags) = 0;

    virtual void AdjustTextureSize(unsigned int* width, unsigned int* height) = 0;
    virtual void ReleaseTextureResource(CTexture* texture) = 0;
    virtual bool CreateTextureResource(CTexture* texture, unsigned int requestedWidth, unsigned int requestedHeight,
        int pixelFormat, unsigned int* outSurfaceWidth, unsigned int* outSurfaceHeight) = 0;
    virtual bool UpdateTextureResource(CTexture* texture, int x, int y, int w, int h,
        const unsigned int* data, bool skipColorKey, int pitch) = 0;
    virtual bool GetQtUiRenderTargetInfo(QtUiRenderTargetInfo* outInfo) const = 0;
    virtual bool GetQtUiTextureTargetInfo(const CTexture* texture, QtUiRenderTargetInfo* outInfo) const = 0;
};

IRenderDevice& GetRenderDevice();

// Direct3D 7 samples at integer pixel corners, so its UI quads sit half a pixel
// up and left. The modern shaders sample pixel centers, and that same shift
// lands on the boundary between texels.
inline float OverlayScreenEdge(int pixel)
{
    if (GetRenderDevice().GetBackendType() == RenderBackendType::LegacyDirect3D7) {
        return static_cast<float>(pixel) - 0.5f;
    }
    return static_cast<float>(pixel);
}