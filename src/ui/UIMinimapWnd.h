#pragma once

#include "UIFrameWnd.h"

#include <array>
#include <string>

class UIBitmapButton;

class UIRoMapWnd : public UIFrameWnd {
public:
    UIRoMapWnd();
    ~UIRoMapWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    bool IsUpdateNeed() override;
    int SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra) override;
    void OnCreate(int x, int y) override;
    void OnProcess() override;
    void OnDraw() override;
    void OnWheel(int delta) override;
    void StoreInfo() override;

private:
    void EnsureCreated();
    void LayoutChildren();
    void LoadAssets();
    void ReleaseAssets();
    void UpdateMinimapBitmap();
    std::string GetCurrentMinimapBitmapName() const;
    unsigned long long BuildVisualStateToken() const;

    bool m_controlsCreated;
    UIBitmapButton* m_closeButton;
    HBITMAP m_titleBarBitmap;
    HBITMAP m_titleTextBitmap;
    HBITMAP m_bodyBitmap;
    HBITMAP m_mapBitmap;
    int m_mapBitmapWidth;
    int m_mapBitmapHeight;
    std::string m_loadedBitmapName;
    std::string m_loadedBitmapPath;
    unsigned long long m_lastVisualStateToken;
    bool m_hasVisualStateToken;
    int m_lastPlayerX;
    int m_lastPlayerY;
    int m_lastPlayerDir;
    u32 m_lastDynamicInvalidateTick;
};
