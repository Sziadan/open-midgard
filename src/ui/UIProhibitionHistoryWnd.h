#pragma once

#include "UIFrameWnd.h"
#include <string>

class UIProhibitionHistoryWnd : public UIFrameWnd {
public:
    UIProhibitionHistoryWnd();
    void SetTarget(u32 aid, std::string server);
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnKeyDown(int key) override;
    void OnWheel(int delta) override;

private:
    RECT CloseRect() const;
    u32 m_aid = 0;
    std::string m_server;
    bool m_closePressed = false;
    int m_firstRow = 0;
};
