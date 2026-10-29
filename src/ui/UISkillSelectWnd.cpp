#include "UISkillSelectWnd.h"

#include "core/File.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "item/Item.h"
#include "main/WinMain.h"
#include "session/Session.h"
#include "UIWindowMgr.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace {
constexpr int kWindowWidth = 244;
constexpr int kWindowHeight = 221;
constexpr int kCloseButtonSize = 12;
constexpr int kPadding = 8;
constexpr int kListTop = 27;
constexpr int kRowHeight = 32;
constexpr int kVisibleRows = 4;
constexpr int kIconBoxSize = 24;
constexpr int kScrollBarWidth = 10;
constexpr int kScrollBarGap = 4;
constexpr int kButtonWidth = 56;
constexpr int kButtonHeight = 20;
constexpr int kButtonGap = 8;
constexpr unsigned short kSkillTeleport = 26;
constexpr unsigned short kSkillWarpPortal = 27;

bool IsInsideRect(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string MapStem(std::string mapName)
{
    const std::string lowered = ToLowerAscii(mapName);
    if (lowered.size() > 4
        && (lowered.compare(lowered.size() - 4, 4, ".gat") == 0
            || lowered.compare(lowered.size() - 4, 4, ".rsw") == 0
            || lowered.compare(lowered.size() - 4, 4, ".gnd") == 0)) {
        mapName.resize(mapName.size() - 4);
    }
    return mapName;
}

std::string TitleCaseMapStem(const std::string& stem)
{
    std::string label;
    bool capitalize = true;
    for (unsigned char ch : stem) {
        if (ch == '_' || ch == ' ') {
            label.push_back(' ');
            capitalize = true;
            continue;
        }
        if (capitalize && std::isalpha(ch)) {
            label.push_back(static_cast<char>(std::toupper(ch)));
            capitalize = false;
            continue;
        }
        label.push_back(static_cast<char>(ch));
        capitalize = false;
    }
    return label.empty() ? stem : label;
}

std::string DecodeMapLabel(const std::string& raw)
{
    bool ascii = true;
    for (unsigned char ch : raw) {
        if (ch >= 0x80) {
            ascii = false;
            break;
        }
    }
    if (ascii || raw.empty()) {
        return raw;
    }

    const int wideLength = MultiByteToWideChar(949, 0, raw.data(), static_cast<int>(raw.size()), nullptr, 0);
    if (wideLength <= 0) {
        return raw;
    }
    std::wstring wide(static_cast<size_t>(wideLength), L'\0');
    MultiByteToWideChar(949, 0, raw.data(), static_cast<int>(raw.size()), wide.data(), wideLength);
    const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, nullptr, 0, nullptr, nullptr);
    if (utf8Length <= 0) {
        return raw;
    }
    std::string utf8(static_cast<size_t>(utf8Length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, utf8.data(), utf8Length, nullptr, nullptr);
    return utf8;
}

const std::unordered_map<std::string, std::string>& MapDisplayNames()
{
    static const std::unordered_map<std::string, std::string> names = [] {
        std::unordered_map<std::string, std::string> loaded;
        const char* candidates[] = {
            "mapnametable.txt",
            "data\\mapnametable.txt",
            "data/mapnametable.txt",
            nullptr
        };
        unsigned char* bytes = nullptr;
        int size = 0;
        for (int index = 0; candidates[index] != nullptr; ++index) {
            bytes = g_fileMgr.GetData(candidates[index], &size);
            if (bytes && size > 0) {
                break;
            }
            delete[] bytes;
            bytes = nullptr;
            size = 0;
        }
        if (!bytes || size <= 0) {
            delete[] bytes;
            return loaded;
        }

        const std::string text(reinterpret_cast<const char*>(bytes), static_cast<size_t>(size));
        delete[] bytes;
        size_t cursor = 0;
        while (cursor < text.size()) {
            size_t end = text.find_first_of("\r\n", cursor);
            if (end == std::string::npos) {
                end = text.size();
            }
            const std::string line = text.substr(cursor, end - cursor);
            cursor = end + 1;
            const size_t hash = line.find('#');
            if (hash == std::string::npos || hash == 0) {
                continue;
            }
            const std::string stem = ToLowerAscii(MapStem(line.substr(0, hash)));
            size_t labelEnd = line.find('#', hash + 1);
            if (labelEnd == std::string::npos) {
                labelEnd = line.size();
            }
            std::string label = DecodeMapLabel(line.substr(hash + 1, labelEnd - hash - 1));
            if (stem.empty() || label.empty()) {
                continue;
            }
            loaded.emplace(stem, std::move(label));
        }
        return loaded;
    }();
    return names;
}

std::string WarpDestinationLabel(const std::string& mapName)
{
    if (ToLowerAscii(mapName) == "random") {
        return "Random";
    }
    const std::string stem = MapStem(mapName);
    const auto found = MapDisplayNames().find(ToLowerAscii(stem));
    if (found != MapDisplayNames().end() && !found->second.empty()) {
        return found->second;
    }
    return TitleCaseMapStem(stem);
}

std::string WarpWindowTitle(unsigned short skillId)
{
    if (skillId == kSkillTeleport) {
        return "Teleport";
    }
    if (skillId == kSkillWarpPortal) {
        return "Warp Portal";
    }
    return "Select Destination";
}

int InventoryCountForItem(unsigned int itemId)
{
    int count = 0;
    for (const ITEM_INFO& item : g_session.GetInventoryItems()) {
        if (item.GetItemId() == itemId && item.m_num > 0 && item.m_wearLocation == 0) {
            count += item.m_num;
        }
    }
    return count;
}
}

