#include "RenderDevice.h"

#include "Device.h"
#include "D3dutil.h"
#include "DebugLog.h"
#include "ModernRenderState.h"
#include "render/Renderer.h"
#include "res/Texture.h"

#include <d3d12.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#if RO_HAS_VULKAN
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

namespace {

#if RO_HAS_VULKAN
HMODULE g_vulkanModule = nullptr;

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
PFN_vkCreateInstance vkCreateInstance = nullptr;
PFN_vkDestroyInstance vkDestroyInstance = nullptr;
PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties = nullptr;
PFN_vkCreateDevice vkCreateDevice = nullptr;
PFN_vkDestroyDevice vkDestroyDevice = nullptr;
PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
PFN_vkQueueSubmit vkQueueSubmit = nullptr;
PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;
PFN_vkCreateFence vkCreateFence = nullptr;
PFN_vkDestroyFence vkDestroyFence = nullptr;
PFN_vkResetFences vkResetFences = nullptr;
PFN_vkWaitForFences vkWaitForFences = nullptr;
PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
PFN_vkCreateImageView vkCreateImageView = nullptr;
PFN_vkDestroyImageView vkDestroyImageView = nullptr;
PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
PFN_vkFreeCommandBuffers vkFreeCommandBuffers = nullptr;
PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
PFN_vkCreateFramebuffer vkCreateFramebuffer = nullptr;
PFN_vkDestroyFramebuffer vkDestroyFramebuffer = nullptr;
PFN_vkCreateRenderPass vkCreateRenderPass = nullptr;
PFN_vkDestroyRenderPass vkDestroyRenderPass = nullptr;
PFN_vkCreateBuffer vkCreateBuffer = nullptr;
PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
PFN_vkAllocateMemory vkAllocateMemory = nullptr;
PFN_vkFreeMemory vkFreeMemory = nullptr;
PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
PFN_vkMapMemory vkMapMemory = nullptr;
PFN_vkUnmapMemory vkUnmapMemory = nullptr;
PFN_vkCmdSetViewport vkCmdSetViewport = nullptr;
PFN_vkCmdSetScissor vkCmdSetScissor = nullptr;
PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = nullptr;
PFN_vkCmdEndRenderPass vkCmdEndRenderPass = nullptr;
PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage = nullptr;
PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = nullptr;
PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;

bool LoadVulkanLoader()
{
    if (vkGetInstanceProcAddr) {
        return true;
    }

    if (!g_vulkanModule) {
        g_vulkanModule = LoadLibraryA("vulkan-1.dll");
    }
    if (!g_vulkanModule) {
        DbgLog("[Render] Vulkan loader 'vulkan-1.dll' not found.\n");
        return false;
    }

    vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(g_vulkanModule, "vkGetInstanceProcAddr"));
    if (!vkGetInstanceProcAddr) {
        DbgLog("[Render] Vulkan loader missing vkGetInstanceProcAddr.\n");
        return false;
    }

    vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(vkGetInstanceProcAddr(nullptr, "vkCreateInstance"));
    return vkCreateInstance != nullptr;
}

bool LoadVulkanInstanceFunctions(VkInstance instance)
{
    if (!vkGetInstanceProcAddr || !instance) {
        return false;
    }

    vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(vkGetInstanceProcAddr(instance, "vkDestroyInstance"));
    vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
    vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties"));
    vkEnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(vkGetInstanceProcAddr(instance, "vkEnumerateDeviceExtensionProperties"));
    vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(vkGetInstanceProcAddr(instance, "vkCreateDevice"));
    vkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR"));
    vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(vkGetInstanceProcAddr(instance, "vkDestroySurfaceKHR"));
    vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    vkGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
    vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));

    return vkDestroyInstance
        && vkEnumeratePhysicalDevices
        && vkGetPhysicalDeviceQueueFamilyProperties
        && vkGetPhysicalDeviceMemoryProperties
        && vkEnumerateDeviceExtensionProperties
        && vkCreateDevice
        && vkCreateWin32SurfaceKHR
        && vkDestroySurfaceKHR
        && vkGetPhysicalDeviceSurfaceSupportKHR
        && vkGetPhysicalDeviceSurfaceCapabilitiesKHR
        && vkGetPhysicalDeviceSurfaceFormatsKHR
        && vkGetPhysicalDeviceSurfacePresentModesKHR
        && vkGetDeviceProcAddr;
}

