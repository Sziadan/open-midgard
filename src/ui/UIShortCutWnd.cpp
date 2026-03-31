#include "UIShortCutWnd.h"

#include "UIShopCommon.h"
#include "UIWindowMgr.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "item/Item.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "session/Session.h"
#include "skill/Skill.h"

#include <algorithm>
#include <string>

namespace {

constexpr int kDefaultWindowWidth = 275;
constexpr int kDefaultWindowHeight = 31;
constexpr int kSlotLeft = 5;
constexpr int kSlotTop = 4;
constexpr int kSlotStride = 29;
constexpr int kSlotSize = 24;
constexpr int kPageTextInsetX = 13;
constexpr int kPageTextInsetY = 16;

void DrawBitmapFit(HDC hdc, HBITMAP bitmap, const RECT& rect)
{
    shopui::DrawBitmapTransparent(hdc, bitmap, rect);
}

void DrawOutlinedText(HDC hdc, RECT rect, const std::string& text, COLORREF fillColor, COLORREF outlineColor = RGB(0, 0, 0))
{
    if (!hdc || text.empty()) {
        return;
    }

    SetBkMode(hdc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(hdc, shopui::GetUiFont());

    RECT shadowRect = rect;
    SetTextColor(hdc, outlineColor);
    OffsetRect(&shadowRect, -1, 0);
    DrawTextA(hdc, text.c_str(), -1, &shadowRect, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
    shadowRect = rect;
    OffsetRect(&shadowRect, 1, 0);
    DrawTextA(hdc, text.c_str(), -1, &shadowRect, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
    shadowRect = rect;
    OffsetRect(&shadowRect, 0, -1);
    DrawTextA(hdc, text.c_str(), -1, &shadowRect, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
    shadowRect = rect;
    OffsetRect(&shadowRect, 0, 1);
    DrawTextA(hdc, text.c_str(), -1, &shadowRect, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);

    SetTextColor(hdc, fillColor);
    DrawTextA(hdc, text.c_str(), -1, &rect, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
}

std::string ResolveShortcutSkillIconPath(int skillId)
{
    g_skillMgr.EnsureLoaded();
    const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillId);
    if (metadata && !metadata->skillIdName.empty()) {
        const std::string lowered = shopui::ToLowerAscii(metadata->skillIdName);
        const std::string direct = "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\" + lowered + ".bmp";
        const std::string dataPath = "data\\" + direct;
        if (g_fileMgr.IsDataExist(direct.c_str())) {
            return direct;
        }
        if (g_fileMgr.IsDataExist(dataPath.c_str())) {
            return dataPath;
        }
        return direct;
    }
    return g_skillMgr.GetSkillIconPath(skillId);
}

} // namespace

UIShortCutWnd::UIShortCutWnd()
    : m_backgroundBitmap(nullptr),
      m_slotButtonBitmap(nullptr),
      m_hoverSlot(-1),
      m_pressedSlot(-1),
      m_pressedSlotAbsoluteIndex(-1),
      m_slotPressArmed(0),
      m_isDragging(0),
      m_startGlobalX(0),
      m_startGlobalY(0),
      m_orgX(0),
      m_orgY(0),
      m_lastDrawStateToken(0ull),
      m_hasDrawStateToken(false)
{
    Create(kDefaultWindowWidth, kDefaultWindowHeight);
    LoadAssets();

    int x = 0;
    int y = 0;
    if (!LoadUiWindowPlacement("ShortCutWnd", &x, &y)) {
        RECT clientRect{ 0, 0, 800, 600 };
        if (g_hMainWnd) {
            GetClientRect(g_hMainWnd, &clientRect);
        }
        x = (std::max)(0, (static_cast<int>(clientRect.right) - m_w) / 2);
        y = (std::max)(0, static_cast<int>(clientRect.bottom) - m_h - 96);
    }
    Move(x, y);
}

UIShortCutWnd::~UIShortCutWnd()
{
    ReleaseAssets();
}

void UIShortCutWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
}

bool UIShortCutWnd::IsUpdateNeed()
{
    if (m_show == 0) {
        return false;
    }
    if (m_isDirty != 0 || !m_hasDrawStateToken) {
        return true;
    }
    return BuildDisplayStateToken() != m_lastDrawStateToken;
}

void UIShortCutWnd::DragAndDrop(int x, int y, const DRAG_INFO* const info)
{
    if (!info) {
        return;
    }

    const int visibleSlot = GetSlotAtGlobalPoint(x, y);
    if (visibleSlot < 0) {
        return;
    }

    bool changed = false;
    if (info->source == static_cast<int>(DragSource::ShortcutWindow)
        && info->shortcutSlotAbsoluteIndex >= 0) {
        const int targetAbsoluteSlot = g_session.GetShortcutSlotAbsoluteIndex(visibleSlot);
        if (targetAbsoluteSlot == info->shortcutSlotAbsoluteIndex) {
            return;
        }

        const SHORTCUT_SLOT* sourceSlot = g_session.GetShortcutSlotByAbsoluteIndex(info->shortcutSlotAbsoluteIndex);
        if (!sourceSlot || sourceSlot->id == 0) {
            return;
        }

        changed = g_session.SetShortcutSlotByAbsoluteIndex(
            targetAbsoluteSlot,
            sourceSlot->isSkill,
            sourceSlot->id,
            sourceSlot->count);
        const bool cleared = g_session.ClearShortcutSlotByAbsoluteIndex(info->shortcutSlotAbsoluteIndex);
        if (changed) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShortcutUpdate, targetAbsoluteSlot, 0, 0);
        }
        if (cleared) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShortcutUpdate, info->shortcutSlotAbsoluteIndex, 0, 0);
        }
        if (changed || cleared) {
            Invalidate();
        }
        return;
    }

    if (info->type == static_cast<int>(DragType::ShortcutItem) && info->itemId != 0) {
        changed = g_session.SetShortcutSlotByVisibleIndex(visibleSlot, 0, info->itemId, 0);
    } else if (info->type == static_cast<int>(DragType::ShortcutSkill) && info->skillId != 0) {
        const unsigned short skillLevel = static_cast<unsigned short>((std::max)(1, info->skillLevel));
        changed = g_session.SetShortcutSlotByVisibleIndex(visibleSlot, 1, static_cast<unsigned int>(info->skillId), skillLevel);
    }

    if (changed) {
        const int absoluteSlot = g_session.GetShortcutSlotAbsoluteIndex(visibleSlot);
        g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShortcutUpdate, absoluteSlot, 0, 0);
        Invalidate();
    }
}

