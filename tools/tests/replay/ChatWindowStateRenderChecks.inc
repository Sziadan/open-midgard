#include "ChatScrollTrimRenderChecks.inc"
#include "ChatRoomRenderChecks.inc"
#include "ChatRoomEntryRenderChecks.inc"
#include "ChatRoomSettingsRenderChecks.inc"
#include "ChatRoomActionsRenderChecks.inc"
#include "WhisperRenderChecks.inc"
#include "AutoMessageRenderChecks.inc"
#include "ChatBattleOptionsRenderChecks.inc"
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
    g_windowMgr.PushSourceChatEvent("SOURCE CHAT HISTORY MARKER", 0x00ff2020, 0, ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::None);
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
    g_windowMgr.PushSourceChatEvent("ST INDEPENDENT HISTORY", 0xff2020, 6, ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::None, "SourceST");
    g_windowMgr.PushSourceChatEvent("BT INDEPENDENT HISTORY", 0xff2020, 6, ro::chat::NoticeRoute::Command, ro::chat::NoticeType::None, "SourceBT");
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
    chat->StickOnOff(false);
    g_windowMgr.UpdateChatAuxiliaryVisibility();
    g_windowMgr.DeleteWindow(chat);
    adapter.syncChat(); Frames(app, view);
    auto* retainedSt = Find(view.rootObject(), "detachedChatHistory0");
    auto* retainedBt = Find(view.rootObject(), "detachedChatHistory1");
    Require(!state->chatWindowVisible() && retainedSt && retainedSt->isVisible()
        && retainedBt && retainedBt->isVisible() && miniItem->isVisible(),
        "actual Qt retained auxiliary family disappeared with main");
    Require(view.grabWindow().save(output + "/chat-retained-without-main.qml.png"), "save retained-owner Qt frame failed");
    std::fill_n(auxSurface.GetPixels(), view.width() * view.height(), 0xff181b20u);
    st->DrawToHdc(auxSurface.GetDC()); bt->DrawToHdc(auxSurface.GetDC());
    g_windowMgr.m_subChatMiniWnd->DrawToHdc(auxSurface.GetDC());
    Require(QImage(reinterpret_cast<uchar*>(auxSurface.GetPixels()), view.width(), view.height(),
        view.width() * 4, QImage::Format_RGB32).save(output + "/chat-retained-without-main.native.png"),
        "save retained-owner native frame failed");
    // Primary source rows must outlive the50-event receipt cache and render from
    // shared source offsets, not an independent QML scrolling document.
    g_windowMgr.EnsureChatWindowVisible();
    auto* primary = g_windowMgr.m_chatWnd;
    primary->StickOnOff(true); primary->SetState(15); primary->Move(20,40);
    primary->SetWindowFocused(true);
    for (int i = 0; i < 240; ++i) {
        const std::string text = i == 0 ? "SOURCE ROW ZERO - OLDER THAN RECEIPT CACHE" : "source row " + std::to_string(i);
        g_windowMgr.PushSourceChatEvent(text.c_str(), i == 0 ? 0xff2020 : 0xaaaaaa, 6,
            ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::None, "SourceZero");
    }
    primary->ScrollSourceHistory(-1000);
    Require(primary->GetLines().size() == 50 && primary->GetSourceHistory(false).size() == 240
        && primary->GetVisibleLines().size() == 17 && primary->GetVisibleLines().front().text.find("SOURCE ROW ZERO") == 0,
        "primary source rows were reduced to the receipt cache");
    for (int i=0;i<270;++i) g_windowMgr.PushChatEvent("UNCLASSIFIED RECEIPT",0x20ff20,0);
    Require(g_windowMgr.GetChatEvents().size()==256 && primary->GetLines().size()==50,
        "manager receipt cap and primary cache cap were confused");
    const auto capturePrimary = [&](const QString& name, bool expectRed) {
        adapter.syncChat(); Frames(app,view);
        const auto projected = state->chatWindowLines();
        Require(projected.size() == static_cast<int>(primary->GetVisibleLines().size()), "actual Qt row projection differs from native source view");
        const auto geometry = primary->GetLayoutDisplay();
        const QRect area(primary->m_x + geometry.history.left, primary->m_y + geometry.history.top,
            geometry.history.right - geometry.history.left, geometry.history.bottom - geometry.history.top);
        const auto qt = view.grabWindow().convertToFormat(QImage::Format_RGB32);
        ArgbDibSurface dib;
        Require(dib.EnsureSize(view.width(),view.height()), "primary native DIB allocation failed");
        std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
        primary->DrawNativeToHdc(dib.GetDC());
        const auto native = QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),
            view.width()*4,QImage::Format_RGB32).copy();
        Require((redPixels(qt,area)>20)==expectRed && (redPixels(native,area)>20)==expectRed,
            "native/Qt primary stream pixels diverged");
        if (expectRed) {
            // Rows after the first are gray170 over a darker background. Straight-alpha
            // pixels passed to the premultiplied native blender exceed this bound.
            for (int y=area.top()+21; y<area.top()+area.height(); ++y)
                for (int x=area.left()+2; x<area.left()+area.width()-13; ++x) {
                    const auto pixel=native.pixel(x,y);
                    Require(qRed(pixel)<=170 && qGreen(pixel)<=170 && qBlue(pixel)<=170,
                        "native source text violates premultiplied alpha range");
                }
        }
        Require(qt.save(output+"/"+name+".qml.png") && native.save(output+"/"+name+".native.png"), "save primary source frames failed");
        auto* firstRow = Find(view.rootObject(),"sourceChatRow0");
        Require(firstRow && qRound(firstRow->y()) == 3, "actual first source row is not top aligned");
        auto* secondRow = Find(view.rootObject(),"sourceChatRow1");
        if (secondRow) Require(qRound(secondRow->y()-firstRow->y())==14, "actual source row pitch differs from14px");
    };
    const auto tapPrimaryArrow=[&](RECT r){
        const int x=primary->m_x+r.left+1,y=primary->m_y+(r.top+r.bottom)/2;
        WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
        WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    };
    tapPrimaryArrow(primary->GetLayoutDisplay().scrollDown);
    Require(primary->GetSourceScrollBar().position==1,"actual primary lower arrow did not step one");
    tapPrimaryArrow(primary->GetLayoutDisplay().scrollUp);
    Require(primary->GetSourceScrollBar().position==0 && !GetCapture(),"actual primary upper arrow/release differs");
    capturePrimary("chat-primary-oldest",true);
    Require(state->chatWindowLines().front().toMap().value("text").toString().startsWith("SOURCE ROW ZERO"),
        "actual QML cannot show source rows older than50 receipts");
    g_windowMgr.PushSourceChatEvent("BT ONLY SOURCE MESSAGE",0x20ff20,0,
        ro::chat::NoticeRoute::Command,ro::chat::NoticeType::None,"CommandPeer");
    const auto click = [&](RECT bounds) {
        const int x = primary->m_x + bounds.left + 5, y = primary->m_y + bounds.top + 7;
        WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
        WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    };
    click(primary->GetLayoutDisplay().tabs[1]);
    Require(primary->GetSourceTab()==1 && primary->GetVisibleLines().size()==1
        && primary->GetVisibleLines().front().text=="BT ONLY SOURCE MESSAGE", "native pointer failed to select command source tab");
    capturePrimary("chat-primary-command",false);
    for (int i=1;i<40;++i) {
        const auto text="command row "+std::to_string(i);
        g_windowMgr.PushSourceChatEvent(text.c_str(),0x20ff20,0,
            ro::chat::NoticeRoute::Command,ro::chat::NoticeType::None,"CommandPeer");
    }
    primary->ScrollSourceHistory(-9);
    Require(primary->GetScrollBarState().firstVisibleLine==14, "command view did not scroll away from bottom");
    click(primary->GetLayoutDisplay().tabs[0]);
    Require(primary->GetScrollBarState().firstVisibleLine==223, "pointer tab switch did not invoke source resize snap");
    click(primary->GetLayoutDisplay().tabs[1]);
    Require(primary->GetScrollBarState().firstVisibleLine==23, "command tab failed to snap to bottom on return");
    click(primary->GetLayoutDisplay().tabs[0]);
    auto scrollLayout=primary->GetLayoutDisplay();
    const int thumbX=primary->m_x+scrollLayout.scrollThumb.left+2;
    const int thumbY=primary->m_y+scrollLayout.scrollThumb.top+2;
    WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(thumbX,thumbY));
    WindowProc(g_hMainWnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(thumbX,primary->m_y+scrollLayout.scrollTrack.top+2));
    WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(-20,-20));
    Require(primary->GetScrollBarState().firstVisibleLine==0 && !primary->IsQtPointerCaptured(),
        "shared source scrollbar failed drag-to-top/outside release");
    capturePrimary("chat-primary-scroll-top",true);
    const auto historyRect=primary->GetLayoutDisplay().history;
    click({historyRect.left,historyRect.top,historyRect.right-13,historyRect.bottom});
    WindowProc(g_hMainWnd,WM_LBUTTONDBLCLK,MK_LBUTTON,
        MAKELPARAM(primary->m_x+historyRect.left+5,primary->m_y+historyRect.top+7));
    // Double-click is the second down; complete its matching up before detach.
    WindowProc(g_hMainWnd,WM_LBUTTONUP,0,
        MAKELPARAM(primary->m_x+historyRect.left+5,primary->m_y+historyRect.top+7));
    Require(primary->GetWhisperTargetText()=="SourceZero" && primary->IsMessageInputActive(),
        "native source row doubleclick did not select its stored talker");
    adapter.syncChat(); Frames(app,view);
    Require(view.grabWindow().save(output+"/chat-primary-talker.qml.png"), "save source talker frame failed");
    QFile primaryResults(output+"/chat-primary-results.json");
    Require(primaryResults.open(QIODevice::WriteOnly), "primary results open failed");
    primaryResults.write(QJsonDocument(QJsonObject{{"source_rows",240},{"legacy_view_rows",50},{"manager_receipts",256},{"row_pitch",14},
        {"tabs",2},{"resize_snap",true},{"shared_scrollbar",true},{"talker_selection",true},{"outcome","PASS"}}).toJson());

    // Detached logical messages and wrapped visual rows have independent caps.
    auto* wrappedST=static_cast<UISubChatHisWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_SUBCHATST));
    auto* wrappedBT=static_cast<UISubChatHisWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_SUBCHATBT));
    g_windowMgr.MakeWindow(UIWindowMgr::WID_SUBCHATMINI);
    primary->StickOnOff(false);
    Require(primary->GetExpansionState()==0,"completed primary talker gesture left false frame capture");
    g_windowMgr.m_onStSubChat=g_windowMgr.m_onBtSubChat=true;
    g_windowMgr.UpdateChatAuxiliaryVisibility();
    wrappedST->Resize(300,106);wrappedST->Move(20,380);
    wrappedBT->Resize(280,106);wrappedBT->Move(340,380);
    for (int i=0;i<121;++i) {
        const auto text="older-"+std::to_string(i)+"\r\ncontinuation-"+std::to_string(i);
        wrappedST->Append(text.c_str(),0xff2020,"DetachedPeer");
    }
    const std::string longDetached(150,'M');
    wrappedBT->Append(longDetached.c_str(),0x20ff20,"WrappedPeer");
    const int narrowRows=static_cast<int>(wrappedBT->GetVisualRows().size());
    Require(wrappedST->GetRows().size()==121 && wrappedST->GetVisualRows().size()==122
        && wrappedST->GetVisualRows().front().text=="older-60" && narrowRows>1,
        "actual detached fixture lacks independent capped/wrapped rows");
    wrappedST->ScrollHistory(-1000);
    const auto captureDetached=[&](const QString& name) {
        adapter.syncChat();Frames(app,view);
        const auto layout=wrappedST->GetHistoryLayout();
        const QRect area(wrappedST->m_x+layout.history.left,wrappedST->m_y+layout.history.top,
            layout.history.right-layout.history.left,layout.history.bottom-layout.history.top);
        auto* row=Find(view.rootObject(),"detachedChatRow0_0");
        Require(row && row->isVisible() && row->property("text").toString()
            ==QString::fromUtf8(wrappedST->GetVisibleRows().front().text.c_str()),
            "actual detached QML visual row differs from shared native view");
        const auto location=row->mapToScene(QPointF(0,0));
        Require(qRound(location.y())==area.top()+3,"detached source row y20+3 differs");
        auto* viewport=Find(view.rootObject(),"detachedChatViewport0");
        Require(viewport && viewport->mapRectToScene(viewport->boundingRect()).toAlignedRect()==area,
            "actual detached source viewport geometry differs");
        const auto qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"detached visual native DIB failed");
        std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
        wrappedST->DrawToHdc(dib.GetDC());wrappedBT->DrawToHdc(dib.GetDC());
        const auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),
            view.width()*4,QImage::Format_RGB32).copy();
        Require(redPixels(qt,area)>20 && redPixels(native,area)>20,"detached visual source pixels missing");
        Require(qt.save(output+"/"+name+".qml.png") && native.save(output+"/"+name+".native.png"),
            "save detached visual frames failed");
    };
    const auto tapDetachedArrow=[&](RECT r){
        const int x=wrappedST->m_x+r.left+1,y=wrappedST->m_y+(r.top+r.bottom)/2;
        WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
        WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    };
    tapDetachedArrow(wrappedST->GetHistoryLayout().scrollDown);
    Require(wrappedST->GetScrollModel().position==1,"actual detached lower arrow differs");
    tapDetachedArrow(wrappedST->GetHistoryLayout().scrollUp);
    Require(wrappedST->GetScrollModel().position==0 && !GetCapture(),"actual detached upper arrow/release differs");
    captureDetached("chat-wrapped-oldest");
    auto scroll=wrappedST->GetHistoryLayout();
    wrappedST->ScrollHistory(10000);scroll=wrappedST->GetHistoryLayout();
    const int scrollX=wrappedST->m_x+scroll.scrollThumb.left+2;
    const int scrollY=wrappedST->m_y+scroll.scrollThumb.top+2;
    WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(scrollX,scrollY));
    Require(GetCapture()==g_hMainWnd && g_windowMgr.m_captureWindow==wrappedST,
        "detached visual scrollbar did not acquire real capture");
    WindowProc(g_hMainWnd,WM_MOUSEMOVE,MK_LBUTTON,
        MAKELPARAM(scrollX,wrappedST->m_y+scroll.scrollTrack.top+2));
    WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(-20,-20));
    Require(wrappedST->GetHistoryLayout().first==0 && !GetCapture() && !g_windowMgr.m_captureWindow,
        "detached visual scrollbar drag/outside release failed");
    captureDetached("chat-wrapped-scroll-top");
    const int rowX=wrappedST->m_x+10,rowY=wrappedST->m_y+27;
    WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(rowX,rowY));
    WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(rowX,rowY));
    WindowProc(g_hMainWnd,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(rowX,rowY));
    WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(rowX,rowY));
    Require(primary->GetWhisperTargetText()=="DetachedPeer" && primary->IsMessageInputActive(),
        "detached visual row did not target main whisper editor");
    wrappedBT->Resize(504,106);
    Require(static_cast<int>(wrappedBT->GetVisualRows().size())<narrowRows,
        "actual width resize failed to reflow detached source visual rows");
    captureDetached("chat-wrapped-wide");
    g_windowMgr.DeleteWindow(primary);
    WindowProc(g_hMainWnd,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(rowX,rowY));
    WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(rowX,rowY));
    Require(!g_windowMgr.m_chatWnd,"absent-main detached doubleclick recreated main");
    captureDetached("chat-wrapped-without-main");
    QFile detachedResults(output+"/chat-wrapped-results.json");
    Require(detachedResults.open(QIODevice::WriteOnly),"detached visual results open failed");
    detachedResults.write(QJsonDocument(QJsonObject{{"logical_rows",121},{"visual_rows",122},
        {"narrow_rows",narrowRows},{"wide_rows",static_cast<int>(wrappedBT->GetVisualRows().size())},
        {"viewport_top",20},{"row_pitch",14},{"real_scroll_capture",true},{"talker_target",true},
        {"absent_main_noop",true},{"outcome","PASS"}}).toJson());

    CheckChatBattleOptionsUi(app,view,state,adapter,output);
    CheckChatScrollTrimUi(app,view,state,adapter,output);
    CheckChatRoomUi(app,view,state,mode,adapter,output);
    CheckChatRoomEntryUi(app,view,state,mode,adapter,output);
    CheckChatRoomSettingsUi(app,view,state,mode,adapter,output);
    CheckChatRoomActionsUi(app,view,state,mode,adapter,output);
    CheckWhisperUi(app,view,state,mode,adapter,output);
    CheckAutoMessageUi(app,view,state,mode,adapter,output);

}
