#include "qtui/QtUiState.h"
#include "qtui/QtChatInput.h"
#include "ui/ChatText.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QRawFont>
#include <QFont>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickView>
#include <QQuickRenderControl>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QImage>
#include <QDir>
#include <memory>
#include <cassert>
#include <iostream>
#include <windows.h>

int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);
    const auto sans=QFontDatabase::addApplicationFont("third_party/fonts/NotoSans.ttf");
    const auto japanese=QFontDatabase::addApplicationFont("third_party/fonts/NotoSansJP.ttf");
    assert(sans>=0 && japanese>=0);
    for(auto script:{QChar::Script_Han,QChar::Script_Hiragana,QChar::Script_Katakana})QFontDatabase::addApplicationFallbackFontFamily(script,"Noto Sans JP");
    QFont latin("Noto Sans"),jp("Noto Sans JP");latin.setPixelSize(16);jp.setPixelSize(16);
    auto lf=QRawFont::fromFont(latin),jf=QRawFont::fromFont(jp);
    for(QChar c:QString::fromUtf8("åäöÅÄÖПривет"))assert(lf.supportsCharacter(c));
    for(QChar c:QString::fromUtf8("ひらがなカタカナ日本語漢字"))assert(jf.supportsCharacter(c));
    QtUiState state;
    QObject::connect(&state,&QtUiState::chatFocusRequested,[&](int field) {
        state.setChatWindowWhisperInputActive(field==1);state.setChatWindowMessageInputActive(field==2);
    });
    QObject::connect(&state,&QtUiState::chatEditRequested,[&](int field,const QString& text) {
        const auto cleaned=QString::fromStdString(ro::chat::CleanInput(text.toUtf8().toStdString(),field==1?24:180,field==1?24:180));
        if(field==1)state.setChatWindowWhisperTargetText(cleaned);else state.setChatWindowInputText(cleaned);
    });
    QQmlEngine engine;engine.rootContext()->setContextProperty("uiState",&state);
    QQmlComponent component(&engine,QUrl::fromLocalFile(QDir::currentPath()+"/src/qtui/qml/RoChatContent.qml"));
    if(component.isError()) {qCritical()<<component.errors();return 2;}
    QQuickRenderControl control;QQuickWindow window(&control);
    window.setGeometry(0,0,640,260);window.contentItem()->setSize({640,260});
    std::unique_ptr<QQuickItem> root(qobject_cast<QQuickItem*>(component.create()));
    if(!root) {qCritical()<<component.errors();return 3;}
    root->setParentItem(window.contentItem());root->setSize({640,260});
    app.processEvents();
    QtChatInput bridge;bridge.setScene(&window,root.get());
    HWND native=CreateWindowExW(0,L"STATIC",L"Chat test",WS_POPUP,0,0,640,260,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    assert(native);
    auto message=root->findChild<QQuickItem*>("chatMessageEditor");
    auto whisper=root->findChild<QQuickItem*>("chatWhisperEditor");
    auto* historyPane=root->findChild<QQuickItem*>("chatHistoryPane");
    assert(message && whisper && historyPane);
    auto click=[&](QQuickItem* item,int x=4) {
        const auto point=item->mapToScene({static_cast<qreal>(x),12});
        const auto position=MAKELPARAM(static_cast<int>(point.x()),static_cast<int>(point.y()));
        assert(bridge.handle(native,WM_LBUTTONDOWN,MK_LBUTTON,position,true));
        assert(bridge.handle(native,WM_LBUTTONUP,0,position,true));app.processEvents();
    };
    auto shortcut=[&](int vk) {
        BYTE saved[256]{},keys[256]{};GetKeyboardState(saved);memcpy(keys,saved,256);keys[VK_CONTROL]=0x80;SetKeyboardState(keys);
        assert(bridge.handle(native,WM_KEYDOWN,vk,0,true));
        bridge.handle(native,WM_KEYUP,vk,0,true);SetKeyboardState(saved);app.processEvents();
    };
    click(message);
    assert(message->hasActiveFocus());
    const auto multilingual=QString::fromUtf8("åäö Привет ひらがな カタカナ 日本語 😀");
    for(QChar c:multilingual)assert(bridge.handle(native,WM_CHAR,c.unicode(),0,true));
    assert(state.chatWindowInputText()==multilingual);
    shortcut('A');shortcut('C');assert(app.clipboard()->text()==multilingual);
    shortcut('X');assert(state.chatWindowInputText().isEmpty());
    shortcut('V');assert(state.chatWindowInputText()==multilingual);
    bridge.handle(native,WM_KEYDOWN,VK_END,0,true);
    bridge.handle(native,WM_KEYDOWN,VK_BACK,0,true);
    assert(state.chatWindowInputText()==multilingual.left(multilingual.size()-2));
    app.clipboard()->setText(QString::fromUtf8("åäö\n日本語\tПривет"));shortcut('A');shortcut('V');
    assert(!state.chatWindowInputText().contains('\n') && !state.chatWindowInputText().contains('\t'));
    QInputMethodEvent preedit(QString::fromUtf8("にほん"),{});QCoreApplication::sendEvent(&window,&preedit);
    assert(message->property("preeditText").toString()==QString::fromUtf8("にほん"));
    QInputMethodEvent commit;commit.setCommitString(QString::fromUtf8("日本"));QCoreApplication::sendEvent(&window,&commit);
    assert(state.chatWindowInputText().contains(QString::fromUtf8("日本")));
    click(whisper);app.clipboard()->setText(QString(20,QChar(0x65E5)));shortcut('V');
    assert(state.chatWindowWhisperTargetText().toUtf8().size()==24);
    assert(whisper->property("text").toString()==state.chatWindowWhisperTargetText());
    // Source history/scrollbar input is native-owned; QtChatInput must leave
    // the history pane alone so arrows, drag and wheel can reach UINewChatWnd.
    QVariantList lines;
    for(auto text:{QString::fromUtf8("First: åäö Привет"),QString::fromUtf8("Second: 日本語 <b>literal</b>")})lines.push_back(QVariantMap{{"text",text},{"color","#f0e0c0"}});
    state.setChatWindowLines(lines);app.processEvents();
    const auto historyPoint=historyPane->mapToScene({4,12});
    const auto historyPos=MAKELPARAM(static_cast<int>(historyPoint.x()),static_cast<int>(historyPoint.y()));
    assert(!bridge.handle(native,WM_LBUTTONDOWN,MK_LBUTTON,historyPos,true));
    assert(whisper->hasActiveFocus());
    POINT scrollPoint{30,30};ClientToScreen(native,&scrollPoint);
    assert(!bridge.handle(native,WM_MOUSEWHEEL,MAKEWPARAM(0,120),MAKELPARAM(scrollPoint.x,scrollPoint.y),true));
    // Scene coordinates must also work when the game's GUI scale changes.
    root->setScale(1.5);window.setGeometry(0,0,960,390);window.contentItem()->setSize({960,390});
    click(message);const auto previous=state.chatWindowInputText();bridge.handle(native,WM_CHAR,0x00F6,0,true);
    assert(state.chatWindowInputText().size()==previous.size()+1);
    assert(!bridge.handle(native,WM_CHAR,'x',0,false));
    assert(!bridge.handle(native,WM_CHAR,'x',0,true));
    assert(ro::chat::CleanInput("a\xF0\x9F\x98\x80",2)=="a\xF0\x9F\x98\x80");
    assert(ro::chat::CleanInput("\xED\xA0\x80",10).empty());
    assert(ro::chat::CleanInput("\xC0\xAF",10).empty());
    assert(ro::chat::CleanInput(QString(180,QChar(0x65E5)).toUtf8().toStdString(),180,180).size()==180);
    DestroyWindow(native);
    // Render the actual component as well as exercising its editing behavior.
    state.setChatWindowInputText(multilingual);
    state.setChatWindowWhisperTargetText(QString::fromUtf8("日本語"));
    state.setChatWindowLines({QVariantMap{{"text",multilingual},{"color","#e8dfcf"}},QVariantMap{{"text",QString::fromUtf8("Chat history: ÅÄÖ — Привет — 日本語 漢字")},{"color","#8fd3ef"}}});
    QQuickView view;view.rootContext()->setContextProperty("uiState",&state);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl::fromLocalFile(QDir::currentPath()+"/src/qtui/qml/RoChatContent.qml"));
    assert(view.status()==QQuickView::Ready);view.resize(640,260);view.show();
    for(int i=0;i<5;++i)app.processEvents();
    const auto image=view.grabWindow();assert(!image.isNull());assert(image.save("build-chat-tests/chat-unicode.png"));
    std::cout<<"PASS: glyph coverage; native UTF-16/surrogates; edit/copy/cut/paste; IME preedit/commit; UTF-8 limits; history input left to native owner\n";
}
