#include "QtUiRuntime.h"

bool IsQtUiRuntimeCompiled()
{
    return false;
}

bool IsQtUiRuntimeEnabled()
{
    return false;
}

void InitializeQtUiRuntime(RoNativeWindowHandle)
{
}

void ShutdownQtUiRuntime()
{
}

void ProcessQtUiRuntimeEvents()
{
}

void NotifyQtUiRuntimeWindowMessage(RoWindowMessage, RoWindowWParam, RoWindowLParam)
{
}

bool HandleQtUiRuntimeWindowMessage(RoWindowMessage, RoWindowWParam, RoWindowLParam)
{
    return false;
}

bool CompositeQtUiMenuOverlay(void*, int, int, int)
{
    return false;
}

bool RenderQtUiMenuOverlayTexture(CTexture*, int, int)
{
    return false;
}

bool CompositeQtUiGameplayOverlay(CGameMode&, void*, int, int, int)
{
    return false;
}

bool RenderQtUiGameplayOverlayTexture(CGameMode&, CTexture*, int, int)
{
    return false;
}