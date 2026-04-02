#include "QtPlatformWindow.h"

#if RO_ENABLE_QT6_UI

#include "DebugLog.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointF>
#include <QWheelEvent>
#include <QWindow>

#include <algorithm>
#include <cstdlib>

namespace {

constexpr unsigned int WM_MOUSEMOVE = 0x0200u;
constexpr unsigned int WM_LBUTTONDOWN = 0x0201u;
constexpr unsigned int WM_LBUTTONUP = 0x0202u;
constexpr unsigned int WM_LBUTTONDBLCLK = 0x0203u;
constexpr unsigned int WM_RBUTTONDOWN = 0x0204u;
constexpr unsigned int WM_RBUTTONUP = 0x0205u;
constexpr unsigned int WM_MOUSEWHEEL = 0x020Au;
constexpr unsigned int WM_CHAR = 0x0102u;
constexpr unsigned int WM_KEYDOWN = 0x0100u;
constexpr unsigned int WM_SYSKEYDOWN = 0x0104u;
constexpr unsigned int WM_ACTIVATE = 0x0006u;
constexpr unsigned int WM_CLOSE = 0x0010u;

constexpr std::uintptr_t MK_LBUTTON = 0x0001u;
constexpr std::uintptr_t MK_RBUTTON = 0x0002u;
constexpr std::uintptr_t WA_INACTIVE = 0u;
constexpr std::uintptr_t WA_ACTIVE = 1u;

constexpr int VK_BACK = 0x08;
constexpr int VK_TAB = 0x09;
constexpr int VK_RETURN = 0x0D;
constexpr int VK_SHIFT = 0x10;
constexpr int VK_CONTROL = 0x11;
constexpr int VK_MENU = 0x12;
constexpr int VK_ESCAPE = 0x1B;
constexpr int VK_PRIOR = 0x21;
constexpr int VK_NEXT = 0x22;
constexpr int VK_LEFT = 0x25;
constexpr int VK_UP = 0x26;
constexpr int VK_RIGHT = 0x27;
constexpr int VK_DOWN = 0x28;
constexpr int VK_INSERT = 0x2D;
constexpr int VK_DELETE = 0x2E;
constexpr int VK_F1 = 0x70;
constexpr int VK_F9 = 0x78;
constexpr int VK_OEM_PLUS = 0xBB;
constexpr int VK_OEM_MINUS = 0xBD;

std::intptr_t MakeLParam(int x, int y)
{
    const std::uint16_t low = static_cast<std::uint16_t>(static_cast<std::int16_t>(x));
    const std::uint16_t high = static_cast<std::uint16_t>(static_cast<std::int16_t>(y));
    return static_cast<std::intptr_t>((static_cast<std::uint32_t>(high) << 16) | low);
}

std::uintptr_t MakeWheelWParam(int delta, std::uintptr_t keyState)
{
    const std::uint16_t low = static_cast<std::uint16_t>(keyState & 0xFFFFu);
    const std::uint16_t high = static_cast<std::uint16_t>(static_cast<std::int16_t>(delta));
    return static_cast<std::uintptr_t>((static_cast<std::uint32_t>(high) << 16) | low);
}

std::uintptr_t BuildMouseKeyState(Qt::MouseButtons buttons)
{
    std::uintptr_t state = 0;
    if (buttons.testFlag(Qt::LeftButton)) {
        state |= MK_LBUTTON;
    }
    if (buttons.testFlag(Qt::RightButton)) {
        state |= MK_RBUTTON;
    }
    return state;
}

int MapQtKeyToVirtualKey(int key)
{
    if ((key >= Qt::Key_0 && key <= Qt::Key_9) || (key >= Qt::Key_A && key <= Qt::Key_Z)) {
        return key;
    }

    switch (key) {
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Tab: return VK_TAB;
    case Qt::Key_Return:
    case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Shift: return VK_SHIFT;
    case Qt::Key_Control: return VK_CONTROL;
    case Qt::Key_Alt: return VK_MENU;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_F1: return VK_F1;
    case Qt::Key_F9: return VK_F9;
    case Qt::Key_Plus:
    case Qt::Key_Equal: return VK_OEM_PLUS;
    case Qt::Key_Minus:
    case Qt::Key_Underscore: return VK_OEM_MINUS;
    default:
        return key;
    }
}

class RoQtMainWindow final : public QWindow {
public:
    explicit RoQtMainWindow(RoQtPlatformWindowProc windowProc)
        : m_windowProc(windowProc)
    {
        setSurfaceType(QSurface::VulkanSurface);
        setFlags(Qt::Window);
    }

protected:
    void focusInEvent(QFocusEvent* event) override
    {
        QWindow::focusInEvent(event);
        sendMessage(WM_ACTIVATE, WA_ACTIVE, 0);
    }