bool LoadVulkanDeviceFunctions(VkDevice device)
{
    if (!vkGetDeviceProcAddr || !device) {
        return false;
    }

    vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(vkGetDeviceProcAddr(device, "vkDestroyDevice"));
    vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(vkGetDeviceProcAddr(device, "vkGetDeviceQueue"));
    vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(vkGetDeviceProcAddr(device, "vkQueueSubmit"));
    vkDeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(vkGetDeviceProcAddr(device, "vkDeviceWaitIdle"));
    vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(vkGetDeviceProcAddr(device, "vkCreateFence"));
    vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(vkGetDeviceProcAddr(device, "vkDestroyFence"));
    vkResetFences = reinterpret_cast<PFN_vkResetFences>(vkGetDeviceProcAddr(device, "vkResetFences"));
    vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(vkGetDeviceProcAddr(device, "vkWaitForFences"));
    vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(vkGetDeviceProcAddr(device, "vkCreateSemaphore"));
    vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(vkGetDeviceProcAddr(device, "vkDestroySemaphore"));
    vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(vkGetDeviceProcAddr(device, "vkCreateImageView"));
    vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(vkGetDeviceProcAddr(device, "vkDestroyImageView"));
    vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(vkGetDeviceProcAddr(device, "vkCreateCommandPool"));
    vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(vkGetDeviceProcAddr(device, "vkDestroyCommandPool"));
    vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers"));
    vkFreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(vkGetDeviceProcAddr(device, "vkFreeCommandBuffers"));
    vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(vkGetDeviceProcAddr(device, "vkBeginCommandBuffer"));
    vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(vkGetDeviceProcAddr(device, "vkEndCommandBuffer"));
    vkResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(vkGetDeviceProcAddr(device, "vkResetCommandBuffer"));
    vkCreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(vkGetDeviceProcAddr(device, "vkCreateFramebuffer"));
    vkDestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(vkGetDeviceProcAddr(device, "vkDestroyFramebuffer"));
    vkCreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(vkGetDeviceProcAddr(device, "vkCreateRenderPass"));
    vkDestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(vkGetDeviceProcAddr(device, "vkDestroyRenderPass"));
    vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(vkGetDeviceProcAddr(device, "vkCreateBuffer"));
    vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(vkGetDeviceProcAddr(device, "vkDestroyBuffer"));
    vkGetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(vkGetDeviceProcAddr(device, "vkGetBufferMemoryRequirements"));
    vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(vkGetDeviceProcAddr(device, "vkAllocateMemory"));
    vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(vkGetDeviceProcAddr(device, "vkFreeMemory"));
    vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(vkGetDeviceProcAddr(device, "vkBindBufferMemory"));
    vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(vkGetDeviceProcAddr(device, "vkMapMemory"));
    vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(vkGetDeviceProcAddr(device, "vkUnmapMemory"));
    vkCmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(vkGetDeviceProcAddr(device, "vkCmdSetViewport"));
    vkCmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(vkGetDeviceProcAddr(device, "vkCmdSetScissor"));
    vkCmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(vkGetDeviceProcAddr(device, "vkCmdBeginRenderPass"));
    vkCmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(vkGetDeviceProcAddr(device, "vkCmdEndRenderPass"));
    vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier"));
    vkCmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(vkGetDeviceProcAddr(device, "vkCmdCopyBufferToImage"));
    vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR"));
    vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR"));
    vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(vkGetDeviceProcAddr(device, "vkGetSwapchainImagesKHR"));
    vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR"));
    vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(device, "vkQueuePresentKHR"));

    return vkDestroyDevice
        && vkGetDeviceQueue
        && vkQueueSubmit
        && vkDeviceWaitIdle
        && vkCreateFence
        && vkDestroyFence
        && vkResetFences
        && vkWaitForFences
        && vkCreateSemaphore
        && vkDestroySemaphore
        && vkCreateImageView
        && vkDestroyImageView
        && vkCreateCommandPool
        && vkDestroyCommandPool
        && vkAllocateCommandBuffers
        && vkFreeCommandBuffers
        && vkBeginCommandBuffer
        && vkEndCommandBuffer
        && vkResetCommandBuffer
        && vkCreateFramebuffer
        && vkDestroyFramebuffer
        && vkCreateRenderPass
        && vkDestroyRenderPass
        && vkCreateBuffer
        && vkDestroyBuffer
        && vkGetBufferMemoryRequirements
        && vkAllocateMemory
        && vkFreeMemory
        && vkBindBufferMemory
        && vkMapMemory
        && vkUnmapMemory
        && vkCmdSetViewport
        && vkCmdSetScissor
        && vkCmdBeginRenderPass
        && vkCmdEndRenderPass
        && vkCmdPipelineBarrier
        && vkCmdCopyBufferToImage
        && vkCreateSwapchainKHR
        && vkDestroySwapchainKHR
        && vkGetSwapchainImagesKHR
        && vkAcquireNextImageKHR
        && vkQueuePresentKHR;
}
#endif

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

    if (texture->m_backendTextureUpload) {
        texture->m_backendTextureUpload->Release();
        texture->m_backendTextureUpload = nullptr;
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

D3D12_BLEND ConvertBlendFactor12(D3DBLEND blend)
{
    switch (blend) {
    case D3DBLEND_ZERO:
        return D3D12_BLEND_ZERO;
    case D3DBLEND_ONE:
        return D3D12_BLEND_ONE;
    case D3DBLEND_SRCCOLOR:
        return D3D12_BLEND_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR:
        return D3D12_BLEND_INV_SRC_COLOR;
    case D3DBLEND_SRCALPHA:
        return D3D12_BLEND_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA:
        return D3D12_BLEND_INV_SRC_ALPHA;
    case D3DBLEND_DESTALPHA:
        return D3D12_BLEND_DEST_ALPHA;
    case D3DBLEND_INVDESTALPHA:
        return D3D12_BLEND_INV_DEST_ALPHA;
    case D3DBLEND_DESTCOLOR:
        return D3D12_BLEND_DEST_COLOR;
    case D3DBLEND_INVDESTCOLOR:
        return D3D12_BLEND_INV_DEST_COLOR;
    default:
        return D3D12_BLEND_ONE;
    }
}

bool ShouldEnableD3D12DebugLayer()
{
#if defined(_DEBUG) || !defined(NDEBUG)
    return true;
#else
    return false;
#endif
}

void LogD3D12InitFailure(const char* stage, HRESULT hr, int width, int height)
{
    DbgLog("[Render] D3D12 init failed at %s hr=0x%08X size=%dx%d.\n",
        stage ? stage : "(unknown)",
        static_cast<unsigned int>(hr),
        width,
        height);
}

void LogD3D12ResizeFailure(HRESULT hr, int width, int height)
{
    DbgLog("[Render] D3D12 swap-chain resize failed hr=0x%08X size=%dx%d.\n",
        static_cast<unsigned int>(hr),
        width,
        height);
}

void LogD3D12PresentFailure(ID3D12Device* device, HRESULT hr, bool vertSync)
{
    const HRESULT removedReason = device ? device->GetDeviceRemovedReason() : S_OK;
    DbgLog("[Render] D3D12 present failed hr=0x%08X vsync=%d removedReason=0x%08X.\n",
        static_cast<unsigned int>(hr),
        vertSync ? 1 : 0,
        static_cast<unsigned int>(removedReason));
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

struct D3D12PrimitiveTopologyInfo {
    D3D_PRIMITIVE_TOPOLOGY topology;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType;
};

D3D12PrimitiveTopologyInfo ConvertPrimitiveTopology12(D3DPRIMITIVETYPE primitiveType)
{
    switch (primitiveType) {
    case D3DPT_POINTLIST:
        return { D3D_PRIMITIVE_TOPOLOGY_POINTLIST, D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT };
    case D3DPT_LINELIST:
        return { D3D_PRIMITIVE_TOPOLOGY_LINELIST, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE };
    case D3DPT_LINESTRIP:
        return { D3D_PRIMITIVE_TOPOLOGY_LINESTRIP, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE };
    case D3DPT_TRIANGLELIST:
        return { D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE };
    case D3DPT_TRIANGLESTRIP:
        return { D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE };
    default:
        return { D3D_PRIMITIVE_TOPOLOGY_UNDEFINED, D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED };
    }
}

UINT AlignTo(UINT value, UINT alignment)
{
    if (alignment == 0u) {
        return value;
    }
    return (value + alignment - 1u) & ~(alignment - 1u);
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

    bool UpdateBackBufferFromMemory(const void* bgraPixels, int width, int height, int pitch) override
    {
        if (!bgraPixels || width <= 0 || height <= 0 || pitch <= 0) {
            return false;
        }

        HDC dc = nullptr;
        if (!AcquireBackBufferDC(&dc) || !dc) {
            return false;
        }

        std::vector<unsigned int> packedPixels;
        const void* sourcePixels = bgraPixels;
        const int packedPitch = width * static_cast<int>(sizeof(unsigned int));
        if (pitch != packedPitch) {
            packedPixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
            for (int row = 0; row < height; ++row) {
                const unsigned char* srcRow = static_cast<const unsigned char*>(bgraPixels) + static_cast<size_t>(row) * static_cast<size_t>(pitch);
                std::memcpy(packedPixels.data() + static_cast<size_t>(row) * static_cast<size_t>(width), srcRow, static_cast<size_t>(packedPitch));
            }
            sourcePixels = packedPixels.data();
        }

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        const int blitResult = StretchDIBits(
            dc,
            0,
            0,
            width,
            height,
            0,
            0,
            width,
            height,
            sourcePixels,
            &bmi,
            DIB_RGB_COLORS,
            SRCCOPY);
        ReleaseBackBufferDC(dc);
        if (blitResult == GDI_ERROR) {
            return false;
        }
        return true;
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
                    m_samplerState(nullptr),
          m_captureDc(nullptr), m_captureBitmap(nullptr), m_captureBits(nullptr), m_captureWidth(0), m_captureHeight(0)
    {
                ResetModernFixedFunctionState(&m_pipelineState);
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
        ResetModernFixedFunctionState(&m_pipelineState);
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
        *outDc = nullptr;
        CaptureRenderTargetSnapshot();
        *outDc = m_captureDc;
        return *outDc != nullptr;
    }

    void ReleaseBackBufferDC(HDC dc) override
    {
        (void)dc;
    }

    bool UpdateBackBufferFromMemory(const void* bgraPixels, int width, int height, int pitch) override
    {
        if (!bgraPixels || width <= 0 || height <= 0 || pitch <= 0 || !m_context || !m_swapChain) {
            return false;
        }

        if (width != m_renderWidth || height != m_renderHeight) {
            return false;
        }

        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
        if (FAILED(hr) || !backBuffer) {
            SafeRelease(backBuffer);
            return false;
        }

        D3D11_BOX updateBox{};
        updateBox.left = 0;
        updateBox.top = 0;
        updateBox.front = 0;
        updateBox.right = static_cast<UINT>(width);
        updateBox.bottom = static_cast<UINT>(height);
        updateBox.back = 1;
        m_context->UpdateSubresource(backBuffer, 0, &updateBox, bgraPixels, static_cast<UINT>(pitch), 0);
        SafeRelease(backBuffer);

        if (m_renderTargetView) {
            m_context->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
            ApplyViewport();
        }

        return true;
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
        ApplyModernRenderState(&m_pipelineState, state, value);
    }

    void SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override
    {
        ApplyModernTextureStageState(&m_pipelineState, stage, type, value);
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
    struct BlendStateEntry { D3D11_BLEND_DESC desc; ID3D11BlendState* state; };
    struct DepthStateEntry { D3D11_DEPTH_STENCIL_DESC desc; ID3D11DepthStencilState* state; };
    struct RasterizerStateEntry { D3D11_RASTERIZER_DESC desc; ID3D11RasterizerState* state; };

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
        constantBufferDesc.ByteWidth = static_cast<UINT>(sizeof(ModernDrawConstants));
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
        desc.RenderTarget[0].BlendEnable = m_pipelineState.alphaBlendEnable ? TRUE : FALSE;
        desc.RenderTarget[0].SrcBlend = ConvertBlendFactor(m_pipelineState.srcBlend);
        desc.RenderTarget[0].DestBlend = ConvertBlendFactor(m_pipelineState.destBlend);
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
        desc.DepthEnable = m_pipelineState.depthEnable != D3DZB_FALSE ? TRUE : FALSE;
        desc.DepthWriteMask = m_pipelineState.depthWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
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
        switch (m_pipelineState.cullMode) {
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

    void DrawTransformedPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat,
        const void* vertices, DWORD vertexCount, const unsigned short* indices, DWORD indexCount)
    {
        if (!m_context || !m_renderTargetView || !vertices || vertexCount == 0) {
            return;
        }

        const bool isLightmap = vertexFormat == kModernLightmapFvf;
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
            convertedIndices = ::BuildTriangleFanIndices(indices, vertexCount, indexCount);
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

        ModernDrawConstants constants{};
        constants.screenWidth = static_cast<float>((std::max)(1, m_renderWidth));
        constants.screenHeight = static_cast<float>((std::max)(1, m_renderHeight));
        constants.alphaRef = static_cast<float>(m_pipelineState.alphaRef) / 255.0f;
        constants.flags = BuildModernDrawFlags(
            vertexFormat,
            m_pipelineState,
            textureViews[0] != nullptr,
            textureViews[1] != nullptr);
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
    ModernFixedFunctionState m_pipelineState;
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

class D3D12RenderDevice final : public IRenderDevice {
public:
    static constexpr UINT kFrameCount = 2;
    static constexpr UINT kSrvSlotCount = 2;
    static constexpr UINT kSrvHeapCapacity = 65536;

    D3D12RenderDevice()
        : m_hwnd(nullptr), m_renderWidth(0), m_renderHeight(0),
          m_factory(nullptr), m_device(nullptr), m_commandQueue(nullptr), m_swapChain(nullptr),
          m_rtvHeap(nullptr), m_dsvHeap(nullptr), m_srvHeap(nullptr), m_depthStencil(nullptr),
          m_commandAllocator(nullptr), m_commandList(nullptr), m_fence(nullptr),
          m_fenceEvent(nullptr), m_fenceValue(0), m_frameIndex(0), m_rtvDescriptorSize(0), m_srvDescriptorSize(0), m_srvHeapCursor(0),
          m_rootSignature(nullptr),
          m_vertexShaderTlBlob(nullptr), m_vertexShaderLmBlob(nullptr), m_pixelShaderBlob(nullptr),
                        m_captureReadbackBuffer(nullptr), m_captureDc(nullptr), m_captureBitmap(nullptr), m_captureBits(nullptr),
            m_captureWidth(0), m_captureHeight(0), m_captureRowPitch(0),
                    m_frameCommandsOpen(false), m_loggedSrvHeapExhausted(false)
    {
        ResetModernFixedFunctionState(&m_pipelineState);
        m_boundTextures[0] = nullptr;
        m_boundTextures[1] = nullptr;
        for (UINT index = 0; index < kFrameCount; ++index) {
            m_renderTargets[index] = nullptr;
        }
    }

    RenderBackendType GetBackendType() const override
    {
        return RenderBackendType::Direct3D12;
    }

    bool Initialize(HWND hwnd, RenderBackendBootstrapResult* outResult) override
    {
        Shutdown();
        m_hwnd = hwnd;
        RefreshRenderSize();

        RenderBackendBootstrapResult result{};
        result.backend = RenderBackendType::Direct3D12;
        result.initHr = static_cast<int>(E_FAIL);

        if (!m_hwnd || m_renderWidth <= 0 || m_renderHeight <= 0) {
            result.initHr = static_cast<int>(E_INVALIDARG);
            if (outResult) {
                *outResult = result;
            }
            return false;
        }

        UINT dxgiFactoryFlags = 0;
        if (ShouldEnableD3D12DebugLayer()) {
            ID3D12Debug* debugController = nullptr;
            const HRESULT debugHr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
            if (SUCCEEDED(debugHr) && debugController) {
                debugController->EnableDebugLayer();
                dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
                DbgLog("[Render] D3D12 debug layer enabled for development build.\n");
            } else {
                DbgLog("[Render] D3D12 debug layer unavailable hr=0x%08X.\n", static_cast<unsigned int>(debugHr));
            }
            SafeRelease(debugController);
        }

        HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory));
        if (FAILED(hr) || !m_factory) {
            result.initHr = static_cast<int>(hr);
            LogD3D12InitFailure("CreateDXGIFactory2", hr, m_renderWidth, m_renderHeight);
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }

        hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
        if (FAILED(hr) || !m_device) {
            IDXGIAdapter* warpAdapter = nullptr;
            if (SUCCEEDED(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter))) && warpAdapter) {
                hr = D3D12CreateDevice(warpAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
            }
            SafeRelease(warpAdapter);
        }
        if (FAILED(hr) || !m_device) {
            result.initHr = static_cast<int>(hr);
            LogD3D12InitFailure("D3D12CreateDevice", hr, m_renderWidth, m_renderHeight);
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
        if (FAILED(hr) || !m_commandQueue) {
            result.initHr = static_cast<int>(hr);
            LogD3D12InitFailure("CreateCommandQueue", hr, m_renderWidth, m_renderHeight);
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
        swapChainDesc.BufferCount = kFrameCount;
        swapChainDesc.Width = static_cast<UINT>(m_renderWidth);
        swapChainDesc.Height = static_cast<UINT>(m_renderHeight);
        swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        IDXGISwapChain1* swapChain = nullptr;
        hr = m_factory->CreateSwapChainForHwnd(m_commandQueue, m_hwnd, &swapChainDesc, nullptr, nullptr, &swapChain);
        if (SUCCEEDED(hr) && swapChain) {
            hr = swapChain->QueryInterface(IID_PPV_ARGS(&m_swapChain));
        }
        SafeRelease(swapChain);
        if (FAILED(hr) || !m_swapChain) {
            result.initHr = static_cast<int>(hr);
            LogD3D12InitFailure("CreateSwapChainForHwnd", hr, m_renderWidth, m_renderHeight);
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }

        m_factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
        rtvHeapDesc.NumDescriptors = kFrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
        if (FAILED(hr) || !m_rtvHeap) {
            result.initHr = static_cast<int>(hr);
            LogD3D12InitFailure("CreateDescriptorHeap(RTV)", hr, m_renderWidth, m_renderHeight);
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }
        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        hr = m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap));
        if (FAILED(hr) || !m_dsvHeap) {
            result.initHr = static_cast<int>(hr);
            LogD3D12InitFailure("CreateDescriptorHeap(DSV)", hr, m_renderWidth, m_renderHeight);
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
        srvHeapDesc.NumDescriptors = kSrvHeapCapacity;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr = m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap));
        if (FAILED(hr) || !m_srvHeap) {
            result.initHr = static_cast<int>(hr);
            LogD3D12InitFailure("CreateDescriptorHeap(SRV)", hr, m_renderWidth, m_renderHeight);
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }
        m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
        if (FAILED(hr) || !m_commandAllocator) {
            result.initHr = static_cast<int>(hr);
            LogD3D12InitFailure("CreateCommandAllocator", hr, m_renderWidth, m_renderHeight);
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }

        hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator, nullptr, IID_PPV_ARGS(&m_commandList));
        if (FAILED(hr) || !m_commandList) {
            result.initHr = static_cast<int>(hr);
            LogD3D12InitFailure("CreateCommandList", hr, m_renderWidth, m_renderHeight);
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }
        m_commandList->Close();

        hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        if (FAILED(hr) || !m_fence) {
            result.initHr = static_cast<int>(hr);
            LogD3D12InitFailure("CreateFence", hr, m_renderWidth, m_renderHeight);
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }

        m_fenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        if (!m_fenceEvent) {
            result.initHr = static_cast<int>(HRESULT_FROM_WIN32(GetLastError()));
            LogD3D12InitFailure("CreateEventA", static_cast<HRESULT>(result.initHr), m_renderWidth, m_renderHeight);
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }

        if (!CreatePipelineResources()) {
            result.initHr = static_cast<int>(E_FAIL);
            LogD3D12InitFailure("CreatePipelineResources", E_FAIL, m_renderWidth, m_renderHeight);
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }

        if (!RefreshRenderTargets() || !CreateDepthStencilResources()) {
            result.initHr = static_cast<int>(E_FAIL);
            LogD3D12InitFailure("RefreshRenderTargets/CreateDepthStencilResources", E_FAIL, m_renderWidth, m_renderHeight);
            Shutdown();
            if (outResult) {
                *outResult = result;
            }
            return false;
        }

        if (outResult) {
            result.initHr = static_cast<int>(S_OK);
            *outResult = result;
        }

        DbgLog("[Render] Initialized backend '%s' with %ux%u swap-chain buffers.\n",
            GetRenderBackendName(RenderBackendType::Direct3D12),
            static_cast<unsigned int>(m_renderWidth),
            static_cast<unsigned int>(m_renderHeight));
        return true;
    }

    void Shutdown() override
    {
        WaitForGpu();
        ReleaseSwapChainResources();
        ReleasePendingUploadBuffers();
        ReleaseCaptureResources();
        ReleaseCachedStates();
        ReleaseUploadPages(m_indexUploadPages);
        ReleaseUploadPages(m_vertexUploadPages);
        ReleaseUploadPages(m_constantUploadPages);
        SafeRelease(m_pixelShaderBlob);
        SafeRelease(m_vertexShaderLmBlob);
        SafeRelease(m_vertexShaderTlBlob);
        SafeRelease(m_rootSignature);
        SafeRelease(m_commandList);
        SafeRelease(m_commandAllocator);
        SafeRelease(m_fence);
        if (m_fenceEvent) {
            CloseHandle(m_fenceEvent);
            m_fenceEvent = nullptr;
        }
        SafeRelease(m_dsvHeap);
        SafeRelease(m_srvHeap);
        SafeRelease(m_rtvHeap);
        SafeRelease(m_swapChain);
        SafeRelease(m_commandQueue);
        SafeRelease(m_device);
        SafeRelease(m_factory);
        m_hwnd = nullptr;
        m_renderWidth = 0;
        m_renderHeight = 0;
        m_fenceValue = 0;
        m_frameIndex = 0;
        m_rtvDescriptorSize = 0;
        m_srvDescriptorSize = 0;
        m_frameCommandsOpen = false;
        ResetModernFixedFunctionState(&m_pipelineState);
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
        if (!EnsureFrameCommandsStarted()) {
            return -1;
        }

        const float clearColor[4] = {
            static_cast<float>((color >> 16) & 0xFFu) / 255.0f,
            static_cast<float>((color >> 8) & 0xFFu) / 255.0f,
            static_cast<float>(color & 0xFFu) / 255.0f,
            static_cast<float>((color >> 24) & 0xFFu) / 255.0f,
        };
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCurrentRtvHandle();
        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        return 0;
    }
    int ClearDepth() override
    {
        if (!EnsureFrameCommandsStarted() || !m_depthStencil || !m_dsvHeap) {
            return -1;
        }

        m_commandList->ClearDepthStencilView(GetDsvHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        return 0;
    }
    int Present(bool vertSync) override
    {
        if (!m_swapChain || !m_commandQueue) {
            return -1;
        }

        if (m_frameCommandsOpen) {
            const D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(
                GetCurrentBackBuffer(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT);
            m_commandList->ResourceBarrier(1, &barrier);
            if (FAILED(m_commandList->Close())) {
                return -1;
            }
            ID3D12CommandList* commandLists[] = { m_commandList };
            m_commandQueue->ExecuteCommandLists(1, commandLists);
            m_frameCommandsOpen = false;
        }

        const HRESULT presentHr = m_swapChain->Present(vertSync ? 1 : 0, 0);
        if (FAILED(presentHr)) {
            LogD3D12PresentFailure(m_device, presentHr, vertSync);
            return static_cast<int>(presentHr);
        }

        WaitForGpu();
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
        return static_cast<int>(presentHr);
    }
    bool AcquireBackBufferDC(HDC* outDc) override
    {
        if (!outDc) {
            return false;
        }
        *outDc = nullptr;
        if (!CaptureRenderTargetSnapshot()) {
            return false;
        }
        *outDc = m_captureDc;
        return *outDc != nullptr;
    }
    void ReleaseBackBufferDC(HDC dc) override { (void)dc; }
    bool UpdateBackBufferFromMemory(const void* bgraPixels, int width, int height, int pitch) override
    {
        if (!bgraPixels || width <= 0 || height <= 0 || pitch <= 0 || !m_device || !m_swapChain) {
            return false;
        }
        if (width != m_renderWidth || height != m_renderHeight) {
            return false;
        }
        if (!EnsureFrameCommandsStarted()) {
            return false;
        }

        const UINT uploadRowPitch = AlignTo(static_cast<UINT>(width * static_cast<int>(sizeof(unsigned int))), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        const UINT uploadBufferSize = uploadRowPitch * static_cast<UINT>(height);

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC uploadDesc{};
        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Width = uploadBufferSize;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ID3D12Resource* uploadBuffer = nullptr;
        HRESULT hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadBuffer));
        if (FAILED(hr) || !uploadBuffer) {
            SafeRelease(uploadBuffer);
            return false;
        }

        void* mapped = nullptr;
        D3D12_RANGE readRange{};
        hr = uploadBuffer->Map(0, &readRange, &mapped);
        if (FAILED(hr) || !mapped) {
            SafeRelease(uploadBuffer);
            return false;
        }

        unsigned char* dstBytes = static_cast<unsigned char*>(mapped);
        for (int row = 0; row < height; ++row) {
            const unsigned char* srcBytes = static_cast<const unsigned char*>(bgraPixels) + static_cast<size_t>(row) * static_cast<size_t>(pitch);
            std::memcpy(dstBytes + static_cast<size_t>(row) * uploadRowPitch, srcBytes, static_cast<size_t>(width) * sizeof(unsigned int));
        }
        D3D12_RANGE writtenRange{ 0, uploadBufferSize };
        uploadBuffer->Unmap(0, &writtenRange);

        D3D12_TEXTURE_COPY_LOCATION srcLocation{};
        srcLocation.pResource = uploadBuffer;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint.Offset = 0;
        srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        srcLocation.PlacedFootprint.Footprint.Width = static_cast<UINT>(width);
        srcLocation.PlacedFootprint.Footprint.Height = static_cast<UINT>(height);
        srcLocation.PlacedFootprint.Footprint.Depth = 1;
        srcLocation.PlacedFootprint.Footprint.RowPitch = uploadRowPitch;

        D3D12_TEXTURE_COPY_LOCATION dstLocation{};
        dstLocation.pResource = GetCurrentBackBuffer();
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.SubresourceIndex = 0;

        const D3D12_RESOURCE_BARRIER toCopy = TransitionBarrier(
            GetCurrentBackBuffer(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST);
        m_commandList->ResourceBarrier(1, &toCopy);
        m_commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
        const D3D12_RESOURCE_BARRIER toRender = TransitionBarrier(
            GetCurrentBackBuffer(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &toRender);

        const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCurrentRtvHandle();
        const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetDsvHandle();
        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, m_depthStencil ? &dsvHandle : nullptr);
        ApplyViewportAndScissor();

        m_pendingUploadBuffers.push_back(uploadBuffer);
        return true;
    }
    bool BeginScene() override { return EnsureFrameCommandsStarted(); }
    void EndScene() override {}
    void SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) override { (void)state; (void)matrix; }
    void SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override
    {
        ApplyModernRenderState(&m_pipelineState, state, value);
    }
    void SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override
    {
        ApplyModernTextureStageState(&m_pipelineState, stage, type, value);
    }
    void BindTexture(DWORD stage, CTexture* texture) override
    {
        if (stage < kSrvSlotCount) {
            m_boundTextures[stage] = texture;
        }
    }
    void DrawPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat, const void* vertices, DWORD vertexCount, DWORD flags) override
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

        D3D12_RESOURCE_DESC textureDesc{};
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Width = static_cast<UINT64>((std::max)(1u, requestedWidth));
        textureDesc.Height = static_cast<UINT>((std::max)(1u, requestedHeight));
        textureDesc.DepthOrArraySize = 1;
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        ID3D12Resource* textureObject = nullptr;
        HRESULT hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            nullptr,
            IID_PPV_ARGS(&textureObject));
        if (FAILED(hr) || !textureObject) {
            SafeRelease(textureObject);
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
        descriptorHeapDesc.NumDescriptors = 1;
        descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        ID3D12DescriptorHeap* descriptorHeap = nullptr;
        hr = m_device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
        if (FAILED(hr) || !descriptorHeap) {
            SafeRelease(descriptorHeap);
            SafeRelease(textureObject);
            return false;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(textureObject, &srvDesc, descriptorHeap->GetCPUDescriptorHandleForHeapStart());

        texture->m_backendTextureObject = textureObject;
        texture->m_backendTextureView = descriptorHeap;
        if (outSurfaceWidth) {
            *outSurfaceWidth = static_cast<unsigned int>(textureDesc.Width);
        }
        if (outSurfaceHeight) {
            *outSurfaceHeight = textureDesc.Height;
        }
        return true;
    }
    bool UpdateTextureResource(CTexture* texture, int x, int y, int w, int h,
        const unsigned int* data, bool skipColorKey, int pitch) override
    {
        if (!texture || !texture->m_backendTextureObject || !data || w <= 0 || h <= 0 || !m_device) {
            return false;
        }
        if (!EnsureFrameCommandsStarted()) {
            return false;
        }

        ID3D12Resource* textureObject = static_cast<ID3D12Resource*>(texture->m_backendTextureObject);
        const UINT srcPitch = static_cast<UINT>(pitch > 0 ? pitch : w * static_cast<int>(sizeof(unsigned int)));
        const UINT uploadRowPitch = AlignTo(static_cast<UINT>(w * static_cast<int>(sizeof(unsigned int))), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        const UINT uploadBufferSize = uploadRowPitch * static_cast<UINT>(h);

        std::vector<unsigned int> uploadPixels(static_cast<size_t>(w) * static_cast<size_t>(h));
        for (int row = 0; row < h; ++row) {
            const unsigned int* srcRow = reinterpret_cast<const unsigned int*>(reinterpret_cast<const unsigned char*>(data) + static_cast<size_t>(row) * srcPitch);
            unsigned int* dstRow = uploadPixels.data() + static_cast<size_t>(row) * static_cast<size_t>(w);
            for (int col = 0; col < w; ++col) {
                unsigned int pixel = srcRow[col];
                if (!skipColorKey && (pixel & 0x00FFFFFFu) == 0x00FF00FFu) {
                    pixel = 0x00000000u;
                }
                dstRow[col] = pixel;
            }
        }

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC uploadDesc{};
        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Width = uploadBufferSize;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ID3D12Resource* uploadBuffer = nullptr;
        HRESULT hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadBuffer));
        if (FAILED(hr) || !uploadBuffer) {
            SafeRelease(uploadBuffer);
            return false;
        }

        void* mapped = nullptr;
        D3D12_RANGE readRange{};
        hr = uploadBuffer->Map(0, &readRange, &mapped);
        if (FAILED(hr) || !mapped) {
            SafeRelease(uploadBuffer);
            return false;
        }

        unsigned char* dstBytes = static_cast<unsigned char*>(mapped);
        for (int row = 0; row < h; ++row) {
            const unsigned char* srcBytes = reinterpret_cast<const unsigned char*>(uploadPixels.data() + static_cast<size_t>(row) * static_cast<size_t>(w));
            std::memcpy(dstBytes + static_cast<size_t>(row) * uploadRowPitch, srcBytes, static_cast<size_t>(w) * sizeof(unsigned int));
        }
        D3D12_RANGE writtenRange{ 0, uploadBufferSize };
        uploadBuffer->Unmap(0, &writtenRange);

        D3D12_TEXTURE_COPY_LOCATION srcLocation{};
        srcLocation.pResource = uploadBuffer;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint.Offset = 0;
        srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        srcLocation.PlacedFootprint.Footprint.Width = static_cast<UINT>(w);
        srcLocation.PlacedFootprint.Footprint.Height = static_cast<UINT>(h);
        srcLocation.PlacedFootprint.Footprint.Depth = 1;
        srcLocation.PlacedFootprint.Footprint.RowPitch = uploadRowPitch;

        D3D12_TEXTURE_COPY_LOCATION dstLocation{};
        dstLocation.pResource = textureObject;
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.SubresourceIndex = 0;

        const D3D12_RESOURCE_BARRIER toCopy = TransitionBarrier(
            textureObject,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_COPY_DEST);
        m_commandList->ResourceBarrier(1, &toCopy);
        m_commandList->CopyTextureRegion(&dstLocation, static_cast<UINT>(x), static_cast<UINT>(y), 0, &srcLocation, nullptr);
        const D3D12_RESOURCE_BARRIER toShader = TransitionBarrier(
            textureObject,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_commandList->ResourceBarrier(1, &toShader);

        if (texture->m_backendTextureUpload) {
            texture->m_backendTextureUpload->Release();
        }
        texture->m_backendTextureUpload = uploadBuffer;
        uploadBuffer->AddRef();
        m_pendingUploadBuffers.push_back(uploadBuffer);
        return true;
    }

private:
    struct PipelineStateEntry {
        bool isLightmap;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType;
        D3D12_BLEND_DESC blendDesc;
        D3D12_DEPTH_STENCIL_DESC depthStencilDesc;
        D3D12_RASTERIZER_DESC rasterizerDesc;
        ID3D12PipelineState* state;
    };

    D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState) const
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = beforeState;
        barrier.Transition.StateAfter = afterState;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        return barrier;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRtvHandle() const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(m_frameIndex) * static_cast<SIZE_T>(m_rtvDescriptorSize);
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const
    {
        return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpuHandle(UINT index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(index) * static_cast<SIZE_T>(m_srvDescriptorSize);
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle(UINT index) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<UINT64>(index) * static_cast<UINT64>(m_srvDescriptorSize);
        return handle;
    }

    ID3D12Resource* GetCurrentBackBuffer() const
    {
        return m_frameIndex < kFrameCount ? m_renderTargets[m_frameIndex] : nullptr;
    }

    bool RefreshRenderTargets()
    {
        if (!m_device || !m_swapChain || !m_rtvHeap) {
            return false;
        }

        for (UINT index = 0; index < kFrameCount; ++index) {
            SafeRelease(m_renderTargets[index]);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT index = 0; index < kFrameCount; ++index) {
            HRESULT hr = m_swapChain->GetBuffer(index, IID_PPV_ARGS(&m_renderTargets[index]));
            if (FAILED(hr) || !m_renderTargets[index]) {
                return false;
            }
            m_device->CreateRenderTargetView(m_renderTargets[index], nullptr, handle);
            handle.ptr += static_cast<SIZE_T>(m_rtvDescriptorSize);
        }

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
        return true;
    }

    bool CreatePipelineResources()
    {
        if (!m_device || !m_srvHeap) {
            return false;
        }

        const char* shaderSource = GetD3D11ShaderSource();
        if (!CompileShaderBlob(shaderSource, "VSMainTL", "vs_5_0", &m_vertexShaderTlBlob)
            || !CompileShaderBlob(shaderSource, "VSMainLM", "vs_5_0", &m_vertexShaderLmBlob)
            || !CompileShaderBlob(shaderSource, "PSMain", "ps_5_0", &m_pixelShaderBlob)) {
            return false;
        }

        D3D12_DESCRIPTOR_RANGE srvRange{};
        srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors = kSrvSlotCount;
        srvRange.BaseShaderRegister = 0;
        srvRange.RegisterSpace = 0;
        srvRange.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER rootParameters[2]{};
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[0].Descriptor.ShaderRegister = 0;
        rootParameters[0].Descriptor.RegisterSpace = 0;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
        rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.ShaderRegister = 0;
        samplerDesc.RegisterSpace = 0;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
        rootSignatureDesc.NumParameters = static_cast<UINT>(std::size(rootParameters));
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = 1;
        rootSignatureDesc.pStaticSamplers = &samplerDesc;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* rootSignatureBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, &errorBlob);
        if (FAILED(hr) || !rootSignatureBlob) {
            if (errorBlob && errorBlob->GetBufferPointer()) {
                DbgLog("[Render] D3D12 root-signature serialize failed: %s\n", static_cast<const char*>(errorBlob->GetBufferPointer()));
            }
            SafeRelease(errorBlob);
            SafeRelease(rootSignatureBlob);
            return false;
        }

        hr = m_device->CreateRootSignature(0,
            rootSignatureBlob->GetBufferPointer(),
            rootSignatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&m_rootSignature));
        SafeRelease(errorBlob);
        SafeRelease(rootSignatureBlob);
        if (FAILED(hr) || !m_rootSignature) {
            return false;
        }

        if (!CreateNullSrvDescriptors()) {
            return false;
        }

        return true;
    }

    struct UploadPage {
        ID3D12Resource* resource;
        void* mapped;
        size_t size;
        size_t cursor;
    };

    bool CreateUploadBuffer(UINT size, ID3D12Resource** outResource, void** outMapped, UINT64* outGpuAddress)
    {
        if ((!outResource || !outMapped) || !m_device || size == 0u) {
            return false;
        }

        *outResource = nullptr;
        *outMapped = nullptr;
        if (outGpuAddress) {
            *outGpuAddress = 0;
        }

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = size;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(outResource));
        if (FAILED(hr) || !*outResource) {
            return false;
        }

        D3D12_RANGE readRange{};
        hr = (*outResource)->Map(0, &readRange, outMapped);
        if (FAILED(hr) || !*outMapped) {
            SafeRelease(*outResource);
            return false;
        }

        if (outGpuAddress) {
            *outGpuAddress = (*outResource)->GetGPUVirtualAddress();
        }
        return true;
    }

    void ReleaseUploadPages(std::vector<UploadPage>& pages)
    {
        for (UploadPage& page : pages) {
            if (page.resource && page.mapped) {
                D3D12_RANGE writtenRange{};
                page.resource->Unmap(0, &writtenRange);
            }
            SafeRelease(page.resource);
            page.mapped = nullptr;
            page.size = 0;
            page.cursor = 0;
        }
        pages.clear();
    }

    void ResetUploadPageCursors(std::vector<UploadPage>& pages)
    {
        for (UploadPage& page : pages) {
            page.cursor = 0;
        }
    }

    static size_t AlignUploadOffset(size_t value, size_t alignment)
    {
        if (alignment <= 1) {
            return value;
        }
        const size_t mask = alignment - 1;
        return (value + mask) & ~mask;
    }

    bool AllocateUploadSlice(std::vector<UploadPage>& pages,
        size_t requiredSize,
        size_t alignment,
        size_t minimumPageSize,
        void** outMapped,
        UINT64* outGpuAddress)
    {
        if (!outMapped || !outGpuAddress || requiredSize == 0 || !m_device) {
            return false;
        }

        for (UploadPage& page : pages) {
            const size_t alignedOffset = AlignUploadOffset(page.cursor, alignment);
            if (alignedOffset + requiredSize <= page.size) {
                *outMapped = static_cast<unsigned char*>(page.mapped) + alignedOffset;
                *outGpuAddress = page.resource->GetGPUVirtualAddress() + alignedOffset;
                page.cursor = alignedOffset + requiredSize;
                return true;
            }
        }

        const size_t targetSize = (std::max)(minimumPageSize, requiredSize);
        const size_t pageSize = AlignUploadOffset(targetSize, alignment);
        ID3D12Resource* resource = nullptr;
        void* mapped = nullptr;
        if (!CreateUploadBuffer(static_cast<UINT>(pageSize), &resource, &mapped, nullptr)) {
            return false;
        }

        pages.push_back({ resource, mapped, pageSize, 0 });
        UploadPage& page = pages.back();
        *outMapped = page.mapped;
        *outGpuAddress = page.resource->GetGPUVirtualAddress();
        page.cursor = requiredSize;
        return true;
    }

    bool AllocateConstantBufferSlice(size_t requiredSize, void** outMapped, UINT64* outGpuAddress)
    {
        return AllocateUploadSlice(
            m_constantUploadPages,
            AlignUploadOffset(requiredSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT),
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT,
            64u * 1024u,
            outMapped,
            outGpuAddress);
    }

    bool AllocateVertexBufferSlice(size_t requiredSize, void** outMapped, UINT64* outGpuAddress)
    {
        return AllocateUploadSlice(m_vertexUploadPages, requiredSize, 16u, 4u * 1024u * 1024u, outMapped, outGpuAddress);
    }

    bool AllocateIndexBufferSlice(size_t requiredSize, void** outMapped, UINT64* outGpuAddress)
    {
        return AllocateUploadSlice(m_indexUploadPages, requiredSize, 4u, 512u * 1024u, outMapped, outGpuAddress);
    }

    void UnmapUploadBuffer(ID3D12Resource* resource, void** mapped)
    {
        if (resource && mapped && *mapped) {
            D3D12_RANGE writtenRange{};
            resource->Unmap(0, &writtenRange);
            *mapped = nullptr;
        }
    }

    bool CreateNullSrvDescriptors()
    {
        if (!m_device || !m_srvHeap) {
            return false;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        for (UINT index = 0; index < kSrvHeapCapacity; ++index) {
            m_device->CreateShaderResourceView(nullptr, &srvDesc, GetSrvCpuHandle(index));
        }
        return true;
    }

    void WriteNullSrvDescriptor(UINT index)
    {
        if (!m_device || !m_srvHeap || index >= kSrvHeapCapacity) {
            return;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(nullptr, &srvDesc, GetSrvCpuHandle(index));
    }

    bool CopyTextureDescriptor(UINT index, CTexture* texture)
    {
        if (!m_device || !m_srvHeap || index >= kSrvHeapCapacity || !texture) {
            WriteNullSrvDescriptor(index);
            return false;
        }

        ID3D12Resource* textureObject = static_cast<ID3D12Resource*>(texture->m_backendTextureObject);
        if (!textureObject) {
            WriteNullSrvDescriptor(index);
            return false;
        }

        D3D12_RESOURCE_DESC textureDesc = textureObject->GetDesc();
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        m_device->CreateShaderResourceView(textureObject, &srvDesc, GetSrvCpuHandle(index));
        return true;
    }

    bool ReserveSrvTable(UINT descriptorCount, UINT* outBaseIndex)
    {
        if (!outBaseIndex || descriptorCount == 0 || descriptorCount > kSrvHeapCapacity) {
            return false;
        }
        if (m_srvHeapCursor + descriptorCount > kSrvHeapCapacity) {
            if (!m_loggedSrvHeapExhausted) {
                m_loggedSrvHeapExhausted = true;
                DbgLog("[Render] D3D12 SRV heap exhausted: cursor=%u need=%u capacity=%u.\n",
                    static_cast<unsigned int>(m_srvHeapCursor),
                    static_cast<unsigned int>(descriptorCount),
                    static_cast<unsigned int>(kSrvHeapCapacity));
            }
            return false;
        }
        *outBaseIndex = m_srvHeapCursor;
        m_srvHeapCursor += descriptorCount;
        return true;
    }

    void ReleasePendingUploadBuffers()
    {
        for (ID3D12Resource* resource : m_pendingUploadBuffers) {
            SafeRelease(resource);
        }
        m_pendingUploadBuffers.clear();
    }

    void ReleaseCaptureResources()
    {
        SafeRelease(m_captureReadbackBuffer);
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
        m_captureRowPitch = 0;
    }

    bool EnsureCaptureResources()
    {
        if (!m_device || m_renderWidth <= 0 || m_renderHeight <= 0) {
            return false;
        }

        const UINT desiredRowPitch = AlignTo(static_cast<UINT>(m_renderWidth * static_cast<int>(sizeof(unsigned int))), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        const bool sizeMatches = m_captureReadbackBuffer && m_captureDc && m_captureBitmap
            && m_captureWidth == m_renderWidth && m_captureHeight == m_renderHeight && m_captureRowPitch == desiredRowPitch;
        if (sizeMatches) {
            return true;
        }

        ReleaseCaptureResources();

        const UINT64 readbackSize = static_cast<UINT64>(desiredRowPitch) * static_cast<UINT64>(m_renderHeight);
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_READBACK;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = readbackSize;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_captureReadbackBuffer));
        if (FAILED(hr) || !m_captureReadbackBuffer) {
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
        m_captureRowPitch = desiredRowPitch;
        return true;
    }

    bool CaptureRenderTargetSnapshot()
    {
        if (!EnsureCaptureResources() || !GetCurrentBackBuffer() || !EnsureFrameCommandsStarted()) {
            return false;
        }

        D3D12_TEXTURE_COPY_LOCATION srcLocation{};
        srcLocation.pResource = GetCurrentBackBuffer();
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLocation.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION dstLocation{};
        dstLocation.pResource = m_captureReadbackBuffer;
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dstLocation.PlacedFootprint.Offset = 0;
        dstLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        dstLocation.PlacedFootprint.Footprint.Width = static_cast<UINT>(m_renderWidth);
        dstLocation.PlacedFootprint.Footprint.Height = static_cast<UINT>(m_renderHeight);
        dstLocation.PlacedFootprint.Footprint.Depth = 1;
        dstLocation.PlacedFootprint.Footprint.RowPitch = m_captureRowPitch;

        const D3D12_RESOURCE_BARRIER toCopy = TransitionBarrier(
            GetCurrentBackBuffer(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        m_commandList->ResourceBarrier(1, &toCopy);
        m_commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
        const D3D12_RESOURCE_BARRIER toPresent = TransitionBarrier(
            GetCurrentBackBuffer(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_PRESENT);
        m_commandList->ResourceBarrier(1, &toPresent);

        if (FAILED(m_commandList->Close())) {
            return false;
        }

        ID3D12CommandList* commandLists[] = { m_commandList };
        m_commandQueue->ExecuteCommandLists(1, commandLists);
        m_frameCommandsOpen = false;
        WaitForGpu();

        void* mapped = nullptr;
        D3D12_RANGE readRange{ 0, static_cast<SIZE_T>(m_captureRowPitch) * static_cast<SIZE_T>(m_captureHeight) };
        HRESULT hr = m_captureReadbackBuffer->Map(0, &readRange, &mapped);
        if (FAILED(hr) || !mapped || !m_captureBits) {
            if (mapped) {
                D3D12_RANGE writtenRange{};
                m_captureReadbackBuffer->Unmap(0, &writtenRange);
            }
            return false;
        }

        const size_t dstPitch = static_cast<size_t>(m_captureWidth) * sizeof(unsigned int);
        for (int row = 0; row < m_captureHeight; ++row) {
            const unsigned char* srcRow = static_cast<const unsigned char*>(mapped) + static_cast<size_t>(row) * static_cast<size_t>(m_captureRowPitch);
            unsigned char* dstRow = static_cast<unsigned char*>(m_captureBits) + static_cast<size_t>(row) * dstPitch;
            std::memcpy(dstRow, srcRow, dstPitch);
        }
        D3D12_RANGE writtenRange{};
        m_captureReadbackBuffer->Unmap(0, &writtenRange);
        return true;
    }

    void ReleaseCachedStates()
    {
        for (PipelineStateEntry& entry : m_pipelineStates) {
            SafeRelease(entry.state);
        }
        m_pipelineStates.clear();
    }

    D3D12_BLEND_DESC BuildBlendDesc() const
    {
        D3D12_BLEND_DESC desc{};
        desc.RenderTarget[0].BlendEnable = m_pipelineState.alphaBlendEnable ? TRUE : FALSE;
        desc.RenderTarget[0].SrcBlend = ConvertBlendFactor12(m_pipelineState.srcBlend);
        desc.RenderTarget[0].DestBlend = ConvertBlendFactor12(m_pipelineState.destBlend);
        desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        return desc;
    }

    D3D12_DEPTH_STENCIL_DESC BuildDepthStencilDesc() const
    {
        D3D12_DEPTH_STENCIL_DESC desc{};
        desc.DepthEnable = m_pipelineState.depthEnable != D3DZB_FALSE ? TRUE : FALSE;
        desc.DepthWriteMask = m_pipelineState.depthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        desc.StencilEnable = FALSE;
        return desc;
    }

    D3D12_RASTERIZER_DESC BuildRasterizerDesc() const
    {
        D3D12_RASTERIZER_DESC desc{};
        desc.FillMode = D3D12_FILL_MODE_SOLID;
        switch (m_pipelineState.cullMode) {
        case D3DCULL_CW:
            desc.CullMode = D3D12_CULL_MODE_FRONT;
            break;
        case D3DCULL_CCW:
            desc.CullMode = D3D12_CULL_MODE_BACK;
            break;
        default:
            desc.CullMode = D3D12_CULL_MODE_NONE;
            break;
        }
        desc.FrontCounterClockwise = FALSE;
        desc.DepthClipEnable = TRUE;
        return desc;
    }

    ID3D12PipelineState* GetPipelineState(bool isLightmap, D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType)
    {
        if (!m_device || !m_rootSignature || !m_vertexShaderTlBlob || !m_vertexShaderLmBlob || !m_pixelShaderBlob) {
            return nullptr;
        }

        const D3D12_BLEND_DESC blendDesc = BuildBlendDesc();
        const D3D12_DEPTH_STENCIL_DESC depthStencilDesc = BuildDepthStencilDesc();
        const D3D12_RASTERIZER_DESC rasterizerDesc = BuildRasterizerDesc();
        for (PipelineStateEntry& entry : m_pipelineStates) {
            if (entry.isLightmap == isLightmap
                && entry.topologyType == topologyType
                && std::memcmp(&entry.blendDesc, &blendDesc, sizeof(blendDesc)) == 0
                && std::memcmp(&entry.depthStencilDesc, &depthStencilDesc, sizeof(depthStencilDesc)) == 0
                && std::memcmp(&entry.rasterizerDesc, &rasterizerDesc, sizeof(rasterizerDesc)) == 0) {
                return entry.state;
            }
        }

        const D3D12_INPUT_ELEMENT_DESC tlLayoutDesc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(tlvertex3d, x)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, static_cast<UINT>(offsetof(tlvertex3d, color)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(tlvertex3d, tu)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };
        const D3D12_INPUT_ELEMENT_DESC lmLayoutDesc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(lmtlvertex3d, vert) + offsetof(tlvertex3d, x)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, static_cast<UINT>(offsetof(lmtlvertex3d, vert) + offsetof(tlvertex3d, color)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(lmtlvertex3d, vert) + offsetof(tlvertex3d, tu)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(lmtlvertex3d, tu2)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = m_rootSignature;
        desc.VS = { isLightmap ? m_vertexShaderLmBlob->GetBufferPointer() : m_vertexShaderTlBlob->GetBufferPointer(),
            isLightmap ? m_vertexShaderLmBlob->GetBufferSize() : m_vertexShaderTlBlob->GetBufferSize() };
        desc.PS = { m_pixelShaderBlob->GetBufferPointer(), m_pixelShaderBlob->GetBufferSize() };
        desc.BlendState = blendDesc;
        desc.SampleMask = UINT_MAX;
        desc.RasterizerState = rasterizerDesc;
        desc.DepthStencilState = depthStencilDesc;
        desc.InputLayout = {
            isLightmap ? lmLayoutDesc : tlLayoutDesc,
            static_cast<UINT>(isLightmap ? std::size(lmLayoutDesc) : std::size(tlLayoutDesc))
        };
        desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
        desc.PrimitiveTopologyType = topologyType;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.SampleDesc.Count = 1;

        ID3D12PipelineState* state = nullptr;
        if (FAILED(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&state))) || !state) {
            return nullptr;
        }

        m_pipelineStates.push_back({ isLightmap, topologyType, blendDesc, depthStencilDesc, rasterizerDesc, state });
        return state;
    }

    void DrawTransformedPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat,
        const void* vertices, DWORD vertexCount, const unsigned short* indices, DWORD indexCount)
    {
        if (!EnsureFrameCommandsStarted() || !vertices || vertexCount == 0 || !m_commandList) {
            return;
        }

        const bool isLightmap = vertexFormat == kModernLightmapFvf;
        if (!isLightmap && vertexFormat != D3DFVF_TLVERTEX) {
            return;
        }

        std::vector<unsigned short> convertedIndices;
        const unsigned short* drawIndices = indices;
        DWORD drawIndexCount = indexCount;
        D3D12PrimitiveTopologyInfo topologyInfo = ConvertPrimitiveTopology12(primitiveType);
        if (primitiveType == D3DPT_TRIANGLEFAN) {
            convertedIndices = ::BuildTriangleFanIndices(indices, vertexCount, indexCount);
            if (convertedIndices.empty()) {
                return;
            }
            drawIndices = convertedIndices.data();
            drawIndexCount = static_cast<DWORD>(convertedIndices.size());
            topologyInfo = ConvertPrimitiveTopology12(D3DPT_TRIANGLELIST);
        }
        if (topologyInfo.topology == D3D_PRIMITIVE_TOPOLOGY_UNDEFINED
            || topologyInfo.topologyType == D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED) {
            return;
        }

        ID3D12PipelineState* pipelineState = GetPipelineState(isLightmap, topologyInfo.topologyType);
        if (!pipelineState) {
            return;
        }

        const size_t vertexStride = isLightmap ? sizeof(lmtlvertex3d) : sizeof(tlvertex3d);
        const size_t vertexBytes = vertexStride * static_cast<size_t>(vertexCount);
        void* vertexUpload = nullptr;
        UINT64 vertexGpuAddress = 0;
        if (!AllocateVertexBufferSlice(vertexBytes, &vertexUpload, &vertexGpuAddress)) {
            return;
        }
        std::memcpy(vertexUpload, vertices, vertexBytes);

        D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
        vertexBufferView.BufferLocation = vertexGpuAddress;
        vertexBufferView.SizeInBytes = static_cast<UINT>(vertexBytes);
        vertexBufferView.StrideInBytes = static_cast<UINT>(vertexStride);

        D3D12_INDEX_BUFFER_VIEW indexBufferView{};
        const bool hasIndices = drawIndices && drawIndexCount > 0;
        if (hasIndices) {
            const size_t indexBytes = static_cast<size_t>(drawIndexCount) * sizeof(unsigned short);
            void* indexUpload = nullptr;
            UINT64 indexGpuAddress = 0;
            if (!AllocateIndexBufferSlice(indexBytes, &indexUpload, &indexGpuAddress)) {
                return;
            }
            std::memcpy(indexUpload, drawIndices, indexBytes);
            indexBufferView.BufferLocation = indexGpuAddress;
            indexBufferView.SizeInBytes = static_cast<UINT>(indexBytes);
            indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        }

        UINT srvBaseIndex = 0;
        if (!ReserveSrvTable(kSrvSlotCount, &srvBaseIndex)) {
            return;
        }

        const bool hasTexture0 = CopyTextureDescriptor(srvBaseIndex + 0, m_boundTextures[0]);
        const bool hasTexture1 = CopyTextureDescriptor(srvBaseIndex + 1, m_boundTextures[1]);
        ModernDrawConstants constants{};
        constants.screenWidth = static_cast<float>((std::max)(1, m_renderWidth));
        constants.screenHeight = static_cast<float>((std::max)(1, m_renderHeight));
        constants.alphaRef = static_cast<float>(m_pipelineState.alphaRef) / 255.0f;
        constants.flags = BuildModernDrawFlags(vertexFormat, m_pipelineState, hasTexture0, hasTexture1);
        void* constantUpload = nullptr;
        UINT64 constantGpuAddress = 0;
        const size_t constantBytes = AlignTo(static_cast<UINT>(sizeof(ModernDrawConstants)), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        if (!AllocateConstantBufferSlice(constantBytes, &constantUpload, &constantGpuAddress)) {
            return;
        }
        std::memset(constantUpload, 0, constantBytes);
        std::memcpy(constantUpload, &constants, sizeof(constants));

        ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvHeap };
        m_commandList->SetDescriptorHeaps(1, descriptorHeaps);
        m_commandList->SetGraphicsRootSignature(m_rootSignature);
        m_commandList->SetPipelineState(pipelineState);
        m_commandList->SetGraphicsRootConstantBufferView(0, constantGpuAddress);
        m_commandList->SetGraphicsRootDescriptorTable(1, GetSrvGpuHandle(srvBaseIndex));
        m_commandList->IASetPrimitiveTopology(topologyInfo.topology);
        m_commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        if (hasIndices) {
            m_commandList->IASetIndexBuffer(&indexBufferView);
            m_commandList->DrawIndexedInstanced(drawIndexCount, 1, 0, 0, 0);
        } else {
            m_commandList->IASetIndexBuffer(nullptr);
            m_commandList->DrawInstanced(vertexCount, 1, 0, 0);
        }
    }

    bool CreateDepthStencilResources()
    {
        SafeRelease(m_depthStencil);
        if (!m_device || !m_dsvHeap || m_renderWidth <= 0 || m_renderHeight <= 0) {
            return false;
        }

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC depthDesc{};
        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width = static_cast<UINT64>(m_renderWidth);
        depthDesc.Height = static_cast<UINT>(m_renderHeight);
        depthDesc.DepthOrArraySize = 1;
        depthDesc.MipLevels = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        const HRESULT hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&m_depthStencil));
        if (FAILED(hr) || !m_depthStencil) {
            return false;
        }

        m_device->CreateDepthStencilView(m_depthStencil, nullptr, GetDsvHandle());
        return true;
    }

    void ReleaseSwapChainResources()
    {
        SafeRelease(m_depthStencil);
        for (UINT index = 0; index < kFrameCount; ++index) {
            SafeRelease(m_renderTargets[index]);
        }
    }

    void WaitForGpu()
    {
        if (!m_commandQueue || !m_fence || !m_fenceEvent) {
            return;
        }

        const UINT64 fenceValue = ++m_fenceValue;
        if (FAILED(m_commandQueue->Signal(m_fence, fenceValue))) {
            return;
        }
        if (m_fence->GetCompletedValue() < fenceValue) {
            if (SUCCEEDED(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent))) {
                WaitForSingleObject(m_fenceEvent, INFINITE);
            }
        }
        ReleasePendingUploadBuffers();
    }

    void ApplyViewportAndScissor()
    {
        if (!m_commandList || m_renderWidth <= 0 || m_renderHeight <= 0) {
            return;
        }

        D3D12_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(m_renderWidth);
        viewport.Height = static_cast<float>(m_renderHeight);
        viewport.MaxDepth = 1.0f;
        D3D12_RECT scissor{ 0, 0, m_renderWidth, m_renderHeight };
        m_commandList->RSSetViewports(1, &viewport);
        m_commandList->RSSetScissorRects(1, &scissor);
    }

    bool EnsureFrameCommandsStarted()
    {
        if (m_frameCommandsOpen) {
            return true;
        }
        if (!m_commandAllocator || !m_commandList || !GetCurrentBackBuffer()) {
            return false;
        }

        HRESULT hr = m_commandAllocator->Reset();
        if (FAILED(hr)) {
            return false;
        }
        hr = m_commandList->Reset(m_commandAllocator, nullptr);
        if (FAILED(hr)) {
            return false;
        }

        const D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(
            GetCurrentBackBuffer(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &barrier);
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCurrentRtvHandle();
        const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetDsvHandle();
        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, m_depthStencil ? &dsvHandle : nullptr);
        ApplyViewportAndScissor();
        m_srvHeapCursor = 0;
        m_loggedSrvHeapExhausted = false;
        ResetUploadPageCursors(m_constantUploadPages);
        ResetUploadPageCursors(m_vertexUploadPages);
        ResetUploadPageCursors(m_indexUploadPages);
        m_frameCommandsOpen = true;
        return true;
    }

    void ResizeSwapChainBuffers()
    {
        if (!m_swapChain || m_renderWidth <= 0 || m_renderHeight <= 0) {
            return;
        }

        WaitForGpu();
        m_frameCommandsOpen = false;
        ReleaseCaptureResources();
        ReleaseSwapChainResources();
        const HRESULT hr = m_swapChain->ResizeBuffers(
            kFrameCount,
            static_cast<UINT>(m_renderWidth),
            static_cast<UINT>(m_renderHeight),
            DXGI_FORMAT_B8G8R8A8_UNORM,
            0);
        if (FAILED(hr)) {
            LogD3D12ResizeFailure(hr, m_renderWidth, m_renderHeight);
            return;
        }

        RefreshRenderTargets();
        CreateDepthStencilResources();
    }

    HWND m_hwnd;
    int m_renderWidth;
    int m_renderHeight;
    IDXGIFactory4* m_factory;
    ID3D12Device* m_device;
    ID3D12CommandQueue* m_commandQueue;
    IDXGISwapChain3* m_swapChain;
    ID3D12DescriptorHeap* m_rtvHeap;
    ID3D12DescriptorHeap* m_dsvHeap;
    ID3D12DescriptorHeap* m_srvHeap;
    ID3D12Resource* m_renderTargets[kFrameCount];
    ID3D12Resource* m_depthStencil;
    ID3D12CommandAllocator* m_commandAllocator;
    ID3D12GraphicsCommandList* m_commandList;
    ID3D12Fence* m_fence;
    HANDLE m_fenceEvent;
    UINT64 m_fenceValue;
    UINT m_frameIndex;
    UINT m_rtvDescriptorSize;
    UINT m_srvDescriptorSize;
    UINT m_srvHeapCursor;
    ID3D12RootSignature* m_rootSignature;
    ID3DBlob* m_vertexShaderTlBlob;
    ID3DBlob* m_vertexShaderLmBlob;
    ID3DBlob* m_pixelShaderBlob;
    std::vector<UploadPage> m_constantUploadPages;
    std::vector<UploadPage> m_vertexUploadPages;
    std::vector<UploadPage> m_indexUploadPages;
    ID3D12Resource* m_captureReadbackBuffer;
    HDC m_captureDc;
    HBITMAP m_captureBitmap;
    void* m_captureBits;
    int m_captureWidth;
    int m_captureHeight;
    UINT m_captureRowPitch;
    ModernFixedFunctionState m_pipelineState;
    CTexture* m_boundTextures[kSrvSlotCount];
    std::vector<PipelineStateEntry> m_pipelineStates;
    std::vector<ID3D12Resource*> m_pendingUploadBuffers;
    bool m_frameCommandsOpen;
    bool m_loggedSrvHeapExhausted;
};

