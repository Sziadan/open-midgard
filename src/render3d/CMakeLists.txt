cmake_minimum_required(VERSION 3.20)

add_library(ro_render3d STATIC
    D3dutil.h
    D3dutil.cpp
    Device.h
    Device.cpp
    GraphicsSettings.h
    GraphicsSettings.cpp
    ModernRenderState.h
    ModernRenderState.cpp
    RenderBackend.h
    RenderBackend.cpp
    RenderDevice.h
    RenderDevice.cpp
)

target_include_directories(ro_render3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_render3d PUBLIC
    ro_render
    ro_core
)

set(RO_HAS_VULKAN 0)
if(DEFINED ENV{VULKAN_SDK})
    set(VULKAN_SDK_DIR "$ENV{VULKAN_SDK}")
    set(VULKAN_INCLUDE_DIR "${VULKAN_SDK_DIR}/Include")
    if(EXISTS "${VULKAN_INCLUDE_DIR}/vulkan/vulkan.h")
        set(RO_HAS_VULKAN 1)
        target_include_directories(ro_render3d PUBLIC "${VULKAN_INCLUDE_DIR}")
        message(STATUS "Vulkan SDK detected for ro_render3d: ${VULKAN_SDK_DIR}")
    else()
        message(STATUS "VULKAN_SDK is set but Vulkan headers were not found for ro_render3d")
    endif()
endif()

target_compile_definitions(ro_render3d PUBLIC RO_HAS_VULKAN=${RO_HAS_VULKAN})
