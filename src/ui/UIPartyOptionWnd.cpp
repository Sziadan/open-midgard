#include "UIPartyOptionWnd.h"

#include "UIWindow.h"
#include "UIWindowMgr.h"
#include "gamemode/GameMode.h"
#include "main/WinMain.h"
#include "render/DC.h"
#include "session/Session.h"
#include "core/UiMessageStrings.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kTooltipMaxWidth = 248;
constexpr int kTooltipPaddingX = 8;
constexpr int kTooltipPaddingY = 6;
constexpr int kHelpIconSize = 13;

void FillRectColor(HDC hdc, const RECT& rect, COLORREF color);
void FrameRectColor(HDC hdc, const RECT& rect, COLORREF color);
void DrawTextLine(HDC hdc, const RECT& rect, const char* text, COLORREF color, UINT format);

RECT MakeRect(int left, int top, int width, int height)
{
    return RECT{ left, top, left + width, top + height };
}

void DrawFilledEllipse(HDC hdc, const RECT& rect, COLORREF fillColor, COLORREF borderColor)
{
    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    Ellipse(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void DrawTooltipBox(HDC hdc, const RECT& windowRect, const RECT& anchorRect, const std::string& text)
{
    if (!hdc || text.empty()) {
        return;
    }

    RECT measureRect{ 0, 0, kTooltipMaxWidth, 0 };
    DrawTextA(hdc,
        text.c_str(),
        -1,
        &measureRect,
        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);

    const int tooltipWidth = (measureRect.right - measureRect.left) + kTooltipPaddingX * 2;
    const int tooltipHeight = (measureRect.bottom - measureRect.top) + kTooltipPaddingY * 2;
    const int anchorCenterX = (anchorRect.left + anchorRect.right) / 2;
    int tooltipLeft = anchorCenterX - (tooltipWidth / 2);
    int tooltipTop = anchorRect.top - tooltipHeight - 14;

    if (tooltipLeft < windowRect.left + 4) {
        tooltipLeft = windowRect.left + 4;
    }
    if (tooltipLeft + tooltipWidth > windowRect.right - 4) {
        tooltipLeft = windowRect.right - tooltipWidth - 4;
    }
    if (tooltipTop + tooltipHeight > windowRect.bottom - 4) {
        tooltipTop = windowRect.bottom - tooltipHeight - 4;
    }
    if (tooltipTop < windowRect.top + 4) {
        tooltipTop = anchorRect.bottom + 12;
    }
    if (tooltipTop + tooltipHeight > windowRect.bottom - 4) {
        tooltipTop = windowRect.bottom - tooltipHeight - 4;
    }

    const RECT tooltipRect{ tooltipLeft, tooltipTop, tooltipLeft + tooltipWidth, tooltipTop + tooltipHeight };
    FillRectColor(hdc, tooltipRect, RGB(48, 48, 48));
    FrameRectColor(hdc, tooltipRect, RGB(96, 96, 96));

    RECT textRect{ tooltipRect.left + kTooltipPaddingX, tooltipRect.top + kTooltipPaddingY,
        tooltipRect.right - kTooltipPaddingX, tooltipRect.bottom - kTooltipPaddingY };
    DrawTextLine(hdc, textRect, text.c_str(), RGB(255, 255, 255), DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
}

constexpr int kWindowWidth = 332;
constexpr int kWindowHeight = 236;
constexpr int kTitleBarHeight = 18;
constexpr int kCloseButtonWidth = 14;
constexpr int kCloseButtonHeight = 12;
constexpr int kNameFieldWidth = 178;
constexpr int kNameFieldHeight = 22;
constexpr int kButtonWidth = 84;
constexpr int kButtonHeight = 24;

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

bool IsPointInRect(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
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

void DrawTextLine(HDC hdc, const RECT& rect, const char* text, COLORREF color, UINT format)
{
    if (!hdc || !text) {
        return;
    }

    const int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    const COLORREF oldColor = SetTextColor(hdc, color);
    RECT drawRect = rect;
    DrawTextA(hdc, text, -1, &drawRect, format);
    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldBkMode);
}

const char* GetChoiceLabel(int groupId, int optionId)
{
    switch (groupId) {
    case UIPartyOptionWnd::GroupExpShare:
        return optionId == 0 ? "Each Take" : "Even Share";
    case UIPartyOptionWnd::GroupItemShare:
        return optionId == 0 ? "Each Take" : "Party Share";
    case UIPartyOptionWnd::GroupItemSharingType:
        return optionId == 0 ? "Individual" : "Shared";
    default:
        return "";
    }
}

const char* GetGroupHeaderLabel(int groupId)
{
    switch (groupId) {
    case UIPartyOptionWnd::GroupExpShare:
        return "How to share EXP:";
    case UIPartyOptionWnd::GroupItemShare:
        return "How to share items:";
    case UIPartyOptionWnd::GroupItemSharingType:
        return "Item sharing type:";
    default:
        return "";
    }
}

int MeasureUiLabelWidth(const char* text)
{
    if (!text) {
        return 0;
    }

    HDC hdc = AcquireMainWindowDrawTarget();
    if (!hdc) {
        return static_cast<int>(std::strlen(text)) * 6 + 2;
    }

    RECT measureRect{ 0, 0, 0, 0 };
    DrawTextA(hdc, text, -1, &measureRect, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
    ReleaseMainWindowDrawTarget(hdc);
    return measureRect.right - measureRect.left;
}

} // namespace

UIPartyOptionWnd::UIPartyOptionWnd()
    : m_mode(ModeCreate),
      m_nameEditCtrl(new UIEditCtrl()),
      m_controlsCreated(false),
      m_pressedButtonId(0),
      m_expShareOption(0),
      m_itemShareOption(0),
      m_itemSharingTypeOption(0),
            m_hoverTooltipGroup(-1),
            m_lastTooltipMouseX(-1),
            m_lastTooltipMouseY(-1),
      m_lastVisualStateToken(0),
      m_hasVisualStateToken(false)
{
    Create(kWindowWidth, kWindowHeight);
    m_nameEditCtrl->Create(kNameFieldWidth, kNameFieldHeight);
    m_nameEditCtrl->m_maxchar = 23;
    m_nameEditCtrl->m_textR = 0;
    m_nameEditCtrl->m_textG = 0;
    m_nameEditCtrl->m_textB = 0;
    AddChild(m_nameEditCtrl);
}

UIPartyOptionWnd::~UIPartyOptionWnd() = default;

void UIPartyOptionWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (!m_controlsCreated && show != 0) {
        OnCreate(0, 0);
    }

    if (m_nameEditCtrl) {
        m_nameEditCtrl->SetShow(show != 0 && m_mode == ModeCreate ? 1 : 0);
    }

    if (show == 0) {
        ClearEditFocus();
        m_pressedButtonId = 0;
        m_hoverTooltipGroup = -1;
        m_lastTooltipMouseX = -1;
        m_lastTooltipMouseY = -1;
    } else if (m_mode == ModeCreate && m_nameEditCtrl) {
        g_windowMgr.m_editWindow = m_nameEditCtrl;
        m_nameEditCtrl->m_hasFocus = true;
    }
}

void UIPartyOptionWnd::Move(int x, int y)
{
    g_windowMgr.ClampWindowToClient(&x, &y, m_w, m_h);
    UIWindow::Move(x, y);
    LayoutControls();
}

bool UIPartyOptionWnd::IsUpdateNeed()
{
    const unsigned long long token = BuildVisualStateToken();
    if (!m_hasVisualStateToken || m_lastVisualStateToken != token) {
        return true;
    }
    return UIWindow::IsUpdateNeed();
}

void UIPartyOptionWnd::OnCreate(int x, int y)
{
    (void)x;
    (void)y;
    if (m_controlsCreated) {
        return;
    }

    Create(kWindowWidth, kWindowHeight);
    m_id = UIWindowMgr::WID_PARTYOPTIONWND;
    m_controlsCreated = true;

    int savedX = 0;
    int savedY = 0;
    if (!LoadUiWindowPlacement("PartyOptionWnd", &savedX, &savedY)) {
        savedX = 378;
        savedY = 88;
    }
    Move(savedX, savedY);
    LayoutControls();
}

void UIPartyOptionWnd::OnDestroy()
{
    StoreInfo();
    ClearEditFocus();
}

void UIPartyOptionWnd::OnDraw()
{
    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    DisplayData display{};
    GetDisplayDataForQt(&display);

    const RECT outer{ m_x, m_y, m_x + m_w, m_y + m_h };
    const RECT titleBar{ m_x + 1, m_y + 1, m_x + m_w - 1, m_y + kTitleBarHeight };
    const RECT content{ m_x + 1, m_y + kTitleBarHeight, m_x + m_w - 1, m_y + m_h - 1 };

    FillRectColor(hdc, outer, RGB(232, 225, 210));
    FrameRectColor(hdc, outer, RGB(92, 82, 67));
    FillRectColor(hdc, titleBar, RGB(92, 111, 96));
    FrameRectColor(hdc, titleBar, RGB(60, 74, 62));
    FillRectColor(hdc, content, RGB(245, 240, 229));

    const RECT titleTextRect{ m_x + 10, m_y + 1, m_x + 220, m_y + kTitleBarHeight };
    DrawTextLine(hdc, titleTextRect, display.title.c_str(), RGB(248, 248, 240), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    const RECT closeRect = GetCloseButtonRect();
    FillRectColor(hdc, closeRect, RGB(210, 198, 180));
    FrameRectColor(hdc, closeRect, RGB(110, 96, 74));
    DrawTextLine(hdc, closeRect, "x", RGB(34, 34, 34), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    for (const DisplayLabel& label : display.labels) {
        const RECT rect{ label.x, label.y, label.x + label.width, label.y + label.height };
        DrawTextLine(hdc,
            rect,
            label.text.c_str(),
            label.header ? RGB(74, 60, 38) : RGB(46, 46, 46),
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    for (const DisplayHelpIcon& icon : display.helpIcons) {
        const RECT rect{ icon.x, icon.y, icon.x + icon.width, icon.y + icon.height };
        DrawFilledEllipse(hdc, rect, icon.hovered ? RGB(212, 226, 239) : RGB(235, 229, 214), RGB(121, 106, 79));
        DrawTextLine(hdc, rect, "?", RGB(28, 28, 28), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    if (!display.showNameEdit) {
        const RECT nameFieldRect{ display.nameFieldX, display.nameFieldY, display.nameFieldX + display.nameFieldWidth, display.nameFieldY + display.nameFieldHeight };
        const RECT textRect{ nameFieldRect.left, nameFieldRect.top, nameFieldRect.right, nameFieldRect.bottom };
        DrawTextLine(hdc, textRect, display.nameValue.c_str(), RGB(32, 32, 32), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    for (const DisplayChoice& choice : display.choices) {
        const RECT rect{ choice.x, choice.y, choice.x + choice.width, choice.y + choice.height };
        const RECT bulletRect{ rect.left, rect.top + 2, rect.left + 14, rect.top + 16 };
        FillRectColor(hdc, bulletRect, RGB(247, 243, 233));
        FrameRectColor(hdc, bulletRect, choice.enabled?RGB(126, 111, 85):RGB(160,160,160));
        if (choice.selected) {
            const RECT fillRect{ bulletRect.left + 3, bulletRect.top + 3, bulletRect.right - 3, bulletRect.bottom - 3 };
            FillRectColor(hdc, fillRect, RGB(87, 111, 138));
        }
        const RECT labelRect{ rect.left + 20, rect.top, rect.right, rect.bottom };
        DrawTextLine(hdc, labelRect, choice.label.c_str(), choice.enabled?RGB(44, 44, 44):RGB(128,128,128), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    for (const QtButtonDisplay& button : display.buttons) {
        if (!button.visible) {
            continue;
        }
        const RECT rect{ button.x, button.y, button.x + button.width, button.y + button.height };
        FillRectColor(hdc, rect, button.active ? RGB(212, 226, 239) : RGB(235, 229, 214));
        FrameRectColor(hdc, rect, RGB(121, 106, 79));
        DrawTextLine(hdc, rect, button.label.c_str(), RGB(28, 28, 28), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    if (!display.tooltipText.empty()) {
        DrawTooltipBox(hdc, outer, GetGroupTooltipAnchorRect(m_hoverTooltipGroup), display.tooltipText);
    }

    DrawChildrenToHdc(hdc);

    ReleaseDrawTarget(hdc);
    m_lastVisualStateToken = BuildVisualStateToken();
    m_hasVisualStateToken = true;
    m_isDirty = 0;
}

void UIPartyOptionWnd::OnLBtnDown(int x, int y)
{
    const int buttonId = HitTestButton(x, y);
    if (buttonId != 0) {
        m_pressedButtonId = buttonId;
        Invalidate();
        return;
    }

    int groupId = -1;
    int optionId = -1;
    if (HitTestChoice(x, y, &groupId, &optionId)) {
        return;
    }

    UIFrameWnd::OnLBtnDown(x, y);
}

void UIPartyOptionWnd::OnLBtnUp(int x, int y)
{
    const bool wasDragging = m_isDragging != 0;
    UIFrameWnd::OnLBtnUp(x, y);
    if (wasDragging) {
        return;
    }

    int groupId = -1;
    int optionId = -1;
    if (HitTestChoice(x, y, &groupId, &optionId)) {
        SetSelectedChoice(groupId, optionId);
        return;
    }

    const int buttonId = HitTestButton(x, y);
    const int pressedButtonId = m_pressedButtonId;
    m_pressedButtonId = 0;
    if (buttonId == 0 || buttonId != pressedButtonId) {
        Invalidate();
        return;
    }

    if (buttonId == ButtonClose || buttonId == ButtonCancel) {
        CloseDialog();
        return;
    }
    if (buttonId == ButtonConfirm) {
        Submit();
    }
}

void UIPartyOptionWnd::OnLBtnDblClk(int x, int y)
{
    OnLBtnUp(x, y);
}

void UIPartyOptionWnd::OnMouseMove(int x, int y)
{
    UpdateTooltipHover(x, y);
    UIFrameWnd::OnMouseMove(x, y);
}

void UIPartyOptionWnd::OnMouseHover(int x, int y)
{
    UpdateTooltipHover(x, y);
}

void UIPartyOptionWnd::OnKeyDown(int virtualKey)
{
    if (virtualKey == VK_ESCAPE) {
        CloseDialog();
        return;
    }
    if (virtualKey == VK_RETURN) {
        Submit();
        return;
    }

    if (m_mode == ModeCreate && m_nameEditCtrl) {
        m_nameEditCtrl->OnKeyDown(virtualKey);
    }
}

void UIPartyOptionWnd::StoreInfo()
{
    SaveUiWindowPlacement("PartyOptionWnd", m_x, m_y);
}

void UIPartyOptionWnd::OpenCreateDialog()
{
    m_mode = ModeCreate;
    ResetCreateDefaults();
    LayoutControls();
    if (m_nameEditCtrl) {
        m_nameEditCtrl->SetText("");
        m_nameEditCtrl->m_hasFocus = true;
        g_windowMgr.m_editWindow = m_nameEditCtrl;
    }
    Invalidate();
}

void UIPartyOptionWnd::OpenConfigureDialog()
{
    m_mode = ModeConfigure;
    LoadCurrentPartyOptions();
    LayoutControls();
    ClearEditFocus();
    Invalidate();
}

bool UIPartyOptionWnd::CanEditSettings() const {return g_session.GetNumParty()==0 || g_session.m_amIPartyMaster;}

bool UIPartyOptionWnd::CanEditChoice(int groupId) const
{
    return CanEditSettings() && (m_mode == ModeCreate || groupId == GroupExpShare);
}

bool UIPartyOptionWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData) {
        return false;
    }

    outData->title = m_mode == ModeCreate ? "Create Party" : "Configure Party";
    outData->showNameEdit = m_mode == ModeCreate;
    outData->nameFocused = m_nameEditCtrl ? m_nameEditCtrl->m_hasFocus : false;
    outData->nameLabel = m_mode == ModeCreate ? "Party Name" : "Current Party";
    outData->nameValue = m_mode == ModeCreate
        ? (m_nameEditCtrl ? m_nameEditCtrl->GetText() : "")
        : g_session.m_partyName;

    const RECT nameRect = GetNameFieldRect();
    outData->nameFieldX = nameRect.left;
    outData->nameFieldY = nameRect.top;
    outData->nameFieldWidth = nameRect.right - nameRect.left;
    outData->nameFieldHeight = nameRect.bottom - nameRect.top;
    outData->tooltipText = GetTooltipText(m_hoverTooltipGroup);

    const RECT tooltipAnchorRect = GetGroupTooltipAnchorRect(m_hoverTooltipGroup);
    outData->tooltipX = tooltipAnchorRect.left;
    outData->tooltipY = tooltipAnchorRect.top;

    DisplayLabel nameLabel{};
    nameLabel.x = m_x + 16;
    nameLabel.y = m_y + 30;
    nameLabel.width = 90;
    nameLabel.height = 18;
    nameLabel.text = outData->nameLabel;
    nameLabel.header = true;
    outData->labels.push_back(nameLabel);

    for (int groupId = GroupExpShare; groupId <= GroupItemSharingType; ++groupId) {
        const int headerWidth = MeasureUiLabelWidth(GetGroupHeaderLabel(groupId));
        DisplayLabel header{};
        header.x = m_x + 16;
        header.y = m_y + 68 + groupId * 44;
        header.width = headerWidth;
        header.height = 18;
        header.text = GetGroupHeaderLabel(groupId);
        header.header = true;
        outData->labels.push_back(header);

        const RECT helpIconRect = GetHelpIconRect(groupId);
        DisplayHelpIcon helpIcon{};
        helpIcon.groupId = groupId;
        helpIcon.x = helpIconRect.left;
        helpIcon.y = helpIconRect.top;
        helpIcon.width = helpIconRect.right - helpIconRect.left;
        helpIcon.height = helpIconRect.bottom - helpIconRect.top;
        helpIcon.hovered = m_hoverTooltipGroup == groupId;
        outData->helpIcons.push_back(helpIcon);

        for (int optionId = 0; optionId < 2; ++optionId) {
            const RECT choiceRect = GetChoiceRect(groupId, optionId);
            DisplayChoice choice{};
            choice.groupId = groupId;
            choice.optionId = optionId;
            choice.x = choiceRect.left;
            choice.y = choiceRect.top;
            choice.width = choiceRect.right - choiceRect.left;
            choice.height = choiceRect.bottom - choiceRect.top;
            choice.label = GetChoiceLabel(groupId, optionId);
            choice.selected = GetSelectedChoice(groupId) == optionId;
            choice.enabled = CanEditChoice(groupId);
            outData->choices.push_back(choice);
        }
    }

    for (int buttonId : { ButtonConfirm, ButtonCancel }) {
        const RECT rect = GetButtonRect(buttonId);
        QtButtonDisplay button{};
        button.id = buttonId;
        button.x = rect.left;
        button.y = rect.top;
        button.width = rect.right - rect.left;
        button.height = rect.bottom - rect.top;
        button.label = buttonId == ButtonConfirm ? (m_mode == ModeCreate ? "Create" : "Apply") : "Cancel";
        button.active = m_pressedButtonId == buttonId;
        outData->buttons.push_back(std::move(button));
    }
    return true;
}

void UIPartyOptionWnd::LayoutControls()
{
    if (!m_nameEditCtrl) {
        return;
    }

    const RECT nameRect = GetNameFieldRect();
    m_nameEditCtrl->Move(nameRect.left, nameRect.top);
    m_nameEditCtrl->Resize(nameRect.right - nameRect.left, nameRect.bottom - nameRect.top);
    m_nameEditCtrl->SetShow(m_show != 0 && m_mode == ModeCreate ? 1 : 0);
}

void UIPartyOptionWnd::ResetCreateDefaults()
{
    m_expShareOption = 0;
    m_itemShareOption = 0;
    m_itemSharingTypeOption = 0;
}

void UIPartyOptionWnd::LoadCurrentPartyOptions()
{
    m_expShareOption = g_session.m_partyExpShare ? 1 : 0;
    m_itemShareOption = g_session.m_itemCollectType ? 1 : 0;
    m_itemSharingTypeOption = g_session.m_itemDivType ? 1 : 0;
}

void UIPartyOptionWnd::ClearEditFocus()
{
    if (g_windowMgr.m_editWindow == m_nameEditCtrl) {
        g_windowMgr.m_editWindow = nullptr;
    }
    if (m_nameEditCtrl) {
        m_nameEditCtrl->m_hasFocus = false;
    }
}

unsigned int UIPartyOptionWnd::BuildOptionBits() const
{
    return (m_expShareOption != 0 ? 0x01u : 0x00u)
        | (m_itemShareOption != 0 ? 0x02u : 0x00u)
        | (m_itemSharingTypeOption != 0 ? 0x04u : 0x00u);
}

bool UIPartyOptionWnd::Submit()
{
    if(!CanEditSettings()){CloseDialog();return true;}
    if (m_mode == ModeCreate) {
        const char* partyName = m_nameEditCtrl ? m_nameEditCtrl->GetText() : "";
        if (!partyName || *partyName == '\0') {
            g_windowMgr.PushChatEvent("Enter a party name.", 0x00FF4040u, 6);
            if (m_nameEditCtrl) {
                m_nameEditCtrl->m_hasFocus = true;
                g_windowMgr.m_editWindow = m_nameEditCtrl;
            }
            return false;
        }
        if (g_modeMgr.SendMsg(CGameMode::GameMsg_RequestPartyCreate,
                reinterpret_cast<msgparam_t>(partyName),
                static_cast<msgparam_t>(BuildOptionBits()),
                0) == 0) {
            return false;
        }
    } else {
        if (g_modeMgr.SendMsg(CGameMode::GameMsg_RequestPartyChangeOptions,
                static_cast<msgparam_t>(m_expShareOption != 0),
                0,
                0) == 0) {
            return false;
        }
    }

    CloseDialog();
    return true;
}

void UIPartyOptionWnd::CloseDialog()
{
    // The source retires its manager owner on confirm/cancel/close. A retained
    // hidden object would make the next source Alt+P delete instead of reopen.
    StoreInfo();if(g_windowMgr.m_partyOptionWnd==this)g_windowMgr.DeleteWindow(this);else SetShow(0);
}

std::string UIPartyOptionWnd::GetTooltipText(int groupId) const
{
    switch (groupId) {
    case GroupExpShare:
        return "Each take - Party members only gain EXP for monsters they damage.\n\nEven Share - Monsters' EXP is shared evenly among members in range.";
    case GroupItemShare:
        if (m_mode == ModeConfigure) {
            return "The party leader sets the item pickup rule when creating the party.";
        }
        return "Each take - Only the player who killed the monster can pick up the item.\n\nParty share - Any party member can pick up items dropped by monsters, regardless of who killed it.";
    case GroupItemSharingType:
        if (m_mode == ModeConfigure) {
            return "The party leader sets the loot distribution rule when creating the party.";
        }
        return "Individual - Items picked up are kept by the player that picked it up.\n\nShared - Items are automatically distributed to members randomly when picked up.";
    default:
        return std::string();
    }
}

int UIPartyOptionWnd::GetTooltipGroupAtPoint(int x, int y) const
{
    if (x < 0 || y < 0) {
        return -1;
    }

    for (int groupId = GroupExpShare; groupId <= GroupItemSharingType; ++groupId) {
        if (IsPointInRect(GetHelpIconRect(groupId), x, y)) {
            return groupId;
        }
    }

    return -1;
}

void UIPartyOptionWnd::UpdateTooltipHover(int x, int y)
{
    const int hoveredGroup = GetTooltipGroupAtPoint(x, y);
    const bool positionChanged = m_lastTooltipMouseX != x || m_lastTooltipMouseY != y;
    if (m_hoverTooltipGroup == hoveredGroup && (!positionChanged || hoveredGroup < 0)) {
        return;
    }

    m_hoverTooltipGroup = hoveredGroup;
    m_lastTooltipMouseX = hoveredGroup >= 0 ? x : -1;
    m_lastTooltipMouseY = hoveredGroup >= 0 ? y : -1;
    Invalidate();
}

RECT UIPartyOptionWnd::GetGroupHeaderRect(int groupId) const
{
    return RECT{ m_x + 16, m_y + 68 + groupId * 44, m_x + 180, m_y + 68 + groupId * 44 + 18 };
}

RECT UIPartyOptionWnd::GetHelpIconRect(int groupId) const
{
    const RECT secondChoiceRect = GetChoiceRect(groupId, 1);
    const int iconLeft = secondChoiceRect.right + 4;
    const int iconTop = secondChoiceRect.top + ((secondChoiceRect.bottom - secondChoiceRect.top - kHelpIconSize) / 2);
    return MakeRect(iconLeft, iconTop, kHelpIconSize, kHelpIconSize);
}

RECT UIPartyOptionWnd::GetGroupTooltipAnchorRect(int groupId) const
{
    if (groupId < GroupExpShare || groupId > GroupItemSharingType) {
        return GetNameFieldRect();
    }

    if (m_lastTooltipMouseX >= 0 && m_lastTooltipMouseY >= 0) {
        return MakeRect(m_lastTooltipMouseX, m_lastTooltipMouseY, 1, 1);
    }

    return GetHelpIconRect(groupId);
}

void UIPartyOptionWnd::SetSelectedChoice(int groupId, int optionId)
{
    if(!CanEditSettings()){
        g_windowMgr.SendMsg(3,0,0,0);ro::UiMessageStrings strings;strings.Load();const auto text=strings.Get(294,"");
        g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(text.c_str()),16777060,9,0);return;
    }
    if (!CanEditChoice(groupId)) {
        return;
    }
    switch (groupId) {
    case GroupExpShare:
        m_expShareOption = optionId;
        break;
    case GroupItemShare:
        m_itemShareOption = optionId;
        break;
    case GroupItemSharingType:
        m_itemSharingTypeOption = optionId;
        break;
    default:
        return;
    }
    Invalidate();
}

int UIPartyOptionWnd::GetSelectedChoice(int groupId) const
{
    switch (groupId) {
    case GroupExpShare:
        return m_expShareOption;
    case GroupItemShare:
        return m_itemShareOption;
    case GroupItemSharingType:
        return m_itemSharingTypeOption;
    default:
        return 0;
    }
}

int UIPartyOptionWnd::HitTestButton(int x, int y) const
{
    for (int buttonId : { ButtonClose, ButtonConfirm, ButtonCancel }) {
        if (IsPointInRect(GetButtonRect(buttonId), x, y)) {
            return buttonId;
        }
    }
    return 0;
}

bool UIPartyOptionWnd::HitTestChoice(int x, int y, int* outGroupId, int* outOptionId) const
{
    for (int groupId = GroupExpShare; groupId <= GroupItemSharingType; ++groupId) {
        for (int optionId = 0; optionId < 2; ++optionId) {
            if (IsPointInRect(GetChoiceRect(groupId, optionId), x, y)) {
                if (outGroupId) {
                    *outGroupId = groupId;
                }
                if (outOptionId) {
                    *outOptionId = optionId;
                }
                return true;
            }
        }
    }
    return false;
}

RECT UIPartyOptionWnd::GetCloseButtonRect() const
{
    return RECT{ m_x + m_w - 22, m_y + 3, m_x + m_w - 22 + kCloseButtonWidth, m_y + 3 + kCloseButtonHeight };
}

RECT UIPartyOptionWnd::GetNameFieldRect() const
{
    return RECT{ m_x + 112, m_y + 28, m_x + 112 + kNameFieldWidth, m_y + 28 + kNameFieldHeight };
}

RECT UIPartyOptionWnd::GetButtonRect(int buttonId) const
{
    if (buttonId == ButtonClose) {
        return GetCloseButtonRect();
    }

    const int top = m_y + m_h - 36;
    if (buttonId == ButtonConfirm) {
        return RECT{ m_x + m_w - 184, top, m_x + m_w - 184 + kButtonWidth, top + kButtonHeight };
    }
    return RECT{ m_x + m_w - 92, top, m_x + m_w - 92 + kButtonWidth, top + kButtonHeight };
}

RECT UIPartyOptionWnd::GetChoiceRect(int groupId, int optionId) const
{
    const int left = m_x + 30 + optionId * 138;
    const int top = m_y + 86 + groupId * 44;
    return RECT{ left, top, left + 120, top + 18 };
}

unsigned long long UIPartyOptionWnd::BuildVisualStateToken() const
{
    // Authority changes must repaint the shared native/Qt disabled state.
    unsigned long long hash = 1469598103934665603ull;
    HashTokenValue(&hash,CanEditSettings()?1ull:0ull);
    HashTokenValue(&hash, static_cast<unsigned long long>(m_show));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_mode));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_expShareOption));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_itemShareOption));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_itemSharingTypeOption));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_hoverTooltipGroup + 1));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_lastTooltipMouseX + 1));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_lastTooltipMouseY + 1));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_pressedButtonId));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_x));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_y));
    if (m_nameEditCtrl) {
        HashTokenString(&hash, m_nameEditCtrl->GetText());
        HashTokenValue(&hash, static_cast<unsigned long long>(m_nameEditCtrl->m_hasFocus ? 1 : 0));
    }
    HashTokenString(&hash, g_session.m_partyName);
    return hash;
}