class VulkanRenderDevice final : public IRenderDevice {
public:
#if RO_HAS_VULKAN
    VulkanRenderDevice()
        : m_hwnd(nullptr), m_renderWidth(0), m_renderHeight(0),
          m_instance(VK_NULL_HANDLE), m_surface(VK_NULL_HANDLE), m_physicalDevice(VK_NULL_HANDLE),
          m_device(VK_NULL_HANDLE), m_graphicsQueue(VK_NULL_HANDLE), m_presentQueue(VK_NULL_HANDLE),
          m_swapChain(VK_NULL_HANDLE), m_swapChainFormat(VK_FORMAT_UNDEFINED),
          m_renderPass(VK_NULL_HANDLE), m_commandPool(VK_NULL_HANDLE),
          m_imageAvailableSemaphore(VK_NULL_HANDLE), m_renderFinishedSemaphore(VK_NULL_HANDLE),
          m_inFlightFence(VK_NULL_HANDLE), m_graphicsQueueFamilyIndex(kInvalidQueueFamilyIndex),
          m_presentQueueFamilyIndex(kInvalidQueueFamilyIndex), m_currentImageIndex(0),
          m_frameBegun(false), m_renderPassActive(false), m_pendingDepthClear(false)
    {
        m_bootstrap.backend = RenderBackendType::Vulkan;
        m_bootstrap.initHr = static_cast<int>(E_NOTIMPL);
        std::memset(&m_pendingClearColor, 0, sizeof(m_pendingClearColor));
    }

