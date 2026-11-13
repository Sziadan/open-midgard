#include "UICompanionSkillListWnd.h"

#include "UIFriendOptionWnd.h"
#include "UIShopCommon.h"
#include "UIWindowMgr.h"
#include "core/UiMessageStrings.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "session/Session.h"
#include "skill/Skill.h"

#include <algorithm>
#include <iterator>

namespace {
constexpr int kTitleHeight = 17;
constexpr int kRowHeight = 32;
constexpr int kVisibleRows = 2;
constexpr int kClose = 135;
constexpr int kUpgradeX = 78;
constexpr int kUpgradeYOffset = 4;
constexpr int kUpgradeSize = 24;
}

UICompanionSkillListWnd::UICompanionSkillListWnd(int windowId)
    : m_family(windowId == UIWindowMgr::WID_HOMUNSKILLLISTWND ? 1 : 2),
      m_close(new UIFriendOptionButton(kClose, "basic_interface\\sys_close_off.bmp",
          "basic_interface\\sys_close_on.bmp", "basic_interface\\sys_close_on.bmp", 11, 11))
{
    m_id = windowId;
    ro::UiMessageStrings strings;
    strings.Load();
    m_title = m_family == 1 ? strings.Get(937, "Homunculus Skill List")
                           : strings.Get(1262, "Mercenary Skill List");
    m_pointsFormat = strings.Get(284, "Skill Point: %d");
    Create(280, 120);
    m_close->Move(m_w - 14, 3);
    m_close->SetToolTip("Close");
    AddChild(m_close);
    if (m_family == 1) {
        const char* paths[] = {"basic_interface\\skill_up_a.bmp", "basic_interface\\skill_up_c.bmp"};
        for (std::size_t i = 0; i < m_upgradeArt.size(); ++i) {
            m_upgradeArt[i].pixels = LoadChatRoomSkinBitmap(paths[i]);
            m_upgradeArt[i].uri = ChatRoomBitmapUri(m_upgradeArt[i].pixels);
        }
    }
}

const std::list<PLAYER_SKILL_INFO>& UICompanionSkillListWnd::Skills() const
{
    return m_family == 1 ? g_session.GetHomunSkillItems() : g_session.GetMercSkillItems();
}

int UICompanionSkillListWnd::MaxOffset() const
{
    return std::max(0, static_cast<int>(Skills().size()) - kVisibleRows);
}

const PLAYER_SKILL_INFO* UICompanionSkillListWnd::SkillAt(int x, int y) const
{
    const int rowY = y - m_y - kTitleHeight;
    if (x < m_x || x >= m_x + m_w || rowY < 0 || rowY >= kRowHeight * kVisibleRows) return nullptr;
    const int index = std::min(m_offset, MaxOffset()) + rowY / kRowHeight;
    const auto& skills = Skills();
    return index < static_cast<int>(skills.size()) ? &*std::next(skills.begin(), index) : nullptr;
}

bool UICompanionSkillListWnd::CanUpgrade(const PLAYER_SKILL_INFO& skill) const
{
    return m_family == 1 && g_session.m_homunStatus.skillPoints > 0
        && skill.m_isValid && skill.upgradable;
}

const PLAYER_SKILL_INFO* UICompanionSkillListWnd::UpgradeAt(int x, int y) const
{
    const auto* skill = SkillAt(x, y);
    if (!skill || !CanUpgrade(*skill)) return nullptr;
    const int localX = x - m_x - kUpgradeX;
    const int rowY = (y - m_y - kTitleHeight) % kRowHeight;
    return localX >= 0 && localX < kUpgradeSize
        && rowY >= kUpgradeYOffset && rowY < kUpgradeYOffset + kUpgradeSize ? skill : nullptr;
}

int UICompanionSkillListWnd::UseLevel(int skillId) const
{
    return m_family == 1 ? g_session.GetHomunSkillUseLevel(skillId) : g_session.GetMerSkillUseLevel(skillId);
}

