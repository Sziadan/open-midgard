#include "UICompanionStatusWnd.h"

#include "UIShopCommon.h"
#include "UIWindowMgr.h"
#include "../core/SourceTextCodec.h"
#include "../item/Item.h"
#include "../session/Session.h"

#include <algorithm>
#include <array>
#include <ctime>

namespace {

constexpr int kClose = 135;
constexpr int kWidth = 280;
constexpr int kHeight = 180;

std::string HomunculusIntimacy(int intimacy)
{
    if (intimacy < 0 || intimacy > 1000) return "Unknown";
    if (intimacy <= 3) return "Very hateful";
    if (intimacy <= 10) return "Hateful";
    if (intimacy <= 100) return "Very awkward";
    if (intimacy <= 250) return "Awkward";
    if (intimacy <= 750) return "Normal";
    if (intimacy <= 910) return "Friendly";
    return "Very friendly";
}

const char* MercenaryTitle(int job)
{
    if (job >= 6017 && job <= 6026) return "Archer Mercenary";
    if (job >= 6027 && job <= 6036) return "Lancer Mercenary";
    if (job >= 6037 && job <= 6046) return "Sword Mercenary";
    return "Monster Mercenary";
}

std::string ContractEndTime(int expiresAt)
{
    if (expiresAt <= 0) return {};
    const std::time_t timestamp = expiresAt;
    const auto* localTime = std::localtime(&timestamp);
    char text[32]{};
    if (localTime) std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M", localTime);
    return text;
}

std::string DisplayName(const std::string& name)
{
    return ro::DecodeSourceText(name, ro::SelectedSourceTextCodePage()).value_or(name);
}

} // namespace

UICompanionStatusWnd::UICompanionStatusWnd(int windowId)
    : m_closeButton(new UIBitmapButton)
{
    m_id = windowId;
    Create(kWidth, kHeight);
    m_closeButton->m_id = kClose;
    m_closeButton->SetBitmapName("basic_interface\\sys_close_off.bmp", 0);
    m_closeButton->SetBitmapName("basic_interface\\sys_close_on.bmp", 1);
    m_closeButton->SetBitmapName("basic_interface\\sys_close_on.bmp", 2);
    m_closeButton->SetToolTip("Close");
    m_closeButton->Create(11, 11);
    m_closeButton->Move(m_w - 14, 3);
    AddChild(m_closeButton);
}

