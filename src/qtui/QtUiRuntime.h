#pragma once

#include <cstdint>

#include "render3d/RenderBackend.h"

#if RO_PLATFORM_WINDOWS
#include <windows.h>
using RoWindowMessage = UINT;
using RoWindowWParam = WPARAM;
using RoWindowLParam = LPARAM;
#else
using RoWindowMessage = unsigned int;
using RoWindowWParam = std::uintptr_t;
using RoWindowLParam = std::intptr_t;
#endif

class CGameMode;
class CTexture;

#if RO_ENABLE_QT6_UI
bool IsQtUiRuntimeCompiled();
bool IsQtUiRuntimeEnabled();
bool QtUiRuntimeNeedsRender();
void InitializeQtUiRuntime(RoNativeWindowHandle mainWindow);
void ShutdownQtUiRuntime();
void ProcessQtUiRuntimeEvents();
void NotifyQtUiRuntimeWindowMessage(RoWindowMessage msg, RoWindowWParam wParam, RoWindowLParam lParam);
bool HandleQtUiRuntimeWindowMessage(RoWindowMessage msg, RoWindowWParam wParam, RoWindowLParam lParam);
bool CompositeQtUiMenuOverlay(void* bgraPixels, int width, int height, int pitch);
bool RenderQtUiMenuOverlayTexture(CTexture* texture, int width, int height);
bool CompositeQtUiGameplayOverlay(CGameMode& mode, void* bgraPixels, int width, int height, int pitch);
bool RenderQtUiGameplayOverlayTexture(CGameMode& mode, CTexture* texture, int width, int height);
#else
inline bool IsQtUiRuntimeCompiled() { return false; }
inline bool IsQtUiRuntimeEnabled() { return false; }
inline bool QtUiRuntimeNeedsRender() { return false; }
inline void InitializeQtUiRuntime(RoNativeWindowHandle) {}
inline void ShutdownQtUiRuntime() {}
inline void ProcessQtUiRuntimeEvents() {}
inline void NotifyQtUiRuntimeWindowMessage(RoWindowMessage, RoWindowWParam, RoWindowLParam) {}
inline bool HandleQtUiRuntimeWindowMessage(RoWindowMessage, RoWindowWParam, RoWindowLParam) { return false; }
inline bool CompositeQtUiMenuOverlay(void*, int, int, int) { return false; }
inline bool RenderQtUiMenuOverlayTexture(CTexture*, int, int) { return false; }
inline bool CompositeQtUiGameplayOverlay(CGameMode&, void*, int, int, int) { return false; }
inline bool RenderQtUiGameplayOverlayTexture(CGameMode&, CTexture*, int, int) { return false; }
#endif
