#include "UIQuestWnd.h"

#include "UIShopCommon.h"
#include "UIWindowMgr.h"
#include "core/SourceTextCodec.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "session/Session.h"

#include <ctime>
#include <iterator>

namespace {
constexpr int kWidth = 540;
constexpr int kHeight = 300;
constexpr int kListWidth = 205;
constexpr int kFirstRow = 34;
constexpr int kRowHeight = 22;
constexpr int kRows = 10;
constexpr int kFooter = 268;
constexpr int kDetails = 219;
constexpr int kClose = -1;
constexpr int kPrevious = -2;
constexpr int kNext = -3;
constexpr int kActivity = -4;

std::string QuestText(const std::string& text)
{
    return ro::DecodeSourceText(text, ro::SelectedSourceTextCodePage()).value_or(text);
}

std::string QuestExpiry(u32 seconds)
{
    if (!seconds) return {};
    const auto timestamp = static_cast<std::time_t>(seconds);
    const auto* local = std::localtime(&timestamp);
    char buffer[64]{};
    if (!local || !std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", local)) return {};
    return std::string(timestamp <= std::time(nullptr) ? "Expired " : "Expires ") + buffer;
}
}

UIQuestWnd::UIQuestWnd(int windowId)
{
    m_id = windowId;
    Create(kWidth, kHeight);
    const auto viewport = g_windowMgr.GetChatClientRect();
    Move(viewport.left + (std::max<LONG>)(0, (viewport.right - viewport.left - kWidth) / 2),
        viewport.top + (std::max<LONG>)(0, (viewport.bottom - viewport.top - kHeight) / 2));
    m_catalog.Load();
}

std::size_t UIQuestWnd::PageOffset() const
{
    const auto size = g_session.m_questJournal.states.size();
    return size ? (std::min)(m_offset, (size - 1) / kRows * kRows) : 0;
}

u32 UIQuestWnd::SelectedId() const
{
    const auto& states = g_session.m_questJournal.states;
    if (states.count(m_selectedId)) return m_selectedId;
    return states.empty() ? 0 : std::next(states.begin(), PageOffset())->first;
}

std::string UIQuestWnd::Title(u32 id) const
{
    const auto* entry = m_catalog.Find(id);
    return entry && !entry->title.empty() ? QuestText(entry->title) : "Quest " + std::to_string(id);
}

UIQuestWnd::Display UIQuestWnd::GetDisplay() const
{
    const auto& journal = g_session.m_questJournal;
    const auto offset = PageOffset();
    const u32 selected = SelectedId();
    Display display;
    display.previous = offset > 0;
    display.next = offset + kRows < journal.states.size();
    const auto first = std::next(journal.states.begin(), offset);
    for (auto it = first; it != journal.states.end() && display.rows.size() < kRows; ++it) {
        display.rows.push_back({it->first, Title(it->first), it->second == ro::quest::State::Active,
            it->first == selected});
    }
    if (!selected) {
        display.title = "No quests";
        return display;
    }
    display.title = Title(selected);
    display.action = journal.states.at(selected) == ro::quest::State::Active ? "Deactivate" : "Activate";
    if (const auto* entry = m_catalog.Find(selected)) {
        display.description = QuestText(entry->summary);
        if (!entry->objective.empty()) {
            if (!display.description.empty()) display.description += "\n";
            display.description += QuestText(entry->objective);
        }
    }
    if (const auto it = journal.missions.find(selected); it != journal.missions.end()) {
        const auto& mission = it->second;
        display.expiry = QuestExpiry(mission.expiresAt);
        for (u16 i = 0; i < mission.objectiveCount; ++i) {
            const auto& objective = mission.objectives[i];
            std::string row = objective.name.empty() ? "Objective " + std::to_string(i + 1) : QuestText(objective.name);
            row += "  " + std::to_string(objective.currentCount);
            if (objective.requiredCount) row += " / " + std::to_string(*objective.requiredCount);
            display.objectives.push_back(std::move(row));
        }
    }
    return display;
}

bool UIQuestWnd::IsUpdateNeed()
{
    return m_show != 0;
}

void UIQuestWnd::OnDraw()
{
    HDC dc = AcquireDrawTarget();
    if (!dc) return;
    const auto display = GetDisplay();
    const auto fill = [&](RECT bounds, COLORREF color) {
        OffsetRect(&bounds, m_x, m_y);
        shopui::FillRectColor(dc, bounds, color);
    };
    const auto draw = [&](const std::string& text, RECT bounds, UINT flags) {
        OffsetRect(&bounds, m_x, m_y);
        shopui::DrawWindowTextRect(dc, bounds, text, RGB(28, 35, 46), flags | DT_NOPREFIX);
    };
    constexpr UINT single = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS;
    fill({0, 0, kWidth, kHeight}, RGB(255, 255, 255));
    fill({0, 0, kWidth, 18}, RGB(222, 230, 241));
    fill({0, 18, kListWidth, kHeight}, RGB(243, 246, 250));
    draw("Quest Journal", {8, 0, kWidth - 22, 18}, single);
    draw("x", {kWidth - 18, 0, kWidth, 18}, single | DT_CENTER);
    int y = kFirstRow;
    for (const auto& row : display.rows) {
        if (row.selected) fill({4, y, kListWidth - 4, y + kRowHeight}, RGB(215, 230, 245));
        const std::string text = row.active ? row.title : row.title + " (Inactive)";
        draw(text, {9, y, kListWidth - 8, y + kRowHeight}, single);
        y += kRowHeight;
    }
    draw(display.previous ? "Previous" : "", {8, kFooter, 86, 294}, single);
    draw(display.next ? "Next" : "", {110, kFooter, kListWidth - 8, 294}, single);
    draw(display.title, {kDetails, 24, kWidth - 8, 47}, single);
    draw(display.expiry, {kDetails, 47, kWidth - 8, 67}, single);
    draw(display.description, {kDetails, 72, kWidth - 10, 185}, DT_WORDBREAK | DT_END_ELLIPSIS);
    y = 190;
    for (const auto& objective : display.objectives) {
        draw(objective, {kDetails, y, kWidth - 8, y + 20}, single);
        y += 20;
    }
    if (!display.action.empty()) {
        fill({kDetails, kFooter, kDetails + 100, 294}, RGB(222, 230, 241));
        draw(display.action, {kDetails, kFooter, kDetails + 100, 294}, single | DT_CENTER);
    }
    ReleaseDrawTarget(dc);
}

int UIQuestWnd::Hit(int x, int y) const
{
    x -= m_x;
    y -= m_y;
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return 0;
    if (y < 18 && x >= kWidth - 18) return kClose;
    if (x < kListWidth && y >= kFirstRow && y < kFirstRow + kRows * kRowHeight)
        return (y - kFirstRow) / kRowHeight + 1;
    if (y < kFooter || y >= 294) return 0;
    if (x >= 8 && x < 86) return kPrevious;
    if (x >= 110 && x < kListWidth - 8) return kNext;
    if (x >= kDetails && x < kDetails + 100) return kActivity;
    return 0;
}

void UIQuestWnd::OnLBtnDown(int x, int y)
{
    m_pressed = Hit(x, y);
    m_pressedId = m_pressed == kActivity ? SelectedId() : 0;
    if (m_pressedId) {
        m_requestedActive = g_session.m_questJournal.states.at(m_pressedId) == ro::quest::State::Inactive;
    }
    if (!m_pressed) UIFrameWnd::OnLBtnDown(x, y);
}

void UIQuestWnd::OnLBtnUp(int x, int y)
{
    const int pressed = m_pressed;
    m_pressed = 0;
    UIFrameWnd::OnLBtnUp(x, y);
    if (pressed != Hit(x, y)) return;
    if (pressed == kClose) SetShow(0);
    if (pressed == kPrevious) OnWheel(1);
    if (pressed == kNext) OnWheel(-1);
    const auto& states = g_session.m_questJournal.states;
    if (pressed > 0 && PageOffset() + pressed <= states.size()) {
        m_selectedId = std::next(states.begin(), PageOffset() + pressed - 1)->first;
    }
    if (pressed == kActivity && m_pressedId && m_pressedId == SelectedId()) {
        g_modeMgr.SendMsg(CGameMode::GameMsg_RequestQuestActive, m_pressedId, m_requestedActive, 0);
    }
}

void UIQuestWnd::OnWheel(int delta)
{
    const auto offset = PageOffset();
    if (delta > 0 && offset >= kRows) m_offset = offset - kRows;
    else if (delta < 0 && offset + kRows < g_session.m_questJournal.states.size()) m_offset = offset + kRows;
    else return;
    m_selectedId = 0;
}