    RenderBackendType GetBackendType() const override
    {
        return RenderBackendType::Vulkan;
    }

    bool Initialize(HWND hwnd, RenderBackendBootstrapResult* outResult) override
    {
        Shutdown();
        m_hwnd = hwnd;
        RefreshRenderSize();

        m_bootstrap.backend = RenderBackendType::Vulkan;
        m_bootstrap.initHr = static_cast<int>(VK_ERROR_INITIALIZATION_FAILED);

        if (!m_hwnd || m_renderWidth <= 0 || m_renderHeight <= 0) {
            m_bootstrap.initHr = static_cast<int>(VK_ERROR_INITIALIZATION_FAILED);
            if (outResult) {
                *outResult = m_bootstrap;
            }
            return false;
        }

        if (!LoadVulkanLoader()) {
            m_bootstrap.initHr = static_cast<int>(E_NOINTERFACE);
            if (outResult) {
                *outResult = m_bootstrap;
            }
            return false;
        }

        if (!CreateInstance()) {
            if (outResult) {
                *outResult = m_bootstrap;
            }
            Shutdown();
            return false;
        }
        if (!CreateSurface()) {
            if (outResult) {
                *outResult = m_bootstrap;
            }
            Shutdown();
            return false;
        }
        if (!PickPhysicalDevice()) {
            if (outResult) {
                *outResult = m_bootstrap;
            }
            Shutdown();
            return false;
        }
        if (!CreateLogicalDevice()) {
            if (outResult) {
                *outResult = m_bootstrap;
            }
            Shutdown();
            return false;
        }
        if (!CreateCommandPool()) {
            if (outResult) {
                *outResult = m_bootstrap;
            }
            Shutdown();
            return false;
        }
        if (!CreateSyncObjects()) {
            if (outResult) {
                *outResult = m_bootstrap;
            }
            Shutdown();
            return false;
        }
        if (!CreateSwapChainResources(VK_NULL_HANDLE)) {
            if (outResult) {
                *outResult = m_bootstrap;
            }
            Shutdown();
            return false;
        }

        m_bootstrap.initHr = static_cast<int>(VK_SUCCESS);
        DbgLog("[Render] Initialized backend '%s' with %ux%u swapchain.\n",
            GetRenderBackendName(RenderBackendType::Vulkan),
            static_cast<unsigned int>(m_swapChainExtent.width),
            static_cast<unsigned int>(m_swapChainExtent.height));

        if (outResult) {
            *outResult = m_bootstrap;
        }
        return true;
    }

