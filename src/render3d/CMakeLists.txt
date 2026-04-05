cmake_minimum_required(VERSION 3.20)

add_library(ro_render3d STATIC
    D3dutil.h
    Device.h
    GraphicsSettings.h
    GraphicsSettings.cpp
    ModernRenderState.h
    ModernRenderState.cpp
    RenderBackend.h
    RenderBackend.cpp
    RenderDevice.h
)

if(WIN32)
    target_sources(ro_render3d PRIVATE
        D3dutil.cpp
        Device.cpp
        RenderDevice.cpp
    )
else()
    target_sources(ro_render3d PRIVATE
        D3dutil.cpp
        Device_nonwin.cpp
        RenderDevice.cpp
    )
endif()

target_include_directories(ro_render3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_render3d PUBLIC
    ro_render
    ro_core
)

if(NOT WIN32 AND RO_ENABLE_QT6_UI)
    target_link_libraries(ro_render3d PUBLIC Qt6::Gui)
endif()

set(RO_HAS_VULKAN 0)
if(WIN32 AND DEFINED ENV{VULKAN_SDK})
    set(VULKAN_SDK_DIR "$ENV{VULKAN_SDK}")
    set(VULKAN_INCLUDE_DIR "${VULKAN_SDK_DIR}/Include")
    if(EXISTS "${VULKAN_INCLUDE_DIR}/vulkan/vulkan.h")
        set(RO_HAS_VULKAN 1)
        target_include_directories(ro_render3d PUBLIC "${VULKAN_INCLUDE_DIR}")
        message(STATUS "Vulkan SDK detected for ro_render3d: ${VULKAN_SDK_DIR}")
    else()
        message(STATUS "VULKAN_SDK is set but Vulkan headers were not found for ro_render3d")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Vulkan REQUIRED)
    if(TARGET Vulkan::Vulkan)
        set(RO_HAS_VULKAN 1)
        target_link_libraries(ro_render3d PUBLIC Vulkan::Vulkan)
        message(STATUS "Vulkan package detected for ro_render3d: ${Vulkan_INCLUDE_DIRS}")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_package(Vulkan REQUIRED COMPONENTS MoltenVK)
    if(TARGET Vulkan::Vulkan)
        set(RO_HAS_VULKAN 1)
        target_link_libraries(ro_render3d PUBLIC Vulkan::Vulkan)
        message(STATUS "Vulkan package detected for ro_render3d: ${Vulkan_INCLUDE_DIRS}")
    endif()
endif()

target_compile_definitions(ro_render3d PUBLIC RO_HAS_VULKAN=${RO_HAS_VULKAN})
