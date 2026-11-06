#include "ui/UIWhisperWnd.h"
#include "ui/UIChatRoomSettingsWnd.h"

void CheckWhisperUi(QGuiApplication& app,QQuickView& view,QtUiState* state,
    CGameMode& mode,QtUiStateAdapter& adapter,const QString& output)
{
    g_windowMgr.RemoveAllWindows();
    struct Restore {
        CGameMode& mode;int scale=GetConfiguredUiScalePercent(),folder=g_readFolderFirst,codePage=g_codePage;
        bool base=g_session.m_isShowWhisperWnd,friendWindows=g_session.m_isShowWhisperWnd_Friend,sound=g_session.m_isPlayWhisperOpenSound;
        bool autoMessage=g_session.m_onAutoMSG,save=g_session.m_isSaveChat;
        SubChatWindowInfo geometry=g_windowMgr.m_whisperWindowInfo;std::array<char,25> name{};
        std::list<FRIEND_INFO> friends=g_session.m_friendList;ro::guild::WindowState guild=g_session.m_guildWindow;
        std::vector<std::string> excluded=g_session.m_exNameList;
        std::string pendingName,pendingText,submitted;int repeat;
        std::filesystem::path cwd=std::filesystem::current_path();
        Restore(CGameMode& value):mode(value),pendingName(value.m_lastWhisperName),pendingText(value.m_lastWhisper),submitted(value.m_sourceLastSubmittedChat),repeat(value.m_sourceSameChatRepeatCount)
        {std::copy(std::begin(g_session.m_playerName),std::end(g_session.m_playerName),name.begin());}
        ~Restore(){g_windowMgr.RemoveAllWindows();SetRuntimeUiScalePercent(scale);g_readFolderFirst=folder;g_codePage=codePage;std::filesystem::current_path(cwd);
            g_session.m_isShowWhisperWnd=base;g_session.m_isShowWhisperWnd_Friend=friendWindows;g_session.m_isPlayWhisperOpenSound=sound;
            g_session.m_onAutoMSG=autoMessage;g_session.m_isSaveChat=save;g_windowMgr.m_whisperWindowInfo=geometry;
            g_session.m_friendList=std::move(friends);g_session.m_guildWindow=std::move(guild);g_session.m_exNameList=std::move(excluded);
            std::copy(name.begin(),name.end(),std::begin(g_session.m_playerName));
            mode.m_lastWhisperName=pendingName;mode.m_lastWhisper=pendingText;mode.m_sourceLastSubmittedChat=submitted;mode.m_sourceSameChatRepeatCount=repeat;}
    } restore(mode);
    const auto assets=std::filesystem::path(output.toStdString())/"whisper-assets";std::filesystem::create_directories(assets);
    {std::ofstream file(assets/"msgstringtable.txt",std::ios::binary);for(int i=0;i<=941;++i)file<<(i==102?"Friend Label#\r\n":"#\r\n");}
    std::filesystem::current_path(assets);g_readFolderFirst=1;g_codePage=65001;SetRuntimeUiScalePercent(100);
    g_session.m_isShowWhisperWnd=g_session.m_isShowWhisperWnd_Friend=true;g_session.m_isPlayWhisperOpenSound=false;
    g_session.m_onAutoMSG=g_session.m_isSaveChat=false;g_session.m_friendList.clear();g_session.m_guildWindow={};g_session.m_exNameList.clear();
    std::memset(g_session.m_playerName,0,sizeof(g_session.m_playerName));std::memcpy(g_session.m_playerName,"Viewer",6);
    g_windowMgr.m_whisperWindowInfo={20,40,280,120};
    const auto incoming=[&](const std::string& sender,const std::string& text){std::vector<u8> packet(29+text.size());packet[0]=0x97;
        packet[2]=u8(packet.size());packet[3]=u8(packet.size()>>8);std::copy(sender.begin(),sender.end(),packet.begin()+4);std::copy(text.begin(),text.end(),packet.begin()+28);Dispatch(mode,packet);};
    incoming("Alpha","first <literal>");auto* alpha=g_windowMgr.m_whisperWindows.at("Alpha");
    FRIEND_INFO friendInfo;friendInfo.characterName="Beta";g_session.m_friendList.push_back(friendInfo);incoming("Beta","friend");auto* beta=g_windowMgr.m_whisperWindows.at("Beta");
    Require(!g_windowMgr.m_chatWnd && alpha->m_x==20 && alpha->m_y==40 && beta->m_x==37 && beta->m_y==57,"actual per-name creation/stagger required main or changed source placement");
    const char* marker="VISIBLE COLOR MARKER";alpha->SendMsg(nullptr,37,reinterpret_cast<msgparam_t>(marker),0x2020ff,0);
    alpha->Move(100,80);beta->Move(420,80);
    const auto pointer=[&](UINT message,int x,int y){WindowProc(g_hMainWnd,message,message==WM_LBUTTONUP?0:MK_LBUTTON,
        MAKELPARAM(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y)));};
    const auto tap=[&](int x,int y){pointer(WM_LBUTTONDOWN,x,y);pointer(WM_LBUTTONUP,x,y);};
    const auto capture=[&](const QString& name){
        state->setUiScale(GetConfiguredUiScaleFactor());adapter.syncChat();Frames(app,view);
        auto* item=Find(view.rootObject(),"sourceWhisperWindow0");auto* viewport=Find(view.rootObject(),"sourceWhisperWindow0History");
        Require(item && item->isVisible() && viewport,"actual QML whisper owner/history missing");
        const double scale=GetConfiguredUiScaleFactor();const auto bounds=item->mapRectToScene(item->boundingRect());
        Require(std::abs(bounds.left()-alpha->m_x*scale)<0.001 && std::abs(bounds.top()-alpha->m_y*scale)<0.001
            && std::abs(bounds.width()-alpha->m_w*scale)<0.001 && std::abs(bounds.height()-alpha->m_h*scale)<0.001,"whisper native/QML affine geometry differs");
        const auto layout=alpha->GetLayout();const auto history=viewport->mapRectToScene(viewport->boundingRect());
        Require(std::abs(history.left()-(alpha->m_x+layout.history.left)*scale)<0.001 && std::abs(history.top()-(alpha->m_y+17)*scale)<0.001,"whisper source child viewport differs");
        const auto data=state->chatWindowUi().value("whisperWindows").toList();Require(data.size()==2 && data[0].toMap().value("name")=="Alpha" && data[1].toMap().value("title")=="Beta  (Friend Label)  *^_^*","actual Qt per-name identity/title projection changed");
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"whisper native DIB failed");std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
        for(auto* child:g_windowMgr.m_children)if(auto* window=dynamic_cast<UIWhisperWnd*>(child))if(window->m_show)window->DrawToHdc(dib.GetDC());
        auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
        if(GetConfiguredUiScalePercent()!=100)native=native.scaled(qRound(native.width()*scale),qRound(native.height()*scale),Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
        const auto qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        const QRect area(UiScaleLogicalToRawCoordinate(alpha->m_x+3),UiScaleLogicalToRawCoordinate(alpha->m_y+20),UiScaleLogicalToRawCoordinate(alpha->m_w-20),UiScaleLogicalToRawCoordinate(alpha->m_h-38));
        const auto red=[&](const QImage& image){int count=0;const auto clip=area.intersected(image.rect());for(int y=clip.top();y<=clip.bottom();++y)for(int x=clip.left();x<=clip.right();++x){const auto c=image.pixelColor(x,y);if(c.red()>150 && c.green()<120 && c.blue()<120)++count;}return count;};
        Require(red(native)>20 && red(qt)>20,"native/QML whisper source color rows missing behind peer");
        Require(native.save(output+"/"+name+".native.png") && qt.save(output+"/"+name+".qml.png"),"save whisper frames failed");
    };
    capture("whisper-two-names");beta->Move(120,90);incoming("Alpha","front without editor focus");
    const auto edit=alpha->GetLayout().edit;tap(alpha->m_x+edit.left+2,alpha->m_y+edit.top+2);
    for(char c:std::string("literal <draft>"))WindowProc(g_hMainWnd,WM_CHAR,c,0);
    Require(alpha->GetInputText()=="literal <draft>" && beta->GetInputText().empty(),"native focused input crossed per-name owners");capture("whisper-overlap-input");
    const auto rows=alpha->GetRows().size();WindowProc(g_hMainWnd,WM_KEYDOWN,VK_RETURN,0);WindowProc(g_hMainWnd,WM_CHAR,'\r',0);
    Require(alpha->GetInputText().empty() && alpha->GetRows().size()==rows && alpha->GetInputHistory().size()==1,"native whisper Enter duplicated/echoed or failed to clear");
    WindowProc(g_hMainWnd,WM_KEYDOWN,VK_ESCAPE,0);Require(g_windowMgr.m_whisperWindows.at("Alpha")==alpha,"whisper Escape unexpectedly closed source owner");
    const auto resize=alpha->GetLayout().resizer;const int rx=alpha->m_x+resize.left+12,ry=alpha->m_y+resize.top+12;
    pointer(WM_LBUTTONDOWN,rx,ry);Require(GetCapture()==g_hMainWnd && g_windowMgr.m_captureWindow==alpha,"whisper resize lacks real OS capture");
    pointer(WM_MOUSEMOVE,rx+90,ry+90);pointer(WM_LBUTTONUP,-20,-20);
    Require(alpha->m_w==344 && alpha->m_h==184 && !GetCapture() && !g_windowMgr.m_captureWindow,"native source whisper resizer/outside release failed");
    const int x=alpha->m_x,y=alpha->m_y;pointer(WM_LBUTTONDOWN,x+3,y+3);pointer(WM_MOUSEMOVE,x+28,y+18);pointer(WM_LBUTTONUP,-20,-20);
    Require(alpha->m_x==x+25 && alpha->m_y==y+15,"source whisper frame drag failed");
    pointer(WM_LBUTTONDOWN,alpha->m_x+3,alpha->m_y+3);WindowProc(g_hMainWnd,WM_CANCELMODE,0,0);pointer(WM_MOUSEMOVE,alpha->m_x+90,alpha->m_y+90);
    Require(alpha->m_x==x+25 && alpha->m_y==y+15 && !GetCapture() && !g_windowMgr.m_captureWindow,"cancelled whisper frame continued dragging");capture("whisper-resized");
    SetRuntimeUiScalePercent(125);capture("whisper-125");beta->SetShow(0);incoming("Beta","hidden receive");capture("whisper-hidden-peer");
    Require(!Find(view.rootObject(),"sourceWhisperWindow1")->isVisible() && !beta->m_show,"retained hidden owner was unhidden by receive/projection");
    g_windowMgr.m_editWindow=alpha;g_windowMgr.ShowRoomValidationMessage(13);auto* modal=g_windowMgr.m_roomMessageBox;
    incoming("UnderModal","arrival");auto* under=g_windowMgr.m_whisperWindows.at("UnderModal");under->Move(modal->m_x,modal->m_y);
    state->setUiScale(GetConfiguredUiScaleFactor());adapter.syncChat();Frames(app,view);
    ArgbDibSurface modalDib;Require(modalDib.EnsureSize(view.width(),view.height()),"whisper/modal DIB failed");std::fill_n(modalDib.GetPixels(),view.width()*view.height(),0xff181b20u);
    g_windowMgr.DrawVisibleWindowsToHdc(modalDib.GetDC(),true);
    auto modalNative=QImage(reinterpret_cast<uchar*>(modalDib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
    const double modalScale=GetConfiguredUiScaleFactor();modalNative=modalNative.scaled(qRound(modalNative.width()*modalScale),qRound(modalNative.height()*modalScale),Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
    const auto modalQt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
    const QPoint sample(UiScaleLogicalToRawCoordinate(modal->m_x+15),UiScaleLogicalToRawCoordinate(modal->m_y+30));
    Require(modalNative.pixelColor(sample)==QColor("#f2f2f2") && modalQt.pixelColor(sample)==QColor("#f2f2f2"),"new whisper covered active modal in native/QML draw order");
    Require(modalNative.save(output+"/whisper-modal-overlap.native.png") && modalQt.save(output+"/whisper-modal-overlap.qml.png"),"save whisper/modal frames failed");
    WindowProc(g_hMainWnd,WM_CHAR,'z',0);Require(alpha->GetInputText().empty() && under->GetInputText().empty(),"actual modal leaked native input into whisper");
    WindowProc(g_hMainWnd,WM_KEYDOWN,VK_ESCAPE,0);Require(g_windowMgr.m_editWindow==alpha && !g_windowMgr.m_modalWindow,"modal dismissal lost live whisper focus");g_windowMgr.DeleteWindow(under);
    const auto close=alpha->GetLayout().close;pointer(WM_LBUTTONDOWN,alpha->m_x+close.left+5,alpha->m_y+8);pointer(WM_LBUTTONUP,-20,-20);
    Require(g_windowMgr.m_whisperWindows.at("Alpha")==alpha,"outside close-button release deleted source whisper");
    tap(alpha->m_x+close.left+5,alpha->m_y+8);Require(!g_windowMgr.m_whisperWindows.count("Alpha") && g_windowMgr.m_whisperWindows.count("Beta"),"closing one name removed another or retained stale map owner");
    adapter.syncChat();Frames(app,view);Require(!Find(view.rootObject(),"sourceWhisperWindow0")->isVisible(),"actual QML retained closed Alpha rather than hidden Beta");
    beta->SetShow(1);pointer(WM_LBUTTONDOWN,beta->m_x+3,beta->m_y+3);Require(GetCapture()==g_hMainWnd,"whisper teardown fixture lacks capture");
    g_windowMgr.DeleteWindow(beta);Require(!GetCapture() && !g_windowMgr.m_captureWindow && g_windowMgr.m_whisperWindows.empty(),"deleting captured whisper retained pointer/OS capture");
    adapter.syncChat();Frames(app,view);Require(state->chatWindowUi().value("whisperWindows").toList().empty(),"actual QML retained destroyed per-name owners");
    QFile result(output+"/whisper-results.json");Require(result.open(QIODevice::WriteOnly),"whisper render result open failed");
    result.write(QJsonDocument(QJsonObject{{"native_qml_frames",12},{"two_names_without_main",true},{"peer_stacking",true},{"native_input",true},{"modal_above_new_receive",true},
        {"resize_drag_cancel_release",true},{"scale_125",true},{"hidden_delivery",true},{"individual_captured_teardown",true},{"outcome","PASS"}}).toJson());
}
