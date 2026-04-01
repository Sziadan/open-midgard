#pragma once

#include "UIFrameWnd.h"

#include <array>
#include <string>
#include <vector>

class UIBitmapButton;
class QImage;

class UIRoMapWnd : public UIFrameWnd {
public:
    struct DisplayMarker {
        int x = 0;
        int y = 0;
        int radius = 0;
        unsigned int color = 0;
    };

    struct DisplayData {
        int mapX = 0;
        int mapY = 0;
        int mapWidth = 0;
        int mapHeight = 0;
        int closeX = 0;
        int closeY = 0;
        int closeWidth = 0;
        int closeHeight = 0;
        int coordsX = 0;
        int coordsY = 0;
        int coordsWidth = 0;
        int coordsHeight = 0;
        int imageRevision = 0;
        std::string mapName;
        std::string coordsText;
        std::vector<DisplayMarker> markers;
    };

    UIRoMapWnd();
    ~UIRoMapWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    bool IsUpdateNeed() override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    void OnCreate(int x, int y) override;
    void OnProcess() override;
    void OnDraw() override;
    void OnWheel(int delta) override;
    void StoreInfo() override;
    void DrawToHdc(HDC hdc, int drawX, int drawY);
    bool GetDisplayDataForQt(DisplayData* outData) const;
    bool BuildQtMinimapImage(QImage* outImage) const;

private:
    void EnsureCreated();
    void LayoutChildren();
    void LoadAssets();
    void ReleaseAssets();
    void DrawCloseButton(HDC hdc, int drawX, int drawY);
    void EnsureRenderCache();
    void ReleaseRenderCache();
    void InvalidateRenderCache();
    void DrawWindowContents(HDC hdc, int baseX, int baseY);
    void UpdateMinimapBitmap();
    std::string GetCurrentMinimapBitmapName() const;
    unsigned long long BuildVisualStateToken() const;

    bool m_controlsCreated;
    UIBitmapButton* m_closeButton;
    HBITMAP m_titleBarBitmap;
    HBITMAP m_bodyBitmap;
    HBITMAP m_mapBitmap;
    HDC m_renderCacheDC;
    HBITMAP m_renderCacheBitmap;
    HBITMAP m_renderCacheOldBitmap;
    void* m_renderCacheBits;
    int m_renderCacheWidth;
    int m_renderCacheHeight;
    bool m_renderCacheDirty;
    int m_mapBitmapWidth;
    int m_mapBitmapHeight;
    std::vector<u32> m_mapPixels;
    std::string m_loadedBitmapName;
    std::string m_loadedBitmapPath;
    unsigned long long m_lastVisualStateToken;
    bool m_hasVisualStateToken;
    int m_lastPlayerX;
    int m_lastPlayerY;
    int m_lastPlayerDir;
    u32 m_lastDynamicInvalidateTick;
};