    void Shutdown() override
    {
        if (m_device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_device);
        }

        DestroySwapChainResources();

        if (m_device != VK_NULL_HANDLE && m_imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr);
            m_imageAvailableSemaphore = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphore, nullptr);
            m_renderFinishedSemaphore = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, m_inFlightFence, nullptr);
            m_inFlightFence = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE) {
            vkDestroyDevice(m_device, nullptr);
            m_device = VK_NULL_HANDLE;
        }
        if (m_surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
        }
        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
        }

        m_hwnd = nullptr;
        m_renderWidth = 0;
        m_renderHeight = 0;
        m_swapChainFormat = VK_FORMAT_UNDEFINED;
        m_swapChainExtent = {};
        m_bootstrap.backend = RenderBackendType::Vulkan;
        m_bootstrap.initHr = static_cast<int>(E_NOTIMPL);
        m_physicalDevice = VK_NULL_HANDLE;
        m_graphicsQueue = VK_NULL_HANDLE;
        m_presentQueue = VK_NULL_HANDLE;
        m_graphicsQueueFamilyIndex = kInvalidQueueFamilyIndex;
        m_presentQueueFamilyIndex = kInvalidQueueFamilyIndex;
        m_currentImageIndex = 0;
        m_frameBegun = false;
        m_renderPassActive = false;
        m_pendingDepthClear = false;
        std::memset(&m_pendingClearColor, 0, sizeof(m_pendingClearColor));
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

    int GetRenderWidth() const override { return m_renderWidth; }
    int GetRenderHeight() const override { return m_renderHeight; }
    HWND GetWindowHandle() const override { return m_hwnd; }
    IDirect3DDevice7* GetLegacyDevice() const override { return nullptr; }

    int ClearColor(unsigned int color) override
    {
        m_pendingClearColor.float32[0] = static_cast<float>((color >> 16) & 0xFFu) / 255.0f;
        m_pendingClearColor.float32[1] = static_cast<float>((color >> 8) & 0xFFu) / 255.0f;
        m_pendingClearColor.float32[2] = static_cast<float>(color & 0xFFu) / 255.0f;
        m_pendingClearColor.float32[3] = static_cast<float>((color >> 24) & 0xFFu) / 255.0f;
        return EnsureFrameStarted() ? 0 : -1;
    }

    int ClearDepth() override
    {
        m_pendingDepthClear = true;
        return 0;
    }

    int Present(bool vertSync) override
    {
        if (!m_device || !m_swapChain || !m_frameBegun) {
            return -1;
        }

        if (m_renderPassActive) {
            vkCmdEndRenderPass(GetCurrentCommandBuffer());
            m_renderPassActive = false;
        }

        const VkResult endResult = vkEndCommandBuffer(GetCurrentCommandBuffer());
        if (endResult != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(endResult);
            m_frameBegun = false;
            return -1;
        }

        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_imageAvailableSemaphore;
        submitInfo.pWaitDstStageMask = &waitStage;
        const VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_renderFinishedSemaphore;

        VkResult result = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFence);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            m_frameBegun = false;
            return -1;
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_renderFinishedSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_swapChain;
        presentInfo.pImageIndices = &m_currentImageIndex;
        result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

        m_frameBegun = false;
        m_pendingDepthClear = false;

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            ResizeSwapChain();
            return 0;
        }
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return -1;
        }

        return vertSync ? 1 : 0;
    }

    bool AcquireBackBufferDC(HDC* outDc) override
    {
        if (outDc) {
            *outDc = nullptr;
        }
        return false;
    }

    void ReleaseBackBufferDC(HDC dc) override
    {
        (void)dc;
    }

    bool UpdateBackBufferFromMemory(const void* bgraPixels, int width, int height, int pitch) override
    {
        if (!bgraPixels || width <= 0 || height <= 0 || pitch <= 0) {
            return false;
        }
        if (width != m_renderWidth || height != m_renderHeight) {
            return false;
        }
        if (!EnsureTransferFrameStarted()) {
            return false;
        }

        const VkDeviceSize uploadSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * sizeof(unsigned int);
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        if (!CreateStagingBuffer(uploadSize, &stagingBuffer, &stagingMemory)) {
            return false;
        }

        void* mapped = nullptr;
        VkResult result = vkMapMemory(m_device, stagingMemory, 0, uploadSize, 0, &mapped);
        if (result != VK_SUCCESS || !mapped) {
            if (mapped) {
                vkUnmapMemory(m_device, stagingMemory);
            }
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            vkFreeMemory(m_device, stagingMemory, nullptr);
            m_bootstrap.initHr = static_cast<int>(result);
            return false;
        }

        unsigned char* dstBytes = static_cast<unsigned char*>(mapped);
        const size_t dstPitch = static_cast<size_t>(width) * sizeof(unsigned int);
        for (int row = 0; row < height; ++row) {
            const unsigned char* srcRow = static_cast<const unsigned char*>(bgraPixels) + static_cast<size_t>(row) * static_cast<size_t>(pitch);
            std::memcpy(dstBytes + static_cast<size_t>(row) * dstPitch, srcRow, dstPitch);
        }
        vkUnmapMemory(m_device, stagingMemory);

        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.srcAccessMask = 0;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = m_swapChainImages[m_currentImageIndex];
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.baseMipLevel = 0;
        toTransfer.subresourceRange.levelCount = 1;
        toTransfer.subresourceRange.baseArrayLayer = 0;
        toTransfer.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(
            GetCurrentCommandBuffer(),
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &toTransfer);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = static_cast<uint32_t>(width);
        copyRegion.bufferImageHeight = static_cast<uint32_t>(height);
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageOffset = { 0, 0, 0 };
        copyRegion.imageExtent.width = static_cast<uint32_t>(width);
        copyRegion.imageExtent.height = static_cast<uint32_t>(height);
        copyRegion.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(
            GetCurrentCommandBuffer(),
            stagingBuffer,
            m_swapChainImages[m_currentImageIndex],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion);

        VkImageMemoryBarrier toPresent{};
        toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toPresent.dstAccessMask = 0;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.image = m_swapChainImages[m_currentImageIndex];
        toPresent.subresourceRange = toTransfer.subresourceRange;
        vkCmdPipelineBarrier(
            GetCurrentCommandBuffer(),
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &toPresent);

        m_pendingReleaseBuffers.push_back(stagingBuffer);
        m_pendingReleaseMemory.push_back(stagingMemory);
        return true;
    }

    bool BeginScene() override { return EnsureFrameStarted(); }
    void EndScene() override {}

    void SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) override
    {
        (void)state;
        (void)matrix;
    }

    void SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override
    {
        (void)state;
        (void)value;
    }

    void SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override
    {
        (void)stage;
        (void)type;
        (void)value;
    }

    void BindTexture(DWORD stage, CTexture* texture) override
    {
        (void)stage;
        (void)texture;
    }

    void DrawPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat,
        const void* vertices, DWORD vertexCount, DWORD flags) override
    {
        (void)primitiveType;
        (void)vertexFormat;
        (void)vertices;
        (void)vertexCount;
        (void)flags;
    }

    void DrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat,
        const void* vertices, DWORD vertexCount, const unsigned short* indices,
        DWORD indexCount, DWORD flags) override
    {
        (void)primitiveType;
        (void)vertexFormat;
        (void)vertices;
        (void)vertexCount;
        (void)indices;
        (void)indexCount;
        (void)flags;
    }

    void AdjustTextureSize(unsigned int* width, unsigned int* height) override
    {
        if (!width || !height) {
            return;
        }

        *width = (std::max)(1u, *width);
        *height = (std::max)(1u, *height);
    }

    void ReleaseTextureResource(CTexture* texture) override
    {
        (void)texture;
    }

    bool CreateTextureResource(CTexture* texture, unsigned int requestedWidth, unsigned int requestedHeight,
        int pixelFormat, unsigned int* outSurfaceWidth, unsigned int* outSurfaceHeight) override
    {
        (void)texture;
        (void)requestedWidth;
        (void)requestedHeight;
        (void)pixelFormat;
        if (outSurfaceWidth) {
            *outSurfaceWidth = 0;
        }
        if (outSurfaceHeight) {
            *outSurfaceHeight = 0;
        }
        return false;
    }

    bool UpdateTextureResource(CTexture* texture, int x, int y, int w, int h,
        const unsigned int* data, bool skipColorKey, int pitch) override
    {
        (void)texture;
        (void)x;
        (void)y;
        (void)w;
        (void)h;
        (void)data;
        (void)skipColorKey;
        (void)pitch;
        return false;
    }

