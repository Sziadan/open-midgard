#include "qtui/QtChatInput.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>

class EventWindow : public QQuickWindow {
public:
    int mouseEvents = 0;
    bool event(QEvent* event) override {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease
            || event->type() == QEvent::MouseMove) ++mouseEvents;
        return QQuickWindow::event(event);
    }
};

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    EventWindow window;
    auto* root = window.contentItem();
    QQuickItem panel(root);
    panel.setObjectName("chatWindowPanel");
    panel.setWidth(200); panel.setHeight(100);
    QQuickItem field(&panel);
    field.setObjectName("chatMessageField");
    field.setWidth(100); field.setHeight(30);
    QQuickItem editor(&field);
    editor.setObjectName("chatMessageEditor");
    editor.setWidth(100); editor.setHeight(30);
    QtChatInput input;
    input.setScene(&window, root);
    HWND native = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
        0, 0, 200, 100, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    assert(native);
    const LPARAM point = MAKELPARAM(10, 10);

    assert(!input.handle(native, WM_LBUTTONDOWN, MK_LBUTTON, point, true, false, false));
    assert(!input.pointerCaptured());
    assert(window.mouseEvents == 0);

    assert(input.handle(native, WM_LBUTTONDOWN, MK_LBUTTON, point, true, false, true));
    assert(input.pointerCaptured());
    assert(input.handle(native, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(300, 300), true, false, false));
    assert(input.handle(native, WM_LBUTTONUP, 0, MAKELPARAM(300, 300), true, false, false));
    assert(!input.pointerCaptured());
    assert(window.mouseEvents == 3);
    DestroyWindow(native);
}