UISkillSelectWnd::UISkillSelectWnd()
    : m_kind(Kind::None),
      m_skillId(0),
      m_title("Select"),
      m_emptyText("Nothing to select."),
      m_entries(),
      m_closeHovered(false),
      m_closePressed(false),
      m_okHovered(false),
      m_okPressed(false),
      m_cancelHovered(false),
      m_cancelPressed(false),
      m_hoveredIndex(-1),
      m_selectedIndex(-1),
      m_viewOffset(0),
      m_isDraggingScrollThumb(false),
      m_scrollDragOffsetY(0),
      m_iconCache()
{
    Create(kWindowWidth, kWindowHeight);
    Move(96, 96);

    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("SkillSelectWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
    }
}

RECT UISkillSelectWnd::GetCloseButtonRect() const
{
    return shopui::MakeRect(m_x + m_w - kPadding - kCloseButtonSize, m_y + 2, kCloseButtonSize, kCloseButtonSize);
}

RECT UISkillSelectWnd::GetListRect() const
{
    const int width = m_w - (kPadding * 2) - kScrollBarGap - kScrollBarWidth;
    return shopui::MakeRect(m_x + kPadding, m_y + kListTop, width, kRowHeight * kVisibleRows);
}

RECT UISkillSelectWnd::GetRowRect(int visibleRow) const
{
    const RECT listRect = GetListRect();
    return shopui::MakeRect(listRect.left, listRect.top + visibleRow * kRowHeight, listRect.right - listRect.left, kRowHeight);
}

RECT UISkillSelectWnd::GetScrollTrackRect() const
{
    const RECT listRect = GetListRect();
    return shopui::MakeRect(listRect.right + kScrollBarGap, listRect.top, kScrollBarWidth, listRect.bottom - listRect.top);
}

RECT UISkillSelectWnd::GetScrollThumbRect(int entryCount) const
{
    const RECT trackRect = GetScrollTrackRect();
    if (!IsScrollBarVisible(entryCount) || trackRect.bottom <= trackRect.top) {
        return shopui::MakeRect(trackRect.left, trackRect.top, trackRect.right - trackRect.left, trackRect.bottom - trackRect.top);
    }

    const int maxOffset = GetMaxViewOffset(entryCount);
    const int trackHeight = trackRect.bottom - trackRect.top;
    const int thumbHeight = (std::max)(18, (trackHeight * kVisibleRows) / (std::max)(entryCount, 1));
    const int travel = (std::max)(0, trackHeight - thumbHeight);
    const int thumbY = trackRect.top + (maxOffset > 0 ? (travel * m_viewOffset) / maxOffset : 0);
    return shopui::MakeRect(trackRect.left, thumbY, trackRect.right - trackRect.left, thumbHeight);
}