void UIShortCutWnd::StoreInfo()
{
    SaveUiWindowPlacement("ShortCutWnd", m_x, m_y);
}

void UIShortCutWnd::OnDraw()
{
    if (IsQtUiRuntimeEnabled()) {
        m_lastDrawStateToken = BuildDisplayStateToken();
        m_hasDrawStateToken = true;
        m_isDirty = 0;
        return;
    }

    bool useShared = false;
    HDC hdc = AcquireDrawTarget(&useShared);
    if (!hdc) {
        return;
    }

    const RECT bounds{ m_x, m_y, m_x + m_w, m_y + m_h };
    if (m_backgroundBitmap) {
        shopui::DrawBitmapTransparent(hdc, m_backgroundBitmap, bounds);
    } else {
        shopui::FillRectColor(hdc, bounds, RGB(224, 224, 224));
        shopui::FrameRectColor(hdc, bounds, RGB(96, 96, 96));
    }

    if (m_slotButtonBitmap && m_hoverSlot >= 0) {
        DrawBitmapFit(hdc, m_slotButtonBitmap, GetSlotRect(m_hoverSlot));
    }

    for (int slotIndex = 0; slotIndex < kShortcutSlotsPerPage; ++slotIndex) {
        const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByVisibleIndex(slotIndex);
        if (!slot || slot->id == 0) {
            continue;
        }

        const int absoluteSlot = g_session.GetShortcutSlotAbsoluteIndex(slotIndex);
        const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
        if (gameMode
            && gameMode->m_dragType != static_cast<int>(DragType::None)
            && gameMode->m_dragInfo.source == static_cast<int>(DragSource::ShortcutWindow)
            && gameMode->m_dragInfo.shortcutSlotAbsoluteIndex == absoluteSlot) {
            continue;
        }

        RECT slotRect = GetSlotRect(slotIndex);
        RECT iconRect = slotRect;
        if (slot->isSkill != 0) {
            if (HBITMAP icon = GetSkillIcon(static_cast<int>(slot->id))) {
                DrawBitmapFit(hdc, icon, iconRect);
            }

            const PLAYER_SKILL_INFO* skill = g_session.GetSkillItemBySkillId(static_cast<int>(slot->id));
            const int displayedLevel = skill ? skill->level : static_cast<int>(slot->count);
            if (displayedLevel > 0) {
                DrawSlotOverlayText(hdc, slotRect, displayedLevel);
            }
        } else {
            ITEM_INFO fallbackItem{};
            fallbackItem.SetItemId(slot->id);
            fallbackItem.m_isIdentified = 1;
            const ITEM_INFO* item = g_session.GetInventoryItemByItemId(slot->id);
            const ITEM_INFO& iconSource = item ? *item : fallbackItem;
            if (HBITMAP icon = GetItemIcon(iconSource)) {
                DrawBitmapFit(hdc, icon, iconRect);
            }

            const int displayedCount = item ? item->m_num : 0;
            if (displayedCount > 0) {
                DrawSlotOverlayText(hdc, slotRect, displayedCount);
            }
        }
    }

    RECT pageRect{ m_x + m_w - kPageTextInsetX - 12, m_y + m_h - kPageTextInsetY, m_x + m_w - 2, m_y + m_h - 2 };
    DrawOutlinedText(hdc, pageRect, std::to_string(g_session.GetShortcutPage() + 1), RGB(255, 255, 255));

    ReleaseDrawTarget(hdc, useShared);

    m_lastDrawStateToken = BuildDisplayStateToken();
    m_hasDrawStateToken = true;
    m_isDirty = 0;
}