private:
    static constexpr uint32_t kInvalidQueueFamilyIndex = std::numeric_limits<uint32_t>::max();

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    bool CreateInstance()
    {
        if (!vkCreateInstance) {
            m_bootstrap.initHr = static_cast<int>(E_NOINTERFACE);
            return false;
        }

        const char* extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        };

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "HighPriest";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName = "OpenMidgard";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(std::size(extensions));
        createInfo.ppEnabledExtensionNames = extensions;

        const VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DbgLog("[Render] Vulkan vkCreateInstance failed (vk=%d).\n", static_cast<int>(result));
            return false;
        }

        if (!LoadVulkanInstanceFunctions(m_instance)) {
            m_bootstrap.initHr = static_cast<int>(E_NOINTERFACE);
            DbgLog("[Render] Vulkan instance function resolution failed.\n");
            return false;
        }
        return true;
    }

    bool CreateSurface()
    {
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hinstance = GetModuleHandleA(nullptr);
        createInfo.hwnd = m_hwnd;

        const VkResult result = vkCreateWin32SurfaceKHR(m_instance, &createInfo, nullptr, &m_surface);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DbgLog("[Render] Vulkan vkCreateWin32SurfaceKHR failed (vk=%d).\n", static_cast<int>(result));
            return false;
        }
        return true;
    }

    bool PickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        VkResult result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        if (result != VK_SUCCESS || deviceCount == 0) {
            m_bootstrap.initHr = static_cast<int>(result != VK_SUCCESS ? result : VK_ERROR_INITIALIZATION_FAILED);
            DbgLog("[Render] Vulkan physical device enumeration failed (vk=%d, count=%u).\n",
                static_cast<int>(result), static_cast<unsigned int>(deviceCount));
            return false;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return false;
        }

        for (VkPhysicalDevice device : devices) {
            uint32_t graphicsIndex = kInvalidQueueFamilyIndex;
            uint32_t presentIndex = kInvalidQueueFamilyIndex;
            if (IsPhysicalDeviceSuitable(device, &graphicsIndex, &presentIndex)) {
                m_physicalDevice = device;
                m_graphicsQueueFamilyIndex = graphicsIndex;
                m_presentQueueFamilyIndex = presentIndex;
                return true;
            }
        }

        m_bootstrap.initHr = static_cast<int>(VK_ERROR_FEATURE_NOT_PRESENT);
        DbgLog("[Render] Vulkan did not find a suitable physical device.\n");
        return false;
    }

    bool IsPhysicalDeviceSuitable(VkPhysicalDevice device, uint32_t* outGraphicsIndex, uint32_t* outPresentIndex) const
    {
        if (!outGraphicsIndex || !outPresentIndex) {
            return false;
        }

        *outGraphicsIndex = kInvalidQueueFamilyIndex;
        *outPresentIndex = kInvalidQueueFamilyIndex;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        if (queueFamilyCount == 0) {
            return false;
        }

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        for (uint32_t index = 0; index < queueFamilyCount; ++index) {
            if ((queueFamilies[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                *outGraphicsIndex = index;
            }

            VkBool32 presentSupported = VK_FALSE;
            const VkResult presentResult = vkGetPhysicalDeviceSurfaceSupportKHR(device, index, m_surface, &presentSupported);
            if (presentResult == VK_SUCCESS && presentSupported == VK_TRUE) {
                *outPresentIndex = index;
            }
        }

        if (*outGraphicsIndex == kInvalidQueueFamilyIndex || *outPresentIndex == kInvalidQueueFamilyIndex) {
            return false;
        }

        uint32_t extensionCount = 0;
        const VkResult extResult = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        if (extResult != VK_SUCCESS || extensionCount == 0) {
            return false;
        }

        std::vector<VkExtensionProperties> extensions(extensionCount);
        if (vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data()) != VK_SUCCESS) {
            return false;
        }

        bool hasSwapchainExtension = false;
        for (const VkExtensionProperties& extension : extensions) {
            if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchainExtension = true;
                break;
            }
        }
        if (!hasSwapchainExtension) {
            return false;
        }

        const SwapChainSupportDetails support = QuerySwapChainSupport(device);
        return !support.formats.empty() && !support.presentModes.empty();
    }

    bool CreateLogicalDevice()
    {
        const float queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        VkDeviceQueueCreateInfo graphicsQueueInfo{};
        graphicsQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        graphicsQueueInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
        graphicsQueueInfo.queueCount = 1;
        graphicsQueueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(graphicsQueueInfo);

        if (m_presentQueueFamilyIndex != m_graphicsQueueFamilyIndex) {
            VkDeviceQueueCreateInfo presentQueueInfo{};
            presentQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            presentQueueInfo.queueFamilyIndex = m_presentQueueFamilyIndex;
            presentQueueInfo.queueCount = 1;
            presentQueueInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(presentQueueInfo);
        }

        const char* extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(std::size(extensions));
        createInfo.ppEnabledExtensionNames = extensions;
        createInfo.pEnabledFeatures = &deviceFeatures;

        const VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DbgLog("[Render] Vulkan vkCreateDevice failed (vk=%d).\n", static_cast<int>(result));
            return false;
        }

        if (!LoadVulkanDeviceFunctions(m_device)) {
            m_bootstrap.initHr = static_cast<int>(E_NOINTERFACE);
            DbgLog("[Render] Vulkan device function resolution failed.\n");
            return false;
        }

        vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, m_presentQueueFamilyIndex, 0, &m_presentQueue);
        return m_graphicsQueue != VK_NULL_HANDLE && m_presentQueue != VK_NULL_HANDLE;
    }

    bool CreateCommandPool()
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;

        const VkResult result = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DbgLog("[Render] Vulkan vkCreateCommandPool failed (vk=%d).\n", static_cast<int>(result));
            return false;
        }
        return true;
    }

    bool CreateSyncObjects()
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkResult result = vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphore);
        if (result == VK_SUCCESS) {
            result = vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphore);
        }
        if (result == VK_SUCCESS) {
            result = vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFence);
        }
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DbgLog("[Render] Vulkan sync object creation failed (vk=%d).\n", static_cast<int>(result));
            return false;
        }
        return true;
    }

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const
    {
        SwapChainSupportDetails details{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
        if (formatCount > 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
        if (presentModeCount > 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
    {
        for (const VkSurfaceFormatKHR& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }
        return formats.front();
    }

    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes) const
    {
        for (VkPresentModeKHR presentMode : presentModes) {
            if (presentMode == VK_PRESENT_MODE_FIFO_KHR) {
                return presentMode;
            }
        }
        return presentModes.empty() ? VK_PRESENT_MODE_FIFO_KHR : presentModes.front();
    }

    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }

        const uint32_t width = static_cast<uint32_t>((std::max)(1, m_renderWidth));
        const uint32_t height = static_cast<uint32_t>((std::max)(1, m_renderHeight));
        VkExtent2D extent{};
        extent.width = (std::max)(capabilities.minImageExtent.width, (std::min)(capabilities.maxImageExtent.width, width));
        extent.height = (std::max)(capabilities.minImageExtent.height, (std::min)(capabilities.maxImageExtent.height, height));
        return extent;
    }

    bool CreateSwapChainResources(VkSwapchainKHR oldSwapChain)
    {
        const SwapChainSupportDetails support = QuerySwapChainSupport(m_physicalDevice);
        if (support.formats.empty() || support.presentModes.empty()) {
            m_bootstrap.initHr = static_cast<int>(VK_ERROR_INITIALIZATION_FAILED);
            return false;
        }

        const VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(support.formats);
        const VkPresentModeKHR presentMode = ChooseSwapPresentMode(support.presentModes);
        const VkExtent2D extent = ChooseSwapExtent(support.capabilities);
        uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
            imageCount = support.capabilities.maxImageCount;
        }

        uint32_t queueFamilyIndices[] = { m_graphicsQueueFamilyIndex, m_presentQueueFamilyIndex };

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (m_graphicsQueueFamilyIndex != m_presentQueueFamilyIndex) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        createInfo.preTransform = support.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = oldSwapChain;

        VkSwapchainKHR newSwapChain = VK_NULL_HANDLE;
        VkResult result = vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &newSwapChain);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DbgLog("[Render] Vulkan vkCreateSwapchainKHR failed (vk=%d).\n", static_cast<int>(result));
            return false;
        }

        uint32_t actualImageCount = 0;
        result = vkGetSwapchainImagesKHR(m_device, newSwapChain, &actualImageCount, nullptr);
        if (result != VK_SUCCESS || actualImageCount == 0) {
            m_bootstrap.initHr = static_cast<int>(result != VK_SUCCESS ? result : VK_ERROR_INITIALIZATION_FAILED);
            vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
            return false;
        }

        std::vector<VkImage> newImages(actualImageCount);
        result = vkGetSwapchainImagesKHR(m_device, newSwapChain, &actualImageCount, newImages.data());
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
            return false;
        }

        VkRenderPass newRenderPass = VK_NULL_HANDLE;
        if (!CreateRenderPass(surfaceFormat.format, &newRenderPass)) {
            vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
            return false;
        }

        std::vector<VkImageView> newImageViews;
        if (!CreateImageViews(newImages, surfaceFormat.format, &newImageViews)) {
            vkDestroyRenderPass(m_device, newRenderPass, nullptr);
            vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
            return false;
        }

        std::vector<VkFramebuffer> newFramebuffers;
        if (!CreateFramebuffers(newRenderPass, newImageViews, extent, &newFramebuffers)) {
            DestroyImageViews(newImageViews);
            vkDestroyRenderPass(m_device, newRenderPass, nullptr);
            vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
            return false;
        }

        std::vector<VkCommandBuffer> newCommandBuffers;
        if (!AllocateCommandBuffers(static_cast<uint32_t>(newFramebuffers.size()), &newCommandBuffers)) {
            DestroyFramebuffers(newFramebuffers);
            DestroyImageViews(newImageViews);
            vkDestroyRenderPass(m_device, newRenderPass, nullptr);
            vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
            return false;
        }

        DestroySwapChainResources();
        m_swapChain = newSwapChain;
        m_swapChainImages = std::move(newImages);
        m_swapChainImageViews = std::move(newImageViews);
        m_swapChainFormat = surfaceFormat.format;
        m_swapChainExtent = extent;
        m_renderPass = newRenderPass;
        m_framebuffers = std::move(newFramebuffers);
        m_commandBuffers = std::move(newCommandBuffers);
        m_currentImageIndex = 0;
        return true;
    }

    bool CreateRenderPass(VkFormat format, VkRenderPass* outRenderPass)
    {
        if (!outRenderPass) {
            return false;
        }

        *outRenderPass = VK_NULL_HANDLE;
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        const VkResult result = vkCreateRenderPass(m_device, &renderPassInfo, nullptr, outRenderPass);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DbgLog("[Render] Vulkan vkCreateRenderPass failed (vk=%d).\n", static_cast<int>(result));
            return false;
        }
        return true;
    }

    bool CreateImageViews(const std::vector<VkImage>& images, VkFormat format, std::vector<VkImageView>* outViews)
    {
        if (!outViews) {
            return false;
        }

        outViews->clear();
        outViews->reserve(images.size());
        for (VkImage image : images) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = format;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            VkImageView imageView = VK_NULL_HANDLE;
            const VkResult result = vkCreateImageView(m_device, &viewInfo, nullptr, &imageView);
            if (result != VK_SUCCESS) {
                m_bootstrap.initHr = static_cast<int>(result);
                DbgLog("[Render] Vulkan vkCreateImageView failed (vk=%d).\n", static_cast<int>(result));
                DestroyImageViews(*outViews);
                return false;
            }
            outViews->push_back(imageView);
        }
        return true;
    }

    bool CreateFramebuffers(VkRenderPass renderPass, const std::vector<VkImageView>& imageViews,
        const VkExtent2D& extent, std::vector<VkFramebuffer>* outFramebuffers)
    {
        if (!outFramebuffers) {
            return false;
        }

        outFramebuffers->clear();
        outFramebuffers->reserve(imageViews.size());
        for (VkImageView imageView : imageViews) {
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &imageView;
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;

            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            const VkResult result = vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &framebuffer);
            if (result != VK_SUCCESS) {
                m_bootstrap.initHr = static_cast<int>(result);
                DbgLog("[Render] Vulkan vkCreateFramebuffer failed (vk=%d).\n", static_cast<int>(result));
                DestroyFramebuffers(*outFramebuffers);
                return false;
            }
            outFramebuffers->push_back(framebuffer);
        }
        return true;
    }

    bool AllocateCommandBuffers(uint32_t count, std::vector<VkCommandBuffer>* outCommandBuffers)
    {
        if (!outCommandBuffers) {
            return false;
        }

        outCommandBuffers->assign(count, VK_NULL_HANDLE);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = count;

        const VkResult result = vkAllocateCommandBuffers(m_device, &allocInfo, outCommandBuffers->data());
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DbgLog("[Render] Vulkan vkAllocateCommandBuffers failed (vk=%d).\n", static_cast<int>(result));
            outCommandBuffers->clear();
            return false;
        }
        return true;
    }

    void DestroyFramebuffers(std::vector<VkFramebuffer>& framebuffers)
    {
        for (VkFramebuffer framebuffer : framebuffers) {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }
        framebuffers.clear();
    }

    void DestroyImageViews(std::vector<VkImageView>& imageViews)
    {
        for (VkImageView imageView : imageViews) {
            vkDestroyImageView(m_device, imageView, nullptr);
        }
        imageViews.clear();
    }

    void DestroySwapChainResources()
    {
        if (m_device == VK_NULL_HANDLE) {
            return;
        }

        ReleasePendingTransferResources();

        if (!m_commandBuffers.empty()) {
            vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
            m_commandBuffers.clear();
        }
        DestroyFramebuffers(m_framebuffers);
        DestroyImageViews(m_swapChainImageViews);
        if (m_renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device, m_renderPass, nullptr);
            m_renderPass = VK_NULL_HANDLE;
        }
        if (m_swapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
            m_swapChain = VK_NULL_HANDLE;
        }
        m_swapChainImages.clear();
    }

    bool EnsureFrameStarted()
    {
        if (m_frameBegun) {
            return true;
        }
        if (m_device == VK_NULL_HANDLE || m_swapChain == VK_NULL_HANDLE || m_commandBuffers.empty()) {
            return false;
        }

        VkResult result = BeginFrame();
        if (result != VK_SUCCESS) {
            return false;
        }

        VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();

        VkClearValue clearValue{};
        clearValue.color = m_pendingClearColor;
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass;
        renderPassInfo.framebuffer = m_framebuffers[m_currentImageIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_swapChainExtent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearValue;
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_swapChainExtent.width);
        viewport.height = static_cast<float>(m_swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = m_swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        m_frameBegun = true;
        m_renderPassActive = true;
        return true;
    }

    bool EnsureTransferFrameStarted()
    {
        if (m_frameBegun) {
            return !m_renderPassActive;
        }

        const VkResult result = BeginFrame();
        if (result != VK_SUCCESS) {
            return false;
        }

        m_frameBegun = true;
        m_renderPassActive = false;
        return true;
    }

    VkResult BeginFrame()
    {
        ReleasePendingTransferResources();

        VkResult result = vkWaitForFences(m_device, 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return result;
        }
        result = vkResetFences(m_device, 1, &m_inFlightFence);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return result;
        }

        result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphore, VK_NULL_HANDLE, &m_currentImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            ResizeSwapChain();
            return result;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            m_bootstrap.initHr = static_cast<int>(result);
            return result;
        }

        VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
        vkResetCommandBuffer(commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return result;
        }
        return VK_SUCCESS;
    }

    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
    {
        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);
        for (uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
            if ((typeFilter & (1u << index)) != 0
                && (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties) {
                return index;
            }
        }
        return kInvalidQueueFamilyIndex;
    }

    bool CreateStagingBuffer(VkDeviceSize size, VkBuffer* outBuffer, VkDeviceMemory* outMemory)
    {
        if (!outBuffer || !outMemory || size == 0) {
            return false;
        }

        *outBuffer = VK_NULL_HANDLE;
        *outMemory = VK_NULL_HANDLE;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult result = vkCreateBuffer(m_device, &bufferInfo, nullptr, outBuffer);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(m_device, *outBuffer, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(
            memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (allocInfo.memoryTypeIndex == kInvalidQueueFamilyIndex) {
            vkDestroyBuffer(m_device, *outBuffer, nullptr);
            *outBuffer = VK_NULL_HANDLE;
            m_bootstrap.initHr = static_cast<int>(VK_ERROR_MEMORY_MAP_FAILED);
            return false;
        }

        result = vkAllocateMemory(m_device, &allocInfo, nullptr, outMemory);
        if (result != VK_SUCCESS) {
            vkDestroyBuffer(m_device, *outBuffer, nullptr);
            *outBuffer = VK_NULL_HANDLE;
            m_bootstrap.initHr = static_cast<int>(result);
            return false;
        }

        result = vkBindBufferMemory(m_device, *outBuffer, *outMemory, 0);
        if (result != VK_SUCCESS) {
            vkDestroyBuffer(m_device, *outBuffer, nullptr);
            vkFreeMemory(m_device, *outMemory, nullptr);
            *outBuffer = VK_NULL_HANDLE;
            *outMemory = VK_NULL_HANDLE;
            m_bootstrap.initHr = static_cast<int>(result);
            return false;
        }

        return true;
    }

    void ReleasePendingTransferResources()
    {
        for (VkBuffer buffer : m_pendingReleaseBuffers) {
            vkDestroyBuffer(m_device, buffer, nullptr);
        }
        m_pendingReleaseBuffers.clear();

        for (VkDeviceMemory memory : m_pendingReleaseMemory) {
            vkFreeMemory(m_device, memory, nullptr);
        }
        m_pendingReleaseMemory.clear();
    }

    void ResizeSwapChain()
    {
        if (m_device == VK_NULL_HANDLE || m_renderWidth <= 0 || m_renderHeight <= 0) {
            return;
        }

        vkDeviceWaitIdle(m_device);
        m_frameBegun = false;
        m_renderPassActive = false;
        CreateSwapChainResources(m_swapChain);
    }

    VkCommandBuffer GetCurrentCommandBuffer() const
    {
        return m_currentImageIndex < m_commandBuffers.size() ? m_commandBuffers[m_currentImageIndex] : VK_NULL_HANDLE;
    }

    HWND m_hwnd;
    int m_renderWidth;
    int m_renderHeight;
    RenderBackendBootstrapResult m_bootstrap;
    VkInstance m_instance;
    VkSurfaceKHR m_surface;
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    VkSwapchainKHR m_swapChain;
    VkFormat m_swapChainFormat;
    VkExtent2D m_swapChainExtent;
    VkRenderPass m_renderPass;
    VkCommandPool m_commandPool;
    VkSemaphore m_imageAvailableSemaphore;
    VkSemaphore m_renderFinishedSemaphore;
    VkFence m_inFlightFence;
    uint32_t m_graphicsQueueFamilyIndex;
    uint32_t m_presentQueueFamilyIndex;
    uint32_t m_currentImageIndex;
    bool m_frameBegun;
    bool m_renderPassActive;
    bool m_pendingDepthClear;
    VkClearColorValue m_pendingClearColor;
    std::vector<VkImage> m_swapChainImages;
    std::vector<VkImageView> m_swapChainImageViews;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkBuffer> m_pendingReleaseBuffers;
    std::vector<VkDeviceMemory> m_pendingReleaseMemory;

#else
    VulkanRenderDevice()
        : m_hwnd(nullptr), m_renderWidth(0), m_renderHeight(0)
    {
        m_bootstrap.backend = RenderBackendType::Vulkan;
        m_bootstrap.initHr = static_cast<int>(E_NOTIMPL);
    }

    RenderBackendType GetBackendType() const override
    {
        return RenderBackendType::Vulkan;
    }

    bool Initialize(HWND hwnd, RenderBackendBootstrapResult* outResult) override
    {
        Shutdown();
        m_hwnd = hwnd;
        RefreshRenderSize();

        m_bootstrap.backend = RenderBackendType::Vulkan;
        m_bootstrap.initHr = static_cast<int>(E_NOTIMPL);
        DbgLog("[Render] Vulkan SDK unavailable at build time; Vulkan backend is disabled.\n");

        if (outResult) {
            *outResult = m_bootstrap;
        }
        return false;
    }

    void Shutdown() override
    {
        m_hwnd = nullptr;
        m_renderWidth = 0;
        m_renderHeight = 0;
        m_bootstrap.backend = RenderBackendType::Vulkan;
        m_bootstrap.initHr = static_cast<int>(E_NOTIMPL);
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

    int GetRenderWidth() const override { return m_renderWidth; }
    int GetRenderHeight() const override { return m_renderHeight; }
    HWND GetWindowHandle() const override { return m_hwnd; }
    IDirect3DDevice7* GetLegacyDevice() const override { return nullptr; }
    int ClearColor(unsigned int color) override { (void)color; return -1; }
    int ClearDepth() override { return -1; }
    int Present(bool vertSync) override { (void)vertSync; return -1; }
    bool AcquireBackBufferDC(HDC* outDc) override { if (outDc) { *outDc = nullptr; } return false; }
    void ReleaseBackBufferDC(HDC dc) override { (void)dc; }
    bool UpdateBackBufferFromMemory(const void* bgraPixels, int width, int height, int pitch) override { (void)bgraPixels; (void)width; (void)height; (void)pitch; return false; }
    bool BeginScene() override { return false; }
    void EndScene() override {}
    void SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) override { (void)state; (void)matrix; }
    void SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override { (void)state; (void)value; }
    void SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override { (void)stage; (void)type; (void)value; }
    void BindTexture(DWORD stage, CTexture* texture) override { (void)stage; (void)texture; }
    void DrawPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat, const void* vertices, DWORD vertexCount, DWORD flags) override { (void)primitiveType; (void)vertexFormat; (void)vertices; (void)vertexCount; (void)flags; }
    void DrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat, const void* vertices, DWORD vertexCount, const unsigned short* indices, DWORD indexCount, DWORD flags) override { (void)primitiveType; (void)vertexFormat; (void)vertices; (void)vertexCount; (void)indices; (void)indexCount; (void)flags; }
    void AdjustTextureSize(unsigned int* width, unsigned int* height) override { if (width && height) { *width = (std::max)(1u, *width); *height = (std::max)(1u, *height); } }
    void ReleaseTextureResource(CTexture* texture) override { (void)texture; }
    bool CreateTextureResource(CTexture* texture, unsigned int requestedWidth, unsigned int requestedHeight, int pixelFormat, unsigned int* outSurfaceWidth, unsigned int* outSurfaceHeight) override { (void)texture; (void)requestedWidth; (void)requestedHeight; (void)pixelFormat; if (outSurfaceWidth) { *outSurfaceWidth = 0; } if (outSurfaceHeight) { *outSurfaceHeight = 0; } return false; }
    bool UpdateTextureResource(CTexture* texture, int x, int y, int w, int h, const unsigned int* data, bool skipColorKey, int pitch) override { (void)texture; (void)x; (void)y; (void)w; (void)h; (void)data; (void)skipColorKey; (void)pitch; return false; }

