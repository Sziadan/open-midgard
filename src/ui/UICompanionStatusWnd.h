#pragma once

#include "UIFrameWnd.h"
#include <vector>

class UIFriendOptionButton;

class UICompanionStatusWnd : public UIFrameWnd {
public:
    struct Text {
        int x, y, width;
        std::string value;
        bool rightAligned = false;
    };

    struct Gauge {
        int x, y, width, height;
        int filledWidth;
        COLORREF color;
    };

    struct Display {
        std::vector<Text> text;
        std::vector<Gauge> gauges;
    };

    explicit UICompanionStatusWnd(int windowId);
    Display GetDisplay() const;
    const std::vector<UIFriendOptionButton*>& GetButtons() const { return m_buttons; }
    UIEditCtrl* GetNameEdit() const { return m_nameEdit && m_nameEdit->m_show ? m_nameEdit : nullptr; }
    std::string GetEditDisplayText() const;
    void RefreshReceivedName();
    void Move(int x, int y) override;
    void OnDraw() override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;

private:
    void ShowPrompt(int messageId, std::string text, bool confirmation, msgparam_t action);
    void ApplyConfirmedAction(msgparam_t action);
    std::vector<UIFriendOptionButton*> m_buttons;
    UIEditCtrl* m_nameEdit = nullptr;
};
