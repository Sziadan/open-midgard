#pragma once
#include "UIFrameWnd.h"
class UIGuildWnd:public UIFrameWnd {
public:
    UIGuildWnd();
    void SetShow(int show) override;
    void OnKeyDown(int key) override;
    void StoreInfo() override;
    void Refresh();
    static void SendRefreshRequests();
    int tab=0;
};