RECT UISkillSelectWnd::GetOkButtonRect() const
{
    const int y = m_y + m_h - kPadding - kButtonHeight;
    const int cancelLeft = m_x + m_w - kPadding - kButtonWidth;
    const int okLeft = cancelLeft - kButtonGap - kButtonWidth;
    return shopui::MakeRect(okLeft, y, kButtonWidth, kButtonHeight);
}

RECT UISkillSelectWnd::GetCancelButtonRect() const
{
    const int y = m_y + m_h - kPadding - kButtonHeight;
    const int left = m_x + m_w - kPadding - kButtonWidth;
    return shopui::MakeRect(left, y, kButtonWidth, kButtonHeight);
}

int UISkillSelectWnd::GetMaxViewOffset(int entryCount) const
{
    return (std::max)(0, entryCount - kVisibleRows);
}

bool UISkillSelectWnd::IsScrollBarVisible(int entryCount) const
{
    return entryCount > kVisibleRows;
}

void UISkillSelectWnd::ClampViewOffset(int entryCount)
{
    m_viewOffset = std::clamp(m_viewOffset, 0, GetMaxViewOffset(entryCount));
}

void UISkillSelectWnd::UpdateScrollFromThumbPosition(int globalY, int entryCount)
{
    const RECT trackRect = GetScrollTrackRect();
    const RECT thumbRect = GetScrollThumbRect(entryCount);
    const int maxOffset = GetMaxViewOffset(entryCount);
    if (maxOffset <= 0) {
        m_viewOffset = 0;
        return;
    }

    const int thumbHeight = thumbRect.bottom - thumbRect.top;
    const int minTop = trackRect.top;
    const int maxTop = trackRect.bottom - thumbHeight;
    const int clampedTop = std::clamp(globalY, minTop, maxTop);
    const int travel = (std::max)(1, maxTop - minTop);
    m_viewOffset = ((clampedTop - minTop) * maxOffset + (travel / 2)) / travel;
}

void UISkillSelectWnd::ResetInteraction()
{
    m_hoveredIndex = -1;
    m_selectedIndex = -1;
    m_viewOffset = 0;
    m_isDraggingScrollThumb = false;
    m_closePressed = false;
    m_okPressed = false;
    m_cancelPressed = false;
    UpdateInteraction(-1, -1);
}

void UISkillSelectWnd::SetWarpDestinations(unsigned short skillId, const std::vector<std::string>& mapNames)
{
    m_kind = Kind::Warp;
    m_skillId = skillId;
    m_title = WarpWindowTitle(skillId);
    m_emptyText = "No destinations available.";
    m_entries.clear();
    m_entries.reserve(mapNames.size());
    for (const std::string& mapName : mapNames) {
        if (mapName.empty()) {
            continue;
        }
        Entry entry;
        entry.label = WarpDestinationLabel(mapName);
        entry.mapName = mapName;
        m_entries.push_back(std::move(entry));
    }
    ResetInteraction();
    Invalidate();
}

void UISkillSelectWnd::SetMakingArrowItems(const std::vector<unsigned int>& itemIds)
{
    m_kind = Kind::Arrow;
    m_skillId = 0;
    m_title = "Create Arrow";
    m_emptyText = "No arrow materials available.";
    m_entries.clear();
    m_entries.reserve(itemIds.size());
    for (unsigned int itemId : itemIds) {
        if (itemId == 0) {
            continue;
        }
        Entry entry;
        entry.itemId = itemId;
        entry.showIcon = true;
        entry.count = InventoryCountForItem(itemId);
        entry.label = g_ttemmgr.GetDisplayName(itemId, true);
        if (entry.label.empty()) {
            entry.label = std::to_string(itemId);
        }
        m_entries.push_back(std::move(entry));
    }
    ResetInteraction();
    Invalidate();
}

