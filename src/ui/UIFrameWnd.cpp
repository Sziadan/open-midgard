#include "UIFrameWnd.h"

UIFrameWnd::UIFrameWnd() 
    : m_startGlobalX(0), m_startGlobalY(0), m_orgX(0), m_orgY(0), 
      m_defPushId(0), m_defCancelPushId(0)
{
}

UIFrameWnd::~UIFrameWnd() {
}

int UIFrameWnd::SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra) {
    return 0;
}