void UIShortCutWnd::OnLBtnDown(int x, int y)
{
    const int visibleSlot = GetSlotAtGlobalPoint(x, y);
    if (visibleSlot >= 0) {
        const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByVisibleIndex(visibleSlot);
        if (slot && slot->id != 0) {
            m_pressedSlot = visibleSlot;
            m_pressedSlotAbsoluteIndex = g_session.GetShortcutSlotAbsoluteIndex(visibleSlot);
            m_slotPressArmed = 1;
            m_startGlobalX = x;
            m_startGlobalY = y;
        }
        return;
    }

    m_pressedSlot = -1;
    m_pressedSlotAbsoluteIndex = -1;
    m_slotPressArmed = 0;

    const RECT bounds{ m_x, m_y, m_x + m_w, m_y + m_h };
    if (x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom) {
        m_isDragging = 1;
        m_startGlobalX = x;
        m_startGlobalY = y;
        m_orgX = m_x;
        m_orgY = m_y;
    }
}

void UIShortCutWnd::OnLBtnDblClk(int x, int y)
{
    const int visibleSlot = GetSlotAtGlobalPoint(x, y);
    if (visibleSlot >= 0) {
        ActivateSlot(visibleSlot);
    }
}

void UIShortCutWnd::OnLBtnUp(int x, int y)
{
    if (m_slotPressArmed != 0
        && m_pressedSlot >= 0
        && GetSlotAtGlobalPoint(x, y) == m_pressedSlot) {
        ActivateSlot(m_pressedSlot);
    }
    m_pressedSlot = -1;
    m_pressedSlotAbsoluteIndex = -1;
    m_slotPressArmed = 0;

    if (m_isDragging != 0) {
        m_isDragging = 0;
        StoreInfo();
    }
}