UICompanionStatusWnd::Display UICompanionStatusWnd::GetDisplay() const
{
    Display display;
    const auto addText = [&](int x, int y, int width, std::string value, bool rightAligned = false) {
        display.text.push_back({x, y, width, std::move(value), rightAligned});
    };
    const auto addGauge = [&](int x, int y, int width, int height, int value, int maxValue, COLORREF color) {
        const int filled = maxValue > 0
            ? static_cast<int>(static_cast<long long>(std::clamp(value, 0, maxValue)) * width / maxValue)
            : 0;
        display.gauges.push_back({x, y, width, height, filled, color});
    };
    const auto& homunculus = g_session.m_homunStatus;
    const auto& mercenary = g_session.m_merStatus;
    const bool homunculusWindow = m_id == UIWindowMgr::WID_HOMUNINFOWND;
    const auto& combat = homunculusWindow ? homunculus.combat : mercenary.combat;
    const std::array<const char*, 8> labels{"ATK", "MATK", "HIT", "CRI", "DEF", "MDEF", "FLEE", "ASPD"};
    const std::array<int, 8> values{
        combat.attack, combat.magicAttack, combat.hit, combat.critical,
        combat.defense, combat.magicDefense, combat.flee,
        static_cast<int>((2000LL - combat.attackSpeed) / 10)
    };
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const int y = 24 + static_cast<int>(i) * 18;
        addText(8, y, 34, labels[i]);
        addText(43, y, 44, std::to_string(values[i]), true);
    }

    addText(5, 2, 246, homunculusWindow ? "Homunculus" : MercenaryTitle(mercenary.job));
    addText(100, 24, 34, "Name");
    addText(135, 24, 137, DisplayName(homunculusWindow ? homunculus.name : mercenary.name));
    addText(100, 47, 34, "Level");
    addText(135, 47, 72, std::to_string(homunculusWindow ? homunculus.level : mercenary.level));
    addText(100, 70, 17, "HP");
    addText(100, 85, 17, "SP");
    addText(204, 70, 72, std::to_string(combat.hp) + " / " + std::to_string(combat.maxHp), true);
    addText(204, 85, 72, std::to_string(combat.sp) + " / " + std::to_string(combat.maxSp), true);
    addGauge(117, 73, 85, 9, combat.hp, combat.maxHp, RGB(66, 160, 55));
    addGauge(117, 88, 85, 9, combat.sp, combat.maxSp, RGB(65, 113, 200));

    if (homunculusWindow) {
        addText(100, 105, 40, "EXP");
        addText(140, 105, 63, std::to_string(homunculus.nextExperience), true);
        addGauge(101, 119, 102, 6, homunculus.experience, homunculus.nextExperience, RGB(209, 152, 50));
        addText(100, 131, 47, "Hunger");
        addText(147, 131, 56, std::to_string(homunculus.hunger) + " / 100", true);
        addGauge(101, 145, 102, 6, homunculus.hunger, 100, RGB(209, 152, 50));
        addText(216, 105, 60, "Accessory");
        ITEM_INFO accessory{};
        if (homunculus.accessory) accessory.SetItemId(homunculus.accessory);
        addText(216, 130, 60, homunculus.accessory ? accessory.GetItemName() : "Unequipped");
        addText(100, 159, 54, "Intimacy");
        addText(155, 159, 120, HomunculusIntimacy(homunculus.intimacy));
    } else {
        addText(100, 111, 55, "Expires");
        addText(155, 111, 121, ContractEndTime(mercenary.expiresAt));
        addText(100, 135, 65, "Faith");
        addText(166, 135, 47, std::to_string(mercenary.faith), true);
        addText(100, 159, 65, "Calls");
        addText(166, 159, 47, std::to_string(mercenary.calls), true);
        addText(230, 135, 46, "Kills");
        addText(230, 159, 46, std::to_string(mercenary.kills), true);
    }
    return display;
}

void UICompanionStatusWnd::Move(int x, int y)
{
    m_closeButton->Move(x + m_w - 14, y + 3);
    UIFrameWnd::Move(x, y);
}

void UICompanionStatusWnd::OnDraw()
{
    HDC dc = AcquireDrawTarget();
    if (!dc) return;
    const auto display = GetDisplay();
    shopui::FillRectColor(dc, {m_x, m_y, m_x + m_w, m_y + m_h}, RGB(255, 255, 255));
    shopui::FillRectColor(dc, {m_x, m_y, m_x + m_w, m_y + 18}, RGB(222, 230, 241));
    for (const auto& gauge : display.gauges) {
        const int x = m_x + gauge.x;
        const int y = m_y + gauge.y;
        shopui::FillRectColor(dc, {x, y, x + gauge.width, y + gauge.height}, RGB(228, 229, 232));
        if (gauge.filledWidth)
            shopui::FillRectColor(dc, {x, y, x + gauge.filledWidth, y + gauge.height}, gauge.color);
    }
    for (const auto& row : display.text) {
        const int x = m_x + row.x;
        const int y = m_y + row.y;
        shopui::DrawWindowTextRect(dc, {x, y, x + row.width, y + 16}, row.value, RGB(0, 0, 0),
            DT_TOP | DT_SINGLELINE | DT_NOPREFIX | (row.rightAligned ? DT_RIGHT : DT_LEFT));
    }
    DrawChildrenToHdc(dc);
    ReleaseDrawTarget(dc);
}

msgresult_t UICompanionStatusWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    if (msg == 6 && wparam == kClose) {
        g_windowMgr.DeleteWindow(this);
        return 1;
    }
    return UIFrameWnd::SendMsg(sender, msg, wparam, lparam, extra);
}
