#include "UIMinimapWnd.h"

#include "UIWindowMgr.h"
#include "core/File.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "session/Session.h"
#include "world/World.h"

#include <gdiplus.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")

namespace {

constexpr int kWindowWidth = 184;
constexpr int kWindowHeight = 212;
constexpr int kTitleBarHeight = 17;
constexpr int kBodyInset = 10;
constexpr int kMapTop = 20;
constexpr int kMapSize = 164;
constexpr int kCoordsTop = kMapTop + kMapSize + 4;
constexpr int kCoordsHeight = 16;
constexpr int kDefaultScreenMargin = 20;
constexpr int kButtonIdClose = 135;
constexpr int kWindowCornerRadius = 10;
constexpr u32 kDynamicRefreshIntervalMs = 100;
constexpr float kMinZoom = 1.0f;
constexpr float kMaxZoom = 4.0f;
constexpr float kZoomStep = 0.25f;

const char* UiKorPrefix()
{
    static const char* kUiKor =
        "texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\";
    return kUiKor;
}

ULONG_PTR EnsureGdiplusStarted()
{
    static ULONG_PTR s_token = 0;
    static bool s_started = false;
    if (!s_started) {
        Gdiplus::GdiplusStartupInput startupInput;
        if (Gdiplus::GdiplusStartup(&s_token, &startupInput, nullptr) == Gdiplus::Ok) {
            s_started = true;
        }
    }
    return s_token;
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - 'A' + 'a');
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string NormalizeSlash(std::string value)
{
    std::replace(value.begin(), value.end(), '/', '\\');
    return value;
}

void AddUniqueCandidate(std::vector<std::string>& out, const std::string& raw)
{
    if (raw.empty()) {
        return;
    }

    const std::string normalized = NormalizeSlash(raw);
    const std::string lowered = ToLowerAscii(normalized);
    for (const std::string& existing : out) {
        if (ToLowerAscii(existing) == lowered) {
            return;
        }
    }
    out.push_back(normalized);
}

std::vector<std::string> BuildUiAssetCandidates(const char* fileName)
{
    std::vector<std::string> out;
    if (!fileName || !*fileName) {
        return out;
    }

    const char* prefixes[] = {
        "",
        "skin\\default\\",
        "skin\\default\\basic_interface\\",
        "texture\\",
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "data\\",
        "data\\texture\\",
        "data\\texture\\interface\\",
        "data\\texture\\interface\\basic_interface\\",
        UiKorPrefix(),
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        nullptr
    };

    std::string base = NormalizeSlash(fileName);
    AddUniqueCandidate(out, base);

    std::string filenameOnly = base;
    const size_t slashPos = filenameOnly.find_last_of('\\');
    if (slashPos != std::string::npos && slashPos + 1 < filenameOnly.size()) {
        filenameOnly = filenameOnly.substr(slashPos + 1);
    }

    for (int index = 0; prefixes[index]; ++index) {
        AddUniqueCandidate(out, std::string(prefixes[index]) + filenameOnly);
    }

    return out;
}

std::string ResolveUiAssetPath(const char* fileName)
{
    for (const std::string& candidate : BuildUiAssetCandidates(fileName)) {
        if (g_fileMgr.IsDataExist(candidate.c_str())) {
            return candidate;
        }
    }
    return NormalizeSlash(fileName ? fileName : "");
}

std::vector<std::string> BuildMinimapCandidates(const std::string& bitmapName)
{
    std::vector<std::string> out;
    if (bitmapName.empty()) {
        return out;
    }

    const std::array<const char*, 16> prefixes = {
        "",
        "texture\\",
        "texture\\map\\",
        "texture\\minimap\\",
        "texture\\interface\\map\\",
        "texture\\interface\\minimap\\",
        "data\\",
        "data\\texture\\",
        "data\\texture\\map\\",
        "data\\texture\\minimap\\",
        "data\\texture\\interface\\map\\",
        "data\\texture\\interface\\minimap\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\map\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\minimap\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\map\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\minimap\\"
    };

    std::string normalized = NormalizeSlash(bitmapName);
    AddUniqueCandidate(out, normalized);

    std::string filenameOnly = normalized;
    const size_t slashPos = filenameOnly.find_last_of('\\');
    if (slashPos != std::string::npos && slashPos + 1 < filenameOnly.size()) {
        filenameOnly = filenameOnly.substr(slashPos + 1);
    }

    for (const char* prefix : prefixes) {
        AddUniqueCandidate(out, std::string(prefix) + filenameOnly);
    }

    return out;
}

std::string ResolveMinimapPath(const std::string& bitmapName)
{
    for (const std::string& candidate : BuildMinimapCandidates(bitmapName)) {
        if (g_fileMgr.IsDataExist(candidate.c_str())) {
            return candidate;
        }
    }
    return std::string();
}

HBITMAP LoadBitmapFromGameData(const std::string& path, int* outWidth = nullptr, int* outHeight = nullptr)
{
    if (outWidth) {
        *outWidth = 0;
    }
    if (outHeight) {
        *outHeight = 0;
    }

    if (path.empty() || !EnsureGdiplusStarted()) {
        return nullptr;
    }

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData(path.c_str(), &size);
    if (!bytes || size <= 0) {
        delete[] bytes;
        return nullptr;
    }

    HBITMAP outBitmap = nullptr;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(size));
    if (mem) {
        void* dst = GlobalLock(mem);
        if (dst) {
            std::memcpy(dst, bytes, static_cast<size_t>(size));
            GlobalUnlock(mem);

            IStream* stream = nullptr;
            if (CreateStreamOnHGlobal(mem, TRUE, &stream) == S_OK) {
                auto* bitmap = Gdiplus::Bitmap::FromStream(stream, FALSE);
                if (bitmap && bitmap->GetLastStatus() == Gdiplus::Ok) {
                    bitmap->GetHBITMAP(RGB(0, 0, 0), &outBitmap);
                }
                delete bitmap;
                stream->Release();
            } else {
                GlobalFree(mem);
            }
        } else {
            GlobalFree(mem);
        }
    }