private:
    HWND m_hwnd;
    int m_renderWidth;
    int m_renderHeight;
    RenderBackendBootstrapResult m_bootstrap;
#endif
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
            if (m_d3d12.Initialize(hwnd, &result)) {
                m_active = &m_d3d12;
            } else {
                DbgLog("[Render] Failed to initialize backend '%s' (hr=0x%08X). Falling back to Direct3D11.\n",
                    GetRenderBackendName(requestedBackend),
                    static_cast<unsigned int>(result.initHr));
                m_active = &m_d3d11;
                if (!m_d3d11.Initialize(hwnd, &result)) {
                    DbgLog("[Render] Failed to initialize backend '%s' (hr=0x%08X). Falling back to Direct3D7.\n",
                        GetRenderBackendName(RenderBackendType::Direct3D11),
                        static_cast<unsigned int>(result.initHr));
                    m_active = &m_legacy;
                    if (!m_legacy.Initialize(hwnd, &result)) {
                        if (outResult) {
                            *outResult = result;
                        }
                        return false;
                    }
                }
            }
            break;

        case RenderBackendType::Vulkan:
            if (m_vulkan.Initialize(hwnd, &result)) {
                m_active = &m_vulkan;
            } else {
                DbgLog("[Render] Failed to initialize backend '%s' (hr=0x%08X). Falling back to Direct3D11.\n",
                    GetRenderBackendName(requestedBackend),
                    static_cast<unsigned int>(result.initHr));
                m_active = &m_d3d11;
                if (!m_d3d11.Initialize(hwnd, &result)) {
                    DbgLog("[Render] Failed to initialize backend '%s' (hr=0x%08X). Falling back to Direct3D7.\n",
                        GetRenderBackendName(RenderBackendType::Direct3D11),
                        static_cast<unsigned int>(result.initHr));
                    m_active = &m_legacy;
                    if (!m_legacy.Initialize(hwnd, &result)) {
                        if (outResult) {
                            *outResult = result;
                        }
                        return false;
                    }
                }
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
        m_vulkan.Shutdown();
        m_d3d12.Shutdown();
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
    bool UpdateBackBufferFromMemory(const void* bgraPixels, int width, int height, int pitch) override { return m_active->UpdateBackBufferFromMemory(bgraPixels, width, height, pitch); }
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
    D3D12RenderDevice m_d3d12;
    VulkanRenderDevice m_vulkan;
    IRenderDevice* m_active;
};

} // namespace

IRenderDevice& GetRenderDevice()
{
    static RoutedRenderDevice s_renderDevice;
    return s_renderDevice;
}