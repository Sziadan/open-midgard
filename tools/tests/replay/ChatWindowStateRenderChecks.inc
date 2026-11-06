#include "ui/UINewChatWnd.h"

void CheckChatWindowStateUi(QGuiApplication& app, QQuickView& view, QtUiState* state,
    CGameMode& mode, QtUiStateAdapter& adapter, const QString& output)
{
    Require(state != nullptr, "chat state Qt view missing");
    SetRuntimeUiScalePercent(100);
    if (g_windowMgr.m_itemWnd) g_windowMgr.m_itemWnd->SetShow(0);
    if (g_windowMgr.m_storageWnd) g_windowMgr.m_storageWnd->SetShow(0);
    ro::commerce::state.cart.visible = false;
    g_windowMgr.EnsureChatWindowVisible();
    auto* chat = g_windowMgr.m_chatWnd;
    chat->Resize(600, chat->m_h);
    chat->SetWindowFocused(true);
    chat->SetQtInputText(UINewChatWnd::InputField_WhisperTarget, "SourceTarget");
    chat->SetQtInputText(UINewChatWnd::InputField_Message, "compact editor remains available");
    g_windowMgr.ClearChatEvents();
    g_windowMgr.PushChatEvent("SOURCE CHAT HISTORY MARKER", 0x00ff2020, 0);
    const auto redPixels = [](const QImage& image, QRect area) {
        int count = 0;
        area = area.intersected(image.rect());
        for (int y = area.top(); y <= area.bottom(); ++y)
            for (int x = area.left(); x <= area.right(); ++x) {
                const auto color = image.pixelColor(x, y);
                if (color.red() > 150 && color.green() < 120 && color.blue() < 120) ++count;
            }
        return count;
    };
    QJsonArray cases;
    for (const int value : {0, 1, 7, 15}) {
        chat->SetState(value);
        chat->Move(20, view.height() - 20 - chat->m_h);
        chat->OnProcess();
        Require(adapter.syncGameplay(mode, RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7, -1, -1), "chat state gameplay projection failed");
        Frames(app, view);
        auto* history = Find(view.rootObject(), "chatHistoryPane");
        auto* whisper = Find(view.rootObject(), "chatWhisperField");
        auto* message = Find(view.rootObject(), "chatMessageField");
        Require(history && whisper && message, "actual chat QML regions missing");
        Require(history->isVisible() == (value != 0) && whisper->isVisible() && message->isVisible(),
            "compact chat hid editors or retained history");
        const auto rects = chat->GetLayoutDisplay();
        const auto editorTop = message->mapToScene(QPointF(0, 0));
        Require(qAbs(editorTop.x() - chat->m_x - rects.messageInput.left) < .01
            && qAbs(editorTop.y() - chat->m_y - rects.messageInput.top) < .01
            && qRound(message->width()) == 460 && qRound(message->height()) == 16,
            "actual QML editor differs from native source rectangle");
        const auto point = message->mapToScene(QPointF(10, 8));
        Require(chat->HandleQtMouseDown(qRound(point.x()), qRound(point.y())), "source editor pointer rejected");
        chat->HandleQtMouseUp(qRound(point.x()), qRound(point.y()));
        Require(chat->IsMessageInputActive(), "source editor pointer failed to focus message input");
        adapter.syncChat();
        Frames(app, view);
        auto* editor = Find(view.rootObject(), "chatMessageEditor");
        Require(editor && editor->hasActiveFocus(), "actual Qt editor did not receive native owner focus");

        const QImage qt = view.grabWindow().convertToFormat(QImage::Format_RGB32);
        const QRect historyBounds(chat->m_x + rects.history.left, chat->m_y + rects.history.top,
            rects.history.right - rects.history.left, rects.history.bottom - rects.history.top);
        const int qtRed = redPixels(qt, historyBounds);
        ArgbDibSurface surface;
        Require(surface.EnsureSize(view.width(), view.height()), "native chat state DIB failed");
        std::fill_n(surface.GetPixels(), view.width() * view.height(), 0xff181b20u);
        chat->DrawNativeToHdc(surface.GetDC());
        const QImage native = QImage(reinterpret_cast<uchar*>(surface.GetPixels()), view.width(), view.height(),
            view.width() * 4, QImage::Format_RGB32).copy();
        const int nativeRed = redPixels(native, historyBounds);
        Require((qtRed > 20) == (value != 0) && (nativeRed > 20) == (value != 0),
            "native/Qt state did not paint or hide the source history");
        const QString stem = "chat-state-" + QString::number(value);
        Require(qt.save(output + "/" + stem + ".qml.png")
            && native.save(output + "/" + stem + ".native.png"), "chat state frame save failed");
        cases.push_back(QJsonObject{{"state",value},{"height",chat->m_h},{"bottom",chat->m_y+chat->m_h},
            {"qt_history_pixels",qtRed},{"native_history_pixels",nativeRed},{"editor_focused",true}});
    }
    chat->SetState(7);
    chat->StickOnOff(false);
    g_windowMgr.m_onStSubChat = g_windowMgr.m_onBtSubChat = true;
    g_windowMgr.UpdateChatAuxiliaryVisibility();
    auto* st = g_windowMgr.m_subChatWndST;
    auto* bt = g_windowMgr.m_subChatWndBT;
    Require(st && bt, "render fixture missing managed histories");
    st->Move(20, 260); bt->Move(340, 260);
    st->Append("ST INDEPENDENT HISTORY", 0xff2020, "SourceST");
    bt->Append("BT INDEPENDENT HISTORY", 0xff2020, "SourceBT");
    adapter.syncChat(); Frames(app, view);
    auto* stItem = Find(view.rootObject(), "detachedChatHistory0");
    auto* btItem = Find(view.rootObject(), "detachedChatHistory1");
    auto* miniItem = Find(view.rootObject(), "detachedChatMini");
    Require(stItem && btItem && miniItem && stItem->isVisible() && btItem->isVisible()
        && miniItem->isVisible() && qRound(stItem->width()) == 300 && qRound(stItem->height()) == 95,
        "actual auxiliary QML geometry/visibility diverged");
    const auto qtAux = view.grabWindow().convertToFormat(QImage::Format_RGB32);
    ArgbDibSurface auxSurface;
    Require(auxSurface.EnsureSize(view.width(), view.height()), "auxiliary DIB failed");
    std::fill_n(auxSurface.GetPixels(), view.width() * view.height(), 0xff181b20u);
    chat->DrawNativeToHdc(auxSurface.GetDC());
    st->DrawToHdc(auxSurface.GetDC()); bt->DrawToHdc(auxSurface.GetDC());
    g_windowMgr.m_subChatMiniWnd->DrawToHdc(auxSurface.GetDC());
    const auto nativeAux = QImage(reinterpret_cast<uchar*>(auxSurface.GetPixels()), view.width(), view.height(),
        view.width() * 4, QImage::Format_RGB32).copy();
    for (auto* history : {st, bt}) {
        const QRect bounds(history->m_x, history->m_y + 18, history->m_w, history->m_h - 35);
        Require(redPixels(qtAux, bounds) > 20 && redPixels(nativeAux, bounds) > 20,
            "actual detached native/Qt text pixels missing");
    }
    Require(qtAux.save(output + "/chat-detached.qml.png")
        && nativeAux.save(output + "/chat-detached.native.png"), "save detached frames failed");
#if RO_PLATFORM_WINDOWS
    // Real OS capture and the native WindowProc route, using hidden fixture HWNDs.
    struct HiddenCaptureWindows {
        HWND previous = g_hMainWnd;
        HWND first = CreateWindowExA(0, "STATIC", "chat capture fixture", WS_POPUP,
            0, 0, 1280, 720, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        HWND second = CreateWindowExA(0, "STATIC", "capture replacement fixture", WS_POPUP,
            0, 0, 32, 32, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        ~HiddenCaptureWindows() {
            if (GetCapture() == first || GetCapture() == second) ReleaseCapture();
            g_hMainWnd = previous;
            if (second) DestroyWindow(second);
            if (first) DestroyWindow(first);
        }
    } captureWindows;
    Require(captureWindows.first && captureWindows.second, "hidden native capture fixture creation failed");
    g_hMainWnd = captureWindows.first;
    const auto point = [](int x, int y) { return MAKELPARAM(static_cast<WORD>(x), static_cast<WORD>(y)); };
    WindowProc(captureWindows.first, WM_LBUTTONDOWN, MK_LBUTTON, point(30, 268));
    Require(GetCapture() == captureWindows.first && g_windowMgr.m_captureWindow == st,
        "native detached title did not acquire real OS capture");
    WindowProc(captureWindows.first, WM_MOUSEMOVE, MK_LBUTTON, point(-50, -30));
    WindowProc(captureWindows.first, WM_LBUTTONUP, 0, point(-50, -30));
    Require(GetCapture() == nullptr && !g_windowMgr.m_captureWindow && st->m_x == -60 && st->m_y == -38,
        "outside-client native release lost drag or retained capture");
    st->Move(20, 260);
    WindowProc(captureWindows.first, WM_LBUTTONDOWN, MK_LBUTTON, point(30, 268));
    SetCapture(captureWindows.second);
    WindowProc(captureWindows.first, WM_CAPTURECHANGED, 0, reinterpret_cast<LPARAM>(captureWindows.second));
    Require(GetCapture() == captureWindows.second && !g_windowMgr.m_captureWindow,
        "lost capture retained native owner or released replacement owner");
    WindowProc(captureWindows.first, WM_MOUSEMOVE, MK_LBUTTON, point(-50, -30));
    Require(st->m_x == 20 && st->m_y == 260, "lost capture kept dragging history");
    ReleaseCapture();
    WindowProc(captureWindows.first, WM_LBUTTONDOWN, MK_LBUTTON, point(30, 268));
    WindowProc(captureWindows.first, WM_CANCELMODE, 0, 0);
    Require(GetCapture() == nullptr && !g_windowMgr.m_captureWindow, "cancel mode retained OS/native capture");
    chat->StickOnOff(true); chat->Move(20, 500);
    const auto detach = chat->GetLayoutDisplay().stickButton;
    WindowProc(captureWindows.first, WM_LBUTTONDOWN, MK_LBUTTON,
        point(chat->m_x + detach.left + 7, chat->m_y + detach.top + 7));
    Require(chat->GetExpansionState() == 0 && !g_windowMgr.m_chatWndStickOn,
        "native inline detach button was mistaken for captured frame");
    WindowProc(captureWindows.first, WM_LBUTTONUP, 0, point(-10, -10));
    Require(GetCapture() == nullptr && !g_windowMgr.m_captureWindow, "native main release retained OS capture");
#endif
    chat->StickOnOff(true); adapter.syncChat(); Frames(app, view);
    Require(!miniItem->isVisible(), "actual mini survived attach");
    Require(view.grabWindow().save(output + "/chat-reattached.qml.png"), "save reattached frame failed");
    QFile file(output + "/chat-state-results.json");
    Require(file.open(QIODevice::WriteOnly), "chat state results open failed");
    file.write(QJsonDocument(QJsonObject{{"cases",cases},{"main_state_phase","PASS"},
        {"auxiliary_lifecycle_pending",true}}).toJson());
}
