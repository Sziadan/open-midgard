#include "RenderDevice.h"

#include "Device.h"
#include "D3dutil.h"
#include "DebugLog.h"
#include "GraphicsSettings.h"
#include "ModernRenderState.h"
#include "render/Renderer.h"
#include "res/Texture.h"
#include "VulkanSmaaShaders.generated.h"
#include "VulkanShaders.generated.h"

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
PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures = nullptr;
PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
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
PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = nullptr;
PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = nullptr;
PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
PFN_vkResetDescriptorPool vkResetDescriptorPool = nullptr;
PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = nullptr;
PFN_vkCreateSampler vkCreateSampler = nullptr;
PFN_vkDestroySampler vkDestroySampler = nullptr;
PFN_vkCreateImage vkCreateImage = nullptr;
PFN_vkDestroyImage vkDestroyImage = nullptr;
PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
PFN_vkBindImageMemory vkBindImageMemory = nullptr;
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
PFN_vkCmdBindPipeline vkCmdBindPipeline = nullptr;
PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = nullptr;
PFN_vkCmdPushConstants vkCmdPushConstants = nullptr;
PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers = nullptr;
PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer = nullptr;
PFN_vkCmdDraw vkCmdDraw = nullptr;
PFN_vkCmdDrawIndexed vkCmdDrawIndexed = nullptr;
PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
PFN_vkCmdCopyImage vkCmdCopyImage = nullptr;
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
    vkGetPhysicalDeviceFeatures = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures"));
    vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));
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
        && vkGetPhysicalDeviceFeatures
        && vkGetPhysicalDeviceProperties
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
    vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(vkGetDeviceProcAddr(device, "vkCreateShaderModule"));
    vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(vkGetDeviceProcAddr(device, "vkDestroyShaderModule"));
    vkCreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(vkGetDeviceProcAddr(device, "vkCreatePipelineLayout"));
    vkDestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(vkGetDeviceProcAddr(device, "vkDestroyPipelineLayout"));
    vkCreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(vkGetDeviceProcAddr(device, "vkCreateGraphicsPipelines"));
    vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(vkGetDeviceProcAddr(device, "vkDestroyPipeline"));
    vkCreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(vkGetDeviceProcAddr(device, "vkCreateDescriptorSetLayout"));
    vkDestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(vkGetDeviceProcAddr(device, "vkDestroyDescriptorSetLayout"));
    vkCreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(vkGetDeviceProcAddr(device, "vkCreateDescriptorPool"));
    vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(vkGetDeviceProcAddr(device, "vkDestroyDescriptorPool"));
    vkResetDescriptorPool = reinterpret_cast<PFN_vkResetDescriptorPool>(vkGetDeviceProcAddr(device, "vkResetDescriptorPool"));
    vkAllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(vkGetDeviceProcAddr(device, "vkAllocateDescriptorSets"));
    vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(vkGetDeviceProcAddr(device, "vkUpdateDescriptorSets"));
    vkCreateSampler = reinterpret_cast<PFN_vkCreateSampler>(vkGetDeviceProcAddr(device, "vkCreateSampler"));
    vkDestroySampler = reinterpret_cast<PFN_vkDestroySampler>(vkGetDeviceProcAddr(device, "vkDestroySampler"));
    vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(vkGetDeviceProcAddr(device, "vkCreateImage"));
    vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(vkGetDeviceProcAddr(device, "vkDestroyImage"));
    vkGetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(vkGetDeviceProcAddr(device, "vkGetImageMemoryRequirements"));
    vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(vkGetDeviceProcAddr(device, "vkBindImageMemory"));
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
    vkCmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(vkGetDeviceProcAddr(device, "vkCmdBindPipeline"));
    vkCmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(vkGetDeviceProcAddr(device, "vkCmdBindDescriptorSets"));
    vkCmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(vkGetDeviceProcAddr(device, "vkCmdPushConstants"));
    vkCmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(vkGetDeviceProcAddr(device, "vkCmdBindVertexBuffers"));
    vkCmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(vkGetDeviceProcAddr(device, "vkCmdBindIndexBuffer"));
    vkCmdDraw = reinterpret_cast<PFN_vkCmdDraw>(vkGetDeviceProcAddr(device, "vkCmdDraw"));
    vkCmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(vkGetDeviceProcAddr(device, "vkCmdDrawIndexed"));
    vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier"));
    vkCmdCopyImage = reinterpret_cast<PFN_vkCmdCopyImage>(vkGetDeviceProcAddr(device, "vkCmdCopyImage"));
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
        && vkCreateShaderModule
        && vkDestroyShaderModule
        && vkCreatePipelineLayout
        && vkDestroyPipelineLayout
        && vkCreateGraphicsPipelines
        && vkDestroyPipeline
        && vkCreateDescriptorSetLayout
        && vkDestroyDescriptorSetLayout
        && vkCreateDescriptorPool
        && vkDestroyDescriptorPool
        && vkResetDescriptorPool
        && vkAllocateDescriptorSets
        && vkUpdateDescriptorSets
        && vkCreateSampler
        && vkDestroySampler
        && vkCreateImage
        && vkDestroyImage
        && vkGetImageMemoryRequirements
        && vkBindImageMemory
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
        && vkCmdBindPipeline
        && vkCmdBindDescriptorSets
        && vkCmdPushConstants
        && vkCmdBindVertexBuffers
        && vkCmdBindIndexBuffer
        && vkCmdDraw
        && vkCmdDrawIndexed
        && vkCmdPipelineBarrier
        && vkCmdCopyImage
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

#if RO_HAS_VULKAN
static constexpr UINT kModernTextureSlotCount = 2;

VkPrimitiveTopology ConvertPrimitiveTopologyVk(D3DPRIMITIVETYPE primitiveType);
VkBlendFactor ConvertBlendFactorVk(D3DBLEND blend);
VkCullModeFlags ConvertCullModeVk(D3DCULL cullMode);
#endif

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

#if RO_HAS_VULKAN
class VulkanTextureHandle final : public IUnknown {
public:
    VulkanTextureHandle(VkDevice device, uint32_t width, uint32_t height)
        : m_refCount(1), m_device(device), m_width(width), m_height(height),
          m_image(VK_NULL_HANDLE), m_memory(VK_NULL_HANDLE), m_view(VK_NULL_HANDLE),
          m_layout(VK_IMAGE_LAYOUT_UNDEFINED)
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** outObject) override
    {
        if (!outObject) {
            return E_POINTER;
        }
        *outObject = nullptr;
        if (riid == IID_IUnknown) {
            *outObject = static_cast<IUnknown*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const LONG refCount = InterlockedDecrement(&m_refCount);
        if (refCount == 0) {
            if (m_device != VK_NULL_HANDLE) {
                if (m_view != VK_NULL_HANDLE) {
                    vkDestroyImageView(m_device, m_view, nullptr);
                    m_view = VK_NULL_HANDLE;
                }
                if (m_image != VK_NULL_HANDLE) {
                    vkDestroyImage(m_device, m_image, nullptr);
                    m_image = VK_NULL_HANDLE;
                }
                if (m_memory != VK_NULL_HANDLE) {
                    vkFreeMemory(m_device, m_memory, nullptr);
                    m_memory = VK_NULL_HANDLE;
                }
            }
            delete this;
            return 0;
        }
        return static_cast<ULONG>(refCount);
    }

    VkDevice m_device;
    uint32_t m_width;
    uint32_t m_height;
    VkImage m_image;
    VkDeviceMemory m_memory;
    VkImageView m_view;
    VkImageLayout m_layout;

private:
    ~VulkanTextureHandle() = default;

    LONG m_refCount;
};

VulkanTextureHandle* GetVulkanTextureHandle(CTexture* texture)
{
    return texture ? static_cast<VulkanTextureHandle*>(texture->m_backendTextureObject) : nullptr;
}

const VulkanTextureHandle* GetVulkanTextureHandle(const CTexture* texture)
{
    return texture ? static_cast<const VulkanTextureHandle*>(texture->m_backendTextureObject) : nullptr;
}
#endif

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

constexpr AntiAliasingMode kCompiledPostProcessAntiAliasingMode = AntiAliasingMode::FXAA;

bool CompileShaderBlob(const char* source, const char* entryPoint, const char* target, ID3DBlob** outBlob);

const char* GetPostProcessPixelShaderEntryPoint(AntiAliasingMode mode)
{
    switch (mode) {
    case AntiAliasingMode::FXAA:
        return "PSMainFXAA";
    case AntiAliasingMode::SMAA:
        return "PSMainSMAAEdge";

    default:
        return nullptr;
    }
}

bool CompilePostProcessShaderBlob(const char* source, AntiAliasingMode mode, const char* target, ID3DBlob** outBlob)
{
    const char* entryPoint = GetPostProcessPixelShaderEntryPoint(mode);
    return entryPoint && CompileShaderBlob(source, entryPoint, target, outBlob);
}

struct VulkanPostShaderProgram {
    const uint8_t* vertexBytes;
    size_t vertexByteCount;
    const uint8_t* fragmentBytes;
    size_t fragmentByteCount;
    const char* vertexEntryPoint;
    const char* fragmentEntryPoint;
};

bool GetVulkanPostShaderProgram(AntiAliasingMode mode, VulkanPostShaderProgram* outProgram)
{
    if (!outProgram) {
        return false;
    }

    switch (mode) {
    case AntiAliasingMode::FXAA:
        outProgram->vertexBytes = kVulkanPostFxaaVsSpirv;
        outProgram->vertexByteCount = kVulkanPostFxaaVsSpirvSize;
        outProgram->fragmentBytes = kVulkanPostFxaaPsSpirv;
        outProgram->fragmentByteCount = kVulkanPostFxaaPsSpirvSize;
        outProgram->vertexEntryPoint = "VSMainPost";
        outProgram->fragmentEntryPoint = "PSMainFXAA";
        return true;
    case AntiAliasingMode::SMAA:
        outProgram->vertexBytes = kVulkanPostSmaaVsSpirv;
        outProgram->vertexByteCount = kVulkanPostSmaaVsSpirvSize;
        outProgram->fragmentBytes = kVulkanPostSmaaEdgePsSpirv;
        outProgram->fragmentByteCount = kVulkanPostSmaaEdgePsSpirvSize;
        outProgram->vertexEntryPoint = "VSMainPost";
        outProgram->fragmentEntryPoint = "PSMainSMAAEdge";
        return true;

    default:
        outProgram->vertexBytes = nullptr;
        outProgram->vertexByteCount = 0;
        outProgram->fragmentBytes = nullptr;
        outProgram->fragmentByteCount = 0;
        outProgram->vertexEntryPoint = nullptr;
        outProgram->fragmentEntryPoint = nullptr;
        return false;
    }
}

const char* GetModernRenderShaderSource()
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

struct VSOutputPost {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutputPost VSMainPost(uint vertexId : SV_VertexID)
{
    VSOutputPost output;
    float2 clipPos;
    clipPos.x = (vertexId == 2u) ? 3.0f : -1.0f;
    clipPos.y = (vertexId == 1u) ? 3.0f : -1.0f;
    output.pos = float4(clipPos, 0.0f, 1.0f);
    output.uv = float2((clipPos.x + 1.0f) * 0.5f, 1.0f - ((clipPos.y + 1.0f) * 0.5f));
    return output;
}

float ComputeFxaaLuma(float3 rgb)
{
    return dot(rgb, float3(0.299f, 0.587f, 0.114f));
}

float ComputeSmaaColorDelta(float3 a, float3 b)
{
    const float3 delta = abs(a - b);
    return max(delta.r, max(delta.g, delta.b));
}

float4 PSMainFXAA(VSOutputPost input) : SV_Target
{
    float2 invResolution = float2(
        1.0f / max(g_screenWidth, 1.0f),
        1.0f / max(g_screenHeight, 1.0f));

    float4 centerSample = g_texture0.Sample(g_sampler0, input.uv);
    float3 rgbM = centerSample.rgb;
    float lumaM = ComputeFxaaLuma(rgbM);

    float3 rgbNW = g_texture0.Sample(g_sampler0, input.uv + float2(-1.0f, -1.0f) * invResolution).rgb;
    float3 rgbNE = g_texture0.Sample(g_sampler0, input.uv + float2(1.0f, -1.0f) * invResolution).rgb;
    float3 rgbSW = g_texture0.Sample(g_sampler0, input.uv + float2(-1.0f, 1.0f) * invResolution).rgb;
    float3 rgbSE = g_texture0.Sample(g_sampler0, input.uv + float2(1.0f, 1.0f) * invResolution).rgb;

    float lumaNW = ComputeFxaaLuma(rgbNW);
    float lumaNE = ComputeFxaaLuma(rgbNE);
    float lumaSW = ComputeFxaaLuma(rgbSW);
    float lumaSE = ComputeFxaaLuma(rgbSE);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;
    float threshold = max(0.03125f, lumaMax * 0.125f);
    if (lumaRange < threshold) {
        return centerSample;
    }

    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = (lumaNW + lumaSW) - (lumaNE + lumaSE);

    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25f / 8.0f),
        1.0f / 128.0f);
    float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, float2(-8.0f, -8.0f), float2(8.0f, 8.0f)) * invResolution;

    float3 rgbA = 0.5f * (
        g_texture0.Sample(g_sampler0, input.uv + dir * (1.0f / 3.0f - 0.5f)).rgb +
        g_texture0.Sample(g_sampler0, input.uv + dir * (2.0f / 3.0f - 0.5f)).rgb);
    float3 rgbB = rgbA * 0.5f + 0.25f * (
        g_texture0.Sample(g_sampler0, input.uv + dir * -0.5f).rgb +
        g_texture0.Sample(g_sampler0, input.uv + dir * 0.5f).rgb);
    float lumaB = ComputeFxaaLuma(rgbB);

    return float4((lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB, centerSample.a);
}