const UICompanionSkillListWnd::Icon* UICompanionSkillListWnd::GetIcon(int skillId)
{
    const auto [entry, inserted] = m_icons.try_emplace(skillId);
    if (inserted) {
        const auto path = g_skillMgr.GetSkillIconPath(skillId);
        if (!path.empty()) {
            entry->second.pixels = shopui::LoadBitmapPixelsFromGameData(path, true);
            entry->second.uri = ChatRoomBitmapUri(entry->second.pixels);
        }
    }
    return &entry->second;
}

std::vector<UICompanionSkillListWnd::Row> UICompanionSkillListWnd::GetRows()
{
    const auto& skills = Skills();
    auto entry = std::next(skills.begin(), std::min(m_offset, MaxOffset()));
    std::vector<Row> rows;
    for (int i = 0; i < kVisibleRows && entry != skills.end(); ++i, ++entry) {
        rows.push_back({entry->SKID, kTitleHeight + i * kRowHeight, entry->level,
            entry->spcost, entry->SKID == m_selectedSkill, entry->skillName, GetIcon(entry->SKID),
            CanUpgrade(*entry) ? &m_upgradeArt[entry->SKID == m_pressedUpgradeSkill ? 1 : 0] : nullptr,
            kUpgradeX, kUpgradeYOffset, kUpgradeSize});
    }
    return rows;
}

std::string UICompanionSkillListWnd::GetTitle() const
{
    return m_title;
}

std::string UICompanionSkillListWnd::GetFooter() const
{
    auto caption = m_pointsFormat;
    const auto placeholder = caption.find("%d");
    if (placeholder != std::string::npos) {
        const int points = m_family == 1 ? g_session.m_homunStatus.skillPoints : 0;
        caption.replace(placeholder, 2, std::to_string(points));
    }
    return caption;
}

void UICompanionSkillListWnd::Move(int x, int y)
{
    m_close->Move(x + m_w - 14, y + 3);
    UIFrameWnd::Move(x, y);
}

