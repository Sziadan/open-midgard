#include "UIWaitWnd.h"

UIWaitWnd::UIWaitWnd() : m_fontHeight(0), m_fontType(0) {}
UIWaitWnd::~UIWaitWnd() {}

int UIWaitWnd::SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra) {
    return 0; // Stub
}

void UIWaitWnd::OnCreate(int x, int y) {
    m_x = x;
    m_y = y;
}

void UIWaitWnd::OnDraw() {
    // Draw the "waiting" message on screen
}

void UIWaitWnd::SetMsg(const char* waitMsg, int fontHeight, int fontType) {
    if (waitMsg) m_waitMsg = waitMsg;
    m_fontHeight = fontHeight;
    m_fontType = fontType;
}