    delete[] bytes;

    if (outBitmap) {
        BITMAP bm{};
        if (GetObjectA(outBitmap, sizeof(bm), &bm)) {
            if (outWidth) {
                *outWidth = bm.bmWidth;
            }
            if (outHeight) {
                *outHeight = bm.bmHeight;
            }
        }
    }

    return outBitmap;
}

void DrawBitmapTransparent(HDC target, HBITMAP bitmap, const RECT& dst)
{
    if (!target || !bitmap) {
        return;
    }

    BITMAP bm{};
    if (!GetObjectA(bitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        return;
    }

    HDC srcDC = CreateCompatibleDC(target);
    if (!srcDC) {
        return;
    }

    HGDIOBJ oldBitmap = SelectObject(srcDC, bitmap);
    TransparentBlt(target,
        dst.left,
        dst.top,
        dst.right - dst.left,
        dst.bottom - dst.top,
        srcDC,
        0,
        0,
        bm.bmWidth,
        bm.bmHeight,
        RGB(255, 0, 255));
    SelectObject(srcDC, oldBitmap);
    DeleteDC(srcDC);
}

void DrawBitmapStretched(HDC target, HBITMAP bitmap, const RECT& dst, const RECT* srcRect = nullptr)
{
    if (!target || !bitmap) {
        return;
    }

    BITMAP bm{};
    if (!GetObjectA(bitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        return;
    }

    RECT src{ 0, 0, bm.bmWidth, bm.bmHeight };
    if (srcRect) {
        src = *srcRect;
    }

    HDC srcDC = CreateCompatibleDC(target);
    if (!srcDC) {
        return;
    }

    HGDIOBJ oldBitmap = SelectObject(srcDC, bitmap);
    SetStretchBltMode(target, HALFTONE);
    StretchBlt(target,
        dst.left,
        dst.top,
        dst.right - dst.left,
        dst.bottom - dst.top,
        srcDC,
        src.left,
        src.top,
        src.right - src.left,
        src.bottom - src.top,
        SRCCOPY);
    SelectObject(srcDC, oldBitmap);
    DeleteDC(srcDC);
}

RECT FitRectPreservingAspect(const RECT& dst, int srcWidth, int srcHeight)
{
    RECT fitted = dst;
    if (srcWidth <= 0 || srcHeight <= 0 || dst.right <= dst.left || dst.bottom <= dst.top) {
        return fitted;
    }

    const float srcAspect = static_cast<float>(srcWidth) / static_cast<float>(srcHeight);
    const float dstAspect = static_cast<float>(dst.right - dst.left) / static_cast<float>(dst.bottom - dst.top);
    if (srcAspect > dstAspect) {
        const int fittedHeight = static_cast<int>((dst.right - dst.left) / srcAspect);
        const int pad = ((dst.bottom - dst.top) - fittedHeight) / 2;
        fitted.top += pad;
        fitted.bottom = fitted.top + fittedHeight;
    } else {
        const int fittedWidth = static_cast<int>((dst.bottom - dst.top) * srcAspect);
        const int pad = ((dst.right - dst.left) - fittedWidth) / 2;
        fitted.left += pad;
        fitted.right = fitted.left + fittedWidth;
    }
    return fitted;
}

bool BuildMarkerPoint(const RECT& attrArea,
    const RECT& drawRect,
    bool zoomedView,
    const RECT& srcRect,
    int bitmapWidth,
    int bitmapHeight,
    int cellX,
    int cellY,
    POINT* outPoint)
{
    if (!outPoint) {
        return false;
    }

    const int attrWidth = attrArea.right - attrArea.left;
    const int attrHeight = attrArea.bottom - attrArea.top;
    if (attrWidth <= 0 || attrHeight <= 0 || drawRect.right <= drawRect.left || drawRect.bottom <= drawRect.top) {
        return false;
    }

    const float normX = static_cast<float>(cellX - attrArea.left) / static_cast<float>(attrWidth);
    const float normY = 1.0f - (static_cast<float>(cellY - attrArea.top) / static_cast<float>(attrHeight));
    const float clampedNormX = (std::max)(0.0f, (std::min)(1.0f, normX));
    const float clampedNormY = (std::max)(0.0f, (std::min)(1.0f, normY));

    if (!zoomedView || bitmapWidth <= 0 || bitmapHeight <= 0) {
        outPoint->x = drawRect.left + static_cast<int>((drawRect.right - drawRect.left) * clampedNormX);
        outPoint->y = drawRect.top + static_cast<int>((drawRect.bottom - drawRect.top) * clampedNormY);
        return true;
    }

    const float bitmapX = clampedNormX * static_cast<float>(bitmapWidth);
    const float bitmapY = clampedNormY * static_cast<float>(bitmapHeight);
    if (bitmapX < static_cast<float>(srcRect.left) || bitmapX > static_cast<float>(srcRect.right)
        || bitmapY < static_cast<float>(srcRect.top) || bitmapY > static_cast<float>(srcRect.bottom)) {
        return false;
    }

    const int srcWidth = (std::max)(1, static_cast<int>(srcRect.right - srcRect.left));
    const int srcHeight = (std::max)(1, static_cast<int>(srcRect.bottom - srcRect.top));
    const float relX = (bitmapX - static_cast<float>(srcRect.left)) / static_cast<float>(srcWidth);
    const float relY = (bitmapY - static_cast<float>(srcRect.top)) / static_cast<float>(srcHeight);
    outPoint->x = drawRect.left + static_cast<int>((drawRect.right - drawRect.left) * relX);
    outPoint->y = drawRect.top + static_cast<int>((drawRect.bottom - drawRect.top) * relY);
    return true;
}

void FillSolidRect(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void FrameSolidRect(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void DrawWindowText(HDC hdc, int windowRight, int x, int y, const char* text, COLORREF color, UINT format, int height = 16)
{
    RECT rect = { x, y, windowRight - 4, y + height };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    HGDIOBJ oldFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
    DrawTextA(hdc, text ? text : "", -1, &rect, format);
    SelectObject(hdc, oldFont);
}

HBITMAP CreateMinimapTitleBitmap()
{
    constexpr int kBitmapWidth = 80;
    constexpr int kBitmapHeight = 16;

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) {
        return nullptr;
    }

    HDC memDc = CreateCompatibleDC(screenDc);
    if (!memDc) {
        ReleaseDC(nullptr, screenDc);
        return nullptr;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = kBitmapWidth;
    bmi.bmiHeader.biHeight = -kBitmapHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        return nullptr;
    }

    HGDIOBJ oldBitmap = SelectObject(memDc, bitmap);
    RECT rect{ 0, 0, kBitmapWidth, kBitmapHeight };
    HBRUSH bgBrush = CreateSolidBrush(RGB(255, 0, 255));
    FillRect(memDc, &rect, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(memDc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(memDc, GetStockObject(DEFAULT_GUI_FONT));
    SetTextColor(memDc, RGB(0, 0, 0));
    RECT shadowRect{ 0, 0, kBitmapWidth, kBitmapHeight };
    DrawTextA(memDc, "Mini Map", -1, &shadowRect, DT_LEFT | DT_TOP | DT_SINGLELINE);
    SetTextColor(memDc, RGB(255, 255, 255));
    RECT frontRect{ 1, 1, kBitmapWidth, kBitmapHeight };
    DrawTextA(memDc, "Mini Map", -1, &frontRect, DT_LEFT | DT_TOP | DT_SINGLELINE);

    SelectObject(memDc, oldFont);
    SelectObject(memDc, oldBitmap);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
    return bitmap;
}

void DrawSmallMarker(HDC hdc, const POINT& pt, COLORREF fillColor, COLORREF outlineColor, int radius)
{
    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = CreatePen(PS_SOLID, 1, outlineColor);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    Ellipse(hdc, pt.x - radius, pt.y - radius, pt.x + radius + 1, pt.y + radius + 1);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

std::string StripExtension(const std::string& value)
{
    std::string out = value;
    const size_t slashPos = out.find_last_of("\\/");
    if (slashPos != std::string::npos && slashPos + 1 < out.size()) {
        out = out.substr(slashPos + 1);
    }
    const size_t dotPos = out.find_last_of('.');
    if (dotPos != std::string::npos) {
        out.resize(dotPos);
    }
    return out;
}

} // namespace

UIRoMapWnd::UIRoMapWnd()
    : m_controlsCreated(false),
      m_closeButton(nullptr),
      m_titleBarBitmap(nullptr),
      m_titleTextBitmap(nullptr),
      m_bodyBitmap(nullptr),
      m_mapBitmap(nullptr),
      m_mapBitmapWidth(0),
      m_mapBitmapHeight(0),
      m_lastVisualStateToken(0ull),
      m_hasVisualStateToken(false),
      m_lastPlayerX(std::numeric_limits<int>::min()),
      m_lastPlayerY(std::numeric_limits<int>::min()),
      m_lastPlayerDir(std::numeric_limits<int>::min()),
      m_lastDynamicInvalidateTick(0)
{
    Create(kWindowWidth, kWindowHeight);

    RECT clientRect{ 0, 0, 640, 480 };
    if (g_hMainWnd) {
        GetClientRect(g_hMainWnd, &clientRect);
    }

    int defaultX = clientRect.right - kWindowWidth - kDefaultScreenMargin;
    int defaultY = clientRect.top + kDefaultScreenMargin;
    g_windowMgr.ClampWindowToClient(&defaultX, &defaultY, m_w, m_h);
    Move(defaultX, defaultY);

    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("RoMapWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
    }
}

UIRoMapWnd::~UIRoMapWnd()
{
    ReleaseAssets();
}

void UIRoMapWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show != 0) {
        EnsureCreated();
        LayoutChildren();
        UpdateMinimapBitmap();
    }
}

void UIRoMapWnd::Move(int x, int y)
{
    UIWindow::Move(x, y);
    if (m_controlsCreated) {
        LayoutChildren();
    }
}

bool UIRoMapWnd::IsUpdateNeed()
{
    if (m_show == 0) {
        return false;
    }
    if (m_isDirty != 0 || !m_hasVisualStateToken) {
        return true;
    }
    return BuildVisualStateToken() != m_lastVisualStateToken;
}

int UIRoMapWnd::SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra)
{
    (void)sender;
    (void)lparam;
    (void)extra;

    if (msg != 6) {
        return 0;
    }

    if (wparam == kButtonIdClose) {
        SetShow(0);
        StoreInfo();
        return 1;
    }
    return 1;
}

void UIRoMapWnd::OnCreate(int x, int y)
{
    (void)x;
    (void)y;
    if (m_controlsCreated) {
        return;
    }

    m_controlsCreated = true;
    LoadAssets();

    m_closeButton = new UIBitmapButton();
    m_closeButton->SetBitmapName(ResolveUiAssetPath("sys_close_off.bmp").c_str(), 0);
    m_closeButton->SetBitmapName(ResolveUiAssetPath("sys_close_on.bmp").c_str(), 1);
    m_closeButton->SetBitmapName(ResolveUiAssetPath("sys_close_on.bmp").c_str(), 2);
    m_closeButton->Create(m_closeButton->m_bitmapWidth, m_closeButton->m_bitmapHeight);
    m_closeButton->m_id = kButtonIdClose;
    m_closeButton->SetToolTip("Close");
    AddChild(m_closeButton);

    LayoutChildren();
    UpdateMinimapBitmap();
}

void UIRoMapWnd::OnProcess()
{
    UIFrameWnd::OnProcess();
    if (m_show == 0) {
        return;
    }

    const int playerX = g_session.m_playerPosX;
    const int playerY = g_session.m_playerPosY;
    const int playerDir = g_session.m_playerDir;
    if (playerX == m_lastPlayerX && playerY == m_lastPlayerY && playerDir == m_lastPlayerDir) {
        return;
    }

    const u32 now = GetTickCount();
    if (m_lastDynamicInvalidateTick != 0 && now - m_lastDynamicInvalidateTick < kDynamicRefreshIntervalMs) {
        return;
    }

    m_lastPlayerX = playerX;
    m_lastPlayerY = playerY;
    m_lastPlayerDir = playerDir;
    m_lastDynamicInvalidateTick = now;
    Invalidate();
}

void UIRoMapWnd::OnDraw()
{
    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    EnsureCreated();
    UpdateMinimapBitmap();

    const bool useShared = (UIWindow::GetSharedDrawDC() != nullptr);
    HDC hdc = useShared ? UIWindow::GetSharedDrawDC() : GetDC(g_hMainWnd);
    if (!hdc) {
        return;
    }

    RECT windowRect{ m_x, m_y, m_x + m_w, m_y + m_h };
    RECT titleRect{ m_x, m_y, m_x + m_w, m_y + kTitleBarHeight };
    RECT bodyRect{ m_x, m_y + kTitleBarHeight, m_x + m_w, m_y + m_h };
    RECT mapRect{ m_x + kBodyInset, m_y + kMapTop, m_x + kBodyInset + kMapSize, m_y + kMapTop + kMapSize };
    RECT coordsRect{ m_x + 8, m_y + kCoordsTop, m_x + m_w - 8, m_y + kCoordsTop + kCoordsHeight };

    if (m_bodyBitmap) {
        DrawBitmapTransparent(hdc, m_bodyBitmap, bodyRect);
    } else {
        FillSolidRect(hdc, bodyRect, RGB(209, 216, 228));
    }

    if (m_titleBarBitmap) {
        DrawBitmapTransparent(hdc, m_titleBarBitmap, titleRect);
    } else {
        FillSolidRect(hdc, titleRect, RGB(98, 114, 158));
    }

    FillSolidRect(hdc, mapRect, RGB(18, 22, 29));

    RECT drawRect = mapRect;
    RECT srcRect{ 0, 0, m_mapBitmapWidth, m_mapBitmapHeight };
    bool zoomedView = false;

    const float zoom = (std::max)(kMinZoom, (std::min)(kMaxZoom, g_windowMgr.m_miniMapZoomFactor));
    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    RECT attrArea{ 0, 0, 0, 0 };
    if (gameMode && gameMode->m_world) {
        attrArea = gameMode->m_world->m_rootNode.m_attrArea;
    }

    if (m_mapBitmap && m_mapBitmapWidth > 0 && m_mapBitmapHeight > 0) {
        if (zoom > kMinZoom + 0.001f) {
            zoomedView = true;
            const float viewAspect = static_cast<float>(mapRect.right - mapRect.left) / static_cast<float>(mapRect.bottom - mapRect.top);
            const float bitmapAspect = static_cast<float>(m_mapBitmapWidth) / static_cast<float>(m_mapBitmapHeight);

            int srcWidth = 0;
            int srcHeight = 0;
            if (bitmapAspect > viewAspect) {
                srcHeight = static_cast<int>(m_mapBitmapHeight / zoom);
                srcWidth = static_cast<int>(srcHeight * viewAspect);
            } else {
                srcWidth = static_cast<int>(m_mapBitmapWidth / zoom);
                srcHeight = static_cast<int>(srcWidth / viewAspect);
            }

            srcWidth = (std::max)(1, (std::min)(srcWidth, m_mapBitmapWidth));
            srcHeight = (std::max)(1, (std::min)(srcHeight, m_mapBitmapHeight));

            float centerNormX = 0.5f;
            float centerNormY = 0.5f;
            const int attrWidth = attrArea.right - attrArea.left;
            const int attrHeight = attrArea.bottom - attrArea.top;
            if (attrWidth > 0 && attrHeight > 0) {
                centerNormX = static_cast<float>(g_session.m_playerPosX - attrArea.left) / static_cast<float>(attrWidth);
                centerNormY = 1.0f - (static_cast<float>(g_session.m_playerPosY - attrArea.top) / static_cast<float>(attrHeight));
                centerNormX = (std::max)(0.0f, (std::min)(1.0f, centerNormX));
                centerNormY = (std::max)(0.0f, (std::min)(1.0f, centerNormY));
            }

            const int centerX = static_cast<int>(centerNormX * m_mapBitmapWidth);
            const int centerY = static_cast<int>(centerNormY * m_mapBitmapHeight);
            srcRect.left = centerX - srcWidth / 2;
            srcRect.top = centerY - srcHeight / 2;
            srcRect.left = (std::max)(0, (std::min)(static_cast<int>(srcRect.left), m_mapBitmapWidth - srcWidth));
            srcRect.top = (std::max)(0, (std::min)(static_cast<int>(srcRect.top), m_mapBitmapHeight - srcHeight));
            srcRect.right = srcRect.left + srcWidth;
            srcRect.bottom = srcRect.top + srcHeight;
            DrawBitmapStretched(hdc, m_mapBitmap, mapRect, &srcRect);
        } else {
            drawRect = FitRectPreservingAspect(mapRect, m_mapBitmapWidth, m_mapBitmapHeight);
            DrawBitmapStretched(hdc, m_mapBitmap, drawRect);
        }
    } else {
        FillSolidRect(hdc, mapRect, RGB(62, 88, 52));
        HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(118, 150, 96));
        HGDIOBJ oldPen = SelectObject(hdc, gridPen);
        for (int i = 1; i < 12; ++i) {
            const int xLine = mapRect.left + ((mapRect.right - mapRect.left) * i) / 12;
            MoveToEx(hdc, xLine, mapRect.top, nullptr);
            LineTo(hdc, xLine, mapRect.bottom);
        }
        for (int i = 1; i < 8; ++i) {
            const int yLine = mapRect.top + ((mapRect.bottom - mapRect.top) * i) / 8;
            MoveToEx(hdc, mapRect.left, yLine, nullptr);
            LineTo(hdc, mapRect.right, yLine);
        }
        SelectObject(hdc, oldPen);
        DeleteObject(gridPen);
    }

    FrameSolidRect(hdc, mapRect, RGB(72, 80, 96));

    if (m_titleTextBitmap) {
        RECT titleTextRect{ m_x + 17, m_y + 2, m_x + 17 + 80, m_y + 18 };
        DrawBitmapTransparent(hdc, m_titleTextBitmap, titleTextRect);
    } else {
        RECT titleTextRectBack{ m_x + 17, m_y + 2, m_x + m_w - 24, m_y + kTitleBarHeight - 1 };
        RECT titleTextRectFront{ m_x + 18, m_y + 3, m_x + m_w - 23, m_y + kTitleBarHeight };
        DrawWindowText(hdc, m_x + m_w, titleTextRectBack.left, titleTextRectBack.top, "Mini Map", RGB(0, 0, 0), DT_LEFT | DT_TOP | DT_SINGLELINE);
        DrawWindowText(hdc, m_x + m_w, titleTextRectFront.left, titleTextRectFront.top, "Mini Map", RGB(255, 255, 255), DT_LEFT | DT_TOP | DT_SINGLELINE);
    }

    std::string mapName = StripExtension(GetCurrentMinimapBitmapName());
    if (!mapName.empty()) {
        RECT mapNameRect{ mapRect.left + 3, mapRect.top + 2, mapRect.right - 3, mapRect.top + 16 };
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextA(hdc, mapName.c_str(), -1, &mapNameRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    auto drawWorldPoint = [&](int cellX, int cellY, COLORREF fillColor, COLORREF outlineColor, int radius) {
        POINT pt{};
        if (BuildMarkerPoint(attrArea, drawRect, zoomedView, srcRect, m_mapBitmapWidth, m_mapBitmapHeight, cellX, cellY, &pt)) {
            DrawSmallMarker(hdc, pt, fillColor, outlineColor, radius);
        }
    };

    if (gameMode) {
        for (const auto& entry : gameMode->m_partyPosList) {
            drawWorldPoint(entry.second.x, entry.second.y, entry.second.color & 0x00FFFFFFu, RGB(24, 24, 24), 2);
        }
        for (const auto& entry : gameMode->m_guildPosList) {
            drawWorldPoint(entry.second.x, entry.second.y, entry.second.color & 0x00FFFFFFu, RGB(24, 24, 24), 2);
        }
        if (g_windowMgr.m_isDrawCompass != 0) {
            for (const auto& entry : gameMode->m_compassPosList) {
                drawWorldPoint(entry.second.x, entry.second.y, entry.second.color & 0x00FFFFFFu, RGB(24, 24, 24), 3);
            }
        }
    }

    POINT playerPoint{};
    if (BuildMarkerPoint(attrArea, drawRect, zoomedView, srcRect, m_mapBitmapWidth, m_mapBitmapHeight,
            g_session.m_playerPosX, g_session.m_playerPosY, &playerPoint)) {
        DrawSmallMarker(hdc, playerPoint, RGB(245, 224, 126), RGB(92, 60, 16), 4);

        int dirX = 0;
        int dirY = -12;
        switch (g_session.m_playerDir & 7) {
        case 0: dirX = 0; dirY = -12; break;
        case 1: dirX = 8; dirY = -8; break;
        case 2: dirX = 12; dirY = 0; break;
        case 3: dirX = 8; dirY = 8; break;
        case 4: dirX = 0; dirY = 12; break;
        case 5: dirX = -8; dirY = 8; break;
        case 6: dirX = -12; dirY = 0; break;
        case 7: dirX = -8; dirY = -8; break;
        }

        HPEN dirPen = CreatePen(PS_SOLID, 2, RGB(92, 60, 16));
        HGDIOBJ oldPen = SelectObject(hdc, dirPen);
        MoveToEx(hdc, playerPoint.x, playerPoint.y, nullptr);
        LineTo(hdc, playerPoint.x + dirX, playerPoint.y + dirY);
        SelectObject(hdc, oldPen);
        DeleteObject(dirPen);
    }

    char coordsText[64] = {};
    std::snprintf(coordsText, sizeof(coordsText), "X : %d    Y : %d", g_session.m_playerPosX, g_session.m_playerPosY);
    DrawWindowText(hdc, m_x + m_w, coordsRect.left, coordsRect.top, coordsText, RGB(0, 0, 0), DT_CENTER | DT_TOP | DT_SINGLELINE);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(57, 66, 86));
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc,
        windowRect.left,
        windowRect.top,
        windowRect.right,
        windowRect.bottom,
        kWindowCornerRadius,
        kWindowCornerRadius);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    DrawChildren();
    if (!useShared) {
        ReleaseDC(g_hMainWnd, hdc);
    }

    m_lastVisualStateToken = BuildVisualStateToken();
    m_hasVisualStateToken = true;
    m_isDirty = 0;
}

void UIRoMapWnd::OnWheel(int delta)
{
    const float direction = delta > 0 ? 1.0f : -1.0f;
    const float nextZoom = g_windowMgr.m_miniMapZoomFactor + direction * kZoomStep;
    g_windowMgr.m_miniMapZoomFactor = (std::max)(kMinZoom, (std::min)(kMaxZoom, nextZoom));
    Invalidate();
}

void UIRoMapWnd::StoreInfo()
{
    SaveUiWindowPlacement("RoMapWnd", m_x, m_y);
}

void UIRoMapWnd::EnsureCreated()
{
    if (!m_controlsCreated) {
        OnCreate(0, 0);
    }
}

void UIRoMapWnd::LayoutChildren()
{
    if (m_closeButton) {
        m_closeButton->Move(m_x + m_w - 19, m_y + 3);
        m_closeButton->SetShow(1);
    }
}

void UIRoMapWnd::LoadAssets()
{
    if (!m_titleBarBitmap) {
        m_titleBarBitmap = LoadBitmapFromGameData(ResolveUiAssetPath("titlebar_fix.bmp"));
    }
    if (!m_titleTextBitmap) {
        m_titleTextBitmap = CreateMinimapTitleBitmap();
    }
    if (!m_bodyBitmap) {
        m_bodyBitmap = LoadBitmapFromGameData(ResolveUiAssetPath("itemwin_mid.bmp"));
    }
}

void UIRoMapWnd::ReleaseAssets()
{
    HBITMAP* bitmaps[] = {
        &m_titleBarBitmap,
        &m_titleTextBitmap,
        &m_bodyBitmap,
        &m_mapBitmap,
    };
    for (HBITMAP* bitmap : bitmaps) {
        if (*bitmap) {
            DeleteObject(*bitmap);
            *bitmap = nullptr;
        }
    }
    m_mapBitmapWidth = 0;
    m_mapBitmapHeight = 0;
}

void UIRoMapWnd::UpdateMinimapBitmap()
{
    const std::string bitmapName = GetCurrentMinimapBitmapName();
    if (bitmapName.empty()) {
        if (m_mapBitmap) {
            DeleteObject(m_mapBitmap);
            m_mapBitmap = nullptr;
        }
        m_mapBitmapWidth = 0;
        m_mapBitmapHeight = 0;
        m_loadedBitmapName.clear();
        m_loadedBitmapPath.clear();
        return;
    }

    if (!m_loadedBitmapName.empty() && ToLowerAscii(m_loadedBitmapName) == ToLowerAscii(bitmapName) && m_mapBitmap) {
        return;
    }

    const std::string resolvedPath = ResolveMinimapPath(bitmapName);
    if (m_mapBitmap) {
        DeleteObject(m_mapBitmap);
        m_mapBitmap = nullptr;
    }
    m_mapBitmapWidth = 0;
    m_mapBitmapHeight = 0;
    m_loadedBitmapName = bitmapName;
    m_loadedBitmapPath = resolvedPath;
    if (!resolvedPath.empty()) {
        m_mapBitmap = LoadBitmapFromGameData(resolvedPath, &m_mapBitmapWidth, &m_mapBitmapHeight);
    }
    Invalidate();
}

std::string UIRoMapWnd::GetCurrentMinimapBitmapName() const
{
    if (const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        if (gameMode->m_minimapBmpName[0] != '\0') {
            return NormalizeSlash(gameMode->m_minimapBmpName);
        }
    }

    if (g_session.m_curMap[0] == '\0') {
        return std::string();
    }

    char buffer[64] = {};
    std::strncpy(buffer, g_session.m_curMap, sizeof(buffer) - 1);
    char* dot = std::strrchr(buffer, '.');
    if (dot) {
        *dot = '\0';
    }

    char bitmapName[72] = {};
    std::snprintf(bitmapName, sizeof(bitmapName), "%s.bmp", buffer);
    return bitmapName;
}

unsigned long long UIRoMapWnd::BuildVisualStateToken() const
{
    unsigned long long hash = 1469598103934665603ull;
    const auto mixValue = [&hash](unsigned long long value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    const auto mixString = [&mixValue](const std::string& value) {
        for (unsigned char ch : value) {
            mixValue(static_cast<unsigned long long>(ch));
        }
        mixValue(0xFFull);
    };

    mixValue(static_cast<unsigned long long>(m_show));
    mixValue(static_cast<unsigned long long>(m_x));
    mixValue(static_cast<unsigned long long>(m_y));
    mixValue(static_cast<unsigned long long>(m_w));
    mixValue(static_cast<unsigned long long>(m_h));
    mixValue(static_cast<unsigned long long>(g_windowMgr.m_isDrawCompass));
    mixValue(static_cast<unsigned long long>(g_windowMgr.m_miniMapZoomFactor * 100.0f));
    mixString(GetCurrentMinimapBitmapName());
    mixString(m_loadedBitmapPath);

    return hash;
}
