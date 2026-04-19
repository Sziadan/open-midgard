#pragma once

#include "UIFrameWnd.h"

#include <string>
#include <vector>

class UIJoinPartyAcceptWnd : public UIFrameWnd {
public:
    enum ButtonId {
        ButtonClose = 1,
        ButtonAccept = 2,
        ButtonDecline = 3,
    };

    struct QtButtonDisplay {
        int id = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::string label;
        bool active = false;
    };

    struct DisplayData {
        std::string title;
        std::string bodyText;
        std::vector<std::string> messageLines;
        std::vector<QtButtonDisplay> buttons;
    };

    UIJoinPartyAcceptWnd();
    ~UIJoinPartyAcceptWnd() override;

    void SetShow(int show) override;
    bool IsUpdateNeed() override;
    void OnCreate(int x, int y) override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnKeyDown(int virtualKey) override;
    bool HandleKeyDown(int virtualKey);

    void OpenInvite(u32 partyId, const char* partyName);
    bool GetDisplayDataForQt(DisplayData* outData) const;

private:
    RECT GetCloseButtonRect() const;
    RECT GetButtonRect(int buttonId) const;
    int HitTestButton(int x, int y) const;
    void ClearState();
    bool SubmitReply(bool accept);
    unsigned long long BuildVisualStateToken() const;

    u32 m_partyId;
    std::string m_partyName;
    int m_hoverButtonId;
    int m_pressedButtonId;
    unsigned long long m_lastVisualStateToken;
    bool m_hasVisualStateToken;
    bool m_controlsCreated;
};