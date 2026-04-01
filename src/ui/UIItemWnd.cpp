#include "UIItemWnd.h"

#include "gamemode/GameMode.h"
#include "UIEquipWnd.h"
#include "UIShortCutWnd.h"
#include "UIWindowMgr.h"
#include "core/File.h"
#include "item/Item.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "res/Bitmap.h"
#include "render/DrawUtil.h"
#include "session/Session.h"
#include "ui/UIWindow.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <list>
#include <string>
#include <vector>

#pragma comment(lib, "msimg32.lib")

namespace {

constexpr int kWindowWidth = 280;
constexpr int kWindowHeight = 134;
constexpr int kMiniHeight = 34;
constexpr int kTitleBarHeight = 17;
constexpr int kGridLeft = 40;
constexpr int kGridTop = 17;
constexpr int kGridCell = 32;
constexpr int kGridRightMargin = 20;
constexpr int kGridBottomMargin = 21;
constexpr int kTabWidth = 20;
constexpr int kTabHeight = 82;
constexpr int kTabCount = 3;
constexpr int kButtonIdBase = 134;
constexpr int kButtonIdClose = 135;
constexpr int kButtonIdMini = 136;
constexpr const char* kUiKorPrefix =
    "texture\\"
    "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
    "\\";

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

void HashTokenValue(unsigned long long* hash, unsigned long long value)
{
    if (!hash) {
        return;
    }
    *hash ^= value;
    *hash *= 1099511628211ull;
}

void HashTokenString(unsigned long long* hash, const std::string& value)
{
    if (!hash) {
        return;
    }
    for (unsigned char ch : value) {
        HashTokenValue(hash, static_cast<unsigned long long>(ch));
    }
    HashTokenValue(hash, 0xFFull);
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

HBITMAP LoadBitmapFromGameData(const std::string& path)
{
    HBITMAP outBitmap = nullptr;
    LoadHBitmapFromGameData(path.c_str(), &outBitmap, nullptr, nullptr);
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

void FillRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void FrameRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void FillRectAlpha(HDC target, const RECT& rect, COLORREF color, BYTE alpha)
{
    if (!target || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    HDC memDc = CreateCompatibleDC(target);
    if (!memDc) {
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = rect.right - rect.left;
    bmi.bmiHeader.biHeight = -(rect.bottom - rect.top);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* memBits = nullptr;
    HBITMAP memBitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &memBits, nullptr, 0);
    if (!memBitmap) {
        DeleteDC(memDc);
        return;
    }

    HGDIOBJ oldBitmap = SelectObject(memDc, memBitmap);
    RECT localRect{ 0, 0, rect.right - rect.left, rect.bottom - rect.top };
    FillRectColor(memDc, localRect, color);

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = alpha;
    blend.AlphaFormat = 0;
    AlphaBlend(target,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        memDc,
        0,
        0,
        rect.right - rect.left,
        rect.bottom - rect.top,
        blend);

    SelectObject(memDc, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDc);
}

void DrawWindowText(HDC hdc, int x, int y, const std::string& text, COLORREF color, UINT format = DT_LEFT | DT_TOP | DT_SINGLELINE)
{
    if (!hdc || text.empty()) {
        return;
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    RECT rect{ x, y, x + 240, y + 24 };
    HGDIOBJ oldFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
    DrawTextA(hdc, text.c_str(), -1, &rect, format);
    SelectObject(hdc, oldFont);
}

void DrawWindowTextRect(HDC hdc, const RECT& rect, const std::string& text, COLORREF color, UINT format)
{
    if (!hdc || text.empty() || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    static HFONT s_sharpUiFont = nullptr;
    if (!s_sharpUiFont) {
        s_sharpUiFont = CreateFontA(
            -11,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            NONANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            "MS Sans Serif");
    }

    RECT drawRect = rect;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    HGDIOBJ oldFont = SelectObject(hdc, s_sharpUiFont ? s_sharpUiFont : GetStockObject(DEFAULT_GUI_FONT));
    DrawTextA(hdc, text.c_str(), -1, &drawRect, format);
    SelectObject(hdc, oldFont);
}

bool IsUsableTabType(int itemType)
{
    return static_cast<unsigned int>(itemType) <= 2u || itemType == 18;
}

bool IsEquipTabType(int itemType)
{
    switch (itemType) {
    case 4:
    case 5:
    case 8:
    case 9:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        return true;
    default:
        return false;
    }
}

bool IsEtcTabType(int itemType)
{
    switch (itemType) {
    case 3:
    case 6:
    case 7:
    case 10:
    case 16:
    case 17:
        return true;
    default:
        return false;
    }
}

bool ItemBelongsToTab(const ITEM_INFO& item, int tabIndex)
{
    switch (tabIndex) {
    case 0:
        return IsUsableTabType(item.m_itemType);
    case 1:
        return IsEquipTabType(item.m_itemType);
    case 2:
        return IsEtcTabType(item.m_itemType);
    default:
        return false;
    }
}

std::string BuildHoverTooltipText(const ITEM_INFO& item)
{
    std::string text;
    if (item.m_refiningLevel > 0 && IsEquipTabType(item.m_itemType)) {
        text = "+" + std::to_string(item.m_refiningLevel) + " ";
    }

    text += item.GetDisplayName();
    text += ": ";
    text += std::to_string((std::max)(1, item.m_num));
    text += " ea";
    return text;
}

std::string BuildShortItemLabel(const ITEM_INFO& item)
{
    std::string shortName = item.GetDisplayName();
    if (shortName.size() > 6) {
        shortName.resize(6);
    }
    return shortName;
}

void DrawItemSlot(HDC hdc,
    HBITMAP slotBitmap,
    HBITMAP hoverBitmap,
    const shopui::BitmapPixels* iconBitmap,
    const RECT& cellRect,
    const ITEM_INFO* item,
    bool hovered)
{
    DrawBitmapTransparent(hdc, slotBitmap, cellRect);

    if (!item) {
        return;
    }

    if (hovered && hoverBitmap) {
        RECT hoverRect{ cellRect.left + 2, cellRect.bottom - 17, cellRect.right - 2, cellRect.bottom - 2 };
        DrawBitmapTransparent(hdc, hoverBitmap, hoverRect);
    }

    if (iconBitmap && iconBitmap->IsValid()) {
        RECT iconRect{ cellRect.left + 4, cellRect.top + 4, cellRect.right - 4, cellRect.bottom - 4 };
        shopui::DrawBitmapPixelsTransparent(hdc, *iconBitmap, iconRect);
    } else {
        std::string shortName = item->GetDisplayName();
        if (shortName.size() > 6) {
            shortName.resize(6);
        }
        DrawWindowText(hdc, cellRect.left + 3, cellRect.top + 10, shortName, RGB(0, 0, 0));
    }

    if (item->m_num > 1) {
        const std::string countText = std::to_string(item->m_num);
        RECT countRect{ cellRect.left + 3, cellRect.bottom - 13, cellRect.right - 3, cellRect.bottom - 2 };
        const UINT countFormat = DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX;
        RECT shadowRect = countRect;
        OffsetRect(&shadowRect, 1, 1);
        DrawWindowTextRect(hdc, shadowRect, countText, RGB(255, 255, 255), countFormat);
        DrawWindowTextRect(hdc, countRect, countText, RGB(34, 46, 80), countFormat);
    }

}

void DrawItemHoverTooltip(HDC hdc, const RECT& clientRect, const RECT& itemRect, const ITEM_INFO& item)
{
    const std::string tooltipText = BuildHoverTooltipText(item);
    DrawDC drawDc(hdc);
    drawDc.SetFont(FONT_DEFAULT, 14, 0);
    SetBkMode(hdc, TRANSPARENT);

    SIZE textSize{};
    drawDc.GetTextExtentPoint32A(tooltipText.c_str(), static_cast<int>(tooltipText.size()), &textSize);

    const int tooltipPaddingX = 8;
    const int tooltipPaddingY = 6;
    const int tooltipHeight = textSize.cy + tooltipPaddingY * 2;
    const int tooltipWidth = textSize.cx + tooltipPaddingX * 2;
    int tooltipLeft = itemRect.left + ((itemRect.right - itemRect.left) - tooltipWidth) / 2;
    int tooltipTop = itemRect.top - tooltipHeight + 2;
    const int minTooltipLeft = static_cast<int>(clientRect.left) + 2;
    const int maxTooltipLeft = static_cast<int>(clientRect.right) - tooltipWidth - 2;
    const int minTooltipTop = static_cast<int>(clientRect.top) + 2;
    tooltipLeft = (std::max)(minTooltipLeft, tooltipLeft);
    tooltipLeft = (std::min)(maxTooltipLeft, tooltipLeft);
    tooltipTop = (std::max)(minTooltipTop, tooltipTop);

    RECT tooltipRect{ tooltipLeft, tooltipTop, tooltipLeft + tooltipWidth, tooltipTop + tooltipHeight };
    FillRectAlpha(hdc, tooltipRect, RGB(48, 48, 48), 180);
    FrameRectColor(hdc, tooltipRect, RGB(96, 96, 96));
    drawDc.SetTextColor(RGB(255, 255, 255));
    drawDc.TextOutA(tooltipRect.left + tooltipPaddingX,
        tooltipRect.top + tooltipPaddingY,
        tooltipText.c_str(),
        static_cast<int>(tooltipText.size()));
}

std::vector<std::string> BuildItemIconCandidates(const ITEM_INFO& item)
{
    std::vector<std::string> out;
    const std::string resource = item.GetResourceName();
    if (resource.empty()) {
        return out;
    }

    std::string stem = NormalizeSlash(resource);
    std::string filenameOnly = stem;
    const size_t slashPos = filenameOnly.find_last_of('\\');
    if (slashPos != std::string::npos && slashPos + 1 < filenameOnly.size()) {
        filenameOnly = filenameOnly.substr(slashPos + 1);
    }

    AddUniqueCandidate(out, stem);
    AddUniqueCandidate(out, stem + ".bmp");
    AddUniqueCandidate(out, filenameOnly);
    AddUniqueCandidate(out, filenameOnly + ".bmp");

    const char* prefixes[] = {
        kUiKorPrefix,
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\",
        "item\\",
        "texture\\item\\",
        "texture\\interface\\item\\",
        "data\\item\\",
        "data\\texture\\item\\",
        "data\\texture\\interface\\item\\",
        nullptr
    };

    for (int index = 0; prefixes[index]; ++index) {
        AddUniqueCandidate(out, std::string(prefixes[index]) + stem);
        AddUniqueCandidate(out, std::string(prefixes[index]) + stem + ".bmp");
        AddUniqueCandidate(out, std::string(prefixes[index]) + filenameOnly);
        AddUniqueCandidate(out, std::string(prefixes[index]) + filenameOnly + ".bmp");
    }

    return out;
}

} // namespace

UIItemWnd::UIItemWnd()
    : m_controlsCreated(false),
      m_currentTab(0),
      m_viewOffset(0),
      m_hoveredItemIndex(-1),
      m_fullHeight(kWindowHeight),
    m_hoverOverlayItem(nullptr),
    m_hoverOverlayRect{},
      m_systemButtons{ nullptr, nullptr, nullptr },
      m_backgroundLeft(nullptr),
      m_backgroundMid(nullptr),
      m_backgroundRight(nullptr),
    m_titleBarBitmap(nullptr),
      m_tabBitmaps{ nullptr, nullptr, nullptr },
      m_hoverBitmap(nullptr),
      m_dragArmed(false),
      m_dragStartPoint{},
      m_dragItemId(0),
      m_dragItemIndex(0),
      m_dragItemEquipLocation(0),
      m_lastVisualStateToken(0ull),
      m_hasVisualStateToken(false)
{
    Create(kWindowWidth, kWindowHeight);
    Move(0, 121);
    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("ItemWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
    }
}

UIItemWnd::~UIItemWnd()
{
    ReleaseAssets();
}

void UIItemWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show != 0) {
        EnsureCreated();
        LayoutChildren();
    }
}

void UIItemWnd::Move(int x, int y)
{
    UIWindow::Move(x, y);
    if (m_controlsCreated) {
        LayoutChildren();
    }
}

bool UIItemWnd::IsUpdateNeed()
{
    if (m_show == 0) {
        return false;
    }
    if (m_isDirty != 0 || !m_hasVisualStateToken) {
        return true;
    }
    return BuildVisualStateToken() != m_lastVisualStateToken;
}

msgresult_t UIItemWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    (void)sender;
    (void)lparam;
    (void)extra;

    if (msg != 6) {
        return 0;
    }

    switch (wparam) {
    case kButtonIdBase:
        SetMiniMode(false);
        return 1;
    case kButtonIdMini:
        SetMiniMode(true);
        return 1;
    case kButtonIdClose:
        SetShow(0);
        return 1;
    default:
        return 1;
    }
}

void UIItemWnd::OnCreate(int x, int y)
{
    (void)x;
    (void)y;
    if (m_controlsCreated) {
        return;
    }

    m_controlsCreated = true;
    LoadAssets();

    struct ButtonSpec {
        const char* offName;
        const char* onName;
        int id;
        const char* tooltip;
    };

    const std::array<ButtonSpec, 3> specs = {{
        { "sys_base_off.bmp", "sys_base_on.bmp", kButtonIdBase, "Base" },
        { "sys_mini_off.bmp", "sys_mini_on.bmp", kButtonIdMini, "Mini" },
        { "sys_close_off.bmp", "sys_close_on.bmp", kButtonIdClose, "Close" },
    }};

    for (size_t index = 0; index < specs.size(); ++index) {
        auto* button = new UIBitmapButton();
        button->SetBitmapName(ResolveUiAssetPath(specs[index].offName).c_str(), 0);
        button->SetBitmapName(ResolveUiAssetPath(specs[index].onName).c_str(), 1);
        button->SetBitmapName(ResolveUiAssetPath(specs[index].onName).c_str(), 2);
        button->Create(button->m_bitmapWidth, button->m_bitmapHeight);
        button->m_id = specs[index].id;
        button->SetToolTip(specs[index].tooltip);
        AddChild(button);
        m_systemButtons[index] = button;
    }

    LayoutChildren();
}

void UIItemWnd::OnDestroy()
{
}

void UIItemWnd::OnDraw()
{
    if (m_show == 0) {
        return;
    }

    EnsureCreated();
    RefreshVisibleItemsForInteractionState();

    if (IsQtUiRuntimeEnabled()) {
        m_lastVisualStateToken = BuildVisualStateToken();
        m_hasVisualStateToken = true;
        m_isDirty = 0;
        return;
    }

    if (m_h > kMiniHeight) {
        for (const VisibleItem& visibleItem : m_visibleItems) {
            if (visibleItem.item) {
                GetItemIcon(*visibleItem.item);
            }
        }
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    RECT windowRect{ m_x, m_y, m_x + m_w, m_y + m_h };
    FillRectColor(hdc, windowRect, RGB(255, 255, 255));

    const int bodyTop = m_y + kTitleBarHeight;
    RECT bodyRect{ m_x, bodyTop, m_x + m_w, m_y + m_h };
    FillRectColor(hdc, bodyRect, RGB(255, 255, 255));

    for (int yPos = bodyTop; yPos < m_y + m_h; yPos += 8) {
        RECT leftRect{ m_x, yPos, m_x + 20, std::min(yPos + 8, m_y + m_h) };
        RECT rightRect{ m_x + m_w - 20, yPos, m_x + m_w, std::min(yPos + 8, m_y + m_h) };
        DrawBitmapTransparent(hdc, m_backgroundLeft, leftRect);
        DrawBitmapTransparent(hdc, m_backgroundRight, rightRect);
    }

    const std::string titleText = GetTitleText();
    RECT titleStrip{ m_x, m_y, m_x + m_w, m_y + kTitleBarHeight };
    DrawBitmapTransparent(hdc, m_titleBarBitmap, titleStrip);
    DrawWindowText(hdc, m_x + 18, m_y + 3, titleText, RGB(255, 255, 255));
    DrawWindowText(hdc, m_x + 17, m_y + 2, titleText, RGB(0, 0, 0));

    RECT activeTabStrip{ m_x, m_y + kGridTop, m_x + kTabWidth, m_y + kGridTop + kTabHeight };
    DrawBitmapTransparent(hdc, m_tabBitmaps[m_currentTab], activeTabStrip);

    if (m_h > kMiniHeight) {
        const int columns = GetItemColumns();

        for (size_t drawIndex = 0; drawIndex < m_visibleItems.size(); ++drawIndex) {
            const int itemIndex = m_viewOffset * columns + static_cast<int>(drawIndex);
            const RECT& cellRect = m_visibleItems[drawIndex].rect;
            const ITEM_INFO* const drawItem = m_visibleItems[drawIndex].item;
            const bool hovered = drawItem && itemIndex == m_hoveredItemIndex;
            DrawItemSlot(hdc,
                m_backgroundMid,
                m_hoverBitmap,
                drawItem ? GetItemIcon(*drawItem) : nullptr,
                cellRect,
                drawItem,
                hovered);

            if (hovered) {
                m_hoverOverlayItem = drawItem;
                m_hoverOverlayRect = cellRect;
            }
        }

        const std::vector<const ITEM_INFO*> filteredItems = GetFilteredItems();
        RECT scrollbarRect{ m_x + m_w - 14, m_y + kGridTop, m_x + m_w - 4, m_y + m_h - kGridBottomMargin };
        FillRectColor(hdc, scrollbarRect, RGB(227, 231, 238));
        FrameRectColor(hdc, scrollbarRect, RGB(164, 173, 189));

        const int maxOffset = std::max(1, GetMaxViewOffset(static_cast<int>(filteredItems.size())) + 1);
        const int trackHeight = scrollbarRect.bottom - scrollbarRect.top - 8;
        const int thumbHeight = std::max(14, trackHeight / maxOffset);
        const int thumbTop = scrollbarRect.top + 4 + ((trackHeight - thumbHeight) * m_viewOffset) / maxOffset;
        RECT thumbRect{ scrollbarRect.left + 2, thumbTop, scrollbarRect.right - 2, thumbTop + thumbHeight };
        FillRectColor(hdc, thumbRect, RGB(129, 146, 199));
        FrameRectColor(hdc, thumbRect, RGB(63, 86, 132));
    }

    DrawChildrenToHdc(hdc);
    ReleaseDrawTarget(hdc);

    m_lastVisualStateToken = BuildVisualStateToken();
    m_hasVisualStateToken = true;
    m_isDirty = 0;
}

void UIItemWnd::DrawHoverOverlay(HDC hdc, const RECT& clientRect) const
{
    if (IsQtUiRuntimeEnabled()) {
        return;
    }

    if (!hdc || m_show == 0 || !m_hoverOverlayItem) {
        return;
    }

    DrawItemHoverTooltip(hdc, clientRect, m_hoverOverlayRect, *m_hoverOverlayItem);
}

void UIItemWnd::OnLBtnDblClk(int x, int y)
{
    if (y >= m_y && y < m_y + kTitleBarHeight) {
        SetMiniMode(m_h != kMiniHeight);
        return;
    }

    UpdateHoveredItem(x, y);
    if (m_hoveredItemIndex >= 0) {
        const std::vector<const ITEM_INFO*> filteredItems = GetFilteredItems();
        if (m_hoveredItemIndex < static_cast<int>(filteredItems.size())) {
            const ITEM_INFO* item = filteredItems[m_hoveredItemIndex];
            if (item && IsEquipTabType(item->m_itemType)) {
                if (item->m_wearLocation != 0) {
                    if (g_modeMgr.SendMsg(
                            CGameMode::GameMsg_RequestUnequipInventoryItem,
                            static_cast<int>(item->m_itemIndex),
                            0,
                            0) != 0) {
                        return;
                    }
                } else if (item->m_location != 0) {
                    if (g_modeMgr.SendMsg(
                            CGameMode::GameMsg_RequestEquipInventoryItem,
                            static_cast<int>(item->m_itemIndex),
                            item->m_location,
                            0) != 0) {
                        return;
                    }
                }
            }
        }
    }

    UIFrameWnd::OnLBtnDblClk(x, y);
}

void UIItemWnd::OnLBtnDown(int x, int y)
{
    m_dragArmed = false;
    m_dragItemId = 0;
    m_dragItemIndex = 0;
    m_dragItemEquipLocation = 0;

    if (y >= m_y && y < m_y + kTitleBarHeight) {
        UIFrameWnd::OnLBtnDown(x, y);
        return;
    }

    if (x >= m_x && x < m_x + kTabWidth && y >= m_y + kGridTop && y < m_y + kGridTop + kTabHeight) {
        SetCurrentTab(GetTabAtPoint(x, y));
        return;
    }

    if (m_h > kMiniHeight
        && x >= m_x + m_w - 14
        && x < m_x + m_w - 4
        && y >= m_y + kGridTop
        && y < m_y + m_h - kGridBottomMargin) {
        const int maxOffset = GetMaxViewOffset(static_cast<int>(GetFilteredItems().size()));
        if (maxOffset > 0) {
            const int trackTop = m_y + kGridTop + 4;
            const int trackHeight = (m_y + m_h - kGridBottomMargin) - (m_y + kGridTop) - 8;
            const int relative = std::max(0, std::min(y - trackTop, trackHeight));
            m_viewOffset = std::min(maxOffset, (relative * (maxOffset + 1)) / std::max(1, trackHeight));
        }
        return;
    }

    UpdateHoveredItem(x, y);
    if (m_hoveredItemIndex >= 0) {
        const std::vector<const ITEM_INFO*> filteredItems = GetFilteredItems();
        if (m_hoveredItemIndex < static_cast<int>(filteredItems.size())) {
            const ITEM_INFO* item = filteredItems[m_hoveredItemIndex];
            if (item) {
                m_dragArmed = true;
                m_dragStartPoint = POINT{ x, y };
                m_dragItemId = item->GetItemId();
                m_dragItemIndex = item->m_itemIndex;
                m_dragItemEquipLocation = item->m_location;
            }
        }
    }
}

void UIItemWnd::OnLBtnUp(int x, int y)
{
    m_dragArmed = false;
    m_dragItemId = 0;
    m_dragItemIndex = 0;
    m_dragItemEquipLocation = 0;
    UIFrameWnd::OnLBtnUp(x, y);
}

void UIItemWnd::OnMouseMove(int x, int y)
{
    UIFrameWnd::OnMouseMove(x, y);
    if (m_dragArmed) {
        const int dx = x - m_dragStartPoint.x;
        const int dy = y - m_dragStartPoint.y;
        if ((dx * dx) + (dy * dy) >= 16) {
            if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
                gameMode->m_dragType = static_cast<int>(DragType::ShortcutItem);
                gameMode->m_dragInfo = DRAG_INFO{};
                gameMode->m_dragInfo.type = static_cast<int>(DragType::ShortcutItem);
                gameMode->m_dragInfo.source = static_cast<int>(DragSource::InventoryWindow);
                gameMode->m_dragInfo.itemId = m_dragItemId;
                gameMode->m_dragInfo.itemIndex = m_dragItemIndex;
                gameMode->m_dragInfo.itemEquipLocation = m_dragItemEquipLocation;
                Invalidate();
                if (g_windowMgr.m_shortCutWnd) {
                    g_windowMgr.m_shortCutWnd->Invalidate();
                }
            }
            m_dragArmed = false;
        }
    }
    UpdateHoveredItem(x, y);
}

void UIItemWnd::OnMouseHover(int x, int y)
{
    UpdateHoveredItem(x, y);
}

void UIItemWnd::DragAndDrop(int x, int y, const DRAG_INFO* const dragInfo)
{
    if (m_show == 0 || m_h <= kMiniHeight || !dragInfo) {
        return;
    }
    if (dragInfo->type != static_cast<int>(DragType::ShortcutItem)
        || dragInfo->source != static_cast<int>(DragSource::EquipmentWindow)
        || dragInfo->itemIndex == 0) {
        return;
    }

    if (x < m_x || x >= m_x + m_w || y < m_y + kTitleBarHeight || y >= m_y + m_h) {
        return;
    }

    if (g_modeMgr.SendMsg(
            CGameMode::GameMsg_RequestUnequipInventoryItem,
            static_cast<int>(dragInfo->itemIndex),
            0,
            0) != 0) {
        Invalidate();
        if (g_windowMgr.m_equipWnd) {
            g_windowMgr.m_equipWnd->Invalidate();
        }
    }
}

void UIItemWnd::StoreInfo()
{
    SaveUiWindowPlacement("ItemWnd", m_x, m_y);
}

bool UIItemWnd::IsMiniMode() const
{
    return m_h == kMiniHeight;
}

bool UIItemWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData) {
        return false;
    }

