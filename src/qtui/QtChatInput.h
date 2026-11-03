#pragma once
#include "QtUiRuntime.h"
#include <QChar>
class QQuickWindow;
class QQuickItem;

// Bridges just the interactive chat controls into the offscreen Quick scene.
// Gameplay and the existing window chrome keep their normal native routing.
class QtChatInput {
public:
    void setScene(QQuickWindow* window,QQuickItem* root) {m_window=window;m_root=root;}
    bool handle(RoNativeWindowHandle nativeWindow,RoWindowMessage message,RoWindowWParam wParam,RoWindowLParam lParam,bool enabled);
private:
    QQuickWindow* m_window=nullptr;
    QQuickItem* m_root=nullptr;
    bool m_pointerGrab=false;
    bool m_composing=false;
    QChar m_highSurrogate;
};
