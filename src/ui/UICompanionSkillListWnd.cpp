#include "UICompanionSkillListWnd.h"

#include "UIFriendOptionWnd.h"
#include "UIShopCommon.h"
#include "UIWindowMgr.h"
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
}

UICompanionSkillListWnd::UICompanionSkillListWnd(int windowId)
    : m_family(windowId == UIWindowMgr::WID_HOMUNSKILLLISTWND ? 1 : 2),
      m_close(new UIFriendOptionButton(kClose, "basic_interface\\sys_close_off.bmp",
          "basic_interface\\sys_close_on.bmp", "basic_interface\\sys_close_on.bmp", 11, 11))
{
    m_id = windowId;
    Create(280, 120);
    m_close->Move(m_w - 14, 3);
    m_close->SetToolTip("Close");
    AddChild(m_close);
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
            entry->spcost, entry->SKID == m_selectedSkill, entry->skillName, GetIcon(entry->SKID)});
    }
    return rows;
}

std::string UICompanionSkillListWnd::GetTitle() const
{
    return m_family == 1 ? "Homunculus Skill List" : "Mercenary Skill List";
}

std::string UICompanionSkillListWnd::GetFooter() const
{
    if (m_family == 1) return "Skill points " + std::to_string(g_session.m_homunStatus.skillPoints);
    return {};
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
            {m_x + 6, m_y + row.y + 4, m_x + 30, m_y + row.y + 28});
        text(37, row.y + 7, 135, row.name);
        text(177, row.y + 7, 42, "Lv " + std::to_string(row.level));
        text(224, row.y + 7, 53, "SP " + std::to_string(row.spCost));
    }
    DrawChildrenToHdc(dc);
    ReleaseDrawTarget(dc);
}

void UICompanionSkillListWnd::OnLBtnDown(int x, int y)
{
    m_dragSkill = 0;
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