    DisplayData data{};
    data.title = GetTitleText();
    data.currentTab = m_currentTab;
    data.viewOffset = m_viewOffset;

    const std::vector<const ITEM_INFO*> filteredItems = GetFilteredItems();
    data.maxViewOffset = GetMaxViewOffset(static_cast<int>(filteredItems.size()));

    if (m_h > kMiniHeight) {
        const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
        const bool hideDraggedItem = gameMode
            && gameMode->m_dragType == static_cast<int>(DragType::ShortcutItem)
            && gameMode->m_dragInfo.itemIndex != 0;
        const int columns = GetItemColumns();
        const int rows = GetItemRows();
        const int firstIndex = m_viewOffset * columns;
        const int slotCount = columns * rows;

        data.slots.reserve(static_cast<size_t>(slotCount));
        for (int drawIndex = 0; drawIndex < slotCount; ++drawIndex) {
            const int itemIndex = firstIndex + drawIndex;
            const int column = drawIndex % columns;
            const int row = drawIndex / columns;

            DisplaySlot slot{};
            slot.x = m_x + kGridLeft + column * kGridCell;
            slot.y = m_y + kGridTop + row * kGridCell;
            slot.width = kGridCell;
            slot.height = kGridCell;

            const ITEM_INFO* item = itemIndex < static_cast<int>(filteredItems.size())
                ? filteredItems[itemIndex]
                : nullptr;
            const bool isDraggedSource = item
                && hideDraggedItem
                && item->m_itemIndex == gameMode->m_dragInfo.itemIndex;
            const ITEM_INFO* drawItem = isDraggedSource ? nullptr : item;
            if (drawItem) {
                slot.occupied = true;
                slot.hovered = itemIndex == m_hoveredItemIndex;
                slot.count = drawItem->m_num;
                slot.label = BuildShortItemLabel(*drawItem);
                slot.tooltip = BuildHoverTooltipText(*drawItem);
            }
            data.slots.push_back(slot);
        }
    }