bool UISkillSelectWnd::HasValidSelection() const
{
    return m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_entries.size());
}

int UISkillSelectWnd::HitTestVisibleRow(int x, int y) const
{
    const int entryCount = static_cast<int>(m_entries.size());
    const int firstIndex = std::clamp(m_viewOffset, 0, GetMaxViewOffset(entryCount));
    const int visibleCount = (std::min)(kVisibleRows, entryCount - firstIndex);
    for (int index = 0; index < visibleCount; ++index) {
        if (IsInsideRect(GetRowRect(index), x, y)) {
            return firstIndex + index;
        }
    }
    return -1;
}

void UISkillSelectWnd::UpdateInteraction(int x, int y)
{
    m_closeHovered = IsInsideRect(GetCloseButtonRect(), x, y);
    m_okHovered = IsInsideRect(GetOkButtonRect(), x, y);
    m_cancelHovered = IsInsideRect(GetCancelButtonRect(), x, y);
    m_hoveredIndex = HitTestVisibleRow(x, y);
}

const shopui::BitmapPixels* UISkillSelectWnd::GetItemIcon(unsigned int itemId)
{
    auto found = m_iconCache.find(itemId);
    if (found != m_iconCache.end()) {
        return found->second.IsValid() ? &found->second : nullptr;
    }

    ITEM_INFO probe;
    probe.SetItemId(itemId);
    probe.m_isIdentified = 1;
    shopui::BitmapPixels bitmap;
    shopui::TryLoadItemIconPixels(probe, &bitmap);
    auto inserted = m_iconCache.emplace(itemId, std::move(bitmap));
    return inserted.first->second.IsValid() ? &inserted.first->second : nullptr;
}

void UISkillSelectWnd::SubmitSelection()
{
    if (!HasValidSelection()) {
        return;
    }

    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!gameMode) {
        return;
    }

    const Entry& entry = m_entries[static_cast<size_t>(m_selectedIndex)];
    bool sent = false;
    if (m_kind == Kind::Warp) {
        sent = gameMode->SendWarpMapSelection(entry.mapName);
    } else if (m_kind == Kind::Arrow && entry.itemId != 0 && entry.itemId <= 0xFFFFu) {
        sent = gameMode->SendMakingArrowSelection(static_cast<unsigned short>(entry.itemId));
    }
    if (sent) {
        g_windowMgr.DeleteWindow(this);
    }
}

void UISkillSelectWnd::CancelSelection()
{
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (m_kind == Kind::Warp) {
        if (!gameMode || !gameMode->CancelWarpMapSelection()) {
            return;
        }
    } else if (m_kind == Kind::Arrow && gameMode) {
        gameMode->SendMakingArrowSelection(0);
    }
    g_windowMgr.DeleteWindow(this);
}