    void focusOutEvent(QFocusEvent* event) override
    {
        QWindow::focusOutEvent(event);
        sendMessage(WM_ACTIVATE, WA_INACTIVE, 0);
    }

    void closeEvent(QCloseEvent* event) override
    {
        sendMessage(WM_CLOSE, 0, 0);
        event->ignore();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const QPointF pos = event->position();
        sendMessage(WM_MOUSEMOVE, BuildMouseKeyState(event->buttons()), MakeLParam(static_cast<int>(pos.x()), static_cast<int>(pos.y())));
        QWindow::mouseMoveEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        const unsigned int msg = event->button() == Qt::RightButton ? WM_RBUTTONDOWN : WM_LBUTTONDOWN;
        const QPointF pos = event->position();
        sendMessage(msg, BuildMouseKeyState(event->buttons()), MakeLParam(static_cast<int>(pos.x()), static_cast<int>(pos.y())));
        QWindow::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        const unsigned int msg = event->button() == Qt::RightButton ? WM_RBUTTONUP : WM_LBUTTONUP;
        const QPointF pos = event->position();
        sendMessage(msg, BuildMouseKeyState(event->buttons()), MakeLParam(static_cast<int>(pos.x()), static_cast<int>(pos.y())));
        QWindow::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        const QPointF pos = event->position();
        sendMessage(WM_LBUTTONDBLCLK, BuildMouseKeyState(event->buttons()), MakeLParam(static_cast<int>(pos.x()), static_cast<int>(pos.y())));
        QWindow::mouseDoubleClickEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        const QPointF pos = event->position();
        sendMessage(WM_MOUSEWHEEL,
            MakeWheelWParam(event->angleDelta().y(), BuildMouseKeyState(event->buttons())),
            MakeLParam(static_cast<int>(pos.x()), static_cast<int>(pos.y())));
        QWindow::wheelEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        const unsigned int msg = event->modifiers().testFlag(Qt::AltModifier) ? WM_SYSKEYDOWN : WM_KEYDOWN;
        sendMessage(msg, static_cast<std::uintptr_t>(MapQtKeyToVirtualKey(event->key())), 0);

        const QString text = event->text();
        if (!text.isEmpty()) {
            const QChar character = text.front();
            if (!character.isNull() && character.unicode() >= 0x20u) {
                sendMessage(WM_CHAR, static_cast<std::uintptr_t>(character.toLatin1()), 0);
            }
        }

        QWindow::keyPressEvent(event);
    }

private:
    void sendMessage(unsigned int msg, std::uintptr_t wParam, std::intptr_t lParam)
    {
        if (m_windowProc) {
            m_windowProc(reinterpret_cast<RoNativeWindowHandle>(this), msg, wParam, lParam);
        }
    }

