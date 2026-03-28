#include "RenderBackend.h"

#include "RenderDevice.h"
#include "DebugLog.h"

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <algorithm>
#include <cctype>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace {

constexpr char kRenderBackendRegPath[] = "Software\\Gravity Soft\\Ragnarok Online";
constexpr char kRenderBackendValueName[] = "RenderBackend";
constexpr RenderBackendType kDefaultConfiguredBackend = RenderBackendType::Direct3D11;

bool IsBackendAllowedOnCurrentBuild(RenderBackendType backend)
{
#if defined(_WIN64)
    return backend != RenderBackendType::LegacyDirect3D7;
#else
    return true;
#endif
}

RenderBackendType GetFallbackBackendForCurrentBuild()
{
    constexpr RenderBackendType kOrderedCandidates[] = {
        RenderBackendType::Direct3D11,
        RenderBackendType::Direct3D12,
        RenderBackendType::Vulkan,
        RenderBackendType::LegacyDirect3D7,
    };

    for (RenderBackendType candidate : kOrderedCandidates) {
        if (IsBackendAllowedOnCurrentBuild(candidate)
            && IsRenderBackendImplemented(candidate)
            && IsRenderBackendSupported(candidate)) {
            return candidate;
        }
    }

    return kDefaultConfiguredBackend;
}

RenderBackendType NormalizeBackendForCurrentBuild(RenderBackendType backend)
{
    if (IsBackendAllowedOnCurrentBuild(backend)
        && IsRenderBackendImplemented(backend)
        && IsRenderBackendSupported(backend)) {
        return backend;
    }

    return GetFallbackBackendForCurrentBuild();
}

template <typename T>
void SafeRelease(T*& value)
{
    if (value) {
        value->Release();
        value = nullptr;
    }
}

struct RenderBackendSupportCache {
    int d3d11;
    int d3d12;
    int vulkan;
};

RenderBackendSupportCache g_supportCache = { -1, -1, -1 };

RenderBackendType ParseRenderBackendName(const char* value)
{
    if (!value || !*value) {
        return kDefaultConfiguredBackend;
    }

    char normalized[32] = {};
    size_t writeIndex = 0;
    for (size_t readIndex = 0; value[readIndex] != '\0' && writeIndex + 1 < sizeof(normalized); ++readIndex) {
        const unsigned char ch = static_cast<unsigned char>(value[readIndex]);
        if (ch == ' ' || ch == '_' || ch == '-') {
            continue;
        }
        normalized[writeIndex++] = static_cast<char>(std::tolower(ch));
    }
    normalized[writeIndex] = '\0';

    if (std::strcmp(normalized, "dx11") == 0 || std::strcmp(normalized, "d3d11") == 0 || std::strcmp(normalized, "direct3d11") == 0) {
        return RenderBackendType::Direct3D11;
    }
    if (std::strcmp(normalized, "dx7") == 0 || std::strcmp(normalized, "d3d7") == 0 || std::strcmp(normalized, "direct3d7") == 0) {
        return RenderBackendType::LegacyDirect3D7;
    }
    if (std::strcmp(normalized, "dx12") == 0 || std::strcmp(normalized, "d3d12") == 0 || std::strcmp(normalized, "direct3d12") == 0) {
        return RenderBackendType::Direct3D12;
    }
    if (std::strcmp(normalized, "vk") == 0 || std::strcmp(normalized, "vulkan") == 0) {
        return RenderBackendType::Vulkan;
    }
    return kDefaultConfiguredBackend;
}

bool IsValidStoredBackend(DWORD rawValue)
{
    return rawValue <= static_cast<DWORD>(RenderBackendType::Vulkan);
}

bool ProbeD3D11Support()
{
    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        featureLevels,
        static_cast<UINT>(std::size(featureLevels)),
        D3D11_SDK_VERSION,
        &device,
        &featureLevel,
        &context);
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            featureLevels,
            static_cast<UINT>(std::size(featureLevels)),
            D3D11_SDK_VERSION,
            &device,
            &featureLevel,
            &context);
    }

    SafeRelease(context);
    SafeRelease(device);
    return SUCCEEDED(hr);
}

bool ProbeD3D12Support()
{
    IDXGIFactory4* factory = nullptr;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) {
        SafeRelease(factory);
        return false;
    }

    ID3D12Device* device = nullptr;
    hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr) || !device) {
        IDXGIAdapter* warpAdapter = nullptr;
        if (SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter))) && warpAdapter) {
            hr = D3D12CreateDevice(warpAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
        }
        SafeRelease(warpAdapter);
    }

    SafeRelease(device);
    SafeRelease(factory);
    return SUCCEEDED(hr);
}

