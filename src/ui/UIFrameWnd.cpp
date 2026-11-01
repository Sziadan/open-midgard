#include "UiMetrics.h"
#include "UIFrameWnd.h"
#include "UIWindowMgr.h"

UIFrameWnd::UIFrameWnd() 
    : m_startGlobalX(0), m_startGlobalY(0), m_orgX(0), m_orgY(0), 
      m_defPushId(0), m_defCancelPushId(0), m_isDragging(0)
{
}

UIFrameWnd::~UIFrameWnd() {
}

msgresult_t UIFrameWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) {
    return 0;
}

void UIFrameWnd::OnLBtnDown(int x, int y)
{
    constexpr int kTitleBarHeight = uiMetrics::titleHeight;
    if (m_show == 0) {
        return;
    }
    if (x < m_x || x >= m_x + m_w || y < m_y || y >= m_y + kTitleBarHeight) {
        return;
    }

    m_isDragging = 1;
    m_startGlobalX = x;
    m_startGlobalY = y;
    m_orgX = m_x;
    m_orgY = m_y;
}

void UIFrameWnd::OnMouseMove(int x, int y)
{
    if (m_isDragging == 0) {
        return;
    }

    int snappedX = m_orgX + (x - m_startGlobalX);
    int snappedY = m_orgY + (y - m_startGlobalY);
    g_windowMgr.SnapWindowToNearby(this, &snappedX, &snappedY);
    Move(snappedX, snappedY);
}

void UIFrameWnd::OnLBtnUp(int x, int y)
{
    (void)x;
    (void)y;

    if (m_isDragging == 0) {
        return;
    }

    m_isDragging = 0;
    StoreInfo();
}
