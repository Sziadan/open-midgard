#pragma once

#include <windows.h>

enum class RenderBackendType {
    LegacyDirect3D7 = 0,
    Direct3D11,
    Direct3D12,
    Vulkan,
};

struct RenderBackendBootstrapResult {
    RenderBackendType backend;
    int initHr;
};

const char* GetRenderBackendName(RenderBackendType backend);
bool IsRenderBackendImplemented(RenderBackendType backend);
bool IsRenderBackendSupported(RenderBackendType backend);
RenderBackendType GetConfiguredRenderBackend();
bool SetConfiguredRenderBackend(RenderBackendType backend);
RenderBackendType GetRequestedRenderBackend();
bool InitializeRenderBackend(HWND hwnd, RenderBackendBootstrapResult* outResult);