#include "QtPlatformWindow.h"
#include <qnamespace.h>

#if RO_ENABLE_QT6_UI

#include "DebugLog.h"
#include "ui/UIWindowMgr.h"
#include "ui/UIGroundSkillTextWnd.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QInputMethodQueryEvent>
#include <QMouseEvent>
#include <QPointF>
#include <QWheelEvent>
#include <QWindow>

#include <algorithm>
#include <cstdlib>

namespace {

constexpr unsigned int kWmMouseMove = 0x0200u;
constexpr unsigned int kWmLButtonDown = 0x0201u;
constexpr unsigned int kWmLButtonUp = 0x0202u;
constexpr unsigned int kWmLButtonDblClk = 0x0203u;
constexpr unsigned int kWmRButtonDown = 0x0204u;
constexpr unsigned int kWmRButtonUp = 0x0205u;
constexpr unsigned int kWmMouseWheel = 0x020Au;
constexpr unsigned int kWmChar = 0x0102u;
constexpr unsigned int kWmKeyDown = 0x0100u;
constexpr unsigned int kWmSysKeyDown = 0x0104u;
constexpr unsigned int kWmActivate = 0x0006u;
constexpr unsigned int kWmClose = 0x0010u;

constexpr std::uintptr_t kMkLButton = 0x0001u;
constexpr std::uintptr_t kMkRButton = 0x0002u;
constexpr std::uintptr_t kWaInactive = 0u;
constexpr std::uintptr_t kWaActive = 1u;

constexpr int kVkBack = 0x08;
constexpr int kVkTab = 0x09;
constexpr int kVkReturn = 0x0D;
constexpr int kVkShift = 0x10;
constexpr int kVkControl = 0x11;
constexpr int kVkMenu = 0x12;
constexpr int kVkEscape = 0x1B;
constexpr int kVkPrior = 0x21;
constexpr int kVkNext = 0x22;
constexpr int kVkEnd = 0x23;
constexpr int kVkHome = 0x24;
constexpr int kVkLeft = 0x25;
constexpr int kVkUp = 0x26;
constexpr int kVkRight = 0x27;
constexpr int kVkDown = 0x28;
constexpr int kVkInsert = 0x2D;
constexpr int kVkDelete = 0x2E;
constexpr int kVkF1 = 0x70;
constexpr int kVkF9 = 0x78;
constexpr int kVkF10 = 0x79;
constexpr int kVkF11 = 0x7A;
constexpr int kVkF12 = 0x7B;
constexpr int kVkOemPlus = 0xBB;
constexpr int kVkOemMinus = 0xBD;

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
        state |= kMkLButton;
    }
    if (buttons.testFlag(Qt::RightButton)) {
        state |= kMkRButton;
    }
    return state;
}