    RoQtPlatformWindowProc m_windowProc;
};

QGuiApplication* EnsureApplication()
{
    if (!QCoreApplication::instance()) {
#if !RO_PLATFORM_WINDOWS
        const char* qtQpaPlatform = std::getenv("QT_QPA_PLATFORM");
        const bool hasExplicitPlatform = qtQpaPlatform && *qtQpaPlatform;
        const bool isWsl = (std::getenv("WSL_DISTRO_NAME") && *std::getenv("WSL_DISTRO_NAME"))
            || (std::getenv("WSL_INTEROP") && *std::getenv("WSL_INTEROP"));
        if (isWsl && !hasExplicitPlatform) {
            qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("xcb"));
            DbgLog("[QtHost] WSL detected; defaulting QT_QPA_PLATFORM=xcb.\n");
        }
#endif
    }

    if (QCoreApplication::instance()) {
        return qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    }

    static int argc = 1;
    static char arg0[] = "open-midgard";
    static char* argv[] = { arg0, nullptr };
    return new QGuiApplication(argc, argv);
}

RoQtMainWindow* AsWindow(RoNativeWindowHandle window)
{
    return reinterpret_cast<RoQtMainWindow*>(window);
}

} // namespace

bool RoQtCreateMainWindow(const char* title,
    int width,
    int height,
    bool fullscreen,
    RoQtPlatformWindowProc windowProc,
    RoNativeWindowHandle* outWindow)
{
    if (!outWindow) {
        return false;
    }

    if (!EnsureApplication()) {
        DbgLog("[QtHost] Failed to create QGuiApplication.\n");
        return false;
    }

    RoQtMainWindow* window = new RoQtMainWindow(windowProc);
    window->setTitle(QString::fromUtf8(title ? title : "open-midgard"));
    window->resize((std::max)(width, 1), (std::max)(height, 1));
    if (fullscreen) {
        window->showFullScreen();
    } else {
        window->show();
    }

    *outWindow = reinterpret_cast<RoNativeWindowHandle>(window);
    return true;
}

void RoQtDestroyMainWindow(RoNativeWindowHandle window)
{
    RoQtMainWindow* mainWindow = AsWindow(window);
    if (!mainWindow) {
        return;
    }

    mainWindow->close();
    delete mainWindow;
}

void RoQtProcessEvents()
{
    if (!QCoreApplication::instance()) {
        return;
    }

    QCoreApplication::sendPostedEvents(nullptr, 0);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 0);
}

bool RoQtGetClientRect(RoNativeWindowHandle window, RECT* rect)
{
    RoQtMainWindow* mainWindow = AsWindow(window);
    if (!mainWindow || !rect) {
        return false;
    }

    rect->left = 0;
    rect->top = 0;
    rect->right = mainWindow->width();
    rect->bottom = mainWindow->height();
    return true;
}

bool RoQtScreenToClient(RoNativeWindowHandle window, POINT* point)
{
    RoQtMainWindow* mainWindow = AsWindow(window);
    if (!mainWindow || !point) {
        return false;
    }

    const QPoint local = mainWindow->mapFromGlobal(QPoint(point->x, point->y));
    point->x = local.x();
    point->y = local.y();
    return true;
}

bool RoQtGetCursorPos(POINT* point)
{
    if (!point) {
        return false;
    }

    const QPoint pos = QCursor::pos();
    point->x = pos.x();
    point->y = pos.y();
    return true;
}

bool RoQtSetWindowTitle(RoNativeWindowHandle window, const char* title)
{
    RoQtMainWindow* mainWindow = AsWindow(window);
    if (!mainWindow) {
        return false;
    }

    mainWindow->setTitle(QString::fromUtf8(title ? title : "open-midgard"));
    return true;
}

bool RoQtShowWindow(RoNativeWindowHandle window)
{
    RoQtMainWindow* mainWindow = AsWindow(window);
    if (!mainWindow) {
        return false;
    }

    mainWindow->show();
    return true;
}

bool RoQtFocusWindow(RoNativeWindowHandle window)
{
    RoQtMainWindow* mainWindow = AsWindow(window);
    if (!mainWindow) {
        return false;
    }

    mainWindow->requestActivate();
    return true;
}

QWindow* RoQtGetQWindow(RoNativeWindowHandle window)
{
    return AsWindow(window);
}

#endif