    *outData = std::move(data);
    return true;
}

void UIItemWnd::EnsureCreated()
{
    if (!m_controlsCreated) {
        OnCreate(0, 0);
    }
}

void UIItemWnd::LayoutChildren()
{
    if (!m_controlsCreated) {
        return;
    }

    if (m_systemButtons[0]) {
        m_systemButtons[0]->Move(m_x + 247, m_y + 3);
        m_systemButtons[0]->SetShow(m_h == kMiniHeight ? 1 : 0);
    }
    if (m_systemButtons[1]) {
        m_systemButtons[1]->Move(m_x + 247, m_y + 3);
        m_systemButtons[1]->SetShow(m_h != kMiniHeight ? 1 : 0);
    }
    if (m_systemButtons[2]) {
        m_systemButtons[2]->Move(m_x + 265, m_y + 3);
        m_systemButtons[2]->SetShow(1);
    }
}

void UIItemWnd::LoadAssets()
{
    m_backgroundLeft = LoadBitmapFromGameData(ResolveUiAssetPath("itemwin_left.bmp"));
    m_backgroundMid = LoadBitmapFromGameData(ResolveUiAssetPath("itemwin_mid.bmp"));
    m_backgroundRight = LoadBitmapFromGameData(ResolveUiAssetPath("itemwin_right.bmp"));
    m_titleBarBitmap = LoadBitmapFromGameData(ResolveUiAssetPath("titlebar_fix.bmp"));
    m_tabBitmaps[0] = LoadBitmapFromGameData(ResolveUiAssetPath("tab_itm_01.bmp"));
    m_tabBitmaps[1] = LoadBitmapFromGameData(ResolveUiAssetPath("tab_itm_02.bmp"));
    m_tabBitmaps[2] = LoadBitmapFromGameData(ResolveUiAssetPath("tab_itm_03.bmp"));
    m_hoverBitmap = LoadBitmapFromGameData(ResolveUiAssetPath("item_invert.bmp"));
}