int MapQtKeyToVirtualKey(int key)
{
    if ((key >= Qt::Key_0 && key <= Qt::Key_9) || (key >= Qt::Key_A && key <= Qt::Key_Z)) {
        return key;
    }

    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        return VK_F1 + (key - Qt::Key_F1);
    }

    switch (key) {
    case Qt::Key_Backspace: return kVkBack;
    case Qt::Key_Tab: return kVkTab;
    case Qt::Key_Return:
    case Qt::Key_Enter: return kVkReturn;
    case Qt::Key_Shift: return kVkShift;
    case Qt::Key_Control: return kVkControl;
    case Qt::Key_Alt: return kVkMenu;
    case Qt::Key_Escape: return kVkEscape;
    case Qt::Key_PageUp: return kVkPrior;
    case Qt::Key_PageDown: return kVkNext;
    case Qt::Key_End: return kVkEnd;
    case Qt::Key_Home: return kVkHome;
    case Qt::Key_Left: return kVkLeft;
    case Qt::Key_Up: return kVkUp;
    case Qt::Key_Right: return kVkRight;
    case Qt::Key_Down: return kVkDown;
    case Qt::Key_Insert: return kVkInsert;
    case Qt::Key_Delete: return kVkDelete;
    case Qt::Key_Plus:
    case Qt::Key_Equal: return kVkOemPlus;
    case Qt::Key_Minus:
    case Qt::Key_Underscore: return kVkOemMinus;
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
        sendMessage(kWmActivate, kWaActive, 0);
    }

    void focusOutEvent(QFocusEvent* event) override
    {
        QWindow::focusOutEvent(event);
        sendMessage(kWmActivate, kWaInactive, 0);
    }

    void closeEvent(QCloseEvent* event) override
    {
        sendMessage(kWmClose, 0, 0);
        event->ignore();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const QPointF pos = event->position();
        sendMessage(kWmMouseMove, BuildMouseKeyState(event->buttons()), MakeLParam(static_cast<int>(pos.x()), static_cast<int>(pos.y())));
        QWindow::mouseMoveEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        const unsigned int msg = event->button() == Qt::RightButton ? kWmRButtonDown : kWmLButtonDown;
        const QPointF pos = event->position();
        sendMessage(msg, BuildMouseKeyState(event->buttons()), MakeLParam(static_cast<int>(pos.x()), static_cast<int>(pos.y())));
        QWindow::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        const unsigned int msg = event->button() == Qt::RightButton ? kWmRButtonUp : kWmLButtonUp;
        const QPointF pos = event->position();
        sendMessage(msg, BuildMouseKeyState(event->buttons()), MakeLParam(static_cast<int>(pos.x()), static_cast<int>(pos.y())));
        QWindow::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        const QPointF pos = event->position();
        sendMessage(kWmLButtonDblClk, BuildMouseKeyState(event->buttons()), MakeLParam(static_cast<int>(pos.x()), static_cast<int>(pos.y())));
        QWindow::mouseDoubleClickEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        const QPointF pos = event->position();
        sendMessage(kWmMouseWheel,
            MakeWheelWParam(event->angleDelta().y(), BuildMouseKeyState(event->buttons())),
            MakeLParam(static_cast<int>(pos.x()), static_cast<int>(pos.y())));
        QWindow::wheelEvent(event);
    }

    bool event(QEvent* event) override
    {
        if (auto* prompt = g_windowMgr.GetFocusedGroundSkillText()) {
            if (event->type() == QEvent::InputMethod) {
                auto* input = static_cast<QInputMethodEvent*>(event);
                prompt->CommitText(input->commitString().toStdU16String());
                return true;
            }
            if (event->type() == QEvent::InputMethodQuery) {
                auto* query = static_cast<QInputMethodQueryEvent*>(event);
                query->setValue(Qt::ImEnabled, true);
                const auto* edit = prompt->GetEditCtrl();
                query->setValue(Qt::ImCursorRectangle, QRect(edit->m_x, edit->m_y, 1, edit->m_h));
                return true;
            }
        }
        return QWindow::event(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        const int virtualKey = MapQtKeyToVirtualKey(event->key());
        if (auto* prompt = g_windowMgr.GetFocusedGroundSkillText()) {
            const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
            const bool control = event->modifiers().testFlag(Qt::ControlModifier);
            HWND clipboardOwner = nullptr;
#if defined(_WIN32)
            clipboardOwner = reinterpret_cast<HWND>(winId());
#endif
            if (!(event->modifiers() & (Qt::AltModifier | Qt::GroupSwitchModifier))
                && prompt->HandleClipboardKey(virtualKey, shift, control, clipboardOwner)) {
                event->accept();
                return;
            }
            if (!control || shift) prompt->HandleEditKey(virtualKey,shift);
            if (!(event->modifiers() & (Qt::AltModifier | Qt::ControlModifier))
                || event->modifiers().testFlag(Qt::GroupSwitchModifier))
                prompt->CommitText(event->text().toStdU16String());
            event->accept();
            return;
        }
        if (g_windowMgr.OnQtKeyDown(
                virtualKey,
                event->modifiers().testFlag(Qt::AltModifier),
                event->modifiers().testFlag(Qt::ControlModifier),
                event->modifiers().testFlag(Qt::ShiftModifier))) {
            event->accept();
            return;
        }

        const unsigned int msg = event->modifiers().testFlag(Qt::AltModifier) ? kWmSysKeyDown : kWmKeyDown;
        sendMessage(msg, static_cast<std::uintptr_t>(virtualKey), 0);

        const QString text = event->text();
        if (!text.isEmpty()) {
            const QChar character = text.front();
            if (!character.isNull() && character.unicode() >= 0x20u) {
                sendMessage(kWmChar, static_cast<std::uintptr_t>(character.toLatin1()), 0);
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
    window->setCursor(Qt::BlankCursor);
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