bool ProbeVulkanSupport()
{
#if RO_HAS_VULKAN
    HMODULE module = LoadLibraryA("vulkan-1.dll");
    if (!module) {
        return false;
    }

    const FARPROC getInstanceProcAddr = GetProcAddress(module, "vkGetInstanceProcAddr");
    const FARPROC createInstance = GetProcAddress(module, "vkCreateInstance");
    FreeLibrary(module);
    return getInstanceProcAddr != nullptr && createInstance != nullptr;
#else
    return false;
#endif
}

} // namespace

const char* GetRenderBackendName(RenderBackendType backend)
{
    switch (backend) {
    case RenderBackendType::LegacyDirect3D7:
        return "Direct3D7";
    case RenderBackendType::Direct3D11:
        return "Direct3D11";
    case RenderBackendType::Direct3D12:
        return "Direct3D12";
    case RenderBackendType::Vulkan:
        return "Vulkan";
    default:
        return "Unknown";
    }
}

bool IsRenderBackendImplemented(RenderBackendType backend)
{
    switch (backend) {
    case RenderBackendType::LegacyDirect3D7:
    case RenderBackendType::Direct3D11:
    case RenderBackendType::Direct3D12:
        return true;

    case RenderBackendType::Vulkan:
        return RO_HAS_VULKAN != 0;

    default:
        return false;
    }
}

bool IsRenderBackendSupported(RenderBackendType backend)
{
    if (!IsBackendAllowedOnCurrentBuild(backend)) {
        return false;
    }

    switch (backend) {
    case RenderBackendType::LegacyDirect3D7:
        return true;

    case RenderBackendType::Direct3D11:
        if (g_supportCache.d3d11 < 0) {
            g_supportCache.d3d11 = ProbeD3D11Support() ? 1 : 0;
        }
        return g_supportCache.d3d11 != 0;

    case RenderBackendType::Direct3D12:
        if (g_supportCache.d3d12 < 0) {
            g_supportCache.d3d12 = ProbeD3D12Support() ? 1 : 0;
        }
        return g_supportCache.d3d12 != 0;

    case RenderBackendType::Vulkan:
        if (!IsRenderBackendImplemented(backend)) {
            return false;
        }
        if (g_supportCache.vulkan < 0) {
            g_supportCache.vulkan = ProbeVulkanSupport() ? 1 : 0;
        }
        return g_supportCache.vulkan != 0;

    default:
        return false;
    }
}

RenderBackendType GetConfiguredRenderBackend()
{
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kRenderBackendRegPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return kDefaultConfiguredBackend;
    }

    DWORD rawValue = static_cast<DWORD>(kDefaultConfiguredBackend);
    DWORD valueSize = sizeof(rawValue);
    const LONG status = RegQueryValueExA(
        key,
        kRenderBackendValueName,
        nullptr,
        nullptr,
        reinterpret_cast<BYTE*>(&rawValue),
        &valueSize);
    RegCloseKey(key);

    if (status != ERROR_SUCCESS || valueSize != sizeof(rawValue) || !IsValidStoredBackend(rawValue)) {
        return kDefaultConfiguredBackend;
    }

    return NormalizeBackendForCurrentBuild(static_cast<RenderBackendType>(rawValue));
}

bool SetConfiguredRenderBackend(RenderBackendType backend)
{
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kRenderBackendRegPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    backend = NormalizeBackendForCurrentBuild(backend);
    const DWORD rawValue = static_cast<DWORD>(backend);
    const LONG status = RegSetValueExA(
        key,
        kRenderBackendValueName,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&rawValue),
        sizeof(rawValue));
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

RenderBackendType GetRequestedRenderBackend()
{
    char buffer[64] = {};
    const DWORD length = GetEnvironmentVariableA("OPEN_MIDGARD_RENDER_BACKEND", buffer, static_cast<DWORD>(sizeof(buffer)));
    if (length == 0 || length >= sizeof(buffer)) {
        return GetConfiguredRenderBackend();
    }
    return NormalizeBackendForCurrentBuild(ParseRenderBackendName(buffer));
}

bool InitializeRenderBackend(HWND hwnd, RenderBackendBootstrapResult* outResult)
{
    return GetRenderDevice().Initialize(hwnd, outResult);
}