void UIShortCutWnd::OnMouseMove(int x, int y)
{
    if (m_isDragging != 0) {
        int snappedX = m_orgX + (x - m_startGlobalX);
        int snappedY = m_orgY + (y - m_startGlobalY);
        g_windowMgr.SnapWindowToNearby(this, &snappedX, &snappedY);
        g_windowMgr.ClampWindowToClient(&snappedX, &snappedY, m_w, m_h);
        Move(snappedX, snappedY);
        return;
    }

    if (m_slotPressArmed != 0 && m_pressedSlotAbsoluteIndex >= 0) {
        const int dx = x - m_startGlobalX;
        const int dy = y - m_startGlobalY;
        if ((dx * dx) + (dy * dy) >= 16) {
            const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByAbsoluteIndex(m_pressedSlotAbsoluteIndex);
            if (slot && slot->id != 0) {
                if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
                    gameMode->m_dragType = static_cast<int>(slot->isSkill != 0 ? DragType::ShortcutSkill : DragType::ShortcutItem);
                    gameMode->m_dragInfo = DRAG_INFO{};
                    gameMode->m_dragInfo.type = gameMode->m_dragType;
                    gameMode->m_dragInfo.source = static_cast<int>(DragSource::ShortcutWindow);
                    gameMode->m_dragInfo.shortcutSlotAbsoluteIndex = m_pressedSlotAbsoluteIndex;
                    if (slot->isSkill != 0) {
                        gameMode->m_dragInfo.skillId = static_cast<int>(slot->id);
                        gameMode->m_dragInfo.skillLevel = static_cast<int>(slot->count);
                    } else {
                        gameMode->m_dragInfo.itemId = slot->id;
                    }
                    Invalidate();
                }
            }
            m_slotPressArmed = 0;
        }
    }

    UpdateHoverSlot(x, y);
}

void UIShortCutWnd::OnMouseHover(int x, int y)
{
    UpdateHoverSlot(x, y);
}

void UIShortCutWnd::OnRBtnDown(int x, int y)
{
    const int visibleSlot = GetSlotAtGlobalPoint(x, y);
    if (visibleSlot < 0) {
        return;
    }

    if (g_session.ClearShortcutSlotByVisibleIndex(visibleSlot)) {
        const int absoluteSlot = g_session.GetShortcutSlotAbsoluteIndex(visibleSlot);
        g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShortcutUpdate, absoluteSlot, 0, 0);
        Invalidate();
    }
}

void UIShortCutWnd::OnWheel(int delta)
{
    const int oldPage = g_session.GetShortcutPage();
    int newPage = oldPage;
    if (delta > 0) {
        newPage = oldPage - 1;
    } else if (delta < 0) {
        newPage = oldPage + 1;
    }
    g_session.SetShortcutPage(newPage);
    if (g_session.GetShortcutPage() != oldPage) {
        Invalidate();
    }
}

int UIShortCutWnd::GetHoverSlot() const
{
    return m_hoverSlot;
}

void UIShortCutWnd::LoadAssets()
{
    ReleaseAssets();
    m_backgroundBitmap = shopui::LoadBitmapFromGameData(shopui::ResolveUiAssetPath("shortitem_bg.bmp"));
    m_slotButtonBitmap = shopui::LoadBitmapFromGameData(shopui::ResolveUiAssetPath("shortitem_btn.bmp"));

    BITMAP bm{};
    if (m_backgroundBitmap && GetObjectA(m_backgroundBitmap, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
        Resize(bm.bmWidth, bm.bmHeight);
    }
}

void UIShortCutWnd::ReleaseAssets()
{
    if (m_backgroundBitmap) {
        DeleteObject(m_backgroundBitmap);
        m_backgroundBitmap = nullptr;
    }
    if (m_slotButtonBitmap) {
        DeleteObject(m_slotButtonBitmap);
        m_slotButtonBitmap = nullptr;
    }
    for (auto& entry : m_itemIconCache) {
        if (entry.second) {
            DeleteObject(entry.second);
        }
    }
    m_itemIconCache.clear();
    for (auto& entry : m_skillIconCache) {
        if (entry.second) {
            DeleteObject(entry.second);
        }
    }
    m_skillIconCache.clear();
}

void UIShortCutWnd::UpdateHoverSlot(int globalX, int globalY)
{
    const int newHoverSlot = GetSlotAtGlobalPoint(globalX, globalY);
    if (m_hoverSlot != newHoverSlot) {
        m_hoverSlot = newHoverSlot;
        Invalidate();
    }
}

int UIShortCutWnd::GetSlotAtGlobalPoint(int globalX, int globalY) const
{
    const int localX = globalX - m_x;
    const int localY = globalY - m_y;
    if (localY < 0 || localY >= 24 || localX <= 3) {
        return -1;
    }

    const int adjustedX = localX - 4;
    if (adjustedX < 0) {
        return -1;
    }

    const int slot = adjustedX / kSlotStride;
    if (slot < 0 || slot >= kShortcutSlotsPerPage) {
        return -1;
    }
    if (adjustedX >= ((slot + 1) * kSlotStride)) {
        return -1;
    }
    return slot;
}

RECT UIShortCutWnd::GetSlotRect(int visibleSlot) const
{
    return RECT{
        m_x + kSlotLeft + visibleSlot * kSlotStride,
        m_y + kSlotTop,
        m_x + kSlotLeft + visibleSlot * kSlotStride + kSlotSize,
        m_y + kSlotTop + kSlotSize
    };
}

void UIShortCutWnd::ActivateSlot(int visibleSlot)
{
    g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShortcutUse, visibleSlot, 0, 0);
}

