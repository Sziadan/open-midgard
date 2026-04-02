#pragma once

#include <cstdint>

#include "render3d/RenderBackend.h"

#if !RO_PLATFORM_WINDOWS
struct RECT;
struct POINT;
#endif

using RoQtPlatformWindowProc = std::intptr_t (*)(RoNativeWindowHandle, unsigned int, std::uintptr_t, std::intptr_t);

#if RO_ENABLE_QT6_UI
class QWindow;

bool RoQtCreateMainWindow(const char* title,
    int width,
    int height,
    bool fullscreen,
    RoQtPlatformWindowProc windowProc,
    RoNativeWindowHandle* outWindow);
void RoQtDestroyMainWindow(RoNativeWindowHandle window);
void RoQtProcessEvents();
bool RoQtGetClientRect(RoNativeWindowHandle window, RECT* rect);
bool RoQtScreenToClient(RoNativeWindowHandle window, POINT* point);
bool RoQtGetCursorPos(POINT* point);
bool RoQtSetWindowTitle(RoNativeWindowHandle window, const char* title);
bool RoQtShowWindow(RoNativeWindowHandle window);
bool RoQtFocusWindow(RoNativeWindowHandle window);
QWindow* RoQtGetQWindow(RoNativeWindowHandle window);
#else
class QWindow;

inline bool RoQtCreateMainWindow(const char*, int, int, bool, RoQtPlatformWindowProc, RoNativeWindowHandle*) { return false; }
inline void RoQtDestroyMainWindow(RoNativeWindowHandle) {}
inline void RoQtProcessEvents() {}
inline bool RoQtGetClientRect(RoNativeWindowHandle, RECT*) { return false; }
inline bool RoQtScreenToClient(RoNativeWindowHandle, POINT*) { return false; }
inline bool RoQtGetCursorPos(POINT*) { return false; }
inline bool RoQtSetWindowTitle(RoNativeWindowHandle, const char*) { return false; }
inline bool RoQtShowWindow(RoNativeWindowHandle) { return false; }
inline bool RoQtFocusWindow(RoNativeWindowHandle) { return false; }
inline QWindow* RoQtGetQWindow(RoNativeWindowHandle) { return nullptr; }
#endif