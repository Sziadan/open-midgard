#include "UIMinimapWnd.h"

#include "UIWindowMgr.h"
#include "core/File.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "render/DC.h"
#include "res/Bitmap.h"
#include "session/Session.h"
#include "world/World.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#endif

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
constexpr int kFallbackCloseButtonSize = 11;
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

shopui::BitmapPixels LoadBitmapPixelsFromGameData(const std::string& path)
{
    return shopui::LoadBitmapPixelsFromGameData(path, true);
}

void DrawBitmapPixelsAlphaBlended(HDC target, const shopui::BitmapPixels& bitmap, const RECT& dst)
{
    if (!target || !bitmap.IsValid() || dst.right <= dst.left || dst.bottom <= dst.top) {
        return;
    }
    AlphaBlendArgbToHdc(target,
                        dst.left,
                        dst.top,
                        dst.right - dst.left,
                        dst.bottom - dst.top,
                        bitmap.pixels.data(),
                        bitmap.width,
                        bitmap.height);
}

void DrawBgraPixelsStretched(HDC target,
    const u32* pixels,
    int bitmapWidth,
    int bitmapHeight,
    const RECT& dst,
    const RECT* srcRect = nullptr)
{
    if (!target || !pixels || bitmapWidth <= 0 || bitmapHeight <= 0 || dst.right <= dst.left || dst.bottom <= dst.top) {
        return;
    }
    RECT src{ 0, 0, bitmapWidth, bitmapHeight };
    if (srcRect) {
        src = *srcRect;
    }
    if (src.left < 0 || src.top < 0 || src.right > bitmapWidth || src.bottom > bitmapHeight || src.right <= src.left || src.bottom <= src.top) {
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bitmapWidth;
    bmi.bmiHeader.biHeight = -bitmapHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    const int oldStretchMode = SetStretchBltMode(target, COLORONCOLOR);
    StretchDIBits(target,
        dst.left,
        dst.top,
        dst.right - dst.left,
        dst.bottom - dst.top,
        src.left,
        src.top,
        src.right - src.left,
        src.bottom - src.top,
        pixels,
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY);
    SetStretchBltMode(target, oldStretchMode);
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

#if RO_ENABLE_QT6_UI
QFont BuildMinimapFontFromHdc(HDC hdc)
{
    LOGFONTA logFont{};
    if (hdc) {
        if (HGDIOBJ fontObject = GetCurrentObject(hdc, OBJ_FONT)) {
            GetObjectA(fontObject, sizeof(logFont), &logFont);
        }
    }

    const QString family = logFont.lfFaceName[0] != '\0'
        ? QString::fromLocal8Bit(logFont.lfFaceName)
        : QStringLiteral("MS Sans Serif");
    QFont font(family);
    font.setPixelSize(logFont.lfHeight != 0 ? (std::max)(1, std::abs(logFont.lfHeight)) : 13);
    font.setBold(logFont.lfWeight >= FW_BOLD);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

Qt::Alignment ToQtTextAlignment(UINT format)
{
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignTop;
    if (format & DT_CENTER) {
        alignment &= ~Qt::AlignLeft;
        alignment |= Qt::AlignHCenter;
    } else if (format & DT_RIGHT) {
        alignment &= ~Qt::AlignLeft;
        alignment |= Qt::AlignRight;
    }

    if (format & DT_VCENTER) {
        alignment &= ~Qt::AlignTop;
        alignment |= Qt::AlignVCenter;
    } else if (format & DT_BOTTOM) {
        alignment &= ~Qt::AlignTop;
        alignment |= Qt::AlignBottom;
    }

    return alignment;
}

void DrawWindowTextQt(HDC hdc, const RECT& rect, const char* text, COLORREF color, UINT format)
{
    if (!hdc || !text || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    const QString label = QString::fromLocal8Bit(text);
    if (label.isEmpty()) {
        return;
    }

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(BuildMinimapFontFromHdc(hdc));
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(QRect(0, 0, width, height), ToQtTextAlignment(format) | Qt::TextSingleLine, label);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

void DrawWindowText(HDC hdc, int windowRight, int x, int y, const char* text, COLORREF color, UINT format, int height = 16)
{
    RECT rect = { x, y, windowRight - 4, y + height };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    HGDIOBJ oldFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
#if RO_ENABLE_QT6_UI
    DrawWindowTextQt(hdc, rect, text ? text : "", color, format);
#else
    DrawTextA(hdc, text ? text : "", -1, &rect, format);
#endif
    SelectObject(hdc, oldFont);
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

#if RO_ENABLE_QT6_UI
QImage BitmapPixelsToQImage(const shopui::BitmapPixels& bitmap)
{
    if (!bitmap.IsValid()) {
        return QImage();
    }

    QImage image(
        reinterpret_cast<const uchar*>(bitmap.pixels.data()),
        bitmap.width,
        bitmap.height,
        bitmap.width * static_cast<int>(sizeof(unsigned int)),
        QImage::Format_ARGB32);
    return image.copy();
}
#endif

bool BlitArgbCacheToHdc(HDC target, int x, int y, int width, int height, const void* bits)
{
    if (!target || !bits || width <= 0 || height <= 0) {
        return false;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    return StretchDIBits(
               target,
               x,
               y,
               width,
               height,
               0,
               0,
               width,
               height,
               bits,
               &bmi,
               DIB_RGB_COLORS,
               SRCCOPY) != GDI_ERROR;
}

} // namespace

UIRoMapWnd::UIRoMapWnd()
    : m_controlsCreated(false),
      m_closeButton(nullptr),
      m_titleBarBitmap(),
      m_bodyBitmap(),
      m_renderCacheSurface(),
      m_renderCacheDirty(true),
      m_mapBitmapWidth(0),
      m_mapBitmapHeight(0),
      m_lastVisualStateToken(0ull),
      m_hasVisualStateToken(false),
      m_lastPlayerX(std::numeric_limits<int>::min()),
      m_lastPlayerY(std::numeric_limits<int>::min()),
      m_lastPlayerDir(std::numeric_limits<int>::min()),
      m_lastDynamicInvalidateTick(0),
      m_qtCloseButtonPressed(false)
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
        InvalidateRenderCache();
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

msgresult_t UIRoMapWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
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

    if (IsQtUiRuntimeEnabled()) {
        LayoutChildren();
        UpdateMinimapBitmap();
        return;
    }

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

void UIRoMapWnd::OnLBtnDown(int x, int y)
{
    const RECT closeRect = GetCloseButtonRect();
    const bool insideClose = x >= closeRect.left && x < closeRect.right
        && y >= closeRect.top && y < closeRect.bottom;
    if (IsQtUiRuntimeEnabled() && insideClose) {
        m_qtCloseButtonPressed = true;
        return;
    }

    m_qtCloseButtonPressed = false;
    UIFrameWnd::OnLBtnDown(x, y);
}

void UIRoMapWnd::OnLBtnDblClk(int x, int y)
{
    OnLBtnDown(x, y);
}

void UIRoMapWnd::OnLBtnUp(int x, int y)
{
    if (m_qtCloseButtonPressed) {
        const RECT closeRect = GetCloseButtonRect();
        const bool shouldClose = x >= closeRect.left && x < closeRect.right
            && y >= closeRect.top && y < closeRect.bottom;
        m_qtCloseButtonPressed = false;
        if (shouldClose) {
            SetShow(0);
            StoreInfo();
        }
        return;
    }

    UIFrameWnd::OnLBtnUp(x, y);
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
    InvalidateRenderCache();
    Invalidate();
}

void UIRoMapWnd::DrawWindowContents(HDC hdc, int baseX, int baseY)
{
    if (!hdc) {
        return;
    }

    RECT windowRect{ baseX, baseY, baseX + m_w, baseY + m_h };
    RECT titleRect{ baseX, baseY, baseX + m_w, baseY + kTitleBarHeight };
    RECT bodyRect{ baseX, baseY + kTitleBarHeight, baseX + m_w, baseY + m_h };
    RECT mapRect{ baseX + kBodyInset, baseY + kMapTop, baseX + kBodyInset + kMapSize, baseY + kMapTop + kMapSize };
    RECT coordsRect{ baseX + 8, baseY + kCoordsTop, baseX + m_w - 8, baseY + kCoordsTop + kCoordsHeight };
    const int savedDc = SaveDC(hdc);
    HRGN clipRgn = CreateRoundRectRgn(
        windowRect.left,
        windowRect.top,
        windowRect.right + 1,
        windowRect.bottom + 1,
        kWindowCornerRadius,
        kWindowCornerRadius);
    if (clipRgn) {
        SelectClipRgn(hdc, clipRgn);
    }

    if (m_bodyBitmap.IsValid()) {
        DrawBitmapPixelsAlphaBlended(hdc, m_bodyBitmap, bodyRect);
    } else {
        FillSolidRect(hdc, bodyRect, RGB(209, 216, 228));
    }

    if (m_titleBarBitmap.IsValid()) {
        DrawBitmapPixelsAlphaBlended(hdc, m_titleBarBitmap, titleRect);
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

    if (m_mapBitmapWidth > 0 && m_mapBitmapHeight > 0) {
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
            DrawBgraPixelsStretched(hdc, m_mapPixels.data(), m_mapBitmapWidth, m_mapBitmapHeight, mapRect, &srcRect);
        } else {
            drawRect = FitRectPreservingAspect(mapRect, m_mapBitmapWidth, m_mapBitmapHeight);
            DrawBgraPixelsStretched(hdc, m_mapPixels.data(), m_mapBitmapWidth, m_mapBitmapHeight, drawRect);
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

    DrawWindowText(hdc, baseX + m_w, baseX + 18, baseY + 3, "Mini Map", RGB(255, 255, 255), DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawWindowText(hdc, baseX + m_w, baseX + 17, baseY + 2, "Mini Map", RGB(0, 0, 0), DT_LEFT | DT_TOP | DT_SINGLELINE);

    std::string mapName = StripExtension(GetCurrentMinimapBitmapName());

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
        DrawSmallMarker(hdc, playerPoint, RGB(245, 224, 126), RGB(0, 0, 0), 4);
    }

    char coordsText[64] = {};
    std::snprintf(coordsText, sizeof(coordsText), "X : %d    Y : %d", g_session.m_playerPosX, g_session.m_playerPosY);
    if (!mapName.empty()) {
        DrawWindowText(hdc, coordsRect.right, coordsRect.left, coordsRect.top, mapName.c_str(),
            RGB(0, 0, 0), DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    DrawWindowText(hdc, coordsRect.right, coordsRect.left + 78, coordsRect.top, coordsText,
        RGB(0, 0, 0), DT_RIGHT | DT_TOP | DT_SINGLELINE);

    RestoreDC(hdc, savedDc);
    if (clipRgn) {
        DeleteObject(clipRgn);
    }

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
}

void UIRoMapWnd::OnDraw()
{
    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    if (IsQtUiRuntimeEnabled()) {
        UpdateMinimapBitmap();
        m_lastVisualStateToken = BuildVisualStateToken();
        m_hasVisualStateToken = true;
        m_isDirty = 0;
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    DrawToHdc(hdc, m_x, m_y);
    ReleaseDrawTarget(hdc);
}

void UIRoMapWnd::DrawToHdc(HDC hdc, int drawX, int drawY)
{
    if (!hdc || m_show == 0) {
        return;
    }

    EnsureCreated();
    UpdateMinimapBitmap();

    const unsigned long long visualStateToken = BuildVisualStateToken();
    if (!m_hasVisualStateToken || visualStateToken != m_lastVisualStateToken) {
        InvalidateRenderCache();
    }

    EnsureRenderCache();
    if (m_renderCacheSurface.IsValid()) {
        if (m_renderCacheDirty) {
            DrawWindowContents(m_renderCacheSurface.GetDC(), 0, 0);
            m_renderCacheDirty = false;
        }
        if (!BlitArgbCacheToHdc(hdc, drawX, drawY, m_w, m_h, m_renderCacheSurface.GetBits())) {
            BitBlt(hdc, drawX, drawY, m_w, m_h, m_renderCacheSurface.GetDC(), 0, 0, SRCCOPY);
        }
    } else {
        DrawWindowContents(hdc, drawX, drawY);
    }

    DrawCloseButton(hdc, drawX, drawY);

    m_lastVisualStateToken = visualStateToken;
    m_hasVisualStateToken = true;
    m_isDirty = 0;
}

void UIRoMapWnd::OnWheel(int delta)
{
    const float direction = delta > 0 ? 1.0f : -1.0f;
    const float nextZoom = g_windowMgr.m_miniMapZoomFactor + direction * kZoomStep;
    g_windowMgr.m_miniMapZoomFactor = (std::max)(kMinZoom, (std::min)(kMaxZoom, nextZoom));
    InvalidateRenderCache();
    Invalidate();
}

void UIRoMapWnd::StoreInfo()
{
    SaveUiWindowPlacement("RoMapWnd", m_x, m_y);
}

bool UIRoMapWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData) {
        return false;
    }

    UIRoMapWnd* mutableThis = const_cast<UIRoMapWnd*>(this);
    mutableThis->EnsureCreated();
    mutableThis->UpdateMinimapBitmap();

    DisplayData data{};
    data.imageRevision = static_cast<int>(BuildVisualStateToken() & 0x7fffffff);

    const RECT mapRect{ m_x + kBodyInset, m_y + kMapTop, m_x + kBodyInset + kMapSize, m_y + kMapTop + kMapSize };
    data.mapX = mapRect.left;
    data.mapY = mapRect.top;
    data.mapWidth = mapRect.right - mapRect.left;
    data.mapHeight = mapRect.bottom - mapRect.top;

    const RECT coordsRect{ m_x + 8, m_y + kCoordsTop, m_x + m_w - 8, m_y + kCoordsTop + kCoordsHeight };
    data.coordsX = coordsRect.left;
    data.coordsY = coordsRect.top;
    data.coordsWidth = coordsRect.right - coordsRect.left;
    data.coordsHeight = coordsRect.bottom - coordsRect.top;

    const RECT closeRect = GetCloseButtonRect();
    data.closeX = closeRect.left;
    data.closeY = closeRect.top;
    data.closeWidth = closeRect.right - closeRect.left;
    data.closeHeight = closeRect.bottom - closeRect.top;
    data.closePressed = m_qtCloseButtonPressed;

    data.mapName = StripExtension(GetCurrentMinimapBitmapName());
    char coordsText[64] = {};
    std::snprintf(coordsText, sizeof(coordsText), "X : %d    Y : %d", g_session.m_playerPosX, g_session.m_playerPosY);
    data.coordsText = coordsText;

    RECT drawRect = mapRect;
    RECT srcRect{ 0, 0, m_mapBitmapWidth, m_mapBitmapHeight };
    bool zoomedView = false;

    const float zoom = (std::max)(kMinZoom, (std::min)(kMaxZoom, g_windowMgr.m_miniMapZoomFactor));
    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    RECT attrArea{ 0, 0, 0, 0 };
    if (gameMode && gameMode->m_world) {
        attrArea = gameMode->m_world->m_rootNode.m_attrArea;
    }

    if (m_mapBitmapWidth > 0 && m_mapBitmapHeight > 0) {
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
        } else {
            drawRect = FitRectPreservingAspect(mapRect, m_mapBitmapWidth, m_mapBitmapHeight);
        }
    }

    auto appendMarker = [&](int cellX, int cellY, COLORREF fillColor, int radius) {
        POINT pt{};
        if (!BuildMarkerPoint(attrArea, drawRect, zoomedView, srcRect, m_mapBitmapWidth, m_mapBitmapHeight, cellX, cellY, &pt)) {
            return;
        }
        DisplayMarker marker{};
        marker.x = pt.x;
        marker.y = pt.y;
        marker.radius = radius;
        marker.color = static_cast<unsigned int>(fillColor & 0x00FFFFFFu);
        data.markers.push_back(marker);
    };

    if (gameMode) {
        for (const auto& entry : gameMode->m_partyPosList) {
            appendMarker(entry.second.x, entry.second.y, entry.second.color & 0x00FFFFFFu, 2);
        }
        for (const auto& entry : gameMode->m_guildPosList) {
            appendMarker(entry.second.x, entry.second.y, entry.second.color & 0x00FFFFFFu, 2);
        }
        if (g_windowMgr.m_isDrawCompass != 0) {
            for (const auto& entry : gameMode->m_compassPosList) {
                appendMarker(entry.second.x, entry.second.y, entry.second.color & 0x00FFFFFFu, 3);
            }
        }
    }

    appendMarker(g_session.m_playerPosX, g_session.m_playerPosY, RGB(245, 224, 126), 4);

    *outData = std::move(data);
    return true;
}

bool UIRoMapWnd::BuildQtMinimapImage(QImage* outImage) const
{
#if !RO_ENABLE_QT6_UI
    (void)outImage;
    return false;
#else
    if (!outImage) {
        return false;
    }

    UIRoMapWnd* mutableThis = const_cast<UIRoMapWnd*>(this);
    mutableThis->EnsureCreated();
    mutableThis->UpdateMinimapBitmap();

    if (m_mapBitmapWidth <= 0 || m_mapBitmapHeight <= 0 || m_mapPixels.empty()) {
        return false;
    }

    const QImage sourceImage(
        reinterpret_cast<const uchar*>(m_mapPixels.data()),
        m_mapBitmapWidth,
        m_mapBitmapHeight,
        m_mapBitmapWidth * static_cast<int>(sizeof(u32)),
        QImage::Format_ARGB32);
    if (sourceImage.isNull()) {
        return false;
    }

    QImage composed(kMapSize, kMapSize, QImage::Format_ARGB32_Premultiplied);
    composed.fill(qRgba(18, 22, 29, 255));

    QRect targetRect(0, 0, kMapSize, kMapSize);
    QRect sourceRect(0, 0, sourceImage.width(), sourceImage.height());
    const float zoom = (std::max)(kMinZoom, (std::min)(kMaxZoom, g_windowMgr.m_miniMapZoomFactor));
    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    RECT attrArea{ 0, 0, 0, 0 };
    if (gameMode && gameMode->m_world) {
        attrArea = gameMode->m_world->m_rootNode.m_attrArea;
    }

    if (zoom > kMinZoom + 0.001f) {
        const float viewAspect = static_cast<float>(kMapSize) / static_cast<float>(kMapSize);
        const float bitmapAspect = static_cast<float>(sourceImage.width()) / static_cast<float>(sourceImage.height());
        int srcWidth = 0;
        int srcHeight = 0;
        if (bitmapAspect > viewAspect) {
            srcHeight = static_cast<int>(sourceImage.height() / zoom);
            srcWidth = static_cast<int>(srcHeight * viewAspect);
        } else {
            srcWidth = static_cast<int>(sourceImage.width() / zoom);
            srcHeight = static_cast<int>(srcWidth / viewAspect);
        }

        srcWidth = (std::max)(1, (std::min)(srcWidth, sourceImage.width()));
        srcHeight = (std::max)(1, (std::min)(srcHeight, sourceImage.height()));

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

        const int centerX = static_cast<int>(centerNormX * sourceImage.width());
        const int centerY = static_cast<int>(centerNormY * sourceImage.height());
        int srcLeft = centerX - srcWidth / 2;
        int srcTop = centerY - srcHeight / 2;
        srcLeft = (std::max)(0, (std::min)(srcLeft, sourceImage.width() - srcWidth));
        srcTop = (std::max)(0, (std::min)(srcTop, sourceImage.height() - srcHeight));
        sourceRect = QRect(srcLeft, srcTop, srcWidth, srcHeight);
    } else {
        RECT fitted = FitRectPreservingAspect(RECT{ 0, 0, kMapSize, kMapSize }, sourceImage.width(), sourceImage.height());
        targetRect = QRect(fitted.left, fitted.top, fitted.right - fitted.left, fitted.bottom - fitted.top);
    }

    {
        QPainter painter(&composed);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(targetRect, sourceImage, sourceRect);
    }

    *outImage = composed;
    return true;
#endif
}

bool UIRoMapWnd::BuildOverlayImageForRenderer(QImage* outImage) const
{
#if !RO_ENABLE_QT6_UI
    (void)outImage;
    return false;
#else
    if (!outImage) {
        return false;
    }

    UIRoMapWnd* mutableThis = const_cast<UIRoMapWnd*>(this);
    mutableThis->EnsureCreated();
    mutableThis->UpdateMinimapBitmap();
    mutableThis->LayoutChildren();

    QImage minimapImage;
    if (!BuildQtMinimapImage(&minimapImage)) {
        return false;
    }

    QImage composed(m_w, m_h, QImage::Format_ARGB32);
    composed.fill(Qt::transparent);

    QPainter painter(&composed);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);

    const QRect windowRect(0, 0, m_w, m_h);
    const QRect titleRect(0, 0, m_w, kTitleBarHeight);
    const QRect bodyRect(0, kTitleBarHeight, m_w, m_h - kTitleBarHeight);
    const QRect mapRect(kBodyInset, kMapTop, kMapSize, kMapSize);
    const QRect coordsRect(8, kCoordsTop, m_w - 16, kCoordsHeight);

    QPainterPath clipPath;
    clipPath.addRoundedRect(QRectF(windowRect), static_cast<qreal>(kWindowCornerRadius), static_cast<qreal>(kWindowCornerRadius));
    painter.setClipPath(clipPath);

    const QImage bodyImage = BitmapPixelsToQImage(m_bodyBitmap);
    if (!bodyImage.isNull()) {
        painter.drawImage(bodyRect, bodyImage);
    } else {
        painter.fillRect(bodyRect, QColor(209, 216, 228));
    }

    const QImage titleImage = BitmapPixelsToQImage(m_titleBarBitmap);
    if (!titleImage.isNull()) {
        painter.drawImage(titleRect, titleImage);
    } else {
        painter.fillRect(titleRect, QColor(98, 114, 158));
    }

    painter.drawImage(mapRect, minimapImage);
    painter.setClipping(false);

    QPen borderPen(QColor(57, 66, 86));
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(0.5, 0.5, static_cast<qreal>(m_w - 1), static_cast<qreal>(m_h - 1)),
        static_cast<qreal>(kWindowCornerRadius),
        static_cast<qreal>(kWindowCornerRadius));
    painter.drawRect(QRectF(mapRect.x() + 0.5, mapRect.y() + 0.5, static_cast<qreal>(mapRect.width() - 1), static_cast<qreal>(mapRect.height() - 1)));

    QFont font(QStringLiteral("MS Sans Serif"));
    font.setPixelSize(11);
    font.setStyleStrategy(QFont::NoAntialias);
    painter.setFont(font);

    painter.setPen(Qt::black);
    painter.drawText(QPoint(17, 11), QStringLiteral("Mini Map"));
    painter.setPen(Qt::white);
    painter.drawText(QPoint(18, 12), QStringLiteral("Mini Map"));

    painter.setPen(Qt::black);
    painter.drawText(coordsRect.adjusted(0, 0, -78, 0), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
        QString::fromStdString(StripExtension(GetCurrentMinimapBitmapName())));
    const QString coordsText = QStringLiteral("X : %1    Y : %2").arg(g_session.m_playerPosX).arg(g_session.m_playerPosY);
    painter.drawText(coordsRect, Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, coordsText);

    if (m_closeButton && m_closeButton->m_show != 0) {
        const shopui::BitmapPixels* drawBmp = nullptr;
        if (m_closeButton->m_state == 1 && m_closeButton->m_pressedBitmap.IsValid()) {
            drawBmp = &m_closeButton->m_pressedBitmap;
        } else if (m_closeButton->m_state == 2 && m_closeButton->m_mouseonBitmap.IsValid()) {
            drawBmp = &m_closeButton->m_mouseonBitmap;
        } else if (m_closeButton->m_normalBitmap.IsValid()) {
            drawBmp = &m_closeButton->m_normalBitmap;
        } else if (m_closeButton->m_mouseonBitmap.IsValid()) {
            drawBmp = &m_closeButton->m_mouseonBitmap;
        } else if (m_closeButton->m_pressedBitmap.IsValid()) {
            drawBmp = &m_closeButton->m_pressedBitmap;
        }

        if (drawBmp) {
            const QImage closeImage = BitmapPixelsToQImage(*drawBmp);
            if (!closeImage.isNull()) {
                const int buttonOffsetX = m_closeButton->m_x - m_x;
                const int buttonOffsetY = m_closeButton->m_y - m_y;
                painter.drawImage(QRect(buttonOffsetX, buttonOffsetY, drawBmp->width, drawBmp->height), closeImage);
            }
        }
    }

    painter.end();
    *outImage = composed;
    return true;
#endif
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
        const RECT closeRect = GetCloseButtonRect();
        m_closeButton->Move(closeRect.left, closeRect.top);
        m_closeButton->SetShow(1);
    }
}

RECT UIRoMapWnd::GetCloseButtonRect() const
{
    int width = kFallbackCloseButtonSize;
    int height = kFallbackCloseButtonSize;
    if (m_closeButton) {
        width = m_closeButton->m_bitmapWidth > 0 ? m_closeButton->m_bitmapWidth : width;
        height = m_closeButton->m_bitmapHeight > 0 ? m_closeButton->m_bitmapHeight : height;
    }

    RECT rc{ m_x + m_w - 19, m_y + 3, m_x + m_w - 19 + width, m_y + 3 + height };
    return rc;
}

void UIRoMapWnd::LoadAssets()
{
    if (!m_titleBarBitmap.IsValid()) {
        m_titleBarBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("titlebar_fix.bmp"));
    }
    if (!m_bodyBitmap.IsValid()) {
        m_bodyBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("itemwin_mid.bmp"));
    }
    InvalidateRenderCache();
}

void UIRoMapWnd::DrawCloseButton(HDC hdc, int drawX, int drawY)
{
    if (!hdc || !m_closeButton || m_closeButton->m_show == 0) {
        return;
    }

    const shopui::BitmapPixels* drawBmp = nullptr;
    if (m_closeButton->m_state == 1 && m_closeButton->m_pressedBitmap.IsValid()) {
        drawBmp = &m_closeButton->m_pressedBitmap;
    } else if (m_closeButton->m_state == 2 && m_closeButton->m_mouseonBitmap.IsValid()) {
        drawBmp = &m_closeButton->m_mouseonBitmap;
    } else if (m_closeButton->m_normalBitmap.IsValid()) {
        drawBmp = &m_closeButton->m_normalBitmap;
    } else if (m_closeButton->m_mouseonBitmap.IsValid()) {
        drawBmp = &m_closeButton->m_mouseonBitmap;
    } else {
        drawBmp = m_closeButton->m_pressedBitmap.IsValid() ? &m_closeButton->m_pressedBitmap : nullptr;
    }

    if (!drawBmp) {
        return;
    }

    const int buttonOffsetX = m_closeButton->m_x - m_x;
    const int buttonOffsetY = m_closeButton->m_y - m_y;
    RECT dst{
        drawX + buttonOffsetX,
        drawY + buttonOffsetY,
        drawX + buttonOffsetX + drawBmp->width,
        drawY + buttonOffsetY + drawBmp->height
    };
    shopui::DrawBitmapPixelsTransparent(hdc, *drawBmp, dst);
    m_closeButton->m_bitmapWidth = drawBmp->width;
    m_closeButton->m_bitmapHeight = drawBmp->height;
    m_closeButton->m_isDirty = 0;
}

void UIRoMapWnd::ReleaseAssets()
{
    ReleaseRenderCache();
    m_titleBarBitmap.Clear();
    m_bodyBitmap.Clear();
    m_mapBitmapWidth = 0;
    m_mapBitmapHeight = 0;
    m_mapPixels.clear();
    m_loadedBitmapName.clear();
    m_loadedBitmapPath.clear();
}

void UIRoMapWnd::EnsureRenderCache()
{
    if (m_w <= 0 || m_h <= 0) {
        return;
    }
    if (m_renderCacheSurface.IsValid()
        && m_renderCacheSurface.GetWidth() == m_w
        && m_renderCacheSurface.GetHeight() == m_h) {
        return;
    }

    ReleaseRenderCache();
    if (!m_renderCacheSurface.EnsureSize(m_w, m_h)) {
        return;
    }
    m_renderCacheDirty = true;
}

void UIRoMapWnd::ReleaseRenderCache()
{
    m_renderCacheSurface.Release();
    m_renderCacheDirty = true;
}

void UIRoMapWnd::InvalidateRenderCache()
{
    m_renderCacheDirty = true;
}

void UIRoMapWnd::UpdateMinimapBitmap()
{
    const std::string bitmapName = GetCurrentMinimapBitmapName();
    if (bitmapName.empty()) {
        m_mapPixels.clear();
        m_mapBitmapWidth = 0;
        m_mapBitmapHeight = 0;
        m_loadedBitmapName.clear();
        m_loadedBitmapPath.clear();
        InvalidateRenderCache();
        return;
    }

    const bool hasCachedMinimap = !m_mapPixels.empty() && m_mapBitmapWidth > 0 && m_mapBitmapHeight > 0;
    if (!m_loadedBitmapName.empty() && ToLowerAscii(m_loadedBitmapName) == ToLowerAscii(bitmapName) && hasCachedMinimap) {
        return;
    }

    const std::string resolvedPath = ResolveMinimapPath(bitmapName);
    m_mapPixels.clear();
    m_mapBitmapWidth = 0;
    m_mapBitmapHeight = 0;
    m_loadedBitmapName = bitmapName;
    m_loadedBitmapPath = resolvedPath;
    if (!resolvedPath.empty()) {
        u32* pixels = nullptr;
        if (LoadBgraPixelsFromGameData(resolvedPath.c_str(), &pixels, &m_mapBitmapWidth, &m_mapBitmapHeight) && pixels) {
            const size_t pixelCount = static_cast<size_t>(m_mapBitmapWidth) * static_cast<size_t>(m_mapBitmapHeight);
            m_mapPixels.assign(pixels, pixels + pixelCount);
        }
        delete[] pixels;
    }
    InvalidateRenderCache();
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