void UIItemWnd::ReleaseAssets()
{
    if (m_backgroundLeft) {
        DeleteObject(m_backgroundLeft);
        m_backgroundLeft = nullptr;
    }
    if (m_backgroundMid) {
        DeleteObject(m_backgroundMid);
        m_backgroundMid = nullptr;
    }
    if (m_backgroundRight) {
        DeleteObject(m_backgroundRight);
        m_backgroundRight = nullptr;
    }
    if (m_titleBarBitmap) {
        DeleteObject(m_titleBarBitmap);
        m_titleBarBitmap = nullptr;
    }
    for (HBITMAP& bitmap : m_tabBitmaps) {
        if (bitmap) {
            DeleteObject(bitmap);
            bitmap = nullptr;
        }
    }
    if (m_hoverBitmap) {
        DeleteObject(m_hoverBitmap);
        m_hoverBitmap = nullptr;
    }
    m_iconCache.clear();
}

void UIItemWnd::SetMiniMode(bool miniMode)
{
    Resize(kWindowWidth, miniMode ? kMiniHeight : m_fullHeight);
    LayoutChildren();
}

void UIItemWnd::SetCurrentTab(int tabIndex)
{
    const int clamped = std::max(0, std::min(tabIndex, kTabCount - 1));
    if (m_currentTab != clamped) {
        m_currentTab = clamped;
        m_viewOffset = 0;
        m_hoveredItemIndex = -1;
    }
}

