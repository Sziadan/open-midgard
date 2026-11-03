#include "QtChatInput.h"
#include <QCoreApplication>
#include <QInputMethodEvent>
#include <QInputMethodQueryEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTextCharFormat>
#include <QWheelEvent>
#include <array>
#include <algorithm>

#if RO_PLATFORM_WINDOWS
#include <windows.h>
#include <imm.h>

namespace {
Qt::KeyboardModifiers Modifiers() {
    Qt::KeyboardModifiers result=Qt::NoModifier;
    if(GetKeyState(VK_SHIFT)&0x8000)result|=Qt::ShiftModifier;
    if(GetKeyState(VK_CONTROL)&0x8000)result|=Qt::ControlModifier;
    if(GetKeyState(VK_MENU)&0x8000)result|=Qt::AltModifier;
    // AltGr produces Ctrl+Alt on Windows but its WM_CHAR text is still input.
    return result;
}
int Key(unsigned key) {
    switch(key) {
    case VK_LEFT:return Qt::Key_Left;case VK_RIGHT:return Qt::Key_Right;
    case VK_UP:return Qt::Key_Up;case VK_DOWN:return Qt::Key_Down;
    case VK_HOME:return Qt::Key_Home;case VK_END:return Qt::Key_End;
    case VK_PRIOR:return Qt::Key_PageUp;case VK_NEXT:return Qt::Key_PageDown;
    case VK_BACK:return Qt::Key_Backspace;case VK_DELETE:return Qt::Key_Delete;
    case VK_INSERT:return Qt::Key_Insert;case VK_TAB:return Qt::Key_Tab;
    case VK_RETURN:return Qt::Key_Return;case VK_ESCAPE:return Qt::Key_Escape;
    default:return key;
    }
}
QString Composition(HIMC context,DWORD kind) {
    const LONG bytes=ImmGetCompositionStringW(context,kind,nullptr,0);
    if(bytes<=0 || bytes>65536 || bytes%2)return {};
    QString text(bytes/2,Qt::Uninitialized);
    ImmGetCompositionStringW(context,kind,text.data(),bytes);
    return text;
}
}
#endif

