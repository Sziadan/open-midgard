#pragma once

#include "UIFrameWnd.h"
#include "core/UiMessageStrings.h"

#include <string>
#include <vector>

class UIInstanceWnd final : public UIFrameWnd {
public:
    explicit UIInstanceWnd(int windowId);
    struct Display {
        std::string title;
        std::string status;
        std::vector<std::string> details;
    };
    Display GetDisplay(u64 now) const;
    void OnProcess() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void StoreInfo() override;

private:
    bool InCloseButton(int x, int y) const;
    ro::UiMessageStrings m_strings;
    u64 m_displaySecond = 0;
    bool m_closePressed = false;
};