float4 PSMainSMAAEdge(VSOutputPost input) : SV_Target
{
    const float2 invResolution = float2(
        1.0f / max(g_screenWidth, 1.0f),
        1.0f / max(g_screenHeight, 1.0f));

    const float4 centerSample = g_texture0.Sample(g_sampler0, input.uv);
    const float3 leftSample = g_texture0.Sample(g_sampler0, input.uv + float2(-1.0f, 0.0f) * invResolution).rgb;
    const float3 topSample = g_texture0.Sample(g_sampler0, input.uv + float2(0.0f, -1.0f) * invResolution).rgb;
    const float3 rightSample = g_texture0.Sample(g_sampler0, input.uv + float2(1.0f, 0.0f) * invResolution).rgb;
    const float3 bottomSample = g_texture0.Sample(g_sampler0, input.uv + float2(0.0f, 1.0f) * invResolution).rgb;

    const float lumaCenter = ComputeFxaaLuma(centerSample.rgb);
    const float lumaLeft = ComputeFxaaLuma(leftSample);
    const float lumaTop = ComputeFxaaLuma(topSample);
    const float lumaRight = ComputeFxaaLuma(rightSample);
    const float lumaBottom = ComputeFxaaLuma(bottomSample);

    const float lumaHorizontal = max(abs(lumaCenter - lumaTop), abs(lumaCenter - lumaBottom));
    const float lumaVertical = max(abs(lumaCenter - lumaLeft), abs(lumaCenter - lumaRight));
    const float colorHorizontal = max(ComputeSmaaColorDelta(centerSample.rgb, topSample), ComputeSmaaColorDelta(centerSample.rgb, bottomSample));
    const float colorVertical = max(ComputeSmaaColorDelta(centerSample.rgb, leftSample), ComputeSmaaColorDelta(centerSample.rgb, rightSample));

    const float threshold = 0.050f;
    const float horizontalEdge = step(threshold, max(lumaHorizontal, colorHorizontal));
    const float verticalEdge = step(threshold, max(lumaVertical, colorVertical));

    return float4(horizontalEdge, verticalEdge, 0.0f, centerSample.a);
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
        const WindowMode windowMode = GetEffectiveWindowModeForBackend(
            RenderBackendType::LegacyDirect3D7,
            GetCachedGraphicsSettings().windowMode);
        const unsigned int initFlags = windowMode == WindowMode::Fullscreen ? 1u : 0u;
        GUID deviceCandidates[] = {
            IID_IDirect3DTnLHalDevice,
            IID_IDirect3DHALDevice,
            IID_IDirect3DRGBDevice
        };

        m_bootstrap.backend = RenderBackendType::LegacyDirect3D7;
        m_bootstrap.initHr = -1;
        for (GUID& deviceGuid : deviceCandidates) {
            m_bootstrap.initHr = g_3dDevice.Init(hwnd, nullptr, &deviceGuid, nullptr, initFlags);
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

    bool PrepareOverlayPass() override { return true; }

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
        const unsigned int scale = static_cast<unsigned int>((std::max)(1, GetCachedGraphicsSettings().textureUpscaleFactor));
        *width = (std::max)(1u, *width * scale);
        *height = (std::max)(1u, *height * scale);
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
                    m_renderTargetView(nullptr), m_swapChainRenderTargetView(nullptr), m_sceneTexture(nullptr), m_sceneRenderTargetView(nullptr), m_sceneShaderResourceView(nullptr), m_depthStencilTexture(nullptr), m_depthStencilView(nullptr),
          m_captureTexture(nullptr),
                    m_vertexShaderTl(nullptr), m_vertexShaderLm(nullptr), m_pixelShader(nullptr), m_postVertexShader(nullptr), m_fxaaPixelShader(nullptr),
          m_inputLayoutTl(nullptr), m_inputLayoutLm(nullptr), m_constantBuffer(nullptr),
          m_vertexBuffer(nullptr), m_vertexBufferSize(0), m_indexBuffer(nullptr), m_indexBufferSize(0),
                                        m_samplerState(nullptr), m_postSamplerState(nullptr), m_postBlendState(nullptr), m_postDepthStencilState(nullptr), m_postRasterizerState(nullptr),
                    m_captureDc(nullptr), m_captureBitmap(nullptr), m_captureBits(nullptr), m_captureWidth(0), m_captureHeight(0), m_scenePresentDirty(false)
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
        ReleaseSceneRenderTargetResources();
        SafeRelease(m_postRasterizerState);
        SafeRelease(m_postDepthStencilState);
        SafeRelease(m_postBlendState);
        SafeRelease(m_postSamplerState);
        SafeRelease(m_samplerState);
        SafeRelease(m_indexBuffer);
        m_indexBufferSize = 0;
        SafeRelease(m_vertexBuffer);
        m_vertexBufferSize = 0;
        SafeRelease(m_constantBuffer);
        SafeRelease(m_inputLayoutLm);
        SafeRelease(m_inputLayoutTl);
        SafeRelease(m_fxaaPixelShader);
        SafeRelease(m_postVertexShader);
        SafeRelease(m_pixelShader);
        SafeRelease(m_vertexShaderLm);
        SafeRelease(m_vertexShaderTl);
        SafeRelease(m_depthStencilView);
        SafeRelease(m_depthStencilTexture);
        SafeRelease(m_swapChainRenderTargetView);
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
        m_scenePresentDirty = false;
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
        m_scenePresentDirty = IsFxaaEnabled();
        return 0;
    }

    int ClearDepth() override
    {
        if (m_context && m_depthStencilView) {
            m_context->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
            if (IsFxaaEnabled()) {
                m_scenePresentDirty = true;
            }
        }
        return 0;
    }

    int Present(bool vertSync) override
    {
        if (!m_swapChain) {
            return -1;
        }
        EnsureScenePresentedToBackBuffer();
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

        m_scenePresentDirty = false;

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

    bool PrepareOverlayPass() override { return true; }

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
            const unsigned int scale = static_cast<unsigned int>((std::max)(1, GetCachedGraphicsSettings().textureUpscaleFactor));
            *width = (std::max)(1u, *width * scale);
            *height = (std::max)(1u, *height * scale);
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
        unsigned int surfaceWidth = requestedWidth;
        unsigned int surfaceHeight = requestedHeight;
        AdjustTextureSize(&surfaceWidth, &surfaceHeight);

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = (std::max)(1u, surfaceWidth);
        desc.Height = (std::max)(1u, surfaceHeight);
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

        const char* shaderSource = GetModernRenderShaderSource();
        ID3DBlob* vertexShaderTlBlob = nullptr;
        ID3DBlob* vertexShaderLmBlob = nullptr;
        ID3DBlob* pixelShaderBlob = nullptr;
        ID3DBlob* postVertexShaderBlob = nullptr;
        ID3DBlob* fxaaPixelShaderBlob = nullptr;
        const bool compiled = CompileShaderBlob(shaderSource, "VSMainTL", "vs_4_0", &vertexShaderTlBlob)
            && CompileShaderBlob(shaderSource, "VSMainLM", "vs_4_0", &vertexShaderLmBlob)
            && CompileShaderBlob(shaderSource, "PSMain", "ps_4_0", &pixelShaderBlob)
            && CompileShaderBlob(shaderSource, "VSMainPost", "vs_4_0", &postVertexShaderBlob)
            && CompilePostProcessShaderBlob(shaderSource, kCompiledPostProcessAntiAliasingMode, "ps_4_0", &fxaaPixelShaderBlob);
        if (!compiled) {
            SafeRelease(vertexShaderTlBlob);
            SafeRelease(vertexShaderLmBlob);
            SafeRelease(pixelShaderBlob);
            SafeRelease(postVertexShaderBlob);
            SafeRelease(fxaaPixelShaderBlob);
            return false;
        }

        HRESULT hr = m_device->CreateVertexShader(vertexShaderTlBlob->GetBufferPointer(), vertexShaderTlBlob->GetBufferSize(), nullptr, &m_vertexShaderTl);
        if (FAILED(hr) || !m_vertexShaderTl) {
            SafeRelease(vertexShaderTlBlob);
            SafeRelease(vertexShaderLmBlob);
            SafeRelease(pixelShaderBlob);
            SafeRelease(postVertexShaderBlob);
            SafeRelease(fxaaPixelShaderBlob);
            return false;
        }

        hr = m_device->CreateVertexShader(vertexShaderLmBlob->GetBufferPointer(), vertexShaderLmBlob->GetBufferSize(), nullptr, &m_vertexShaderLm);
        if (FAILED(hr) || !m_vertexShaderLm) {
            SafeRelease(vertexShaderTlBlob);
            SafeRelease(vertexShaderLmBlob);
            SafeRelease(pixelShaderBlob);
            SafeRelease(postVertexShaderBlob);
            SafeRelease(fxaaPixelShaderBlob);
            return false;
        }

        hr = m_device->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, &m_pixelShader);
        if (FAILED(hr) || !m_pixelShader) {
            SafeRelease(vertexShaderTlBlob);
            SafeRelease(vertexShaderLmBlob);
            SafeRelease(pixelShaderBlob);
            SafeRelease(postVertexShaderBlob);
            SafeRelease(fxaaPixelShaderBlob);
            return false;
        }

        hr = m_device->CreateVertexShader(postVertexShaderBlob->GetBufferPointer(), postVertexShaderBlob->GetBufferSize(), nullptr, &m_postVertexShader);
        if (FAILED(hr) || !m_postVertexShader) {
            SafeRelease(vertexShaderTlBlob);
            SafeRelease(vertexShaderLmBlob);
            SafeRelease(pixelShaderBlob);
            SafeRelease(postVertexShaderBlob);
            SafeRelease(fxaaPixelShaderBlob);
            return false;
        }

        hr = m_device->CreatePixelShader(fxaaPixelShaderBlob->GetBufferPointer(), fxaaPixelShaderBlob->GetBufferSize(), nullptr, &m_fxaaPixelShader);
        if (FAILED(hr) || !m_fxaaPixelShader) {
            SafeRelease(vertexShaderTlBlob);
            SafeRelease(vertexShaderLmBlob);
            SafeRelease(pixelShaderBlob);
            SafeRelease(postVertexShaderBlob);
            SafeRelease(fxaaPixelShaderBlob);
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
        SafeRelease(postVertexShaderBlob);
        SafeRelease(fxaaPixelShaderBlob);
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
        const int anisotropicLevel = GetCachedGraphicsSettings().anisotropicLevel;
        const bool useAnisotropic = anisotropicLevel > 1;
        samplerDesc.Filter = useAnisotropic ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.MaxAnisotropy = static_cast<UINT>(useAnisotropic ? anisotropicLevel : 1);
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = m_device->CreateSamplerState(&samplerDesc, &m_samplerState);
        if (FAILED(hr) || !m_samplerState) {
            return false;
        }

        D3D11_SAMPLER_DESC postSamplerDesc{};
        postSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        postSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        postSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        postSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        postSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = m_device->CreateSamplerState(&postSamplerDesc, &m_postSamplerState);
        if (FAILED(hr) || !m_postSamplerState) {
            return false;
        }

        D3D11_BLEND_DESC postBlendDesc{};
        postBlendDesc.RenderTarget[0].BlendEnable = FALSE;
        postBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = m_device->CreateBlendState(&postBlendDesc, &m_postBlendState);
        if (FAILED(hr) || !m_postBlendState) {
            return false;
        }

        D3D11_DEPTH_STENCIL_DESC postDepthDesc{};
        postDepthDesc.DepthEnable = FALSE;
        postDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        postDepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        hr = m_device->CreateDepthStencilState(&postDepthDesc, &m_postDepthStencilState);
        if (FAILED(hr) || !m_postDepthStencilState) {
            return false;
        }

        D3D11_RASTERIZER_DESC postRasterizerDesc{};
        postRasterizerDesc.FillMode = D3D11_FILL_SOLID;
        postRasterizerDesc.CullMode = D3D11_CULL_NONE;
        postRasterizerDesc.DepthClipEnable = TRUE;
        hr = m_device->CreateRasterizerState(&postRasterizerDesc, &m_postRasterizerState);
        return SUCCEEDED(hr) && m_postRasterizerState != nullptr;
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

        if (IsFxaaEnabled()) {
            m_scenePresentDirty = true;
        }
    }

    bool RefreshRenderTarget()
    {
        if (!m_swapChain || !m_device) {
            return false;
        }
        if ((m_renderWidth <= 0 || m_renderHeight <= 0) && m_hwnd) {
            RECT clientRect{};
            GetClientRect(m_hwnd, &clientRect);
            m_renderWidth = (std::max)(1L, clientRect.right - clientRect.left);
            m_renderHeight = (std::max)(1L, clientRect.bottom - clientRect.top);
        }
        SafeRelease(m_renderTargetView);
        ReleaseSceneRenderTargetResources();
        SafeRelease(m_swapChainRenderTargetView);
        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
        if (FAILED(hr) || !backBuffer) {
            SafeRelease(backBuffer);
            return false;
        }
        hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_swapChainRenderTargetView);
        SafeRelease(backBuffer);
        if (FAILED(hr) || !m_swapChainRenderTargetView) {
            return false;
        }
        if (IsFxaaEnabled()) {
            if (!CreateSceneRenderTargetResources()) {
                return false;
            }
            m_renderTargetView = m_sceneRenderTargetView;
        } else {
            m_renderTargetView = m_swapChainRenderTargetView;
        }
        if (m_renderTargetView) {
            m_renderTargetView->AddRef();
        }
        if (!CreateDepthStencilResources()) {
            return false;
        }
        m_context->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
        ApplyViewport();
        m_scenePresentDirty = false;
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
        ReleaseSceneRenderTargetResources();
        SafeRelease(m_swapChainRenderTargetView);
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

        if (!EnsureScenePresentedToBackBuffer()) {
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

    bool IsFxaaEnabled() const
    {
        return GetCachedGraphicsSettings().antiAliasing == AntiAliasingMode::FXAA;
    }

    void ReleaseSceneRenderTargetResources()
    {
        SafeRelease(m_sceneShaderResourceView);
        SafeRelease(m_sceneRenderTargetView);
        SafeRelease(m_sceneTexture);
    }

    bool CreateSceneRenderTargetResources()
    {
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

        D3D11_TEXTURE2D_DESC sceneDesc{};
        sceneDesc.Width = static_cast<UINT>(targetWidth);
        sceneDesc.Height = static_cast<UINT>(targetHeight);
        sceneDesc.MipLevels = 1;
        sceneDesc.ArraySize = 1;
        sceneDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sceneDesc.SampleDesc.Count = 1;
        sceneDesc.Usage = D3D11_USAGE_DEFAULT;
        sceneDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = m_device->CreateTexture2D(&sceneDesc, nullptr, &m_sceneTexture);
        if (FAILED(hr) || !m_sceneTexture) {
            ReleaseSceneRenderTargetResources();
            return false;
        }

        hr = m_device->CreateRenderTargetView(m_sceneTexture, nullptr, &m_sceneRenderTargetView);
        if (FAILED(hr) || !m_sceneRenderTargetView) {
            ReleaseSceneRenderTargetResources();
            return false;
        }

        hr = m_device->CreateShaderResourceView(m_sceneTexture, nullptr, &m_sceneShaderResourceView);
        if (FAILED(hr) || !m_sceneShaderResourceView) {
            ReleaseSceneRenderTargetResources();
            return false;
        }

        return true;
    }

    bool EnsureScenePresentedToBackBuffer()
    {
        if (!IsFxaaEnabled() || !m_scenePresentDirty) {
            return true;
        }
        if (!m_context || !m_swapChainRenderTargetView || !m_sceneShaderResourceView
            || !m_postVertexShader || !m_fxaaPixelShader || !m_constantBuffer
            || !m_postSamplerState || !m_postBlendState || !m_postDepthStencilState || !m_postRasterizerState) {
            return false;
        }

        ModernDrawConstants constants{};
        constants.screenWidth = static_cast<float>((std::max)(1, m_renderWidth));
        constants.screenHeight = static_cast<float>((std::max)(1, m_renderHeight));
        if (!UploadBuffer(m_constantBuffer, &constants, sizeof(constants))) {
            return false;
        }

        ID3D11ShaderResourceView* postViews[2] = { m_sceneShaderResourceView, nullptr };
        const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        m_context->OMSetRenderTargets(1, &m_swapChainRenderTargetView, nullptr);
        m_context->OMSetBlendState(m_postBlendState, blendFactor, 0xFFFFFFFFu);
        m_context->OMSetDepthStencilState(m_postDepthStencilState, 0);
        m_context->RSSetState(m_postRasterizerState);
        ApplyViewport();

        m_context->IASetInputLayout(nullptr);
        m_context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
        m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->VSSetShader(m_postVertexShader, nullptr, 0);
        m_context->PSSetShader(m_fxaaPixelShader, nullptr, 0);
        m_context->VSSetConstantBuffers(0, 1, &m_constantBuffer);
        m_context->PSSetConstantBuffers(0, 1, &m_constantBuffer);
        m_context->PSSetSamplers(0, 1, &m_postSamplerState);
        m_context->PSSetShaderResources(0, 2, postViews);
        m_context->Draw(3, 0);

        ID3D11ShaderResourceView* clearViews[2] = { nullptr, nullptr };
        m_context->PSSetShaderResources(0, 2, clearViews);
        m_context->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
        ApplyViewport();
        m_scenePresentDirty = false;
        return true;
    }

    HWND m_hwnd;
    int m_renderWidth;
    int m_renderHeight;
    IDXGISwapChain* m_swapChain;
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;
    ID3D11RenderTargetView* m_renderTargetView;
    ID3D11RenderTargetView* m_swapChainRenderTargetView;
    ID3D11Texture2D* m_sceneTexture;
    ID3D11RenderTargetView* m_sceneRenderTargetView;
    ID3D11ShaderResourceView* m_sceneShaderResourceView;
    ID3D11Texture2D* m_depthStencilTexture;
    ID3D11DepthStencilView* m_depthStencilView;
    ID3D11Texture2D* m_captureTexture;
    ID3D11VertexShader* m_vertexShaderTl;
    ID3D11VertexShader* m_vertexShaderLm;
    ID3D11PixelShader* m_pixelShader;
    ID3D11VertexShader* m_postVertexShader;
    ID3D11PixelShader* m_fxaaPixelShader;
    ID3D11InputLayout* m_inputLayoutTl;
    ID3D11InputLayout* m_inputLayoutLm;
    ID3D11Buffer* m_constantBuffer;
    ID3D11Buffer* m_vertexBuffer;
    size_t m_vertexBufferSize;
    ID3D11Buffer* m_indexBuffer;
    size_t m_indexBufferSize;
    ID3D11SamplerState* m_samplerState;
    ID3D11SamplerState* m_postSamplerState;
    ID3D11BlendState* m_postBlendState;
    ID3D11DepthStencilState* m_postDepthStencilState;
    ID3D11RasterizerState* m_postRasterizerState;
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
    bool m_scenePresentDirty;
};

class D3D12RenderDevice final : public IRenderDevice {
public:
    static constexpr UINT kFrameCount = 2;
    static constexpr UINT kSrvSlotCount = 2;
    static constexpr UINT kSrvHeapCapacity = 65536;

    D3D12RenderDevice()
        : m_hwnd(nullptr), m_renderWidth(0), m_renderHeight(0),
          m_factory(nullptr), m_device(nullptr), m_commandQueue(nullptr), m_swapChain(nullptr),
          m_rtvHeap(nullptr), m_dsvHeap(nullptr), m_srvHeap(nullptr), m_depthStencil(nullptr), m_postPipelineState(nullptr),
          m_commandAllocator(nullptr), m_commandList(nullptr), m_fence(nullptr),
          m_fenceEvent(nullptr), m_fenceValue(0), m_frameIndex(0), m_rtvDescriptorSize(0), m_srvDescriptorSize(0), m_srvHeapCursor(0),
          m_rootSignature(nullptr),
          m_vertexShaderTlBlob(nullptr), m_vertexShaderLmBlob(nullptr), m_pixelShaderBlob(nullptr), m_postVertexShaderBlob(nullptr), m_fxaaPixelShaderBlob(nullptr),
                        m_captureReadbackBuffer(nullptr), m_captureDc(nullptr), m_captureBitmap(nullptr), m_captureBits(nullptr),
            m_captureWidth(0), m_captureHeight(0), m_captureRowPitch(0),
                    m_frameCommandsOpen(false), m_loggedSrvHeapExhausted(false), m_scenePresentDirty(false)
    {
        ResetModernFixedFunctionState(&m_pipelineState);
        m_boundTextures[0] = nullptr;
        m_boundTextures[1] = nullptr;
        for (UINT index = 0; index < kFrameCount; ++index) {
            m_renderTargets[index] = nullptr;
            m_sceneRenderTargets[index] = nullptr;
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
        rtvHeapDesc.NumDescriptors = kFrameCount * 2;
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
        SafeRelease(m_postPipelineState);
        SafeRelease(m_fxaaPixelShaderBlob);
        SafeRelease(m_postVertexShaderBlob);
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
        m_scenePresentDirty = false;
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
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = IsFxaaEnabled() ? GetCurrentSceneRtvHandle() : GetCurrentRtvHandle();
        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        m_scenePresentDirty = IsFxaaEnabled();
        return 0;
    }
    int ClearDepth() override
    {
        if (!EnsureFrameCommandsStarted() || !m_depthStencil || !m_dsvHeap) {
            return -1;
        }

        m_commandList->ClearDepthStencilView(GetDsvHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        if (IsFxaaEnabled()) {
            m_scenePresentDirty = true;
        }
        return 0;
    }
    int Present(bool vertSync) override
    {
        if (!m_swapChain || !m_commandQueue) {
            return -1;
        }

        if (!EnsureScenePresentedToBackBuffer()) {
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
        m_scenePresentDirty = false;
        return true;
    }
    bool BeginScene() override { return EnsureFrameCommandsStarted(); }
    bool PrepareOverlayPass() override { return true; }
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
            const unsigned int scale = static_cast<unsigned int>((std::max)(1, GetCachedGraphicsSettings().textureUpscaleFactor));
            *width = (std::max)(1u, *width * scale);
            *height = (std::max)(1u, *height * scale);
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
        unsigned int surfaceWidth = requestedWidth;
        unsigned int surfaceHeight = requestedHeight;
        AdjustTextureSize(&surfaceWidth, &surfaceHeight);

        D3D12_RESOURCE_DESC textureDesc{};
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Width = static_cast<UINT64>((std::max)(1u, surfaceWidth));
        textureDesc.Height = static_cast<UINT>((std::max)(1u, surfaceHeight));
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
            SafeRelease(m_sceneRenderTargets[index]);
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

        if (IsFxaaEnabled()) {
            if (!CreateSceneRenderTargets()) {
                return false;
            }
        }

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
        m_scenePresentDirty = false;
        return true;
    }

    bool CreatePipelineResources()
    {
        if (!m_device || !m_srvHeap) {
            return false;
        }

        const char* shaderSource = GetModernRenderShaderSource();
        if (!CompileShaderBlob(shaderSource, "VSMainTL", "vs_5_0", &m_vertexShaderTlBlob)
            || !CompileShaderBlob(shaderSource, "VSMainLM", "vs_5_0", &m_vertexShaderLmBlob)
            || !CompileShaderBlob(shaderSource, "PSMain", "ps_5_0", &m_pixelShaderBlob)
            || !CompileShaderBlob(shaderSource, "VSMainPost", "vs_5_0", &m_postVertexShaderBlob)
            || !CompilePostProcessShaderBlob(shaderSource, kCompiledPostProcessAntiAliasingMode, "ps_5_0", &m_fxaaPixelShaderBlob)) {
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
        const int anisotropicLevel = GetCachedGraphicsSettings().anisotropicLevel;
        const bool useAnisotropic = anisotropicLevel > 1;
        samplerDesc.Filter = useAnisotropic ? D3D12_FILTER_ANISOTROPIC : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.ShaderRegister = 0;
        samplerDesc.RegisterSpace = 0;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        samplerDesc.MaxAnisotropy = static_cast<UINT>(useAnisotropic ? anisotropicLevel : 1);
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

        return CreatePostPipelineResources(kCompiledPostProcessAntiAliasingMode);
    }

    ID3DBlob* GetPostPixelShaderBlob(AntiAliasingMode mode) const
    {
        switch (mode) {
        case AntiAliasingMode::FXAA:
            return m_fxaaPixelShaderBlob;
        case AntiAliasingMode::SMAA:
            return nullptr;

        default:
            return nullptr;
        }
    }

    bool CreatePostPipelineResources(AntiAliasingMode mode)
    {
        ID3DBlob* postPixelShaderBlob = GetPostPixelShaderBlob(mode);
        if (!m_device || !m_rootSignature || !m_postVertexShaderBlob || !postPixelShaderBlob) {
            return false;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = m_rootSignature;
        desc.VS = { m_postVertexShaderBlob->GetBufferPointer(), m_postVertexShaderBlob->GetBufferSize() };
        desc.PS = { postPixelShaderBlob->GetBufferPointer(), postPixelShaderBlob->GetBufferSize() };
        desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        desc.SampleMask = UINT_MAX;
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.RasterizerState.FrontCounterClockwise = FALSE;
        desc.RasterizerState.DepthClipEnable = TRUE;
        desc.DepthStencilState.DepthEnable = FALSE;
        desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        desc.DepthStencilState.StencilEnable = FALSE;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;

        HRESULT hr = m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_postPipelineState));
        return SUCCEEDED(hr) && m_postPipelineState != nullptr;
    }

    bool IsFxaaEnabled() const
    {
        return GetCachedGraphicsSettings().antiAliasing == AntiAliasingMode::FXAA;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentSceneRtvHandle() const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(kFrameCount + m_frameIndex) * static_cast<SIZE_T>(m_rtvDescriptorSize);
        return handle;
    }

    bool CreateSceneRenderTargets()
    {
        if (!m_device || m_renderWidth <= 0 || m_renderHeight <= 0) {
            return false;
        }

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC sceneDesc{};
        sceneDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        sceneDesc.Width = static_cast<UINT64>(m_renderWidth);
        sceneDesc.Height = static_cast<UINT>(m_renderHeight);
        sceneDesc.DepthOrArraySize = 1;
        sceneDesc.MipLevels = 1;
        sceneDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sceneDesc.SampleDesc.Count = 1;
        sceneDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        sceneDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

        for (UINT index = 0; index < kFrameCount; ++index) {
            HRESULT hr = m_device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &sceneDesc,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                &clearValue,
                IID_PPV_ARGS(&m_sceneRenderTargets[index]));
            if (FAILED(hr) || !m_sceneRenderTargets[index]) {
                return false;
            }

            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += static_cast<SIZE_T>(kFrameCount + index) * static_cast<SIZE_T>(m_rtvDescriptorSize);
            m_device->CreateRenderTargetView(m_sceneRenderTargets[index], nullptr, handle);
        }

        return true;
    }

    bool WriteSceneSrvDescriptors(UINT baseIndex)
    {
        if (baseIndex + 1 >= kSrvHeapCapacity || !m_device || !m_sceneRenderTargets[m_frameIndex]) {
            return false;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        m_device->CreateShaderResourceView(m_sceneRenderTargets[m_frameIndex], &srvDesc, GetSrvCpuHandle(baseIndex));
        WriteNullSrvDescriptor(baseIndex + 1);
        return true;
    }

    bool EnsureScenePresentedToBackBuffer()
    {
        if (!IsFxaaEnabled() || !m_scenePresentDirty) {
            return true;
        }
        if (!EnsureFrameCommandsStarted() || !m_postPipelineState || !m_sceneRenderTargets[m_frameIndex]) {
            return false;
        }

        UINT srvBaseIndex = 0;
        if (!ReserveSrvTable(kSrvSlotCount, &srvBaseIndex) || !WriteSceneSrvDescriptors(srvBaseIndex)) {
            return false;
        }

        ModernDrawConstants constants{};
        constants.screenWidth = static_cast<float>((std::max)(1, m_renderWidth));
        constants.screenHeight = static_cast<float>((std::max)(1, m_renderHeight));
        void* constantUpload = nullptr;
        UINT64 constantGpuAddress = 0;
        const size_t constantBytes = AlignTo(static_cast<UINT>(sizeof(ModernDrawConstants)), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        if (!AllocateConstantBufferSlice(constantBytes, &constantUpload, &constantGpuAddress)) {
            return false;
        }
        std::memset(constantUpload, 0, constantBytes);
        std::memcpy(constantUpload, &constants, sizeof(constants));

        const D3D12_RESOURCE_BARRIER sceneToShader = TransitionBarrier(
            m_sceneRenderTargets[m_frameIndex],
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_commandList->ResourceBarrier(1, &sceneToShader);

        ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvHeap };
        m_commandList->SetDescriptorHeaps(1, descriptorHeaps);
        m_commandList->SetGraphicsRootSignature(m_rootSignature);
        m_commandList->SetPipelineState(m_postPipelineState);
        m_commandList->SetGraphicsRootConstantBufferView(0, constantGpuAddress);
        m_commandList->SetGraphicsRootDescriptorTable(1, GetSrvGpuHandle(srvBaseIndex));
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->IASetVertexBuffers(0, 0, nullptr);
        m_commandList->IASetIndexBuffer(nullptr);
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCurrentRtvHandle();
        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        ApplyViewportAndScissor();
        m_commandList->DrawInstanced(3, 1, 0, 0);

        const D3D12_RESOURCE_BARRIER sceneToRender = TransitionBarrier(
            m_sceneRenderTargets[m_frameIndex],
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &sceneToRender);

        const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetDsvHandle();
        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, m_depthStencil ? &dsvHandle : nullptr);
        ApplyViewportAndScissor();
        m_scenePresentDirty = false;
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
        if (!EnsureCaptureResources() || !GetCurrentBackBuffer()) {
            return false;
        }

        if (!EnsureScenePresentedToBackBuffer() || !EnsureFrameCommandsStarted()) {
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

        if (IsFxaaEnabled()) {
            m_scenePresentDirty = true;
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
            SafeRelease(m_sceneRenderTargets[index]);
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
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = IsFxaaEnabled() ? GetCurrentSceneRtvHandle() : GetCurrentRtvHandle();
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
    ID3D12Resource* m_sceneRenderTargets[kFrameCount];
    ID3D12Resource* m_depthStencil;
    ID3D12PipelineState* m_postPipelineState;
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
    ID3DBlob* m_postVertexShaderBlob;
    ID3DBlob* m_fxaaPixelShaderBlob;
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
    bool m_scenePresentDirty;
};

class VulkanRenderDevice final : public IRenderDevice {
public:
#if RO_HAS_VULKAN
    VulkanRenderDevice()
        : m_hwnd(nullptr), m_renderWidth(0), m_renderHeight(0),
          m_instance(VK_NULL_HANDLE), m_surface(VK_NULL_HANDLE), m_physicalDevice(VK_NULL_HANDLE),
          m_device(VK_NULL_HANDLE), m_graphicsQueue(VK_NULL_HANDLE), m_presentQueue(VK_NULL_HANDLE),
          m_swapChain(VK_NULL_HANDLE), m_swapChainFormat(VK_FORMAT_UNDEFINED),
                    m_renderPass(VK_NULL_HANDLE), m_overlayRenderPass(VK_NULL_HANDLE), m_depthFormat(VK_FORMAT_D16_UNORM),
                    m_sceneImage(VK_NULL_HANDLE), m_sceneMemory(VK_NULL_HANDLE), m_sceneImageView(VK_NULL_HANDLE), m_sceneFramebuffer(VK_NULL_HANDLE),
          m_depthImage(VK_NULL_HANDLE), m_depthMemory(VK_NULL_HANDLE), m_depthImageView(VK_NULL_HANDLE),
                    m_descriptorSetLayout(VK_NULL_HANDLE), m_postDescriptorSetLayout(VK_NULL_HANDLE),
                                        m_descriptorPool(VK_NULL_HANDLE), m_sampler(VK_NULL_HANDLE), m_postSampler(VK_NULL_HANDLE),
                                        m_pipelineLayout(VK_NULL_HANDLE), m_postPipelineLayout(VK_NULL_HANDLE), m_postPipeline(VK_NULL_HANDLE), m_vertexShaderTlModule(VK_NULL_HANDLE),
                                        m_vertexShaderLmModule(VK_NULL_HANDLE), m_fragmentShaderModule(VK_NULL_HANDLE), m_postVertexShaderModule(VK_NULL_HANDLE), m_postFxaaFragmentShaderModule(VK_NULL_HANDLE),
                    m_commandPool(VK_NULL_HANDLE), m_immediateCommandPool(VK_NULL_HANDLE),
          m_imageAvailableSemaphore(VK_NULL_HANDLE), m_renderFinishedSemaphore(VK_NULL_HANDLE),
          m_inFlightFence(VK_NULL_HANDLE), m_immediateFence(VK_NULL_HANDLE), m_graphicsQueueFamilyIndex(kInvalidQueueFamilyIndex),
          m_presentQueueFamilyIndex(kInvalidQueueFamilyIndex), m_currentImageIndex(0),
                    m_frameBegun(false), m_renderPassActive(false), m_pendingDepthClear(false), m_verticalSyncEnabled(false), m_overlayPassPrepared(false),
                    m_defaultTexture(nullptr), m_samplerAnisotropySupported(false), m_maxSamplerAnisotropy(1.0f)
    {
        m_bootstrap.backend = RenderBackendType::Vulkan;
        m_bootstrap.initHr = static_cast<int>(E_NOTIMPL);
        std::memset(&m_pendingClearColor, 0, sizeof(m_pendingClearColor));
                ResetModernFixedFunctionState(&m_pipelineState);
                m_boundTextures[0] = nullptr;
                m_boundTextures[1] = nullptr;
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
        if (!CreatePipelineResources()) {
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

        ReleaseUploadPages(m_vertexUploadPages);
        ReleaseUploadPages(m_indexUploadPages);
        ReleaseAllTrackedTextures();
        DestroySwapChainResources();
        DestroyPipelineResources();

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
        if (m_device != VK_NULL_HANDLE && m_immediateFence != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, m_immediateFence, nullptr);
            m_immediateFence = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_immediateCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_immediateCommandPool, nullptr);
            m_immediateCommandPool = VK_NULL_HANDLE;
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
        m_overlayPassPrepared = false;
        std::memset(&m_pendingClearColor, 0, sizeof(m_pendingClearColor));
        ResetModernFixedFunctionState(&m_pipelineState);
        m_boundTextures[0] = nullptr;
        m_boundTextures[1] = nullptr;
        m_defaultTexture = nullptr;
        m_liveTextures.clear();
        m_samplerAnisotropySupported = false;
        m_maxSamplerAnisotropy = 1.0f;
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
            if (m_device != VK_NULL_HANDLE && m_swapChain != VK_NULL_HANDLE) {
                ResizeSwapChain();
            }
        }
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

    bool IsFxaaEnabled() const
    {
        return GetCachedGraphicsSettings().antiAliasing == AntiAliasingMode::FXAA;
    }

    bool ShouldUseVulkanFxaaPostProcess() const
    {
        return true;
    }

    int Present(bool vertSync) override
    {
        if (!m_device || !m_swapChain || !m_frameBegun) {
            DbgLog("[Render][Vulkan] Present skipped device=%p swapChain=%p frameBegun=%d\n",
                m_device,
                m_swapChain,
                m_frameBegun ? 1 : 0);
            return -1;
        }

        const bool presentModeChanged = m_verticalSyncEnabled != vertSync;
        if (presentModeChanged) {
            m_verticalSyncEnabled = vertSync;
        }

        if (IsFxaaEnabled() && !m_overlayPassPrepared) {
            if (!PrepareOverlayPass()) {
                m_frameBegun = false;
                return -1;
            }
        }

        if (m_renderPassActive) {
            vkCmdEndRenderPass(GetCurrentCommandBuffer());
            m_renderPassActive = false;
            if (m_currentImageIndex < m_swapChainImageLayouts.size()) {
                m_swapChainImageLayouts[m_currentImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            }
        }

        const VkResult endResult = vkEndCommandBuffer(GetCurrentCommandBuffer());
        if (endResult != VK_SUCCESS) {
            DbgLog("[Render][Vulkan] vkEndCommandBuffer failed: %d\n", static_cast<int>(endResult));
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
            DbgLog("[Render][Vulkan] vkQueueSubmit failed: %d\n", static_cast<int>(result));
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
        m_overlayPassPrepared = false;

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            ResizeSwapChain();
            return 0;
        }
        if (result != VK_SUCCESS) {
            DbgLog("[Render][Vulkan] vkQueuePresentKHR failed: %d\n", static_cast<int>(result));
            m_bootstrap.initHr = static_cast<int>(result);
            return -1;
        }

        if (presentModeChanged) {
            ResizeSwapChain();
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

        if (!TransitionCurrentSwapChainImage(
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT)) {
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            vkFreeMemory(m_device, stagingMemory, nullptr);
            return false;
        }

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

        if (!TransitionCurrentSwapChainImage(
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                0,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)) {
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            vkFreeMemory(m_device, stagingMemory, nullptr);
            return false;
        }

        m_pendingReleaseBuffers.push_back(stagingBuffer);
        m_pendingReleaseMemory.push_back(stagingMemory);
        return true;
    }

    bool BeginScene() override { return EnsureFrameStarted(); }
    bool PrepareOverlayPass() override
    {
        if (!IsFxaaEnabled()) {
            return true;
        }
        if (!m_frameBegun || m_overlayPassPrepared) {
            return m_overlayPassPrepared;
        }
        if (!m_renderPassActive || m_sceneImage == VK_NULL_HANDLE || m_overlayRenderPass == VK_NULL_HANDLE
            || m_currentImageIndex >= m_framebuffers.size() || m_currentImageIndex >= m_swapChainImages.size()) {
            return false;
        }

        VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
        if (commandBuffer == VK_NULL_HANDLE) {
            return false;
        }

        vkCmdEndRenderPass(commandBuffer);
        m_renderPassActive = false;

        if (!ShouldUseVulkanFxaaPostProcess()) {
            VkImageMemoryBarrier sceneToCopy{};
            sceneToCopy.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            sceneToCopy.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            sceneToCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            sceneToCopy.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            sceneToCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            sceneToCopy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            sceneToCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            sceneToCopy.image = m_sceneImage;
            sceneToCopy.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            sceneToCopy.subresourceRange.baseMipLevel = 0;
            sceneToCopy.subresourceRange.levelCount = 1;
            sceneToCopy.subresourceRange.baseArrayLayer = 0;
            sceneToCopy.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &sceneToCopy);

            if (!TransitionCurrentSwapChainImage(
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT)) {
                return false;
            }

            VkImageCopy copyRegion{};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.mipLevel = 0;
            copyRegion.dstSubresource.baseArrayLayer = 0;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.extent.width = m_swapChainExtent.width;
            copyRegion.extent.height = m_swapChainExtent.height;
            copyRegion.extent.depth = 1;
            vkCmdCopyImage(
                commandBuffer,
                m_sceneImage,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                m_swapChainImages[m_currentImageIndex],
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copyRegion);

            if (!TransitionCurrentSwapChainImage(
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)) {
                return false;
            }

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_overlayRenderPass;
            renderPassInfo.framebuffer = m_framebuffers[m_currentImageIndex];
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = m_swapChainExtent;
            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            m_renderPassActive = true;
            m_overlayPassPrepared = true;
            return true;
        }

        VkImageMemoryBarrier sceneToSample{};
        sceneToSample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        sceneToSample.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        sceneToSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sceneToSample.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        sceneToSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        sceneToSample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        sceneToSample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        sceneToSample.image = m_sceneImage;
        sceneToSample.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        sceneToSample.subresourceRange.baseMipLevel = 0;
        sceneToSample.subresourceRange.levelCount = 1;
        sceneToSample.subresourceRange.baseArrayLayer = 0;
        sceneToSample.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &sceneToSample);

        if (!TransitionCurrentSwapChainImage(
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)) {
            return false;
        }

        VkDescriptorSet postDescriptorSet = AllocatePostDescriptorSet(m_sceneImageView);
        if (postDescriptorSet == VK_NULL_HANDLE || m_postPipeline == VK_NULL_HANDLE || m_postPipelineLayout == VK_NULL_HANDLE) {
            return false;
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_overlayRenderPass;
        renderPassInfo.framebuffer = m_framebuffers[m_currentImageIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_swapChainExtent;
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

        ModernDrawConstants constants{};
        constants.screenWidth = static_cast<float>((std::max)(1, m_renderWidth));
        constants.screenHeight = static_cast<float>((std::max)(1, m_renderHeight));

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postPipelineLayout, 0, 1, &postDescriptorSet, 0, nullptr);
        vkCmdPushConstants(commandBuffer, m_postPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
            0, static_cast<uint32_t>(sizeof(constants)), &constants);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        m_renderPassActive = true;
        m_overlayPassPrepared = true;
        return true;
    }
    void EndScene() override {}

    void SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) override
    {
        (void)state;
        (void)matrix;
    }

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
        if (stage < kModernTextureSlotCount) {
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
        if (!width || !height) {
            return;
        }

        const unsigned int scale = static_cast<unsigned int>((std::max)(1, GetCachedGraphicsSettings().textureUpscaleFactor));
        *width = (std::max)(1u, *width * scale);
        *height = (std::max)(1u, *height * scale);
    }

    void ReleaseTextureResource(CTexture* texture) override
    {
        UntrackTexture(texture);
        ReleaseTextureMembers(texture);
    }

    bool CreateTextureResource(CTexture* texture, unsigned int requestedWidth, unsigned int requestedHeight,
        int pixelFormat, unsigned int* outSurfaceWidth, unsigned int* outSurfaceHeight) override
    {
        (void)pixelFormat;
        if (!texture || m_device == VK_NULL_HANDLE) {
            return false;
        }

        ReleaseTextureMembers(texture);
        unsigned int surfaceWidth = requestedWidth;
        unsigned int surfaceHeight = requestedHeight;
        AdjustTextureSize(&surfaceWidth, &surfaceHeight);

        VulkanTextureHandle* textureHandle = CreateTextureHandle(
            static_cast<uint32_t>((std::max)(1u, surfaceWidth)),
            static_cast<uint32_t>((std::max)(1u, surfaceHeight)));
        if (!textureHandle) {
            return false;
        }

        texture->m_backendTextureObject = textureHandle;
        texture->m_backendTextureView = nullptr;
        texture->m_backendTextureUpload = nullptr;
        TrackTexture(texture);

        if (outSurfaceWidth) {
            *outSurfaceWidth = textureHandle->m_width;
        }
        if (outSurfaceHeight) {
            *outSurfaceHeight = textureHandle->m_height;
        }
        return true;
    }

    bool UpdateTextureResource(CTexture* texture, int x, int y, int w, int h,
        const unsigned int* data, bool skipColorKey, int pitch) override
    {
        VulkanTextureHandle* textureHandle = GetVulkanTextureHandle(texture);
        if (!textureHandle || !data || x < 0 || y < 0 || w <= 0 || h <= 0) {
            return false;
        }
        if (static_cast<uint32_t>(x + w) > textureHandle->m_width || static_cast<uint32_t>(y + h) > textureHandle->m_height) {
            DbgLog("[Render][Vulkan] UpdateTextureResource out of bounds tex=%p size=%ux%u update=%d,%d %dx%d\n",
                static_cast<void*>(texture),
                textureHandle->m_width,
                textureHandle->m_height,
                x,
                y,
                w,
                h);
            return false;
        }
        if (!UploadTextureRegion(textureHandle, x, y, w, h, data, skipColorKey, pitch, true)) {
            DbgLog("[Render][Vulkan] UploadTextureRegion failed tex=%p update=%d,%d %dx%d immediate=1 frameBegun=%d renderPass=%d hr=%d\n",
                static_cast<void*>(texture),
                x,
                y,
                w,
                h,
                m_frameBegun ? 1 : 0,
                m_renderPassActive ? 1 : 0,
                m_bootstrap.initHr);
            return false;
        }

        return true;
    }

private:
    static VkAccessFlags AccessMaskForImageLayout(VkImageLayout layout)
    {
        switch (layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        default:
            return 0;
        }
    }

    static VkPipelineStageFlags PipelineStageForImageLayout(VkImageLayout layout)
    {
        switch (layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }
    }

    bool TransitionCurrentSwapChainImage(VkImageLayout newLayout, VkAccessFlags dstAccessMask, VkPipelineStageFlags dstStageMask)
    {
        if (m_currentImageIndex >= m_swapChainImages.size() || m_currentImageIndex >= m_swapChainImageLayouts.size()) {
            return false;
        }

        const VkImageLayout oldLayout = m_swapChainImageLayouts[m_currentImageIndex];
        if (oldLayout == newLayout) {
            return true;
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = AccessMaskForImageLayout(oldLayout);
        barrier.dstAccessMask = dstAccessMask;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_swapChainImages[m_currentImageIndex];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(
            GetCurrentCommandBuffer(),
            PipelineStageForImageLayout(oldLayout),
            dstStageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        m_swapChainImageLayouts[m_currentImageIndex] = newLayout;
        return true;
    }

    static constexpr uint32_t kInvalidQueueFamilyIndex = std::numeric_limits<uint32_t>::max();

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct PipelineEntry {
        bool isLightmap;
        VkPrimitiveTopology topology;
        VkBool32 blendEnable;
        VkBlendFactor srcBlend;
        VkBlendFactor dstBlend;
        VkBool32 depthEnable;
        VkBool32 depthWriteEnable;
        VkCullModeFlags cullMode;
        VkPipeline pipeline;
    };

    bool CreatePipelineResources()
    {
        if (m_device == VK_NULL_HANDLE) {
            return false;
        }

        VkDescriptorSetLayoutBinding layoutBindings[3]{};
        layoutBindings[0].binding = 0;
        layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        layoutBindings[0].descriptorCount = 1;
        layoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        layoutBindings[1].binding = 1;
        layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        layoutBindings[1].descriptorCount = 1;
        layoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        layoutBindings[2].binding = 2;
        layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        layoutBindings[2].descriptorCount = 1;
        layoutBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
        descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayoutInfo.bindingCount = static_cast<uint32_t>(std::size(layoutBindings));
        descriptorLayoutInfo.pBindings = layoutBindings;
        VkResult result = vkCreateDescriptorSetLayout(m_device, &descriptorLayoutInfo, nullptr, &m_descriptorSetLayout);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DestroyPipelineResources();
            return false;
        }

        VkDescriptorPoolSize poolSizes[2]{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes[0].descriptorCount = 16384;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[1].descriptorCount = 8192;

        VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
        descriptorPoolInfo.pPoolSizes = poolSizes;
        descriptorPoolInfo.maxSets = 8192;
        result = vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_descriptorPool);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DestroyPipelineResources();
            return false;
        }

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        const int anisotropicLevel = GetCachedGraphicsSettings().anisotropicLevel;
        const bool useAnisotropic = m_samplerAnisotropySupported && anisotropicLevel > 1;
        samplerInfo.anisotropyEnable = useAnisotropic ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = useAnisotropic
            ? (std::min)(m_maxSamplerAnisotropy, static_cast<float>(anisotropicLevel))
            : 1.0f;
        result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DestroyPipelineResources();
            return false;
        }

        if (!CreateShaderModuleFromBytes(kVulkanVsTlSpirv, kVulkanVsTlSpirvSize, &m_vertexShaderTlModule)
            || !CreateShaderModuleFromBytes(kVulkanVsLmSpirv, kVulkanVsLmSpirvSize, &m_vertexShaderLmModule)
            || !CreateShaderModuleFromBytes(kVulkanPsSpirv, kVulkanPsSpirvSize, &m_fragmentShaderModule)) {
            DestroyPipelineResources();
            return false;
        }

        VkDescriptorSetLayoutBinding postLayoutBindings[2]{};
        postLayoutBindings[0].binding = 0;
        postLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        postLayoutBindings[0].descriptorCount = 1;
        postLayoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        postLayoutBindings[1].binding = 1;
        postLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        postLayoutBindings[1].descriptorCount = 1;
        postLayoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo postDescriptorLayoutInfo{};
        postDescriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        postDescriptorLayoutInfo.bindingCount = static_cast<uint32_t>(std::size(postLayoutBindings));
        postDescriptorLayoutInfo.pBindings = postLayoutBindings;
        result = vkCreateDescriptorSetLayout(m_device, &postDescriptorLayoutInfo, nullptr, &m_postDescriptorSetLayout);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DestroyPipelineResources();
            return false;
        }

        VkSamplerCreateInfo postSamplerInfo{};
        postSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        postSamplerInfo.magFilter = VK_FILTER_LINEAR;
        postSamplerInfo.minFilter = VK_FILTER_LINEAR;
        postSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        postSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        postSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        postSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        postSamplerInfo.minLod = 0.0f;
        postSamplerInfo.maxLod = 0.0f;
        postSamplerInfo.anisotropyEnable = VK_FALSE;
        result = vkCreateSampler(m_device, &postSamplerInfo, nullptr, &m_postSampler);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DestroyPipelineResources();
            return false;
        }

        VulkanPostShaderProgram postShaderProgram{};
        if (!GetVulkanPostShaderProgram(kCompiledPostProcessAntiAliasingMode, &postShaderProgram)
            || !CreateShaderModuleFromBytes(postShaderProgram.vertexBytes, postShaderProgram.vertexByteCount, &m_postVertexShaderModule)
            || !CreateShaderModuleFromBytes(postShaderProgram.fragmentBytes, postShaderProgram.fragmentByteCount, &m_postFxaaFragmentShaderModule)) {
            DestroyPipelineResources();
            return false;
        }

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = static_cast<uint32_t>(sizeof(ModernDrawConstants));

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &m_descriptorSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        result = vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DestroyPipelineResources();
            return false;
        }

        VkPushConstantRange postPushConstantRange{};
        postPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        postPushConstantRange.offset = 0;
        postPushConstantRange.size = static_cast<uint32_t>(sizeof(ModernDrawConstants));

        VkPipelineLayoutCreateInfo postLayoutInfo{};
        postLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        postLayoutInfo.setLayoutCount = 1;
        postLayoutInfo.pSetLayouts = &m_postDescriptorSetLayout;
        postLayoutInfo.pushConstantRangeCount = 1;
        postLayoutInfo.pPushConstantRanges = &postPushConstantRange;

        result = vkCreatePipelineLayout(m_device, &postLayoutInfo, nullptr, &m_postPipelineLayout);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DestroyPipelineResources();
            return false;
        }

        m_defaultTexture = CreateTextureHandle(1, 1);
        if (!m_defaultTexture) {
            DestroyPipelineResources();
            return false;
        }

        const unsigned int whitePixel = 0xFFFFFFFFu;
        if (!UploadTextureRegion(m_defaultTexture, 0, 0, 1, 1, &whitePixel, true, sizeof(unsigned int), true)) {
            DestroyPipelineResources();
            return false;
        }

        return true;
    }

    void DestroyPipelineResources()
    {
        ReleaseCachedPipelines();

        if (m_defaultTexture) {
            m_defaultTexture->Release();
            m_defaultTexture = nullptr;
        }
        if (m_device != VK_NULL_HANDLE && m_postPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_device, m_postPipelineLayout, nullptr);
            m_postPipelineLayout = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device, m_sampler, nullptr);
            m_sampler = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_postSampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device, m_postSampler, nullptr);
            m_postSampler = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_postDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device, m_postDescriptorSetLayout, nullptr);
            m_postDescriptorSetLayout = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
            m_descriptorSetLayout = VK_NULL_HANDLE;
        }

        if (m_device != VK_NULL_HANDLE && m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
            m_pipelineLayout = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_fragmentShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_device, m_fragmentShaderModule, nullptr);
            m_fragmentShaderModule = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_postFxaaFragmentShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_device, m_postFxaaFragmentShaderModule, nullptr);
            m_postFxaaFragmentShaderModule = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_vertexShaderLmModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_device, m_vertexShaderLmModule, nullptr);
            m_vertexShaderLmModule = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_postVertexShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_device, m_postVertexShaderModule, nullptr);
            m_postVertexShaderModule = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_vertexShaderTlModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_device, m_vertexShaderTlModule, nullptr);
            m_vertexShaderTlModule = VK_NULL_HANDLE;
        }
    }

    VkShaderModule GetPostFragmentShaderModule(AntiAliasingMode mode) const
    {
        switch (mode) {
        case AntiAliasingMode::FXAA:
            return m_postFxaaFragmentShaderModule;
        case AntiAliasingMode::SMAA:
            return VK_NULL_HANDLE;

        default:
            return VK_NULL_HANDLE;
        }
    }

    bool CreatePostPipeline(VkRenderPass renderPass, AntiAliasingMode mode, VkPipeline* outPipeline)
    {
        VulkanPostShaderProgram postShaderProgram{};
        const VkShaderModule postFragmentShaderModule = GetPostFragmentShaderModule(mode);
        if (!GetVulkanPostShaderProgram(mode, &postShaderProgram)
            || !outPipeline || renderPass == VK_NULL_HANDLE || m_device == VK_NULL_HANDLE || m_postPipelineLayout == VK_NULL_HANDLE
            || m_postVertexShaderModule == VK_NULL_HANDLE || postFragmentShaderModule == VK_NULL_HANDLE) {
            return false;
        }

        *outPipeline = VK_NULL_HANDLE;

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        VkViewport viewport{};
        viewport.width = 1.0f;
        viewport.height = 1.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{};
        scissor.extent.width = 1;
        scissor.extent.height = 1;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        const VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
        dynamicState.pDynamicStates = dynamicStates;

        VkPipelineShaderStageCreateInfo shaderStages[2]{};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = m_postVertexShaderModule;
        shaderStages[0].pName = postShaderProgram.vertexEntryPoint;
        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = postFragmentShaderModule;
        shaderStages[1].pName = postShaderProgram.fragmentEntryPoint;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(std::size(shaderStages));
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_postPipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        const VkResult result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, outPipeline);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            *outPipeline = VK_NULL_HANDLE;
            return false;
        }

        return true;
    }

    bool CreateShaderModuleFromBytes(const uint8_t* bytes, size_t byteCount, VkShaderModule* outShaderModule)
    {
        if (!bytes || byteCount == 0 || !outShaderModule) {
            return false;
        }

        *outShaderModule = VK_NULL_HANDLE;
        std::vector<uint32_t> codeWords((byteCount + sizeof(uint32_t) - 1) / sizeof(uint32_t), 0);
        std::memcpy(codeWords.data(), bytes, byteCount);

        VkShaderModuleCreateInfo moduleInfo{};
        moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.codeSize = codeWords.size() * sizeof(uint32_t);
        moduleInfo.pCode = codeWords.data();

        const VkResult result = vkCreateShaderModule(m_device, &moduleInfo, nullptr, outShaderModule);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return false;
        }

        return true;
    }

    void ReleaseCachedPipelines()
    {
        for (PipelineEntry& entry : m_pipelines) {
            if (entry.pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_device, entry.pipeline, nullptr);
            }
        }
        m_pipelines.clear();
    }

    VkPipeline GetPipelineState(bool isLightmap, VkPrimitiveTopology topology)
    {
        if (m_device == VK_NULL_HANDLE || m_renderPass == VK_NULL_HANDLE || m_pipelineLayout == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }

        const VkBool32 blendEnable = m_pipelineState.alphaBlendEnable ? VK_TRUE : VK_FALSE;
        const VkBlendFactor srcBlend = ConvertBlendFactorVk(m_pipelineState.srcBlend);
        const VkBlendFactor dstBlend = ConvertBlendFactorVk(m_pipelineState.destBlend);
        const VkBool32 depthEnable = m_pipelineState.depthEnable != D3DZB_FALSE ? VK_TRUE : VK_FALSE;
        const VkBool32 depthWriteEnable = m_pipelineState.depthWriteEnable ? VK_TRUE : VK_FALSE;
        const VkCullModeFlags cullMode = ConvertCullModeVk(m_pipelineState.cullMode);

        for (const PipelineEntry& entry : m_pipelines) {
            if (entry.isLightmap == isLightmap
                && entry.topology == topology
                && entry.blendEnable == blendEnable
                && entry.srcBlend == srcBlend
                && entry.dstBlend == dstBlend
                && entry.depthEnable == depthEnable
                && entry.depthWriteEnable == depthWriteEnable
                && entry.cullMode == cullMode) {
                return entry.pipeline;
            }
        }

        const VkVertexInputBindingDescription bindingDesc = {
            0,
            static_cast<uint32_t>(isLightmap ? sizeof(lmtlvertex3d) : sizeof(tlvertex3d)),
            VK_VERTEX_INPUT_RATE_VERTEX,
        };

        const VkVertexInputAttributeDescription tlAttributes[] = {
            { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(tlvertex3d, x)) },
            { 1, 0, VK_FORMAT_B8G8R8A8_UNORM, static_cast<uint32_t>(offsetof(tlvertex3d, color)) },
            { 2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(tlvertex3d, tu)) },
        };
        const VkVertexInputAttributeDescription lmAttributes[] = {
            { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(lmtlvertex3d, vert) + offsetof(tlvertex3d, x)) },
            { 1, 0, VK_FORMAT_B8G8R8A8_UNORM, static_cast<uint32_t>(offsetof(lmtlvertex3d, vert) + offsetof(tlvertex3d, color)) },
            { 2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(lmtlvertex3d, vert) + offsetof(tlvertex3d, tu)) },
            { 3, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(lmtlvertex3d, tu2)) },
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = isLightmap ? static_cast<uint32_t>(std::size(lmAttributes)) : static_cast<uint32_t>(std::size(tlAttributes));
        vertexInputInfo.pVertexAttributeDescriptions = isLightmap ? lmAttributes : tlAttributes;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = topology;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        VkViewport viewport{};
        viewport.width = 1.0f;
        viewport.height = 1.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{};
        scissor.extent.width = 1;
        scissor.extent.height = 1;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = cullMode;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = depthEnable;
        depthStencil.depthWriteEnable = depthWriteEnable;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.blendEnable = blendEnable;
        colorBlendAttachment.srcColorBlendFactor = srcBlend;
        colorBlendAttachment.dstColorBlendFactor = dstBlend;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        const VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
        dynamicState.pDynamicStates = dynamicStates;

        VkPipelineShaderStageCreateInfo shaderStages[2]{};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = isLightmap ? m_vertexShaderLmModule : m_vertexShaderTlModule;
        shaderStages[0].pName = isLightmap ? "VSMainLM" : "VSMainTL";
        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = m_fragmentShaderModule;
        shaderStages[1].pName = "PSMain";

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(std::size(shaderStages));
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;

        VkPipeline pipeline = VK_NULL_HANDLE;
        const VkResult result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return VK_NULL_HANDLE;
        }

        m_pipelines.push_back({ isLightmap, topology, blendEnable, srcBlend, dstBlend, depthEnable, depthWriteEnable, cullMode, pipeline });
        return pipeline;
    }

    bool CreateDepthResources(const VkExtent2D& extent, VkImage* outImage, VkDeviceMemory* outMemory, VkImageView* outImageView)
    {
        if (!outImage || !outMemory || !outImageView || m_device == VK_NULL_HANDLE || extent.width == 0 || extent.height == 0) {
            return false;
        }

        *outImage = VK_NULL_HANDLE;
        *outMemory = VK_NULL_HANDLE;
        *outImageView = VK_NULL_HANDLE;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = m_depthFormat;
        imageInfo.extent.width = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkResult result = vkCreateImage(m_device, &imageInfo, nullptr, outImage);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(m_device, *outImage, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (allocInfo.memoryTypeIndex == kInvalidQueueFamilyIndex) {
            m_bootstrap.initHr = static_cast<int>(VK_ERROR_MEMORY_MAP_FAILED);
            vkDestroyImage(m_device, *outImage, nullptr);
            *outImage = VK_NULL_HANDLE;
            return false;
        }

        result = vkAllocateMemory(m_device, &allocInfo, nullptr, outMemory);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            vkDestroyImage(m_device, *outImage, nullptr);
            *outImage = VK_NULL_HANDLE;
            return false;
        }

        result = vkBindImageMemory(m_device, *outImage, *outMemory, 0);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            vkFreeMemory(m_device, *outMemory, nullptr);
            vkDestroyImage(m_device, *outImage, nullptr);
            *outMemory = VK_NULL_HANDLE;
            *outImage = VK_NULL_HANDLE;
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = *outImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        result = vkCreateImageView(m_device, &viewInfo, nullptr, outImageView);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            vkFreeMemory(m_device, *outMemory, nullptr);
            vkDestroyImage(m_device, *outImage, nullptr);
            *outImageView = VK_NULL_HANDLE;
            *outMemory = VK_NULL_HANDLE;
            *outImage = VK_NULL_HANDLE;
            return false;
        }

        return true;
    }

    bool CreateSceneColorResources(const VkExtent2D& extent, VkImage* outImage, VkDeviceMemory* outMemory, VkImageView* outImageView)
    {
        if (!outImage || !outMemory || !outImageView || m_device == VK_NULL_HANDLE || extent.width == 0 || extent.height == 0) {
            return false;
        }

        *outImage = VK_NULL_HANDLE;
        *outMemory = VK_NULL_HANDLE;
        *outImageView = VK_NULL_HANDLE;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        imageInfo.extent.width = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkResult result = vkCreateImage(m_device, &imageInfo, nullptr, outImage);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(m_device, *outImage, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (allocInfo.memoryTypeIndex == kInvalidQueueFamilyIndex) {
            m_bootstrap.initHr = static_cast<int>(VK_ERROR_MEMORY_MAP_FAILED);
            vkDestroyImage(m_device, *outImage, nullptr);
            *outImage = VK_NULL_HANDLE;
            return false;
        }

        result = vkAllocateMemory(m_device, &allocInfo, nullptr, outMemory);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            vkDestroyImage(m_device, *outImage, nullptr);
            *outImage = VK_NULL_HANDLE;
            return false;
        }

        result = vkBindImageMemory(m_device, *outImage, *outMemory, 0);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            vkFreeMemory(m_device, *outMemory, nullptr);
            vkDestroyImage(m_device, *outImage, nullptr);
            *outMemory = VK_NULL_HANDLE;
            *outImage = VK_NULL_HANDLE;
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = *outImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        result = vkCreateImageView(m_device, &viewInfo, nullptr, outImageView);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            vkFreeMemory(m_device, *outMemory, nullptr);
            vkDestroyImage(m_device, *outImage, nullptr);
            *outImageView = VK_NULL_HANDLE;
            *outMemory = VK_NULL_HANDLE;
            *outImage = VK_NULL_HANDLE;
            return false;
        }

        return true;
    }

    VulkanTextureHandle* CreateTextureHandle(uint32_t width, uint32_t height)
    {
        VulkanTextureHandle* textureHandle = new VulkanTextureHandle(m_device, width, height);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkResult result = vkCreateImage(m_device, &imageInfo, nullptr, &textureHandle->m_image);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            textureHandle->Release();
            return nullptr;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(m_device, textureHandle->m_image, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (allocInfo.memoryTypeIndex == kInvalidQueueFamilyIndex) {
            m_bootstrap.initHr = static_cast<int>(VK_ERROR_MEMORY_MAP_FAILED);
            textureHandle->Release();
            return nullptr;
        }

        result = vkAllocateMemory(m_device, &allocInfo, nullptr, &textureHandle->m_memory);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            textureHandle->Release();
            return nullptr;
        }

        result = vkBindImageMemory(m_device, textureHandle->m_image, textureHandle->m_memory, 0);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            textureHandle->Release();
            return nullptr;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = textureHandle->m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        result = vkCreateImageView(m_device, &viewInfo, nullptr, &textureHandle->m_view);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            textureHandle->Release();
            return nullptr;
        }

        return textureHandle;
    }

    bool BeginImmediateCommandBuffer(VkCommandBuffer* outCommandBuffer)
    {
        if (!outCommandBuffer || m_device == VK_NULL_HANDLE || m_immediateCommandPool == VK_NULL_HANDLE || m_immediateFence == VK_NULL_HANDLE) {
            return false;
        }

        *outCommandBuffer = VK_NULL_HANDLE;
        if (!m_frameBegun && m_inFlightFence != VK_NULL_HANDLE) {
            VkResult result = vkWaitForFences(m_device, 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
            if (result != VK_SUCCESS) {
                m_bootstrap.initHr = static_cast<int>(result);
                return false;
            }
        }

        VkResult result = vkWaitForFences(m_device, 1, &m_immediateFence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return false;
        }
        result = vkResetFences(m_device, 1, &m_immediateFence);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return false;
        }
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_immediateCommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        result = vkAllocateCommandBuffers(m_device, &allocInfo, outCommandBuffer);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return false;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = vkBeginCommandBuffer(*outCommandBuffer, &beginInfo);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            vkFreeCommandBuffers(m_device, m_immediateCommandPool, 1, outCommandBuffer);
            *outCommandBuffer = VK_NULL_HANDLE;
            return false;
        }

        return true;
    }

    bool EndImmediateCommandBuffer(VkCommandBuffer commandBuffer)
    {
        if (commandBuffer == VK_NULL_HANDLE) {
            return false;
        }

        VkResult result = vkEndCommandBuffer(commandBuffer);
        if (result == VK_SUCCESS) {
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            result = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_immediateFence);
        }
        if (result == VK_SUCCESS) {
            result = vkWaitForFences(m_device, 1, &m_immediateFence, VK_TRUE, UINT64_MAX);
        }
        vkFreeCommandBuffers(m_device, m_immediateCommandPool, 1, &commandBuffer);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return false;
        }

        return true;
    }

    bool UploadTextureRegion(VulkanTextureHandle* textureHandle, int x, int y, int w, int h,
        const unsigned int* data, bool skipColorKey, int pitch, bool immediate)
    {
        if (!textureHandle || !data || w <= 0 || h <= 0) {
            return false;
        }

        const UINT srcPitch = static_cast<UINT>(pitch > 0 ? pitch : w * static_cast<int>(sizeof(unsigned int)));
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

        const VkDeviceSize uploadSize = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h) * sizeof(unsigned int);
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

        std::memcpy(mapped, uploadPixels.data(), static_cast<size_t>(uploadSize));
        vkUnmapMemory(m_device, stagingMemory);

        VkCommandBuffer commandBuffer = immediate ? VK_NULL_HANDLE : GetCurrentCommandBuffer();
        if (immediate) {
            if (!BeginImmediateCommandBuffer(&commandBuffer)) {
                vkDestroyBuffer(m_device, stagingBuffer, nullptr);
                vkFreeMemory(m_device, stagingMemory, nullptr);
                return false;
            }
        }

        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.srcAccessMask = textureHandle->m_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ? VK_ACCESS_SHADER_READ_BIT : 0;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = textureHandle->m_layout;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = textureHandle->m_image;
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.baseMipLevel = 0;
        toTransfer.subresourceRange.levelCount = 1;
        toTransfer.subresourceRange.baseArrayLayer = 0;
        toTransfer.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(
            commandBuffer,
            textureHandle->m_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &toTransfer);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = static_cast<uint32_t>(w);
        copyRegion.bufferImageHeight = static_cast<uint32_t>(h);
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageOffset = { x, y, 0 };
        copyRegion.imageExtent.width = static_cast<uint32_t>(w);
        copyRegion.imageExtent.height = static_cast<uint32_t>(h);
        copyRegion.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, textureHandle->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        VkImageMemoryBarrier toShaderRead{};
        toShaderRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShaderRead.image = textureHandle->m_image;
        toShaderRead.subresourceRange = toTransfer.subresourceRange;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &toShaderRead);

        textureHandle->m_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        if (immediate) {
            const bool submitted = EndImmediateCommandBuffer(commandBuffer);
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            vkFreeMemory(m_device, stagingMemory, nullptr);
            return submitted;
        }

        m_pendingReleaseBuffers.push_back(stagingBuffer);
        m_pendingReleaseMemory.push_back(stagingMemory);
        return true;
    }

    struct TextureDescriptorCacheEntry {
        VkImageView texture0View;
        VkImageView texture1View;
        VkDescriptorSet descriptorSet;
    };

    VkDescriptorSet AllocateTextureDescriptorSet()
    {
        if (m_device == VK_NULL_HANDLE || m_descriptorPool == VK_NULL_HANDLE || m_descriptorSetLayout == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_descriptorSetLayout;

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        const VkResult result = vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSet);
        if (result != VK_SUCCESS) {
            DbgLog("[Render][Vulkan] vkAllocateDescriptorSets failed: %d\n", static_cast<int>(result));
            m_bootstrap.initHr = static_cast<int>(result);
            return VK_NULL_HANDLE;
        }
        return descriptorSet;
    }

    VkDescriptorSet GetOrCreateTextureDescriptorSet(VkImageView texture0View, VkImageView texture1View)
    {
        if (texture0View == VK_NULL_HANDLE || texture1View == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }

        for (const TextureDescriptorCacheEntry& entry : m_descriptorSetCache) {
            if (entry.texture0View == texture0View && entry.texture1View == texture1View) {
                return entry.descriptorSet;
            }
        }

        VkDescriptorSet descriptorSet = AllocateTextureDescriptorSet();
        if (descriptorSet == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }

        VkDescriptorImageInfo imageInfos[2]{};
        imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[0].imageView = texture0View;
        imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[1].imageView = texture1View;
        VkDescriptorImageInfo samplerInfo{};
        samplerInfo.sampler = m_sampler;

        VkWriteDescriptorSet descriptorWrites[3]{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[0].pImageInfo = &imageInfos[0];
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[1].pImageInfo = &imageInfos[1];
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptorSet;
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        descriptorWrites[2].pImageInfo = &samplerInfo;
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(std::size(descriptorWrites)), descriptorWrites, 0, nullptr);

        m_descriptorSetCache.push_back({ texture0View, texture1View, descriptorSet });
        return descriptorSet;
    }

    VkDescriptorSet AllocatePostDescriptorSet(VkImageView sceneImageView)
    {
        if (sceneImageView == VK_NULL_HANDLE || m_device == VK_NULL_HANDLE || m_descriptorPool == VK_NULL_HANDLE
            || m_postDescriptorSetLayout == VK_NULL_HANDLE || m_postSampler == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_postDescriptorSetLayout;

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        const VkResult result = vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSet);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            return VK_NULL_HANDLE;
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = sceneImageView;

        VkDescriptorImageInfo samplerInfo{};
        samplerInfo.sampler = m_postSampler;

        VkWriteDescriptorSet descriptorWrites[2]{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[0].pImageInfo = &imageInfo;
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        descriptorWrites[1].pImageInfo = &samplerInfo;
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(std::size(descriptorWrites)), descriptorWrites, 0, nullptr);

        return descriptorSet;
    }

    void TrackTexture(CTexture* texture)
    {
        if (!texture) {
            return;
        }
        for (CTexture* trackedTexture : m_liveTextures) {
            if (trackedTexture == texture) {
                return;
            }
        }
        m_liveTextures.push_back(texture);
    }

    void UntrackTexture(CTexture* texture)
    {
        if (!texture) {
            return;
        }
        const auto it = std::find(m_liveTextures.begin(), m_liveTextures.end(), texture);
        if (it != m_liveTextures.end()) {
            m_liveTextures.erase(it);
        }
    }

    void ReleaseAllTrackedTextures()
    {
        for (CTexture* texture : m_liveTextures) {
            ReleaseTextureMembers(texture);
        }
        m_liveTextures.clear();
    }

    struct UploadPage {
        VkBuffer buffer;
        VkDeviceMemory memory;
        void* mapped;
        VkDeviceSize size;
        VkDeviceSize cursor;
    };

    bool CreateHostVisibleBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer* outBuffer, VkDeviceMemory* outMemory)
    {
        if (!outBuffer || !outMemory || size == 0) {
            return false;
        }

        *outBuffer = VK_NULL_HANDLE;
        *outMemory = VK_NULL_HANDLE;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
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

    void ReleaseUploadPages(std::vector<UploadPage>& pages)
    {
        for (UploadPage& page : pages) {
            if (page.mapped) {
                vkUnmapMemory(m_device, page.memory);
                page.mapped = nullptr;
            }
            if (page.buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device, page.buffer, nullptr);
                page.buffer = VK_NULL_HANDLE;
            }
            if (page.memory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device, page.memory, nullptr);
                page.memory = VK_NULL_HANDLE;
            }
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

    static VkDeviceSize AlignUploadOffset(VkDeviceSize value, VkDeviceSize alignment)
    {
        if (alignment <= 1) {
            return value;
        }
        const VkDeviceSize mask = alignment - 1;
        return (value + mask) & ~mask;
    }

    bool AllocateUploadSlice(std::vector<UploadPage>& pages,
        VkDeviceSize requiredSize,
        VkDeviceSize alignment,
        VkDeviceSize minimumPageSize,
        VkBufferUsageFlags usage,
        void** outMapped,
        VkBuffer* outBuffer,
        VkDeviceSize* outOffset)
    {
        if (!outMapped || !outBuffer || !outOffset || requiredSize == 0 || m_device == VK_NULL_HANDLE) {
            return false;
        }

        for (UploadPage& page : pages) {
            const VkDeviceSize alignedOffset = AlignUploadOffset(page.cursor, alignment);
            if (alignedOffset + requiredSize <= page.size) {
                *outMapped = static_cast<unsigned char*>(page.mapped) + static_cast<size_t>(alignedOffset);
                *outBuffer = page.buffer;
                *outOffset = alignedOffset;
                page.cursor = alignedOffset + requiredSize;
                return true;
            }
        }

        const VkDeviceSize pageSize = AlignUploadOffset((std::max)(minimumPageSize, requiredSize), alignment);
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        if (!CreateHostVisibleBuffer(pageSize, usage, &buffer, &memory)) {
            return false;
        }

        void* mapped = nullptr;
        const VkResult result = vkMapMemory(m_device, memory, 0, pageSize, 0, &mapped);
        if (result != VK_SUCCESS || !mapped) {
            if (mapped) {
                vkUnmapMemory(m_device, memory);
            }
            vkDestroyBuffer(m_device, buffer, nullptr);
            vkFreeMemory(m_device, memory, nullptr);
            m_bootstrap.initHr = static_cast<int>(result);
            return false;
        }

        pages.push_back({ buffer, memory, mapped, pageSize, requiredSize });
        *outMapped = mapped;
        *outBuffer = buffer;
        *outOffset = 0;
        return true;
    }

    bool AllocateVertexBufferSlice(VkDeviceSize requiredSize, void** outMapped, VkBuffer* outBuffer, VkDeviceSize* outOffset)
    {
        return AllocateUploadSlice(
            m_vertexUploadPages,
            requiredSize,
            16,
            4u * 1024u * 1024u,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            outMapped,
            outBuffer,
            outOffset);
    }

    bool AllocateIndexBufferSlice(VkDeviceSize requiredSize, void** outMapped, VkBuffer* outBuffer, VkDeviceSize* outOffset)
    {
        return AllocateUploadSlice(
            m_indexUploadPages,
            requiredSize,
            4,
            512u * 1024u,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            outMapped,
            outBuffer,
            outOffset);
    }

    void DrawTransformedPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat,
        const void* vertices, DWORD vertexCount, const unsigned short* indices, DWORD indexCount)
    {
        if (!EnsureFrameStarted() || !vertices || vertexCount == 0) {
            return;
        }

        const bool isLightmap = vertexFormat == kModernLightmapFvf;
        if (!isLightmap && vertexFormat != D3DFVF_TLVERTEX) {
            return;
        }

        std::vector<unsigned short> convertedIndices;
        const unsigned short* drawIndices = indices;
        DWORD drawIndexCount = indexCount;
        VkPrimitiveTopology topology = ConvertPrimitiveTopologyVk(primitiveType);
        if (primitiveType == D3DPT_TRIANGLEFAN) {
            convertedIndices = ::BuildTriangleFanIndices(indices, vertexCount, indexCount);
            if (convertedIndices.empty()) {
                return;
            }
            drawIndices = convertedIndices.data();
            drawIndexCount = static_cast<DWORD>(convertedIndices.size());
            topology = ConvertPrimitiveTopologyVk(D3DPT_TRIANGLELIST);
        }
        if (topology == VK_PRIMITIVE_TOPOLOGY_MAX_ENUM) {
            return;
        }

        VkPipeline pipeline = GetPipelineState(isLightmap, topology);
        if (pipeline == VK_NULL_HANDLE) {
            return;
        }

        const size_t vertexStride = isLightmap ? sizeof(lmtlvertex3d) : sizeof(tlvertex3d);
        const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(vertexStride) * static_cast<VkDeviceSize>(vertexCount);
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceSize vertexOffset = 0;
        void* vertexMapped = nullptr;
        if (!AllocateVertexBufferSlice(vertexBytes, &vertexMapped, &vertexBuffer, &vertexOffset)) {
            return;
        }
        std::memcpy(vertexMapped, vertices, static_cast<size_t>(vertexBytes));
        if (isLightmap) {
            lmtlvertex3d* mappedVertices = static_cast<lmtlvertex3d*>(vertexMapped);
            for (DWORD vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
                mappedVertices[vertexIndex].vert.y = static_cast<float>(m_renderHeight) - mappedVertices[vertexIndex].vert.y;
            }
        } else {
            tlvertex3d* mappedVertices = static_cast<tlvertex3d*>(vertexMapped);
            for (DWORD vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
                mappedVertices[vertexIndex].y = static_cast<float>(m_renderHeight) - mappedVertices[vertexIndex].y;
            }
        }

        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceSize indexOffset = 0;
        const bool hasIndices = drawIndices && drawIndexCount > 0;
        if (hasIndices) {
            const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(drawIndexCount) * sizeof(unsigned short);
            void* indexMapped = nullptr;
            if (!AllocateIndexBufferSlice(indexBytes, &indexMapped, &indexBuffer, &indexOffset)) {
                return;
            }
            std::memcpy(indexMapped, drawIndices, static_cast<size_t>(indexBytes));
        }

        ModernDrawConstants constants{};
        constants.screenWidth = static_cast<float>((std::max)(1, m_renderWidth));
        constants.screenHeight = static_cast<float>((std::max)(1, m_renderHeight));
        constants.alphaRef = static_cast<float>(m_pipelineState.alphaRef) / 255.0f;
        const VulkanTextureHandle* texture0 = GetVulkanTextureHandle(m_boundTextures[0]);
        const VulkanTextureHandle* texture1 = GetVulkanTextureHandle(m_boundTextures[1]);
        const bool hasTexture0 = texture0 && texture0->m_view != VK_NULL_HANDLE && texture0->m_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        const bool hasTexture1 = texture1 && texture1->m_view != VK_NULL_HANDLE && texture1->m_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        constants.flags = BuildModernDrawFlags(vertexFormat, m_pipelineState, hasTexture0, hasTexture1);

        VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
        const VkImageView texture0View = hasTexture0 ? texture0->m_view : m_defaultTexture->m_view;
        const VkImageView texture1View = hasTexture1 ? texture1->m_view : m_defaultTexture->m_view;
        VkDescriptorSet descriptorSet = GetOrCreateTextureDescriptorSet(texture0View, texture1View);
        if (descriptorSet == VK_NULL_HANDLE) {
            return;
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, static_cast<uint32_t>(sizeof(constants)), &constants);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
        if (hasIndices) {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, indexOffset, VK_INDEX_TYPE_UINT16);
            vkCmdDrawIndexed(commandBuffer, drawIndexCount, 1, 0, 0, 0);
        } else {
            vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
        }
    }

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

    bool IsPhysicalDeviceSuitable(VkPhysicalDevice device, uint32_t* outGraphicsIndex, uint32_t* outPresentIndex)
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
        if (support.formats.empty() || support.presentModes.empty()) {
            return false;
        }

        VkPhysicalDeviceFeatures features{};
        vkGetPhysicalDeviceFeatures(device, &features);
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        m_samplerAnisotropySupported = features.samplerAnisotropy == VK_TRUE;
        m_maxSamplerAnisotropy = properties.limits.maxSamplerAnisotropy > 1.0f ? properties.limits.maxSamplerAnisotropy : 1.0f;
        return true;
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
        if (m_samplerAnisotropySupported) {
            deviceFeatures.samplerAnisotropy = VK_TRUE;
        }

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

        VkResult result = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DbgLog("[Render] Vulkan vkCreateCommandPool failed (vk=%d).\n", static_cast<int>(result));
            return false;
        }

        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        result = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_immediateCommandPool);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
            DbgLog("[Render] Vulkan vkCreateCommandPool(immediate) failed (vk=%d).\n", static_cast<int>(result));
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
        if (result == VK_SUCCESS) {
            result = vkCreateFence(m_device, &fenceInfo, nullptr, &m_immediateFence);
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
        if (m_verticalSyncEnabled) {
            for (VkPresentModeKHR presentMode : presentModes) {
                if (presentMode == VK_PRESENT_MODE_FIFO_KHR) {
                    return presentMode;
                }
            }
            return presentModes.empty() ? VK_PRESENT_MODE_FIFO_KHR : presentModes.front();
        }

        for (VkPresentModeKHR presentMode : presentModes) {
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return presentMode;
            }
        }
        for (VkPresentModeKHR presentMode : presentModes) {
            if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
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

        VkImage newDepthImage = VK_NULL_HANDLE;
        VkDeviceMemory newDepthMemory = VK_NULL_HANDLE;
        VkImageView newDepthImageView = VK_NULL_HANDLE;
        if (!CreateDepthResources(extent, &newDepthImage, &newDepthMemory, &newDepthImageView)) {
            vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
            return false;
        }

        VkRenderPass newRenderPass = VK_NULL_HANDLE;
        VkRenderPass newOverlayRenderPass = VK_NULL_HANDLE;
        VkImage newSceneImage = VK_NULL_HANDLE;
        VkDeviceMemory newSceneMemory = VK_NULL_HANDLE;
        VkImageView newSceneImageView = VK_NULL_HANDLE;
        VkFramebuffer newSceneFramebuffer = VK_NULL_HANDLE;
        VkPipeline newPostPipeline = VK_NULL_HANDLE;

        if (IsFxaaEnabled()) {
            if (!CreateRenderPass(surfaceFormat.format,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    &newRenderPass)
                || !CreateRenderPass(surfaceFormat.format,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    &newOverlayRenderPass)) {
                if (newOverlayRenderPass != VK_NULL_HANDLE) {
                    vkDestroyRenderPass(m_device, newOverlayRenderPass, nullptr);
                }
                if (newRenderPass != VK_NULL_HANDLE) {
                    vkDestroyRenderPass(m_device, newRenderPass, nullptr);
                }
                vkDestroyImageView(m_device, newDepthImageView, nullptr);
                vkFreeMemory(m_device, newDepthMemory, nullptr);
                vkDestroyImage(m_device, newDepthImage, nullptr);
                vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
                return false;
            }
            if (ShouldUseVulkanFxaaPostProcess()
                && !CreatePostPipeline(newOverlayRenderPass, kCompiledPostProcessAntiAliasingMode, &newPostPipeline)) {
                vkDestroyRenderPass(m_device, newOverlayRenderPass, nullptr);
                vkDestroyRenderPass(m_device, newRenderPass, nullptr);
                vkDestroyImageView(m_device, newDepthImageView, nullptr);
                vkFreeMemory(m_device, newDepthMemory, nullptr);
                vkDestroyImage(m_device, newDepthImage, nullptr);
                vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
                return false;
            }
        } else if (!CreateRenderPass(surfaceFormat.format,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                &newRenderPass)) {
            vkDestroyImageView(m_device, newDepthImageView, nullptr);
            vkFreeMemory(m_device, newDepthMemory, nullptr);
            vkDestroyImage(m_device, newDepthImage, nullptr);
            vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
            return false;
        }

        std::vector<VkImageView> newImageViews;
        if (!CreateImageViews(newImages, surfaceFormat.format, &newImageViews)) {
            if (newPostPipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_device, newPostPipeline, nullptr);
            }
            if (newOverlayRenderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(m_device, newOverlayRenderPass, nullptr);
            }
            vkDestroyRenderPass(m_device, newRenderPass, nullptr);
            vkDestroyImageView(m_device, newDepthImageView, nullptr);
            vkFreeMemory(m_device, newDepthMemory, nullptr);
            vkDestroyImage(m_device, newDepthImage, nullptr);
            vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
            return false;
        }

        std::vector<VkFramebuffer> newFramebuffers;
        const VkRenderPass framebufferRenderPass = newOverlayRenderPass != VK_NULL_HANDLE ? newOverlayRenderPass : newRenderPass;
        if (!CreateFramebuffers(framebufferRenderPass, newImageViews, newDepthImageView, extent, &newFramebuffers)) {
            DestroyImageViews(newImageViews);
            if (newPostPipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_device, newPostPipeline, nullptr);
            }
            if (newOverlayRenderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(m_device, newOverlayRenderPass, nullptr);
            }
            vkDestroyRenderPass(m_device, newRenderPass, nullptr);
            vkDestroyImageView(m_device, newDepthImageView, nullptr);
            vkFreeMemory(m_device, newDepthMemory, nullptr);
            vkDestroyImage(m_device, newDepthImage, nullptr);
            vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
            return false;
        }

        if (IsFxaaEnabled()) {
            if (!CreateSceneColorResources(extent, &newSceneImage, &newSceneMemory, &newSceneImageView)
                || !CreateSceneFramebuffer(newRenderPass, newSceneImageView, newDepthImageView, extent, &newSceneFramebuffer)) {
                if (newSceneFramebuffer != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(m_device, newSceneFramebuffer, nullptr);
                }
                if (newSceneImageView != VK_NULL_HANDLE) {
                    vkDestroyImageView(m_device, newSceneImageView, nullptr);
                }
                if (newSceneMemory != VK_NULL_HANDLE) {
                    vkFreeMemory(m_device, newSceneMemory, nullptr);
                }
                if (newSceneImage != VK_NULL_HANDLE) {
                    vkDestroyImage(m_device, newSceneImage, nullptr);
                }
                DestroyFramebuffers(newFramebuffers);
                DestroyImageViews(newImageViews);
                if (newPostPipeline != VK_NULL_HANDLE) {
                    vkDestroyPipeline(m_device, newPostPipeline, nullptr);
                }
                vkDestroyRenderPass(m_device, newOverlayRenderPass, nullptr);
                vkDestroyRenderPass(m_device, newRenderPass, nullptr);
                vkDestroyImageView(m_device, newDepthImageView, nullptr);
                vkFreeMemory(m_device, newDepthMemory, nullptr);
                vkDestroyImage(m_device, newDepthImage, nullptr);
                vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
                return false;
            }
        }

        std::vector<VkCommandBuffer> newCommandBuffers;
        if (!AllocateCommandBuffers(static_cast<uint32_t>(newFramebuffers.size()), &newCommandBuffers)) {
            if (newSceneFramebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(m_device, newSceneFramebuffer, nullptr);
            }
            if (newSceneImageView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, newSceneImageView, nullptr);
            }
            if (newSceneMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device, newSceneMemory, nullptr);
            }
            if (newSceneImage != VK_NULL_HANDLE) {
                vkDestroyImage(m_device, newSceneImage, nullptr);
            }
            DestroyFramebuffers(newFramebuffers);
            DestroyImageViews(newImageViews);
            if (newPostPipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_device, newPostPipeline, nullptr);
            }
            if (newOverlayRenderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(m_device, newOverlayRenderPass, nullptr);
            }
            vkDestroyRenderPass(m_device, newRenderPass, nullptr);
            vkDestroyImageView(m_device, newDepthImageView, nullptr);
            vkFreeMemory(m_device, newDepthMemory, nullptr);
            vkDestroyImage(m_device, newDepthImage, nullptr);
            vkDestroySwapchainKHR(m_device, newSwapChain, nullptr);
            return false;
        }

        DestroySwapChainResources();
        m_swapChain = newSwapChain;
        m_swapChainImages = std::move(newImages);
        m_swapChainImageLayouts.assign(m_swapChainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);
        m_swapChainImageViews = std::move(newImageViews);
        m_swapChainFormat = surfaceFormat.format;
        m_swapChainExtent = extent;
        m_renderPass = newRenderPass;
        m_overlayRenderPass = newOverlayRenderPass;
        m_postPipeline = newPostPipeline;
        m_sceneImage = newSceneImage;
        m_sceneMemory = newSceneMemory;
        m_sceneImageView = newSceneImageView;
        m_sceneFramebuffer = newSceneFramebuffer;
        m_depthImage = newDepthImage;
        m_depthMemory = newDepthMemory;
        m_depthImageView = newDepthImageView;
        m_framebuffers = std::move(newFramebuffers);
        m_commandBuffers = std::move(newCommandBuffers);
        m_currentImageIndex = 0;
        m_overlayPassPrepared = false;
        return true;
    }

    bool CreateRenderPass(VkFormat format, VkAttachmentLoadOp colorLoadOp, VkImageLayout colorInitialLayout, VkImageLayout colorFinalLayout, VkRenderPass* outRenderPass)
    {
        if (!outRenderPass) {
            return false;
        }

        *outRenderPass = VK_NULL_HANDLE;
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = colorLoadOp;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = colorInitialLayout;
        colorAttachment.finalLayout = colorFinalLayout;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = m_depthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 1;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        const VkAttachmentDescription attachments[] = {
            colorAttachment,
            depthAttachment,
        };

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(std::size(attachments));
        renderPassInfo.pAttachments = attachments;
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

    bool CreateSceneFramebuffer(VkRenderPass renderPass, VkImageView colorImageView, VkImageView depthImageView,
        const VkExtent2D& extent, VkFramebuffer* outFramebuffer)
    {
        if (!outFramebuffer || renderPass == VK_NULL_HANDLE || colorImageView == VK_NULL_HANDLE || depthImageView == VK_NULL_HANDLE) {
            return false;
        }

        *outFramebuffer = VK_NULL_HANDLE;
        const VkImageView attachments[] = {
            colorImageView,
            depthImageView,
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(std::size(attachments));
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        const VkResult result = vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, outFramebuffer);
        if (result != VK_SUCCESS) {
            m_bootstrap.initHr = static_cast<int>(result);
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
        VkImageView depthImageView,
        const VkExtent2D& extent, std::vector<VkFramebuffer>* outFramebuffers)
    {
        if (!outFramebuffers || depthImageView == VK_NULL_HANDLE) {
            return false;
        }

        outFramebuffers->clear();
        outFramebuffers->reserve(imageViews.size());
        for (VkImageView imageView : imageViews) {
            const VkImageView attachments[] = {
                imageView,
                depthImageView,
            };
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(std::size(attachments));
            framebufferInfo.pAttachments = attachments;
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
        ReleaseCachedPipelines();

        if (!m_commandBuffers.empty()) {
            vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
            m_commandBuffers.clear();
        }
        if (m_postPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device, m_postPipeline, nullptr);
            m_postPipeline = VK_NULL_HANDLE;
        }
        DestroyFramebuffers(m_framebuffers);
        DestroyImageViews(m_swapChainImageViews);
        if (m_sceneFramebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device, m_sceneFramebuffer, nullptr);
            m_sceneFramebuffer = VK_NULL_HANDLE;
        }
        if (m_sceneImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_sceneImageView, nullptr);
            m_sceneImageView = VK_NULL_HANDLE;
        }
        if (m_sceneMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_sceneMemory, nullptr);
            m_sceneMemory = VK_NULL_HANDLE;
        }
        if (m_sceneImage != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_sceneImage, nullptr);
            m_sceneImage = VK_NULL_HANDLE;
        }
        if (m_depthImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_depthImageView, nullptr);
            m_depthImageView = VK_NULL_HANDLE;
        }
        if (m_depthMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_depthMemory, nullptr);
            m_depthMemory = VK_NULL_HANDLE;
        }
        if (m_depthImage != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_depthImage, nullptr);
            m_depthImage = VK_NULL_HANDLE;
        }
        if (m_renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device, m_renderPass, nullptr);
            m_renderPass = VK_NULL_HANDLE;
        }
        if (m_overlayRenderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device, m_overlayRenderPass, nullptr);
            m_overlayRenderPass = VK_NULL_HANDLE;
        }
        if (m_swapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
            m_swapChain = VK_NULL_HANDLE;
        }
        m_swapChainImages.clear();
        m_swapChainImageLayouts.clear();
        m_overlayPassPrepared = false;
    }

    bool EnsureFrameStarted()
    {
        if (m_frameBegun) {
            if (m_renderPassActive) {
                return true;
            }

            VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
            if (commandBuffer == VK_NULL_HANDLE || m_renderPass == VK_NULL_HANDLE || m_currentImageIndex >= m_framebuffers.size()) {
                return false;
            }

            if (!IsFxaaEnabled()) {
                if (!TransitionCurrentSwapChainImage(
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)) {
                    return false;
                }
            }

            VkClearValue clearValues[2]{};
            clearValues[0].color = m_pendingClearColor;
            clearValues[1].depthStencil.depth = 1.0f;
            clearValues[1].depthStencil.stencil = 0;
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_renderPass;
            renderPassInfo.framebuffer = IsFxaaEnabled() ? m_sceneFramebuffer : m_framebuffers[m_currentImageIndex];
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = m_swapChainExtent;
            renderPassInfo.clearValueCount = static_cast<uint32_t>(std::size(clearValues));
            renderPassInfo.pClearValues = clearValues;
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

            m_renderPassActive = true;
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

        if (!IsFxaaEnabled()) {
            if (!TransitionCurrentSwapChainImage(
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)) {
                return false;
            }
        }

        VkClearValue clearValues[2]{};
        clearValues[0].color = m_pendingClearColor;
        clearValues[1].depthStencil.depth = 1.0f;
        clearValues[1].depthStencil.stencil = 0;
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass;
        renderPassInfo.framebuffer = IsFxaaEnabled() ? m_sceneFramebuffer : m_framebuffers[m_currentImageIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_swapChainExtent;
        renderPassInfo.clearValueCount = static_cast<uint32_t>(std::size(clearValues));
        renderPassInfo.pClearValues = clearValues;
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
        m_overlayPassPrepared = false;
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
        m_overlayPassPrepared = false;
        return true;
    }

    VkResult BeginFrame()
    {
        ReleasePendingTransferResources();

        VkResult result = vkWaitForFences(m_device, 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) {
            DbgLog("[Render][Vulkan] vkWaitForFences(frame) failed: %d\n", static_cast<int>(result));
            m_bootstrap.initHr = static_cast<int>(result);
            return result;
        }
        result = vkResetFences(m_device, 1, &m_inFlightFence);
        if (result != VK_SUCCESS) {
            DbgLog("[Render][Vulkan] vkResetFences(frame) failed: %d\n", static_cast<int>(result));
            m_bootstrap.initHr = static_cast<int>(result);
            return result;
        }
        if (m_descriptorPool != VK_NULL_HANDLE) {
            result = vkResetDescriptorPool(m_device, m_descriptorPool, 0);
            if (result != VK_SUCCESS) {
                DbgLog("[Render][Vulkan] vkResetDescriptorPool failed: %d\n", static_cast<int>(result));
                m_bootstrap.initHr = static_cast<int>(result);
                return result;
            }
        }
        m_descriptorSetCache.clear();
        ResetUploadPageCursors(m_vertexUploadPages);
        ResetUploadPageCursors(m_indexUploadPages);
        m_overlayPassPrepared = false;

        result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphore, VK_NULL_HANDLE, &m_currentImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            ResizeSwapChain();
            return result;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            DbgLog("[Render][Vulkan] vkAcquireNextImageKHR failed: %d\n", static_cast<int>(result));
            m_bootstrap.initHr = static_cast<int>(result);
            return result;
        }

        VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
        result = vkResetCommandBuffer(commandBuffer, 0);
        if (result != VK_SUCCESS) {
            DbgLog("[Render][Vulkan] vkResetCommandBuffer failed: %d\n", static_cast<int>(result));
            m_bootstrap.initHr = static_cast<int>(result);
            return result;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (result != VK_SUCCESS) {
            DbgLog("[Render][Vulkan] vkBeginCommandBuffer failed: %d\n", static_cast<int>(result));
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
        m_overlayPassPrepared = false;
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
    VkRenderPass m_overlayRenderPass;
    VkFormat m_depthFormat;
    VkImage m_sceneImage;
    VkDeviceMemory m_sceneMemory;
    VkImageView m_sceneImageView;
    VkFramebuffer m_sceneFramebuffer;
    VkImage m_depthImage;
    VkDeviceMemory m_depthMemory;
    VkImageView m_depthImageView;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorSetLayout m_postDescriptorSetLayout;
    VkDescriptorPool m_descriptorPool;
    VkSampler m_sampler;
    VkSampler m_postSampler;
    VkPipelineLayout m_pipelineLayout;
    VkPipelineLayout m_postPipelineLayout;
    VkPipeline m_postPipeline;
    VkShaderModule m_vertexShaderTlModule;
    VkShaderModule m_vertexShaderLmModule;
    VkShaderModule m_fragmentShaderModule;
    VkShaderModule m_postVertexShaderModule;
    VkShaderModule m_postFxaaFragmentShaderModule;
    VkCommandPool m_commandPool;
    VkCommandPool m_immediateCommandPool;
    VkSemaphore m_imageAvailableSemaphore;
    VkSemaphore m_renderFinishedSemaphore;
    VkFence m_inFlightFence;
    VkFence m_immediateFence;
    uint32_t m_graphicsQueueFamilyIndex;
    uint32_t m_presentQueueFamilyIndex;
    uint32_t m_currentImageIndex;
    bool m_frameBegun;
    bool m_renderPassActive;
    bool m_pendingDepthClear;
    bool m_verticalSyncEnabled;
    bool m_overlayPassPrepared;
    VkClearColorValue m_pendingClearColor;
    ModernFixedFunctionState m_pipelineState;
    CTexture* m_boundTextures[kModernTextureSlotCount];
    VulkanTextureHandle* m_defaultTexture;
    bool m_samplerAnisotropySupported;
    float m_maxSamplerAnisotropy;
    std::vector<CTexture*> m_liveTextures;
    std::vector<VkImage> m_swapChainImages;
    std::vector<VkImageLayout> m_swapChainImageLayouts;
    std::vector<VkImageView> m_swapChainImageViews;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<TextureDescriptorCacheEntry> m_descriptorSetCache;
    std::vector<PipelineEntry> m_pipelines;
    std::vector<UploadPage> m_vertexUploadPages;
    std::vector<UploadPage> m_indexUploadPages;
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
    bool PrepareOverlayPass() override { return false; }
    void EndScene() override {}
    void SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) override { (void)state; (void)matrix; }
    void SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override { (void)state; (void)value; }
    void SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override { (void)stage; (void)type; (void)value; }
    void BindTexture(DWORD stage, CTexture* texture) override { (void)stage; (void)texture; }
    void DrawPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat, const void* vertices, DWORD vertexCount, DWORD flags) override { (void)primitiveType; (void)vertexFormat; (void)vertices; (void)vertexCount; (void)flags; }
    void DrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexFormat, const void* vertices, DWORD vertexCount, const unsigned short* indices, DWORD indexCount, DWORD flags) override { (void)primitiveType; (void)vertexFormat; (void)vertices; (void)vertexCount; (void)indices; (void)indexCount; (void)flags; }
    void AdjustTextureSize(unsigned int* width, unsigned int* height) override { if (width && height) { const unsigned int scale = static_cast<unsigned int>((std::max)(1, GetCachedGraphicsSettings().textureUpscaleFactor)); *width = (std::max)(1u, *width * scale); *height = (std::max)(1u, *height * scale); } }
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
    bool PrepareOverlayPass() override { return m_active->PrepareOverlayPass(); }
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

#if RO_HAS_VULKAN
namespace {

VkPrimitiveTopology ConvertPrimitiveTopologyVk(D3DPRIMITIVETYPE primitiveType)
{
    switch (primitiveType) {
    case D3DPT_POINTLIST:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case D3DPT_LINELIST:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case D3DPT_LINESTRIP:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case D3DPT_TRIANGLELIST:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case D3DPT_TRIANGLESTRIP:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    default:
        return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    }
}

VkBlendFactor ConvertBlendFactorVk(D3DBLEND blend)
{
    switch (blend) {
    case D3DBLEND_ZERO:
        return VK_BLEND_FACTOR_ZERO;
    case D3DBLEND_ONE:
        return VK_BLEND_FACTOR_ONE;
    case D3DBLEND_SRCCOLOR:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case D3DBLEND_SRCALPHA:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case D3DBLEND_DESTALPHA:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case D3DBLEND_INVDESTALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case D3DBLEND_DESTCOLOR:
        return VK_BLEND_FACTOR_DST_COLOR;
    case D3DBLEND_INVDESTCOLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    default:
        return VK_BLEND_FACTOR_ONE;
    }
}

VkCullModeFlags ConvertCullModeVk(D3DCULL cullMode)
{
    switch (cullMode) {
    case D3DCULL_CW:
        return VK_CULL_MODE_FRONT_BIT;
    case D3DCULL_CCW:
        return VK_CULL_MODE_BACK_BIT;
    default:
        return VK_CULL_MODE_NONE;
    }
}

} // namespace
#endif