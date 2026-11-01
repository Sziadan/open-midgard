#include "UIShortCutWnd.h"

#include "UIShopCommon.h"
#include "UIWindowMgr.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "item/Item.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "render/DC.h"
#include "session/Session.h"
#include "skill/Skill.h"

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

#include <algorithm>
#include <string>

namespace {

constexpr int kDefaultWindowWidth = 275;
constexpr int kDefaultWindowHeight = 31;
constexpr int kShortcutWindowWidth = 295;
constexpr int kShortcutRowHeight = 29;
constexpr int kShortcutControlLeft = 270;
constexpr int kShortcutControlRight = 295;
constexpr int kMaximumVisibleRows = kShortcutPageCount;
constexpr int kSlotLeft = 5;
constexpr int kSlotTop = 4;
constexpr int kSlotStride = 29;
constexpr int kSlotSize = 24;

void DrawBitmapFit(HDC hdc, const shopui::BitmapPixels& bitmap, const RECT& rect)
{
    shopui::DrawBitmapPixelsTransparent(hdc, bitmap, rect);
}

#if RO_ENABLE_QT6_UI
QFont BuildShortCutFontFromHdc(HDC hdc)
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
    font.setPixelSize(logFont.lfHeight != 0 ? (std::max)(1, static_cast<int>(std::abs(logFont.lfHeight))) : 13);
    font.setBold(logFont.lfWeight >= FW_BOLD);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

void DrawShortCutTextPass(HDC hdc, const RECT& rect, const QString& text, COLORREF color)
{
    if (!hdc || text.isEmpty() || rect.right <= rect.left || rect.bottom <= rect.top) {
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
    painter.setFont(BuildShortCutFontFromHdc(hdc));
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(QRect(0, 0, width, height), Qt::AlignRight | Qt::AlignBottom | Qt::TextSingleLine, text);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

void DrawOutlinedText(HDC hdc, RECT rect, const std::string& text, COLORREF fillColor, COLORREF outlineColor = RGB(0, 0, 0))
{
    if (!hdc || text.empty()) {
        return;
    }

    SetBkMode(hdc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(hdc, shopui::GetUiFont());

#if RO_ENABLE_QT6_UI
    const QString label = QString::fromLocal8Bit(text.c_str());
    RECT shadowRect = rect;
    OffsetRect(&shadowRect, -1, 0);
    DrawShortCutTextPass(hdc, shadowRect, label, outlineColor);
    shadowRect = rect;
    OffsetRect(&shadowRect, 1, 0);
    DrawShortCutTextPass(hdc, shadowRect, label, outlineColor);
    shadowRect = rect;
    OffsetRect(&shadowRect, 0, -1);
    DrawShortCutTextPass(hdc, shadowRect, label, outlineColor);
    shadowRect = rect;
    OffsetRect(&shadowRect, 0, 1);
    DrawShortCutTextPass(hdc, shadowRect, label, outlineColor);
    DrawShortCutTextPass(hdc, rect, label, fillColor);
#else
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
#endif
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

std::string BuildShortcutSlotKeyLabel(int absoluteSlot)
{
    if (absoluteSlot < 0) {
        return std::string();
    }
    return g_windowMgr.GetShortcutKeyBindingLabel(absoluteSlot);
}

int ResolveShortcutSkillUseLevel(const SHORTCUT_SLOT& slot, const PLAYER_SKILL_INFO* skill)
{
    if (!skill || skill->level <= 0) return 0;
    return std::clamp(slot.count > 0 ? static_cast<int>(slot.count) : skill->level, 1, skill->level);
}

int ResolveShortcutSkillSpCost(int skillId, int useLevel, const PLAYER_SKILL_INFO* skill)
{
    if (useLevel <= 0) {
        return 0;
    }

    g_skillMgr.EnsureLoaded();
    if (const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillId)) {
        const size_t levelIndex = static_cast<size_t>(useLevel - 1);
        if (levelIndex < metadata->levelSpCosts.size()) {
            return metadata->levelSpCosts[levelIndex];
        }
    }

    if (skill && skill->spcost > 0) {
        return skill->spcost;
    }

    return 0;
}

std::string ResolveShortcutSkillName(int skillId, const PLAYER_SKILL_INFO* skill)
{
    if (skill) {
        if (!skill->skillName.empty()) {
            return skill->skillName;
        }
        if (!skill->skillIdName.empty()) {
            return skill->skillIdName;
        }
    }

    g_skillMgr.EnsureLoaded();
    if (const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillId)) {
        if (!metadata->displayName.empty()) {
            return metadata->displayName;
        }
        if (!metadata->skillIdName.empty()) {
            return metadata->skillIdName;
        }
    }

    return "Skill " + std::to_string(skillId);
}

std::string BuildShortcutSkillHoverText(int visibleSlot, const SHORTCUT_SLOT& slot)
{
    if (slot.id == 0 || slot.isSkill == 0) {
        return std::string();
    }

    const PLAYER_SKILL_INFO* skill = g_session.GetSkillItemBySkillId(static_cast<int>(slot.id));
    const std::string keyLabel = BuildShortcutSlotKeyLabel(visibleSlot);
    const std::string skillName = ResolveShortcutSkillName(static_cast<int>(slot.id), skill);
    const int useLevel = ResolveShortcutSkillUseLevel(slot, skill);
    const int spCost = ResolveShortcutSkillSpCost(static_cast<int>(slot.id), useLevel, skill);

    std::string text = keyLabel;
    if (!text.empty()) {
        text += " ";
    }
    text += skillName;
    text += " Use Lv ";
    text += std::to_string(useLevel);
    text += " (Sp: ";
    text += std::to_string(spCost);
    text += ")";
    return text;
}

std::string BuildShortcutItemHoverText(int visibleSlot, const ITEM_INFO& item)
{
    const std::string keyLabel = BuildShortcutSlotKeyLabel(visibleSlot);
    const std::string itemText = shopui::BuildItemHoverText(item);
    if (keyLabel.empty()) {
        return itemText;
    }
    if (itemText.empty()) {
        return keyLabel;
    }
    return keyLabel + " " + itemText;
}

} // namespace

UIShortCutWnd::UIShortCutWnd()
    : m_backgroundBitmap(),
      m_slotButtonBitmap(),
      m_hoverSlot(-1),
      m_visibleRowCount(1),
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
    Resize(kShortcutWindowWidth, kDefaultWindowHeight);

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
        const int targetAbsoluteSlot = GetAbsoluteSlotIndex(visibleSlot);
        if (targetAbsoluteSlot < 0) {
            return;
        }
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
        changed = g_session.SetShortcutSlotByAbsoluteIndex(GetAbsoluteSlotIndex(visibleSlot), 0, info->itemId, 0);
    } else if (info->type == static_cast<int>(DragType::ShortcutSkill) && info->skillId != 0) {
        const unsigned short skillLevel = static_cast<unsigned short>((std::max)(1, info->skillLevel));
        changed = g_session.SetShortcutSlotByAbsoluteIndex(GetAbsoluteSlotIndex(visibleSlot), 1, static_cast<unsigned int>(info->skillId), skillLevel);
    }