void UIItemWnd::RefreshVisibleItemsForInteractionState()
{
    const std::vector<const ITEM_INFO*> filteredItems = GetFilteredItems();
    if (filteredItems.empty()) {
        m_hoveredItemIndex = -1;
    } else {
        m_hoveredItemIndex = std::min(m_hoveredItemIndex, static_cast<int>(filteredItems.size()) - 1);
    }
    m_viewOffset = std::min(m_viewOffset, GetMaxViewOffset(static_cast<int>(filteredItems.size())));
    m_viewOffset = std::max(m_viewOffset, 0);
    m_visibleItems.clear();
    m_hoverOverlayItem = nullptr;
    m_hoverOverlayRect = RECT{};

    if (m_h <= kMiniHeight) {
        return;
    }

    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    const bool hideDraggedItem = gameMode
        && gameMode->m_dragType == static_cast<int>(DragType::ShortcutItem)
        && gameMode->m_dragInfo.itemIndex != 0;
    const int columns = GetItemColumns();
    const int rows = GetItemRows();
    const int firstIndex = m_viewOffset * columns;
    const int slotCount = columns * rows;
    m_visibleItems.reserve(static_cast<size_t>(slotCount));

    for (int drawIndex = 0; drawIndex < slotCount; ++drawIndex) {
        const int itemIndex = firstIndex + drawIndex;
        const int column = drawIndex % columns;
        const int row = drawIndex / columns;
        RECT cellRect{
            m_x + kGridLeft + column * kGridCell,
            m_y + kGridTop + row * kGridCell,
            m_x + kGridLeft + (column + 1) * kGridCell,
            m_y + kGridTop + (row + 1) * kGridCell,
        };

        const ITEM_INFO* item = itemIndex < static_cast<int>(filteredItems.size())
            ? filteredItems[itemIndex]
            : nullptr;
        const bool isDraggedSource = item
            && hideDraggedItem
            && item->m_itemIndex == gameMode->m_dragInfo.itemIndex;
        const ITEM_INFO* drawItem = isDraggedSource ? nullptr : item;
        m_visibleItems.push_back({ drawItem, cellRect });
    }
}