bool QtChatInput::handle(RoNativeWindowHandle nativeWindow,RoWindowMessage msg,RoWindowWParam wParam,RoWindowLParam lParam,bool enabled)
{
#if RO_PLATFORM_WINDOWS
    if(!m_window || !m_root)return false;
    const std::array<const char*,3> editors={"chatHistoryEditor","chatWhisperEditor","chatMessageEditor"};
    auto editor=[&](int index) {return m_root->findChild<QQuickItem*>(editors[index]);};
    auto focused=[&]() -> QQuickItem* {
        for(int i=0;i<3;++i)if(auto* item=editor(i);item && item->isVisible() && item->hasActiveFocus())return item;
        return nullptr;
    };
    if(!enabled) {
        for(int i=0;i<3;++i)if(auto* item=editor(i))item->setFocus(false);
        if(m_pointerGrab && GetCapture()==nativeWindow)ReleaseCapture();
        m_pointerGrab=false;m_composing=false;m_highSurrogate={};return false;
    }
    const QPointF point(static_cast<short>(lParam&0xFFFF),static_cast<short>((lParam>>16)&0xFFFF));
    const std::array<const char*,3> regions={"chatHistoryPane","chatWhisperField","chatMessageField"};
    int hit=-1;
    for(int i=0;i<3;++i)if(auto* item=m_root->findChild<QQuickItem*>(regions[i]);item && item->isVisible() && item->contains(item->mapFromScene(point)))hit=i;
    if(msg==WM_LBUTTONDOWN || msg==WM_LBUTTONDBLCLK || msg==WM_LBUTTONUP || msg==WM_MOUSEMOVE) {
        const bool down=msg==WM_LBUTTONDOWN || msg==WM_LBUTTONDBLCLK;
        if(down && hit<0) {
            // Header/tab/resize gestures keep the current editor focused.
            // Clicks outside the chat panel relinquish keyboard focus.
            auto* panel=m_root->findChild<QQuickItem*>("chatWindowPanel");
            if(!panel)panel=m_root;
            if(panel->isVisible() && panel->contains(panel->mapFromScene(point)))return false;
            for(int i=0;i<3;++i)if(auto* item=editor(i))item->setFocus(false);
            m_highSurrogate={};return false;
        }
        if(hit<0 && !m_pointerGrab)return false;
        if(down) {
            if(auto* item=editor(hit))item->forceActiveFocus();
            SetCapture(nativeWindow);m_pointerGrab=true;
        }
        const auto type=msg==WM_MOUSEMOVE?QEvent::MouseMove:msg==WM_LBUTTONUP?QEvent::MouseButtonRelease:msg==WM_LBUTTONDBLCLK?QEvent::MouseButtonDblClick:QEvent::MouseButtonPress;
        const auto button=msg==WM_MOUSEMOVE?Qt::NoButton:Qt::LeftButton;
        QMouseEvent event(type,point,point,button,(wParam&MK_LBUTTON)?Qt::LeftButton:Qt::NoButton,Modifiers());
        QCoreApplication::sendEvent(m_window,&event);
        if(msg==WM_LBUTTONUP) {m_pointerGrab=false;if(GetCapture()==nativeWindow)ReleaseCapture();}
        return true;
    }
    if(msg==WM_CAPTURECHANGED) {m_pointerGrab=false;return false;}
    if(msg==WM_MOUSEWHEEL) {
        POINT client{static_cast<LONG>(point.x()),static_cast<LONG>(point.y())};ScreenToClient(nativeWindow,&client);
        auto* pane=m_root->findChild<QQuickItem*>("chatHistoryPane");
        if(!pane || !pane->isVisible() || !pane->contains(pane->mapFromScene({static_cast<qreal>(client.x),static_cast<qreal>(client.y)})))return false;
        QWheelEvent event(QPointF(client.x,client.y),point,{},QPoint(0,static_cast<short>((wParam>>16)&0xFFFF)),Qt::NoButton,Modifiers(),Qt::NoScrollPhase,false);
        QCoreApplication::sendEvent(m_window,&event);return true;
    }
    auto* focus=focused();if(!focus)return false;
    auto commit=[&](const QString& text) {QInputMethodEvent event;event.setCommitString(text);QCoreApplication::sendEvent(m_window,&event);};
    if(msg==WM_KEYDOWN || msg==WM_SYSKEYDOWN || msg==WM_KEYUP || msg==WM_SYSKEYUP) {
        if(wParam==VK_PROCESSKEY)return false;
        if(m_composing)return true;
        const auto type=msg==WM_KEYUP || msg==WM_SYSKEYUP?QEvent::KeyRelease:QEvent::KeyPress;
        const int key=wParam==VK_TAB && (Modifiers()&Qt::ShiftModifier)?Qt::Key_Backtab:Key(wParam);
        QKeyEvent event(type,key,Modifiers(),{},(lParam&(1u<<30))!=0);
        QCoreApplication::sendEvent(m_window,&event);return true;
    }
    if(msg==WM_CHAR || msg==WM_UNICHAR) {
        if(msg==WM_UNICHAR) {
            if(wParam==UNICODE_NOCHAR)return true;
            const char32_t code=static_cast<char32_t>(wParam);if(code>=32 && code<=0x10FFFF)commit(QString::fromUcs4(&code,1));return true;
        }
        const QChar character(static_cast<ushort>(wParam));
        if(character.unicode()<32)return true;
        if(character.isHighSurrogate()) {m_highSurrogate=character;return true;}
        QString text;
        if(character.isLowSurrogate()) {if(m_highSurrogate.isNull())return true;text+=m_highSurrogate;}
        m_highSurrogate={};text+=character;commit(text);return true;
    }
    if(msg==WM_IME_SETCONTEXT) {
        DefWindowProcW(nativeWindow,msg,wParam,lParam&~ISC_SHOWUICOMPOSITIONWINDOW);return true;
    }
    if(msg==WM_IME_STARTCOMPOSITION || msg==WM_IME_COMPOSITION || msg==WM_IME_ENDCOMPOSITION) {
        m_highSurrogate={};
        HIMC context=ImmGetContext(nativeWindow);if(!context)return false;
        if(msg==WM_IME_ENDCOMPOSITION) {QInputMethodEvent event;QCoreApplication::sendEvent(m_window,&event);m_composing=false;}
        else {
            if(msg==WM_IME_COMPOSITION && (lParam&GCS_RESULTSTR)) {commit(Composition(context,GCS_RESULTSTR));m_composing=false;}
            if(msg==WM_IME_STARTCOMPOSITION || (lParam&GCS_COMPSTR)) {
                const QString preedit=Composition(context,GCS_COMPSTR);
                QList<QInputMethodEvent::Attribute> attributes;
                QTextCharFormat format;format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
                attributes.append({QInputMethodEvent::TextFormat,0,static_cast<int>(preedit.size()),format});
                const LONG cursor=ImmGetCompositionStringW(context,GCS_CURSORPOS,nullptr,0);
                attributes.append({QInputMethodEvent::Cursor,std::clamp<int>(cursor,0,preedit.size()),1,{}});
                QInputMethodEvent event(preedit,attributes);QCoreApplication::sendEvent(m_window,&event);m_composing=!preedit.isEmpty();
            }
            QInputMethodQueryEvent query(Qt::ImCursorRectangle);QCoreApplication::sendEvent(focus,&query);
            const QRectF caret=focus->mapRectToScene(query.value(Qt::ImCursorRectangle).toRectF());
            CANDIDATEFORM candidate{};candidate.dwStyle=CFS_EXCLUDE;
            candidate.ptCurrentPos={static_cast<LONG>(caret.left()),static_cast<LONG>(caret.bottom())};
            candidate.rcArea={static_cast<LONG>(caret.left()),static_cast<LONG>(caret.top()),static_cast<LONG>(caret.right()+1),static_cast<LONG>(caret.bottom()+1)};
            ImmSetCandidateWindow(context,&candidate);
            COMPOSITIONFORM composition{};composition.dwStyle=CFS_POINT;composition.ptCurrentPos=candidate.ptCurrentPos;
            ImmSetCompositionWindow(context,&composition);
        }
        ImmReleaseContext(nativeWindow,context);return true;
    }
    if(msg==WM_KILLFOCUS) {QInputMethodEvent event;QCoreApplication::sendEvent(m_window,&event);m_composing=false;m_highSurrogate={};}
#else
    (void)nativeWindow;(void)msg;(void)wParam;(void)lParam;(void)enabled;
#endif
    return false;
}