    if (changed) {
        const int absoluteSlot = GetAbsoluteSlotIndex(visibleSlot);
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

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    const RECT bounds{ m_x, m_y, m_x + m_w, m_y + m_h };
    if (m_backgroundBitmap.IsValid()) {
        shopui::DrawBitmapPixelsTransparent(hdc, m_backgroundBitmap, bounds);
    } else {
        shopui::FillRectColor(hdc, bounds, RGB(224, 224, 224));
        shopui::FrameRectColor(hdc, bounds, RGB(96, 96, 96));
    }

    if (m_slotButtonBitmap.IsValid() && m_hoverSlot >= 0) {
        DrawBitmapFit(hdc, m_slotButtonBitmap, GetSlotRect(m_hoverSlot));
    }

    for (int slotIndex = 0; slotIndex < m_visibleRowCount * kShortcutSlotsPerPage; ++slotIndex) {
        const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByAbsoluteIndex(GetAbsoluteSlotIndex(slotIndex));
        if (!slot || slot->id == 0) {
            continue;
        }

        const int absoluteSlot = GetAbsoluteSlotIndex(slotIndex);
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
            if (const shopui::BitmapPixels* icon = GetSkillIcon(static_cast<int>(slot->id))) {
                DrawBitmapFit(hdc, *icon, iconRect);
            }

            const PLAYER_SKILL_INFO* skill = g_session.GetSkillItemBySkillId(static_cast<int>(slot->id));
            const int displayedLevel = ResolveShortcutSkillUseLevel(*slot, skill);
            if (displayedLevel > 0) {
                DrawSlotOverlayText(hdc, slotRect, displayedLevel);
            }
        } else {
            ITEM_INFO fallbackItem{};
            fallbackItem.SetItemId(slot->id);
            fallbackItem.m_isIdentified = 1;
            const ITEM_INFO* item = g_session.GetInventoryItemByItemId(slot->id);
            const ITEM_INFO& iconSource = item ? *item : fallbackItem;
            if (const shopui::BitmapPixels* icon = GetItemIcon(iconSource)) {
                DrawBitmapFit(hdc, *icon, iconRect);
            }

            const int displayedCount = item ? item->m_num : 0;
            if (displayedCount > 0) {
                DrawSlotOverlayText(hdc, slotRect, displayedCount);
            }
        }
    }

