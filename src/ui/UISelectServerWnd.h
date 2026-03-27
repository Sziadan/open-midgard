#pragma once

#include "UIFrameWnd.h"

#include <vector>
#include <windows.h>

class UISelectServerWnd : public UIFrameWnd {
public:
    UISelectServerWnd();
    ~UISelectServerWnd() override;

    void SetShow(int show) override;
    void OnCreate(int cx, int cy) override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnKeyDown(int virtualKey);

private:
    void EnsureCreated();
    void SyncGeometry();
    int ComputeWindowHeight() const;
    int HitTestEntry(int x, int y) const;

    bool m_controlsCreated;
    int m_hoverIndex;
    std::vector<RECT> m_entryRects;
};