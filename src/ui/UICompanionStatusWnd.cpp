#include "UICompanionStatusWnd.h"

#include "UIShopCommon.h"
#include "UIFriendOptionWnd.h"
#include "UIWindowMgr.h"
#include "../core/SourceTextCodec.h"
#include "../core/UiMessageStrings.h"
#include "../gamemode/Mode.h"
#include "../item/Item.h"
#include "../session/Session.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <ctime>

namespace {

constexpr int kClose = 135;
constexpr int kRename = 118;
constexpr int kFeed = 224;
constexpr int kDismissHomunculus = 225;
constexpr int kDismissMercenary = 284;
constexpr int kHomunculusSkills = 223;
constexpr int kMercenarySkills = 283;
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

std::string PromptText(int id, const char* fallback)
{
    ro::UiMessageStrings strings;
    strings.Load();
    auto text = DisplayName(strings.Get(id, fallback));
    // The shared confirmation box renders plain text.
    for (auto pos = text.find('^'); pos != std::string::npos; pos = text.find('^', pos)) {
        if (pos + 7 <= text.size()
            && std::all_of(text.begin() + pos + 1, text.begin() + pos + 7,
                [](unsigned char ch) { return std::isxdigit(ch) != 0; })) {
            text.erase(pos, 7);
        } else {
            ++pos;
        }
    }
    return text;
}

class CompanionNameEdit final : public UIEditCtrl {
public:
    void OnChar(char c) override
    {
        if (c != '\b') {
            UIEditCtrl::OnChar(c);
        } else if (!m_text.empty()) {
            const int codePage = ro::SelectedSourceTextCodePage();
            do {
                m_text.pop_back();
            } while (!m_text.empty() && !ro::DecodeSourceText(m_text, codePage));
            Invalidate();
        }
    }

    void OnKeyDown(int key) override
    {
        if (key == VK_BACK) OnChar('\b');
    }
};

} // namespace

UICompanionStatusWnd::UICompanionStatusWnd(int windowId)
{
    m_id = windowId;
    Create(kWidth, kHeight);
    const auto addButton = [&](UIFriendOptionButton* button, int x, int y, const char* label) {
        button->Move(x, y);
        button->SetToolTip(label);
        m_buttons.push_back(button);
        AddChild(button);
    };
    const auto addAction = [&](int id, const std::string& asset, int x, int y, const char* label) {
        addButton(new UIFriendOptionButton(id, (asset + ".bmp").c_str(), (asset + "_a.bmp").c_str(),
            (asset + "_b.bmp").c_str(), 40, 20), x, y, label);
    };
    addButton(new UIFriendOptionButton(kClose, "basic_interface\\sys_close_off.bmp",
        "basic_interface\\sys_close_on.bmp", "basic_interface\\sys_close_on.bmp", 11, 11),
        m_w - 14, 3, "Close");
    if (windowId == UIWindowMgr::WID_HOMUNINFOWND) {
        addAction(kDismissHomunculus, "btn_del", 187, 44, "Delete");
        addAction(kHomunculusSkills, "btn_skill", 232, 44, "Skills");
        addAction(kFeed, "btn_feed", 210, 155, "Feed");
        addAction(kRename, "btn_rewrite", 232, 21, "Rename");
        m_nameEdit = new CompanionNameEdit;
        m_nameEdit->Create(80, 16);
        m_nameEdit->Move(135, 24);
        m_nameEdit->m_maxchar = 50;
        m_nameEdit->SetFrameColor(232, 232, 232);
        AddChild(m_nameEdit);
        RefreshReceivedName();
    } else {
        addAction(kDismissMercenary, "btn_fired", 187, 44, "Dismiss");
        addAction(kMercenarySkills, "btn_skill", 232, 44, "Skills");
    }
}

std::string UICompanionStatusWnd::GetEditDisplayText() const
{
    return m_nameEdit ? DisplayName(m_nameEdit->GetText()) : std::string{};
}