void UIShortCutWnd::DrawSlotOverlayText(HDC hdc, const RECT& slotRect, int value) const
{
    RECT textRect = slotRect;
    textRect.left += 3;
    textRect.top += 10;
    textRect.right -= 0;
    textRect.bottom -= 0;
    DrawOutlinedText(hdc, textRect, std::to_string(value), RGB(0, 0, 0), RGB(255, 255, 255));
}

HBITMAP UIShortCutWnd::GetItemIcon(const ITEM_INFO& item)
{
    const unsigned int itemId = item.GetItemId();
    const auto found = m_itemIconCache.find(itemId);
    if (found != m_itemIconCache.end()) {
        return found->second;
    }

    HBITMAP bitmap = nullptr;
    for (const std::string& candidate : shopui::BuildItemIconCandidates(item)) {
        if (!g_fileMgr.IsDataExist(candidate.c_str())) {
            continue;
        }
        bitmap = shopui::LoadBitmapFromGameData(candidate);
        if (bitmap) {
            break;
        }
    }

    m_itemIconCache[itemId] = bitmap;
    return bitmap;
}

HBITMAP UIShortCutWnd::GetSkillIcon(int skillId)
{
    const auto found = m_skillIconCache.find(skillId);
    if (found != m_skillIconCache.end()) {
        return found->second;
    }

    HBITMAP bitmap = nullptr;
    const std::string path = ResolveShortcutSkillIconPath(skillId);
    if (!path.empty() && g_fileMgr.IsDataExist(path.c_str())) {
        bitmap = shopui::LoadBitmapFromGameData(path);
    }

    m_skillIconCache[skillId] = bitmap;
    return bitmap;
}

unsigned long long UIShortCutWnd::BuildDisplayStateToken() const
{
    unsigned long long hash = 1469598103934665603ull;
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_show));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_x));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_y));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_w));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_h));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_hoverSlot + 1));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(g_session.GetShortcutPage()));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_pressedSlot + 1));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_slotPressArmed));

    if (const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragType));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.source));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.shortcutSlotAbsoluteIndex + 1));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.itemId));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(static_cast<unsigned int>(gameMode->m_dragInfo.skillId)));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(static_cast<unsigned int>(gameMode->m_dragInfo.skillLevel)));
    }

    for (int slotIndex = 0; slotIndex < kShortcutSlotsPerPage; ++slotIndex) {
        const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByVisibleIndex(slotIndex);
        const unsigned char isSkill = slot ? slot->isSkill : 0;
        const unsigned int id = slot ? slot->id : 0;
        const unsigned short count = slot ? slot->count : 0;
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(isSkill));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(id));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(count));

        if (!slot || id == 0) {
            continue;
        }

        if (isSkill != 0) {
            const PLAYER_SKILL_INFO* skill = g_session.GetSkillItemBySkillId(static_cast<int>(id));
            shopui::HashTokenValue(&hash, static_cast<unsigned long long>(skill ? skill->level : count));
            shopui::HashTokenValue(&hash, static_cast<unsigned long long>(skill ? skill->spcost : 0));
        } else {
            const ITEM_INFO* item = g_session.GetInventoryItemByItemId(id);
            shopui::HashTokenValue(&hash, static_cast<unsigned long long>(item ? item->m_num : 0));
            shopui::HashTokenValue(&hash, static_cast<unsigned long long>(item ? item->m_itemIndex : 0));
        }
    }

    return hash;
}
