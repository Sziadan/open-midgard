#include "ui/UIChatRoomEntry.h"
#include <fstream>
#include <QBuffer>

void CheckChatRoomEntryUi(QGuiApplication& app,QQuickView& view,QtUiState* state,
    CGameMode& mode,QtUiStateAdapter& adapter,const QString& output)
{
    Require(mode.m_world!=nullptr,"room title render fixture needs the real world");
    g_windowMgr.RemoveAllWindows();CGameActor owner;owner.m_gid=0x718293a4;owner.m_isVisible=1;
    mode.m_world->RegisterActor(&owner);mode.m_runtimeActors[owner.m_gid]=&owner;
    struct Restore {
        CGameMode& mode;CGameActor& owner;int folder=g_readFolderFirst,scale=GetConfiguredUiScalePercent();
        std::filesystem::path cwd=std::filesystem::current_path();
        int wait;u32 tick;
        Restore(CGameMode& m,CGameActor& a):mode(m),owner(a),wait(m.m_isWaitingEnterRoom),tick(m.m_reqTickChatRoom){}
        ~Restore(){g_windowMgr.RemoveAllWindows();mode.m_world->UnregisterActor(&owner);mode.m_runtimeActors.erase(owner.m_gid);
            mode.m_isWaitingEnterRoom=wait;mode.m_reqTickChatRoom=tick;g_readFolderFirst=folder;
            std::filesystem::current_path(cwd);SetRuntimeUiScalePercent(scale);}
    } restore(mode,owner);
    const auto assets=std::filesystem::path(output.toStdString())/"entry-assets";std::filesystem::create_directories(assets);
    std::filesystem::create_directories(assets/"source-bitmaps");
    const std::string sourceDirectory="texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";
#if RO_PLATFORM_WINDOWS
    const std::string legacy=assets.string()+"\\"+sourceDirectory;
    CreateDirectoryA((assets.string()+"\\texture").c_str(),nullptr);
    Require(CreateDirectoryA(legacy.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS,"legacy ANSI fixture directory failed");
#else
    std::string legacy=assets.string()+"/"+sourceDirectory;
    std::replace(legacy.begin(),legacy.end(),'\\','/');std::filesystem::create_directories(legacy);
#endif
    const auto bmp=[&](const char* name,int width,int height,QColor color){
        QImage image(width,height,QImage::Format_RGB32);image.fill(color);
        QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);
        Require(image.save(&buffer,"BMP"),"room title synthetic bitmap encode failed");
        {std::ofstream f(assets/"source-bitmaps"/name,std::ios::binary);f.write(bytes.data(),bytes.size());}
#if RO_PLATFORM_WINDOWS
        const auto file=CreateFileA((legacy+"\\"+name).c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        Require(file!=INVALID_HANDLE_VALUE,"source-key fixture file open failed");DWORD count=0;
        const bool wrote=WriteFile(file,bytes.constData(),static_cast<DWORD>(bytes.size()),&count,nullptr)!=0;CloseHandle(file);
        Require(wrote && count==static_cast<DWORD>(bytes.size()),"source-key bitmap fixture write failed");
#else
        {std::ofstream f(std::filesystem::path(legacy)/name,std::ios::binary);f.write(bytes.data(),bytes.size());}
#endif
        // A valid but transparent conflicting flat asset must not override the
        // source path. This catches accidentally testing fallback lookup only.
        image.fill(QColor("#ff00ff"));
        Require(image.save(QString::fromStdString((assets/name).string()),"BMP"),"room title shadow bitmap write failed");
    };
    bmp("sysbox_lu.bmp",12,28,QColor("#c0c0c0"));bmp("sysbox_ru.bmp",12,28,QColor("#c0c0c0"));
    bmp("sysbox_mu.bmp",12,28,QColor("#80d080"));bmp("sysbox_ld.bmp",12,4,QColor("#305070"));
    bmp("sysbox_rd.bmp",12,4,QColor("#305070"));bmp("sysbox_md.bmp",12,4,QColor("#305070"));
    bmp("sysbox_arr_l.bmp",10,6,QColor("#e0c040"));bmp("chat_open.bmp",20,20,QColor("#d04040"));
    bmp("chat_close.bmp",20,20,QColor("#4040d0"));
    bmp("btn_ok.bmp",43,20,QColor("#c0a080"));bmp("btn_ok_a.bmp",44,21,QColor("#80a0c0"));bmp("btn_ok_b.bmp",45,22,QColor("#806040"));
    {std::ofstream f(assets/"msgstringtable.txt",std::ios::binary);for(int i=0;i<=15;++i)f<<(i==15?"PASSWORD PROMPT#\r\n":"#\r\n");}
    std::filesystem::current_path(assets);g_readFolderFirst=1;SetRuntimeUiScalePercent(100);
    const auto listing=[&](u8 type,const std::string& text){
        std::vector<u8> packet(17+text.size());const auto put16=[&](int at,u16 value){packet[at]=u8(value);packet[at+1]=u8(value>>8);};
        const auto put32=[&](int at,u32 value){for(int i=0;i<4;++i)packet[at+i]=u8(value>>(8*i));};
        put16(0,0xd7);put16(2,u16(packet.size()));put32(4,owner.m_gid);put32(8,0xfedc1234);put16(12,8);put16(14,2);packet[16]=type;
        std::copy(text.begin(),text.end(),packet.begin()+17);Dispatch(mode,packet);
        Require(owner.m_chatTitle!=nullptr,"D7 failed to create actual title renderer");owner.m_chatTitle->Move(40,80);
    };
    const auto capture=[&](const QString& name){
        state->setUiScale(GetConfiguredUiScaleFactor());adapter.syncChat();Frames(app,view);auto* title=Find(view.rootObject(),"sourceChatRoomWorldTitle0");
        Require(title && title->isVisible(),"actual QML world title missing");
        const auto position=title->mapToScene(QPointF(0,0));auto* nativeTitle=owner.m_chatTitle;
        Require(qRound(position.x())==UiScaleLogicalToRawCoordinate(nativeTitle->m_x)
            && qRound(position.y())==UiScaleLogicalToRawCoordinate(nativeTitle->m_y),"native/Qt title position diverged");
        const auto data=state->chatWindowUi().value("roomTitles").toList();
        Require(data.size()==1 && data[0].toMap().value("id").toULongLong()==0xfedc1234u,"title projection lost unsigned ID");
        auto qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"title native DIB allocation failed");
        std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);nativeTitle->DrawToHdc(dib.GetDC());
        if(g_windowMgr.m_chatRoomPasswordWnd)g_windowMgr.m_chatRoomPasswordWnd->DrawToHdc(dib.GetDC());
        auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
        if(GetConfiguredUiScalePercent()!=100)native=native.scaled(qRound(native.width()*GetConfiguredUiScaleFactor()),qRound(native.height()*GetConfiguredUiScaleFactor()),Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
        if(GetConfiguredUiScalePercent()==100){
            for(auto sample:{QPoint(50,2),QPoint(100,31),QPoint(5,5)}){
                const int x=nativeTitle->m_x+sample.x(),y=nativeTitle->m_y+sample.y();
                const QColor expected=sample==QPoint(50,2)?QColor("#80d080"):sample==QPoint(100,31)?QColor("#e0c040")
                    :nativeTitle->m_roomInfo.roomType?QColor("#d04040"):QColor("#4040d0");
                Require(native.pixelColor(x,y)==expected && qt.pixelColor(x,y)==expected,"native/Qt source tile/icon/arrow composition differs");
            }
        }
        if(GetConfiguredUiScalePercent()==100 && g_windowMgr.m_chatRoomPasswordWnd){
            const auto* dialog=g_windowMgr.m_chatRoomPasswordWnd;const auto ok=dialog->GetOkBounds();
            const int x=dialog->m_x+ok.left+5,y=dialog->m_y+ok.top+5;
            const QColor expected=dialog->IsOkPressed()?QColor("#806040"):QColor("#c0a080");
            Require(native.pixelColor(x,y)==expected && qt.pixelColor(x,y)==expected,"native/Qt source OK bitmap state differs");
        }
        Require(native.save(output+"/"+name+".native.png") && qt.save(output+"/"+name+".qml.png"),"title frame save failed");
    };
    listing(1,"Long <literal> world room title that must shorten safely");
    Require(owner.m_chatTitle->GetArtwork().IsValid() && owner.m_chatTitle->GetDisplayTitle().find("...")!=std::string::npos,
        "source frame assets or title ellipsis not used");capture("chat-entry-world");
    listing(0,"Private <literal>");mode.m_isWaitingEnterRoom=0;
    WindowProc(g_hMainWnd,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(60,90));WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(60,90));
    auto* password=g_windowMgr.m_chatRoomPasswordWnd;Require(password && password->GetPrompt()=="PASSWORD PROMPT","native private-title doubleclick/prompt failed");
    const int oldX=password->m_x,oldY=password->m_y;
    WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(oldX+3,oldY+42));
    Require(GetCapture()==g_hMainWnd && g_windowMgr.m_captureWindow==password,"password drag did not acquire actual OS capture");
    WindowProc(g_hMainWnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(oldX+28,oldY+57));
    WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(-20,-20));
    Require(password->m_x==oldX+25 && password->m_y==oldY+15 && !GetCapture() && !g_windowMgr.m_captureWindow,
        "uncovered password frame drag/outside release differs from source");
    WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(password->m_x+3,password->m_y+42));
    WindowProc(g_hMainWnd,WM_CANCELMODE,0,0);const int cancelX=password->m_x,cancelY=password->m_y;
    WindowProc(g_hMainWnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(cancelX+60,cancelY+60));
    Require(password->m_x==cancelX && password->m_y==cancelY && !GetCapture() && !g_windowMgr.m_captureWindow,
        "cancelled password capture kept dragging");
    WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(password->m_x+130,password->m_y+24));
    Require(g_windowMgr.m_chatRoomPasswordWnd==password && password->IsOkPressed(),"password OK dispatched on press instead of release");
    capture("chat-entry-password-pressed");
    WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(-20,-20));
    Require(g_windowMgr.m_chatRoomPasswordWnd==password && !password->IsOkPressed() && !GetCapture(),"password outside button release submitted or retained capture");
    for(char c:std::string("secret"))WindowProc(g_hMainWnd,WM_CHAR,c,0);
    Require(password->GetOkBounds().right-password->GetOkBounds().left==45 && password->GetOkBounds().bottom-password->GetOkBounds().top==22,"password button ignored maximum dimensions across source bitmap states");
    Require(password->GetInputText()=="secret","native password typing did not own input");capture("chat-entry-password");
    Require(Find(view.rootObject(),"sourceChatRoomPasswordMask")->property("text").toString()=="******"
        && !state->chatWindowUi().value("roomPassword").toMap().contains("input"),"password projection exposed unmasked text");
    WindowProc(g_hMainWnd,WM_KEYDOWN,VK_ESCAPE,0);Require(g_windowMgr.m_chatRoomPasswordWnd==password,"source password cancel117 changed");
    WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(password->m_x+3,password->m_y+42));
    WindowProc(g_hMainWnd,WM_KEYDOWN,VK_RETURN,0);Require(!GetCapture() && !g_windowMgr.m_captureWindow && !g_windowMgr.m_chatRoomPasswordWnd && !g_windowMgr.m_editWindow,"password Enter retained deleted owner/focus");
    SetRuntimeUiScalePercent(125);mode.m_isWaitingEnterRoom=0;owner.m_chatTitle->OnLBtnDblClk(0,0);
    password=g_windowMgr.m_chatRoomPasswordWnd;Require(password,"125% private title lost password dialog");capture("chat-entry-password-125");
    const int okX=UiScaleLogicalToRawCoordinate(password->m_x+130),okY=UiScaleLogicalToRawCoordinate(password->m_y+24);
    WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(okX,okY));WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(okX,okY));
    Require(!g_windowMgr.m_chatRoomPasswordWnd && !g_windowMgr.m_captureWindow,"125% actual password button/focus teardown failed");
    SetRuntimeUiScalePercent(100);state->setUiScale(1.0);g_windowMgr.RemoveAllWindows();Require(!owner.m_chatTitle,"render teardown retained actor title link");adapter.syncChat();Frames(app,view);
    Require(state->chatWindowUi().value("roomTitles").toList().empty(),"QML retained destroyed world titles");
    auto* staleTitle=Find(view.rootObject(),"sourceChatRoomWorldTitle0");
    Require((!staleTitle || !staleTitle->isVisible()) && !Find(view.rootObject(),"sourceChatRoomPassword")->isVisible(),"actual QML retained deleted title/password owners");
    QFile result(output+"/chat-entry-results.json");Require(result.open(QIODevice::WriteOnly),"entry result open failed");
    result.write(QJsonDocument(QJsonObject{{"actual_d7_world_title",true},{"native_qt_bitmap_tiles",true},{"ellipsis",true},
        {"private_doubleclick",true},{"masked_native_input",true},{"drag_cancel_release",true},{"release_button",true},{"delete_during_capture",true},{"actual_125_button",true},{"teardown",true},{"outcome","PASS"}}).toJson());
}