void UICompanionStatusWnd::RefreshReceivedName()
{
    if (!m_nameEdit) return;
    const auto& status = g_session.m_homunStatus;
    m_nameEdit->SetText(status.name.c_str());
    const bool canRename = !status.nameModified;
    m_nameEdit->SetShow(canRename);
    m_buttons.back()->SetShow(canRename);
    if (!canRename && g_windowMgr.m_editWindow == m_nameEdit)
        g_windowMgr.m_editWindow = nullptr;
    Invalidate();
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
    if (!GetNameEdit())
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
    const int dx = x - m_x;
    const int dy = y - m_y;
    UIFrameWnd::Move(x, y);
    for (auto* child : m_children) child->Move(child->m_x + dx, child->m_y + dy);
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
    if (msg == 6) {
        if (wparam == kClose) {
            g_windowMgr.DeleteWindow(this);
            return 1;
        }
        if (wparam == kRename && GetNameEdit()) {
            const std::string name = m_nameEdit->GetText();
            if (name.size() >= 24) {
                ShowPrompt(938, PromptText(938, "Please give your Homunculus a name no longer than 23 letters."),
                    false, wparam);
            } else {
                auto text = PromptText(939, "You can name a Homunculus only once. Use the name %s?");
                if (const auto pos = text.find("%s"); pos != std::string::npos)
                    text.replace(pos, 2, DisplayName(name));
                ShowPrompt(939, std::move(text), true, wparam);
            }
            return 1;
        }
        if (m_id == UIWindowMgr::WID_HOMUNINFOWND) {
            if (wparam == kHomunculusSkills) {
                g_windowMgr.ToggleWindow(UIWindowMgr::WID_HOMUNSKILLLISTWND);
                return 1;
            }
            if (wparam == kFeed) {
                ShowPrompt(601, PromptText(601, "Are you sure that you want to feed your pet?"), true, wparam);
                return 1;
            }
            if (wparam == kDismissHomunculus) {
                ShowPrompt(955, PromptText(955, "Permanently delete this Homunculus?"), true, wparam);
                return 1;
            }
        } else if (wparam == kMercenarySkills) {
            g_windowMgr.ToggleWindow(UIWindowMgr::WID_MERSKILLLISTWND);
            return 1;
        } else if (wparam == kDismissMercenary) {
            ShowPrompt(1370, PromptText(1370, "Delete this mercenary and its growth history?"), true, wparam);
            return 1;
        }
    }
    return UIFrameWnd::SendMsg(sender, msg, wparam, lparam, extra);
}

void UICompanionStatusWnd::ShowPrompt(int messageId, std::string text, bool confirmation, msgparam_t action)
{
    if (g_windowMgr.m_modalWindow) return;
    const auto generation = g_modeMgr.GetOperationGeneration();
    const bool homunculus = m_id == UIWindowMgr::WID_HOMUNINFOWND;
    const auto gid = homunculus ? g_session.m_homunGID : g_session.m_merGID;
    auto* box = new UIChatRoomMessageBox(std::move(text), messageId);
    box->SetSourceOwner(this);
    if (confirmation) {
        box->ConfigureConfirmation(this, [this, generation, homunculus, gid, action](bool accepted) {
            if (accepted && g_modeMgr.GetOperationGeneration() == generation
                && gid == (homunculus ? g_session.m_homunGID : g_session.m_merGID)
                && g_windowMgr.ContainsWindow(this)) {
                ApplyConfirmedAction(action);
            }
        });
    }
    g_windowMgr.m_roomMessageBox = box;
    g_windowMgr.m_children.push_back(box);
    g_windowMgr.m_modalWindow = box;
    g_windowMgr.m_editWindow = nullptr;
}

void UICompanionStatusWnd::ApplyConfirmedAction(msgparam_t action)
{
    if (action == kRename) {
        if (!GetNameEdit()) return;
        const std::string name = m_nameEdit->GetText();
        if (name.size() >= 24) return;
        g_modeMgr.SendMsg(199, reinterpret_cast<msgparam_t>(name.c_str()), 0, 0);
        g_session.m_homunStatus.name = name;
        g_session.m_homunStatus.nameModified = 1;
        RefreshReceivedName();
    } else if (action == kFeed) {
        g_modeMgr.SendMsg(198, 1, 0, 0);
    } else {
        const bool homunculus = action == kDismissHomunculus;
        g_modeMgr.SendMsg(homunculus ? 198 : 200, 2, 0, 0);
        if (homunculus) g_session.m_homunStatus.alive = false;
        else g_session.m_merStatus.alive = false;
        g_windowMgr.CloseCompanionWindows(homunculus);
    }
}