void UICompanionSkillListWnd::OnDraw()
{
    HDC dc = AcquireDrawTarget();
    if (!dc) return;
    shopui::FillRectColor(dc, {m_x, m_y, m_x + m_w, m_y + m_h}, RGB(255, 255, 255));
    shopui::FillRectColor(dc, {m_x, m_y, m_x + m_w, m_y + kTitleHeight}, RGB(222, 230, 241));
    const auto text = [&](int x, int y, int width, const std::string& value) {
        shopui::DrawWindowTextRect(dc, {m_x + x, m_y + y, m_x + x + width, m_y + y + 18},
            value, RGB(0, 0, 0), DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    };
    text(16, 2, m_w - 35, GetTitle());
    text(16, m_h - 20, m_w - 32, GetFooter());
    for (const auto& row : GetRows()) {
        if (row.selected)
            shopui::FillRectColor(dc, {m_x + 2, m_y + row.y, m_x + m_w - 2, m_y + row.y + kRowHeight},
                RGB(225, 235, 252));
        const auto& pixels = row.icon->pixels;
        shopui::DrawBitmapPixelsTransparent(dc, pixels,
            {m_x + 44, m_y + row.y + 4, m_x + 68, m_y + row.y + 28});
        text(112, row.y + 4, 164, row.name);
        text(112, row.y + 17, 58, "Lv " + std::to_string(row.level));
        text(175, row.y + 17, 83, "SP " + std::to_string(row.spCost));
        if (row.upgrade) {
            const int x = m_x + row.upgradeX;
            const int y = m_y + row.y + row.upgradeY;
            const RECT bounds{x, y, x + row.upgradeSize, y + row.upgradeSize};
            if (row.upgrade->pixels.IsValid()) {
                shopui::DrawBitmapPixelsTransparent(dc, row.upgrade->pixels, bounds);
            } else {
                shopui::FillRectColor(dc, bounds, RGB(232, 232, 232));
                text(row.upgradeX + 7, row.y + row.upgradeY + 4, 12, "+");
            }
        }
    }
    DrawChildrenToHdc(dc);
    ReleaseDrawTarget(dc);
}

void UICompanionSkillListWnd::OnLBtnDown(int x, int y)
{
    m_dragSkill = 0;
    m_pressedUpgradeSkill = 0;
    if (const auto* upgrade = UpgradeAt(x, y)) {
        m_pressedUpgradeSkill = upgrade->SKID;
        Invalidate();
        return;
    }
    if (const auto* row = SkillAt(x, y)) {
        m_selectedSkill = row->SKID;
        if (row->m_isValid && row->level && row->type) {
            m_dragSkill = row->SKID;
            m_dragStart = {x, y};
        }
        Invalidate();
    } else {
        UIFrameWnd::OnLBtnDown(x, y);
    }
}

void UICompanionSkillListWnd::OnLBtnDblClk(int x, int y)
{
    if (UpgradeAt(x, y)) return;
    if (const auto* row = SkillAt(x, y)) {
        m_selectedSkill = row->SKID;
        ArmPendingSkillUseFromSkillList(row->SKID, m_family);
        Invalidate();
    } else {
        UIFrameWnd::OnLBtnDown(x, y);
    }
}

void UICompanionSkillListWnd::OnLBtnUp(int x, int y)
{
    m_dragSkill = 0;
    if (m_pressedUpgradeSkill) {
        const int pressedSkill = m_pressedUpgradeSkill;
        m_pressedUpgradeSkill = 0;
        const auto* row = UpgradeAt(x, y);
        auto* mode = g_modeMgr.GetCurrentGameMode();
        if (row && row->SKID == pressedSkill && mode && !mode->m_isReqUpgradeSkillLevel)
            mode->SendMsg(CGameMode::GameMsg_RequestUpgradeSkillLevel, pressedSkill, 0, 0);
        Invalidate();
        return;
    }
    UIFrameWnd::OnLBtnUp(x, y);
}

void UICompanionSkillListWnd::OnMouseMove(int x, int y)
{
    UIFrameWnd::OnMouseMove(x, y);
    if (!m_dragSkill) return;
    const auto dx = static_cast<long long>(x) - m_dragStart.x;
    const auto dy = static_cast<long long>(y) - m_dragStart.y;
    if (dx * dx + dy * dy < 16) return;
    if (auto* mode = g_modeMgr.GetCurrentGameMode()) {
        const auto& skills = Skills();
        const auto skill = std::find_if(skills.begin(), skills.end(),
            [&](const auto& row) { return row.SKID == m_dragSkill; });
        if (skill != skills.end() && skill->m_isValid && skill->level && skill->type) {
            mode->m_dragType = static_cast<int>(DragType::ShortcutSkill);
            mode->m_dragInfo = {};
            mode->m_dragInfo.type = mode->m_dragType;
            mode->m_dragInfo.source = static_cast<int>(DragSource::SkillListWindow);
            mode->m_dragInfo.skillId = skill->SKID;
            mode->m_dragInfo.skillLevel = UseLevel(skill->SKID);
            Invalidate();
        }
    }
    m_dragSkill = 0;
}

void UICompanionSkillListWnd::OnRBtnDown(int x, int y)
{
    if (const auto* row = SkillAt(x, y))
        g_windowMgr.ShowSkillDescribeWindow(*row, x + 12, y + 12);
}

void UICompanionSkillListWnd::OnWheel(int delta)
{
    if (!delta) return;
    m_offset = std::clamp(m_offset + (delta > 0 ? -1 : 1), 0, MaxOffset());
    Invalidate();
}

msgresult_t UICompanionSkillListWnd::SendMsg(UIWindow* sender, int msg, msgparam_t action, msgparam_t lparam, msgparam_t extra)
{
    if (msg == 6 && action == kClose) {
        g_windowMgr.DeleteWindow(this);
        return 1;
    }
    return UIFrameWnd::SendMsg(sender, msg, action, lparam, extra);
}
