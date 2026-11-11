#pragma once

#include "UIFrameWnd.h"
#include <vector>

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
    void Move(int x, int y) override;
    void OnDraw() override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;

private:
    UIBitmapButton* m_closeButton;
};