    ReleaseDrawTarget(hdc);

    m_lastDrawStateToken = BuildDisplayStateToken();
    m_hasDrawStateToken = true;
    m_isDirty = 0;
}

void UIShortCutWnd::OnLBtnDown(int x, int y)
{
    const int localX = x - m_x;
    const int localY = y - m_y;
    if (localX >= kShortcutControlLeft && localX < kShortcutControlRight) {
        if (localY >= 0 && localY < kShortcutRowHeight && m_visibleRowCount < kMaximumVisibleRows) {
            ++m_visibleRowCount;
            Resize(kShortcutWindowWidth, m_visibleRowCount * kShortcutRowHeight + 2);
            Invalidate();
        } else if (m_visibleRowCount > 1
            && localY >= (m_visibleRowCount - 1) * kShortcutRowHeight
            && localY < m_visibleRowCount * kShortcutRowHeight) {
            --m_visibleRowCount;
            Resize(kShortcutWindowWidth, m_visibleRowCount * kShortcutRowHeight + 2);
            Invalidate();
        }
        return;
    }
    const int visibleSlot = GetSlotAtGlobalPoint(x, y);
    if (visibleSlot >= 0) {
        const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByAbsoluteIndex(GetAbsoluteSlotIndex(visibleSlot));
        if (slot && slot->id != 0) {
            m_pressedSlot = visibleSlot;
            m_pressedSlotAbsoluteIndex = GetAbsoluteSlotIndex(visibleSlot);
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

    const SHORTCUT_SLOT* const slot = g_session.GetShortcutSlotByAbsoluteIndex(GetAbsoluteSlotIndex(visibleSlot));
    if (!slot || slot->id == 0) {
        return;
    }

    if (slot->isSkill != 0) {
        const PLAYER_SKILL_INFO* const skill = g_session.GetSkillItemBySkillId(static_cast<int>(slot->id));
        if (skill) {
            g_windowMgr.ShowSkillDescribeWindow(*skill, x + 12, y + 12);
        }
        return;
    }

    ITEM_INFO fallbackItem{};
    fallbackItem.SetItemId(slot->id);
    fallbackItem.m_isIdentified = 1;
    const ITEM_INFO* const item = g_session.GetInventoryItemByItemId(slot->id);
    g_windowMgr.ShowItemInfoWindow(item ? *item : fallbackItem, x + 12, y + 12);
}

void UIShortCutWnd::OnWheel(int delta)
{
    // Rows are fixed banks; scrolling must not rotate their contents.
    (void)delta;
}

int UIShortCutWnd::GetHoverSlot() const
{
    return m_hoverSlot;
}

bool UIShortCutWnd::GetHoveredItemForQt(shopui::ItemHoverInfo* outData) const
{
    if (!outData || m_show == 0 || m_hoverSlot < 0) {
        return false;
    }

    const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByAbsoluteIndex(GetAbsoluteSlotIndex(m_hoverSlot));
    if (!slot) {
        return false;
    }

    outData->anchorRect = GetSlotRect(m_hoverSlot);
    outData->itemId = 0;
    outData->identified = true;

    if (slot->id == 0) {
        outData->text = BuildShortcutSlotKeyLabel(GetAbsoluteSlotIndex(m_hoverSlot));
        return outData->IsValid();
    }

    if (slot->isSkill != 0) {
        outData->text = BuildShortcutSkillHoverText(GetAbsoluteSlotIndex(m_hoverSlot), *slot);
        return outData->IsValid();
    }

    ITEM_INFO fallbackItem{};
    fallbackItem.SetItemId(slot->id);
    fallbackItem.m_isIdentified = 1;
    const ITEM_INFO* item = g_session.GetInventoryItemByItemId(slot->id);
    const ITEM_INFO& hoverItem = item ? *item : fallbackItem;

    outData->text = BuildShortcutItemHoverText(GetAbsoluteSlotIndex(m_hoverSlot), hoverItem);
    outData->itemId = hoverItem.GetItemId();
    outData->identified = hoverItem.m_isIdentified != 0;
    return outData->IsValid();
}

void UIShortCutWnd::LoadAssets()
{
    ReleaseAssets();
    m_backgroundBitmap = shopui::LoadBitmapPixelsFromGameData(shopui::ResolveUiAssetPath("shortitem_bg.bmp"), true);
    m_slotButtonBitmap = shopui::LoadBitmapPixelsFromGameData(shopui::ResolveUiAssetPath("shortitem_btn.bmp"), true);

    if (m_backgroundBitmap.IsValid()) {
        Resize(m_backgroundBitmap.width, m_backgroundBitmap.height);
    }
}

void UIShortCutWnd::ReleaseAssets()
{
    m_backgroundBitmap.Clear();
    m_slotButtonBitmap.Clear();
    m_itemIconCache.clear();
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
    if (localY < 0 || localY >= m_visibleRowCount * kShortcutRowHeight
        || (localY % kShortcutRowHeight) >= kSlotSize || localX <= 3 || localX >= kShortcutControlLeft) {
        return -1;
    }
    const int row = localY / kShortcutRowHeight;
    const int adjustedX = localX - 4;
    if (adjustedX < 0) {
        return -1;
    }

    const int slot = adjustedX / kSlotStride;
    if (slot < 0 || slot >= kShortcutSlotsPerPage || adjustedX >= (slot + 1) * kSlotStride) {
        return -1;
    }
    return row * kShortcutSlotsPerPage + slot;
}

RECT UIShortCutWnd::GetSlotRect(int visibleSlot) const
{
    const int row = visibleSlot / kShortcutSlotsPerPage;
    const int column = visibleSlot % kShortcutSlotsPerPage;
    return RECT{
        m_x + kSlotLeft + column * kSlotStride,
        m_y + kSlotTop + row * kShortcutRowHeight,
        m_x + kSlotLeft + column * kSlotStride + kSlotSize,
        m_y + kSlotTop + row * kShortcutRowHeight + kSlotSize
    };
}

void UIShortCutWnd::ActivateSlot(int visibleSlot)
{
    const int absoluteSlot = GetAbsoluteSlotIndex(visibleSlot);
    if (absoluteSlot >= 0) {
        g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShortcutUse, absoluteSlot, 0, 0);
    }
}

int UIShortCutWnd::GetAbsoluteSlotIndex(int visibleSlot) const
{
    if (visibleSlot < 0 || visibleSlot >= m_visibleRowCount * kShortcutSlotsPerPage) {
        return -1;
    }
    const int page = visibleSlot / kShortcutSlotsPerPage;
    return page * kShortcutSlotsPerPage + visibleSlot % kShortcutSlotsPerPage;
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

const shopui::BitmapPixels* UIShortCutWnd::GetItemIcon(const ITEM_INFO& item)
{
    const unsigned int itemId = item.GetItemId();
    const auto found = m_itemIconCache.find(itemId);
    if (found != m_itemIconCache.end()) {
        return found->second.IsValid() ? &found->second : nullptr;
    }

    shopui::BitmapPixels bitmap;
    for (const std::string& candidate : shopui::BuildItemIconCandidates(item)) {
        if (!g_fileMgr.IsDataExist(candidate.c_str())) {
            continue;
        }
        bitmap = shopui::LoadBitmapPixelsFromGameData(candidate, true);
        if (bitmap.IsValid()) {
            break;
        }
    }

    auto inserted = m_itemIconCache.emplace(itemId, std::move(bitmap));
    return inserted.first->second.IsValid() ? &inserted.first->second : nullptr;
}

const shopui::BitmapPixels* UIShortCutWnd::GetSkillIcon(int skillId)
{
    const auto found = m_skillIconCache.find(skillId);
    if (found != m_skillIconCache.end()) {
        return found->second.IsValid() ? &found->second : nullptr;
    }

    shopui::BitmapPixels bitmap;
    const std::string path = ResolveShortcutSkillIconPath(skillId);
    if (!path.empty() && g_fileMgr.IsDataExist(path.c_str())) {
        bitmap = shopui::LoadBitmapPixelsFromGameData(path, true);
    }

    auto inserted = m_skillIconCache.emplace(skillId, std::move(bitmap));
    return inserted.first->second.IsValid() ? &inserted.first->second : nullptr;
}

unsigned long long UIShortCutWnd::BuildDisplayStateToken() const
{
    unsigned long long hash = 1469598103934665603ull;
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_show));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_x));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_y));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_w));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_h));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_visibleRowCount));
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

    for (int slotIndex = 0; slotIndex < m_visibleRowCount * kShortcutSlotsPerPage; ++slotIndex) {
        const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByAbsoluteIndex(GetAbsoluteSlotIndex(slotIndex));
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
