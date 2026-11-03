#pragma once
#include "QtUiRuntime.h"
#include <QChar>
class QQuickWindow;
class QQuickItem;

// Bridges chat and interactive Qt gameplay panels into the offscreen scene.
// Legacy windows retain their native routing outside those panel bounds.
class QtChatInput {
public:
    void setScene(QQuickWindow* window,QQuickItem* root) {m_window=window;m_root=root;}
    bool pointerCaptured() const {return m_pointerGrab;}
    bool handle(RoNativeWindowHandle nativeWindow,RoWindowMessage message,RoWindowWParam wParam,RoWindowLParam lParam,bool enabled,bool guildEnabled=false);
private:
    QQuickWindow* m_window=nullptr;
    QQuickItem* m_root=nullptr;
    bool m_pointerGrab=false;
    bool m_composing=false;
    QChar m_highSurrogate;
};