void UIItemWnd::UpdateHoveredItem(int globalX, int globalY)
{
    m_hoveredItemIndex = -1;
    const int columns = GetItemColumns();
    for (size_t index = 0; index < m_visibleItems.size(); ++index) {
        const RECT& rect = m_visibleItems[index].rect;
        if (globalX >= rect.left && globalX < rect.right && globalY >= rect.top && globalY < rect.bottom) {
            if (m_visibleItems[index].item) {
                m_hoveredItemIndex = m_viewOffset * columns + static_cast<int>(index);
            }
            return;
        }
    }
}

int UIItemWnd::GetTabAtPoint(int globalX, int globalY) const
{
    if (globalX < m_x || globalX >= m_x + kTabWidth) {
        return m_currentTab;
    }

    const int localY = globalY - (m_y + kGridTop);
    if (localY < 0 || localY >= kTabHeight) {
        return m_currentTab;
    }
    return std::min((localY * kTabCount) / kTabHeight, kTabCount - 1);
}

int UIItemWnd::GetItemColumns() const
{
    return std::max(1, (m_w - kGridLeft - kGridRightMargin) / kGridCell);
}

int UIItemWnd::GetItemRows() const
{
    if (m_h <= kMiniHeight) {
        return 0;
    }
    return std::max(1, (m_h - kGridTop - kGridBottomMargin) / kGridCell);
}