void UISkillSelectWnd::OnDraw()
{
    if (m_show == 0) {
        return;
    }

    HDC hdc = AcquireMainWindowDrawTarget();
    if (!hdc) {
        return;
    }

    const RECT bounds = shopui::MakeRect(m_x, m_y, m_w, m_h);
    shopui::DrawFrameWindow(hdc, bounds, m_title.c_str());

    const RECT closeRect = GetCloseButtonRect();
    const RECT listRect = GetListRect();
    const RECT trackRect = GetScrollTrackRect();
    const RECT okRect = GetOkButtonRect();
    const RECT cancelRect = GetCancelButtonRect();
    const int entryCount = static_cast<int>(m_entries.size());
    ClampViewOffset(entryCount);

    shopui::FillRectColor(hdc, closeRect, m_closePressed ? RGB(185, 199, 222) : (m_closeHovered ? RGB(215, 223, 240) : RGB(231, 226, 214)));
    shopui::FrameRectColor(hdc, closeRect, RGB(127, 122, 112));
    shopui::DrawWindowTextRect(hdc, closeRect, "X", RGB(0, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    shopui::FillRectColor(hdc, listRect, RGB(255, 255, 255));
    shopui::FrameRectColor(hdc, listRect, RGB(196, 192, 181));

    if (entryCount <= 0) {
        shopui::DrawWindowTextRect(hdc, listRect, m_emptyText.c_str(), RGB(80, 80, 80), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        const int firstIndex = m_viewOffset;
        const int visibleCount = (std::min)(kVisibleRows, entryCount - firstIndex);
        for (int drawIndex = 0; drawIndex < visibleCount; ++drawIndex) {
            const Entry& entry = m_entries[static_cast<size_t>(firstIndex + drawIndex)];
            const int entryIndex = firstIndex + drawIndex;
            const RECT rowRect = GetRowRect(drawIndex);
            const bool hovered = m_hoveredIndex == entryIndex;
            const bool selected = m_selectedIndex == entryIndex;
            shopui::FillRectColor(hdc, rowRect, selected ? RGB(215, 223, 240) : (hovered ? RGB(236, 232, 222) : RGB(249, 247, 243)));
            shopui::FrameRectColor(hdc, rowRect, selected ? RGB(126, 149, 191) : RGB(188, 180, 167));

            int nameLeft = rowRect.left + 8;
            if (entry.showIcon) {
                const RECT iconRect = shopui::MakeRect(rowRect.left + 4, rowRect.top + (kRowHeight - kIconBoxSize) / 2, kIconBoxSize, kIconBoxSize);
                shopui::FillRectColor(hdc, iconRect, RGB(245, 242, 234));
                shopui::FrameRectColor(hdc, iconRect, RGB(166, 159, 145));
                if (const shopui::BitmapPixels* icon = GetItemIcon(entry.itemId)) {
                    const RECT dst = shopui::MakeRect(iconRect.left + 1, iconRect.top + 1, kIconBoxSize - 2, kIconBoxSize - 2);
                    shopui::DrawBitmapPixelsTransparent(hdc, *icon, dst);
                }
                nameLeft = rowRect.left + 34;
            }

            const RECT nameRect = shopui::MakeRect(nameLeft, rowRect.top + 4, rowRect.right - nameLeft - 40, 12);
            shopui::DrawWindowTextRect(hdc, nameRect, entry.label.c_str(), RGB(0, 0, 0), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (entry.count > 1) {
                const RECT countRect = shopui::MakeRect(rowRect.right - 34, rowRect.top + 4, 28, 12);
                shopui::DrawWindowTextRect(hdc, countRect, std::to_string(entry.count).c_str(), RGB(90, 64, 32), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }

    if (IsScrollBarVisible(entryCount)) {
        const RECT thumbRect = GetScrollThumbRect(entryCount);
        shopui::FillRectColor(hdc, trackRect, RGB(227, 231, 238));
        shopui::FrameRectColor(hdc, trackRect, RGB(164, 173, 189));
        shopui::FillRectColor(hdc, thumbRect, RGB(180, 188, 205));
        shopui::FrameRectColor(hdc, thumbRect, RGB(120, 130, 150));
    }

    shopui::FillRectColor(hdc, okRect, !HasValidSelection() ? RGB(208, 208, 208) : (m_okPressed ? RGB(185, 199, 222) : (m_okHovered ? RGB(215, 223, 240) : RGB(233, 228, 216))));
    shopui::FrameRectColor(hdc, okRect, RGB(140, 133, 120));
    shopui::DrawWindowTextRect(hdc, okRect, "OK", RGB(0, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    shopui::FillRectColor(hdc, cancelRect, m_cancelPressed ? RGB(185, 199, 222) : (m_cancelHovered ? RGB(215, 223, 240) : RGB(233, 228, 216)));
    shopui::FrameRectColor(hdc, cancelRect, RGB(140, 133, 120));
    shopui::DrawWindowTextRect(hdc, cancelRect, "Cancel", RGB(0, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    ReleaseMainWindowDrawTarget(hdc);
}

void UISkillSelectWnd::OnLBtnDblClk(int x, int y)
{
    const int hitIndex = HitTestVisibleRow(x, y);
    if (hitIndex < 0) {
        OnLBtnDown(x, y);
        return;
    }
    m_selectedIndex = hitIndex;
    SubmitSelection();
}

void UISkillSelectWnd::OnLBtnDown(int x, int y)
{
    UpdateInteraction(x, y);
    if (m_closeHovered) {
        m_closePressed = true;
        Invalidate();
        return;
    }
    if (m_okHovered) {
        m_okPressed = true;
        Invalidate();
        return;
    }
    if (m_cancelHovered) {
        m_cancelPressed = true;
        Invalidate();
        return;
    }

    const int entryCount = static_cast<int>(m_entries.size());
    if (IsScrollBarVisible(entryCount)) {
        const RECT trackRect = GetScrollTrackRect();
        const RECT thumbRect = GetScrollThumbRect(entryCount);
        if (IsInsideRect(thumbRect, x, y)) {
            m_isDraggingScrollThumb = true;
            m_scrollDragOffsetY = y - thumbRect.top;
            return;
        }
        if (IsInsideRect(trackRect, x, y)) {
            UpdateScrollFromThumbPosition(y - ((thumbRect.bottom - thumbRect.top) / 2), entryCount);
            Invalidate();
            return;
        }
    }

    const int hitIndex = HitTestVisibleRow(x, y);
    if (hitIndex >= 0) {
        m_selectedIndex = hitIndex;
        Invalidate();
        return;
    }

    UIFrameWnd::OnLBtnDown(x, y);
}

void UISkillSelectWnd::OnLBtnUp(int x, int y)
{
    const bool closePressed = m_closePressed;
    const bool okPressed = m_okPressed;
    const bool cancelPressed = m_cancelPressed;
    m_closePressed = false;
    m_okPressed = false;
    m_cancelPressed = false;
    const bool wasDraggingScrollThumb = m_isDraggingScrollThumb;
    m_isDraggingScrollThumb = false;

    UIFrameWnd::OnLBtnUp(x, y);
    UpdateInteraction(x, y);
    if (wasDraggingScrollThumb) {
        Invalidate();
        return;
    }
    if (closePressed && m_closeHovered) {
        CancelSelection();
        return;
    }
    if (okPressed && m_okHovered) {
        SubmitSelection();
        return;
    }
    if (cancelPressed && m_cancelHovered) {
        CancelSelection();
        return;
    }
    Invalidate();
}

void UISkillSelectWnd::OnMouseMove(int x, int y)
{
    if (m_isDraggingScrollThumb) {
        UpdateScrollFromThumbPosition(y - m_scrollDragOffsetY, static_cast<int>(m_entries.size()));
        Invalidate();
        return;
    }
    UIFrameWnd::OnMouseMove(x, y);
    UpdateInteraction(x, y);
    Invalidate();
}

void UISkillSelectWnd::OnMouseHover(int x, int y)
{
    UpdateInteraction(x, y);
    Invalidate();
}

void UISkillSelectWnd::OnWheel(int delta)
{
    if (delta > 0) {
        --m_viewOffset;
    } else if (delta < 0) {
        ++m_viewOffset;
    }
    ClampViewOffset(static_cast<int>(m_entries.size()));
    Invalidate();
}

void UISkillSelectWnd::StoreInfo()
{
    SaveUiWindowPlacement("SkillSelectWnd", m_x, m_y);
}

bool UISkillSelectWnd::HandleKeyDown(int virtualKey)
{
    if (m_show == 0) {
        return false;
    }
    if (virtualKey == VK_ESCAPE) {
        CancelSelection();
        return true;
    }
    if (virtualKey == VK_RETURN) {
        SubmitSelection();
        return true;
    }
    if (m_entries.empty()) {
        return false;
    }
    if (virtualKey == VK_UP) {
        m_selectedIndex = m_selectedIndex < 0 ? 0 : (std::max)(0, m_selectedIndex - 1);
        if (m_selectedIndex < m_viewOffset) {
            m_viewOffset = m_selectedIndex;
        }
        Invalidate();
        return true;
    }
    if (virtualKey == VK_DOWN) {
        const int last = static_cast<int>(m_entries.size()) - 1;
        m_selectedIndex = m_selectedIndex < 0 ? 0 : (std::min)(last, m_selectedIndex + 1);
        if (m_selectedIndex >= m_viewOffset + kVisibleRows) {
            m_viewOffset = m_selectedIndex - kVisibleRows + 1;
        }
        Invalidate();
        return true;
    }
    return false;
}

bool UISkillSelectWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData) {
        return false;
    }

    DisplayData data{};
    data.title = m_title;
    data.emptyText = m_emptyText;

    const RECT closeRect = GetCloseButtonRect();
    data.closeButton.x = closeRect.left;
    data.closeButton.y = closeRect.top;
    data.closeButton.width = closeRect.right - closeRect.left;
    data.closeButton.height = closeRect.bottom - closeRect.top;
    data.closeButton.hovered = m_closeHovered;
    data.closeButton.pressed = m_closePressed;
    data.closeButton.label = "X";

    const RECT okRect = GetOkButtonRect();
    data.okButton.x = okRect.left;
    data.okButton.y = okRect.top;
    data.okButton.width = okRect.right - okRect.left;
    data.okButton.height = okRect.bottom - okRect.top;
    data.okButton.hovered = m_okHovered;
    data.okButton.pressed = m_okPressed;
    data.okButton.enabled = HasValidSelection();
    data.okButton.label = "OK";

    const RECT cancelRect = GetCancelButtonRect();
    data.cancelButton.x = cancelRect.left;
    data.cancelButton.y = cancelRect.top;
    data.cancelButton.width = cancelRect.right - cancelRect.left;
    data.cancelButton.height = cancelRect.bottom - cancelRect.top;
    data.cancelButton.hovered = m_cancelHovered;
    data.cancelButton.pressed = m_cancelPressed;
    data.cancelButton.label = "Cancel";

    const int entryCount = static_cast<int>(m_entries.size());
    const int firstIndex = std::clamp(m_viewOffset, 0, GetMaxViewOffset(entryCount));
    const int visibleCount = (std::min)(kVisibleRows, entryCount - firstIndex);
    data.rows.reserve(static_cast<size_t>((std::max)(0, visibleCount)));
    for (int drawIndex = 0; drawIndex < visibleCount; ++drawIndex) {
        const Entry& entry = m_entries[static_cast<size_t>(firstIndex + drawIndex)];
        const RECT rowRect = GetRowRect(drawIndex);
        DisplayRow row{};
        row.itemId = entry.itemId;
        row.identified = true;
        row.showIcon = entry.showIcon;
        row.x = rowRect.left;
        row.y = rowRect.top;
        row.width = rowRect.right - rowRect.left;
        row.height = rowRect.bottom - rowRect.top;
        row.hovered = m_hoveredIndex == firstIndex + drawIndex;
        row.selected = m_selectedIndex == firstIndex + drawIndex;
        row.count = entry.count;
        row.label = entry.label;
        data.rows.push_back(std::move(row));
    }

    data.scrollBar.visible = IsScrollBarVisible(entryCount);
    if (data.scrollBar.visible) {
        const RECT trackRect = GetScrollTrackRect();
        const RECT thumbRect = GetScrollThumbRect(entryCount);
        data.scrollBar.trackX = trackRect.left;
        data.scrollBar.trackY = trackRect.top;
        data.scrollBar.trackWidth = trackRect.right - trackRect.left;
        data.scrollBar.trackHeight = trackRect.bottom - trackRect.top;
        data.scrollBar.thumbX = thumbRect.left;
        data.scrollBar.thumbY = thumbRect.top;
        data.scrollBar.thumbWidth = thumbRect.right - thumbRect.left;
        data.scrollBar.thumbHeight = thumbRect.bottom - thumbRect.top;
    }

    *outData = std::move(data);
    return true;
}