int UIItemWnd::GetMaxViewOffset(int itemCount) const
{
    const int columns = GetItemColumns();
    const int rows = GetItemRows();
    if (columns <= 0 || rows <= 0 || itemCount <= columns * rows) {
        return 0;
    }

    const int usedRows = (itemCount + columns - 1) / columns;
    return std::max(0, usedRows - rows);
}

std::vector<const ITEM_INFO*> UIItemWnd::GetFilteredItems() const
{
    std::vector<const ITEM_INFO*> out;
    const std::list<ITEM_INFO>& items = g_session.GetInventoryItems();
    out.reserve(items.size());
    for (const ITEM_INFO& item : items) {
        if (item.m_wearLocation != 0) {
            continue;
        }
        if (ItemBelongsToTab(item, m_currentTab)) {
            out.push_back(&item);
        }
    }

    std::sort(out.begin(), out.end(), [](const ITEM_INFO* lhs, const ITEM_INFO* rhs) {
        if (!lhs || !rhs) {
            return lhs < rhs;
        }
        if (lhs->m_itemIndex != rhs->m_itemIndex) {
            return lhs->m_itemIndex < rhs->m_itemIndex;
        }
        return lhs->GetItemId() < rhs->GetItemId();
    });
    return out;
}

const shopui::BitmapPixels* UIItemWnd::GetItemIcon(const ITEM_INFO& item)
{
    const unsigned int itemId = item.GetItemId();
    const auto found = m_iconCache.find(itemId);
    if (found != m_iconCache.end()) {
        return found->second.IsValid() ? &found->second : nullptr;
    }

    shopui::BitmapPixels bitmap;
    for (const std::string& candidate : BuildItemIconCandidates(item)) {
        if (!g_fileMgr.IsDataExist(candidate.c_str())) {
            continue;
        }
        bitmap = shopui::LoadBitmapPixelsFromGameData(candidate, true);
        if (bitmap.IsValid()) {
            break;
        }
    }

    auto inserted = m_iconCache.emplace(itemId, std::move(bitmap));
    return inserted.first->second.IsValid() ? &inserted.first->second : nullptr;
}

std::string UIItemWnd::GetTitleText() const
{
    return "Inventory";
}

unsigned long long UIItemWnd::BuildVisualStateToken() const
{
    unsigned long long hash = 1469598103934665603ull;
    HashTokenValue(&hash, static_cast<unsigned long long>(m_show));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_x));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_y));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_w));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_h));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_currentTab));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_viewOffset));
    HashTokenValue(&hash, static_cast<unsigned long long>(static_cast<unsigned int>(m_hoveredItemIndex)));
    if (const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragType));
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.source));
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.itemIndex));
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.itemId));
    }

    const std::list<ITEM_INFO>& items = g_session.GetInventoryItems();
    HashTokenValue(&hash, static_cast<unsigned long long>(items.size()));
    for (const ITEM_INFO& item : items) {
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_itemType));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_location));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_itemIndex));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_wearLocation));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_num));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_price));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_realPrice));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_isIdentified));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_isDamaged));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_refiningLevel));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_isYours));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_deleteTime));
        for (int slotValue : item.m_slot) {
            HashTokenValue(&hash, static_cast<unsigned long long>(slotValue));
        }
        HashTokenString(&hash, item.m_itemName);
    }

    return hash;
}